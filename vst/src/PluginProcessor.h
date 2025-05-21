#pragma once
#include "./JuceHeader.h"
#include "DjIaClient.h"

class DjIaVstProcessor : public juce::AudioProcessor
{
public:
    DjIaVstProcessor();
    ~DjIaVstProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // Méthodes publiques pour interagir avec le plugin
    void generateLoop(const DjIaClient::LoopRequest &request);
    void setApiKey(const juce::String &key);
    void setServerUrl(const juce::String &url);
    void startPlayback();
    void stopPlayback();

    // Méthode de logging
    static void writeToLog(const juce::String &message)
    {
        auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("dj_ia_vst.log");
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        file.appendText(time + ": " + message + "\n");
    }

private:
    // État du plugin
    double currentBPM = 126.0;
    juce::String currentKey = "C minor";
    juce::String currentStyle = "techno";
    double hostBPM = 120.0;

    // Système de buffers ping-pong pour éviter les clicks pendant le chargement
    juce::AudioSampleBuffer pingBuffer;
    juce::AudioSampleBuffer pongBuffer;
    juce::AudioSampleBuffer *activeBuffer;  // Buffer en cours de lecture
    juce::AudioSampleBuffer *loadingBuffer; // Buffer en cours de chargement
    bool isBufferSwitchPending = false;
    juce::CriticalSection bufferLock; // Protection des opérations sur les buffers

    // Variables pour la lecture audio
    bool hasLoop = false;
    bool isNotePlaying = false;
    int currentNoteNumber = -1;
    double readPosition = 0.0;
    float masterVolume = 0.8f;

    // Variables pour le resampling et l'interpolation
    double sampleRate = 44100.0;      // Fréquence d'échantillonnage de la DAW
    double audioSampleRate = 44100.0; // Fréquence d'échantillonnage du fichier audio
    double playbackSpeedRatio = 1.0;  // Ratio pour l'ajustement de vitesse

    // Variables pour le filtre anti-aliasing avancé
    float prevInputL[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevInputR[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevOutputL[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevOutputR[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Client API et configuration
    DjIaClient apiClient;
    juce::CriticalSection apiLock;
    juce::String serverUrl = "http://localhost:8000";
    juce::String apiKey;

    // Variables pour le traitement en arrière-plan
    juce::MemoryBlock pendingAudioData;
    bool hasPendingAudioData = false;

    // Méthodes privées pour le traitement audio
    void processIncomingAudio();
    void applyFilter(float &input, float &output, float *prevInputs, float *prevOutputs, float filterCoeff);
    float interpolateCubic(const float *buffer, double position, int bufferSize);
    void resetFilters();
    void crossfadeBuffers(juce::AudioSampleBuffer &newBuffer, const juce::AudioSampleBuffer &oldBuffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};