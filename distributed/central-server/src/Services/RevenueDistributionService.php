<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Monolog\Logger;
use Obsidian\Utils\Database;
use RuntimeException;

class RevenueDistributionService
{
    private Database $db;
    private Logger $logger;
    private float $workerShare;
    private float $platformMaintenance;
    private float $developmentFund;

    public function __construct(Database $db, Logger $logger)
    {
        $this->db = $db;
        $this->logger = $logger;

        $this->workerShare = (float)($_ENV['DEFAULT_WORKER_SHARE'] ?? 0.75);
        $this->platformMaintenance = (float)($_ENV['PLATFORM_MAINTENANCE'] ?? 0.15);
        $this->developmentFund = (float)($_ENV['DEVELOPMENT_FUND'] ?? 0.10);

        $total = $this->workerShare + $this->platformMaintenance + $this->developmentFund;
        if (abs($total - 1.0) > 0.001) {
            throw new RuntimeException("Revenue distribution ratios must sum to 1.0, got {$total}");
        }
    }

    public function processMonthlyDistribution(string $month): array
    {
        $this->logger->info("Starting monthly revenue distribution for {$month}");

        try {
            $this->db->beginTransaction();

            $totalDonations = $this->getTotalDonationsForMonth($month);

            if ($totalDonations <= 0) {
                $this->logger->info("No donations to distribute for {$month}");
                $this->db->rollback();
                return ['success' => false, 'message' => 'No donations for this month'];
            }

            $allocations = $this->calculateAllocations($totalDonations);
            $workerContributions = $this->calculateWorkerContributions($month);

            if (empty($workerContributions)) {
                $this->logger->warning("No worker contributions found for {$month}");
                $this->db->rollback();
                return ['success' => false, 'message' => 'No active workers for this month'];
            }

            $workerDistributions = $this->distributeWorkerShares($allocations['worker_total'], $workerContributions, $month);

            $this->recordDistributions($workerDistributions, $month);

            $this->markDonationsProcessed($month);

            $this->db->commit();

            $result = [
                'success' => true,
                'month' => $month,
                'total_donations' => $totalDonations,
                'allocations' => $allocations,
                'worker_count' => count($workerDistributions),
                'distributions' => $workerDistributions
            ];

            $this->logger->info("Monthly distribution completed successfully", $result);

            return $result;
        } catch (\Exception $e) {
            $this->db->rollback();
            $this->logger->error("Monthly distribution failed: " . $e->getMessage());
            throw $e;
        }
    }

    public function addDonation(array $donationData): bool
    {
        try {
            $this->db->execute(
                'INSERT INTO donation_sources (source_type, amount, currency, donor_name, message) 
                 VALUES (?, ?, ?, ?, ?)',
                [
                    $donationData['source_type'],
                    $donationData['amount'],
                    $donationData['currency'] ?? 'USD',
                    $donationData['donor_name'] ?? null,
                    $donationData['message'] ?? null
                ]
            );

            $this->logger->info("Donation added", [
                'source' => $donationData['source_type'],
                'amount' => $donationData['amount'],
                'currency' => $donationData['currency'] ?? 'USD'
            ]);

            return true;
        } catch (\Exception $e) {
            $this->logger->error("Failed to add donation: " . $e->getMessage());
            return false;
        }
    }

    public function updateWorkerContributions(int $workerId, array $contributionData): void
    {
        $month = $contributionData['month'] ?? date('Y-m');

        $this->db->execute(
            'INSERT INTO worker_contributions 
             (worker_id, month, total_generations, total_compute_seconds, uptime_hours, performance_score_avg, contribution_score)
             VALUES (?, ?, ?, ?, ?, ?, ?)
             ON DUPLICATE KEY UPDATE
             total_generations = VALUES(total_generations),
             total_compute_seconds = VALUES(total_compute_seconds),
             uptime_hours = VALUES(uptime_hours),
             performance_score_avg = VALUES(performance_score_avg),
             contribution_score = VALUES(contribution_score)',
            [
                $workerId,
                $month,
                $contributionData['total_generations'] ?? 0,
                $contributionData['total_compute_seconds'] ?? 0,
                $contributionData['uptime_hours'] ?? 0,
                $contributionData['performance_score_avg'] ?? 100.0,
                $contributionData['contribution_score'] ?? 0.0
            ]
        );
    }

