#pragma once
#include "JuceHeader.h"
#include "SoundTouch.h"
#include "BPMDetect.h"

class AudioAnalyzer
{
public:
    static float detectBPM(const juce::AudioBuffer<float> &buffer, double sampleRate)
    {
        if (buffer.getNumSamples() == 0)
            return 0.0f;

        try
        {
            DBG("Starting BPM detection...");
            DBG("  Buffer size: " + juce::String(buffer.getNumSamples()) + " samples");
            DBG("  Duration: " + juce::String(buffer.getNumSamples() / sampleRate, 2) + " seconds");

            // SoundTouch BPMDetect ne fonctionne qu'en mono
            soundtouch::BPMDetect bpmDetect(1, (int)sampleRate);

            // Convertir en mono et normaliser
            std::vector<float> monoData;
            monoData.reserve(buffer.getNumSamples());

            float maxLevel = 0.0f;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float mono = buffer.getSample(0, i);
                if (buffer.getNumChannels() > 1)
                {
                    mono = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
                }

                maxLevel = std::max(maxLevel, std::abs(mono));
                monoData.push_back(mono);
            }

            DBG("  Max level: " + juce::String(maxLevel, 3));

            // Vérifier que l'audio n'est pas trop faible
            if (maxLevel < 0.001f)
            {
                DBG("Audio level too low for BPM detection");
                return 0.0f;
            }

            // Normaliser pour améliorer la détection
            float normalizeGain = 0.5f / maxLevel;
            for (auto &sample : monoData)
            {
                sample *= normalizeGain;
            }

            // Analyser par chunks
            const int chunkSize = 4096;

            for (size_t i = 0; i < monoData.size(); i += chunkSize)
            {
                size_t remaining = std::min((size_t)chunkSize, monoData.size() - i);
                bpmDetect.inputSamples(&monoData[i], (int)remaining);
            }

            float detectedBPM = bpmDetect.getBpm();

            DBG("  Raw detected BPM: " + juce::String(detectedBPM, 2));

            // Validation simple : range musicale raisonnable
            if (detectedBPM >= 30.0f && detectedBPM <= 300.0f)
            {
                DBG("BPM detection successful: " + juce::String(detectedBPM, 1));
                return detectedBPM;
            }
            else
            {
                DBG("BPM out of musical range, trying fallback...");

                // Essayer fallback par onset detection
                float fallbackBPM = detectBPMByOnsets(buffer, sampleRate);
                if (fallbackBPM >= 30.0f && fallbackBPM <= 300.0f)
                {
                    DBG("Fallback BPM detection: " + juce::String(fallbackBPM, 1));
                    return fallbackBPM;
                }

                DBG("Both detection methods failed");
                return 0.0f;
            }
        }
        catch (const std::exception &e)
        {
            DBG("BPM Detection exception: " + juce::String(e.what()));
            return 0.0f;
        }
    }

    // Fallback simple par détection d'onset (gardée pour les cas difficiles)
    static float detectBPMByOnsets(const juce::AudioBuffer<float> &buffer, double sampleRate)
    {
        if (buffer.getNumSamples() < sampleRate) // Moins d'1 seconde
            return 0.0f;

        try
        {
            DBG("Trying fallback onset-based BPM detection...");

            // Analyser les transitoires/onsets simples
            const int hopSize = 512;
            const int windowSize = 1024;
            std::vector<float> onsetStrength;

            for (int i = 0; i < buffer.getNumSamples() - windowSize; i += hopSize)
            {
                float energy = 0.0f;
                for (int j = 0; j < windowSize; ++j)
                {
                    float sample = buffer.getSample(0, i + j);
                    if (buffer.getNumChannels() > 1)
                    {
                        sample = (sample + buffer.getSample(1, i + j)) * 0.5f;
                    }
                    energy += sample * sample;
                }
                onsetStrength.push_back(std::sqrt(energy / windowSize));
            }

            // Détection simple de pics
            std::vector<int> onsets;
            float threshold = 0.1f;

            for (int i = 1; i < onsetStrength.size() - 1; ++i)
            {
                if (onsetStrength[i] > threshold &&
                    onsetStrength[i] > onsetStrength[i - 1] &&
                    onsetStrength[i] > onsetStrength[i + 1])
                {
                    onsets.push_back(i);
                }
            }

            if (onsets.size() < 4)
            {
                DBG("Not enough onsets detected: " + juce::String(onsets.size()));
                return 0.0f;
            }

            // Calculer BPM moyen entre onsets
            std::vector<float> intervals;
            for (int i = 1; i < onsets.size(); ++i)
            {
                float intervalSeconds = (onsets[i] - onsets[i - 1]) * hopSize / (float)sampleRate;
                if (intervalSeconds > 0.2f && intervalSeconds < 2.0f) // Entre 30 et 300 BPM
                {
                    intervals.push_back(60.0f / intervalSeconds);
                }
            }

            if (intervals.empty())
            {
                DBG("No valid intervals found");
                return 0.0f;
            }

            // BPM médian pour éviter les outliers
            std::sort(intervals.begin(), intervals.end());
            float medianBPM = intervals[intervals.size() / 2];

            DBG("Onset analysis: " + juce::String(onsets.size()) + " onsets, " +
                juce::String(intervals.size()) + " intervals, median BPM: " + juce::String(medianBPM, 1));

            // Validation simple
            if (medianBPM >= 30.0f && medianBPM <= 300.0f)
            {
                return medianBPM;
            }

            return 0.0f;
        }
        catch (const std::exception &e)
        {
            DBG("Fallback BPM detection failed: " + juce::String(e.what()));
            return 0.0f;
        }
    }

    static void timeStretchBuffer(juce::AudioBuffer<float> &buffer,
                                  double ratio, double sampleRate)
    {
        if (ratio == 1.0 || buffer.getNumSamples() == 0)
            return;

        try
        {
            soundtouch::SoundTouch soundTouch;
            soundTouch.setSampleRate((int)sampleRate);
            soundTouch.setChannels(buffer.getNumChannels());
            soundTouch.setTempo(ratio); // ratio > 1 = plus rapide

            // Input samples
            if (buffer.getNumChannels() == 1)
            {
                soundTouch.putSamples(buffer.getReadPointer(0), buffer.getNumSamples());
            }
            else
            {
                // Interleave stereo
                std::vector<float> interleavedInput;
                interleavedInput.reserve(buffer.getNumSamples() * 2);

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    interleavedInput.push_back(buffer.getSample(0, i));
                    interleavedInput.push_back(buffer.getSample(1, i));
                }

                soundTouch.putSamples(interleavedInput.data(), buffer.getNumSamples());
            }

            soundTouch.flush();

            // Get processed samples
            int outputSamples = soundTouch.numSamples();
            if (outputSamples > 0)
            {
                buffer.setSize(buffer.getNumChannels(), outputSamples, false, false, true);

                if (buffer.getNumChannels() == 1)
                {
                    soundTouch.receiveSamples(buffer.getWritePointer(0), outputSamples);
                }
                else
                {
                    std::vector<float> interleavedOutput(outputSamples * 2);
                    soundTouch.receiveSamples(interleavedOutput.data(), outputSamples);

                    // De-interleave
                    for (int i = 0; i < outputSamples; ++i)
                    {
                        buffer.setSample(0, i, interleavedOutput[i * 2]);
                        buffer.setSample(1, i, interleavedOutput[i * 2 + 1]);
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            DBG("Time stretch error: " + juce::String(e.what()));
        }
    }
};