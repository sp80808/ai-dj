#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// CONFIGURATION DU BUS LAYOUT
//==============================================================================

juce::AudioProcessor::BusesProperties DjIaVstProcessor::createBusLayout()
{
    auto layout = juce::AudioProcessor::BusesProperties();

    // Sortie principale (mix)
    layout = layout.withOutput("Main", juce::AudioChannelSet::stereo(), true);

    // Sorties individuelles pour chaque piste
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        layout = layout.withOutput("Track " + juce::String(i + 1),
                                   juce::AudioChannelSet::stereo(), false);
    }

    return layout;
}

//==============================================================================
// CONSTRUCTEUR ET DESTRUCTEUR
//==============================================================================

DjIaVstProcessor::DjIaVstProcessor()
    : AudioProcessor(createBusLayout()),
      parameters(*this, nullptr, "Parameters", {std::make_unique<juce::AudioParameterBool>("generate", "Generate Loop", false), std::make_unique<juce::AudioParameterBool>("play", "Play Loop", false), std::make_unique<juce::AudioParameterBool>("autoload", "Auto-Load", true), std::make_unique<juce::AudioParameterFloat>("bpm", "BPM", 60.0f, 200.0f, 126.0f), std::make_unique<juce::AudioParameterChoice>("style", "Style", juce::StringArray{"Techno", "House", "Ambient", "Experimental"}, 0)})
{
    // Récupérer les pointeurs vers les paramètres
    generateParam = parameters.getRawParameterValue("generate");
    playParam = parameters.getRawParameterValue("play");
    autoLoadParam = parameters.getRawParameterValue("autoload");

    // Ajouter callbacks pour les changements de paramètres
    parameters.addParameterListener("generate", this);
    parameters.addParameterListener("play", this);
    parameters.addParameterListener("autoload", this);

    selectedTrackId = trackManager.createTrack("Track 1");

    // Initialiser buffers individuels
    individualOutputBuffers.resize(MAX_TRACKS);
    for (auto &buffer : individualOutputBuffers)
    {
        buffer.setSize(2, 512); // Sera redimensionné dans prepareToPlay
    }

    // Synthesiser factice pour compatibilité MIDI
    for (int i = 0; i < 4; ++i)
        synth.addVoice(new DummyVoice());
    synth.addSound(new DummySound());

    writeToLog("=== DJ-IA VST MULTI-TRACK INITIALIZED ===");
}

DjIaVstProcessor::~DjIaVstProcessor()
{
    writeToLog("=== DJ-IA VST DESTRUCTOR START ===");

    try
    {
        // Arrêter les listeners AVANT tout
        parameters.removeParameterListener("generate", this);
        parameters.removeParameterListener("play", this);
        parameters.removeParameterListener("autoload", this);

        // Arrêter tout immédiatement
        isNotePlaying = false;
        hasPendingAudioData = false;
        hasUnloadedSample = false;

        // Vider les callbacks dangereux
        midiIndicatorCallback = nullptr;

        // Nettoyer les buffers individuels
        individualOutputBuffers.clear();

        // Nettoyer le synthesiser factice
        synth.clearVoices();
        synth.clearSounds();

        writeToLog("✅ All multi-track resources cleaned up");
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ Exception in destructor: " + juce::String(e.what()));
    }
    catch (...)
    {
        writeToLog("❌ Unknown exception in destructor");
    }

    writeToLog("=== DJ-IA VST DESTROYED ===");
}

//==============================================================================
// CYCLE DE VIE DE L'AUDIO
//==============================================================================

void DjIaVstProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    hostSampleRate = newSampleRate;

    writeToLog("=== PREPARE TO PLAY (MULTI-TRACK) ===");
    writeToLog("Host sample rate: " + juce::String(hostSampleRate) + " Hz");
    writeToLog("Samples per block: " + juce::String(samplesPerBlock));
    writeToLog("Output buses: " + juce::String(getTotalNumOutputChannels() / 2));

    // Configurer le synthesiser factice
    synth.setCurrentPlaybackSampleRate(newSampleRate);

    // Redimensionner buffers individuels
    for (auto &buffer : individualOutputBuffers)
    {
        buffer.setSize(2, samplesPerBlock);
        buffer.clear();
    }
}

void DjIaVstProcessor::releaseResources()
{
    writeToLog("=== RELEASE RESOURCES (MULTI-TRACK) ===");

    // Clear tous les buffers individuels
    for (auto &buffer : individualOutputBuffers)
    {
        buffer.setSize(0, 0);
    }
}

