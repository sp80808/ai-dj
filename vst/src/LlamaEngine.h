#pragma once
#include <JuceHeader.h>
#include "llama.h"
#include "common.h"
#include <nlohmann/json.hpp>
#include <regex>
#include <map>
#include <vector>
#include <mutex>

struct ChatMessage {
	juce::String role;
	juce::String content;
	double timestamp;
};

class LlamaEngine {
private:
	llama_model* model = nullptr;
	llama_context* ctx = nullptr;
	llama_sampler* sampler = nullptr;
	std::map<juce::String, std::vector<ChatMessage>> conversations;
	std::mutex conversationsMutex;

public:
	bool initialize(const juce::String& modelPath);
	nlohmann::json getNextDecision(const juce::String& userPrompt,
		const juce::String& userId,
		float bpm,
		const juce::String& key);

private:
	void initUserConversation(const juce::String& userId);
	juce::String buildPrompt(const juce::String& userPrompt, float bpm, const juce::String& key);
	juce::String generateResponse(const juce::String& userId);
	nlohmann::json parseDecisionResponse(const juce::String& response,
		const juce::String& defaultKey, const juce::String& userPrompt,
		float bpm);
	juce::String getSystemPrompt();
	void cleanupConversation(const juce::String& userId);
};