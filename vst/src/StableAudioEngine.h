/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>

class StableAudioEngine
{
public:
	struct GenerationParams
	{
		juce::String prompt;
		float duration = 10.0f;
		int numThreads = 4;
		int seed = -1;
		int sampleRate = 44100;

		GenerationParams() = default;
		GenerationParams(const juce::String &p, float dur = 10.0f)
			: prompt(p), duration(dur)
		{
		}
	};

	struct GenerationResult
	{
		std::vector<float> audioData;
		std::vector<float> leftChannel;
		std::vector<float> rightChannel;
		float actualDuration = 0.0f;
		bool success = false;
		juce::String errorMessage = "";
		juce::String performanceInfo = "";

		bool isValid() const
		{
			return success && !audioData.empty() && actualDuration > 0.0f;
		}
	};

private:
	bool isInitialized = false;
	juce::String modelsDirectory;
	juce::File audiogenExecutable;
	juce::Random random;

public:
	StableAudioEngine() {}

	bool initialize(const juce::String &modelsDir);
	bool isReady() const { return isInitialized; }

	GenerationResult generateSample(const GenerationParams &params);
	std::vector<float> generateAudio(const juce::String &prompt, float duration = 10.0f);

private:
	bool checkRequiredFiles();
	std::vector<float> loadAndResampleWavFile(const juce::File &wavFile, double targetSampleRate);
	juce::AudioBuffer<float> resampleBuffer(const juce::AudioBuffer<float> &inputBuffer,
											double inputSampleRate,
											double outputSampleRate);
	void cleanupTempFiles();
	juce::String sanitizePrompt(const juce::String &prompt);
	int generateRandomSeed();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StableAudioEngine)
};