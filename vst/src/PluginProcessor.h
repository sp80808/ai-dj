#pragma once
#include "./JuceHeader.h"
#include "DjIaClient.h"

class DjIaVstProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    // CONSTRUCTEUR ET INTERFACE STANDARD
    //==============================================================================
    DjIaVstProcessor();
    ~DjIaVstProcessor() override;

    // Interface AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    // Interface Plugin
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }

    // Capacités MIDI
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programmes
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    // État
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    //==============================================================================
    // API PUBLIQUE DU PLUGIN
    //==============================================================================

    // Génération et contrôle
    void generateLoop(const DjIaClient::LoopRequest &request);
    void startPlayback();
    void stopPlayback();

    // Configuration
    void setApiKey(const juce::String &key);
    void setServerUrl(const juce::String &url);

    // Logging
    static void writeToLog(const juce::String &message)
    {
        auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("dj_ia_vst.log");
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        file.appendText(time + ": " + message + "\n");
    }

private:
    //==============================================================================
    // ÉTAT DU PLUGIN
    //==============================================================================

    // État de lecture
    std::atomic<bool> hasLoop{false};
    std::atomic<bool> isNotePlaying{false};
    std::atomic<int> currentNoteNumber{-1};
    std::atomic<double> readPosition{0.0};
    std::atomic<float> masterVolume{0.8f};

    // Sample rates et timing
    double hostSampleRate = 0.0;
    double audioSampleRate = 0.0;
    double playbackSpeedRatio = 1.0;

    // État de configuration
    double currentBPM = 126.0;
    juce::String currentKey = "C minor";
    juce::String currentStyle = "techno";
    juce::String serverUrl = "http://localhost:8000";
    juce::String apiKey;

    //==============================================================================
    // SYSTÈME DE BUFFERS SIMPLIFIÉ
    //==============================================================================

    juce::AudioSampleBuffer audioBuffer;
    juce::CriticalSection bufferLock;

    // Variables pour éviter les allocations dans processBlock
    int bufferNumSamples = 0;
    int bufferNumChannels = 0;

    //==============================================================================
    // SYSTÈME D'API
    //==============================================================================

    DjIaClient apiClient;
    juce::CriticalSection apiLock;
    juce::MemoryBlock pendingAudioData;
    std::atomic<bool> hasPendingAudioData{false};

    //==============================================================================
    // MÉTHODES PRIVÉES - TRAITEMENT AUDIO OPTIMISÉ
    //==============================================================================

    // Traitement MIDI
    void processMidiMessages(juce::MidiBuffer &midiMessages);
    void startNotePlayback(int noteNumber);
    void stopNotePlayback();

    // Génération audio simplifiée
    void generateAudioOutput(juce::AudioBuffer<float> &buffer);

    // Interpolation linéaire rapide
    inline float interpolateLinear(const float *buffer, double position, int bufferSize);

    // Chargement audio
    void processIncomingAudio();
    void loadAudioFromReader(juce::AudioFormatReader &reader);
    void clearPendingAudio();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};