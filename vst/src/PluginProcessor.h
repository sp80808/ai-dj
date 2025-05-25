#pragma once
#include "JuceHeader.h"
#include "DjIaClient.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>

//==============================================================================
// TRACK DATA STRUCTURE
//==============================================================================
struct TrackData
{
    // Identifiant unique
    juce::String trackId;
    juce::String trackName;
    std::atomic<bool> isPlaying{false};

    int midiNote = 60;

    // Audio data
    juce::AudioSampleBuffer audioBuffer;
    double sampleRate = 44100.0;
    int numSamples = 0;

    // État de lecture
    std::atomic<bool> isEnabled{true};
    std::atomic<bool> isSolo{false};
    std::atomic<bool> isMuted{false};
    std::atomic<float> volume{0.8f};
    std::atomic<float> pan{0.0f}; // -1.0 = left, 1.0 = right

    // Métadonnées génération
    juce::String prompt;
    juce::String style;
    juce::String stems;
    float bpm = 126.0f;

    // Position de lecture
    std::atomic<double> readPosition{0.0};

    TrackData() : trackId(juce::Uuid().toString())
    {
        volume = 0.8f;      // ← Valeur sûre
        pan = 0.0f;         // ← Valeur sûre
        readPosition = 0.0; // ← Valeur sûre
    }

    void reset()
    {
        audioBuffer.setSize(0, 0);
        numSamples = 0;
        readPosition = 0.0;
        isEnabled = true;
        isMuted = false;
        isSolo = false;
        volume = 0.8f;
        pan = 0.0f;
    }
};

//==============================================================================
// TRACK MANAGER
//==============================================================================
class TrackManager
{
public:
    TrackManager() = default;

    // Gestion des pistes
    juce::String createTrack(const juce::String &name = "Track")
    {
        juce::ScopedLock lock(tracksLock);

        auto track = std::make_unique<TrackData>();
        track->trackName = name + " " + juce::String(tracks.size() + 1);

        juce::String trackId = track->trackId;
        std::string stdId = trackId.toStdString();

        tracks[stdId] = std::move(track);
        trackOrder.push_back(stdId); // ← AJOUTER à l'ordre

        return trackId;
    }

    void removeTrack(const juce::String &trackId)
    {
        juce::ScopedLock lock(tracksLock);
        std::string stdId = trackId.toStdString();

        tracks.erase(stdId);
        trackOrder.erase(std::remove(trackOrder.begin(), trackOrder.end(), stdId), trackOrder.end());
    }

    void reorderTracks(const juce::String &fromTrackId, const juce::String &toTrackId)
    {
        juce::ScopedLock lock(tracksLock);

        std::string fromStdId = fromTrackId.toStdString();
        std::string toStdId = toTrackId.toStdString();

        // Trouver les positions dans trackOrder
        auto fromIt = std::find(trackOrder.begin(), trackOrder.end(), fromStdId);
        auto toIt = std::find(trackOrder.begin(), trackOrder.end(), toStdId);

        if (fromIt == trackOrder.end() || toIt == trackOrder.end())
            return;

        // Déplacer dans le vecteur d'ordre
        std::string movedId = *fromIt;
        trackOrder.erase(fromIt);

        // Recalculer toIt après erase
        toIt = std::find(trackOrder.begin(), trackOrder.end(), toStdId);
        trackOrder.insert(toIt, movedId);
    }

    TrackData *getTrack(const juce::String &trackId)
    {
        juce::ScopedLock lock(tracksLock);
        auto it = tracks.find(trackId.toStdString());
        return (it != tracks.end()) ? it->second.get() : nullptr;
    }

    std::vector<juce::String> getAllTrackIds() const
    {
        juce::ScopedLock lock(tracksLock);
        std::vector<juce::String> ids;
        for (const auto &stdId : trackOrder)
        { // ← Utiliser l'ordre !
            if (tracks.count(stdId))
            {
                ids.push_back(juce::String(stdId));
            }
        }
        return ids;
    }

