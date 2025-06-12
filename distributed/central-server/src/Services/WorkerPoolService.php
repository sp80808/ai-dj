<?php
// distributed/central-server/src/Services/WorkerPoolService.php

declare(strict_types=1);

namespace Obsidian\Services;

use GuzzleHttp\Client;
use GuzzleHttp\Exception\RequestException;
use Monolog\Logger;
use Obsidian\Utils\Database;
use RuntimeException;

class WorkerPoolService
{
    private Database $db;
    private Logger $logger;
    private Client $httpClient;

    public function __construct(Database $db, Logger $logger)
    {
        $this->db = $db;
        $this->logger = $logger;
        $this->httpClient = new Client(['timeout' => 10]);
    }

    public function registerWorker(array $workerData): array
    {
        $this->validateWorkerRegistration($workerData);

        $existingWorker = $this->db->queryOne(
            'SELECT id, status FROM workers WHERE worker_id = ?',
            [$workerData['worker_id']]
        );

        if ($existingWorker) {
            return $this->updateExistingWorker($existingWorker['id'], $workerData);
        }

        return $this->registerNewWorker($workerData);
    }

    public function markWorkerOffline(string $workerId): bool
    {
        try {
            $result = $this->db->execute(
                'UPDATE workers SET status = "offline", last_heartbeat = CURRENT_TIMESTAMP WHERE worker_id = ?',
                [$workerId]
            );

            if ($result) {
                $this->logger->info("Worker marked as offline: {$workerId}");
            } else {
                $this->logger->warning("Failed to mark worker as offline (worker not found): {$workerId}");
            }

            return $result;
        } catch (\Exception $e) {
            $this->logger->error("Error marking worker offline for {$workerId}: " . $e->getMessage());
            return false;
        }
    }

    public function processHeartbeat(string $workerId, array $statusData): bool
    {
        try {
            $worker = $this->db->queryOne(
                'SELECT id FROM workers WHERE worker_id = ?',
                [$workerId]
            );

            if (!$worker) {
                $this->logger->warning("Heartbeat from unknown worker: {$workerId}");
                return false;
            }

            $this->db->execute(
                'UPDATE workers SET 
                    status = ?,
                    gpu_info = ?,
                    performance_score = ?,
                    last_heartbeat = CURRENT_TIMESTAMP
                 WHERE id = ?',
                [
                    $statusData['status'] ?? 'idle',
                    json_encode($statusData),
                    $this->calculatePerformanceScore($statusData),
                    $worker['id']
                ]
            );

            $this->logger->debug("Heartbeat processed for worker {$workerId}");
            return true;
        } catch (\Exception $e) {
            $this->logger->error("Error processing heartbeat for worker {$workerId}: " . $e->getMessage());
            return false;
        }
    }

    public function getAvailableWorkers(): array
    {
        return $this->db->query(
            'SELECT id, worker_id, ip_address, port, status, gpu_info, 
                    performance_score, avg_response_time, last_heartbeat
             FROM workers 
             WHERE status IN ("idle", "busy") 
               AND last_heartbeat > DATE_SUB(NOW(), INTERVAL 2 MINUTE)
             ORDER BY performance_score DESC, avg_response_time ASC'
        );
    }

    public function getWorkerById(string $workerId): ?array
    {
        return $this->db->queryOne(
            'SELECT * FROM workers WHERE worker_id = ?',
            [$workerId]
        );
    }

    public function markWorkerBusy(string $workerId): bool
    {
        $result = $this->db->execute(
            'UPDATE workers SET status = "busy" WHERE worker_id = ? AND status = "idle"',
            [$workerId]
        );

        return $result;
    }

    public function markWorkerIdle(string $workerId): bool
    {
        return $this->db->execute(
            'UPDATE workers SET status = "idle" WHERE worker_id = ?',
            [$workerId]
        );
    }

    public function updateWorkerStats(string $workerId, bool $success, int $responseTimeMs): void
    {
        $this->db->beginTransaction();

        try {
            $worker = $this->db->queryOne(
                'SELECT total_requests, successful_requests, failed_requests, avg_response_time 
                 FROM workers WHERE worker_id = ?',
                [$workerId]
            );

            if (!$worker) {
                $this->db->rollback();
                return;
            }

            $totalRequests = $worker['total_requests'] + 1;
            $successfulRequests = $worker['successful_requests'] + ($success ? 1 : 0);
            $failedRequests = $worker['failed_requests'] + ($success ? 0 : 1);

            $currentAvg = $worker['avg_response_time'];
            $newAvg = (($currentAvg * $worker['total_requests']) + $responseTimeMs) / $totalRequests;

            $this->db->execute(
                'UPDATE workers SET 
                    total_requests = ?,
                    successful_requests = ?,
                    failed_requests = ?,
                    avg_response_time = ?,
                    performance_score = ?
                 WHERE worker_id = ?',
                [
                    $totalRequests,
                    $successfulRequests,
                    $failedRequests,
                    $newAvg,
                    $this->calculatePerformanceScore([
                        'success_rate' => ($successfulRequests / $totalRequests) * 100,
                        'avg_response_time' => $newAvg
                    ]),
                    $workerId
                ]
            );

            $this->db->commit();
            $this->logger->debug("Updated stats for worker {$workerId}: success={$success}, time={$responseTimeMs}ms");
        } catch (\Exception $e) {
            $this->db->rollback();
            $this->logger->error("Error updating worker stats: " . $e->getMessage());
        }
    }

