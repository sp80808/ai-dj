# OBSIDIAN Neural Sound Engine - Distributed Architecture

🎵 **Democratizing AI Music Generation Through Distributed Computing**

## Overview

OBSIDIAN Distributed extends the original OBSIDIAN VST plugin with a secure, community-driven architecture that distributes AI audio generation across volunteer GPU workers while maintaining complete user privacy and content security.

### Key Features

- 🔐 **Cryptographically Signed Generations** - Every sample is authenticated and tracked
- 🌐 **Distributed GPU Computing** - Community-powered processing network
- 🛡️ **Privacy-First Architecture** - Complete anonymization between users and workers
- 💎 **Transparent Revenue Sharing** - Fair distribution of donations to GPU contributors
- 🎯 **25 Free Generations Daily** - No payment required for users
- 📜 **Public Transparency Dashboard** - Real-time donation and distribution tracking
- 🔑 **API Key Management** - Secure authentication for VST plugin integration
- 💰 **Automated Revenue Distribution** - Monthly automated payouts to workers

## Architecture

```
[VST Client] ←→ [Central PHP Server] ←→ [Distributed GPU Workers]
     ↓                    ↓                        ↓
[User Auth]        [Load Balancing]        [AI Generation]
[API Keys]         [WAV Security]          [Anonymized Requests]
[Memory Sync]      [Certification]        [Worker Pool]
                   [Revenue Tracking]
```

## Quick Start

### Prerequisites

- **PHP 8.1+** with extensions: `pdo_mysql`, `gd`, `mbstring`, `curl`
- **MySQL 8.0+** or **MariaDB 10.6+**
- **Composer** for PHP dependencies
- **Redis** (optional, for caching)
- **FFmpeg** for audio processing

### Installation

1. **Clone the repository**

   ```bash
   git clone https://github.com/innermost47/ai-dj.git
   cd ai-dj/distributed/central-server
   ```

2. **Install PHP dependencies**

   ```bash
   composer install
   ```

3. **Configure environment**

   ```bash
   cp .env.example .env
   # Edit .env with your configuration
   ```

4. **Initialize database**

   ```bash
   php init_db.php
   ```

5. **Start development server**

   ```bash
   php -S localhost:8080 -t public/
   ```

6. **Test the installation**
   ```bash
   curl http://localhost:8080/health
   # Should return: {"status":"healthy","service":"obsidian-central-server"}
   ```

## Configuration

### Environment Variables (.env)

```bash
# Application
APP_ENV=development
APP_DEBUG=true
APP_SECRET="your-app-secret-key"
APP_URL=http://localhost:8080

# Database
DB_HOST=localhost
DB_PORT=3306
DB_NAME=obsidian_central
DB_USER=obsidian
DB_PASS=your-secure-password

# JWT Authentication
JWT_SECRET="your-jwt-secret-key"
JWT_EXPIRY=86400
JWT_REFRESH_EXPIRY=604800

# CORS
CORS_ALLOWED_ORIGINS=*

# Logging
LOG_LEVEL=info

# Email Configuration
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
SMTP_ENCRYPTION=tls
SMTP_FROM_EMAIL=noreply@obsidian.dev
SMTP_FROM_NAME=OBSIDIAN

# Revenue Distribution
DEFAULT_WORKER_SHARE=0.75
PLATFORM_MAINTENANCE=0.15
DEVELOPMENT_FUND=0.10

# Worker Management
WORKER_MAX_IDLE_TIME=300
WORKER_TIMEOUT=300

# Donation Webhooks
GITHUB_WEBHOOK_SECRET="your-github-webhook-secret"
STRIPE_WEBHOOK_SECRET="whsec_your-stripe-webhook-secret"
PAYPAL_WEBHOOK_SECRET="your-paypal-webhook-secret"

# Notifications (optional)
DISCORD_WEBHOOK_URL=https://discord.com/api/webhooks/your-webhook
```

## Web Interface

### Public Pages

