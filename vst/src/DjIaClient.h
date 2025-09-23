﻿#pragma once
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
		juce::String errorMessage = "";
		int creditsRemaining = -1;
		bool isUnlimitedKey = false;
		int totalCredits = -1;
		int usedCredits = -1;

		LoopResponse()
			: duration(0.0f), bpm(120.0f)
		{
		}
	};

	DjIaClient(const juce::String &apiKey = "", const juce::String &baseUrl = "http://localhost:8000")
		: apiKey(apiKey), baseUrl(baseUrl + "/api/v1")
	{
	}

	void setApiKey(const juce::String &newApiKey)
	{
		apiKey = newApiKey;
		DBG("DjIaClient: API key updated");
	}

	void setBaseUrl(const juce::String &newBaseUrl)
	{
		if (newBaseUrl.endsWith("/"))
		{
			baseUrl = newBaseUrl.dropLastCharacters(1) + "/api/v1";
		}
		else
		{
			baseUrl = newBaseUrl + "/api/v1";
		}
		DBG("DjIaClient: Base URL updated to: " + baseUrl);
	}

	LoopResponse generateLoop(const LoopRequest &request, double sampleRate, int requestTimeoutMS)
	{
		try
		{
			juce::var jsonRequest(new juce::DynamicObject());
			float bpm = request.bpm;
			if (bpm < 0.0f)
			{
				bpm = 110.0f;
			}
			jsonRequest.getDynamicObject()->setProperty("prompt", request.prompt);
			jsonRequest.getDynamicObject()->setProperty("bpm", bpm);
			jsonRequest.getDynamicObject()->setProperty("key", request.key);
			jsonRequest.getDynamicObject()->setProperty("sample_rate", sampleRate);
			jsonRequest.getDynamicObject()->setProperty("generation_duration", request.generationDuration);

			if (!request.preferredStems.empty())
			{
				juce::Array<juce::var> stems;
				for (const auto &stem : request.preferredStems)
					stems.add(stem);
				jsonRequest.getDynamicObject()->setProperty("preferred_stems", stems);
			}

			auto jsonString = juce::JSON::toString(jsonRequest);

			juce::String headerString = "Content-Type: application/json\n";
			if (apiKey.isNotEmpty())
			{
				headerString += "X-API-Key: " + apiKey + "\n";
			}
			if (baseUrl.isEmpty())
			{
				DBG("ERROR: Base URL is empty!");
				throw std::runtime_error("Server URL not configured. Please set server URL in settings.");
			}

			if (!baseUrl.startsWithIgnoreCase("http"))
			{
				DBG("ERROR: Invalid URL format: " + baseUrl);
				throw std::runtime_error("Invalid server URL format. Must start with http:// or https://");
			}
			int statusCode = 0;
			juce::StringPairArray responseHeaders;
			auto url = juce::URL(baseUrl + "/generate")
						   .withPOSTData(jsonString);
			auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
							   .withStatusCode(&statusCode)
							   .withResponseHeaders(&responseHeaders)
							   .withExtraHeaders(headerString)
							   .withConnectionTimeoutMs(requestTimeoutMS);

			auto response = url.createInputStream(options);
			if (!response)
			{
				DBG("ERROR: Failed to connect to server");
				throw std::runtime_error(("Cannot connect to server at " + baseUrl +
										  ". Please check: Server is running, URL is correct, Network connection")
											 .toStdString());
			}

			DBG("HTTP Status Code: " + juce::String(statusCode));

			if (statusCode == 403)
			{
				DBG("ERROR: HTTP 403 Forbidden");
				throw std::runtime_error("Authentication failed: Invalid or expired API key. Please check your credentials.");
			}
			else if (statusCode == 401)
			{
				DBG("ERROR: HTTP 401 Unauthorized");
				throw std::runtime_error("Authentication failed: API key required or invalid.");
			}
			else if (statusCode == 422)
			{
				DBG("ERROR: HTTP 422 Unprocessable Entity");
				throw std::runtime_error("Invalid request: The server could not process your request. Please check your prompt and parameters.");
			}
			else if (statusCode == 500)
			{
				DBG("ERROR: HTTP 500 Internal Server Error");
				throw std::runtime_error("Server error: The audio generation service is temporarily unavailable. Please try again later.");
			}
			else if (statusCode == 503)
			{
				DBG("ERROR: HTTP 503 Service Unavailable");
				throw std::runtime_error("Service unavailable: All GPU providers are currently busy. Please try again in a few moments.");
			}
			else if (statusCode != 200)
			{
				DBG("ERROR: HTTP " + juce::String(statusCode));
				throw std::runtime_error("HTTP Error " + std::to_string(statusCode) + ": Request failed.");
			}

			if (response->isExhausted())
			{
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
			else
			{
				DBG("ERROR: Cannot create temp file");
				throw std::runtime_error("Cannot create temporary file for audio data.");
			}
			result.duration = request.generationDuration;
			result.bpm = bpm;
			result.key = request.key;
			result.stemsUsed = request.preferredStems;
			juce::String creditsRemaining = responseHeaders["X-Credits-Remaining"];
			juce::String duration = responseHeaders["X-Duration"];
			if (creditsRemaining.isNotEmpty())
			{
				if (creditsRemaining == "unlimited")
				{
					result.isUnlimitedKey = true;
					result.creditsRemaining = -1;
				}
				else
				{
					result.creditsRemaining = creditsRemaining.getIntValue();
					result.isUnlimitedKey = false;
				}
			}

			DBG("WAV file created: " + result.audioData.getFullPathName() +
				" (" + juce::String(result.audioData.getSize()) + " bytes)");

			return result;
		}
		catch (const std::exception &e)
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