    // Audio processing pour toutes les pistes
    void renderAllTracks(juce::AudioBuffer<float> &outputBuffer,
                         std::vector<juce::AudioBuffer<float>> &individualOutputs,
                         double hostSampleRate)
    {

        const int numSamples = outputBuffer.getNumSamples();
        bool anyTrackSolo = false;

        // Vérifier s'il y a des pistes en solo
        {
            juce::ScopedLock lock(tracksLock);
            for (const auto &pair : tracks)
            {
                if (pair.second->isSolo.load())
                {
                    anyTrackSolo = true;
                    break;
                }
            }
        }

        // Clear tous les buffers
        outputBuffer.clear();
        for (auto &buffer : individualOutputs)
        {
            buffer.clear();
        }

        juce::ScopedLock lock(tracksLock);

        int trackIndex = 0;
        for (const auto &pair : tracks)
        {
            if (trackIndex >= individualOutputs.size())
                break;

            auto *track = pair.second.get();

            // Skip si muted ou si solo mode et pas solo
            if (track->isMuted.load() ||
                (anyTrackSolo && !track->isSolo.load()) ||
                !track->isEnabled.load() ||
                track->numSamples == 0)
            {
                trackIndex++;
                continue;
            }

            // Render cette piste
            renderSingleTrack(*track, outputBuffer, individualOutputs[trackIndex],
                              numSamples, hostSampleRate);

            trackIndex++;
        }
    }

    // Sauvegarder/Charger état
    juce::ValueTree saveState() const
    {
        juce::ValueTree state("TrackManager");

        juce::ScopedLock lock(tracksLock);
        for (const auto &pair : tracks)
        {
            auto trackState = juce::ValueTree("Track");
            auto *track = pair.second.get();

            trackState.setProperty("id", track->trackId, nullptr);
            trackState.setProperty("name", track->trackName, nullptr);
            trackState.setProperty("prompt", track->prompt, nullptr);
            trackState.setProperty("style", track->style, nullptr);
            trackState.setProperty("stems", track->stems, nullptr);
            trackState.setProperty("bpm", track->bpm, nullptr);
            trackState.setProperty("volume", track->volume.load(), nullptr);
            trackState.setProperty("pan", track->pan.load(), nullptr);
            trackState.setProperty("muted", track->isMuted.load(), nullptr);
            trackState.setProperty("solo", track->isSolo.load(), nullptr);
            trackState.setProperty("enabled", track->isEnabled.load(), nullptr);

            // Sauvegarder audio data si présent
            if (track->numSamples > 0)
            {
                juce::MemoryBlock audioData;
                juce::MemoryOutputStream stream(audioData, false);

                // Format simple: sampleRate + numChannels + numSamples + data
                stream.writeDouble(track->sampleRate);
                stream.writeInt(track->audioBuffer.getNumChannels());
                stream.writeInt(track->numSamples);

                for (int ch = 0; ch < track->audioBuffer.getNumChannels(); ++ch)
                {
                    stream.write(track->audioBuffer.getReadPointer(ch),
                                 track->numSamples * sizeof(float));
                }

                trackState.setProperty("audioData", audioData.toBase64Encoding(), nullptr);
            }

            state.appendChild(trackState, nullptr);
        }

        return state;
    }

