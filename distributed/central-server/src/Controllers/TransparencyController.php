<?php

declare(strict_types=1);

namespace Obsidian\Controllers;

use Obsidian\Services\RevenueDistributionService;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;

class TransparencyController
{
    private RevenueDistributionService $revenueService;

    public function __construct(RevenueDistributionService $revenueService)
    {
        $this->revenueService = $revenueService;
    }

    public function getMonthlyReport(Request $request, Response $response): Response
    {
        try {
            $month = $request->getAttribute('month') ?? date('Y-m');

            if (!preg_match('/^\d{4}-\d{2}$/', $month)) {
                return $this->errorResponse($response, 'INVALID_MONTH', 'Month must be in YYYY-MM format', 400);
            }

            $report = $this->revenueService->getMonthlyTransparencyReport($month);

            $responseData = [
                'success' => true,
                'data' => $report
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to generate transparency report', 500);
        }
    }

    public function getCurrentReport(Request $request, Response $response): Response
    {
        return $this->getMonthlyReport($request->withAttribute('month', date('Y-m')), $response);
    }

    public function getAvailableMonths(Request $request, Response $response): Response
    {
        try {
            $months = $this->revenueService->getAvailableMonths();

            $responseData = [
                'success' => true,
                'data' => [
                    'months' => $months,
                    'current_month' => date('Y-m'),
                    'last_processed' => $months[0] ?? null
                ]
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to load available months', 500);
        }
    }

    public function getDonationStats(Request $request, Response $response): Response
    {
        try {
            $stats = $this->revenueService->getDonationStats();

            $responseData = [
                'success' => true,
                'data' => $stats
            ];

            $response->getBody()->write(json_encode($responseData));
            return $response->withHeader('Content-Type', 'application/json');
        } catch (\Exception $e) {
            return $this->errorResponse($response, 'SERVER_ERROR', 'Failed to load donation stats', 500);
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
