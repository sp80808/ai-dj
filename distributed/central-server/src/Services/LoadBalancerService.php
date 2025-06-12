<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Monolog\Logger;

class LoadBalancerService
{
    private WorkerPoolService $workerPool;
    private Logger $logger;
    private array $strategies = [
        'performance' => 'selectByPerformance',
        'round_robin' => 'selectRoundRobin',
        'least_loaded' => 'selectLeastLoaded',
        'random' => 'selectRandom'
    ];

    public function __construct(WorkerPoolService $workerPool, Logger $logger)
    {
        $this->workerPool = $workerPool;
        $this->logger = $logger;
    }

    public function selectWorker(array $requestData = [], string $strategy = 'performance'): ?array
    {
        $availableWorkers = $this->workerPool->getAvailableWorkers();

        if (empty($availableWorkers)) {
            $this->logger->warning('No available workers for request');
            return null;
        }

        $suitableWorkers = $this->filterWorkersByRequirements($availableWorkers, $requestData);

        if (empty($suitableWorkers)) {
            $this->logger->warning('No suitable workers found for request requirements');
            return null;
        }

        $selectedWorker = $this->applySelectionStrategy($suitableWorkers, $strategy, $requestData);

        if ($selectedWorker) {
            $this->logger->info("Selected worker {$selectedWorker['worker_id']} using {$strategy} strategy");

            $this->workerPool->markWorkerBusy($selectedWorker['worker_id']);
        }

        return $selectedWorker;
    }

    public function releaseWorker(string $workerId): void
    {
        $this->workerPool->markWorkerIdle($workerId);
        $this->logger->debug("Released worker {$workerId}");
    }

    public function getWorkerLoad(): array
    {
        $workers = $this->workerPool->getAvailableWorkers();
        $load = [
            'total_workers' => count($workers),
            'idle_workers' => 0,
            'busy_workers' => 0,
            'avg_performance' => 0,
            'avg_response_time' => 0
        ];

        if (empty($workers)) {
            return $load;
        }

        $totalPerformance = 0;
        $totalResponseTime = 0;

        foreach ($workers as $worker) {
            if ($worker['status'] === 'idle') {
                $load['idle_workers']++;
            } elseif ($worker['status'] === 'busy') {
                $load['busy_workers']++;
            }

            $totalPerformance += $worker['performance_score'];
            $totalResponseTime += $worker['avg_response_time'];
        }

        $load['avg_performance'] = round($totalPerformance / count($workers), 2);
        $load['avg_response_time'] = round($totalResponseTime / count($workers), 2);

        return $load;
    }

    public function healthCheck(): array
    {
        $removedCount = $this->workerPool->removeOfflineWorkers();

        $load = $this->getWorkerLoad();
        $healthScore = $this->calculateHealthScore($load);

        return [
            'status' => $healthScore > 70 ? 'healthy' : ($healthScore > 30 ? 'degraded' : 'critical'),
            'health_score' => $healthScore,
            'load' => $load,
            'removed_offline_workers' => $removedCount,
            'timestamp' => date('c')
        ];
    }

    private function filterWorkersByRequirements(array $workers, array $requestData): array
    {
        $filtered = [];

        foreach ($workers as $worker) {
            if ($worker['status'] === 'busy' && !$this->isHighLoadSituation()) {
                continue;
            }

            if (!$this->hasAdequateGpuMemory($worker, $requestData)) {
                continue;
            }

            if (!$this->hasRequiredCapabilities($worker, $requestData)) {
                continue;
            }

            if ($worker['performance_score'] < 20.0) {
                continue;
            }

            $filtered[] = $worker;
        }

        return $filtered;
    }

    private function applySelectionStrategy(array $workers, string $strategy, array $requestData): ?array
    {
        if (!isset($this->strategies[$strategy])) {
            $strategy = 'performance';
        }

        $method = $this->strategies[$strategy];
        return $this->{$method}($workers, $requestData);
    }

    private function selectByPerformance(array $workers, array $requestData): ?array
    {
        usort($workers, function ($a, $b) {
            $performanceDiff = $b['performance_score'] - $a['performance_score'];
            if (abs($performanceDiff) < 0.1) {
                return $a['avg_response_time'] <=> $b['avg_response_time'];
            }
            return $performanceDiff <=> 0;
        });

        $idleWorkers = array_filter($workers, fn($w) => $w['status'] === 'idle');
        if (!empty($idleWorkers)) {
            return $idleWorkers[0];
        }

        return $workers[0] ?? null;
    }

