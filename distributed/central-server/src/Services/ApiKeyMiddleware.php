<?php

declare(strict_types=1);

namespace Obsidian\Middleware;

use Obsidian\Services\ApiKeyService;
use Obsidian\Services\AuthService;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Server\MiddlewareInterface;
use Psr\Http\Server\RequestHandlerInterface;
use Slim\Psr7\Response;

class ApiKeyMiddleware implements MiddlewareInterface
{
    private ApiKeyService $apiKeyService;
    private AuthService $authService;

    public function __construct(ApiKeyService $apiKeyService, AuthService $authService)
    {
        $this->apiKeyService = $apiKeyService;
        $this->authService = $authService;
    }

    public function process(ServerRequestInterface $request, RequestHandlerInterface $handler): ResponseInterface
    {
        $apiKey = $this->extractApiKey($request);

        if ($apiKey) {
            $userData = $this->apiKeyService->validateApiKey($apiKey);

            if ($userData) {
                $limitCheck = $this->authService->checkDailyLimit($userData['user_id']);

                if (!$limitCheck['allowed']) {
                    return $this->createLimitExceededResponse($limitCheck);
                }

                $request = $request->withAttribute('user', $userData);
                $request = $request->withAttribute('auth_method', 'api_key');

                return $handler->handle($request);
            }
        }

        $authHeader = $request->getHeaderLine('Authorization');

        if (!empty($authHeader) && str_starts_with($authHeader, 'Bearer ')) {
            $token = substr($authHeader, 7);
            $userData = $this->authService->validateToken($token);

            if ($userData) {
                $request = $request->withAttribute('user', $userData);
                $request = $request->withAttribute('auth_method', 'jwt');

                return $handler->handle($request);
            }
        }

        return $this->createUnauthorizedResponse('Valid API key or JWT token required');
    }

    private function extractApiKey(ServerRequestInterface $request): ?string
    {
        $apiKey = $request->getHeaderLine('X-API-Key');
        if (!empty($apiKey)) {
            return $apiKey;
        }

        $authHeader = $request->getHeaderLine('Authorization');
        if (str_starts_with($authHeader, 'ApiKey ')) {
            return substr($authHeader, 7);
        }

        $queryParams = $request->getQueryParams();
        if (!empty($queryParams['api_key'])) {
            return $queryParams['api_key'];
        }

        return null;
    }

    private function createUnauthorizedResponse(string $message): ResponseInterface
    {
        $response = new Response();
        $errorData = [
            'success' => false,
            'error' => [
                'code' => 'UNAUTHORIZED',
                'message' => $message
            ]
        ];

        $response->getBody()->write(json_encode($errorData));
        return $response
            ->withHeader('Content-Type', 'application/json')
            ->withStatus(401);
    }

    private function createLimitExceededResponse(array $limitCheck): ResponseInterface
    {
        $response = new Response();
        $errorData = [
            'success' => false,
            'error' => [
                'code' => 'DAILY_LIMIT_EXCEEDED',
                'message' => "Daily generation limit reached ({$limitCheck['used']}/{$limitCheck['limit']}). Resets at {$limitCheck['resets_at']}"
            ]
        ];

        $response->getBody()->write(json_encode($errorData));
        return $response
            ->withHeader('Content-Type', 'application/json')
            ->withStatus(429);
    }
}
