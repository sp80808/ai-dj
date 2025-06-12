<?php

declare(strict_types=1);

namespace Obsidian\Models;

class GenerationRequest
{
    public int $id;
    public string $requestId;
    public int $userId;
    public ?int $workerId;
    public string $prompt;
    public float $bpm;
    public ?string $keySignature;
    public ?array $preferredStems;
    public float $generationDuration;
    public int $sampleRate;
    public string $status;
    public \DateTime $createdAt;
    public ?\DateTime $startedAt;
    public ?\DateTime $completedAt;
    public ?int $responseTimeMs;
    public ?string $errorMessage;
    public ?int $fileSizeBytes;
    public ?array $userMemory;

    public const STATUS_QUEUED = 'queued';
    public const STATUS_PROCESSING = 'processing';
    public const STATUS_COMPLETED = 'completed';
    public const STATUS_FAILED = 'failed';
    public const STATUS_TIMEOUT = 'timeout';

    public function __construct(array $data = [])
    {
        $this->id = $data['id'] ?? 0;
        $this->requestId = $data['request_id'] ?? '';
        $this->userId = $data['user_id'] ?? 0;
        $this->workerId = $data['worker_id'] ?? null;
        $this->prompt = $data['prompt'] ?? '';
        $this->bpm = $data['bpm'] ?? 126.0;
        $this->keySignature = $data['key_signature'] ?? null;
        $this->preferredStems = isset($data['preferred_stems'])
            ? (is_string($data['preferred_stems']) ? json_decode($data['preferred_stems'], true) : $data['preferred_stems'])
            : null;
        $this->generationDuration = $data['generation_duration'] ?? 6.0;
        $this->sampleRate = $data['sample_rate'] ?? 48000;
        $this->status = $data['status'] ?? self::STATUS_QUEUED;
        $this->createdAt = isset($data['created_at'])
            ? new \DateTime($data['created_at'])
            : new \DateTime();
        $this->startedAt = isset($data['started_at'])
            ? new \DateTime($data['started_at'])
            : null;
        $this->completedAt = isset($data['completed_at'])
            ? new \DateTime($data['completed_at'])
            : null;
        $this->responseTimeMs = $data['response_time_ms'] ?? null;
        $this->errorMessage = $data['error_message'] ?? null;
        $this->fileSizeBytes = $data['file_size_bytes'] ?? null;
        $this->userMemory = isset($data['user_memory'])
            ? (is_string($data['user_memory']) ? json_decode($data['user_memory'], true) : $data['user_memory'])
            : null;
    }

    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'request_id' => $this->requestId,
            'user_id' => $this->userId,
            'worker_id' => $this->workerId,
            'prompt' => $this->prompt,
            'bpm' => $this->bpm,
            'key_signature' => $this->keySignature,
            'preferred_stems' => $this->preferredStems,
            'generation_duration' => $this->generationDuration,
            'sample_rate' => $this->sampleRate,
            'status' => $this->status,
            'created_at' => $this->createdAt->format('c'),
            'started_at' => $this->startedAt?->format('c'),
            'completed_at' => $this->completedAt?->format('c'),
            'response_time_ms' => $this->responseTimeMs,
            'error_message' => $this->errorMessage,
            'file_size_bytes' => $this->fileSizeBytes
        ];
    }

    public function getDurationSeconds(): ?float
    {
        if (!$this->startedAt || !$this->completedAt) {
            return null;
        }

        $diff = $this->completedAt->getTimestamp() - $this->startedAt->getTimestamp();
        return round($diff, 2);
    }

    public function isCompleted(): bool
    {
        return $this->status === self::STATUS_COMPLETED;
    }

    public function isFailed(): bool
    {
        return in_array($this->status, [self::STATUS_FAILED, self::STATUS_TIMEOUT]);
    }

    public function isProcessing(): bool
    {
        return in_array($this->status, [self::STATUS_QUEUED, self::STATUS_PROCESSING]);
    }
}
