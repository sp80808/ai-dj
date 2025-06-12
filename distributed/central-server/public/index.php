<?php

declare(strict_types=1);

use DI\Container;
use DI\ContainerBuilder;
use Dotenv\Dotenv;
use Monolog\Handler\StreamHandler;
use Monolog\Logger;
use Obsidian\Middleware\AuthMiddleware;
use Obsidian\Middleware\ApiKeyMiddleware;
use Obsidian\Controllers\AuthController;
use Obsidian\Controllers\TransparencyController;
use Obsidian\Controllers\WorkerController;
use Obsidian\Controllers\GenerateController;
use Obsidian\Controllers\DonationWebhookController;
use Obsidian\Services\ApiKeyService;
use Obsidian\Services\WavSecurityService;
use Obsidian\Services\CertificateService;
use Obsidian\Services\AuthService;
use Obsidian\Services\EmailService;
use Obsidian\Services\WorkerPoolService;
use Obsidian\Services\RevenueDistributionService;
use Obsidian\Services\LoadBalancerService;
use Obsidian\Utils\Database;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Slim\Factory\AppFactory;
use Slim\Routing\RouteCollectorProxy;

require __DIR__ . '/../vendor/autoload.php';

// ============================================================================
// CONFIGURATION
// ============================================================================

$dotenv = Dotenv::createImmutable(__DIR__ . '/..');
$dotenv->load();

// ============================================================================
// DEPENDENCY INJECTION CONTAINER
// ============================================================================

$containerBuilder = new ContainerBuilder();

$containerBuilder->addDefinitions([
    // Logger
    Logger::class => function () {
        $logger = new Logger('obsidian-central');
        $logger->pushHandler(new StreamHandler('php://stdout', $_ENV['LOG_LEVEL'] ?? 'info'));
        $logger->pushHandler(new StreamHandler(__DIR__ . '/../storage/logs/app.log', $_ENV['LOG_LEVEL'] ?? 'info'));
        return $logger;
    },

    // Database
    Database::class => function () {
        return new Database(
            $_ENV['DB_HOST'],
            (int)$_ENV['DB_PORT'],
            $_ENV['DB_NAME'],
            $_ENV['DB_USER'],
            $_ENV['DB_PASS']
        );
    },

    // Services
    AuthService::class => function (Container $c) {
        return new AuthService(
            $c->get(Database::class),
            $_ENV['JWT_SECRET'],
            (int)$_ENV['JWT_EXPIRY']
        );
    },

    ApiKeyService::class => function (Container $c) {
        return new ApiKeyService($c->get(Database::class));
    },

    WorkerPoolService::class => function (Container $c) {
        return new WorkerPoolService(
            $c->get(Database::class),
            $c->get(Logger::class)
        );
    },

    LoadBalancerService::class => function (Container $c) {
        return new LoadBalancerService(
            $c->get(WorkerPoolService::class),
            $c->get(Logger::class)
        );
    },

    WavSecurityService::class => function (Container $c) {
        return new WavSecurityService($c->get(Logger::class));
    },

    RevenueDistributionService::class => function (Container $c) {
        return new RevenueDistributionService(
            $c->get(Database::class),
            $c->get(Logger::class)
        );
    },

    CertificateService::class => function (Container $c) {
        return new CertificateService(
            $c->get(Database::class),
            $c->get(Logger::class),
            __DIR__ . '/../storage/keys/private.pem',
            __DIR__ . '/../storage/keys/public.pem'
        );
    },

    EmailService::class => function (Container $c) {
        $smtpConfig = [
            'host' => $_ENV['SMTP_HOST'] ?? 'localhost',
            'port' => (int)($_ENV['SMTP_PORT'] ?? 587),
            'username' => $_ENV['SMTP_USERNAME'] ?? '',
            'password' => $_ENV['SMTP_PASSWORD'] ?? '',
            'encryption' => $_ENV['SMTP_ENCRYPTION'] ?? 'tls',
            'from_email' => $_ENV['SMTP_FROM_EMAIL'] ?? 'noreply@obsidian.dev',
            'from_name' => $_ENV['SMTP_FROM_NAME'] ?? 'OBSIDIAN'
        ];

        $baseUrl = $_ENV['APP_URL'] ?? 'http://localhost:8000';

        return new EmailService(
            $c->get(Database::class),
            $smtpConfig,
            $baseUrl
        );
    },

    // Controllers
    AuthController::class => function (Container $c) {
        return new AuthController(
            $c->get(AuthService::class),
            $c->get(ApiKeyService::class)
        );
    },

    WorkerController::class => function (Container $c) {
        return new WorkerController(
            $c->get(WorkerPoolService::class),
            $c->get(LoadBalancerService::class)
        );
    },

    GenerateController::class => function (Container $c) {
        return new GenerateController(
            $c->get(AuthService::class),
            $c->get(LoadBalancerService::class),
            $c->get(WorkerPoolService::class),
            $c->get(WavSecurityService::class),
            $c->get(CertificateService::class)
        );
    },

    TransparencyController::class => function (Container $c) {
        return new TransparencyController($c->get(RevenueDistributionService::class));
    },

    DonationWebhookController::class => function (Container $c) {
        return new DonationWebhookController(
            $c->get(RevenueDistributionService::class),
            $c->get(Logger::class)
        );
    }
]);

