<?php

declare(strict_types=1);

namespace Obsidian\Middleware;

use Monolog\Logger;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Server\MiddlewareInterface;
use Psr\Http\Server\RequestHandlerInterface;

class LoggingMiddleware implements MiddlewareInterface
{
    private Logger $logger;

    public function __construct(Logger $logger)
    {
        $this->logger = $logger;
    }

    public function process(ServerRequestInterface $request, RequestHandlerInterface $handler): ResponseInterface
    {
        $startTime = microtime(true);
        $method = $request->getMethod();
        $uri = (string)$request->getUri();
        $userAgent = $request->getHeaderLine('User-Agent');
        $ip = $this->getClientIpAddress($request);

        $user = $request->getAttribute('user');
        $userId = $user['user_id'] ?? 'anonymous';
        $username = $user['username'] ?? 'anonymous';

        $this->logger->info("Request started", [
            'method' => $method,
            'uri' => $uri,
            'user_id' => $userId,
            'username' => $username,
            'ip' => $ip,
            'user_agent' => $userAgent
        ]);

        $response = $handler->handle($request);

        $endTime = microtime(true);
        $duration = round(($endTime - $startTime) * 1000, 2);
        $statusCode = $response->getStatusCode();
        $responseSize = $response->getBody()->getSize();

        $logLevel = $statusCode >= 500 ? 'error' : ($statusCode >= 400 ? 'warning' : 'info');

        $this->logger->log($logLevel, "Request completed", [
            'method' => $method,
            'uri' => $uri,
            'user_id' => $userId,
            'username' => $username,
            'status_code' => $statusCode,
            'duration_ms' => $duration,
            'response_size' => $responseSize,
            'ip' => $ip
        ]);

        return $response->withHeader('X-Response-Time', $duration . 'ms');
    }

    private function getClientIpAddress(ServerRequestInterface $request): string
    {
        $forwardedFor = $request->getHeaderLine('X-Forwarded-For');
        if (!empty($forwardedFor)) {
            $ips = explode(',', $forwardedFor);
            return trim($ips[0]);
        }

        $realIp = $request->getHeaderLine('X-Real-IP');
        if (!empty($realIp)) {
            return $realIp;
        }

        $serverParams = $request->getServerParams();
        return $serverParams['REMOTE_ADDR'] ?? '127.0.0.1';
    }
}
