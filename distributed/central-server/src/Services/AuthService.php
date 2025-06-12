<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Firebase\JWT\JWT;
use Firebase\JWT\Key;
use Obsidian\Utils\Database;
use Ramsey\Uuid\Uuid;
use RuntimeException;

class AuthService
{
    private Database $db;
    private string $jwtSecret;
    private int $jwtExpiry;

    public function __construct(Database $db, string $jwtSecret, int $jwtExpiry)
    {
        $this->db = $db;
        $this->jwtSecret = $jwtSecret;
        $this->jwtExpiry = $jwtExpiry;
    }

    public function register(array $data): array
    {
        $this->validateRegistrationData($data);

        $existingUser = $this->db->queryOne(
            'SELECT id FROM users WHERE email = ? OR username = ?',
            [$data['email'], $data['username']]
        );

        if ($existingUser) {
            throw new RuntimeException('User already exists with this email or username');
        }

        $passwordHash = password_hash($data['password'], PASSWORD_ARGON2ID);
        $uuid = Uuid::uuid4()->toString();

        $this->db->execute(
            'INSERT INTO users (uuid, email, username, password_hash, first_name, last_name, plan) 
             VALUES (?, ?, ?, ?, ?, ?, ?)',
            [
                $uuid,
                $data['email'],
                $data['username'],
                $passwordHash,
                $data['first_name'] ?? null,
                $data['last_name'] ?? null,
                $data['plan'] ?? 'free'
            ]
        );

        $userId = (int)$this->db->lastInsertId();

        $this->createUserSession($userId);

        $tokens = $this->generateTokens($userId, $uuid);

        return [
            'user' => [
                'id' => $userId,
                'uuid' => $uuid,
                'email' => $data['email'],
                'username' => $data['username'],
                'plan' => $data['plan'] ?? 'free',
                'credits_remaining' => 50
            ],
            'tokens' => $tokens
        ];
    }

    public function checkDailyLimit(int $userId): array
    {
        $user = $this->db->queryOne(
            'SELECT daily_generations_used, daily_generations_limit, last_generation_date 
         FROM users WHERE id = ?',
            [$userId]
        );

        if (!$user) {
            return ['allowed' => false, 'error' => 'User not found'];
        }

        if ($user['last_generation_date'] !== date('Y-m-d')) {
            $this->db->execute(
                'UPDATE users SET daily_generations_used = 0, last_generation_date = CURRENT_DATE 
             WHERE id = ?',
                [$userId]
            );
            $user['daily_generations_used'] = 0;
        }

        $remaining = $user['daily_generations_limit'] - $user['daily_generations_used'];

        return [
            'allowed' => $remaining > 0,
            'used' => (int)$user['daily_generations_used'],
            'limit' => (int)$user['daily_generations_limit'],
            'remaining' => max(0, $remaining),
            'resets_at' => date('Y-m-d 00:00:00', strtotime('+1 day'))
        ];
    }

    public function incrementDailyUsage(int $userId): bool
    {
        $limitCheck = $this->checkDailyLimit($userId);
        if (!$limitCheck['allowed']) {
            return false;
        }

        $this->db->execute(
            'UPDATE users SET daily_generations_used = daily_generations_used + 1 
         WHERE id = ?',
            [$userId]
        );

        return true;
    }

    public function login(string $emailOrUsername, string $password): array
    {
        $user = $this->db->queryOne(
            'SELECT id, uuid, email, username, password_hash, plan, credits_remaining, is_active 
             FROM users WHERE (email = ? OR username = ?) AND is_active = 1',
            [$emailOrUsername, $emailOrUsername]
        );

        if (!$user || !password_verify($password, $user['password_hash'])) {
            throw new RuntimeException('Invalid credentials');
        }

        $this->db->execute(
            'UPDATE users SET updated_at = CURRENT_TIMESTAMP WHERE id = ?',
            [$user['id']]
        );

        $this->ensureUserSession((int)$user['id']);

        $tokens = $this->generateTokens((int)$user['id'], $user['uuid']);

        return [
            'user' => [
                'id' => (int)$user['id'],
                'uuid' => $user['uuid'],
                'email' => $user['email'],
                'username' => $user['username'],
                'plan' => $user['plan'],
                'credits_remaining' => (int)$user['credits_remaining']
            ],
            'tokens' => $tokens
        ];
    }

