<?php

declare(strict_types=1);

namespace Obsidian\Services;

use Obsidian\Utils\Database;
use PHPMailer\PHPMailer\PHPMailer;
use PHPMailer\PHPMailer\Exception;


class EmailService
{
    private Database $db;
    private PHPMailer $mailer;
    private string $fromEmail;
    private string $fromName;
    private string $baseUrl;

    public function __construct(Database $db, array $smtpConfig, string $baseUrl)
    {
        $this->db = $db;
        $this->baseUrl = $baseUrl;
        $this->fromEmail = $smtpConfig['from_email'];
        $this->fromName = $smtpConfig['from_name'];

        $this->mailer = new PHPMailer(true);

        $this->mailer->isSMTP();
        $this->mailer->Host = $smtpConfig['host'];
        $this->mailer->SMTPAuth = true;
        $this->mailer->Username = $smtpConfig['username'];
        $this->mailer->Password = $smtpConfig['password'];
        $this->mailer->SMTPSecure = $smtpConfig['encryption'] ?? PHPMailer::ENCRYPTION_STARTTLS;
        $this->mailer->Port = $smtpConfig['port'] ?? 587;
        $this->mailer->CharSet = 'UTF-8';

        $this->mailer->setFrom($this->fromEmail, $this->fromName);
    }

    public function sendVerificationEmail(int $userId, string $email, string $username): bool
    {
        try {
            $token = $this->generateVerificationToken($userId);

            $this->storeVerificationToken($userId, $token);

            $verificationUrl = $this->baseUrl . '/verify-email?token=' . urlencode($token);

            $this->mailer->clearAddresses();
            $this->mailer->addAddress($email, $username);
            $this->mailer->isHTML(true);
            $this->mailer->Subject = 'Verify Your OBSIDIAN Account';

            $htmlBody = $this->getVerificationEmailTemplate($username, $verificationUrl);
            $textBody = $this->getVerificationEmailTextTemplate($username, $verificationUrl);

            $this->mailer->Body = $htmlBody;
            $this->mailer->AltBody = $textBody;

            $success = $this->mailer->send();

            if ($success) {
                $this->logEmailSent($userId, $email, 'verification');
            }

            return $success;
        } catch (Exception $e) {
            error_log("Email verification failed: " . $e->getMessage());
            return false;
        }
    }

    public function verifyEmail(string $token): array
    {
        try {
            $verification = $this->db->queryOne(
                'SELECT user_id, created_at FROM email_verifications 
                 WHERE token = ? AND used = 0 AND expires_at > NOW()',
                [$token]
            );

            if (!$verification) {
                return [
                    'success' => false,
                    'error' => 'Invalid or expired verification token'
                ];
            }

            $userId = $verification['user_id'];

            $user = $this->db->queryOne(
                'SELECT id, email, username, email_verified FROM users WHERE id = ?',
                [$userId]
            );

            if (!$user) {
                return [
                    'success' => false,
                    'error' => 'User not found'
                ];
            }

            if ($user['email_verified']) {
                return [
                    'success' => false,
                    'error' => 'Email already verified'
                ];
            }

            $this->db->beginTransaction();

            $this->db->execute(
                'UPDATE users SET email_verified = 1, updated_at = NOW() WHERE id = ?',
                [$userId]
            );

            $this->db->execute(
                'UPDATE email_verifications SET used = 1, verified_at = NOW() WHERE token = ?',
                [$token]
            );

            $this->db->commit();

            $this->sendWelcomeEmail($user['email'], $user['username']);

            return [
                'success' => true,
                'message' => 'Email verified successfully',
                'user' => [
                    'id' => $userId,
                    'email' => $user['email'],
                    'username' => $user['username']
                ]
            ];
        } catch (\Exception $e) {
            $this->db->rollback();
            error_log("Email verification error: " . $e->getMessage());

            return [
                'success' => false,
                'error' => 'Verification failed. Please try again.'
            ];
        }
    }