    void loadState(const juce::ValueTree &state)
    {
        juce::ScopedLock lock(tracksLock);
        tracks.clear();

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto trackState = state.getChild(i);
            if (!trackState.hasType("Track"))
                continue;

            auto track = std::make_unique<TrackData>();

            track->trackId = trackState.getProperty("id", juce::Uuid().toString());
            track->trackName = trackState.getProperty("name", "Track");
            track->prompt = trackState.getProperty("prompt", "");
            track->style = trackState.getProperty("style", "");
            track->stems = trackState.getProperty("stems", "");
            track->bpm = trackState.getProperty("bpm", 126.0f);
            track->volume = trackState.getProperty("volume", 0.8f);
            track->pan = trackState.getProperty("pan", 0.0f);
            track->isMuted = trackState.getProperty("muted", false);
            track->isSolo = trackState.getProperty("solo", false);
            track->isEnabled = trackState.getProperty("enabled", true);

            // Charger audio data si présent
            juce::String audioDataBase64 = trackState.getProperty("audioData", "");
            if (audioDataBase64.isNotEmpty())
            {
                juce::MemoryBlock audioData;
                if (audioData.fromBase64Encoding(audioDataBase64))
                {

                    juce::MemoryInputStream stream(audioData, false);
                    track->sampleRate = stream.readDouble();
                    int numChannels = stream.readInt();
                    track->numSamples = stream.readInt();

                    if (track->numSamples > 0 && numChannels > 0)
                    {
                        track->audioBuffer.setSize(numChannels, track->numSamples);

                        for (int ch = 0; ch < numChannels; ++ch)
                        {
                            stream.read(track->audioBuffer.getWritePointer(ch),
                                        track->numSamples * sizeof(float));
                        }
                    }
                }
            }

            tracks[track->trackId.toStdString()] = std::move(track);
        }
    }

private:
    mutable juce::CriticalSection tracksLock;
    std::unordered_map<std::string, std::unique_ptr<TrackData>> tracks;
    std::vector<std::string> trackOrder;

    void renderSingleTrack(TrackData &track,
                           juce::AudioBuffer<float> &mixOutput,
                           juce::AudioBuffer<float> &individualOutput,
                           int numSamples, double hostSampleRate)
    {

        if (track.numSamples == 0 || !track.isPlaying.load())
            return;

        const float volume = juce::jlimit(0.0f, 1.0f, track.volume.load());
        double currentPosition = track.readPosition.load();

        // RATIO SIMPLE - pas de division par hostSampleRate !
        double playbackRatio = 1.0; // ← Lecture normale sans stretch !

        // Process audio simple
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < std::min(2, track.audioBuffer.getNumChannels()); ++ch)
            {
                // INTERPOLATION SIMPLE comme avant
                int index = static_cast<int>(currentPosition) % track.numSamples;
                float sample = track.audioBuffer.getSample(ch, index) * volume;

                // Mixage simple
                mixOutput.addSample(ch, i, sample);
                individualOutput.setSample(ch, i, sample); // ← SET pas ADD !
            }

            // Avancer position
            currentPosition += playbackRatio;
            if (currentPosition >= track.numSamples)
            {
                currentPosition = 0.0; // Reset au début
            }
        }

        track.readPosition = currentPosition;
    }

    float interpolateLinear(const float *buffer, double position, int bufferSize)
    {
        if (bufferSize <= 0)
            return 0.0f;

        int index = static_cast<int>(position) % bufferSize;
        float fraction = static_cast<float>(position - std::floor(position));

        int nextIndex = (index + 1) % bufferSize;

        return buffer[index] + fraction * (buffer[nextIndex] - buffer[index]);
    }
};

//==============================================================================
// DUMMY SYNTHESISER (pour compatibilité MIDI)
//==============================================================================
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