    public function validateToken(string $token): ?array
    {
        try {
            $decoded = JWT::decode($token, new Key($this->jwtSecret, 'HS256'));

            $user = $this->db->queryOne(
                'SELECT id, uuid, email, username, plan, credits_remaining, is_active 
                 FROM users WHERE id = ? AND is_active = 1',
                [$decoded->user_id]
            );

            if (!$user) {
                return null;
            }

            return [
                'user_id' => (int)$user['id'],
                'uuid' => $user['uuid'],
                'email' => $user['email'],
                'username' => $user['username'],
                'plan' => $user['plan'],
                'credits_remaining' => (int)$user['credits_remaining']
            ];
        } catch (\Exception $e) {
            return null;
        }
    }

    public function refreshToken(string $refreshToken): array
    {
        try {
            $decoded = JWT::decode($refreshToken, new Key($this->jwtSecret, 'HS256'));

            if ($decoded->type !== 'refresh') {
                throw new RuntimeException('Invalid refresh token');
            }

            $user = $this->db->queryOne(
                'SELECT id, uuid FROM users WHERE id = ? AND is_active = 1',
                [$decoded->user_id]
            );

            if (!$user) {
                throw new RuntimeException('User not found');
            }

            return $this->generateTokens((int)$user['id'], $user['uuid']);
        } catch (\Exception $e) {
            throw new RuntimeException('Invalid refresh token');
        }
    }

    public function getUserSession(int $userId): ?array
    {
        return $this->db->queryOne(
            'SELECT conversation_memory, last_prompt FROM user_sessions 
             WHERE user_id = ? AND is_active = 1 
             ORDER BY last_activity DESC LIMIT 1',
            [$userId]
        );
    }

    public function updateUserSession(int $userId, array $memory, ?string $lastPrompt = null): void
    {
        $this->db->execute(
            'UPDATE user_sessions 
             SET conversation_memory = ?, last_prompt = ?, last_activity = CURRENT_TIMESTAMP 
             WHERE user_id = ? AND is_active = 1',
            [json_encode($memory), $lastPrompt, $userId]
        );
    }

    private function generateTokens(int $userId, string $uuid): array
    {
        $now = time();
        $accessExpiry = $now + $this->jwtExpiry;
        $refreshExpiry = $now + (7 * 24 * 60 * 60);
        $accessPayload = [
            'iss' => $_ENV['APP_URL'],
            'aud' => $_ENV['APP_URL'],
            'iat' => $now,
            'exp' => $accessExpiry,
            'user_id' => $userId,
            'uuid' => $uuid,
            'type' => 'access'
        ];

        $refreshPayload = [
            'iss' => $_ENV['APP_URL'],
            'aud' => $_ENV['APP_URL'],
            'iat' => $now,
            'exp' => $refreshExpiry,
            'user_id' => $userId,
            'uuid' => $uuid,
            'type' => 'refresh'
        ];

        return [
            'access_token' => JWT::encode($accessPayload, $this->jwtSecret, 'HS256'),
            'refresh_token' => JWT::encode($refreshPayload, $this->jwtSecret, 'HS256'),
            'expires_in' => $this->jwtExpiry,
            'token_type' => 'Bearer'
        ];
    }

    private function validateRegistrationData(array $data): void
    {
        if (empty($data['email']) || !filter_var($data['email'], FILTER_VALIDATE_EMAIL)) {
            throw new RuntimeException('Valid email is required');
        }

        if (empty($data['username']) || strlen($data['username']) < 3) {
            throw new RuntimeException('Username must be at least 3 characters');
        }

        if (empty($data['password']) || strlen($data['password']) < 8) {
            throw new RuntimeException('Password must be at least 8 characters');
        }

        if (!preg_match('/^[a-zA-Z0-9_-]+$/', $data['username'])) {
            throw new RuntimeException('Username can only contain letters, numbers, underscore and dash');
        }
    }

    private function createUserSession(int $userId): void
    {
        $this->db->execute(
            'INSERT INTO user_sessions (user_id, conversation_memory) VALUES (?, ?)',
            [$userId, json_encode(['conversation' => ['conversation_history' => []]])]
        );
    }

    private function ensureUserSession(int $userId): void
    {
        $session = $this->db->queryOne(
            'SELECT id FROM user_sessions WHERE user_id = ? AND is_active = 1',
            [$userId]
        );

        if (!$session) {
            $this->createUserSession($userId);
        }
    }
}
