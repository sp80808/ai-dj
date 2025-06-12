<?php
// distributed/central-server/src/Controllers/WorkerController.php

declare(strict_types=1);

namespace Obsidian\Controllers;

use Obsidian\Services\WorkerPoolService;
use Obsidian\Services\LoadBalancerService;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use RuntimeException;

class WorkerController
{
    private WorkerPoolService $workerPool;
    private LoadBalancerService $loadBalancer;

    public function __construct(WorkerPoolService $workerPool, LoadBalancerService $loadBalancer)
    {
        $this->workerPool = $workerPool;
        $this->loadBalancer = $loadBalancer;
    }

    public function register(Request $request, Response $response): Response
    {
        try {
            $data = $request->getParsedBody() ?? [];
            $workerIp = $this->getWorkerIpAddress($request);
            $data['ip_address'] = $workerIp;
            $result = $this->workerPool->registerWorker($data);

            $responseData = [
                'success' => true,
                'message' => 'Worker registered successfully',
                'data' => $result
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json')->withStatus(201);
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'REGISTRATION_FAILED', $e->getMessage(), 400);
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Worker registration failed', 500);
        }
    }

    public function heartbeat(Request $request, Response $response): Response
    {
        try {
            $data = $request->getParsedBody() ?? [];
            if (empty($data['worker_id'])) {
                return $this->errorResponse($response, 'INVALID_WORKER_ID', 'Worker ID is required', 400);
            }
            $authHeader = $request->getHeaderLine('Authorization');
            if ($authHeader && str_starts_with($authHeader, 'Bearer ')) {
                $token = substr($authHeader, 7);
                if (!$this->verifyWorkerToken($data['worker_id'], $token)) {
                    return $this->errorResponse($response, 'INVALID_TOKEN', 'Invalid worker session token', 401);
                }
            }
            $success = $this->workerPool->processHeartbeat($data['worker_id'], $data);

            if (!$success) {
                return $this->errorResponse($response, 'HEARTBEAT_FAILED', 'Failed to process heartbeat', 400);
            }

            $responseData = [
                'success' => true,
                'message' => 'Heartbeat processed successfully',
                'timestamp' => date('c')
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Heartbeat processing failed', 500);
        }
    }

    public function status(Request $request, Response $response): Response
    {
        try {
            $load = $this->loadBalancer->getWorkerLoad();
            $health = $this->loadBalancer->healthCheck();
            $availableWorkers = $this->workerPool->getAvailableWorkers();

            $workerSummary = array_map(function ($worker) {
                $gpuInfo = json_decode($worker['gpu_info'], true) ?? [];

                return [
                    'worker_id_short' => substr($worker['worker_id'], 0, 8),
                    'status' => $worker['status'],
                    'performance_score' => (float)$worker['performance_score'],
                    'avg_response_time' => (float)$worker['avg_response_time'],
                    'total_requests' => (int)$worker['total_requests'],
                    'success_rate' => $worker['total_requests'] > 0
                        ? round(($worker['successful_requests'] / $worker['total_requests']) * 100, 1)
                        : 0,
                    'gpu_memory_used' => $gpuInfo['gpu_memory_used'] ?? 0,
                    'gpu_memory_total' => $gpuInfo['gpu_memory_total'] ?? 0,
                    'last_heartbeat' => $worker['last_heartbeat']
                ];
            }, $availableWorkers);

            $responseData = [
                'success' => true,
                'data' => [
                    'cluster_health' => $health,
                    'load_summary' => $load,
                    'workers' => $workerSummary,
                    'timestamp' => date('c')
                ]
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to get worker status', 500);
        }
    }

    public function unregister(Request $request, Response $response, array $args): Response
    {
        try {
            $workerId = $args['workerId'] ?? '';

            if (empty($workerId)) {
                return $this->errorResponse($response, 'INVALID_WORKER_ID', 'Worker ID is required', 400);
            }
            $worker = $this->workerPool->getWorkerById($workerId);
            if (!$worker) {
                return $this->errorResponse($response, 'WORKER_NOT_FOUND', 'Worker not found', 404);
            }
            $authHeader = $request->getHeaderLine('Authorization');
            if (!$authHeader || !str_starts_with($authHeader, 'Bearer ')) {
                return $this->errorResponse($response, 'UNAUTHORIZED', 'Authorization required', 401);
            }

            $token = substr($authHeader, 7);
            if (!$this->verifyWorkerToken($workerId, $token)) {
                return $this->errorResponse($response, 'INVALID_TOKEN', 'Invalid authorization token', 401);
            }

            $success = $this->workerPool->markWorkerOffline($workerId);

            if (!$success) {
                return $this->errorResponse($response, 'UNREGISTER_FAILED', 'Failed to unregister worker', 500);
            }

            $responseData = [
                'success' => true,
                'message' => 'Worker unregistered successfully'
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Worker unregistration failed', 500);
        }
    }

    private function getWorkerIpAddress(Request $request): string
    {
        $forwardedFor = $request->getHeaderLine('X-Forwarded-For');
        if (!empty($forwardedFor)) {
            $ips = explode(',', $forwardedFor);
            return trim($ips[0]);
        }

        $realIp = $request->getHeaderLine('X-Real-IP');
        if (!empty($realIp)) {
            return $realIp;
        }

        $serverParams = $request->getServerParams();
        return $serverParams['REMOTE_ADDR'] ?? '127.0.0.1';
    }

    private function verifyWorkerToken(string $workerId, string $token): bool
    {
        try {
            $worker = $this->workerPool->getWorkerById($workerId);
            if (!$worker) {
                return false;
            }

            return hash_equals($worker['session_token'], $token);
        } catch (\Exception $e) {
            return false;
        }
    }

    private function errorResponse(Response $response, string $code, string $message, int $statusCode): Response
    {
        $errorData = [
            'success' => false,
            'error' => [
                'code' => $code,
                'message' => $message
            ]
        ];

        $response->getBody()->write(json_encode($errorData));
        return $response->withHeader('Content-Type', 'application/json')->withStatus($statusCode);
    }
}
