<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Obsidian\Utils\Database;
use RuntimeException;

class ApiKeyService
{
    private Database $db;

    public function __construct(Database $db)
    {
        $this->db = $db;
    }

    public function generateApiKey(int $userId, string $keyName = 'Default Key'): array
    {
        $keyData = $this->createSecureApiKey();

        $keyHash = hash('sha256', $keyData['full_key']);

        try {
            $this->db->execute(
                'INSERT INTO user_api_keys (user_id, key_name, key_hash, key_prefix) 
                 VALUES (?, ?, ?, ?)',
                [$userId, $keyName, $keyHash, $keyData['prefix']]
            );

            $keyId = $this->db->lastInsertId();

            return [
                'id' => $keyId,
                'key' => $keyData['full_key'],
                'prefix' => $keyData['prefix'],
                'name' => $keyName,
                'created_at' => date('c')
            ];
        } catch (\Exception $e) {
            throw new RuntimeException('Failed to generate API key: ' . $e->getMessage());
        }
    }

    public function validateApiKey(string $apiKey): ?array
    {
        if (empty($apiKey)) {
            return null;
        }

        $keyHash = hash('sha256', $apiKey);

        $result = $this->db->queryOne(
            'SELECT ak.id, ak.user_id, ak.key_name, ak.key_prefix, ak.last_used_at,
                    u.id as user_id, u.uuid, u.email, u.username, 
                    u.daily_generations_used, u.daily_generations_limit, u.last_generation_date
             FROM user_api_keys ak
             JOIN users u ON ak.user_id = u.id
             WHERE ak.key_hash = ? AND ak.is_active = 1 AND u.is_active = 1',
            [$keyHash]
        );

        if (!$result) {
            return null;
        }

        $this->updateLastUsed($result['id']);

        return [
            'key_id' => (int)$result['id'],
            'user_id' => (int)$result['user_id'],
            'uuid' => $result['uuid'],
            'email' => $result['email'],
            'username' => $result['username'],
            'daily_generations_used' => (int)$result['daily_generations_used'],
            'daily_generations_limit' => (int)$result['daily_generations_limit'],
            'last_generation_date' => $result['last_generation_date'],
            'key_name' => $result['key_name'],
            'key_prefix' => $result['key_prefix']
        ];
    }

    public function getUserApiKeys(int $userId): array
    {
        return $this->db->query(
            'SELECT id, key_name, key_prefix, last_used_at, created_at, is_active
             FROM user_api_keys 
             WHERE user_id = ?
             ORDER BY created_at DESC',
            [$userId]
        );
    }

    public function revokeApiKey(int $userId, int $keyId): bool
    {
        $result = $this->db->execute(
            'UPDATE user_api_keys SET is_active = 0 
             WHERE id = ? AND user_id = ?',
            [$keyId, $userId]
        );

        return $result;
    }

    public function updateKeyName(int $userId, int $keyId, string $newName): bool
    {
        $result = $this->db->execute(
            'UPDATE user_api_keys SET key_name = ? 
             WHERE id = ? AND user_id = ?',
            [$newName, $keyId, $userId]
        );

        return $result;
    }

    private function createSecureApiKey(): array
    {
        $prefix = 'obsidian_live_';
        $randomBytes = bin2hex(random_bytes(32));
        $fullKey = $prefix . $randomBytes;

        return [
            'full_key' => $fullKey,
            'prefix' => substr($fullKey, 0, 20) . '...'
        ];
    }

    private function updateLastUsed(int $keyId): void
    {
        $this->db->execute(
            'UPDATE user_api_keys SET last_used_at = CURRENT_TIMESTAMP WHERE id = ?',
            [$keyId]
        );
    }
}
