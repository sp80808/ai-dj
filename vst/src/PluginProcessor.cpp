#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// CONSTRUCTEUR ET DESTRUCTEUR
//==============================================================================

DjIaVstProcessor::DjIaVstProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
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

    // Synthesiser factice
    for (int i = 0; i < 4; ++i)
        synth.addVoice(new DummyVoice());
    synth.addSound(new DummySound());

    writeToLog("=== DJ-IA VST INITIALIZED ===");
}

// Dans PluginProcessor.cpp - destructeur mis à jour :
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
        hasLoop = false;
        hasPendingAudioData = false;

        // Vider les callbacks dangereux
        midiIndicatorCallback = nullptr;

        // Nettoyer les buffers
        {
            juce::ScopedLock lock(bufferLock);
            audioBuffer.setSize(0, 0);
            bufferNumSamples = 0;
        }

        // Nettoyer le synthesiser factice
        synth.clearVoices();
        synth.clearSounds();

        writeToLog("✅ All resources cleaned up");
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

    writeToLog("=== PREPARE TO PLAY ===");
    writeToLog("Host sample rate: " + juce::String(hostSampleRate) + " Hz");
    writeToLog("Samples per block: " + juce::String(samplesPerBlock));

    // DEBUG MIDI CAPABILITIES
    writeToLog("🎹 MIDI Debug Info:");
    writeToLog("  acceptsMidi(): " + juce::String(acceptsMidi() ? "TRUE" : "FALSE"));
    writeToLog("  producesMidi(): " + juce::String(producesMidi() ? "TRUE" : "FALSE"));
    writeToLog("  isMidiEffect(): " + juce::String(isMidiEffect() ? "TRUE" : "FALSE"));
    writeToLog("  Total input channels: " + juce::String(getTotalNumInputChannels()));
    writeToLog("  Total output channels: " + juce::String(getTotalNumOutputChannels()));

    // Configurer le synthesiser factice
    synth.setCurrentPlaybackSampleRate(newSampleRate);

    // Buffer simple de 10 secondes max
    juce::ScopedLock lock(bufferLock);
    int maxBufferSize = static_cast<int>(newSampleRate * 10.0);
    audioBuffer.setSize(2, maxBufferSize, false, true, true);
    audioBuffer.clear();

    bufferNumSamples = 0;
    bufferNumChannels = 2;

    readPosition = 0.0;
    playbackSpeedRatio = 1.0;
}

void DjIaVstProcessor::releaseResources()
{
    writeToLog("=== RELEASE RESOURCES ===");
    juce::ScopedLock lock(bufferLock);
    audioBuffer.setSize(0, 0);
    hasLoop = false;
    bufferNumSamples = 0;
}

//==============================================================================
// TRAITEMENT AUDIO PRINCIPAL - VERSION ULTRA-OPTIMISÉE
//==============================================================================

void DjIaVstProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // COMPTEUR DE DEBUG
    static int totalBlocks = 0;
    static int midiBlocks = 0;
    totalBlocks++;

    // DEBUG MIDI RECEPTION
    int midiEventCount = midiMessages.getNumEvents();

    if (midiEventCount > 0)
    {
        midiBlocks++;
        writeToLog("📨 BLOCK " + juce::String(totalBlocks) + " - MIDI events: " + juce::String(midiEventCount));

        for (const auto metadata : midiMessages)
        {
            auto message = metadata.getMessage();
            writeToLog("🎵 MIDI: " + message.getDescription() + " at sample " + juce::String(metadata.samplePosition));
        }
    }

    // DEBUG PERIODIQUE
    if (totalBlocks % 2000 == 0) // Chaque ~40 secondes à 48kHz/256
    {
        writeToLog("📊 Stats: " + juce::String(totalBlocks) + " total blocks, " +
                   juce::String(midiBlocks) + " with MIDI (" +
                   juce::String((float)midiBlocks / totalBlocks * 100, 1) + "%)");
    }

    // Nettoyer les canaux inutilisés
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Traiter le MIDI AVANT tout le reste
    processMidiMessages(midiMessages);

    // Laisser le DummySynthesiser traiter (pour Bitwig)
    juce::AudioBuffer<float> synthBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    synthBuffer.clear();
    synth.renderNextBlock(synthBuffer, midiMessages, 0, buffer.getNumSamples());

    // Traiter l'audio en attente
    if (hasPendingAudioData.load())
    {
        processIncomingAudio();
    }

    // Si pas de loop OU si on ne joue pas, sortir en silence
    if (!hasLoop.load() || bufferNumSamples == 0 || !isNotePlaying.load())
    {
        buffer.clear();
        return;
    }

    // Générer l'audio seulement si on a un loop ET qu'on joue
    generateAudioOutput(buffer);
}

