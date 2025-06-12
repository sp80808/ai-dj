<?php

declare(strict_types=1);

namespace Obsidian\Utils;

use PDO;
use PDOException;
use RuntimeException;

class Database
{
    private PDO $pdo;

    public function __construct(
        string $host,
        int $port,
        string $database,
        string $username,
        string $password
    ) {
        $dsn = "mysql:host={$host};port={$port};dbname={$database};charset=utf8mb4";

        $options = [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false,
            PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8mb4"
        ];

        try {
            $this->pdo = new PDO($dsn, $username, $password, $options);
        } catch (PDOException $e) {
            throw new RuntimeException("Database connection failed: " . $e->getMessage());
        }
    }

    public function getPdo(): PDO
    {
        return $this->pdo;
    }

    public function query(string $sql, array $params = []): array
    {
        try {
            $stmt = $this->pdo->prepare($sql);
            $stmt->execute($params);
            return $stmt->fetchAll();
        } catch (PDOException $e) {
            throw new RuntimeException("Query failed: " . $e->getMessage());
        }
    }

    public function queryOne(string $sql, array $params = []): ?array
    {
        try {
            $stmt = $this->pdo->prepare($sql);
            $stmt->execute($params);
            $result = $stmt->fetch();
            return $result ?: null;
        } catch (PDOException $e) {
            throw new RuntimeException("Query failed: " . $e->getMessage());
        }
    }

    public function execute(string $sql, array $params = []): bool
    {
        try {
            $stmt = $this->pdo->prepare($sql);
            return $stmt->execute($params);
        } catch (PDOException $e) {
            throw new RuntimeException("Execute failed: " . $e->getMessage());
        }
    }

    public function lastInsertId(): string
    {
        return $this->pdo->lastInsertId();
    }

    public function beginTransaction(): bool
    {
        return $this->pdo->beginTransaction();
    }

    public function commit(): bool
    {
        return $this->pdo->commit();
    }

    public function rollback(): bool
    {
        return $this->pdo->rollback();
    }

    public function createTables(): void
    {
        $this->execute("
            CREATE TABLE IF NOT EXISTS users (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                uuid VARCHAR(36) UNIQUE NOT NULL,
                email VARCHAR(255) UNIQUE NOT NULL,
                username VARCHAR(100) UNIQUE NOT NULL,
                password_hash VARCHAR(255) NOT NULL,
                first_name VARCHAR(100),
                last_name VARCHAR(100),
                plan ENUM('free', 'premium', 'enterprise') DEFAULT 'free',
                credits_remaining INT DEFAULT 50,
                credits_total INT DEFAULT 50,
                is_active BOOLEAN DEFAULT TRUE,
                email_verified BOOLEAN DEFAULT FALSE,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                
                INDEX idx_email (email),
                INDEX idx_username (username),
                INDEX idx_uuid (uuid)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS workers (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                worker_id VARCHAR(64) UNIQUE NOT NULL,
                worker_name VARCHAR(100),
                ip_address VARCHAR(45) NOT NULL,
                port INT NOT NULL,
                status ENUM('registering', 'idle', 'busy', 'error', 'offline') DEFAULT 'registering',
                gpu_info JSON,
                capabilities JSON,
                performance_score DECIMAL(5,2) DEFAULT 100.00,
                total_requests INT DEFAULT 0,
                successful_requests INT DEFAULT 0,
                failed_requests INT DEFAULT 0,
                avg_response_time DECIMAL(8,2) DEFAULT 0,
                last_heartbeat TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                docker_image_hash VARCHAR(64),
                session_token VARCHAR(512),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                
                INDEX idx_worker_id (worker_id),
                INDEX idx_status (status),
                INDEX idx_last_heartbeat (last_heartbeat),
                INDEX idx_performance (performance_score)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS generation_requests (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                request_id VARCHAR(36) UNIQUE NOT NULL,
                user_id BIGINT NOT NULL,
                worker_id BIGINT,
                prompt TEXT NOT NULL,
                bpm DECIMAL(6,2) NOT NULL,
                key_signature VARCHAR(20),
                preferred_stems JSON,
                generation_duration DECIMAL(4,1) DEFAULT 6.0,
                sample_rate INT DEFAULT 48000,
                status ENUM('queued', 'processing', 'completed', 'failed', 'timeout') DEFAULT 'queued',
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                started_at TIMESTAMP NULL,
                completed_at TIMESTAMP NULL,
                response_time_ms INT,
                error_message TEXT,
                file_size_bytes INT,
                user_memory JSON,
                
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
                FOREIGN KEY (worker_id) REFERENCES workers(id) ON DELETE SET NULL,
                INDEX idx_request_id (request_id),
                INDEX idx_user_id (user_id),
                INDEX idx_status (status),
                INDEX idx_created_at (created_at)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS user_sessions (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                user_id BIGINT NOT NULL,
                conversation_memory JSON,
                last_prompt TEXT,
                session_started TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                is_active BOOLEAN DEFAULT TRUE,
                
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
                INDEX idx_user_id (user_id),
                INDEX idx_last_activity (last_activity)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS revenue_sharing (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                worker_id BIGINT NOT NULL,
                source_type ENUM('github_sponsors', 'paypal_donation', 'stripe_donation', 'crypto_donation', 'sponsorship') NOT NULL,
                amount_total DECIMAL(10,4) NOT NULL,
                worker_share DECIMAL(10,4) NOT NULL,
                platform_maintenance DECIMAL(10,4) NOT NULL,
                development_fund DECIMAL(10,4) NOT NULL,
                period_start DATE NOT NULL,
                period_end DATE NOT NULL,
                processed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (worker_id) REFERENCES workers(id) ON DELETE CASCADE,
                INDEX idx_worker_id (worker_id),
                INDEX idx_period (period_start, period_end),
                INDEX idx_source_type (source_type)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS worker_contributions (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                worker_id BIGINT NOT NULL,
                month CHAR(7) NOT NULL,
                total_generations INT DEFAULT 0,
                total_compute_seconds INT DEFAULT 0,
                uptime_hours DECIMAL(8,2) DEFAULT 0,
                performance_score_avg DECIMAL(5,2) DEFAULT 0,
                contribution_score DECIMAL(8,4) DEFAULT 0,
                
                FOREIGN KEY (worker_id) REFERENCES workers(id) ON DELETE CASCADE,
                UNIQUE KEY idx_worker_month (worker_id, month),
                INDEX idx_month (month)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS donation_sources (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                source_type ENUM('github_sponsors', 'paypal_donation', 'stripe_donation', 'crypto_donation', 'sponsorship') NOT NULL,
                amount DECIMAL(10,4) NOT NULL,
                currency VARCHAR(3) DEFAULT 'USD',
                donor_name VARCHAR(255),
                message TEXT,
                received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                processed BOOLEAN DEFAULT FALSE,
                
                INDEX idx_source_type (source_type),
                INDEX idx_received_at (received_at),
                INDEX idx_processed (processed)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        $this->execute("
            CREATE TABLE IF NOT EXISTS user_api_keys (
                id BIGINT PRIMARY KEY AUTO_INCREMENT,
                user_id BIGINT NOT NULL,
                key_name VARCHAR(100) NOT NULL,
                key_hash VARCHAR(255) NOT NULL,
                key_prefix VARCHAR(50) NOT NULL,
                last_used_at TIMESTAMP NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                is_active BOOLEAN DEFAULT TRUE,
                
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
                UNIQUE KEY idx_key_hash (key_hash),
                INDEX idx_user_id (user_id),
                INDEX idx_key_prefix (key_prefix)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ");

        echo "✅ Database tables created successfully\n";
    }
}
