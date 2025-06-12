<?php

declare(strict_types=1);

namespace Obsidian\Models;

class User
{
    public int $id;
    public string $uuid;
    public string $email;
    public string $username;
    public string $passwordHash;
    public ?string $firstName;
    public ?string $lastName;
    public bool $isActive;
    public bool $emailVerified;
    public \DateTime $createdAt;
    public \DateTime $updatedAt;

    public function __construct(array $data = [])
    {
        $this->id = $data['id'] ?? 0;
        $this->uuid = $data['uuid'] ?? '';
        $this->email = $data['email'] ?? '';
        $this->username = $data['username'] ?? '';
        $this->passwordHash = $data['password_hash'] ?? '';
        $this->firstName = $data['first_name'] ?? null;
        $this->lastName = $data['last_name'] ?? null;
        $this->isActive = $data['is_active'] ?? true;
        $this->emailVerified = $data['email_verified'] ?? false;
        $this->createdAt = isset($data['created_at'])
            ? new \DateTime($data['created_at'])
            : new \DateTime();
        $this->updatedAt = isset($data['updated_at'])
            ? new \DateTime($data['updated_at'])
            : new \DateTime();
    }

    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'uuid' => $this->uuid,
            'email' => $this->email,
            'username' => $this->username,
            'first_name' => $this->firstName,
            'last_name' => $this->lastName,
            'is_active' => $this->isActive,
            'email_verified' => $this->emailVerified,
            'created_at' => $this->createdAt->format('c'),
            'updated_at' => $this->updatedAt->format('c')
        ];
    }

    public function getDisplayName(): string
    {
        if ($this->firstName && $this->lastName) {
            return $this->firstName . ' ' . $this->lastName;
        }
        return $this->username;
    }
}
