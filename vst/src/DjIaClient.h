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

    DjIaClient::DjIaClient(const juce::String &apiKey = "", const juce::String &baseUrl = "http://localhost:8000")
        : apiKey(apiKey), baseUrl(baseUrl + "/api/v1")
    {
    }

    bool isServerHealthy()
    {
        auto url = juce::URL(baseUrl + "/health");

        // Créer les options en une seule expression
        if (auto response = url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withConnectionTimeoutMs(120000)
                    .withExtraHeaders(apiKey.isNotEmpty() ? "X-API-Key: " + apiKey + "\n" : juce::String())))
        {
            auto jsonResponse = juce::JSON::parse(response->readEntireStreamAsString());
            return jsonResponse.getProperty("status", "unhealthy") == "healthy";
        }

        return false;
    }

    LoopResponse generateLoop(const LoopRequest &request)
    {
        try
        {
            // Debug info
            writeToLog("Generating loop with parameters:");
            writeToLog("  Prompt: " + request.prompt);
            writeToLog("  Style: " + request.style);
            writeToLog("  BPM: " + juce::String(request.bpm));
            writeToLog("  Key: " + request.key);
            writeToLog("  API Key present: " + juce::String(apiKey.isNotEmpty() ? "yes" : "no"));

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
                writeToLog("Adding stems to request: " + juce::JSON::toString(stems));
            }
            else
            {
                writeToLog("No stems selected, skipping preferred_stems in request");
            }

            // Convertir en string JSON
            auto jsonString = juce::JSON::toString(jsonRequest);
            writeToLog("Request JSON: " + jsonString);

            // Construire les headers dans le bon ordre
            juce::StringPairArray headers;
            headers.set("Content-Type", "application/json");

            if (apiKey.isNotEmpty())
            {
                headers.set("X-API-Key", apiKey);
                writeToLog("Added API Key to headers");
            }
            else
            {
                writeToLog("WARNING: No API Key provided!");
            }

            // Convertir les headers en string
            juce::String headerString;
            for (auto &key : headers.getAllKeys())
            {
                headerString += key + ": " + headers[key] + "\n";
            }
            writeToLog("Request headers: " + headerString);

            // Créer l'URL avec les données POST
            auto url = juce::URL(baseUrl + "/generate")
                           .withPOSTData(jsonString);

            writeToLog("Sending request to: " + url.toString(false));

            // Créer la requête avec toutes les options
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                               .withConnectionTimeoutMs(1200000)
                               .withExtraHeaders(headerString);

            // Dans DjIaClient.h, dans la méthode generateLoop
            if (auto response = url.createInputStream(options))
            {
                auto responseString = response->readEntireStreamAsString();
                writeToLog("Received response: " + responseString);

                auto jsonResponse = juce::JSON::parse(responseString);

                // Vérifier si la réponse contient une erreur
                if (jsonResponse.hasProperty("detail"))
                {
                    throw std::runtime_error(jsonResponse["detail"].toString().toStdString());
                }

                LoopResponse result;

                // Décoder le base64 en données audio
                juce::String audioBase64 = jsonResponse.getProperty("audio_data", "").toString();
                if (audioBase64.isNotEmpty())
                {
                    juce::MemoryOutputStream decoded;
                    juce::Base64::convertFromBase64(decoded, audioBase64);
                    result.audioData = decoded.getMemoryBlock();
                    writeToLog("Decoded audio data size: " + juce::String(result.audioData.getSize()));
                }
                else
                {
                    writeToLog("No audio data received in response");
                }

                result.duration = jsonResponse.getProperty("duration", 0.0);
                result.bpm = jsonResponse.getProperty("bpm", 120.0);
                result.key = jsonResponse.getProperty("key", "");
                result.llmReasoning = jsonResponse.getProperty("llm_reasoning", "");
                if (jsonResponse.hasProperty("sample_rate"))
                {
                    result.sampleRate = static_cast<double>(jsonResponse.getProperty("sample_rate", 44100.0));
                    writeToLog("Extracted sample_rate from JSON: " + juce::String(result.sampleRate) + " Hz");
                }
                else
                {
                    result.sampleRate = 44100.0; // Valeur par défaut
                    writeToLog("WARNING: No sample_rate field in JSON response, using default 44100 Hz");
                }

                // Parser les stems utilisés
                if (auto stemsArray = jsonResponse.getProperty("stems_used", juce::var()).getArray())
                {
                    for (const auto &stem : *stemsArray)
                        result.stemsUsed.push_back(stem.toString());
                }

                return result;
            }
        }
        catch (const std::exception &e)
        {
            writeToLog("Error in generateLoop: " + juce::String(e.what()));
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