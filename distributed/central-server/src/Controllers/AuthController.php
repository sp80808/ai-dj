<?php

declare(strict_types=1);

namespace Obsidian\Controllers;

use Obsidian\Services\AuthService;
use Obsidian\Services\ApiKeyService;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use RuntimeException;

class AuthController
{
    private AuthService $authService;
    private ApiKeyService $apiKeyService;

    public function __construct(AuthService $authService, ApiKeyService $apiKeyService)
    {
        $this->authService = $authService;
        $this->apiKeyService = $apiKeyService;
    }


    public function register(Request $request, Response $response): Response
    {
        try {
            $data = $request->getParsedBody() ?? [];

            $this->validateRegistrationInput($data);

            $result = $this->authService->register($data);

            $responseData = [
                'success' => true,
                'message' => 'User registered successfully',
                'data' => $result
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json')->withStatus(201);
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'REGISTRATION_FAILED', $e->getMessage(), 400);
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Registration failed', 500);
        }
    }


    public function getApiKeys(Request $request, Response $response): Response
    {
        try {
            $user = $request->getAttribute('user');
            $apiKeys = $this->apiKeyService->getUserApiKeys($user['user_id']);

            $responseData = [
                'success' => true,
                'data' => $apiKeys
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to load API keys', 500);
        }
    }

    public function createApiKey(Request $request, Response $response): Response
    {
        try {
            $user = $request->getAttribute('user');
            $data = $request->getParsedBody() ?? [];

            $keyName = !empty($data['key_name']) ? trim($data['key_name']) : 'Default Key';

            if (strlen($keyName) < 3) {
                return $this->errorResponse($response, 'INVALID_INPUT', 'Key name must be at least 3 characters', 400);
            }

            $result = $this->apiKeyService->generateApiKey($user['user_id'], $keyName);

            $responseData = [
                'success' => true,
                'message' => 'API key created successfully',
                'data' => $result
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json')->withStatus(201);
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'API_KEY_CREATION_FAILED', $e->getMessage(), 400);
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to create API key', 500);
        }
    }

    public function revokeApiKey(Request $request, Response $response): Response
    {
        try {
            $user = $request->getAttribute('user');
            $keyId = $request->getAttribute('id');

            if (!$keyId || !is_numeric($keyId)) {
                return $this->errorResponse($response, 'INVALID_INPUT', 'Valid key ID is required', 400);
            }

            $success = $this->apiKeyService->revokeApiKey($user['user_id'], (int)$keyId);

            if (!$success) {
                return $this->errorResponse($response, 'NOT_FOUND', 'API key not found', 404);
            }

            $responseData = [
                'success' => true,
                'message' => 'API key revoked successfully'
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to revoke API key', 500);
        }
    }

    public function login(Request $request, Response $response): Response
    {
        try {
            $data = $request->getParsedBody() ?? [];

            if (empty($data['email_or_username']) || empty($data['password'])) {
                return $this->errorResponse($response, 'INVALID_INPUT', 'Email/username and password are required', 400);
            }

            $result = $this->authService->login($data['email_or_username'], $data['password']);

            $responseData = [
                'success' => true,
                'message' => 'Login successful',
                'data' => $result
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'LOGIN_FAILED', $e->getMessage(), 401);
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Login failed', 500);
        }
    }

    public function refresh(Request $request, Response $response): Response
    {
        try {
            $data = $request->getParsedBody() ?? [];

            if (empty($data['refresh_token'])) {
                return $this->errorResponse($response, 'INVALID_INPUT', 'Refresh token is required', 400);
            }

            $tokens = $this->authService->refreshToken($data['refresh_token']);

            $responseData = [
                'success' => true,
                'message' => 'Token refreshed successfully',
                'data' => ['tokens' => $tokens]
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (RuntimeException $e) {
            return $this->errorResponse($response, 'TOKEN_REFRESH_FAILED', $e->getMessage(), 401);
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Token refresh failed', 500);
        }
    }

    public function logout(Request $request, Response $response): Response
    {
        $responseData = [
            'success' => true,
            'message' => 'Logout successful'
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }

    public function me(Request $request, Response $response): Response
    {
        $user = $request->getAttribute('user');

        $responseData = [
            'success' => true,
            'data' => [
                'user' => $user
            ]
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }

    public function profile(Request $request, Response $response): Response
    {
        $user = $request->getAttribute('user');

        $session = $this->authService->getUserSession($user['user_id']);

        $responseData = [
            'success' => true,
            'data' => [
                'user' => $user,
                'session' => [
                    'last_prompt' => $session['last_prompt'] ?? null,
                    'conversation_length' => count(json_decode($session['conversation_memory'] ?? '{}', true)['conversation']['conversation_history'] ?? [])
                ]
            ]
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }

    public function updateProfile(Request $request, Response $response): Response
    {
        $user = $request->getAttribute('user');
        $data = $request->getParsedBody() ?? [];

        $responseData = [
            'success' => true,
            'message' => 'Profile updated successfully'
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }

    public function usage(Request $request, Response $response): Response
    {
        $user = $request->getAttribute('user');
        $limitCheck = $this->authService->checkDailyLimit($user['user_id']);

        $responseData = [
            'success' => true,
            'data' => [
                'today' => [
                    'used' => $limitCheck['used'],
                    'limit' => $limitCheck['limit'],
                    'remaining' => $limitCheck['remaining']
                ],
                'resets_at' => $limitCheck['resets_at']
            ]
        ];

        $response->getBody()->write(json_encode($responseData));
        return $response->withHeader('Content-Type', 'application/json');
    }


    private function validateRegistrationInput(array $data): void
    {
        if (empty($data['email']) || !filter_var($data['email'], FILTER_VALIDATE_EMAIL)) {
            throw new RuntimeException('Valid email is required');
        }

        if (empty($data['username']) || strlen($data['username']) < 3) {
            throw new RuntimeException('Username must be at least 3 characters');
        }

        if (!preg_match('/^[a-zA-Z0-9_-]+$/', $data['username'])) {
            throw new RuntimeException('Username can only contain letters, numbers, underscore and dash');
        }

        if (empty($data['password'])) {
            throw new RuntimeException('Password is required');
        }

        $password = $data['password'];

        if (strlen($password) < 8) {
            throw new RuntimeException('Password must be at least 8 characters long');
        }

        if (!preg_match('/[A-Z]/', $password)) {
            throw new RuntimeException('Password must contain at least one uppercase letter');
        }

        if (!preg_match('/[a-z]/', $password)) {
            throw new RuntimeException('Password must contain at least one lowercase letter');
        }

        if (!preg_match('/[0-9]/', $password)) {
            throw new RuntimeException('Password must contain at least one number');
        }

        if (!preg_match('/[!@#$%^&*(),.?":{}|<>]/', $password)) {
            throw new RuntimeException('Password must contain at least one special character (!@#$%^&*(),.?":{}|<>)');
        }
    }

    private function errorResponse(Response $response, string $code, string $message, int $statusCode): Response
    {
        $errorData = [
            'success' => false,
            'error' => [
                'code' => $code,
                'message' => $message
            ]
        ];

        $response->getBody()->write(json_encode($errorData));
        return $response->withHeader('Content-Type', 'application/json')->withStatus($statusCode);
    }
}