- **`/`** - Landing page with project overview and navigation
- **`/register`** - User registration form with real-time validation
- **`/transparency`** - Public transparency dashboard showing donations and distributions

### User Dashboard

After registration, users can:

- View daily generation quota (25 free per day)
- Manage API keys for VST plugin integration
- Track usage and account status

## API Endpoints

### Authentication

- `POST /api/v1/auth/register` - User registration with email verification
- `POST /api/v1/auth/login` - User login with JWT tokens
- `POST /api/v1/auth/refresh` - Refresh JWT tokens
- `GET /api/v1/auth/me` - Get authenticated user profile
- `POST /api/v1/auth/logout` - User logout

### User Management

- `GET /api/v1/user/profile` - Get user profile with session info
- `PUT /api/v1/user/profile` - Update user profile
- `GET /api/v1/user/usage` - Check daily generation quota and usage
- `GET /api/v1/user/api-keys` - List user's API keys
- `POST /api/v1/user/api-keys` - Generate new API key
- `DELETE /api/v1/user/api-keys/{id}` - Revoke API key

### Audio Generation

- `POST /api/v1/generate` - Generate audio sample (requires auth)
- `GET /api/v1/generate/status/{requestId}` - Check generation status

### Worker Management

- `POST /api/v1/workers/register` - Worker registration with capabilities
- `POST /api/v1/workers/heartbeat` - Worker heartbeat with status update
- `GET /api/v1/workers/status` - Get cluster status and worker list
- `DELETE /api/v1/workers/{workerId}` - Unregister worker

### Transparency & Donations (Public)

- `GET /api/v1/transparency/current` - Current month transparency report
- `GET /api/v1/transparency/month/{month}` - Specific month report (YYYY-MM)
- `GET /api/v1/transparency/months` - List available months
- `GET /api/v1/transparency/stats` - Overall donation and distribution statistics

### Donation Webhooks (Public)

- `POST /webhooks/github-sponsors` - GitHub Sponsors webhook
- `POST /webhooks/paypal` - PayPal donation webhook
- `POST /webhooks/stripe` - Stripe payment webhook
- `POST /api/v1/donate` - Direct donation via VST plugin (requires API key)

## User Registration & API Keys

### Getting Started for Users

1. **Register an account** at `https://your-domain.com/register`
2. **Verify your email** via the verification link
3. **Generate an API key** in your dashboard for VST plugin use
4. **Configure the VST plugin** with your API key

### API Key Usage

API keys use the format: `obsidian_live_[48-character-hex-string]`

Example usage in VST plugin or API calls:

```bash
curl -H "Authorization: Bearer your-api-key" \
     -H "Content-Type: application/json" \
     -d '{"prompt":"upbeat electronic", "bpm":128}' \
     https://your-domain.com/api/v1/generate
```

## Worker Node Setup

### Docker Deployment

1. **Build worker container**

   ```bash
   cd distributed/worker-node
   docker build -t obsidian-worker .
   ```

2. **Run with GPU support**
   ```bash
   docker run --gpus all \
     -e CENTRAL_SERVER_URL=https://your-domain.com \
     -e LLM_MODEL_PATH=/models/your-model.gguf \
     -p 8001:8001 \
     obsidian-worker
   ```

### Requirements for Workers

- **NVIDIA GPU** with 8GB+ VRAM
- **CUDA 11.8+** drivers
- **Docker** with GPU support
- **Stable internet** connection

Workers automatically register with the central server and start processing requests anonymously.

## Revenue Sharing & Automation

The platform operates on a transparent donation-based model:

- **75%** goes to GPU contributors (distributed fairly based on contribution)
- **15%** covers infrastructure and hosting costs
- **10%** funds ongoing development

### Automated Tasks Setup

#### 1. Monthly Revenue Distribution

**Cron job** (runs 1st of every month at midnight):

```bash
# Add to crontab: crontab -e
0 0 1 * * /usr/bin/php /path/to/obsidian/scripts/auto-distribute.php >> /var/log/obsidian-distribution.log 2>&1
```

