#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// CONSTRUCTEUR ET DESTRUCTEUR
//==============================================================================

DjIaVstProcessor::DjIaVstProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // État de base
    hasLoop = false;
    isNotePlaying = false;
    readPosition = 0.0;
    masterVolume = 0.8f;
    currentNoteNumber = -1;

    // Sample rates - initialisées à 0, seront définies dans prepareToPlay
    hostSampleRate = 0.0;
    audioSampleRate = 0.0;
    playbackSpeedRatio = 1.0;

    // Système ping-pong
    pingBuffer.setSize(2, 0);
    pongBuffer.setSize(2, 0);
    activeBuffer = &pingBuffer;
    loadingBuffer = &pongBuffer;
    isBufferSwitchPending = false;

    // Audio en attente
    hasPendingAudioData = false;

    // Filtres
    resetFilters();

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

    // Redimensionner les buffers (10 secondes de cache)
    const juce::ScopedLock lock(bufferLock);
    int bufferSize = static_cast<int>(newSampleRate * 10.0);
    pingBuffer.setSize(2, bufferSize, false, true, true);
    pongBuffer.setSize(2, bufferSize, false, true, true);
    pingBuffer.clear();
    pongBuffer.clear();

    // Reset de l'état
    readPosition = 0.0;
    playbackSpeedRatio = 1.0;
    resetFilters();
}

void DjIaVstProcessor::releaseResources()
{
    writeToLog("=== RELEASE RESOURCES ===");
    const juce::ScopedLock lock(bufferLock);
    pingBuffer.setSize(0, 0);
    pongBuffer.setSize(0, 0);
    hasLoop = false;
}

//==============================================================================
// TRAITEMENT AUDIO PRINCIPAL
//==============================================================================

void DjIaVstProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Nettoyer les canaux de sortie inutilisés
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Traiter l'audio en attente
    processIncomingAudio();

    // Gérer le changement de buffer (système ping-pong)
    handleBufferSwitch();

    // Si pas de loop, sortir en silence
    if (!hasValidLoop())
    {
        buffer.clear();
        return;
    }

    // Traiter les messages MIDI
    processMidiMessages(midiMessages);

    // Générer l'audio si on joue
    if (isNotePlaying)
    {
        generateAudioOutput(buffer);
    }
    else
    {
        buffer.clear();
    }
}

//==============================================================================
// GESTION DES BUFFERS
//==============================================================================

void DjIaVstProcessor::handleBufferSwitch()
{
    if (isBufferSwitchPending)
    {
        const juce::ScopedLock lock(bufferLock);
        activeBuffer = (activeBuffer == &pingBuffer) ? &pongBuffer : &pingBuffer;
        isBufferSwitchPending = false;
        writeToLog("✅ Buffer switched for playback");
    }
}

bool DjIaVstProcessor::hasValidLoop()
{
    const juce::ScopedLock lock(bufferLock);
    return hasLoop && activeBuffer && activeBuffer->getNumSamples() > 0;
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
            writeToLog("🎹 MIDI Note On: " + juce::String(message.getNoteNumber()) +
                       " velocity=" + juce::String(message.getVelocity()));
            startNotePlayback(message.getNoteNumber());
        }
        else if (message.isNoteOff() && message.getNoteNumber() == currentNoteNumber)
        {
            writeToLog("🎹 MIDI Note Off: " + juce::String(message.getNoteNumber()));
            stopNotePlayback();
        }
    }
}

void DjIaVstProcessor::startNotePlayback(int noteNumber)
{
    isNotePlaying = true;
    currentNoteNumber = noteNumber;
    readPosition = 0.0;
    resetFilters();
    writeToLog("▶️ Playback started from note");
}

void DjIaVstProcessor::stopNotePlayback()
{
    isNotePlaying = false;
    writeToLog("⏹️ Playback stopped from note");
}

//==============================================================================
// GÉNÉRATION AUDIO
//==============================================================================

void DjIaVstProcessor::generateAudioOutput(juce::AudioBuffer<float> &buffer)
{
    const juce::ScopedLock lock(bufferLock);

    // Vérification de sécurité
    if (!activeBuffer || activeBuffer->getNumSamples() == 0)
    {
        writeToLog("⚠️ Empty activeBuffer during playback");
        buffer.clear();
        return;
    }

    // Calculer le ratio de lecture
    calculatePlaybackRatio();

    const int numOutChannels = buffer.getNumChannels();
    const int numInChannels = activeBuffer->getNumChannels();
    const int numSamplesToProcess = buffer.getNumSamples();
    const int sourceBufferSize = activeBuffer->getNumSamples();

    // Calculer le coefficient de filtrage anti-aliasing
    float filterCoeff = calculateFilterCoeff();

    // Traiter chaque canal
    for (int channel = 0; channel < numOutChannels; ++channel)
    {
        processAudioChannel(buffer, channel, numInChannels, numSamplesToProcess,
                            sourceBufferSize, filterCoeff);
    }
}

