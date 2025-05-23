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
    bool hasLoop = false;
    bool isNotePlaying = false;
    int currentNoteNumber = -1;
    double readPosition = 0.0;
    float masterVolume = 0.8f;

    // Sample rates et timing
    double hostSampleRate = 0.0;     // Fréquence de la DAW
    double audioSampleRate = 0.0;    // Fréquence du fichier audio chargé
    double playbackSpeedRatio = 1.0; // Ratio de lecture calculé

    // État de configuration
    double currentBPM = 126.0;
    juce::String currentKey = "C minor";
    juce::String currentStyle = "techno";
    juce::String serverUrl = "http://localhost:8000";
    juce::String apiKey;

    //==============================================================================
    // SYSTÈME DE BUFFERS PING-PONG
    //==============================================================================

    juce::AudioSampleBuffer pingBuffer;     // Buffer A
    juce::AudioSampleBuffer pongBuffer;     // Buffer B
    juce::AudioSampleBuffer *activeBuffer;  // Buffer en cours de lecture
    juce::AudioSampleBuffer *loadingBuffer; // Buffer en cours de chargement
    bool isBufferSwitchPending = false;
    juce::CriticalSection bufferLock; // Protection des opérations sur les buffers

    //==============================================================================
    // FILTRAGE AUDIO
    //==============================================================================

    // Historique pour filtres anti-aliasing (par canal)
    float prevInputL[4] = {0.0f};
    float prevInputR[4] = {0.0f};
    float prevOutputL[4] = {0.0f};
    float prevOutputR[4] = {0.0f};

    //==============================================================================
    // SYSTÈME D'API
    //==============================================================================

    DjIaClient apiClient;
    juce::CriticalSection apiLock;
    juce::MemoryBlock pendingAudioData;
    bool hasPendingAudioData = false;

    //==============================================================================
    // MÉTHODES PRIVÉES - TRAITEMENT AUDIO
    //==============================================================================

    // Gestion des buffers
    void handleBufferSwitch();
    bool hasValidLoop();

    // Traitement MIDI
    void processMidiMessages(juce::MidiBuffer &midiMessages);
    void startNotePlayback(int noteNumber);
    void stopNotePlayback();

    // Génération audio
    void generateAudioOutput(juce::AudioBuffer<float> &buffer);
    void calculatePlaybackRatio();
    float calculateFilterCoeff();
    void processAudioChannel(juce::AudioBuffer<float> &buffer, int channel,
                             int numInChannels, int numSamplesToProcess,
                             int sourceBufferSize, float filterCoeff);
    void advanceReadPosition(int sourceBufferSize);

    // Interpolation et filtrage
    float interpolateCubicSafe(const float *buffer, double position, int bufferSize);
    void applyAntiAliasingFilter(float input, float &output,
                                 float *prevInputs, float *prevOutputs, float filterCoeff);
    void resetFilters();

    // Chargement audio
    void processIncomingAudio();
    void loadAudioFromReader(juce::AudioFormatReader &reader);
    void applyCrossfade(juce::AudioSampleBuffer &newBuffer, const juce::AudioSampleBuffer &oldBuffer);
    void clearPendingAudio();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};