#pragma once
#include "JuceHeader.h"

class SimpleEQ
{
public:
    SimpleEQ() = default;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        this->sampleRate = sampleRate;

        // Initialiser les filtres pour stéréo
        for (int ch = 0; ch < 2; ++ch)
        {
            // Filtre High Shelf (aigus) - 8kHz
            highFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeHighShelf(sampleRate, 8000.0, 0.7, 1.0));

            // Filtre Peak (médiums) - 1kHz
            midFilters[ch].setCoefficients(
                juce::IIRCoefficients::makePeakFilter(sampleRate, 1000.0, 1.0, 1.0));

            // Filtre Low Shelf (graves) - 200Hz
            lowFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeLowShelf(sampleRate, 200.0, 0.7, 1.0));
        }
    }

    void processBlock(juce::AudioBuffer<float> &buffer)
    {
        if (bypass)
            return;

        for (int ch = 0; ch < std::min(2, buffer.getNumChannels()); ++ch)
        {
            auto *channelData = buffer.getWritePointer(ch);

            // Appliquer les filtres en série
            lowFilters[ch].processSamples(channelData, buffer.getNumSamples());
            midFilters[ch].processSamples(channelData, buffer.getNumSamples());
            highFilters[ch].processSamples(channelData, buffer.getNumSamples());
        }
    }

    // Setters pour les gains (en dB)
    void setHighGain(float gainDb)
    {
        if (std::abs(gainDb - highGain) < 0.1f)
            return; // Éviter les recalculs inutiles

        highGain = gainDb;
        float linearGain = juce::Decibels::decibelsToGain(gainDb);

        for (int ch = 0; ch < 2; ++ch)
        {
            highFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeHighShelf(sampleRate, 8000.0, 0.7, linearGain));
        }
    }

    void setMidGain(float gainDb)
    {
        if (std::abs(gainDb - midGain) < 0.1f)
            return;

        midGain = gainDb;
        float linearGain = juce::Decibels::decibelsToGain(gainDb);

        for (int ch = 0; ch < 2; ++ch)
        {
            midFilters[ch].setCoefficients(
                juce::IIRCoefficients::makePeakFilter(sampleRate, 1000.0, 1.0, linearGain));
        }
    }

    void setLowGain(float gainDb)
    {
        if (std::abs(gainDb - lowGain) < 0.1f)
            return;

        lowGain = gainDb;
        float linearGain = juce::Decibels::decibelsToGain(gainDb);

        for (int ch = 0; ch < 2; ++ch)
        {
            lowFilters[ch].setCoefficients(
                juce::IIRCoefficients::makeLowShelf(sampleRate, 200.0, 0.7, linearGain));
        }
    }

    // Getters
    float getHighGain() const { return highGain; }
    float getMidGain() const { return midGain; }
    float getLowGain() const { return lowGain; }

    void setBypass(bool shouldBypass) { bypass = shouldBypass; }
    bool isBypassed() const { return bypass; }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            highFilters[ch].reset();
            midFilters[ch].reset();
            lowFilters[ch].reset();
        }
    }

private:
    double sampleRate = 44100.0;

    // Gains en dB
    float highGain = 0.0f; // Aigus
    float midGain = 0.0f;  // Médiums
    float lowGain = 0.0f;  // Graves

    bool bypass = false;

    // Filtres pour stéréo (L/R)
    juce::IIRFilter highFilters[2]; // High shelf
    juce::IIRFilter midFilters[2];  // Peak filter
    juce::IIRFilter lowFilters[2];  // Low shelf
};