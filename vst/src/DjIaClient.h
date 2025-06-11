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
		DBG("🔧 DjIaClient: API key updated");
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
		DBG("🔧 DjIaClient: Base URL updated to: " + baseUrl);
	}

	LoopResponse generateLoop(const LoopRequest &request, double sampleRate, int requestTimeoutMS)
	{
		try
		{
			juce::var jsonRequest(new juce::DynamicObject());
			jsonRequest.getDynamicObject()->setProperty("prompt", request.prompt);
			jsonRequest.getDynamicObject()->setProperty("bpm", request.bpm);
			jsonRequest.getDynamicObject()->setProperty("key", request.key);
			jsonRequest.getDynamicObject()->setProperty("sample_rate", sampleRate);

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
			auto url = juce::URL(baseUrl + "/generate").withPOSTData(jsonString);
			auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
							   .withConnectionTimeoutMs(requestTimeoutMS)
							   .withExtraHeaders(headerString);

			auto response = url.createInputStream(options);

			if (response->isExhausted())
			{
				DBG("ERROR: Empty response from server");
				throw std::runtime_error("Server returned empty response. Server may be overloaded or misconfigured.");
			}

			auto responseBody = response->readEntireStreamAsString();
			if (responseBody.isEmpty())
			{
				DBG("ERROR: Empty response body");
				throw std::runtime_error("Server returned empty data. Please try again.");
			}

			auto json = juce::JSON::parse(responseBody);
			if (json.isObject())
			{
				auto errorObj = json["error"];
				if (!errorObj.isVoid())
				{
					auto errorCode = errorObj["code"].toString();
					auto errorMessage = errorObj["message"].toString();

					if (errorCode == "INVALID_KEY")
					{
						DBG("ERROR: Invalid API key");
						throw std::runtime_error("Authentication failed: Invalid API key. Please check your credentials.");
					}
					else if (errorCode == "KEY_EXPIRED")
					{
						DBG("ERROR: API key expired");
						throw std::runtime_error("Your API key has expired. Please contact support to renew your access.");
					}
					else if (errorCode == "CREDITS_EXHAUSTED")
					{
						DBG("ERROR: No credits remaining");

						auto fullMessage = errorMessage.toStdString();
						std::string userMessage = "No generation credits remaining on your API key.";

						size_t usedPos = fullMessage.find("used ");
						if (usedPos != std::string::npos)
						{
							size_t endPos = fullMessage.find(" generations", usedPos);
							if (endPos != std::string::npos)
							{
								std::string creditsInfo = fullMessage.substr(usedPos, endPos - usedPos + 12);
								userMessage += " You have " + creditsInfo + " for this API key.";
							}
						}
						else
						{
							userMessage += " " + fullMessage;
						}

						throw std::runtime_error(userMessage);
					}
					else if (errorCode == "ACCESS_DENIED")
					{
						DBG("ERROR: Access forbidden");
						throw std::runtime_error("Access denied: Your API key may be expired or you don't have permission to access this resource.");
					}
					else if (errorCode == "RATE_LIMIT")
					{
						DBG("ERROR: Rate limit exceeded");
						throw std::runtime_error("Rate limit exceeded: Too many requests. Please wait before trying again.");
					}
					else if (errorCode == "SERVER_ERROR")
					{
						DBG("ERROR: Server internal error");
						throw std::runtime_error("Server error: The audio generation service is temporarily unavailable. Please try again later.");
					}
					else if (errorCode == "GPU_UNAVAILABLE")
					{
						DBG("ERROR: No GPU available");
						throw std::runtime_error("No GPU providers available: " + errorMessage.toStdString() +
												 " Please try again in a few minutes.");
					}
					else if (errorCode == "INVALID_PROMPT")
					{
						DBG("ERROR: Invalid prompt");
						throw std::runtime_error("Invalid prompt: " + errorMessage.toStdString());
					}
					else
					{
						DBG("ERROR: API error - " + errorCode);
						throw std::runtime_error("API Error: " + errorMessage.toStdString());
					}
				}
				auto audioData = json["audio"];
				if (audioData.isVoid())
				{
					DBG("ERROR: No audio data in response");
					throw std::runtime_error("Server response missing audio data. Please try again.");
				}
			}
			else
			{
				DBG("ERROR: Invalid JSON response");
				throw std::runtime_error("Invalid server response. Please check if the service is available.");
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
			result.bpm = request.bpm;
			result.key = request.key;
			result.stemsUsed = request.preferredStems;
			if (json.hasProperty("credits_remaining"))
			{
				auto creditsStr = json["credits_remaining"].toString();
				if (creditsStr == "unlimited")
				{
					result.isUnlimitedKey = true;
					result.creditsRemaining = -1;
				}
				else
				{
					result.creditsRemaining = creditsStr.getIntValue();
					result.isUnlimitedKey = false;
				}
			}
			if (json.hasProperty("total_credits"))
			{
				result.totalCredits = json["total_credits"];
			}
			if (json.hasProperty("used_credits"))
			{
				result.usedCredits = json["used_credits"];
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