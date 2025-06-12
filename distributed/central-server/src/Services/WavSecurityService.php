<?php

declare(strict_types=1);

namespace Obsidian\Services;

use RuntimeException;

class WavSecurityService
{
    private string $ffmpegPath;

    public function __construct(string $ffmpegPath = 'ffmpeg')
    {
        $this->ffmpegPath = $ffmpegPath;
    }

    public function validateAndCleanWav(string $inputWavData, array $expectedParams): array
    {
        $startTime = microtime(true);

        try {
            if (strlen($inputWavData) < 44) {
                throw new RuntimeException('WAV data too short');
            }

            if (substr($inputWavData, 0, 4) !== 'RIFF' || substr($inputWavData, 8, 4) !== 'WAVE') {
                throw new RuntimeException('Invalid WAV format');
            }

            $maxSize = ($expectedParams['generation_duration'] ?? 6.0) * 192 * 1024 * 2;
            if (strlen($inputWavData) > $maxSize) {
                throw new RuntimeException('WAV file too large');
            }

            $cleanedData = $this->cleanWavWithFFmpeg($inputWavData, $expectedParams);

            $processingTime = (microtime(true) - $startTime) * 1000;

            return [
                'success' => true,
                'cleaned_data' => $cleanedData,
                'original_size' => strlen($inputWavData),
                'cleaned_size' => strlen($cleanedData),
                'processing_time_ms' => round($processingTime, 2),
                'sha256' => hash('sha256', $cleanedData)
            ];
        } catch (\Exception $e) {
            throw new RuntimeException("WAV validation/cleaning failed: " . $e->getMessage());
        }
    }


    private function cleanWavWithFFmpeg(string $inputWavData, array $expectedParams): string
    {
        $sampleRate = $expectedParams['sample_rate'] ?? 48000;
        $maxDuration = $expectedParams['generation_duration'] ?? 6.0;

        $tempInput = tempnam(sys_get_temp_dir(), 'obsidian_input_');
        $tempOutput = tempnam(sys_get_temp_dir(), 'obsidian_output_');

        try {
            file_put_contents($tempInput, $inputWavData);

            $command = sprintf(
                'ffmpeg -hide_banner -loglevel error -i %s -f wav -ar %d -ac 2 -sample_fmt s16 -t %.1f -y %s 2>&1',
                escapeshellarg($tempInput),
                $sampleRate,
                $maxDuration,
                escapeshellarg($tempOutput)
            );

            exec($command, $output, $returnCode);

            if ($returnCode !== 0) {
                throw new RuntimeException("FFmpeg failed: " . implode("\n", $output));
            }

            $cleanedData = file_get_contents($tempOutput);

            if (!$cleanedData) {
                throw new RuntimeException("Failed to read cleaned WAV");
            }

            return $cleanedData;
        } finally {
            if (file_exists($tempInput)) unlink($tempInput);
            if (file_exists($tempOutput)) unlink($tempOutput);
        }
    }

    public function getAudioInfo(string $wavData): array
    {
        $tempFile = tempnam(sys_get_temp_dir(), 'obsidian_info_') . '.wav';

        try {
            file_put_contents($tempFile, $wavData);

            $command = sprintf(
                '%s -hide_banner -i %s -f null - 2>&1 | grep "Duration\\|Stream"',
                escapeshellcmd($this->ffmpegPath),
                escapeshellarg($tempFile)
            );

            $output = shell_exec($command);

            if (preg_match('/Duration: (\d+):(\d+):(\d+\.\d+)/', $output, $matches)) {
                $hours = (int)$matches[1];
                $minutes = (int)$matches[2];
                $seconds = (float)$matches[3];
                $duration = $hours * 3600 + $minutes * 60 + $seconds;
            } else {
                $duration = 0;
            }

            $channels = 2;
            $sampleRate = 48000;
            $bitDepth = 16;

            if (preg_match('/Stream.*Audio:.*(\d+) Hz.*(\d+) channels/', $output, $matches)) {
                $sampleRate = (int)$matches[1];
                $channels = (int)$matches[2];
            }

            return [
                'duration' => round($duration, 2),
                'sample_rate' => $sampleRate,
                'channels' => $channels,
                'bit_depth' => $bitDepth,
                'size_bytes' => strlen($wavData)
            ];
        } finally {
            if (file_exists($tempFile)) {
                unlink($tempFile);
            }
        }
    }
}
