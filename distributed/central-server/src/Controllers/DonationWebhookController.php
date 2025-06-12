<?php

declare(strict_types=1);

namespace Obsidian\Controllers;

use Obsidian\Services\RevenueDistributionService;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Monolog\Logger;

class DonationWebhookController
{
    private RevenueDistributionService $revenueService;
    private Logger $logger;

    public function __construct(RevenueDistributionService $revenueService, Logger $logger)
    {
        $this->revenueService = $revenueService;
        $this->logger = $logger;
    }

    public function githubSponsorsWebhook(Request $request, Response $response): Response
    {
        $payload = $request->getParsedBody() ?? [];
        $headers = $request->getHeaders();

        if (!$this->verifyGitHubSignature($request)) {
            $this->logger->warning('Invalid GitHub webhook signature');
            return $response->withStatus(401);
        }

        try {
            if ($payload['action'] === 'created' && isset($payload['sponsorship'])) {
                $sponsorship = $payload['sponsorship'];

                $donationData = [
                    'source_type' => 'github_sponsors',
                    'amount' => $sponsorship['tier']['monthly_price_in_dollars'],
                    'currency' => 'USD',
                    'donor_name' => $sponsorship['sponsor']['login'],
                    'message' => $sponsorship['sponsor']['bio'] ?? null
                ];

                $this->revenueService->addDonation($donationData);

                $this->logger->info('GitHub sponsorship processed', $donationData);
            }

            return $response->withStatus(200);
        } catch (\Exception $e) {
            $this->logger->error('GitHub webhook processing failed: ' . $e->getMessage());
            return $response->withStatus(500);
        }
    }

    public function paypalWebhook(Request $request, Response $response): Response
    {
        $payload = $request->getParsedBody() ?? [];

        if (!$this->verifyPayPalSignature($request)) {
            $this->logger->warning('Invalid PayPal webhook signature');
            return $response->withStatus(401);
        }

        try {
            if ($payload['event_type'] === 'PAYMENT.CAPTURE.COMPLETED') {
                $payment = $payload['resource'];

                $donationData = [
                    'source_type' => 'paypal_donation',
                    'amount' => (float)$payment['amount']['value'],
                    'currency' => $payment['amount']['currency_code'],
                    'donor_name' => $payment['payer']['name']['given_name'] ?? 'Anonymous',
                    'message' => $payment['custom_id'] ?? null
                ];

                $this->revenueService->addDonation($donationData);

                $this->logger->info('PayPal donation processed', $donationData);
            }

            return $response->withStatus(200);
        } catch (\Exception $e) {
            $this->logger->error('PayPal webhook processing failed: ' . $e->getMessage());
            return $response->withStatus(500);
        }
    }

    public function stripeWebhook(Request $request, Response $response): Response
    {
        $payload = $request->getBody()->getContents();
        $signature = $request->getHeaderLine('Stripe-Signature');

        if (!$this->verifyStripeSignature($payload, $signature)) {
            $this->logger->warning('Invalid Stripe webhook signature');
            return $response->withStatus(401);
        }

        try {
            $event = json_decode($payload, true);

            if ($event['type'] === 'payment_intent.succeeded') {
                $paymentIntent = $event['data']['object'];

                $donationData = [
                    'source_type' => 'stripe_donation',
                    'amount' => $paymentIntent['amount'] / 100,
                    'currency' => strtoupper($paymentIntent['currency']),
                    'donor_name' => $paymentIntent['metadata']['donor_name'] ?? 'Anonymous',
                    'message' => $paymentIntent['metadata']['message'] ?? null
                ];

                $this->revenueService->addDonation($donationData);

                $this->logger->info('Stripe donation processed', $donationData);
            }

            return $response->withStatus(200);
        } catch (\Exception $e) {
            $this->logger->error('Stripe webhook processing failed: ' . $e->getMessage());
            return $response->withStatus(400);
        }
    }

    private function verifyStripeSignature(string $payload, string $signature): bool
    {
        $secret = $_ENV['STRIPE_WEBHOOK_SECRET'] ?? '';
        if (!$secret || !$signature) {
            return false;
        }

        $elements = explode(',', $signature);
        $timestamp = null;
        $signatures = [];

        foreach ($elements as $element) {
            $parts = explode('=', $element, 2);
            if ($parts[0] === 't') {
                $timestamp = $parts[1];
            } elseif ($parts[0] === 'v1') {
                $signatures[] = $parts[1];
            }
        }

        if (!$timestamp || empty($signatures)) {
            return false;
        }

        $signedPayload = $timestamp . '.' . $payload;
        $expectedSignature = hash_hmac('sha256', $signedPayload, $secret);

        foreach ($signatures as $sig) {
            if (hash_equals($expectedSignature, $sig)) {
                return true;
            }
        }

        return false;
    }

    public function vstDonation(Request $request, Response $response): Response
    {
        $data = $request->getParsedBody() ?? [];

        $apiKey = $request->getHeaderLine('X-API-Key');
        if (!$apiKey || !$this->validateApiKey($apiKey)) {
            return $response->withStatus(401);
        }

        try {
            $donationData = [
                'source_type' => 'vst_donation',
                'amount' => (float)$data['amount'],
                'currency' => $data['currency'] ?? 'USD',
                'donor_name' => $data['donor_name'] ?? 'VST User',
                'message' => $data['message'] ?? 'Donation via VST Plugin'
            ];

            $this->revenueService->addDonation($donationData);

            $responseData = [
                'success' => true,
                'message' => 'Donation recorded successfully',
                'amount' => $donationData['amount']
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            $this->logger->error('VST donation processing failed: ' . $e->getMessage());
            return $response->withStatus(500);
        }
    }

    private function verifyGitHubSignature(Request $request): bool
    {
        $signature = $request->getHeaderLine('X-Hub-Signature-256');
        $payload = $request->getBody()->getContents();
        $secret = $_ENV['GITHUB_WEBHOOK_SECRET'] ?? '';

        if (!$signature || !$secret) {
            return false;
        }

        $expectedSignature = 'sha256=' . hash_hmac('sha256', $payload, $secret);
        return hash_equals($expectedSignature, $signature);
    }

    private function verifyPayPalSignature(Request $request): bool
    {
        // TO DO!
        // https://developer.paypal.com/docs/api/webhooks/v1/#verify-webhook-signature
        return true;
    }

    private function validateApiKey(string $apiKey): bool
    {
        // TO DO!
        return !empty($apiKey);
    }
}
