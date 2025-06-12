<?php

declare(strict_types=1);

namespace Obsidian\Controllers;

use Obsidian\Services\AuthService;
use Obsidian\Services\LoadBalancerService;
use Obsidian\Services\WorkerPoolService;
use Obsidian\Services\WavSecurityService;
use Obsidian\Services\CertificateService;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Ramsey\Uuid\Uuid;
use RuntimeException;

class GenerateController
{
    private AuthService $authService;
    private LoadBalancerService $loadBalancer;
    private WorkerPoolService $workerPool;
    private WavSecurityService $wavSecurity;
    private CertificateService $certificate;

    public function __construct(
        AuthService $authService,
        LoadBalancerService $loadBalancer,
        WorkerPoolService $workerPool,
        WavSecurityService $wavSecurity,
        CertificateService $certificate
    ) {
        $this->authService = $authService;
        $this->loadBalancer = $loadBalancer;
        $this->workerPool = $workerPool;
        $this->wavSecurity = $wavSecurity;
        $this->certificate = $certificate;
    }

    public function generate(Request $request, Response $response): Response
    {
        $startTime = microtime(true);
        $user = $request->getAttribute('user');
        $requestId = Uuid::uuid4()->toString();

        try {
            $limitCheck = $this->authService->checkDailyLimit($user['user_id']);

            if (!$limitCheck['allowed']) {
                return $this->errorResponse(
                    $response,
                    'DAILY_LIMIT_EXCEEDED',
                    "Daily generation limit reached ({$limitCheck['used']}/{$limitCheck['limit']}). Resets at {$limitCheck['resets_at']}",
                    429
                );
            }

            $data = $request->getParsedBody() ?? [];
            $generationParams = $this->validateGenerationInput($data);

            $userSession = $this->authService->getUserSession($user['user_id']);
            $conversationMemory = $userSession ? json_decode($userSession['conversation_memory'], true) : null;

            $selectedWorker = $this->loadBalancer->selectWorker($generationParams);
            if (!$selectedWorker) {
                return $this->errorResponse(
                    $response,
                    'NO_WORKERS_AVAILABLE',
                    'No workers available to process your request. Please try again later.',
                    503
                );
            }

            $workerRequest = [
                'prompt' => $generationParams['prompt'],
                'bpm' => $generationParams['bpm'],
                'key' => $generationParams['key'],
                'measures' => $generationParams['measures'],
                'preferred_stems' => $generationParams['preferred_stems'],
                'generation_duration' => $generationParams['generation_duration'],
                'sample_rate' => $generationParams['sample_rate'],
                'user_memory' => $conversationMemory,
                'request_id' => $requestId,
                'anonymized_user_id' => hash('sha256', $user['user_id'] . $requestId)
            ];

            $workerResponse = $this->workerPool->sendRequestToWorker(
                $selectedWorker['worker_id'],
                $workerRequest
            );

            if (!$workerResponse['success']) {
                throw new RuntimeException('Worker request failed');
            }

            $cleaningResult = $this->wavSecurity->validateAndCleanWav(
                $workerResponse['data'],
                $generationParams
            );

            if (!$cleaningResult['success']) {
                throw new RuntimeException('WAV validation/cleaning failed');
            }

            $audioInfo = $this->wavSecurity->getAudioInfo($cleaningResult['cleaned_data']);

            $responseHeaders = $workerResponse['headers'] ?? [];
            if (isset($responseHeaders['X-Updated-Memory'][0])) {
                $updatedMemory = json_decode($responseHeaders['X-Updated-Memory'][0], true);
                if ($updatedMemory) {
                    $this->authService->updateUserSession(
                        $user['user_id'],
                        $updatedMemory,
                        $generationParams['prompt']
                    );
                }
            }

            $certificate = $this->certificate->generateCertificate([
                'request_id' => $requestId,
                'user_id' => $user['user_id'],
                'username' => $user['username'],
                'prompt' => $generationParams['prompt'],
                'bpm' => $generationParams['bpm'],
                'key_signature' => $generationParams['key'],
                'generation_duration' => $generationParams['generation_duration'],
                'sample_rate' => $generationParams['sample_rate'],
                'preferred_stems' => $generationParams['preferred_stems'],
                'worker_id' => $selectedWorker['worker_id']
            ], $cleaningResult['sha256']);

            $this->loadBalancer->releaseWorker($selectedWorker['worker_id']);

            $totalTime = (microtime(true) - $startTime) * 1000;

            if (!$this->authService->incrementDailyUsage($user['user_id'])) {
                return $this->errorResponse($response, 'LIMIT_ERROR', 'Failed to update usage counter', 500);
            }

            $responseHeaders = [
                'X-Daily-Used' => (string)($limitCheck['used'] + 1),
                'X-Daily-Limit' => (string)$limitCheck['limit'],
                'X-Daily-Remaining' => (string)max(0, $limitCheck['remaining'] - 1),
                'X-Daily-Resets' => $limitCheck['resets_at'],
                'X-Request-ID' => $requestId,
                'X-Duration' => (string)$audioInfo['duration'],
                'X-BPM' => (string)$generationParams['bpm'],
                'X-Key' => $generationParams['key'] ?? '',
                'X-Sample-Rate' => (string)$audioInfo['sample_rate'],
                'X-Channels' => (string)$audioInfo['channels'],
                'X-Processing-Time' => (string)round($totalTime, 2),
                'X-Worker-ID' => substr($selectedWorker['worker_id'], 0, 8),
                'X-Certificate-ID' => $requestId,
                'X-SHA256' => $cleaningResult['sha256'],
                'Content-Disposition' => 'attachment; filename="obsidian_' . substr($requestId, 0, 8) . '.wav"'
            ];

            if (isset($certificate['signature'])) {
                $responseHeaders['X-Certificate-Signature'] = $certificate['signature'];
                $responseHeaders['X-Public-Key-URL'] = $_ENV['APP_URL'] . '/api/v1/certificate/public-key';
            }

            $response->getBody()->write($cleaningResult['cleaned_data']);

            foreach ($responseHeaders as $header => $value) {
                $response = $response->withHeader($header, $value);
            }

            return $response->withHeader('Content-Type', 'audio/wav');
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'GENERATION_FAILED', $e->getMessage(), 500);
        } catch (\Exception $e) {
            return $this->errorResponse(
                $response,
                'SERVER_ERROR',
                'An unexpected error occurred during generation',
                500
            );
        }
    }

    public function status(Request $request, Response $response, array $args): Response
    {
        $requestId = $args['requestId'] ?? '';
        $user = $request->getAttribute('user');

        if (empty($requestId)) {
            return $this->errorResponse($response, 'INVALID_REQUEST_ID', 'Request ID is required', 400);
        }

        $certificate = $this->certificate->searchCertificates([
            'request_id' => $requestId
        ]);

        if (empty($certificate)) {
            return $this->errorResponse(
                $response,
                'REQUEST_NOT_FOUND',
                'Request not found or access denied',
                404
            );
        }

        $cert = $certificate[0];
        if ($cert['data']['user_id'] !== $user['user_id']) {
            return $this->errorResponse(
                $response,
                'ACCESS_DENIED',
                'You do not have access to this request',
                403
            );
        }

        $responseData = [
            'success' => true,
            'data' => [
                'request_id' => $requestId,
                'status' => 'completed',
                'timestamp' => $cert['data']['timestamp'],
                'prompt' => $cert['data']['prompt'],
                'bpm' => $cert['data']['bpm'],
                'key_signature' => $cert['data']['key_signature'],
                'duration' => $cert['data']['generation_duration'],
                'sample_rate' => $cert['data']['sample_rate'],
                'wav_sha256' => $cert['data']['wav_sha256'],
                'certificate_valid' => true,
                'public_ledger_url' => $_ENV['APP_URL'] . '/api/v1/certificate/ledger'
            ]
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }

    private function validateGenerationInput(array $data): array
    {
        if (empty($data['prompt']) || strlen(trim($data['prompt'])) < 3) {
            throw new RuntimeException('Prompt must be at least 3 characters long');
        }

        if (empty($data['bpm']) || !is_numeric($data['bpm'])) {
            throw new RuntimeException('Valid BPM is required');
        }

        $bpm = (float)$data['bpm'];
        if ($bpm < 60 || $bpm > 200) {
            throw new RuntimeException('BPM must be between 60 and 200');
        }

        $generationDuration = isset($data['generation_duration'])
            ? (float)$data['generation_duration'] : 6.0;
        if ($generationDuration < 1.0 || $generationDuration > 30.0) {
            throw new RuntimeException('Generation duration must be between 1 and 30 seconds');
        }

        $sampleRate = isset($data['sample_rate'])
            ? (int)$data['sample_rate'] : 48000;
        if (!in_array($sampleRate, [44100, 48000])) {
            throw new RuntimeException('Sample rate must be 44100 or 48000 Hz');
        }

        $measures = isset($data['measures']) ? (int)$data['measures'] : 4;
        if ($measures < 1 || $measures > 16) {
            throw new RuntimeException('Measures must be between 1 and 16');
        }

        $preferredStems = null;
        if (!empty($data['preferred_stems'])) {
            if (!is_array($data['preferred_stems'])) {
                throw new RuntimeException('Preferred stems must be an array');
            }

            $validStems = ['drums', 'bass', 'other', 'vocals', 'guitar', 'piano'];
            $invalidStems = array_diff($data['preferred_stems'], $validStems);
            if (!empty($invalidStems)) {
                throw new RuntimeException('Invalid stems: ' . implode(', ', $invalidStems));
            }

            $preferredStems = $data['preferred_stems'];
        }

        $key = null;
        if (!empty($data['key'])) {
            $validKeys = [
                'C major',
                'C minor',
                'C# major',
                'C# minor',
                'Db major',
                'Db minor',
                'D major',
                'D minor',
                'D# major',
                'D# minor',
                'Eb major',
                'Eb minor',
                'E major',
                'E minor',
                'F major',
                'F minor',
                'F# major',
                'F# minor',
                'Gb major',
                'Gb minor',
                'G major',
                'G minor',
                'G# major',
                'G# minor',
                'Ab major',
                'Ab minor',
                'A major',
                'A minor',
                'A# major',
                'A# minor',
                'Bb major',
                'Bb minor',
                'B major',
                'B minor'
            ];

            if (!in_array($data['key'], $validKeys)) {
                throw new RuntimeException('Invalid key signature');
            }

            $key = $data['key'];
        }

        return [
            'prompt' => trim($data['prompt']),
            'bpm' => $bpm,
            'key' => $key,
            'measures' => $measures,
            'preferred_stems' => $preferredStems,
            'generation_duration' => $generationDuration,
            'sample_rate' => $sampleRate
        ];
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
