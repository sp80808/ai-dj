#pragma once
#include "JuceHeader.h"
#include "DjIaClient.h"
#include "SimpleEQ.h"
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
    std::atomic<bool> isArmed{false};
    float fineOffset = 0.0f;
    std::atomic<double> cachedPlaybackRatio{1.0};
    juce::AudioSampleBuffer stagingBuffer;
    std::atomic<bool> hasStagingData{false};
    std::atomic<bool> swapRequested{false};

    // Métadonnées staging
    std::atomic<int> stagingNumSamples{0};
    std::atomic<double> stagingSampleRate{48000.0};
    float stagingOriginalBpm = 126.0f;

    double loopStart = 0.0;
    double loopEnd = 4.0;
    float originalBpm = 126.0f;
    int timeStretchMode = 4;
    double timeStretchRatio = 1.0;

    double bpmOffset = -12.0;

    int midiNote = 60;

    // Audio data
    juce::AudioSampleBuffer audioBuffer;
    double sampleRate = 48000.0;
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
        volume = 0.8f;
        pan = 0.0f;
        readPosition = 0.0;
        bpmOffset = -12.0;
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
        bpmOffset = 0.0; // Reset aussi l'offset
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
        track->bpmOffset = -12.0;
        track->midiNote = 60 + tracks.size();
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
    void renderAllTracks(juce::AudioBuffer<float> &outputBuffer,
                         std::vector<juce::AudioBuffer<float>> &individualOutputs,
                         double hostSampleRate)
    {
        const int numSamples = outputBuffer.getNumSamples();
        bool anyTrackSolo = false;

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

            if (track->isEnabled.load() && track->numSamples > 0)
            {
                juce::AudioBuffer<float> tempMixBuffer(outputBuffer.getNumChannels(), numSamples);
                juce::AudioBuffer<float> tempIndividualBuffer(2, numSamples);
                tempMixBuffer.clear();
                tempIndividualBuffer.clear();

                renderSingleTrack(*track, tempMixBuffer, tempIndividualBuffer,
                                  numSamples, hostSampleRate);

                bool shouldHearTrack = !track->isMuted.load() &&
                                       (!anyTrackSolo || track->isSolo.load());

                if (shouldHearTrack)
                {
                    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                    {
                        outputBuffer.addFrom(ch, 0, tempMixBuffer, ch, 0, numSamples);
                    }
                }

                for (int ch = 0; ch < std::min(2, individualOutputs[trackIndex].getNumChannels()); ++ch)
                {
                    individualOutputs[trackIndex].copyFrom(ch, 0, tempIndividualBuffer, ch, 0, numSamples);

                    if (!shouldHearTrack)
                    {
                        individualOutputs[trackIndex].applyGain(ch, 0, numSamples, 0.0f);
                    }
                }
            }

            trackIndex++;
        }
    }

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
            trackState.setProperty("originalBpm", track->originalBpm, nullptr);
            trackState.setProperty("timeStretchMode", track->timeStretchMode, nullptr);
            trackState.setProperty("bpmOffset", track->bpmOffset, nullptr); // NOUVEAU
            trackState.setProperty("midiNote", track->midiNote, nullptr);
            trackState.setProperty("loopStart", track->loopStart, nullptr);
            trackState.setProperty("loopEnd", track->loopEnd, nullptr);
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
        trackOrder.clear();

        DBG("loadState starting - state has " + juce::String(state.getNumChildren()) + " children");

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto trackState = state.getChild(i);
            if (!trackState.hasType("Track"))
            {
                DBG("Skipping non-Track child: " + trackState.getType().toString());
                continue;
            }

            auto track = std::make_unique<TrackData>();

            track->trackId = trackState.getProperty("id", juce::Uuid().toString());
            track->trackName = trackState.getProperty("name", "Track");

            DBG("Loading track: " + track->trackId + " - " + track->trackName);

            track->prompt = trackState.getProperty("prompt", "");
            track->style = trackState.getProperty("style", "");
            track->stems = trackState.getProperty("stems", "");
            track->bpm = trackState.getProperty("bpm", 126.0f);
            track->originalBpm = trackState.getProperty("originalBpm", 126.0f);
            track->timeStretchMode = trackState.getProperty("timeStretchMode", 4);
            track->bpmOffset = trackState.getProperty("bpmOffset", -12.0); // NOUVEAU
            track->midiNote = trackState.getProperty("midiNote", 60);
            track->loopStart = trackState.getProperty("loopStart", 0.0);
            track->loopEnd = trackState.getProperty("loopEnd", 4.0);
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

            std::string stdId = track->trackId.toStdString();
            tracks[stdId] = std::move(track);
            trackOrder.push_back(stdId);

            DBG("Track added to maps - tracks size: " + juce::String(tracks.size()) +
                ", trackOrder size: " + juce::String(trackOrder.size()));
        }
        DBG("loadState complete - final tracks: " + juce::String(tracks.size()));
    }

    void writeToLog(const juce::String &message)
    {
        auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("dj_ia_vst_multitrack.log");

        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        file.appendText(time + ": " + message + "\n");
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

        static bool debugLogged = false;
        if (!debugLogged)
        {
            writeToLog("🎵 === TRACK PLAYING DEBUG ===");
            writeToLog("  Track name: " + track.trackName);
            writeToLog("  Original BPM: " + juce::String(track.originalBpm, 1));
            writeToLog("  Time stretch mode: " + juce::String(track.timeStretchMode));
            writeToLog("  Cached playback ratio: " + juce::String(track.cachedPlaybackRatio.load(), 3));
            debugLogged = true;
        }

        const float volume = juce::jlimit(0.0f, 1.0f, track.volume.load());
        const float pan = juce::jlimit(-1.0f, 1.0f, track.pan.load());
        double currentPosition = track.readPosition.load();

        // CALCUL DU RATIO selon le mode time-stretch
        double playbackRatio = 1.0;

        static bool fundamentalDebug = false;
        if (!fundamentalDebug && track.numSamples > 0)
        {
            writeToLog("🔍 === FUNDAMENTAL DEBUG ===");
            writeToLog("  Track sample rate: " + juce::String(track.sampleRate));
            writeToLog("  Host sample rate: " + juce::String(hostSampleRate));
            writeToLog("  Track numSamples: " + juce::String(track.numSamples));
            writeToLog("  Track duration (calculated): " + juce::String(track.numSamples / track.sampleRate, 3) + "s");
            writeToLog("  Original BPM: " + juce::String(track.originalBpm, 1));
            writeToLog("  Time stretch mode: " + juce::String(track.timeStretchMode));
            writeToLog("  playbackRatio: " + juce::String(playbackRatio, 3));

            double samplesPerSecondWithRatio = track.sampleRate * playbackRatio;
            writeToLog("  Samples/sec with ratio: " + juce::String(samplesPerSecondWithRatio));
            writeToLog("  Expected playback rate: " + juce::String(samplesPerSecondWithRatio / hostSampleRate, 3) + "x");

            fundamentalDebug = true;
        }

        switch (track.timeStretchMode)
        {
        case 1: // Off - garder ratio = 1.0
            playbackRatio = 1.0;
            break;

        case 2: // Manual BPM (slider)
            if (track.originalBpm > 0.0f)
            {
                float adjustedBpm = track.originalBpm + track.bpmOffset + (track.fineOffset / 100.0f);
                playbackRatio = adjustedBpm / track.originalBpm;
            }
            break;

        case 3: // Host BPM (DAW) - DEFAULT
            playbackRatio = 1.0;
            break;

        case 4: // Both (Host + Manual offset)
            if (track.originalBpm > 0.0f)
            {
                float manualAdjust = track.bpmOffset + (track.fineOffset / 100.0f);
                playbackRatio = (track.originalBpm + manualAdjust) / track.originalBpm;
            }
            break;
        }

        // Convertir les temps en samples pour ONE-SHOT
        double startSample = track.loopStart * track.sampleRate;
        double endSample = track.loopEnd * track.sampleRate;

        // Validation des bornes
        startSample = juce::jlimit(0.0, (double)track.numSamples - 1, startSample);
        endSample = juce::jlimit(startSample + 1, (double)track.numSamples, endSample);

        double sectionLength = endSample - startSample;

        // Si pas de section définie ou invalide, utiliser tout le sample
        if (sectionLength < 100) // Minimum 100 samples (~2ms à 44kHz)
        {
            startSample = 0.0;
            endSample = track.numSamples;
            sectionLength = track.numSamples;
        }

        // Process audio en ONE-SHOT (lecture linéaire)
        for (int i = 0; i < numSamples; ++i)
        {
            double absolutePosition = startSample + currentPosition;
            float leftGain = 1.0f;
            float rightGain = 1.0f;

            if (pan < 0.0f)
            {
                // Pan vers la gauche : réduire le canal droit
                rightGain = 1.0f + pan; // pan est négatif, donc rightGain diminue
            }
            else if (pan > 0.0f)
            {
                // Pan vers la droite : réduire le canal gauche
                leftGain = 1.0f - pan;
            }

            // ARRÊTER si on dépasse la fin de la section
            if (absolutePosition >= endSample)
            {
                // ARRÊTER LA LECTURE
                track.isPlaying = false;
                break; // Sortir de la boucle, plus d'audio à générer
            }

            // Sécurité: s'assurer qu'on ne dépasse pas les bornes du buffer
            if (absolutePosition >= track.numSamples)
            {
                track.isPlaying = false;
                break;
            }

            for (int ch = 0; ch < std::min(2, track.audioBuffer.getNumChannels()); ++ch)
            {
                // Interpolation linéaire pour une lecture smooth
                int sampleIndex = static_cast<int>(absolutePosition);
                if (sampleIndex >= track.audioBuffer.getNumSamples())
                {
                    track.isPlaying = false;
                    break;
                }

                float sample = track.audioBuffer.getSample(ch, sampleIndex);
                sample *= volume;
                if (ch == 0)
                {
                    sample *= leftGain;
                }
                else
                {
                    sample *= rightGain;
                }

                // Mixage
                mixOutput.addSample(ch, i, sample);
                individualOutput.setSample(ch, i, sample);

                // Mixage
                mixOutput.addSample(ch, i, sample);
                individualOutput.setSample(ch, i, sample);
            }

            // Avancer position AVEC time-stretch
            currentPosition += playbackRatio;
        }

        // Sauvegarder la position (même si arrêtée)
        track.readPosition = currentPosition;
    }

    float interpolateLinear(const float *buffer, double position, int bufferSize)
    {
        int index = static_cast<int>(position);
        if (index >= bufferSize - 1)
            return buffer[bufferSize - 1];

        float fraction = static_cast<float>(position - index);
        return buffer[index] + fraction * (buffer[index + 1] - buffer[index]);
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
                         public juce::AudioProcessorValueTreeState::Listener,
                         public juce::Timer
{
public:
    void timerCallback() override;
    std::function<void()> onUIUpdateNeeded;
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

    void setMasterVolume(float volume) { masterVolume = volume; }
    void setMasterPan(float pan) { masterPan = pan; }
    void setMasterEQ(float high, float mid, float low)
    {
        masterHighEQ = high;
        masterMidEQ = mid;
        masterLowEQ = low;
    }

    // Getters pour l'UI
    float getMasterVolume() const { return masterVolume.load(); }
    float getMasterPan() const { return masterPan.load(); }

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
    void startNotePlaybackForTrack(const juce::String &trackId, int noteNumber, double hostBpm = 126.0);
    void stopNotePlaybackForTrack(int noteNumber);

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
    std::atomic<bool> needsUIUpdate{false};
    std::atomic<double> cachedHostBpm{126.0};
    static juce::AudioProcessor::BusesProperties createBusLayout();
    static const int MAX_TRACKS = 8;
    void updateMasterEQ();

    void loadAudioDataAsync(const juce::String &trackId, const juce::MemoryBlock &audioData);
    void checkAndSwapStagingBuffers();
    void performAtomicSwap(TrackData *track, const juce::String &trackId);
    void updateWaveformDisplay(const juce::String &trackId);
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
    SimpleEQ masterEQ;
    std::atomic<float> masterVolume{0.8f};
    std::atomic<float> masterPan{0.0f};
    std::atomic<float> masterHighEQ{0.0f};
    std::atomic<float> masterMidEQ{0.0f};
    std::atomic<float> masterLowEQ{0.0f};

    //==============================================================================
    // MÉTHODES PRIVÉES
    //==============================================================================

    void processIncomingAudio();
    void loadAudioDataToTrack(const juce::String &trackId);
    void clearPendingAudio();

    void processMidiMessages(juce::MidiBuffer &midiMessages, bool hostIsPlaying, double hostBpm);
    void updateTimeStretchRatios(double hostBpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};