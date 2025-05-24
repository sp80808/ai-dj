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
    };

    DjIaClient(const juce::String &apiKey = "", const juce::String &baseUrl = "http://localhost:8000")
        : apiKey(apiKey), baseUrl(baseUrl + "/api/v1")
    {
    }

    bool isServerHealthy()
    {
        try
        {
            auto url = juce::URL(baseUrl + "/health");
            
            if (auto response = url.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(5000) // Timeout plus court
                        .withExtraHeaders(apiKey.isNotEmpty() ? "X-API-Key: " + apiKey + "\n" : juce::String())))
            {
                auto jsonResponse = juce::JSON::parse(response->readEntireStreamAsString());
                return jsonResponse.getProperty("status", "unhealthy") == "healthy";
            }
        }
        catch (...)
        {
            return false;
        }
        
        return false;
    }

    LoopResponse generateLoop(const LoopRequest &request)
    {
        try
        {
            writeToLog("🚀 Generating loop with prompt: " + request.prompt);

            // Créer le JSON de la requête
            juce::var jsonRequest(new juce::DynamicObject());
            jsonRequest.getDynamicObject()->setProperty("prompt", request.prompt);
            jsonRequest.getDynamicObject()->setProperty("style", request.style);
            jsonRequest.getDynamicObject()->setProperty("bpm", request.bpm);
            jsonRequest.getDynamicObject()->setProperty("key", request.key);
            jsonRequest.getDynamicObject()->setProperty("measures", request.measures);

            // Ajouter les stems si présents
            if (!request.preferredStems.empty())
            {
                juce::Array<juce::var> stems;
                for (const auto &stem : request.preferredStems)
                    stems.add(stem);
                jsonRequest.getDynamicObject()->setProperty("preferred_stems", stems);
            }

            auto jsonString = juce::JSON::toString(jsonRequest);

            // Headers simplifiés
            juce::String headerString = "Content-Type: application/json\n";
            if (apiKey.isNotEmpty())
            {
                headerString += "X-API-Key: " + apiKey + "\n";
            }

            // Créer la requête
            auto url = juce::URL(baseUrl + "/generate").withPOSTData(jsonString);
            
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                               .withConnectionTimeoutMs(120000)
                               .withExtraHeaders(headerString);

            if (auto response = url.createInputStream(options))
            {
                auto responseString = response->readEntireStreamAsString();
                auto jsonResponse = juce::JSON::parse(responseString);

                // Vérifier les erreurs
                if (jsonResponse.hasProperty("detail"))
                {
                    throw std::runtime_error(jsonResponse["detail"].toString().toStdString());
                }

                LoopResponse result;

                // Décoder le base64
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
                result.sampleRate = static_cast<double>(jsonResponse.getProperty("sample_rate", 44100.0));

                // Parser les stems utilisés
                if (auto stemsArray = jsonResponse.getProperty("stems_used", juce::var()).getArray())
                {
                    for (const auto &stem : *stemsArray)
                        result.stemsUsed.push_back(stem.toString());
                }

                writeToLog("✅ Loop generated successfully");
                return result;
            }
            else
            {
                throw std::runtime_error("Failed to connect to server");
            }
        }
        catch (const std::exception &e)
        {
            writeToLog("❌ Error in generateLoop: " + juce::String(e.what()));
            throw;
        }
    }

private:
    static void writeToLog(const juce::String &message)
    {
        auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("dj_ia_vst.log");

        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        file.appendText(time + ": " + message + "\n");
    }

    juce::String apiKey;
    juce::String baseUrl;
};