$container = $containerBuilder->build();

// ============================================================================
// APPLICATION SETUP
// ============================================================================

AppFactory::setContainer($container);
$app = AppFactory::create();

// Middleware
$app->addErrorMiddleware($_ENV['APP_DEBUG'] === 'true', true, true);
$app->addBodyParsingMiddleware();

// CORS Middleware
$app->add(function (Request $request, $handler): Response {
    $response = $handler->handle($request);
    return $response
        ->withHeader('Access-Control-Allow-Origin', $_ENV['CORS_ALLOWED_ORIGINS'] ?? '*')
        ->withHeader('Access-Control-Allow-Headers', 'X-Requested-With, Content-Type, Accept, Origin, Authorization, X-API-Key')
        ->withHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, PATCH, OPTIONS');
});

// OPTIONS preflight
$app->options('/{routes:.+}', function (Request $request, Response $response) {
    return $response;
});

// ============================================================================
// HEALTH CHECK
// ============================================================================

$app->get('/health', function (Request $request, Response $response) {
    $data = [
        'status' => 'healthy',
        'timestamp' => date('c'),
        'service' => 'obsidian-central-server',
        'version' => '1.0.0'
    ];

    $response->getBody()->write(json_encode($data));
    return $response->withHeader('Content-Type', 'application/json');
});

// ============================================================================
// API ROUTES
// ============================================================================

$app->group('/api/v1', function (RouteCollectorProxy $group) {

    // Authentication routes
    $group->group('/auth', function (RouteCollectorProxy $auth) {
        $auth->post('/register', [AuthController::class, 'register']);
        $auth->post('/login', [AuthController::class, 'login']);
        $auth->post('/refresh', [AuthController::class, 'refresh']);
        $auth->post('/logout', [AuthController::class, 'logout'])->add(AuthMiddleware::class);
        $auth->get('/me', [AuthController::class, 'me'])->add(AuthMiddleware::class);
    });

    // Worker management routes
    $group->group('/workers', function (RouteCollectorProxy $workers) {
        $workers->post('/register', [WorkerController::class, 'register']);
        $workers->post('/heartbeat', [WorkerController::class, 'heartbeat']);
        $workers->get('/status', [WorkerController::class, 'status']);
        $workers->delete('/{workerId}', [WorkerController::class, 'unregister']);
    });

    // Music generation routes
    $group->group('/generate', function (RouteCollectorProxy $generate) {
        $generate->post('', [GenerateController::class, 'generate'])
            ->add(AuthMiddleware::class);
        $generate->get('/status/{requestId}', [GenerateController::class, 'status'])
            ->add(AuthMiddleware::class);
    });

    // User management routes
    $group->group('/user', function (RouteCollectorProxy $user) {
        $user->get('/profile', [AuthController::class, 'profile']);
        $user->put('/profile', [AuthController::class, 'updateProfile']);
        $user->get('/usage', [AuthController::class, 'usage']);
        $user->get('/api-keys', [AuthController::class, 'getApiKeys']);
        $user->post('/api-keys', [AuthController::class, 'createApiKey']);
        $user->delete('/api-keys/{id}', [AuthController::class, 'revokeApiKey']);
    })->add(AuthMiddleware::class);

    // Donation endpoint (with API key auth)
    $group->post('/donate', [DonationWebhookController::class, 'vstDonation'])
        ->add(ApiKeyMiddleware::class);
});

