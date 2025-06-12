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
        $logger = new Logger('revenue-distribution');
        $logger->pushHandler(new StreamHandler('php://stdout', 'info'));
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

$month = $argv[1] ?? date('Y-m');
$result = $revenueService->processMonthlyDistribution($month);

echo "✅ Distribution completed for {$month}\n";
echo "💰 Total distributed: {$result['total_donations']}\n";