    private function selectRoundRobin(array $workers, array $requestData): ?array
    {
        static $lastSelectedIndex = -1;

        $idleWorkers = array_filter($workers, fn($w) => $w['status'] === 'idle');

        if (empty($idleWorkers)) {
            return $this->selectByPerformance($workers, $requestData);
        }

        $lastSelectedIndex = ($lastSelectedIndex + 1) % count($idleWorkers);
        return array_values($idleWorkers)[$lastSelectedIndex];
    }

    private function selectLeastLoaded(array $workers, array $requestData): ?array
    {
        usort($workers, function ($a, $b) {
            $gpuInfoA = json_decode($a['gpu_info'], true) ?? [];
            $gpuInfoB = json_decode($b['gpu_info'], true) ?? [];

            $loadA = ($gpuInfoA['gpu_memory_used'] ?? 0) + ($gpuInfoA['queue_size'] ?? 0) * 10;
            $loadB = ($gpuInfoB['gpu_memory_used'] ?? 0) + ($gpuInfoB['queue_size'] ?? 0) * 10;

            return $loadA <=> $loadB;
        });

        $idleWorkers = array_filter($workers, fn($w) => $w['status'] === 'idle');
        if (!empty($idleWorkers)) {
            return $idleWorkers[0];
        }

        return $workers[0] ?? null;
    }

    private function selectRandom(array $workers, array $requestData): ?array
    {
        $idleWorkers = array_filter($workers, fn($w) => $w['status'] === 'idle');

        if (!empty($idleWorkers)) {
            return $idleWorkers[array_rand($idleWorkers)];
        }

        return $workers[array_rand($workers)] ?? null;
    }

    private function hasAdequateGpuMemory(array $worker, array $requestData): bool
    {
        $gpuInfo = json_decode($worker['gpu_info'], true) ?? [];

        if (empty($gpuInfo)) {
            return false;
        }

        $totalMemory = $gpuInfo['gpu_memory_total'] ?? 0;
        $usedMemory = $gpuInfo['gpu_memory_used'] ?? 0;
        $freeMemory = $totalMemory - $usedMemory;

        $estimatedRequirement = $this->estimateMemoryRequirement($requestData);

        return $freeMemory >= $estimatedRequirement;
    }

    private function hasRequiredCapabilities(array $worker, array $requestData): bool
    {
        $capabilities = json_decode($worker['capabilities'], true) ?? [];

        $required = ['stable-audio-open', 'llm'];

        if (!empty($requestData['preferred_stems'])) {
            $required[] = 'stems-extraction';
        }

        foreach ($required as $capability) {
            if (!in_array($capability, $capabilities)) {
                return false;
            }
        }

        return true;
    }

    private function isHighLoadSituation(): bool
    {
        $load = $this->getWorkerLoad();

        $idleRatio = $load['total_workers'] > 0
            ? $load['idle_workers'] / $load['total_workers']
            : 0;

        return $idleRatio < 0.2;
    }

    private function estimateMemoryRequirement(array $requestData): float
    {
        $baseMemory = 8.0;

        $duration = $requestData['generation_duration'] ?? 6.0;
        $durationMultiplier = min(2.0, $duration / 6.0);

        $stemsMultiplier = !empty($requestData['preferred_stems']) ? 1.5 : 1.0;

        return $baseMemory * $durationMultiplier * $stemsMultiplier;
    }

    private function calculateHealthScore(array $load): float
    {
        $score = 100.0;

        if ($load['total_workers'] === 0) {
            return 0.0;
        }

        $availabilityRatio = $load['idle_workers'] / $load['total_workers'];
        if ($availabilityRatio < 0.1) {
            $score *= 0.3;
        } elseif ($availabilityRatio < 0.3) {
            $score *= 0.7;
        }

        if ($load['avg_performance'] < 50) {
            $score *= 0.8;
        } elseif ($load['avg_performance'] < 30) {
            $score *= 0.5;
        }

        if ($load['avg_response_time'] > 30000) {
            $score *= 0.6;
        } elseif ($load['avg_response_time'] > 15000) {
            $score *= 0.8;
        }

        return round(max(0.0, $score), 1);
    }
}