**Manual execution** for specific month:

```bash
# Distribute for previous month
php scripts/auto-distribute.php

# Distribute for specific month
php scripts/auto-distribute.php 2025-06

# Test distribution (dry run)
php scripts/auto-distribute.php 2025-06 --dry-run
```

#### 2. Worker Cleanup

**Cron job** (runs every 5 minutes):

```bash
# Mark offline workers as inactive
*/5 * * * * /usr/bin/php /path/to/obsidian/scripts/cleanup-workers.php >> /var/log/obsidian-workers.log 2>&1
```

**Manual cleanup**:

```bash
# Remove workers offline for more than 5 minutes
php scripts/cleanup-workers.php

# Force cleanup with custom timeout
php scripts/cleanup-workers.php --timeout=600
```

#### 3. Daily Quota Reset

**Cron job** (runs every day at midnight):

```bash
# Reset daily generation quotas
0 0 * * * /usr/bin/php /path/to/obsidian/scripts/reset-daily-quotas.php >> /var/log/obsidian-quotas.log 2>&1
```

#### 4. Health Monitoring

**Cron job** (runs every minute):

```bash
# Monitor system health and send alerts
* * * * * /usr/bin/php /path/to/obsidian/scripts/health-monitor.php >> /var/log/obsidian-health.log 2>&1
```

### Complete Crontab Setup

```bash
# Edit crontab
crontab -e

# Add these lines:
# Reset daily quotas at midnight
0 0 * * * /usr/bin/php /path/to/obsidian/scripts/reset-daily-quotas.php >> /var/log/obsidian-quotas.log 2>&1

# Monthly revenue distribution on 1st of month
0 0 1 * * /usr/bin/php /path/to/obsidian/scripts/auto-distribute.php >> /var/log/obsidian-distribution.log 2>&1

# Worker cleanup every 5 minutes
*/5 * * * * /usr/bin/php /path/to/obsidian/scripts/cleanup-workers.php >> /var/log/obsidian-workers.log 2>&1

# Health monitoring every minute
* * * * * /usr/bin/php /path/to/obsidian/scripts/health-monitor.php >> /var/log/obsidian-health.log 2>&1

# Log rotation weekly
0 0 * * 0 /usr/sbin/logrotate /etc/logrotate.d/obsidian
```

### Manual Operations

```bash
# Add a manual donation (for testing)
php scripts/add-donation.php github_sponsors 50.00 "Test User" "Thanks for the awesome project!"

# Generate transparency report
php scripts/generate-report.php 2025-06

# Check worker statistics
php scripts/worker-stats.php

# Backup database
php scripts/backup-db.php

# Import donations from CSV
php scripts/import-donations.php donations.csv
```

## Security Features

### Content Security

- **WAV Validation**: All audio files are analyzed and reconstructed using FFmpeg
- **Size Limits**: Strict file size and duration limits
- **Format Verification**: Only valid WAV files are accepted
- **Malware Protection**: Files are never executed, only processed

### Privacy Protection

- **Request Anonymization**: Workers never see real user IDs
- **Memory Isolation**: User conversation history is not persisted on workers
- **No Data Storage**: Workers cannot store generated samples
- **Docker Isolation**: Worker processes run in containerized environments

### Authentication & Authorization

- **JWT Tokens**: Secure session management with refresh tokens
- **API Key System**: Secure authentication for VST plugin integration
- **Rate Limiting**: Daily generation quotas to prevent abuse
- **Email Verification**: Required for account activation

## Development

### Database Schema

The system uses several key tables:

- `users` - User accounts, daily quotas, and verification status
- `user_api_keys` - API keys for VST plugin authentication
- `user_sessions` - User session management and conversation memory
- `workers` - GPU worker registry, capabilities, and statistics
- `generation_requests` - Audio generation request tracking
- `worker_contributions` - Monthly contribution tracking for revenue sharing
- `revenue_sharing` - Revenue distribution records
- `donation_sources` - Donation tracking from various sources