bool DjIaVstProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    writeToLog("🔌 isBusesLayoutSupported called (Multi-Track)");
    writeToLog("  Input buses: " + juce::String(layouts.inputBuses.size()));
    writeToLog("  Output buses: " + juce::String(layouts.outputBuses.size()));

    // Vérifier que la sortie principale est présente
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    {
        writeToLog("❌ Main output must be stereo");
        return false;
    }

    // Les sorties individuelles peuvent être activées/désactivées
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        if (!layouts.outputBuses[i].isDisabled() &&
            layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
        {
            writeToLog("❌ Individual outputs must be stereo or disabled");
            return false;
        }
    }

    writeToLog("✅ Layout accepted: Main stereo + individual stereo outputs");
    return true;
}

//==============================================================================
// TRAITEMENT AUDIO PRINCIPAL MULTI-TRACK
//==============================================================================

void DjIaVstProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // DEBUG MIDI
    static int totalBlocks = 0;
    totalBlocks++;

    int midiEventCount = midiMessages.getNumEvents();
    if (midiEventCount > 0)
    {
        writeToLog("📨 BLOCK " + juce::String(totalBlocks) + " - MIDI events: " + juce::String(midiEventCount));
    }

    // Nettoyer les canaux inutilisés
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Traiter le MIDI AVANT tout le reste
    processMidiMessages(midiMessages);

    // Laisser le DummySynthesiser traiter (pour compatibilité)
    juce::AudioBuffer<float> synthBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    synthBuffer.clear();
    synth.renderNextBlock(synthBuffer, midiMessages, 0, buffer.getNumSamples());

    // Traiter l'audio en attente
    if (hasPendingAudioData.load())
    {
        processIncomingAudio();
    }

    // Redimensionner buffers individuels si nécessaire
    for (auto &indivBuffer : individualOutputBuffers)
    {
        if (indivBuffer.getNumSamples() != buffer.getNumSamples())
        {
            indivBuffer.setSize(2, buffer.getNumSamples(), false, false, true);
        }
        indivBuffer.clear();
    }

    // Clear tous les buffers de sortie
    for (int busIndex = 0; busIndex < getTotalNumOutputChannels() / 2; ++busIndex)
    {
        if (busIndex * 2 + 1 < getTotalNumOutputChannels() && busIndex <= MAX_TRACKS)
        {
            auto busBuffer = getBusBuffer(buffer, false, busIndex);
            busBuffer.clear();
        }
    }

    // Si on ne joue pas, sortir en silence
    if (!isNotePlaying.load())
    {
        return;
    }

    // Render toutes les pistes
    auto mainOutput = getBusBuffer(buffer, false, 0);
    mainOutput.clear();
    trackManager.renderAllTracks(mainOutput, individualOutputBuffers, hostSampleRate);

    // Copier vers les sorties individuelles activées
    for (int busIndex = 1; busIndex < getTotalNumOutputChannels() / 2; ++busIndex)
    {
        if (busIndex * 2 + 1 < getTotalNumOutputChannels())
        {
            auto busBuffer = getBusBuffer(buffer, false, busIndex);

            int trackIndex = busIndex - 1;
            if (trackIndex < individualOutputBuffers.size())
            {
                for (int ch = 0; ch < std::min(busBuffer.getNumChannels(), 2); ++ch)
                {
                    busBuffer.copyFrom(ch, 0, individualOutputBuffers[trackIndex], ch, 0,
                                       buffer.getNumSamples());
                }
            }
        }
    }
}

//==============================================================================
// TRAITEMENT MIDI
//==============================================================================

void DjIaVstProcessor::processMidiMessages(juce::MidiBuffer &midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();
            juce::String noteName = juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 3);

            // Trouver la track correspondant à cette note
            bool trackFound = false;
            auto trackIds = trackManager.getAllTrackIds();
            for (const auto &trackId : trackIds)
            {
                TrackData *track = trackManager.getTrack(trackId);
                if (track && track->midiNote == noteNumber)
                {
                    // Jouer SEULEMENT cette track
                    startNotePlaybackForTrack(trackId, noteNumber);
                    trackFound = true;

                    if (midiIndicatorCallback)
                    {
                        midiIndicatorCallback("Track: " + track->trackName + " (" + noteName + ")");
                    }
                    break;
                }
            }

            if (!trackFound)
            {
                writeToLog("🎹 No track assigned to note: " + noteName);
            }
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();
            stopNotePlaybackForTrack(noteNumber);
        }
    }
}