    public function sendWelcomeEmail(string $email, string $username): bool
    {
        try {
            $this->mailer->clearAddresses();
            $this->mailer->addAddress($email, $username);
            $this->mailer->isHTML(true);
            $this->mailer->Subject = 'Welcome to OBSIDIAN Neural Sound Engine!';

            $htmlBody = $this->getWelcomeEmailTemplate($username);
            $textBody = $this->getWelcomeEmailTextTemplate($username);

            $this->mailer->Body = $htmlBody;
            $this->mailer->AltBody = $textBody;

            return $this->mailer->send();
        } catch (Exception $e) {
            error_log("Welcome email failed: " . $e->getMessage());
            return false;
        }
    }

    public function sendPasswordResetEmail(string $email, string $username): bool
    {
        return true;
    }

    private function generateVerificationToken(int $userId): string
    {
        $data = $userId . '|' . time() . '|' . random_bytes(16);
        return base64_encode(hash('sha256', $data, true));
    }

    private function storeVerificationToken(int $userId, string $token): void
    {
        $this->db->execute(
            'DELETE FROM email_verifications WHERE user_id = ?',
            [$userId]
        );

        $this->db->execute(
            'INSERT INTO email_verifications (user_id, token, expires_at) 
             VALUES (?, ?, DATE_ADD(NOW(), INTERVAL 24 HOUR))',
            [$userId, $token]
        );
    }

    private function logEmailSent(int $userId, string $email, string $type): void
    {
        $this->db->execute(
            'INSERT INTO email_logs (user_id, email, type, sent_at) VALUES (?, ?, ?, NOW())',
            [$userId, $email, $type]
        );
    }