// ============================================================================
// TRANSPARENCY ROUTES (PUBLIC)
// ============================================================================

$app->group('/api/v1/transparency', function (RouteCollectorProxy $group) {
    $group->get('/current', [TransparencyController::class, 'getCurrentReport']);
    $group->get('/month/{month}', [TransparencyController::class, 'getMonthlyReport']);
    $group->get('/months', [TransparencyController::class, 'getAvailableMonths']);
    $group->get('/stats', [TransparencyController::class, 'getDonationStats']);
});

// ============================================================================
// WEBHOOK ROUTES (PUBLIC)
// ============================================================================

$app->group('/webhooks', function (RouteCollectorProxy $group) {
    $group->post('/github-sponsors', [DonationWebhookController::class, 'githubSponsorsWebhook']);
    $group->post('/paypal', [DonationWebhookController::class, 'paypalWebhook']);
    $group->post('/stripe', [DonationWebhookController::class, 'stripeWebhook']);
});

// ============================================================================
// WEB PAGES
// ============================================================================

$app->get('/', function (Request $request, Response $response) {
    $html = file_get_contents(__DIR__ . '/index.html');
    $response->getBody()->write($html);
    return $response->withHeader('Content-Type', 'text/html');
});

$app->get('/register', function (Request $request, Response $response) {
    $html = file_get_contents(__DIR__ . '/register.html');
    $response->getBody()->write($html);
    return $response->withHeader('Content-Type', 'text/html');
});

$app->get('/transparency', function (Request $request, Response $response) {
    $html = file_get_contents(__DIR__ . '/transparency.html');
    $response->getBody()->write($html);
    return $response->withHeader('Content-Type', 'text/html');
});

$app->get('/verify-email', function (Request $request, Response $response) use ($container) {
    $token = $request->getQueryParams()['token'] ?? '';

    if (empty($token)) {
        $response->getBody()->write('<h1>Invalid verification link</h1>');
        return $response->withStatus(400);
    }

    $emailService = $container->get(EmailService::class);
    $result = $emailService->verifyEmail($token);

    if ($result['success']) {
        $html = '<h1>✅ Email Verified!</h1><p>Your account is now active. You can close this window and use the OBSIDIAN VST plugin.</p>';
    } else {
        $html = '<h1>❌ Verification Failed</h1><p>' . $result['error'] . '</p>';
    }

    $response->getBody()->write($html);
    return $response->withHeader('Content-Type', 'text/html');
});

// ============================================================================
// 404 HANDLER
// ============================================================================

$app->map(['GET', 'POST', 'PUT', 'DELETE', 'PATCH'], '/{routes:.+}', function (Request $request, Response $response) {
    $response->getBody()->write(json_encode([
        'error' => [
            'code' => 'ROUTE_NOT_FOUND',
            'message' => 'The requested route was not found.'
        ]
    ]));
    return $response->withStatus(404)->withHeader('Content-Type', 'application/json');
});

// ============================================================================
// RUN APPLICATION
// ============================================================================

$app->run();
