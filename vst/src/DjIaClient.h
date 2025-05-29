#pragma once
#include "./JuceHeader.h"

class DjIaClient
{
public:
    struct LoopRequest
    {
        juce::String prompt;
        juce::String style;
        float bpm;
        juce::String key;
        int measures;
        std::vector<juce::String> preferredStems;
        float generationDuration;

        LoopRequest()
            : generationDuration(6.0f), bpm(120.0f), measures(4)
        {
        }
    };

    struct LoopResponse
    {
        juce::MemoryBlock audioData;
        float duration;
        float bpm;
        juce::String key;
        std::vector<juce::String> stemsUsed;
        juce::String llmReasoning;
        double sampleRate;

        LoopResponse()
            : duration(0.0f), bpm(120.0f), sampleRate(44100.0)
        {
        }
    };

    DjIaClient(const juce::String &apiKey = "", const juce::String &baseUrl = "http://localhost:8000")
        : apiKey(apiKey), baseUrl(baseUrl + "/api/v1")
    {
    }


    LoopResponse generateLoop(const LoopRequest &request)
    {
        try
        {
            juce::var jsonRequest(new juce::DynamicObject());
            jsonRequest.getDynamicObject()->setProperty("prompt", request.prompt);
            jsonRequest.getDynamicObject()->setProperty("style", request.style);
            jsonRequest.getDynamicObject()->setProperty("bpm", request.bpm);
            jsonRequest.getDynamicObject()->setProperty("key", request.key);
            jsonRequest.getDynamicObject()->setProperty("measures", request.measures);

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

            auto url = juce::URL(baseUrl + "/generate").withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                               .withConnectionTimeoutMs(120000)
                               .withExtraHeaders(headerString);

            if (auto response = url.createInputStream(options))
            {
                auto responseString = response->readEntireStreamAsString();
                auto jsonResponse = juce::JSON::parse(responseString);

                if (jsonResponse.hasProperty("detail"))
                {
                    throw std::runtime_error(jsonResponse["detail"].toString().toStdString());
                }

                LoopResponse result;

                juce::String audioBase64 = jsonResponse.getProperty("audio_data", "").toString();
                if (audioBase64.isNotEmpty())
                {
                    juce::MemoryOutputStream decoded;
                    juce::Base64::convertFromBase64(decoded, audioBase64);
                    result.audioData = decoded.getMemoryBlock();
                }

                result.duration = jsonResponse.getProperty("duration", 0.0);
                result.bpm = jsonResponse.getProperty("bpm", 120.0);
                result.key = jsonResponse.getProperty("key", "");
                result.llmReasoning = jsonResponse.getProperty("llm_reasoning", "");
                result.sampleRate = static_cast<double>(jsonResponse.getProperty("sample_rate", 48000.0));

                if (auto stemsArray = jsonResponse.getProperty("stems_used", juce::var()).getArray())
                {
                    for (const auto &stem : *stemsArray)
                        result.stemsUsed.push_back(stem.toString());
                }
                return result;
            }
            else
            {
                throw std::runtime_error("Failed to connect to server");
            }
        }
        catch (const std::exception &e)
        {
            throw;
        }
    }

private:
    juce::String apiKey;
    juce::String baseUrl;
};