    public function getMonthlyTransparencyReport(string $month): array
    {
        $donations = $this->db->query(
            'SELECT source_type, SUM(amount) as total_amount, COUNT(*) as donation_count
             FROM donation_sources 
             WHERE DATE_FORMAT(received_at, "%Y-%m") = ? AND processed = 1
             GROUP BY source_type',
            [$month]
        );

        $distributions = $this->db->query(
            'SELECT w.worker_id, w.worker_name, rs.worker_share, rs.amount_total
             FROM revenue_sharing rs
             JOIN workers w ON rs.worker_id = w.id
             WHERE DATE_FORMAT(rs.period_start, "%Y-%m") = ?
             ORDER BY rs.worker_share DESC',
            [$month]
        );

        $contributions = $this->db->query(
            'SELECT w.worker_id, w.worker_name, wc.total_generations, wc.uptime_hours, wc.contribution_score
             FROM worker_contributions wc
             JOIN workers w ON wc.worker_id = w.id
             WHERE wc.month = ?
             ORDER BY wc.contribution_score DESC',
            [$month]
        );

        $totalDonations = array_sum(array_column($donations, 'total_amount'));
        $totalDistributed = array_sum(array_column($distributions, 'worker_share'));

        return [
            'month' => $month,
            'total_donations' => $totalDonations,
            'total_distributed_to_workers' => $totalDistributed,
            'platform_maintenance' => $totalDonations * $this->platformMaintenance,
            'development_fund' => $totalDonations * $this->developmentFund,
            'donation_sources' => $donations,
            'worker_distributions' => $distributions,
            'worker_contributions' => $contributions,
            'active_workers' => count($contributions)
        ];
    }

    private function getTotalDonationsForMonth(string $month): float
    {
        $result = $this->db->queryOne(
            'SELECT SUM(amount) as total FROM donation_sources 
             WHERE DATE_FORMAT(received_at, "%Y-%m") = ? AND processed = 0',
            [$month]
        );

        return (float)($result['total'] ?? 0.0);
    }

    private function calculateAllocations(float $totalDonations): array
    {
        return [
            'total_donations' => $totalDonations,
            'worker_total' => $totalDonations * $this->workerShare,
            'platform_maintenance' => $totalDonations * $this->platformMaintenance,
            'development_fund' => $totalDonations * $this->developmentFund
        ];
    }

    private function calculateWorkerContributions(string $month): array
    {
        $workers = $this->db->query(
            'SELECT w.id, w.worker_id, w.worker_name, w.total_requests, w.successful_requests, 
                    w.performance_score, w.avg_response_time
             FROM workers w 
             WHERE w.status != "offline" 
               AND w.total_requests > 0',
            []
        );

        $contributions = [];
        $totalScore = 0;

        foreach ($workers as $worker) {
            $generationWeight = 0.5;
            $uptimeWeight = 0.3;
            $performanceWeight = 0.2;

            $generationScore = min(100, ($worker['successful_requests'] ?? 0) * 2);
            $performanceScore = $worker['performance_score'] ?? 50;
            $uptimeScore = 100;

            $contributionScore = (
                $generationScore * $generationWeight +
                $uptimeScore * $uptimeWeight +
                $performanceScore * $performanceWeight
            );

            $contributions[$worker['id']] = [
                'worker_id' => $worker['id'],
                'worker_name' => $worker['worker_name'],
                'total_generations' => $worker['successful_requests'] ?? 0,
                'performance_score' => $performanceScore,
                'contribution_score' => $contributionScore
            ];

            $totalScore += $contributionScore;

            $this->updateWorkerContributions($worker['id'], [
                'month' => $month,
                'total_generations' => $worker['successful_requests'] ?? 0,
                'performance_score_avg' => $performanceScore,
                'contribution_score' => $contributionScore
            ]);
        }

        foreach ($contributions as &$contribution) {
            $contribution['share_percentage'] = $totalScore > 0
                ? $contribution['contribution_score'] / $totalScore
                : 0;
        }

        return $contributions;
    }