void DjIaVstProcessor::startNotePlayback(int noteNumber)
{
    isNotePlaying = true;
    currentNoteNumber = noteNumber;

    // Reset read position pour toutes les pistes
    auto trackIds = trackManager.getAllTrackIds();
    for (const auto &trackId : trackIds)
    {
        if (TrackData *track = trackManager.getTrack(trackId))
        {
            track->readPosition = 0.0;
        }
    }

    writeToLog("▶️ Multi-track playback started from note " + juce::String(noteNumber));
}

void DjIaVstProcessor::stopNotePlayback()
{
    isNotePlaying = false;
    writeToLog("⏹️ Multi-track playback stopped");
}

//==============================================================================
// API MULTI-TRACK
//==============================================================================

juce::String DjIaVstProcessor::createNewTrack(const juce::String &name)
{
    auto trackIds = trackManager.getAllTrackIds();
    if (trackIds.size() >= MAX_TRACKS)
    {
        throw std::runtime_error("Maximum number of tracks reached (" + std::to_string(MAX_TRACKS) + ")");
    }

    juce::String trackId = trackManager.createTrack(name);
    writeToLog("✅ New track created: " + trackId);
    return trackId;
}

void DjIaVstProcessor::reorderTracks(const juce::String &fromTrackId, const juce::String &toTrackId)
{
    trackManager.reorderTracks(fromTrackId, toTrackId);
    writeToLog("🔄 Tracks reordered: " + fromTrackId + " -> " + toTrackId);
}

void DjIaVstProcessor::deleteTrack(const juce::String &trackId)
{
    if (trackId == selectedTrackId)
    {
        // Sélectionner une autre piste ou créer une nouvelle
        auto trackIds = trackManager.getAllTrackIds();
        if (trackIds.size() > 1)
        {
            for (const auto &id : trackIds)
            {
                if (id != trackId)
                {
                    selectedTrackId = id;
                    break;
                }
            }
        }
        else
        {
            selectedTrackId = trackManager.createTrack("Main");
        }
    }

    trackManager.removeTrack(trackId);
    writeToLog("🗑️ Track deleted: " + trackId);
}

void DjIaVstProcessor::selectTrack(const juce::String &trackId)
{
    if (trackManager.getTrack(trackId))
    {
        selectedTrackId = trackId;
        writeToLog("🎯 Track selected: " + trackId);
    }
}

void DjIaVstProcessor::generateLoop(const DjIaClient::LoopRequest &request, const juce::String &targetTrackId)
{
    juce::String trackId = targetTrackId.isEmpty() ? selectedTrackId : targetTrackId;

    try
    {
        writeToLog("🚀 Starting API call for track: " + trackId);

        auto response = apiClient.generateLoop(request);

        writeToLog("📦 API response received for track: " + trackId);
        writeToLog("  Audio data size: " + juce::String(response.audioData.getSize()) + " bytes");
        writeToLog("  Sample rate: " + juce::String(response.sampleRate) + " Hz");

        // Stocker pour la piste spécifique
        {
            const juce::ScopedLock lock(apiLock);
            pendingTrackId = trackId;
            pendingAudioData = response.audioData;
            audioSampleRate = response.sampleRate;
            hasPendingAudioData = true;
        }

        // Stocker les métadonnées de génération
        if (TrackData *track = trackManager.getTrack(trackId))
        {
            track->prompt = request.prompt;
            track->style = request.style;
            track->bpm = request.bpm;

            juce::String stems;
            for (const auto &stem : request.preferredStems)
            {
                if (!stems.isEmpty())
                    stems += ", ";
                stems += stem;
            }
            track->stems = stems;
        }

        writeToLog("✅ Audio data queued for track: " + trackId);
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ Error in generateLoop for track " + trackId + ": " + juce::String(e.what()));
        hasPendingAudioData = false;
    }
}

void DjIaVstProcessor::startPlayback()
{
    isNotePlaying = true;

    // Reset toutes les pistes
    auto trackIds = trackManager.getAllTrackIds();
    for (const auto &trackId : trackIds)
    {
        if (TrackData *track = trackManager.getTrack(trackId))
        {
            track->readPosition = 0.0;
        }
    }

    writeToLog("▶️ Manual multi-track playback started");
}

void DjIaVstProcessor::stopPlayback()
{
    isNotePlaying = false;
    writeToLog("⏹️ Manual multi-track playback stopped");
}

//==============================================================================
// CHARGEMENT AUDIO
//==============================================================================

void DjIaVstProcessor::processIncomingAudio()
{
    if (!hasPendingAudioData.load() || pendingTrackId.isEmpty())
        return;

    writeToLog("📥 Processing pending audio data for track: " + pendingTrackId);

    // Si auto-load activé, charger immédiatement
    if (autoLoadEnabled.load())
    {
        writeToLog("🔄 Auto-loading sample to track: " + pendingTrackId);
        loadAudioDataToTrack(pendingTrackId);
    }
    else
    {
        writeToLog("⏸️ Sample ready for track " + pendingTrackId + " - waiting for manual load");
        hasUnloadedSample = true;
    }
}

