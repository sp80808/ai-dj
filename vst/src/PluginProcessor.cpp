#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AudioAnalyzer.h"

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
    startTimerHz(30);
    writeToLog("=== DJ-IA VST MULTI-TRACK INITIALIZED ===");
}

DjIaVstProcessor::~DjIaVstProcessor()
{
    writeToLog("=== DJ-IA VST DESTRUCTOR START ===");
    stopTimer();
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

void DjIaVstProcessor::timerCallback()
{
    // Seulement si nécessaire
    if (!needsUIUpdate.load())
        return;

    // Notifier l'UI
    if (onUIUpdateNeeded)
    {
        onUIUpdateNeeded();
    }

    needsUIUpdate = false;
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
    masterEQ.prepare(newSampleRate, samplesPerBlock);
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
    // Nettoyer les canaux inutilisés
    checkAndSwapStagingBuffers();
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // IMPORTANT: Récupérer les infos de position du DAW
    auto playHead = getPlayHead();
    bool hostIsPlaying = false;
    double hostBpm = 126.0;
    double hostPpqPosition = 0.0;

    if (playHead)
    {
        if (auto positionInfo = playHead->getPosition())
        {
            hostIsPlaying = positionInfo->getIsPlaying();
            if (auto bpm = positionInfo->getBpm())
            {
                hostBpm = *bpm;
                cachedHostBpm = hostBpm;
            }
            if (auto ppq = positionInfo->getPpqPosition())
            {
                hostPpqPosition = *ppq;
            }
        }
    }

    // Traiter le MIDI AVANT tout le reste
    processMidiMessages(midiMessages, hostIsPlaying, hostBpm);

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

    // Render toutes les pistes ACTIVES (celles qui ont des notes MIDI pressées)
    auto mainOutput = getBusBuffer(buffer, false, 0);
    mainOutput.clear();

    // Mettre à jour les ratios de time-stretch avec le BPM de l'hôte
    updateTimeStretchRatios(hostBpm);

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
    updateMasterEQ();
    masterEQ.processBlock(mainOutput);
    float masterVol = masterVolume.load();
    mainOutput.applyGain(masterVol);

    // Appliquer pan master
    float masterPanVal = masterPan.load();
    if (mainOutput.getNumChannels() >= 2 && std::abs(masterPanVal) > 0.01f)
    {
        if (masterPanVal < 0.0f)
        { // Pan gauche
            mainOutput.applyGain(1, 0, mainOutput.getNumSamples(), 1.0f + masterPanVal);
        }
        else
        { // Pan droite
            mainOutput.applyGain(0, 0, mainOutput.getNumSamples(), 1.0f - masterPanVal);
        }
    }
    bool anyTrackPlaying = false;
    auto trackIds = trackManager.getAllTrackIds();
    for (const auto &trackId : trackIds)
    {
        TrackData *track = trackManager.getTrack(trackId);
        if (track && track->isPlaying.load())
        {
            anyTrackPlaying = true;
            break;
        }
    }

    if (anyTrackPlaying || midiMessages.getNumEvents() > 0)
    {
        needsUIUpdate = true;
    }
}

void DjIaVstProcessor::updateMasterEQ()
{
    masterEQ.setHighGain(masterHighEQ.load());
    masterEQ.setMidGain(masterMidEQ.load());
    masterEQ.setLowGain(masterLowEQ.load());
}

// Modifier processMidiMessages pour accepter les infos host
void DjIaVstProcessor::processMidiMessages(juce::MidiBuffer &midiMessages, bool hostIsPlaying, double hostBpm)
{
    static int totalBlocks = 0;
    totalBlocks++;

    int midiEventCount = midiMessages.getNumEvents();
    if (midiEventCount > 0)
    {
        needsUIUpdate = true;
    }

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
                    // SEULEMENT jouer si on a de l'audio OU si le host joue
                    if (track->numSamples > 0)
                    {
                        startNotePlaybackForTrack(trackId, noteNumber, hostBpm);
                        trackFound = true;

                        if (midiIndicatorCallback)
                        {
                            midiIndicatorCallback(">> " + track->trackName + " (" + noteName + ") - BPM:" + juce::String(hostBpm, 0));
                        }

                        writeToLog("🎹 Playing track: " + track->trackName + " (Note: " + noteName + ", Host BPM: " + juce::String(hostBpm, 1) + ")");
                    }
                    else
                    {
                        writeToLog("⚠️ Track " + track->trackName + " has no audio data");
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

void DjIaVstProcessor::updateTimeStretchRatios(double hostBpm)
{
    auto trackIds = trackManager.getAllTrackIds();
    for (const auto &trackId : trackIds)
    {
        TrackData *track = trackManager.getTrack(trackId);
        if (!track)
            continue;

        double ratio = 1.0;

        switch (track->timeStretchMode)
        {
        case 1: // Off
        case 3: // Host BPM (déjà pré-étiré)
            ratio = 1.0;
            break;

        case 2: // Manual seulement
        case 4: // Host + Manual offset
            if (track->originalBpm > 0.0f && hostBpm > 0.0)
            {
                // ✅ D'ABORD le ratio host, PUIS l'offset
                double hostRatio = hostBpm / track->originalBpm;
                double manualAdjust = track->bpmOffset / track->originalBpm;
                ratio = hostRatio + manualAdjust; // Host sync + correction manuelle
            }
            break;
        }

        ratio = juce::jlimit(0.25, 4.0, ratio);
        track->cachedPlaybackRatio = ratio;
    }
}

void DjIaVstProcessor::startNotePlaybackForTrack(const juce::String &trackId, int noteNumber, double hostBpm)
{
    TrackData *track = trackManager.getTrack(trackId);
    if (!track || track->numSamples == 0)
        return;

    // Vérifier si la track est armée
    if (!track->isArmed.load())
    {
        writeToLog("🎹 Track " + track->trackName + " not armed - ignoring MIDI");
        return;
    }

    // Si déjà en cours de lecture, ignorer
    if (track->isPlaying.load())
    {
        writeToLog("🎹 Track " + track->trackName + " already playing - ignoring MIDI");
        return;
    }

    // Démarrer la lecture
    track->readPosition = 0.0;
    track->isPlaying = true;

    playingTracks[noteNumber] = trackId;

    writeToLog("▶️ Started playback: " + track->trackName + " (Armed -> Playing)");
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
            // GARDER l'ARM - ne pas désarmer automatiquement
            writeToLog("⏹️ Stopped playback for note " + juce::String(noteNumber) + " (still armed)");
        }
        playingTracks.erase(it);
    }
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

//==============================================================================
// CHARGEMENT AUDIO
//==============================================================================

void DjIaVstProcessor::processIncomingAudio()
{
    if (!hasPendingAudioData.load() || pendingTrackId.isEmpty())
        return;

    writeToLog("📥 Processing pending audio data for track: " + pendingTrackId);

    if (autoLoadEnabled.load())
    {
        writeToLog("🔄 Starting ASYNC loading for track: " + pendingTrackId);

        // Lancer dans un thread séparé
        juce::Thread::launch([this, trackId = pendingTrackId, audioData = pendingAudioData]()
                             { loadAudioDataAsync(trackId, audioData); });

        // Clear pending immédiatement pour éviter les doublons
        clearPendingAudio();
    }
    else
    {
        writeToLog("⏸️ Sample ready for track " + pendingTrackId + " - waiting for manual load");
        hasUnloadedSample = true;
    }
}

void DjIaVstProcessor::checkAndSwapStagingBuffers()
{
    auto trackIds = trackManager.getAllTrackIds();

    for (const auto &trackId : trackIds)
    {
        TrackData *track = trackManager.getTrack(trackId);
        if (!track)
            continue;

        // Check si swap demandé
        if (track->swapRequested.exchange(false))
        {
            if (track->hasStagingData.load())
            {
                performAtomicSwap(track, trackId);
                writeToLog("🔄 Buffer swapped for track: " + trackId);
            }
        }
    }
}

void DjIaVstProcessor::performAtomicSwap(TrackData *track, const juce::String &trackId)
{
    // Swap ultra-rapide des buffers
    std::swap(track->audioBuffer, track->stagingBuffer);

    // Copier métadonnées
    track->numSamples = track->stagingNumSamples.load();
    track->sampleRate = track->stagingSampleRate.load();
    track->originalBpm = track->stagingOriginalBpm;

    // Reset loop points selon nouvelle durée
    double sampleDuration = track->numSamples / track->sampleRate;
    if (sampleDuration <= 8.0)
    {
        track->loopStart = 0.0;
        track->loopEnd = sampleDuration;
    }
    else
    {
        double beatDuration = 60.0 / track->originalBpm;
        double fourBars = beatDuration * 16.0; // 4 mesures
        track->loopStart = 0.0;
        track->loopEnd = std::min(fourBars, sampleDuration);
    }

    // Reset lecture
    track->readPosition = 0.0;

    // Clear staging
    track->hasStagingData = false;
    track->stagingBuffer.setSize(0, 0); // Libérer mémoire
    juce::MessageManager::callAsync([this, trackId]()
                                    {
                                        updateWaveformDisplay(trackId); // ← Utilise le vrai trackId
                                    });
    writeToLog("✅ Atomic swap complete for: " + trackId);
}

void DjIaVstProcessor::updateWaveformDisplay(const juce::String &trackId)
{
    if (auto *editor = dynamic_cast<DjIaVstEditor *>(getActiveEditor()))
    {
        for (auto &trackComp : editor->getTrackComponents())
        {
            if (trackComp->getTrackId() == trackId)
            {
                if (trackComp->isWaveformVisible())
                {
                    trackComp->refreshWaveformDisplay();
                    writeToLog("🔄 Waveform updated for: " + trackId);
                }
                break;
            }
        }
    }
}

void DjIaVstProcessor::loadAudioDataAsync(const juce::String &trackId, const juce::MemoryBlock &audioData)
{
    writeToLog("🔄 [ASYNC THREAD] Processing audio for track: " + trackId);

    TrackData *track = trackManager.getTrack(trackId);
    if (!track)
    {
        writeToLog("❌ [ASYNC] Track not found: " + trackId);
        return;
    }

    try
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(
                std::make_unique<juce::MemoryInputStream>(audioData, false)));

        if (!reader)
        {
            writeToLog("❌ [ASYNC] Failed to create audio reader");
            return;
        }

        // Préparer dans le staging buffer (thread-safe)
        int numChannels = reader->numChannels;
        int numSamples = static_cast<int>(reader->lengthInSamples);
        double sampleRate = reader->sampleRate;

        writeToLog("📊 [ASYNC] Staging audio: " + juce::String(numSamples) + " samples");

        // Redimensionner staging buffer
        track->stagingBuffer.setSize(2, numSamples, false, false, true);
        track->stagingBuffer.clear();

        // Charger dans staging (pas le buffer principal !)
        reader->read(&track->stagingBuffer, 0, numSamples, 0, true, numChannels == 1);

        // Dupliquer mono vers stéréo si nécessaire
        if (numChannels == 1 && track->stagingBuffer.getNumChannels() > 1)
        {
            track->stagingBuffer.copyFrom(1, 0, track->stagingBuffer, 0, 0, numSamples);
        }

        float detectedBPM = AudioAnalyzer::detectBPM(track->stagingBuffer, sampleRate);
        writeToLog("🎵 === BPM ANALYZER DEBUG ===");
        writeToLog("  Audio duration: " + juce::String(numSamples / sampleRate, 2) + " seconds");
        writeToLog("  Sample rate: " + juce::String(sampleRate) + " Hz");
        writeToLog("  Raw detected BPM: " + juce::String(detectedBPM, 2));
        writeToLog("  Track requested BPM: " + juce::String(track->bpm, 1));
        writeToLog("  BPM in valid range: " + juce::String((detectedBPM > 60.0f && detectedBPM < 200.0f) ? "YES" : "NO"));

        track->stagingOriginalBpm = (detectedBPM > 60.0f && detectedBPM < 200.0f) ? detectedBPM : track->bpm;
        writeToLog("  Final stagingOriginalBpm: " + juce::String(track->stagingOriginalBpm, 2));

        double hostBpm = cachedHostBpm.load();
        writeToLog("  Host BPM: " + juce::String(hostBpm, 1));
        writeToLog("  BPM difference: " + juce::String(std::abs(hostBpm - track->stagingOriginalBpm), 2));
        writeToLog("  Will time-stretch: " + juce::String((std::abs(hostBpm - track->stagingOriginalBpm) > 1.0) ? "YES" : "NO"));

        if (hostBpm > 0.0 && track->stagingOriginalBpm > 0.0f &&
            std::abs(hostBpm - track->stagingOriginalBpm) > 1.0)
        {
            double stretchRatio = hostBpm / track->stagingOriginalBpm;
            writeToLog("🎵 Time-stretching: " + juce::String(track->stagingOriginalBpm, 1) +
                       " -> " + juce::String(hostBpm, 1) + " BPM (ratio: " + juce::String(stretchRatio, 3) + ")");

            AudioAnalyzer::timeStretchBuffer(track->stagingBuffer, stretchRatio, sampleRate);
            track->stagingOriginalBpm = hostBpm;

            // APRÈS le time-stretch
            writeToLog("🔍 TIME-STRETCH VERIFICATION:");
            writeToLog("  Samples BEFORE stretch: " + juce::String(numSamples));
            writeToLog("  Samples AFTER stretch: " + juce::String(track->stagingBuffer.getNumSamples()));
            writeToLog("  Duration BEFORE: " + juce::String(numSamples / sampleRate, 3) + "s");
            writeToLog("  Duration AFTER: " + juce::String(track->stagingBuffer.getNumSamples() / sampleRate, 3) + "s");
        }

        // Préparer métadonnées staging
        track->stagingNumSamples = numSamples;
        track->stagingSampleRate = sampleRate;

        // SIGNAL : Prêt pour swap atomique
        track->hasStagingData = true;
        track->swapRequested = true;

        writeToLog("✅ [ASYNC] Audio staging complete, ready for swap");
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ [ASYNC] Error: " + juce::String(e.what()));
        track->hasStagingData = false;
        track->swapRequested = false;
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
            writeToLog("⚠️ Invalid sample rate, defaulting to 48000 Hz");
            track->sampleRate = 48000.0;
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

        float detectedBPM = AudioAnalyzer::detectBPM(track->audioBuffer, track->sampleRate);

        if (detectedBPM > 60.0f && detectedBPM < 200.0f)
        {
            track->originalBpm = detectedBPM;
            writeToLog("🎵 Detected BPM: " + juce::String(detectedBPM, 1) +
                       " for track: " + track->trackName);
        }
        else
        {
            track->originalBpm = track->bpm; // Fallback
            writeToLog("⚠️ BPM detection failed for " + track->trackName +
                       ", using requested BPM: " + juce::String(track->bpm));
        }

        // Reset position de lecture
        track->readPosition = 0.0;

        double sampleDuration = track->numSamples / track->sampleRate;

        // Si le sample fait moins de 8 secondes, utiliser tout
        if (sampleDuration <= 8.0)
        {
            track->loopStart = 0.0;
            track->loopEnd = sampleDuration;
        }
        else
        {
            // Pour les samples longs, prendre les 4 premières mesures (estimation)
            double estimatedMeasureDuration = 60.0 / track->originalBpm * 4.0; // 4 beats par mesure
            double fourMeasures = estimatedMeasureDuration * 4.0;

            track->loopStart = 0.0;
            track->loopEnd = std::min(fourMeasures, sampleDuration);
        }

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

void DjIaVstProcessor::addCustomPrompt(const juce::String &prompt)
{
    if (!prompt.isEmpty() && !customPrompts.contains(prompt))
    {
        customPrompts.add(prompt);
        writeToLog("Custom prompt added: " + prompt);
    }
}

juce::StringArray DjIaVstProcessor::getCustomPrompts() const
{
    return customPrompts;
}

void DjIaVstProcessor::clearCustomPrompts()
{
    customPrompts.clear();
}

void DjIaVstProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    writeToLog("=== SAVING STATE ===");

    auto trackIds = trackManager.getAllTrackIds();
    writeToLog("Saving " + juce::String(trackIds.size()) + " tracks");

    for (const auto &id : trackIds)
    {
        TrackData *track = trackManager.getTrack(id);
        if (track)
        {
            writeToLog("  Saving track: " + track->trackName + " (ID: " + id + ")");
            writeToLog("    Has audio: " + juce::String(track->numSamples > 0 ? "YES" : "NO"));
            if (track->numSamples > 0)
            {
                writeToLog("    Audio: " + juce::String(track->numSamples) + " samples");
            }
        }
    }

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
    state.setProperty("lastDuration", lastDuration, nullptr);

    // État multi-track
    state.setProperty("selectedTrackId", selectedTrackId, nullptr);
    writeToLog("Selected track being saved: " + selectedTrackId);

    juce::ValueTree promptsState("CustomPrompts");
    for (int i = 0; i < customPrompts.size(); ++i)
    {
        promptsState.setProperty("prompt_" + juce::String(i), customPrompts[i], nullptr);
    }
    state.appendChild(promptsState, nullptr);
    writeToLog("Saved " + juce::String(customPrompts.size()) + " custom prompts");

    // Sauvegarder toutes les pistes
    auto tracksState = trackManager.saveState();
    state.appendChild(tracksState, nullptr);

    writeToLog("TrackManager state has " + juce::String(tracksState.getNumChildren()) + " children");

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);

    writeToLog("State saved - " + juce::String(destData.getSize()) + " bytes");
}

void DjIaVstProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    writeToLog("=== LOADING STATE ===");
    writeToLog("Data size: " + juce::String(sizeInBytes) + " bytes");

    // ✅ 1. Parser XML sur le thread audio (rapide)
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName("DjIaVstState"))
    {
        writeToLog("Failed to parse state XML!");
        return;
    }

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);

    // ✅ 2. Charger données thread-safe (atomic/simples)
    lastPrompt = state.getProperty("lastPrompt", "").toString();
    lastStyle = state.getProperty("lastStyle", "Techno").toString();
    lastKey = state.getProperty("lastKey", "C minor").toString();
    lastBpm = state.getProperty("lastBpm", 126.0);
    lastPresetIndex = state.getProperty("lastPresetIndex", -1);
    hostBpmEnabled = state.getProperty("hostBpmEnabled", false);
    lastDuration = state.getProperty("lastDuration", 6.0);

    // ✅ 3. Charger custom prompts
    auto promptsState = state.getChildWithName("CustomPrompts");
    if (promptsState.isValid())
    {
        customPrompts.clear();

        // Méthode 1: Si on a un count
        if (promptsState.hasProperty("count"))
        {
            int count = promptsState.getProperty("count", 0);
            writeToLog("Loading " + juce::String(count) + " custom prompts (new format)");

            for (int i = 0; i < promptsState.getNumChildren(); ++i)
            {
                auto promptNode = promptsState.getChild(i);
                if (promptNode.hasType("Prompt"))
                {
                    juce::String prompt = promptNode.getProperty("text", "").toString();
                    if (prompt.isNotEmpty())
                    {
                        customPrompts.add(prompt);
                        writeToLog("  Loaded: " + prompt);
                    }
                }
            }
        }
        else
        {
            // Méthode 2: Ancien format (fallback)
            writeToLog("Loading custom prompts (old format)");

            // Collecter tous les prompts avec leur index
            juce::Array<std::pair<int, juce::String>> indexedPrompts;

            for (int i = 0; i < promptsState.getNumProperties(); ++i)
            {
                auto propertyName = promptsState.getPropertyName(i);
                if (propertyName.toString().startsWith("prompt_"))
                {
                    juce::String indexStr = propertyName.toString().substring(7); // Après "prompt_"
                    int index = indexStr.getIntValue();
                    juce::String prompt = promptsState.getProperty(propertyName, "").toString();

                    if (prompt.isNotEmpty())
                    {
                        indexedPrompts.add({index, prompt});
                    }
                }
            }

            // Trier par index pour maintenir l'ordre
            std::sort(indexedPrompts.begin(), indexedPrompts.end(),
                      [](const auto &a, const auto &b)
                      { return a.first < b.first; });

            // Ajouter dans l'ordre
            for (const auto &pair : indexedPrompts)
            {
                customPrompts.add(pair.second);
                writeToLog("  Loaded: " + pair.second);
            }
        }

        writeToLog("Final custom prompts count: " + juce::String(customPrompts.size()));
    }
    else
    {
        writeToLog("No custom prompts state found");
    }
    // ✅ 4. Config serveur (thread-safe)
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

    // ✅ 5. Charger tracks (AVANT UI)
    writeToLog("Before loading tracks: " + juce::String(trackManager.getAllTrackIds().size()) + " tracks");

    auto tracksState = state.getChildWithName("TrackManager");
    if (tracksState.isValid())
    {
        writeToLog("Found TrackManager state with " + juce::String(tracksState.getNumChildren()) + " children");
        trackManager.loadState(tracksState);
    }
    else
    {
        writeToLog("No TrackManager state found!");
    }

    // ✅ 6. Valider selected track
    selectedTrackId = state.getProperty("selectedTrackId", "").toString();
    auto loadedTrackIds = trackManager.getAllTrackIds();

    if (selectedTrackId.isEmpty() || !trackManager.getTrack(selectedTrackId))
    {
        if (!loadedTrackIds.empty())
        {
            selectedTrackId = loadedTrackIds[0];
            writeToLog("Using first available track: " + selectedTrackId);
        }
        else
        {
            selectedTrackId = trackManager.createTrack("Main");
            writeToLog("Created new track: " + selectedTrackId);
        }
    }

    writeToLog("Loading complete - " + juce::String(loadedTrackIds.size()) + " tracks loaded");

    // ✅ 7. UPDATE UI ASYNC + ÉTALÉ pour éviter le blanc
    juce::MessageManager::callAsync([this]()
                                    {
        if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
        {
            writeToLog("🎨 Starting async UI update...");
            
            // Phase 1: Update UI state SANS tracks
            editor->updateUIFromProcessor();
            
            // Phase 2: Refresh tracks après un petit délai
            juce::Timer::callAfterDelay(50, [this]()
            {
                if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
                {
                    writeToLog("🎨 Refreshing track components...");
                    editor->refreshTrackComponents();
                    
                    // Phase 3: Update waveforms APRÈS que tout soit créé
                    juce::Timer::callAfterDelay(100, [this]()
                    {
                        writeToLog("🎨 Updating waveforms...");
                        updateAllWaveformsAfterLoad();
                        writeToLog("✅ UI update complete!");
                    });
                }
            });
        }
        else
        {
            writeToLog("No editor available to notify");
        } });
}

// ✅ NOUVELLE MÉTHODE pour waveforms
void DjIaVstProcessor::updateAllWaveformsAfterLoad()
{
    if (auto *editor = dynamic_cast<DjIaVstEditor *>(getActiveEditor()))
    {
        auto trackIds = trackManager.getAllTrackIds();
        for (const auto &trackId : trackIds)
        {
            TrackData *track = trackManager.getTrack(trackId);
            if (track && track->numSamples > 0)
            {
                updateWaveformDisplay(trackId);
            }
        }
        writeToLog("✅ All waveforms updated after state load");
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