    public function removeOfflineWorkers(): bool
    {
        $offlineThreshold = $_ENV['WORKER_MAX_IDLE_TIME'] ?? 300;

        $removedCount = $this->db->execute(
            'UPDATE workers SET status = "offline" 
             WHERE status != "offline" 
               AND last_heartbeat < DATE_SUB(NOW(), INTERVAL ? SECOND)',
            [$offlineThreshold]
        );

        if ($removedCount > 0) {
            $this->logger->info("Marked {$removedCount} workers as offline");
        }

        return $removedCount;
    }

    public function sendRequestToWorker(string $workerId, array $requestData): array
    {
        $worker = $this->getWorkerById($workerId);

        if (!$worker) {
            throw new RuntimeException("Worker not found: {$workerId}");
        }

        $url = "http://{$worker['ip_address']}:{$worker['port']}/generate";
        $startTime = microtime(true);

        try {
            $response = $this->httpClient->post($url, [
                'json' => $requestData,
                'timeout' => $_ENV['WORKER_TIMEOUT'] ?? 300,
                'headers' => [
                    'Content-Type' => 'application/json',
                    'X-Request-Source' => 'obsidian-central'
                ]
            ]);

            $responseTime = (int)((microtime(true) - $startTime) * 1000);
            $this->updateWorkerStats($workerId, true, $responseTime);

            return [
                'success' => true,
                'data' => $response->getBody()->getContents(),
                'headers' => $response->getHeaders(),
                'response_time' => $responseTime
            ];
        } catch (RequestException $e) {
            $responseTime = (int)((microtime(true) - $startTime) * 1000);
            $this->updateWorkerStats($workerId, false, $responseTime);

            $this->logger->error("Worker request failed for {$workerId}: " . $e->getMessage());

            $this->db->execute(
                'UPDATE workers SET status = "error" WHERE worker_id = ?',
                [$workerId]
            );

            throw new RuntimeException("Worker request failed: " . $e->getMessage());
        }
    }

    private function registerNewWorker(array $workerData): array
    {
        $sessionToken = bin2hex(random_bytes(32));

        $this->db->execute(
            'INSERT INTO workers (
                worker_id, ip_address, port, status, gpu_info, 
                capabilities, docker_image_hash, session_token
             ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            [
                $workerData['worker_id'],
                $workerData['ip_address'] ?? $_SERVER['REMOTE_ADDR'],
                $workerData['worker_port'] ?? 8001,
                'idle',
                json_encode($workerData['gpu_info'] ?? []),
                json_encode($workerData['capabilities'] ?? []),
                $workerData['docker_image_hash'] ?? null,
                $sessionToken
            ]
        );

        $workerId = $this->db->lastInsertId();

        $this->logger->info("New worker registered: {$workerData['worker_id']}");

        return [
            'worker_id' => $workerData['worker_id'],
            'session_token' => $sessionToken,
            'status' => 'registered',
            'heartbeat_interval' => 30
        ];
    }

    private function updateExistingWorker(int $workerId, array $workerData): array
    {
        $sessionToken = bin2hex(random_bytes(32));

        $this->db->execute(
            'UPDATE workers SET 
                ip_address = ?,
                port = ?,
                status = "idle",
                gpu_info = ?,
                capabilities = ?,
                docker_image_hash = ?,
                session_token = ?,
                last_heartbeat = CURRENT_TIMESTAMP
             WHERE id = ?',
            [
                $workerData['ip_address'] ?? $_SERVER['REMOTE_ADDR'],
                $workerData['worker_port'] ?? 8001,
                json_encode($workerData['gpu_info'] ?? []),
                json_encode($workerData['capabilities'] ?? []),
                $workerData['docker_image_hash'] ?? null,
                $sessionToken,
                $workerId
            ]
        );

        $this->logger->info("Worker re-registered: {$workerData['worker_id']}");

        return [
            'worker_id' => $workerData['worker_id'],
            'session_token' => $sessionToken,
            'status' => 'updated',
            'heartbeat_interval' => 30
        ];
    }

    private function validateWorkerRegistration(array $data): void
    {
        if (empty($data['worker_id'])) {
            throw new RuntimeException('Worker ID is required');
        }

        if (empty($data['gpu_info'])) {
            throw new RuntimeException('GPU information is required');
        }

        if (empty($data['capabilities']) || !is_array($data['capabilities'])) {
            throw new RuntimeException('Worker capabilities are required');
        }
    }

    private function calculatePerformanceScore(array $data): float
    {
        $baseScore = 100.0;

        if (isset($data['success_rate'])) {
            $successRate = (float)$data['success_rate'];
            $baseScore *= ($successRate / 100);
        }

        if (isset($data['avg_response_time'])) {
            $avgTime = (float)$data['avg_response_time'];
            if ($avgTime > 0) {
                $timePenalty = exp(-$avgTime / 10000);
                $baseScore *= $timePenalty;
            }
        }

        if (isset($data['gpu_memory_used'], $data['gpu_memory_total'])) {
            $gpuUsage = $data['gpu_memory_used'] / $data['gpu_memory_total'];
            if ($gpuUsage > 0.9) {
                $baseScore *= 0.8;
            }
        }

        return round(max(0.0, min(100.0, $baseScore)), 2);
    }
}