bool DjIaVstProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    writeToLog("🔌 isBusesLayoutSupported called");
    writeToLog("  Input buses: " + juce::String(layouts.inputBuses.size()));
    writeToLog("  Output buses: " + juce::String(layouts.outputBuses.size()));

    // Accepter : 0 inputs audio, 2 outputs audio (instrument typique)
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
        layouts.getMainInputChannelSet() == juce::AudioChannelSet())
    {
        writeToLog("✅ Layout accepted: 0 inputs, stereo output");
        return true;
    }

    writeToLog("❌ Layout rejected");
    return false;
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
            // Convertir numéro de note en nom
            juce::String noteName = juce::MidiMessage::getMidiNoteName(message.getNoteNumber(), true, true, 3);
            juce::String noteInfo = "Note ON: " + noteName + " (vel:" + juce::String(message.getVelocity()) + ")";

            writeToLog("🎹 " + noteInfo);

            // Notifier l'interface
            if (midiIndicatorCallback)
                midiIndicatorCallback(noteInfo);

            startNotePlayback(message.getNoteNumber());
        }
        else if (message.isNoteOff())
        {
            juce::String noteName = juce::MidiMessage::getMidiNoteName(message.getNoteNumber(), true, true, 3);
            juce::String noteInfo = "Note OFF: " + noteName;

            writeToLog("🎹 " + noteInfo);

            if (midiIndicatorCallback)
                midiIndicatorCallback(noteInfo);

            stopNotePlayback();
        }
    }
}

void DjIaVstProcessor::startNotePlayback(int noteNumber)
{
    isNotePlaying = true;
    currentNoteNumber = noteNumber;
    readPosition = 0.0;
    writeToLog("▶️ Playback started from note " + juce::String(noteNumber));
}

void DjIaVstProcessor::stopNotePlayback()
{
    isNotePlaying = false;
    writeToLog("⏹️ Playback stopped");
}

//==============================================================================
// GÉNÉRATION AUDIO OPTIMISÉE
//==============================================================================

void DjIaVstProcessor::generateAudioOutput(juce::AudioBuffer<float> &buffer)
{
    // Pas de lock dans le cas critique - utilisation d'atomics
    if (bufferNumSamples == 0)
        return;

    const int numSamplesToProcess = buffer.getNumSamples();
    const int numOutputChannels = buffer.getNumChannels();

    // Calculer le ratio une seule fois
    if (hostSampleRate > 0.0 && audioSampleRate > 0.0)
    {
        playbackSpeedRatio = audioSampleRate / hostSampleRate;
    }

    double currentPosition = readPosition.load();
    const float volume = masterVolume.load();

    // Traitement optimisé pour stéréo
    float *leftOut = buffer.getWritePointer(0);
    float *rightOut = numOutputChannels > 1 ? buffer.getWritePointer(1) : leftOut;

    // Pointeurs vers les données source (lecture atomique des pointeurs)
    const float *leftIn = audioBuffer.getReadPointer(0);
    const float *rightIn = audioBuffer.getNumChannels() > 1 ? audioBuffer.getReadPointer(1) : leftIn;

    // Boucle optimisée
    for (int i = 0; i < numSamplesToProcess; ++i)
    {
        // Interpolation linéaire ultra-rapide
        float leftSample = interpolateLinear(leftIn, currentPosition, bufferNumSamples);
        float rightSample = interpolateLinear(rightIn, currentPosition, bufferNumSamples);

        // Application du volume et limitation
        leftOut[i] = juce::jlimit(-1.0f, 1.0f, leftSample * volume);
        rightOut[i] = juce::jlimit(-1.0f, 1.0f, rightSample * volume);

        // Avancer la position avec bouclage
        currentPosition += playbackSpeedRatio;
        if (currentPosition >= bufferNumSamples)
            currentPosition -= bufferNumSamples;
    }

    // Sauvegarder la position
    readPosition = currentPosition;
}