    private function distributeWorkerShares(float $totalWorkerAmount, array $contributions, string $month): array
    {
        $distributions = [];
        $periodStart = $month . '-01';
        $periodEnd = date('Y-m-t', strtotime($periodStart));

        foreach ($contributions as $contribution) {
            $workerShare = $totalWorkerAmount * $contribution['share_percentage'];

            $distributions[] = [
                'worker_id' => $contribution['worker_id'],
                'worker_name' => $contribution['worker_name'],
                'contribution_score' => $contribution['contribution_score'],
                'share_percentage' => $contribution['share_percentage'],
                'worker_share' => $workerShare,
                'period_start' => $periodStart,
                'period_end' => $periodEnd
            ];
        }

        return $distributions;
    }

    private function recordDistributions(array $distributions, string $month): void
    {
        foreach ($distributions as $distribution) {
            $this->db->execute(
                'INSERT INTO revenue_sharing 
                 (worker_id, source_type, amount_total, worker_share, platform_maintenance, development_fund, period_start, period_end)
                 VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
                [
                    $distribution['worker_id'],
                    'monthly_distribution',
                    $distribution['worker_share'],
                    $distribution['worker_share'],
                    0,
                    0,
                    $distribution['period_start'],
                    $distribution['period_end']
                ]
            );
        }
    }

    private function markDonationsProcessed(string $month): void
    {
        $this->db->execute(
            'UPDATE donation_sources SET processed = 1 
             WHERE DATE_FORMAT(received_at, "%Y-%m") = ? AND processed = 0',
            [$month]
        );
    }

    public function getAvailableMonths(): array
    {
        $months = $this->db->query(
            'SELECT DISTINCT DATE_FORMAT(received_at, "%Y-%m") as month
             FROM donation_sources 
             WHERE processed = 1
             ORDER BY month DESC
             LIMIT 12',
            []
        );

        return array_column($months, 'month');
    }

    public function getDonationStats(): array
    {
        $totalAllTime = $this->db->queryOne(
            'SELECT 
                SUM(amount) as total_donations,
                COUNT(*) as total_count,
                AVG(amount) as avg_donation
             FROM donation_sources 
             WHERE processed = 1'
        );

        $bySource = $this->db->query(
            'SELECT 
                source_type,
                SUM(amount) as total_amount,
                COUNT(*) as donation_count,
                AVG(amount) as avg_amount
             FROM donation_sources 
             WHERE processed = 1
             GROUP BY source_type
             ORDER BY total_amount DESC'
        );

        $monthlyTrend = $this->db->query(
            'SELECT 
                DATE_FORMAT(received_at, "%Y-%m") as month,
                SUM(amount) as total_amount,
                COUNT(*) as donation_count
             FROM donation_sources 
             WHERE processed = 1 
               AND received_at >= DATE_SUB(NOW(), INTERVAL 6 MONTH)
             GROUP BY DATE_FORMAT(received_at, "%Y-%m")
             ORDER BY month DESC'
        );

        $activeWorkers = $this->db->queryOne(
            'SELECT COUNT(DISTINCT worker_id) as count
             FROM revenue_sharing
             WHERE processed_at >= DATE_SUB(NOW(), INTERVAL 3 MONTH)'
        );

        return [
            'all_time' => [
                'total_donations' => (float)($totalAllTime['total_donations'] ?? 0),
                'total_count' => (int)($totalAllTime['total_count'] ?? 0),
                'average_donation' => (float)($totalAllTime['avg_donation'] ?? 0)
            ],
            'by_source' => $bySource,
            'monthly_trend' => $monthlyTrend,
            'active_workers_count' => (int)($activeWorkers['count'] ?? 0),
            'distribution_ratios' => [
                'worker_share' => $this->workerShare * 100,
                'platform_maintenance' => $this->platformMaintenance * 100,
                'development_fund' => $this->developmentFund * 100
            ]
        ];
    }
}