void DjIaVstProcessor::loadAudioDataToTrack(const juce::String &trackId)
{
    TrackData *track = trackManager.getTrack(trackId);
    if (!track)
    {
        writeToLog("❌ Track not found: " + trackId);
        clearPendingAudio();
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(
            std::make_unique<juce::MemoryInputStream>(pendingAudioData, false)));

    if (!reader)
    {
        writeToLog("❌ Failed to create audio reader for track: " + trackId);
        clearPendingAudio();
        return;
    }

    try
    {
        // Récupérer les infos du fichier
        track->sampleRate = reader->sampleRate;
        track->numSamples = static_cast<int>(reader->lengthInSamples);
        int numSourceChannels = reader->numChannels;

        writeToLog("📊 Loading audio to track " + track->trackName + ":");
        writeToLog("  Sample rate: " + juce::String(track->sampleRate) + " Hz");
        writeToLog("  Channels: " + juce::String(numSourceChannels));
        writeToLog("  Samples: " + juce::String(track->numSamples));

        // Validation du sample rate
        if (track->sampleRate <= 0.0 || track->sampleRate > 192000.0)
        {
            writeToLog("⚠️ Invalid sample rate, defaulting to 44100 Hz");
            track->sampleRate = 44100.0;
        }

        // Redimensionner le buffer de la piste
        track->audioBuffer.setSize(2, track->numSamples, false, false, true);
        track->audioBuffer.clear();

        // Charger l'audio
        reader->read(&track->audioBuffer, 0, track->numSamples, 0, true, numSourceChannels == 1);

        // Dupliquer mono vers stéréo si nécessaire
        if (numSourceChannels == 1 && track->audioBuffer.getNumChannels() > 1)
        {
            track->audioBuffer.copyFrom(1, 0, track->audioBuffer, 0, 0, track->numSamples);
        }

        // Reset position de lecture
        track->readPosition = 0.0;

        writeToLog("✅ Audio loaded successfully to track: " + track->trackName);
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ Error loading audio to track " + trackId + ": " + juce::String(e.what()));
        track->reset();
    }

    clearPendingAudio();
    hasUnloadedSample = false;
}

void DjIaVstProcessor::loadPendingSample()
{
    if (hasUnloadedSample.load() && !pendingTrackId.isEmpty())
    {
        writeToLog("📂 Loading sample manually to track: " + pendingTrackId);
        loadAudioDataToTrack(pendingTrackId);
    }
}

void DjIaVstProcessor::clearPendingAudio()
{
    const juce::ScopedLock lock(apiLock);
    pendingAudioData.reset();
    pendingTrackId.clear();
    hasPendingAudioData = false;
}

void DjIaVstProcessor::setAutoLoadEnabled(bool enabled)
{
    autoLoadEnabled = enabled;
    writeToLog(enabled ? "🔄 Auto-load enabled" : "⏸️ Auto-load disabled - manual mode");
}

//==============================================================================
// CONFIGURATION
//==============================================================================

void DjIaVstProcessor::setApiKey(const juce::String &key)
{
    apiKey = key;
    apiClient = DjIaClient(apiKey, serverUrl);
    writeToLog("🔑 API key updated");
}

void DjIaVstProcessor::setServerUrl(const juce::String &url)
{
    serverUrl = url;
    apiClient = DjIaClient(apiKey, serverUrl);
    writeToLog("🌐 Server URL updated: " + url);
}

double DjIaVstProcessor::getHostBpm() const
{
    if (auto playHead = getPlayHead())
    {
        if (auto positionInfo = playHead->getPosition())
        {
            if (positionInfo->getBpm().hasValue())
            {
                double bpm = *positionInfo->getBpm();
                writeToLog("🎵 Host BPM detected: " + juce::String(bpm));
                return bpm;
            }
        }
    }

    writeToLog("⚠️ No host BPM available");
    return 0.0;
}

//==============================================================================
// ÉTAT ET SÉRIALISATION
//==============================================================================

juce::AudioProcessorEditor *DjIaVstProcessor::createEditor()
{
    return new DjIaVstEditor(*this);
}

void DjIaVstProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    juce::ValueTree state("DjIaVstState");

    // État basique
    state.setProperty("serverUrl", serverUrl, nullptr);
    state.setProperty("apiKey", apiKey, nullptr);
    state.setProperty("lastPrompt", lastPrompt, nullptr);
    state.setProperty("lastStyle", lastStyle, nullptr);
    state.setProperty("lastKey", lastKey, nullptr);
    state.setProperty("lastBpm", lastBpm, nullptr);
    state.setProperty("lastPresetIndex", lastPresetIndex, nullptr);
    state.setProperty("hostBpmEnabled", hostBpmEnabled, nullptr);

    // État multi-track
    state.setProperty("selectedTrackId", selectedTrackId, nullptr);

    // Sauvegarder toutes les pistes
    state.appendChild(trackManager.saveState(), nullptr);

    writeToLog("💾 Saving multi-track state - " + juce::String(trackManager.getAllTrackIds().size()) + " tracks");

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DjIaVstProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName("DjIaVstState"))
    {
        juce::ValueTree state = juce::ValueTree::fromXml(*xml);

        // Charger état basique
        lastPrompt = state.getProperty("lastPrompt", "").toString();
        lastStyle = state.getProperty("lastStyle", "Techno").toString();
        lastKey = state.getProperty("lastKey", "C minor").toString();
        lastBpm = state.getProperty("lastBpm", 126.0);
        lastPresetIndex = state.getProperty("lastPresetIndex", -1);
        hostBpmEnabled = state.getProperty("hostBpmEnabled", false);

        juce::String newServerUrl = state.getProperty("serverUrl", "http://localhost:8000").toString();
        juce::String newApiKey = state.getProperty("apiKey", "").toString();

        if (newServerUrl != serverUrl)
        {
            setServerUrl(newServerUrl);
        }

        if (newApiKey != apiKey)
        {
            setApiKey(newApiKey);
        }

        // Charger les pistes
        auto tracksState = state.getChildWithName("TrackManager");
        if (tracksState.isValid())
        {
            trackManager.loadState(tracksState);
        }

        // Restaurer piste sélectionnée
        selectedTrackId = state.getProperty("selectedTrackId", "").toString();
        if (selectedTrackId.isEmpty() || !trackManager.getTrack(selectedTrackId))
        {
            auto trackIds = trackManager.getAllTrackIds();
            if (!trackIds.empty())
            {
                selectedTrackId = trackIds[0];
            }
            else
            {
                selectedTrackId = trackManager.createTrack("Main");
            }
        }

        writeToLog("📂 Loading multi-track state - " + juce::String(trackManager.getAllTrackIds().size()) + " tracks loaded");
    }
}

//==============================================================================
// PARAMÈTRES AUTOMATISABLES
//==============================================================================

void DjIaVstProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    writeToLog("🎛️ Parameter changed: " + parameterID + " = " + juce::String(newValue));

    if (parameterID == "generate" && newValue > 0.5f)
    {
        writeToLog("🚀 Generate triggered from Device Panel for track: " + selectedTrackId);

        // Reset le paramètre (bouton momentané)
        juce::MessageManager::callAsync([this]()
                                        { parameters.getParameter("generate")->setValueNotifyingHost(0.0f); });
    }
    else if (parameterID == "play")
    {
        if (newValue > 0.5f)
        {
            writeToLog("▶️ Play triggered from Device Panel");
            startPlayback();
        }
        else
        {
            writeToLog("⏹️ Stop triggered from Device Panel");
            stopPlayback();
        }
    }
    else if (parameterID == "autoload")
    {
        bool enabled = newValue > 0.5f;
        setAutoLoadEnabled(enabled);
        writeToLog("🔄 Auto-load " + juce::String(enabled ? "enabled" : "disabled") + " from Device Panel");
    }
}

//==============================================================================
// LOGGING
//==============================================================================

void DjIaVstProcessor::writeToLog(const juce::String &message)
{
    auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                    .getChildFile("dj_ia_vst_multitrack.log");

    auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
    file.appendText(time + ": " + message + "\n");
}

void DjIaVstProcessor::startNotePlaybackForTrack(const juce::String &trackId, int noteNumber)
{

    TrackData *track = trackManager.getTrack(trackId);
    if (!track || track->numSamples == 0)
        return;

    track->readPosition = 0.0;
    track->isPlaying = true;

    playingTracks[noteNumber] = trackId;
}

void DjIaVstProcessor::stopNotePlaybackForTrack(int noteNumber)
{
    auto it = playingTracks.find(noteNumber);
    if (it != playingTracks.end())
    {
        TrackData *track = trackManager.getTrack(it->second);
        if (track)
        {
            track->isPlaying = false;
        }
        playingTracks.erase(it);
        writeToLog("⏹️ Stopped track for note " + juce::String(noteNumber));
    }
}