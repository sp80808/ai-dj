#pragma once
#include "DjIaClient.h"

struct DummySound : public juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class DummyVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound *) override { return true; }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound *, int currentPitchWheelPosition) override
    {
        // Ne fait rien - juste pour que Bitwig détecte un Synthesiser
    }

    void stopNote(float velocity, bool allowTailOff) override
    {
        clearCurrentNote(); // Important pour libérer la voix
    }

    void pitchWheelMoved(int newPitchWheelValue) override {}
    void controllerMoved(int controllerNumber, int newControllerValue) override {}

    void renderNextBlock(juce::AudioBuffer<float> &outputBuffer,
                         int startSample, int numSamples) override
    {
        // Ne génère aucun audio - ton vrai sampler s'en occupe
    }
};

class DjIaVstProcessor : public juce::AudioProcessor,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    // CONSTRUCTEUR ET INTERFACE STANDARD
    //==============================================================================
    DjIaVstProcessor();
    ~DjIaVstProcessor() override;
    void parameterChanged(const juce::String &parameterID, float newValue) override;
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
    bool isMidiEffect() const override { return false; }
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

    double getHostBpm() const;

    // Getters pour l'UI
    juce::String getServerUrl() const { return serverUrl; }
    juce::String getApiKey() const { return apiKey; }

    juce::String getLastPrompt() const { return lastPrompt; }
    juce::String getLastStyle() const { return lastStyle; }
    juce::String getLastKey() const { return lastKey; }
    double getLastBpm() const { return lastBpm; }
    int getLastPresetIndex() const { return lastPresetIndex; }
    bool getHostBpmEnabled() const { return hostBpmEnabled; }
    void setLastPrompt(const juce::String &prompt) { lastPrompt = prompt; }
    void setLastStyle(const juce::String &style) { lastStyle = style; }
    void setLastKey(const juce::String &key) { lastKey = key; }
    void setLastBpm(double bpm) { lastBpm = bpm; }
    void setLastPresetIndex(int index) { lastPresetIndex = index; }
    void setHostBpmEnabled(bool enabled) { hostBpmEnabled = enabled; }

    void setAutoLoadEnabled(bool enabled);
    bool getAutoLoadEnabled() const { return autoLoadEnabled.load(); }
    void loadPendingSample(); // Pour charger manuellement
    bool hasSampleWaiting() const { return hasUnloadedSample.load(); }
    void loadAudioData();
    void setMidiIndicatorCallback(std::function<void(const juce::String &)> callback)
    {
        midiIndicatorCallback = callback;
    }
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void startNotePlayback(int noteNumber);
    void stopNotePlayback();

    juce::AudioProcessorValueTreeState &getParameters() { return parameters; }
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

    juce::String lastPrompt = "";
    juce::String lastStyle = "Techno";
    juce::String lastKey = "C minor";
    double lastBpm = 126.0;
    int lastPresetIndex = -1; // -1 = Custom
    bool hostBpmEnabled = false;

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

    // Génération audio simplifiée
    void generateAudioOutput(juce::AudioBuffer<float> &buffer);

    // Interpolation linéaire rapide
    inline float interpolateLinear(const float *buffer, double position, int bufferSize);

    // Chargement audio
    void processIncomingAudio();
    void loadAudioFromReader(juce::AudioFormatReader &reader);
    void clearPendingAudio();

    std::function<void(const juce::String &)> midiIndicatorCallback;

    juce::Synthesiser synth;

    std::atomic<bool> autoLoadEnabled{true};
    std::atomic<bool> hasUnloadedSample{false}; // Sample reçu mais pas encore chargé

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> *generateParam = nullptr;
    std::atomic<float> *playParam = nullptr;
    std::atomic<float> *autoLoadParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};