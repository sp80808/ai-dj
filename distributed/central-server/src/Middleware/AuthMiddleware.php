<?php

declare(strict_types=1);

namespace Obsidian\Middleware;

use Obsidian\Services\AuthService;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Server\MiddlewareInterface;
use Psr\Http\Server\RequestHandlerInterface;
use Slim\Psr7\Response;

class AuthMiddleware implements MiddlewareInterface
{
    private AuthService $authService;

    public function __construct(AuthService $authService)
    {
        $this->authService = $authService;
    }

    public function process(ServerRequestInterface $request, RequestHandlerInterface $handler): ResponseInterface
    {
        $authHeader = $request->getHeaderLine('Authorization');

        if (empty($authHeader) || !str_starts_with($authHeader, 'Bearer ')) {
            return $this->createUnauthorizedResponse('Authorization header missing or invalid');
        }

        $token = substr($authHeader, 7);

        $userData = $this->authService->validateToken($token);

        if (!$userData) {
            return $this->createUnauthorizedResponse('Invalid or expired token');
        }

        $request = $request->withAttribute('user', $userData);

        return $handler->handle($request);
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
}