//==============================================================================
// PLUGIN PROCESSOR PRINCIPAL
//==============================================================================
class DjIaVstProcessor : public juce::AudioProcessor,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    // CONSTRUCTEUR ET INTERFACE STANDARD
    //==============================================================================
    DjIaVstProcessor();
    ~DjIaVstProcessor() override;

    // Listener pour les paramètres
    void parameterChanged(const juce::String &parameterID, float newValue) override;

    // Interface AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

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
    // API MULTI-TRACK
    //==============================================================================

    // Track Management
    juce::String createNewTrack(const juce::String &name = "Track");
    void deleteTrack(const juce::String &trackId);
    void selectTrack(const juce::String &trackId);
    juce::String getSelectedTrackId() const { return selectedTrackId; }
    void reorderTracks(const juce::String &fromTrackId, const juce::String &toTrackId);
    std::vector<juce::String> getAllTrackIds() const { return trackManager.getAllTrackIds(); }
    TrackData *getCurrentTrack() { return trackManager.getTrack(selectedTrackId); }
    TrackData *getTrack(const juce::String &trackId) { return trackManager.getTrack(trackId); }

    // Génération et contrôle
    void generateLoop(const DjIaClient::LoopRequest &request, const juce::String &targetTrackId = "");
    void startPlayback();
    void stopPlayback();
    void startNotePlayback(int noteNumber);
    void stopNotePlayback();
    void stopNotePlaybackForTrack(int noteNumber);
    void startNotePlaybackForTrack(const juce::String &trackId, int noteNumber);

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

    // Setters pour l'UI
    void setLastPrompt(const juce::String &prompt) { lastPrompt = prompt; }
    void setLastStyle(const juce::String &style) { lastStyle = style; }
    void setLastKey(const juce::String &key) { lastKey = key; }
    void setLastBpm(double bpm) { lastBpm = bpm; }
    void setLastPresetIndex(int index) { lastPresetIndex = index; }
    void setHostBpmEnabled(bool enabled) { hostBpmEnabled = enabled; }

    // Audio loading
    void setAutoLoadEnabled(bool enabled);
    bool getAutoLoadEnabled() const { return autoLoadEnabled.load(); }
    void loadPendingSample();
    bool hasSampleWaiting() const { return hasUnloadedSample.load(); }

    // MIDI callback
    void setMidiIndicatorCallback(std::function<void(const juce::String &)> callback)
    {
        midiIndicatorCallback = callback;
    }

    // Parameters access
    juce::AudioProcessorValueTreeState &getParameters() { return parameters; }

    // Logging
    static void writeToLog(const juce::String &message);

private:
    //==============================================================================
    // CONFIGURATION DU BUS LAYOUT MULTI-OUTPUT
    //==============================================================================
    static juce::AudioProcessor::BusesProperties createBusLayout();
    static const int MAX_TRACKS = 8;

    std::unordered_map<int, juce::String> playingTracks;
    //==============================================================================
    // ÉTAT DU PLUGIN
    //==============================================================================

    // Multi-track system
    TrackManager trackManager;
    juce::String selectedTrackId;
    std::vector<juce::AudioBuffer<float>> individualOutputBuffers;

    // État de lecture globale
    std::atomic<bool> isNotePlaying{false};
    std::atomic<int> currentNoteNumber{-1};

    // UI state
    juce::String lastPrompt = "";
    juce::String lastStyle = "Techno";
    juce::String lastKey = "C minor";
    double lastBpm = 126.0;
    int lastPresetIndex = -1;
    bool hostBpmEnabled = false;

    // Sample rates
    double hostSampleRate = 0.0;
    double audioSampleRate = 0.0;

    // Configuration
    juce::String serverUrl = "http://localhost:8000";
    juce::String apiKey;

    //==============================================================================
    // SYSTÈME D'API
    //==============================================================================
    DjIaClient apiClient;
    juce::CriticalSection apiLock;
    juce::MemoryBlock pendingAudioData;
    juce::String pendingTrackId;
    std::atomic<bool> hasPendingAudioData{false};
    std::atomic<bool> autoLoadEnabled{true};
    std::atomic<bool> hasUnloadedSample{false};

    //==============================================================================
    // MIDI ET SYNTHESISER
    //==============================================================================
    juce::Synthesiser synth;
    std::function<void(const juce::String &)> midiIndicatorCallback;

    //==============================================================================
    // PARAMÈTRES AUTOMATISABLES
    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> *generateParam = nullptr;
    std::atomic<float> *playParam = nullptr;
    std::atomic<float> *autoLoadParam = nullptr;

    //==============================================================================
    // MÉTHODES PRIVÉES
    //==============================================================================

    // Traitement MIDI
    void processMidiMessages(juce::MidiBuffer &midiMessages);

    // Chargement audio
    void processIncomingAudio();
    void loadAudioDataToTrack(const juce::String &trackId);
    void clearPendingAudio();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};