### Adding New Features

1. **Create feature branch**

   ```bash
   git checkout -b feature/your-feature
   ```

2. **Add database migrations if needed**

   ```php
   // In Database.php createTables() method or create migration script
   ```

3. **Create services and controllers**

   ```php
   // Follow existing patterns in src/Services/ and src/Controllers/
   ```

4. **Add to DI container**

   ```php
   // In public/app.php $containerBuilder->addDefinitions([])
   ```

5. **Test and submit PR**

## Production Deployment

### Server Requirements

- **PHP 8.1+** with OPcache enabled
- **MySQL 8.0+** with adequate resources
- **HTTPS** certificate (required for VST plugin)
- **Reverse proxy** (Nginx/Apache) recommended
- **Process manager** (Supervisor/systemd) for background tasks
- **Cron daemon** for automated tasks

### Performance Optimization

- Enable **OPcache** for PHP
- Use **Redis** for session storage
- Configure **database connection pooling**
- Set up **log rotation**
- Monitor **memory usage** and **database performance**
- Use **CDN** for static assets

### Monitoring

- Monitor `/health` endpoint for service status
- Track worker availability via `/api/v1/workers/status`
- Set up alerts for failed generations or worker issues
- Monitor daily generation quotas and user activity
- Use transparency dashboard at `/transparency` for public metrics

## Donation Setup

### GitHub Sponsors

1. Set up GitHub Sponsors for your repository
2. Configure webhook URL: `https://your-domain.com/webhooks/github-sponsors`
3. Set webhook secret in environment: `GITHUB_WEBHOOK_SECRET`
4. Select events: `sponsorship`

### PayPal

1. Create PayPal developer app
2. Configure webhook URL: `https://your-domain.com/webhooks/paypal`
3. Set webhook secret: `PAYPAL_WEBHOOK_SECRET`
4. Subscribe to event: `PAYMENT.CAPTURE.COMPLETED`

### Stripe

1. Create Stripe account and get API keys
2. Configure webhook URL: `https://your-domain.com/webhooks/stripe`
3. Set webhook secret: `STRIPE_WEBHOOK_SECRET`
4. Subscribe to event: `payment_intent.succeeded`

## Troubleshooting

### Common Issues

**"No workers available"**

- Check worker connectivity: `curl http://worker-ip:8001/health`
- Verify workers are registered: `GET /api/v1/workers/status`
- Check worker logs for connection issues
- Ensure workers are sending heartbeats

**"Daily limit exceeded"**

- Quotas reset at midnight UTC
- Check remaining generations: `GET /api/v1/user/usage`
- Reset manually if needed: `php scripts/reset-user-quota.php user@email.com`

**"API key invalid"**

- Verify API key format: `obsidian_live_[48-chars]`
- Check key is active: `GET /api/v1/user/api-keys`
- Regenerate if necessary in user dashboard

**"WAV validation failed"**

- Ensure FFmpeg is installed: `ffmpeg -version`
- Check worker-generated audio format compliance
- Review security validation logs

**Revenue distribution issues**

- Check donation webhook logs
- Verify webhook signatures are correct
- Run manual distribution: `php scripts/auto-distribute.php`
- Check worker contribution scores

## Contributing

We welcome contributions!

### Key Areas for Contribution

- **Worker efficiency improvements**
- **Additional security measures**
- **Documentation and tutorials**
- **Community management tools**
- **Payment processor integrations**

## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/innermost47/ai-dj/blob/main/LICENSE) file for details.

## Acknowledgments

- **Stability AI** for Stable Audio Open
- **Audio Engineering Society** for supporting AI audio research
- **Community GPU contributors** who make this platform possible
- **OBSIDIAN users** for feedback and support

## Contact

- **GitHub**: https://github.com/innermost47/ai-dj
- **Issues**: https://github.com/innermost47/ai-dj/issues
