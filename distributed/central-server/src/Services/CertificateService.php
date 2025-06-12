<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Monolog\Logger;
use Obsidian\Utils\Database;
use RuntimeException;

class CertificateService
{
    private Database $db;
    private Logger $logger;
    private string $privateKeyPath;
    private string $publicKeyPath;
    private $privateKey;

    public function __construct(Database $db, Logger $logger, string $privateKeyPath, string $publicKeyPath)
    {
        $this->db = $db;
        $this->logger = $logger;
        $this->privateKeyPath = $privateKeyPath;
        $this->publicKeyPath = $publicKeyPath;

        $this->loadPrivateKey();
    }

    public function generateCertificate(array $generationData, string $wavHash): array
    {
        $certificateData = [
            'request_id' => $generationData['request_id'],
            'user_id' => $generationData['user_id'],
            'username' => $generationData['username'],
            'prompt' => $generationData['prompt'],
            'bpm' => $generationData['bpm'],
            'key_signature' => $generationData['key_signature'] ?? null,
            'generation_duration' => $generationData['generation_duration'],
            'sample_rate' => $generationData['sample_rate'],
            'preferred_stems' => $generationData['preferred_stems'] ?? null,
            'worker_id' => $generationData['worker_id'],
            'timestamp' => date('c'),
            'wav_sha256' => $wavHash,
            'model_attribution' => 'Generated using Stability AI Stable Audio Open Small',
            'platform' => 'OBSIDIAN Neural Sound Engine by InnerMost47'
        ];

        $dataToSign = $this->createSignaturePayload($certificateData);
        $signature = $this->signData($dataToSign);

        $certificate = [
            'version' => '1.0',
            'type' => 'generation_attestation',
            'data' => $certificateData,
            'signature' => base64_encode($signature),
            'public_key_fingerprint' => $this->getPublicKeyFingerprint()
        ];

        $this->storeCertificate($certificate);

        $this->addToPublicLedger($certificate);

        $this->logger->info("Certificate generated", [
            'request_id' => $generationData['request_id'],
            'user_id' => $generationData['user_id'],
            'wav_hash' => substr($wavHash, 0, 16) . '...'
        ]);

        return $certificate;
    }

    public function verifyCertificate(array $certificate): array
    {
        try {
            $dataToSign = $this->createSignaturePayload($certificate['data']);
            $signature = base64_decode($certificate['signature']);

            $isValid = openssl_verify(
                $dataToSign,
                $signature,
                $this->getPublicKey(),
                OPENSSL_ALGO_SHA256
            );

            if ($isValid !== 1) {
                return [
                    'valid' => false,
                    'error' => 'Invalid signature'
                ];
            }

            $dbCert = $this->db->queryOne(
                'SELECT * FROM generation_certificates WHERE request_id = ?',
                [$certificate['data']['request_id']]
            );

            return [
                'valid' => true,
                'exists_in_ledger' => $dbCert !== null,
                'certificate_data' => $certificate['data'],
                'verified_at' => date('c')
            ];
        } catch (\Exception $e) {
            return [
                'valid' => false,
                'error' => $e->getMessage()
            ];
        }
    }

    public function getPublicLedger(int $limit = 1000, int $offset = 0): array
    {
        $certificates = $this->db->query(
            'SELECT certificate_data FROM generation_certificates 
             ORDER BY created_at DESC LIMIT ? OFFSET ?',
            [$limit, $offset]
        );

        return [
            'ledger_info' => [
                'generated_at' => date('c'),
                'total_entries' => $this->getTotalCertificates(),
                'platform' => 'OBSIDIAN Neural Sound Engine',
                'maintainer' => 'InnerMost47',
                'public_key_url' => $_ENV['APP_URL'] . '/api/v1/certificate/public-key',
                'verification_url' => $_ENV['APP_URL'] . '/api/v1/certificate/verify',
                'disclaimer' => 'This ledger provides generation attestation only. It does not constitute ownership claims or copyright assignment.'
            ],
            'entries' => array_map(function ($cert) {
                $data = json_decode($cert['certificate_data'], true);
                return [
                    'request_id' => $data['data']['request_id'],
                    'username' => $data['data']['username'],
                    'prompt' => $data['data']['prompt'],
                    'timestamp' => $data['data']['timestamp'],
                    'wav_sha256' => $data['data']['wav_sha256'],
                    'signature' => $data['signature'],
                    'model_attribution' => $data['data']['model_attribution']
                ];
            }, $certificates)
        ];
    }