void DjIaVstProcessor::calculatePlaybackRatio()
{
    if (hostSampleRate > 0.0 && audioSampleRate > 0.0)
    {
        // CORRECTION : ratio pour convertir de l'audio vers l'host
        playbackSpeedRatio = audioSampleRate / hostSampleRate;

        // Log seulement si le ratio change significativement
        static double lastRatio = 0.0;
        if (std::abs(playbackSpeedRatio - lastRatio) > 0.01)
        {
            writeToLog("🎛️ Sample rates - Audio: " + juce::String(audioSampleRate) +
                       " Hz, Host: " + juce::String(hostSampleRate) +
                       " Hz, Ratio: " + juce::String(playbackSpeedRatio, 3));
            lastRatio = playbackSpeedRatio;
        }
    }
    else
    {
        writeToLog("⚠️ Invalid sample rates, using ratio 1.0");
        playbackSpeedRatio = 1.0;
    }
}

float DjIaVstProcessor::calculateFilterCoeff()
{
    // Coefficient de filtre adaptatif selon le ratio
    float filterCoeff = 0.7f; // Valeur par défaut conservative

    if (playbackSpeedRatio > 1.0f) // On ralentit = besoin de plus de filtrage
    {
        filterCoeff = juce::jmin(0.9f, 0.5f / static_cast<float>(playbackSpeedRatio));
    }
    else if (playbackSpeedRatio < 1.0f) // On accélère = moins de filtrage
    {
        filterCoeff = juce::jmax(0.3f, static_cast<float>(playbackSpeedRatio) * 0.8f);
    }

    return filterCoeff;
}

void DjIaVstProcessor::processAudioChannel(juce::AudioBuffer<float> &buffer,
                                           int channel, int numInChannels,
                                           int numSamplesToProcess, int sourceBufferSize,
                                           float filterCoeff)
{
    float *outputData = buffer.getWritePointer(channel);
    const float *inputData = activeBuffer->getReadPointer(juce::jmin(channel, numInChannels - 1));

    if (!inputData)
    {
        writeToLog("⚠️ Null input data for channel " + juce::String(channel));
        buffer.clear(channel, 0, numSamplesToProcess);
        return;
    }

    // Sélectionner les filtres pour ce canal
    float *prevInputs = (channel == 0) ? prevInputL : prevInputR;
    float *prevOutputs = (channel == 0) ? prevOutputL : prevOutputR;

    // Traiter chaque échantillon
    for (int sample = 0; sample < numSamplesToProcess; ++sample)
    {
        // Interpolation sécurisée
        float interpolatedValue = interpolateCubicSafe(inputData, readPosition, sourceBufferSize);

        // Filtrage anti-aliasing
        float filteredOutput;
        applyAntiAliasingFilter(interpolatedValue, filteredOutput, prevInputs, prevOutputs, filterCoeff);

        // Limitation et volume
        outputData[sample] = juce::jlimit(-1.0f, 1.0f, filteredOutput * masterVolume);

        // Avancer la position avec bouclage sécurisé
        advanceReadPosition(sourceBufferSize);
    }
}

void DjIaVstProcessor::advanceReadPosition(int sourceBufferSize)
{
    readPosition += playbackSpeedRatio;

    // Bouclage sécurisé
    if (sourceBufferSize > 0)
    {
        while (readPosition >= sourceBufferSize)
            readPosition -= sourceBufferSize;
        while (readPosition < 0.0)
            readPosition += sourceBufferSize;
    }
}

//==============================================================================
// INTERPOLATION ET FILTRAGE
//==============================================================================

float DjIaVstProcessor::interpolateCubicSafe(const float *buffer, double position, int bufferSize)
{
    if (!buffer || bufferSize <= 0)
        return 0.0f;

    // Pour des buffers très petits, interpolation linéaire
    if (bufferSize < 4)
    {
        int index = static_cast<int>(position);
        float fraction = static_cast<float>(position - index);

        index = juce::jlimit(0, bufferSize - 1, index);
        int nextIndex = juce::jlimit(0, bufferSize - 1, index + 1);

        return buffer[index] * (1.0f - fraction) + buffer[nextIndex] * fraction;
    }

    // Interpolation cubique sécurisée
    int index1 = static_cast<int>(position);
    float fraction = static_cast<float>(position - index1);

    // Assurer que l'index principal est valide
    index1 = index1 % bufferSize;
    if (index1 < 0)
        index1 += bufferSize;

    // Calculer les indices avec wrapping sécurisé
    int index0 = (index1 - 1 + bufferSize) % bufferSize;
    int index2 = (index1 + 1) % bufferSize;
    int index3 = (index1 + 2) % bufferSize;

    // Échantillons pour interpolation
    float y0 = buffer[index0];
    float y1 = buffer[index1];
    float y2 = buffer[index2];
    float y3 = buffer[index3];

    // Interpolation cubique Catmull-Rom
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;

    float result = ((c3 * fraction + c2) * fraction + c1) * fraction + c0;

    // Limitation pour éviter les explosions
    return juce::jlimit(-2.0f, 2.0f, result);
}