//==============================================================================
// INTERPOLATION LINÉAIRE ULTRA-RAPIDE
//==============================================================================

inline float DjIaVstProcessor::interpolateLinear(const float *buffer, double position, int bufferSize)
{
    if (bufferSize <= 0)
        return 0.0f;

    // Conversion rapide sans modulo
    int index = static_cast<int>(position);
    float fraction = static_cast<float>(position - index);

    // Wrapping simple
    if (index >= bufferSize)
        index -= bufferSize;
    if (index < 0)
        index += bufferSize;

    int nextIndex = index + 1;
    if (nextIndex >= bufferSize)
        nextIndex = 0;

    // Interpolation linéaire
    return buffer[index] + fraction * (buffer[nextIndex] - buffer[index]);
}

//==============================================================================
// CHARGEMENT AUDIO SIMPLIFIÉ
//==============================================================================

void DjIaVstProcessor::processIncomingAudio()
{
    if (!hasPendingAudioData.load())
        return;

    writeToLog("📥 Processing pending audio data...");

    // Si auto-load activé, charger immédiatement
    if (autoLoadEnabled.load())
    {
        writeToLog("🔄 Auto-loading sample...");
        loadAudioData();
    }
    else
    {
        writeToLog("⏸️ Sample ready - waiting for manual load");
        hasUnloadedSample = true; // Marquer qu'un sample attend
    }
}

void DjIaVstProcessor::loadAudioData()
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(
        std::make_unique<juce::MemoryInputStream>(pendingAudioData, false)));

    if (!reader)
    {
        writeToLog("❌ Failed to create audio reader");
        clearPendingAudio();
        return;
    }

    try
    {
        loadAudioFromReader(*reader);
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ Error loading audio: " + juce::String(e.what()));
        hasLoop = false;
    }

    clearPendingAudio();
    hasUnloadedSample = false;
}

void DjIaVstProcessor::setAutoLoadEnabled(bool enabled)
{
    autoLoadEnabled = enabled;
    writeToLog(enabled ? "🔄 Auto-load enabled" : "⏸️ Auto-load disabled - manual mode");
}

void DjIaVstProcessor::loadPendingSample()
{
    if (hasUnloadedSample.load())
    {
        writeToLog("📂 Loading sample manually...");
        loadAudioData();
    }
}

void DjIaVstProcessor::loadAudioFromReader(juce::AudioFormatReader &reader)
{
    // Récupérer les infos du fichier
    audioSampleRate = reader.sampleRate;
    int numSourceSamples = static_cast<int>(reader.lengthInSamples);
    int numSourceChannels = reader.numChannels;

    writeToLog("📊 Loading audio:");
    writeToLog("  Sample rate: " + juce::String(audioSampleRate) + " Hz");
    writeToLog("  Channels: " + juce::String(numSourceChannels));
    writeToLog("  Samples: " + juce::String(numSourceSamples));

    // Validation du sample rate
    if (audioSampleRate <= 0.0 || audioSampleRate > 192000.0)
    {
        writeToLog("⚠️ Invalid sample rate, defaulting to 44100 Hz");
        audioSampleRate = 44100.0;
    }

    // Lock pour modifier le buffer
    juce::ScopedLock lock(bufferLock);

    // Redimensionner le buffer principal
    audioBuffer.setSize(2, numSourceSamples, false, false, true);
    audioBuffer.clear();

    // Charger l'audio
    reader.read(&audioBuffer, 0, numSourceSamples, 0, true, numSourceChannels == 1);

    // Dupliquer mono vers stéréo si nécessaire
    if (numSourceChannels == 1 && audioBuffer.getNumChannels() > 1)
    {
        audioBuffer.copyFrom(1, 0, audioBuffer, 0, 0, numSourceSamples);
    }

    // Mettre à jour les variables atomiques
    bufferNumSamples = numSourceSamples;
    readPosition = 0.0;
    hasLoop = true;

    writeToLog("✅ Audio loaded successfully");
}