    public function searchCertificates(array $criteria): array
    {
        $whereConditions = [];
        $params = [];

        if (!empty($criteria['username'])) {
            $whereConditions[] = "JSON_EXTRACT(certificate_data, '$.data.username') = ?";
            $params[] = $criteria['username'];
        }

        if (!empty($criteria['wav_hash'])) {
            $whereConditions[] = "JSON_EXTRACT(certificate_data, '$.data.wav_sha256') = ?";
            $params[] = $criteria['wav_hash'];
        }

        if (!empty($criteria['date_from'])) {
            $whereConditions[] = "JSON_EXTRACT(certificate_data, '$.data.timestamp') >= ?";
            $params[] = $criteria['date_from'];
        }

        if (!empty($criteria['date_to'])) {
            $whereConditions[] = "JSON_EXTRACT(certificate_data, '$.data.timestamp') <= ?";
            $params[] = $criteria['date_to'];
        }

        $whereClause = empty($whereConditions) ? '' : 'WHERE ' . implode(' AND ', $whereConditions);

        $results = $this->db->query(
            "SELECT certificate_data FROM generation_certificates {$whereClause} ORDER BY created_at DESC LIMIT 100",
            $params
        );

        return array_map(function ($cert) {
            return json_decode($cert['certificate_data'], true);
        }, $results);
    }

    private function createSignaturePayload(array $data): string
    {
        $signaturePayload = [
            'request_id' => $data['request_id'],
            'user_id' => $data['user_id'],
            'wav_sha256' => $data['wav_sha256'],
            'timestamp' => $data['timestamp'],
            'prompt' => $data['prompt'],
            'bpm' => $data['bpm'],
            'sample_rate' => $data['sample_rate']
        ];

        ksort($signaturePayload);
        return json_encode($signaturePayload, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    }

    private function signData(string $data): string
    {
        $signature = '';
        $result = openssl_sign($data, $signature, $this->privateKey, OPENSSL_ALGO_SHA256);

        if (!$result) {
            throw new RuntimeException('Failed to sign data: ' . openssl_error_string());
        }

        return $signature;
    }

    private function loadPrivateKey(): void
    {
        if (!file_exists($this->privateKeyPath)) {
            throw new RuntimeException("Private key file not found: {$this->privateKeyPath}");
        }

        $this->privateKey = openssl_pkey_get_private(file_get_contents($this->privateKeyPath));

        if (!$this->privateKey) {
            throw new RuntimeException('Failed to load private key: ' . openssl_error_string());
        }
    }

    private function getPublicKey()
    {
        if (!file_exists($this->publicKeyPath)) {
            throw new RuntimeException("Public key file not found: {$this->publicKeyPath}");
        }

        return openssl_pkey_get_public(file_get_contents($this->publicKeyPath));
    }

    private function getPublicKeyFingerprint(): string
    {
        $publicKeyContent = file_get_contents($this->publicKeyPath);
        return hash('sha256', $publicKeyContent);
    }

    private function storeCertificate(array $certificate): void
    {
        $this->db->execute(
            'INSERT INTO generation_certificates (request_id, user_id, certificate_data, created_at) 
             VALUES (?, ?, ?, NOW())',
            [
                $certificate['data']['request_id'],
                $certificate['data']['user_id'],
                json_encode($certificate)
            ]
        );
    }

    private function addToPublicLedger(array $certificate): void
    {
        $ledgerFile = $_ENV['PUBLIC_LEDGER_PATH'] ?? '/var/www/html/public/ledger.json';

        $currentLedger = $this->getPublicLedger(10000);

        $ledgerDir = dirname($ledgerFile);
        if (!is_dir($ledgerDir)) {
            mkdir($ledgerDir, 0755, true);
        }

        file_put_contents($ledgerFile, json_encode($currentLedger, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

        $this->logger->info("Public ledger updated", ['ledger_file' => $ledgerFile]);
    }

    private function getTotalCertificates(): int
    {
        $result = $this->db->queryOne('SELECT COUNT(*) as total FROM generation_certificates');
        return (int)($result['total'] ?? 0);
    }

    public function initializeKeys(): void
    {
        if (!file_exists($this->privateKeyPath) || !file_exists($this->publicKeyPath)) {
            $this->generateKeyPair();
        }
    }

    private function generateKeyPair(): void
    {
        $config = [
            "digest_alg" => "sha256",
            "private_key_bits" => 2048,
            "private_key_type" => OPENSSL_KEYTYPE_RSA,
        ];

        $keyPair = openssl_pkey_new($config);
        if (!$keyPair) {
            throw new RuntimeException('Failed to generate key pair: ' . openssl_error_string());
        }

        $privateKey = '';
        if (!openssl_pkey_export($keyPair, $privateKey)) {
            throw new RuntimeException('Failed to export private key: ' . openssl_error_string());
        }

        $keyDetails = openssl_pkey_get_details($keyPair);
        $publicKey = $keyDetails['key'];

        $privateDir = dirname($this->privateKeyPath);
        $publicDir = dirname($this->publicKeyPath);

        if (!is_dir($privateDir)) {
            mkdir($privateDir, 0700, true);
        }
        if (!is_dir($publicDir)) {
            mkdir($publicDir, 0755, true);
        }

        file_put_contents($this->privateKeyPath, $privateKey);
        file_put_contents($this->publicKeyPath, $publicKey);

        chmod($this->privateKeyPath, 0600);
        chmod($this->publicKeyPath, 0644);

        $this->logger->info("Generated new key pair", [
            'private_key' => $this->privateKeyPath,
            'public_key' => $this->publicKeyPath
        ]);
    }
}
