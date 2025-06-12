<?php
require 'vendor/autoload.php';

use Dotenv\Dotenv;
use Obsidian\Utils\Database;
use Obsidian\Services\EmailService;

$dotenv = Dotenv::createImmutable(__DIR__);
$dotenv->load();

$db = new Database(
    $_ENV['DB_HOST'],
    $_ENV['DB_PORT'],
    $_ENV['DB_NAME'],
    $_ENV['DB_USER'],
    $_ENV['DB_PASS']
);

echo "🚀 Creating tables...\n";
$db->createTables();
$db->execute("
    CREATE TABLE IF NOT EXISTS email_verifications (
        id BIGINT PRIMARY KEY AUTO_INCREMENT,
        user_id BIGINT NOT NULL,
        token VARCHAR(255) UNIQUE NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMP NOT NULL,
        used BOOLEAN DEFAULT FALSE,
        verified_at TIMESTAMP NULL,
        
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
        INDEX idx_token (token),
        INDEX idx_user_id (user_id),
        INDEX idx_expires (expires_at)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
");

$db->execute("
    CREATE TABLE IF NOT EXISTS email_logs (
        id BIGINT PRIMARY KEY AUTO_INCREMENT,
        user_id BIGINT,
        email VARCHAR(255) NOT NULL,
        type ENUM('verification', 'welcome', 'password_reset', 'notification') NOT NULL,
        sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
        INDEX idx_user_id (user_id),
        INDEX idx_type (type),
        INDEX idx_sent_at (sent_at)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
");

$db->execute("
    CREATE TABLE IF NOT EXISTS generation_certificates (
        id BIGINT PRIMARY KEY AUTO_INCREMENT,
        request_id VARCHAR(36) UNIQUE NOT NULL,
        user_id BIGINT NOT NULL,
        certificate_data JSON NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
        INDEX idx_request_id (request_id),
        INDEX idx_user_id (user_id),
        INDEX idx_created_at (created_at)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
");

echo "✅ Database initialized!\n";
