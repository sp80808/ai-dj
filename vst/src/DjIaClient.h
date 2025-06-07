#pragma once
#include "./JuceHeader.h"

class DjIaClient
{
public:
	struct LoopRequest
	{
		juce::String prompt;
		float generationDuration;
		float bpm;
		juce::String key;
		std::vector<juce::String> preferredStems;

		LoopRequest()
			: prompt(""),
			generationDuration(6.0f),
			bpm(120.0f),
			key(""),
			preferredStems()
		{
		}
	};

	struct LoopResponse
	{
		juce::File audioData;
		float duration;
		float bpm;
		juce::String key;
		std::vector<juce::String> stemsUsed;
		double sampleRate;
		juce::String errorMessage = "";

		LoopResponse()
			: duration(0.0f), bpm(120.0f), sampleRate(48000.0)
		{
		}
	};

	DjIaClient(const juce::String& apiKey = "", const juce::String& baseUrl = "http://localhost:8000")
		: apiKey(apiKey), baseUrl(baseUrl + "/api/v1")
	{
	}

	void setApiKey(const juce::String& newApiKey) {
		apiKey = newApiKey;
		DBG("🔧 DjIaClient: API key updated");
	}

	void setBaseUrl(const juce::String& newBaseUrl) {
		if (newBaseUrl.endsWith("/")) {
			baseUrl = newBaseUrl.dropLastCharacters(1) + "/api/v1";
		}
		else {
			baseUrl = newBaseUrl + "/api/v1";
		}
		DBG("🔧 DjIaClient: Base URL updated to: " + baseUrl);
	}

	LoopResponse generateLoop(const LoopRequest& request)
	{
		try
		{
			juce::var jsonRequest(new juce::DynamicObject());
			jsonRequest.getDynamicObject()->setProperty("prompt", request.prompt);
			jsonRequest.getDynamicObject()->setProperty("bpm", request.bpm);
			jsonRequest.getDynamicObject()->setProperty("key", request.key);

			if (!request.preferredStems.empty())
			{
				juce::Array<juce::var> stems;
				for (const auto& stem : request.preferredStems)
					stems.add(stem);
				jsonRequest.getDynamicObject()->setProperty("preferred_stems", stems);
			}

			auto jsonString = juce::JSON::toString(jsonRequest);

			juce::String headerString = "Content-Type: application/json\n";
			if (apiKey.isNotEmpty())
			{
				headerString += "X-API-Key: " + apiKey + "\n";
			}
			if (baseUrl.isEmpty()) {
				DBG("ERROR: Base URL is empty!");
				throw std::runtime_error("Server URL not configured. Please set server URL in settings.");
			}

			if (!baseUrl.startsWithIgnoreCase("http")) {
				DBG("ERROR: Invalid URL format: " + baseUrl);
				throw std::runtime_error("Invalid server URL format. Must start with http:// or https://");
			}
			auto url = juce::URL(baseUrl + "/generate").withPOSTData(jsonString);
			auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
				.withConnectionTimeoutMs(360000)
				.withExtraHeaders(headerString);

			auto response = url.createInputStream(options);

			if (!response) {
				DBG("ERROR: Failed to connect to server");
				throw std::runtime_error(("Cannot connect to server at " + baseUrl +
					". Please check: Server is running, URL is correct, Network connection").toStdString());
			}

			if (response->isExhausted()) {
				DBG("ERROR: Empty response from server");
				throw std::runtime_error("Server returned empty response. Server may be overloaded or misconfigured.");
			}

			LoopResponse result;
			result.audioData = juce::File::createTempFile(".wav");
			juce::FileOutputStream stream(result.audioData);
			if (stream.openedOk())
			{
				stream.writeFromInputStream(*response, response->getTotalLength());
			}
			else {
				DBG("ERROR: Cannot create temp file");
				throw std::runtime_error("Cannot create temporary file for audio data.");
			}
			result.duration = request.generationDuration;
			result.bpm = request.bpm;
			result.key = request.key;
			result.sampleRate = 48000.0;
			result.stemsUsed = request.preferredStems;

			DBG("WAV file created: " + result.audioData.getFullPathName() +
				" (" + juce::String(result.audioData.getSize()) + " bytes)");

			return result;

		}
		catch (const std::exception& e)
		{
			DBG("API Error: " + juce::String(e.what()));
			LoopResponse emptyResponse;
			emptyResponse.errorMessage = e.what();
			return emptyResponse;
		}
	}

private:
	juce::String apiKey;
	juce::String baseUrl;
};