void DjIaVstProcessor::clearPendingAudio()
{
    const juce::ScopedLock lock(apiLock);
    pendingAudioData.reset();
    hasPendingAudioData = false;
}

//==============================================================================
// API ET CONTRÔLE
//==============================================================================

void DjIaVstProcessor::generateLoop(const DjIaClient::LoopRequest &request)
{
    try
    {
        writeToLog("🚀 Starting API call for loop generation...");

        auto response = apiClient.generateLoop(request);

        writeToLog("📦 API response received:");
        writeToLog("  Audio data size: " + juce::String(response.audioData.getSize()) + " bytes");
        writeToLog("  Sample rate: " + juce::String(response.sampleRate) + " Hz");

        // Stocker les données pour traitement
        {
            const juce::ScopedLock lock(apiLock);
            pendingAudioData = response.audioData;
            audioSampleRate = response.sampleRate;
            hasPendingAudioData = true;
        }

        writeToLog("✅ Audio data queued for processing");
    }
    catch (const std::exception &e)
    {
        writeToLog("❌ Error in generateLoop: " + juce::String(e.what()));
        hasPendingAudioData = false;
        hasLoop = false;
    }
}

void DjIaVstProcessor::startPlayback()
{
    isNotePlaying = true;
    readPosition = 0.0;
    writeToLog("▶️ Manual playback started");
}

void DjIaVstProcessor::stopPlayback()
{
    isNotePlaying = false;
    writeToLog("⏹️ Manual playback stopped");
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
    state.setProperty("bpm", currentBPM, nullptr);
    state.setProperty("key", currentKey, nullptr);
    state.setProperty("style", currentStyle, nullptr);
    state.setProperty("serverUrl", serverUrl, nullptr);
    state.setProperty("apiKey", apiKey, nullptr);
    state.setProperty("lastPrompt", lastPrompt, nullptr);
    state.setProperty("lastStyle", lastStyle, nullptr);
    state.setProperty("lastKey", lastKey, nullptr);
    state.setProperty("lastBpm", lastBpm, nullptr);
    state.setProperty("lastPresetIndex", lastPresetIndex, nullptr);
    state.setProperty("hostBpmEnabled", hostBpmEnabled, nullptr);

    writeToLog("💾 Saving state - URL: " + serverUrl + ", API Key: " + (apiKey.isNotEmpty() ? "***SET***" : "EMPTY"));

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DjIaVstProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName("DjIaVstState"))
    {
        juce::ValueTree state = juce::ValueTree::fromXml(*xml);
        currentBPM = state.getProperty("bpm", 126.0);
        currentKey = state.getProperty("key", "C minor");
        currentStyle = state.getProperty("style", "techno");
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

        writeToLog("📂 Loading state - URL: " + serverUrl + ", API Key: " + (apiKey.isNotEmpty() ? "***LOADED***" : "EMPTY"));
    }
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
    return 0.0; // Pas de BPM disponible
}

void DjIaVstProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    writeToLog("🎛️ Parameter changed: " + parameterID + " = " + juce::String(newValue));

    if (parameterID == "generate" && newValue > 0.5f)
    {
        writeToLog("🚀 Generate triggered from Device Panel");
        // Déclencher generation avec paramètres par défaut
        // Tu peux ajouter la logique de génération ici

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