void DjIaVstProcessor::applyAntiAliasingFilter(float input, float &output,
                                               float *prevInputs, float *prevOutputs,
                                               float filterCoeff)
{
    // Filtre Butterworth passe-bas d'ordre 2 adaptatif
    float a0 = 1.0f;
    float a1 = -1.8f * filterCoeff;
    float a2 = 0.8f * filterCoeff * filterCoeff;
    float b0 = (1.0f - filterCoeff) * (1.0f - filterCoeff);
    float b1 = 2.0f * b0;
    float b2 = b0;

    // Application du filtre
    output = (b0 * input + b1 * prevInputs[0] + b2 * prevInputs[1] -
              a1 * prevOutputs[0] - a2 * prevOutputs[1]) /
             a0;

    // Mise à jour de l'historique
    prevInputs[1] = prevInputs[0];
    prevInputs[0] = input;
    prevOutputs[1] = prevOutputs[0];
    prevOutputs[0] = output;
}

void DjIaVstProcessor::resetFilters()
{
    for (int i = 0; i < 4; ++i)
    {
        prevInputL[i] = 0.0f;
        prevInputR[i] = 0.0f;
        prevOutputL[i] = 0.0f;
        prevOutputR[i] = 0.0f;
    }
}

//==============================================================================
// CHARGEMENT AUDIO
//==============================================================================

void DjIaVstProcessor::processIncomingAudio()
{
    if (!hasPendingAudioData)
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

    writeToLog("📊 Audio file info:");
    writeToLog("  Sample rate: " + juce::String(audioSampleRate) + " Hz");
    writeToLog("  Channels: " + juce::String(numSourceChannels));
    writeToLog("  Samples: " + juce::String(numSourceSamples));
    writeToLog("  Duration: " + juce::String(numSourceSamples / audioSampleRate, 2) + "s");

    // Validation du sample rate
    if (audioSampleRate <= 0.0 || audioSampleRate > 192000.0)
    {
        writeToLog("⚠️ Invalid sample rate, defaulting to 44100 Hz");
        audioSampleRate = 44100.0;
    }

    // Choisir le buffer de chargement
    loadingBuffer = (activeBuffer == &pingBuffer) ? &pongBuffer : &pingBuffer;
    int bufferChannels = juce::jmax(2, numSourceChannels);

    // Charger l'audio dans le buffer
    {
        const juce::ScopedLock lock(bufferLock);

        // Sauvegarder l'ancien buffer pour crossfade
        juce::AudioSampleBuffer oldBuffer;
        if (hasLoop && activeBuffer->getNumSamples() > 0)
        {
            oldBuffer = *activeBuffer;
        }

        // Redimensionner et charger
        loadingBuffer->setSize(bufferChannels, numSourceSamples, false, false, true);
        loadingBuffer->clear();

        reader.read(loadingBuffer, 0, numSourceSamples, 0, true,
                    (bufferChannels > numSourceChannels));

        // Dupliquer mono vers stéréo si nécessaire
        if (numSourceChannels == 1 && bufferChannels > 1)
        {
            loadingBuffer->copyFrom(1, 0, *loadingBuffer, 0, 0, numSourceSamples);
        }

        // Crossfade avec l'ancien buffer si nécessaire
        if (hasLoop && oldBuffer.getNumSamples() > 0)
        {
            applyCrossfade(*loadingBuffer, oldBuffer);
        }

        // Marquer pour changement de buffer
        isBufferSwitchPending = true;
    }

    // Réinitialiser l'état
    readPosition = 0.0;
    resetFilters();
    hasLoop = true;

    writeToLog("✅ Audio loaded successfully and ready for playback");
}

void DjIaVstProcessor::applyCrossfade(juce::AudioSampleBuffer &newBuffer,
                                      const juce::AudioSampleBuffer &oldBuffer)
{
    int fadeLength = juce::jmin(2048, newBuffer.getNumSamples(), oldBuffer.getNumSamples());

    for (int ch = 0; ch < newBuffer.getNumChannels(); ++ch)
    {
        int validOldChannel = juce::jmin(ch, oldBuffer.getNumChannels() - 1);

        for (int i = 0; i < fadeLength; ++i)
        {
            float fadeIn = static_cast<float>(i) / fadeLength;
            float fadeOut = 1.0f - fadeIn;

            float newSample = newBuffer.getSample(ch, i);
            float oldSample = oldBuffer.getSample(validOldChannel, i);

            newBuffer.setSample(ch, i, newSample * fadeIn + oldSample * fadeOut);
        }
    }

    writeToLog("🔄 Applied crossfade over " + juce::String(fadeLength) + " samples");
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
        writeToLog("  Reported sample rate: " + juce::String(response.sampleRate) + " Hz");

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
        const juce::ScopedLock lock(apiLock);
        hasPendingAudioData = false;
        hasLoop = false;
    }
}

void DjIaVstProcessor::startPlayback()
{
    const juce::ScopedLock lock(bufferLock);
    isNotePlaying = true;
    readPosition = 0.0;
    resetFilters();
    writeToLog("▶️ Manual playback started");
}

void DjIaVstProcessor::stopPlayback()
{
    const juce::ScopedLock lock(bufferLock);
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