    private function getVerificationEmailTemplate(string $username, string $verificationUrl): string
    {
        return "
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset='UTF-8'>
            <title>Verify Your OBSIDIAN Account</title>
            <style>
                body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
                .container { max-width: 600px; margin: 0 auto; padding: 20px; }
                .header { background: linear-gradient(135deg, #0f0f23, #1a1a2e); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
                .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
                .button { display: inline-block; background: #00d4ff; color: white; padding: 15px 30px; text-decoration: none; border-radius: 8px; font-weight: bold; margin: 20px 0; }
                .footer { text-align: center; margin-top: 30px; color: #666; font-size: 14px; }
            </style>
        </head>
        <body>
            <div class='container'>
                <div class='header'>
                    <h1>OBSIDIAN</h1>
                    <p>Neural Sound Engine</p>
                </div>
                <div class='content'>
                    <h2>Welcome to OBSIDIAN, {$username}!</h2>
                    <p>Thank you for creating an account with OBSIDIAN Neural Sound Engine. To complete your registration and start generating amazing AI-powered audio, please verify your email address.</p>
                    
                    <p style='text-align: center;'>
                        <a href='{$verificationUrl}' class='button'>Verify Email Address</a>
                    </p>
                    
                    <p>If the button doesn't work, copy and paste this link into your browser:</p>
                    <p style='word-break: break-all; background: #eee; padding: 10px; border-radius: 5px;'>{$verificationUrl}</p>
                    
                    <p><strong>What's next?</strong></p>
                    <ul>
                        <li>Download the OBSIDIAN VST plugin</li>
                        <li>Use your login credentials in the plugin</li>
                        <li>Start generating AI audio in your DAW</li>
                    </ul>
                    
                    <p>This verification link will expire in 24 hours. If you didn't create this account, you can safely ignore this email.</p>
                </div>
                <div class='footer'>
                    <p>OBSIDIAN Neural Sound Engine by InnerMost47</p>
                    <p>This email was sent to verify your account registration.</p>
                </div>
            </div>
        </body>
        </html>";
    }

    private function getVerificationEmailTextTemplate(string $username, string $verificationUrl): string
    {
        return "
Welcome to OBSIDIAN Neural Sound Engine, {$username}!

Thank you for creating an account. To complete your registration and start generating AI-powered audio, please verify your email address by visiting this link:

{$verificationUrl}

What's next:
- Download the OBSIDIAN VST plugin
-Use your login credentials in the plugin
- Start generating AI audio in your DAW

This verification link will expire in 24 hours. If you didn't create this account, you can safely ignore this email.

OBSIDIAN Neural Sound Engine by InnerMost47
This email was sent to verify your account registration.
       ";
    }

    private function getWelcomeEmailTemplate(string $username): string
    {
        return "
       <!DOCTYPE html>
       <html>
       <head>
           <meta charset='UTF-8'>
           <title>Welcome to OBSIDIAN!</title>
           <style>
               body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
               .container { max-width: 600px; margin: 0 auto; padding: 20px; }
               .header { background: linear-gradient(135deg, #0f0f23, #1a1a2e); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
               .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
               .button { display: inline-block; background: #00d4ff; color: white; padding: 15px 30px; text-decoration: none; border-radius: 8px; font-weight: bold; margin: 20px 0; }
               .footer { text-align: center; margin-top: 30px; color: #666; font-size: 14px; }
           </style>
       </head>
       <body>
           <div class='container'>
               <div class='header'>
                   <h1>🎉 Welcome to OBSIDIAN!</h1>
                   <p>Your account is now active</p>
               </div>
               <div class='content'>
                   <h2>Hello {$username}!</h2>
                   <p>Your email has been verified and your OBSIDIAN account is now fully active. You're ready to start creating AI-powered music!</p>
                   
                   <p><strong>🎵 What you can do now:</strong></p>
                   <ul>
                       <li><strong>50 FREE generations per month</strong> - No payment required!</li>
                       <li>Generate audio samples in any style</li>
                       <li>Use stems extraction for advanced mixing</li>
                       <li>All samples are cryptographically certified</li>
                       <li>Access the public generation ledger</li>
                   </ul>
                   
                   <p><strong>🔧 Getting Started:</strong></p>
                   <ol>
                       <li>Download the OBSIDIAN VST3 plugin from GitHub</li>
                       <li>Install it in your DAW</li>
                       <li>Use your email/username and password to login</li>
                       <li>Start generating amazing audio!</li>
                   </ol>
                   
                   <p style='text-align: center;'>
                       <a href='https://github.com/innermost47/ai-dj/releases' class='button'>Download VST Plugin</a>
                   </p>
                   
                   <p><strong>🛡️ About Security:</strong></p>
                   <p>Every sample you generate is cryptographically signed and added to our public ledger for transparency. Workers in our distributed network cannot store or access your generated content.</p>
                   
                   <p>Happy creating!</p>
               </div>
               <div class='footer'>
                   <p>OBSIDIAN Neural Sound Engine by InnerMost47</p>
                   <p>Free AI music generation for everyone</p>
               </div>
           </div>
       </body>
       </html>";
    }

    private function getWelcomeEmailTextTemplate(string $username): string
    {
        return "
🎉 Welcome to OBSIDIAN Neural Sound Engine, {$username}!

Your email has been verified and your account is now fully active. You're ready to start creating AI-powered music!

🎵 What you can do now:
- 50 FREE generations per month - No payment required!
- Generate audio samples in any style
- Use stems extraction for advanced mixing
- All samples are cryptographically certified
- Access the public generation ledger

🔧 Getting Started:
1. Download the OBSIDIAN VST3 plugin from GitHub
2. Install it in your DAW
3. Use your email/username and password to login
4. Start generating amazing audio!

Download: https://github.com/innermost47/ai-dj/releases

🛡️ About Security:
Every sample you generate is cryptographically signed and added to our public ledger for transparency. Workers in our distributed network cannot store or access your generated content.

Happy creating!

OBSIDIAN Neural Sound Engine by InnerMost47
Free AI music generation for everyone
       ";
    }
}
