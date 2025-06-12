<?php

declare(strict_types=1);

namespace Obsidian\Models;

class Worker
{
    public int $id;
    public string $workerId;
    public ?string $workerName;
    public string $ipAddress;
    public int $port;
    public string $status;
    public array $gpuInfo;
    public array $capabilities;
    public float $performanceScore;
    public int $totalRequests;
    public int $successfulRequests;
    public int $failedRequests;
    public float $avgResponseTime;
    public \DateTime $lastHeartbeat;
    public ?string $dockerImageHash;
    public string $sessionToken;
    public \DateTime $createdAt;
    public \DateTime $updatedAt;

    public const STATUS_REGISTERING = 'registering';
    public const STATUS_IDLE = 'idle';
    public const STATUS_BUSY = 'busy';
    public const STATUS_ERROR = 'error';
    public const STATUS_OFFLINE = 'offline';

    public function __construct(array $data = [])
    {
        $this->id = $data['id'] ?? 0;
        $this->workerId = $data['worker_id'] ?? '';
        $this->workerName = $data['worker_name'] ?? null;
        $this->ipAddress = $data['ip_address'] ?? '';
        $this->port = $data['port'] ?? 8001;
        $this->status = $data['status'] ?? self::STATUS_REGISTERING;
        $this->gpuInfo = isset($data['gpu_info'])
            ? (is_string($data['gpu_info']) ? json_decode($data['gpu_info'], true) : $data['gpu_info'])
            : [];
        $this->capabilities = isset($data['capabilities'])
            ? (is_string($data['capabilities']) ? json_decode($data['capabilities'], true) : $data['capabilities'])
            : [];
        $this->performanceScore = $data['performance_score'] ?? 100.0;
        $this->totalRequests = $data['total_requests'] ?? 0;
        $this->successfulRequests = $data['successful_requests'] ?? 0;
        $this->failedRequests = $data['failed_requests'] ?? 0;
        $this->avgResponseTime = $data['avg_response_time'] ?? 0.0;
        $this->lastHeartbeat = isset($data['last_heartbeat'])
            ? new \DateTime($data['last_heartbeat'])
            : new \DateTime();
        $this->dockerImageHash = $data['docker_image_hash'] ?? null;
        $this->sessionToken = $data['session_token'] ?? '';
        $this->createdAt = isset($data['created_at'])
            ? new \DateTime($data['created_at'])
            : new \DateTime();
        $this->updatedAt = isset($data['updated_at'])
            ? new \DateTime($data['updated_at'])
            : new \DateTime();
    }

    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'worker_id' => $this->workerId,
            'worker_name' => $this->workerName,
            'ip_address' => $this->ipAddress,
            'port' => $this->port,
            'status' => $this->status,
            'gpu_info' => $this->gpuInfo,
            'capabilities' => $this->capabilities,
            'performance_score' => $this->performanceScore,
            'total_requests' => $this->totalRequests,
            'successful_requests' => $this->successfulRequests,
            'failed_requests' => $this->failedRequests,
            'avg_response_time' => $this->avgResponseTime,
            'last_heartbeat' => $this->lastHeartbeat->format('c'),
            'docker_image_hash' => $this->dockerImageHash,
            'created_at' => $this->createdAt->format('c'),
            'updated_at' => $this->updatedAt->format('c')
        ];
    }

    public function getShortId(): string
    {
        return substr($this->workerId, 0, 8);
    }

    public function isOnline(): bool
    {
        $maxIdleTime = 300;
        $now = new \DateTime();
        $diff = $now->getTimestamp() - $this->lastHeartbeat->getTimestamp();
        return $diff <= $maxIdleTime && $this->status !== self::STATUS_OFFLINE;
    }

    public function isAvailable(): bool
    {
        return $this->isOnline() && in_array($this->status, [self::STATUS_IDLE, self::STATUS_BUSY]);
    }

    public function getSuccessRate(): float
    {
        if ($this->totalRequests === 0) {
            return 100.0;
        }
        return round(($this->successfulRequests / $this->totalRequests) * 100, 2);
    }

    public function getGpuMemoryUsage(): array
    {
        $used = $this->gpuInfo['gpu_memory_used'] ?? 0;
        $total = $this->gpuInfo['gpu_memory_total'] ?? 0;
        $percentage = $total > 0 ? ($used / $total) * 100 : 0;

        return [
            'used_gb' => round($used, 2),
            'total_gb' => round($total, 2),
            'percentage' => round($percentage, 1)
        ];
    }

    public function hasCapability(string $capability): bool
    {
        return in_array($capability, $this->capabilities);
    }
}
