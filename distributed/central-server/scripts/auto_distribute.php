<?php

require '../vendor/autoload.php';

use DI\Container;
use DI\ContainerBuilder;
use Dotenv\Dotenv;
use Monolog\Handler\StreamHandler;
use Monolog\Logger;
use Obsidian\Utils\Database;
use Obsidian\Services\RevenueDistributionService;

$dotenv = Dotenv::createImmutable(__DIR__ . '/..');
$dotenv->load();

$containerBuilder = new ContainerBuilder();
$containerBuilder->addDefinitions([
    Logger::class => function () {
        $logger = new Logger('auto-distribution');
        $logger->pushHandler(new StreamHandler('php://stdout', 'info'));
        $logger->pushHandler(new StreamHandler(__DIR__ . '/../logs/distribution.log', 'info'));
        return $logger;
    },
    Database::class => function () {
        return new Database(
            $_ENV['DB_HOST'],
            $_ENV['DB_PORT'],
            $_ENV['DB_NAME'],
            $_ENV['DB_USER'],
            $_ENV['DB_PASS']
        );
    },
    RevenueDistributionService::class => function (Container $c) {
        return new RevenueDistributionService(
            $c->get(Database::class),
            $c->get(Logger::class)
        );
    }
]);

$container = $containerBuilder->build();
$revenueService = $container->get(RevenueDistributionService::class);
$logger = $container->get(Logger::class);

try {
    $month = $argv[1] ?? date('Y-m', strtotime('-1 month'));

    $logger->info("Starting automatic distribution for month: {$month}");

    $db = $container->get(Database::class);
    $existingDistribution = $db->queryOne(
        'SELECT COUNT(*) as count FROM revenue_sharing 
         WHERE DATE_FORMAT(period_start, "%Y-%m") = ?',
        [$month]
    );

    if ($existingDistribution['count'] > 0) {
        $logger->info("Distribution already processed for {$month}");
        exit(0);
    }

    $result = $revenueService->processMonthlyDistribution($month);

    if ($result['success']) {
        $logger->info("✅ Automatic distribution completed", [
            'month' => $month,
            'total_donations' => $result['total_donations'],
            'worker_count' => $result['worker_count']
        ]);
        sendDistributionNotification($result);
    } else {
        $logger->warning("⚠️ Distribution skipped: " . $result['message']);
    }
} catch (\Exception $e) {
    $logger->error("❌ Automatic distribution failed: " . $e->getMessage());
    exit(1);
}

function sendDistributionNotification(array $result): void
{
    $webhookUrl = $_ENV['DISCORD_WEBHOOK_URL'] ?? null;

    if ($webhookUrl) {
        $message = [
            'content' => "💰 Monthly distribution completed for {$result['month']}\n" .
                "Total: ${$result['total_donations']}\n" .
                "Workers: {$result['worker_count']}"
        ];

        $ch = curl_init($webhookUrl);
        curl_setopt($ch, CURLOPT_POST, 1);
        curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($message));
        curl_setopt($ch, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_exec($ch);
        curl_close($ch);
    }
}
