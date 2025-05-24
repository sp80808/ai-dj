#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// CONSTRUCTEUR ET DESTRUCTEUR
//==============================================================================

DjIaVstProcessor::DjIaVstProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    writeToLog("=== DJ-IA VST INITIALIZED ===");
}

DjIaVstProcessor::~DjIaVstProcessor()
{
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
    // Nettoyer les canaux inutilisés
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Traiter l'audio en attente (pas dans le thread audio critique)
    if (hasPendingAudioData.load())
    {
        processIncomingAudio();
    }

    // Si pas de loop, sortir en silence
    if (!hasLoop.load() || bufferNumSamples == 0)
    {
        buffer.clear();
        return;
    }

    // Traiter les messages MIDI
    processMidiMessages(midiMessages);

    // Générer l'audio si on joue
    if (isNotePlaying.load())
    {
        generateAudioOutput(buffer);
    }
    else
    {
        buffer.clear();
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
            startNotePlayback(message.getNoteNumber());
        }
        else if (message.isNoteOff() && message.getNoteNumber() == currentNoteNumber.load())
        {
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

        juce::String newServerUrl = state.getProperty("serverUrl", "http://localhost:8000").toString();
        if (newServerUrl != serverUrl)
        {
            setServerUrl(newServerUrl);
        }
    }
}