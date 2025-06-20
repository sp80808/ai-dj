#pragma once
#include <JuceHeader.h>
#include "LlamaEngine.h"
#include "StableAudioEngine.h"
#include <nlohmann/json.hpp>

class ObsidianEngine
{
public:
	struct LoopRequest
	{
		juce::String prompt;
		float generationDuration = 10.0f;
		float bpm = 120.0f;
		juce::String key = "C Aeolian";
		std::vector<juce::String> preferredStems;
	};

	struct LoopResponse
	{
		bool success = false;
		juce::String errorMessage;
		std::vector<float> audioData;
		std::vector<float> leftChannel;
		std::vector<float> rightChannel;
		float actualDuration = 0.0f;
		float bpm = 120.0f;
		float duration = 0.0f;
		juce::String llmReasoning;
		juce::String optimizedPrompt;
		std::vector<juce::String> stemsUsed;
	};

	typedef std::function<void(LoopResponse)> GenerationCallback;

private:
	std::unique_ptr<LlamaEngine> llamaEngine;
	std::unique_ptr<StableAudioEngine> stableAudioEngine;

	juce::File appDataDir;
	juce::String currentUserId = "default_user";

public:
	bool initialize()
	{
		appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
						 .getChildFile("OBSIDIAN-Neural");

		auto llamaDir = appDataDir.getChildFile("llama");
		auto stableAudioDir = appDataDir.getChildFile("stable-audio");

		llamaEngine = std::make_unique<LlamaEngine>();
		auto llamaModelFile = llamaDir.getChildFile("gemma-3-4b-it-Q4_K_M.gguf");

		if (!llamaEngine->initialize(llamaModelFile.getFullPathName()))
		{
			DBG("Failed to initialize LLama engine");
			return false;
		}

		stableAudioEngine = std::make_unique<StableAudioEngine>();
		if (!stableAudioEngine->initialize(stableAudioDir.getFullPathName()))
		{
			DBG("Failed to initialize Stable Audio engine");
			return false;
		}

		DBG("OBSIDIAN Neural engines ready!");
		return true;
	}

	void generateLoopAsync(const LoopRequest &request, GenerationCallback callback)
	{
		juce::Thread::launch([this, request, callback]()
							 {
			auto response = generateLoop(request);
			juce::MessageManager::callAsync([callback, response]() {
				callback(response);
				}); });
	}

private:
	LoopResponse generateLoop(const LoopRequest &request)
	{
		LoopResponse response;
		try
		{
			DBG("Phase 1: LLM processing prompt...");
			auto llmDecision = llamaEngine->getNextDecision(
				request.prompt,
				currentUserId,
				request.bpm,
				request.key);

			juce::String optimizedPrompt = "electronic music sample";
			juce::String reasoning = "LLM processing";

			if (llmDecision.contains("parameters") &&
				llmDecision["parameters"].contains("sample_details") &&
				llmDecision["parameters"]["sample_details"].contains("musicgen_prompt"))
			{
				optimizedPrompt = juce::String(llmDecision["parameters"]["sample_details"]["musicgen_prompt"].get<std::string>());
			}
			if (llmDecision.contains("reasoning"))
			{
				reasoning = juce::String(llmDecision["reasoning"].get<std::string>());
			}

			DBG("LLM optimized prompt: " << optimizedPrompt);
			DBG("LLM reasoning: " << reasoning);
			DBG("Phase 2: Generating audio...");

			StableAudioEngine::GenerationParams audioParams;
			audioParams.prompt = optimizedPrompt;
			audioParams.duration = request.generationDuration;
			audioParams.numThreads = 4;
			audioParams.seed = -1;

			auto audioResult = stableAudioEngine->generateSample(audioParams);

			if (audioResult.success)
			{
				response.success = true;
				response.audioData = audioResult.audioData;
				response.leftChannel = audioResult.leftChannel;
				response.rightChannel = audioResult.rightChannel;
				response.actualDuration = audioResult.actualDuration;
				response.duration = audioResult.actualDuration;
				response.bpm = request.bpm;
				response.llmReasoning = reasoning;
				response.optimizedPrompt = optimizedPrompt;
				response.stemsUsed = request.preferredStems;
				DBG("Generation successful! Duration: " << response.actualDuration << "s");
			}
			else
			{
				response.success = false;
				response.errorMessage = "Audio generation failed: " + audioResult.errorMessage;
			}
		}
		catch (const std::exception &e)
		{
			response.success = false;
			response.errorMessage = juce::String("Generation exception: ") + e.what();
		}
		return response;
	}
};