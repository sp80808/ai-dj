#include "PluginProcessor.h"
#include "PluginEditor.h"

DjIaVstProcessor::DjIaVstProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialisation des variables clés
    hasLoop = false;
    isNotePlaying = false;
    readPosition = 0.0;
    masterVolume = 0.8f;
    currentNoteNumber = -1;
    hasPendingAudioData = false;
    sampleRate = 44100.0;      // Default, sera mis à jour dans prepareToPlay
    audioSampleRate = 44100.0; // Default, sera mis à jour à partir du fichier audio
    playbackSpeedRatio = 1.0;  // Ratio par défaut, sera recalculé si nécessaire

    // Initialisation du système ping-pong
    pingBuffer.setSize(2, 0); // Taille initiale, sera redimensionnée plus tard
    pongBuffer.setSize(2, 0);
    activeBuffer = &pingBuffer;  // Buffer actif par défaut
    loadingBuffer = &pongBuffer; // Buffer de chargement par défaut
    isBufferSwitchPending = false;

    // Réinitialisation des filtres
    resetFilters();

    writeToLog("DjIaVstProcessor initialized with enhanced audio processing");
}

DjIaVstProcessor::~DjIaVstProcessor()
{
    // Nettoyage si nécessaire
}

void DjIaVstProcessor::prepareToPlay(double newSampleRate, int /*samplesPerBlock*/)
{
    this->sampleRate = newSampleRate; // Sauvegarde de la fréquence d'échantillonnage de la DAW
    readPosition = 0;
    playbackSpeedRatio = 1.0; // Sera recalculé plus tard

    writeToLog("prepareToPlay - DAW sample rate: " + juce::String(this->sampleRate) + " Hz");

    // Initialisation des buffers ping-pong avec une taille prudente
    const juce::ScopedLock lock(bufferLock);
    pingBuffer.setSize(2, static_cast<int>(newSampleRate * 10.0), false, true, true); // 10 secondes de buffer
    pongBuffer.setSize(2, static_cast<int>(newSampleRate * 10.0), false, true, true);
    pingBuffer.clear();
    pongBuffer.clear();

    // Réinitialisation des filtres
    resetFilters();
}

void DjIaVstProcessor::releaseResources()
{
    // Libération des ressources
    const juce::ScopedLock lock(bufferLock);
    pingBuffer.setSize(0, 0);
    pongBuffer.setSize(0, 0);
    hasLoop = false;
}

void DjIaVstProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Nettoyer les canaux de sortie qui n'ont pas de données d'entrée
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Traiter les données audio en attente si nécessaire
    processIncomingAudio();

    // Vérifier si on doit changer de buffer (système ping-pong)
    if (isBufferSwitchPending)
    {
        const juce::ScopedLock lock(bufferLock);
        activeBuffer = (activeBuffer == &pingBuffer) ? &pongBuffer : &pingBuffer;
        isBufferSwitchPending = false;
        writeToLog("Switched active buffer for playback");
    }

    // Vérifier si nous avons une boucle à jouer
    if (!hasLoop || activeBuffer->getNumSamples() == 0)
    {
        buffer.clear();
        return;
    }

    // Traiter le MIDI
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            writeToLog("MIDI Note On: " + juce::String(message.getNoteNumber()) +
                       " velocity=" + juce::String(message.getVelocity()));
            isNotePlaying = true;
            currentNoteNumber = message.getNoteNumber();
            readPosition = 0; // Réinitialiser la position de lecture pour une nouvelle note
            resetFilters();   // Réinitialiser les filtres pour éviter les artefacts
        }
        else if (message.isNoteOff() && message.getNoteNumber() == currentNoteNumber)
        {
            writeToLog("MIDI Note Off: " + juce::String(message.getNoteNumber()));
            isNotePlaying = false;
        }
    }

    if (isNotePlaying)
    {
        const juce::ScopedLock lock(bufferLock);

        // Vérifier à nouveau la taille du buffer après avoir acquis le lock
        if (activeBuffer->getNumChannels() == 0 || activeBuffer->getNumSamples() == 0)
        {
            writeToLog("Warning: Empty activeBuffer while trying to play.");
            buffer.clear();
            return;
        }

        // Calculer le ratio de vitesse de lecture en fonction des fréquences d'échantillonnage
        if (this->sampleRate > 0.001 && this->audioSampleRate > 0.001)
        {
            playbackSpeedRatio = this->audioSampleRate / this->sampleRate;
        }
        else
        {
            // En cas d'erreur, utiliser le ratio par défaut
            writeToLog("Warning: Invalid sample rate(s). Using default playback speed ratio 1.0.");
            playbackSpeedRatio = 1.0;
        }

        const int numOutChannels = buffer.getNumChannels();
        const int numInChannels = activeBuffer->getNumChannels();
        const int numSamplesToProcess = buffer.getNumSamples();
        const int sourceBufferSize = activeBuffer->getNumSamples();

        // Calculer le coefficient du filtre en fonction du ratio de vitesse
        float filterCoeff = juce::jmin(0.7f, juce::jmax(0.1f,
                                                        static_cast<float>(playbackSpeedRatio * 0.25f)));

        for (int channel = 0; channel < numOutChannels; ++channel)
        {
            float *outputData = buffer.getWritePointer(channel);
            // Utiliser le canal source 0 pour la sortie mono si activeBuffer est mono, ou le canal correspondant
            const float *inputData = activeBuffer->getReadPointer(juce::jmin(channel, numInChannels - 1));

            if (inputData == nullptr)
            {
                writeToLog("Warning: Null input data for channel " + juce::String(channel));
                buffer.clear(channel, 0, numSamplesToProcess);
                continue;
            }

            // Sélectionner les tableaux de filtres en fonction du canal
            float *prevInputs = (channel == 0) ? prevInputL : prevInputR;
            float *prevOutputs = (channel == 0) ? prevOutputL : prevOutputR;

            for (int sample = 0; sample < numSamplesToProcess; ++sample)
            {
                // Interpolation cubique améliorée
                float interpolatedValue = interpolateCubic(inputData, readPosition, sourceBufferSize);

                // Appliquer le filtre anti-aliasing amélioré
                applyFilter(interpolatedValue, outputData[sample], prevInputs, prevOutputs, filterCoeff);

                // Appliquer le volume
                outputData[sample] *= masterVolume;

                // Avancer la position de lecture
                readPosition += playbackSpeedRatio;

                // S'assurer que readPosition est toujours dans les limites du buffer
                while (readPosition >= sourceBufferSize)
                    readPosition -= sourceBufferSize;
            }
        }
    }
    else
    {
        buffer.clear();
    }
}

float DjIaVstProcessor::interpolateCubic(const float *buffer, double position, int bufferSize)
{
    int index1 = static_cast<int>(position);
    float fraction = static_cast<float>(position - index1);

    int index0 = index1 - 1;
    int index2 = index1 + 1;
    int index3 = index1 + 2;

    // Gérer les conditions aux limites par wrapping
    if (index0 < 0)
        index0 += bufferSize;
    if (index2 >= bufferSize)
        index2 -= bufferSize;
    if (index3 >= bufferSize)
        index3 -= bufferSize;

    // S'assurer que tous les indices sont valides après le wrapping
    if (index0 >= 0 && index0 < bufferSize &&
        index1 >= 0 && index1 < bufferSize &&
        index2 >= 0 && index2 < bufferSize &&
        index3 >= 0 && index3 < bufferSize)
    {
        float y0 = buffer[index0];
        float y1 = buffer[index1];
        float y2 = buffer[index2];
        float y3 = buffer[index3];

        // Interpolation cubique Catmull-Rom correcte
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;

        return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
    }
    else
    {
        // Fallback si les indices sont hors limites (ne devrait pas arriver)
        return 0.0f;
    }
}

void DjIaVstProcessor::applyFilter(float &input, float &output, float *prevInputs, float *prevOutputs, float filterCoeff)
{
    // Filtre Butterworth d'ordre 2
    float a0 = 1.0f;
    float a1 = -1.85f * (1.0f - filterCoeff);
    float a2 = 0.85f * (1.0f - filterCoeff);
    float b0 = filterCoeff * filterCoeff;
    float b1 = 2.0f * b0;
    float b2 = b0;

    // Appliquer le filtre
    output = (b0 * input + b1 * prevInputs[0] + b2 * prevInputs[1] -
              a1 * prevOutputs[0] - a2 * prevOutputs[1]) /
             a0;

    // Mettre à jour les valeurs précédentes
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

void DjIaVstProcessor::processIncomingAudio()
{
    if (hasPendingAudioData)
    {
        writeToLog("Processing pending audio data...");
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(
            std::make_unique<juce::MemoryInputStream>(pendingAudioData, false)));

        if (reader != nullptr)
        {
            try
            {
                double reportedSampleRateByServer = this->audioSampleRate; // Valeur de la réponse API
                double actualSampleRateInWav = reader->sampleRate;

                writeToLog("Sample rate from API response: " + juce::String(reportedSampleRateByServer) + " Hz");
                writeToLog("Actual sample rate in WAV data: " + juce::String(actualSampleRateInWav) + " Hz");
                writeToLog("DAW sample rate: " + juce::String(this->sampleRate) + " Hz");

                if (std::abs(reportedSampleRateByServer - actualSampleRateInWav) > 0.1 && reportedSampleRateByServer > 0)
                {
                    writeToLog("WARNING: Sample rate mismatch. Using WAV metadata.");
                }

                // Utiliser la fréquence d'échantillonnage du fichier audio
                this->audioSampleRate = actualSampleRateInWav;
                if (this->audioSampleRate < 1.0)
                {
                    writeToLog("ERROR: Invalid sample rate. Defaulting to 44100 Hz.");
                    this->audioSampleRate = 44100.0;
                }

                int numSourceSamples = static_cast<int>(reader->lengthInSamples);
                int numSourceChannels = reader->numChannels;

                writeToLog("WAV info: " + juce::String(numSourceChannels) + " channels, " +
                           juce::String(numSourceSamples) + " samples, " +
                           juce::String(this->audioSampleRate) + " Hz");

                // Choisir le buffer de chargement (opposé au buffer actif)
                loadingBuffer = (activeBuffer == &pingBuffer) ? &pongBuffer : &pingBuffer;

                // Utiliser au moins 2 canaux pour le buffer si la sortie est stéréo
                int bufferChannels = juce::jmax(2, numSourceChannels);

                // Protéger l'accès aux buffers
                {
                    const juce::ScopedLock lock(bufferLock);

                    // Conserver l'ancien buffer pour le crossfade si nécessaire
                    juce::AudioSampleBuffer oldBuffer;
                    if (hasLoop)
                    {
                        oldBuffer = *activeBuffer; // Sauvegarder l'ancien buffer
                    }

                    // Redimensionner et nettoyer le buffer de chargement
                    loadingBuffer->setSize(bufferChannels, numSourceSamples, false, false, true);
                    loadingBuffer->clear();

                    // Lire les données audio dans le buffer de chargement
                    reader->read(loadingBuffer, 0, numSourceSamples, 0, true, (bufferChannels > numSourceChannels));

                    // Si la source est mono et le buffer est stéréo, copier le canal 0 vers le canal 1
                    if (numSourceChannels == 1 && bufferChannels > 1)
                    {
                        loadingBuffer->copyFrom(1, 0, *loadingBuffer, 0, 0, numSourceSamples);
                    }

                    // Appliquer un crossfade si nécessaire
                    if (hasLoop && oldBuffer.getNumSamples() > 0)
                    {
                        crossfadeBuffers(*loadingBuffer, oldBuffer);
                    }

                    // Marquer le changement de buffer comme en attente
                    isBufferSwitchPending = true;
                }

                readPosition = 0.0;
                resetFilters();
                hasLoop = true;

                writeToLog("Audio loaded successfully into buffer. Ready for playback.");
            }
            catch (const std::exception &e)
            {
                writeToLog("Error processing audio: " + juce::String(e.what()));
                hasLoop = false;
            }
        }
        else
        {
            writeToLog("Failed to create audio reader from pendingAudioData.");
            hasLoop = false;
        }

        // Nettoyer les données en attente
        const juce::ScopedLock lock(apiLock);
        pendingAudioData.reset();
        hasPendingAudioData = false;
        writeToLog("Pending audio data processed and cleared.");
    }
}

void DjIaVstProcessor::crossfadeBuffers(juce::AudioSampleBuffer &newBuffer, const juce::AudioSampleBuffer &oldBuffer)
{
    // Appliquer un crossfade entre l'ancien et le nouveau buffer
    int fadeLength = juce::jmin(2048, newBuffer.getNumSamples(), oldBuffer.getNumSamples());

    for (int ch = 0; ch < newBuffer.getNumChannels(); ++ch)
    {
        int validChannel = juce::jmin(ch, oldBuffer.getNumChannels() - 1);

        for (int i = 0; i < fadeLength; ++i)
        {
            float fadeIn = static_cast<float>(i) / fadeLength;
            float fadeOut = 1.0f - fadeIn;

            if (i < oldBuffer.getNumSamples())
            {
                float newSample = newBuffer.getSample(ch, i);
                float oldSample = oldBuffer.getSample(validChannel, i % oldBuffer.getNumSamples());
                newBuffer.setSample(ch, i, newSample * fadeIn + oldSample * fadeOut);
            }
        }
    }

    writeToLog("Applied crossfade between buffers over " + juce::String(fadeLength) + " samples");
}

void DjIaVstProcessor::generateLoop(const DjIaClient::LoopRequest &request)
{
    try
    {
        writeToLog("Starting generateLoop (API call)...");

        // Appel à l'API
        auto response = apiClient.generateLoop(request);

        writeToLog("API response received. Audio data size: " + juce::String(response.audioData.getSize()));
        writeToLog("Reported sample rate from server: " + juce::String(response.sampleRate) + " Hz");

        {
            const juce::ScopedLock lock(apiLock);
            pendingAudioData = response.audioData;
            this->audioSampleRate = response.sampleRate;
            hasPendingAudioData = true;
        }

        writeToLog("Audio data queued for processing.");
    }
    catch (const std::exception &e)
    {
        writeToLog("Error in generateLoop: " + juce::String(e.what()));
        const juce::ScopedLock lock(apiLock);
        hasPendingAudioData = false;
        hasLoop = false;
    }
    catch (...)
    {
        writeToLog("Unknown error in generateLoop");
        const juce::ScopedLock lock(apiLock);
        hasPendingAudioData = false;
        hasLoop = false;
    }
}

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
    // L'API key est sensible, ne pas la sauvegarder

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DjIaVstProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName("DjIaVstState"))
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

void DjIaVstProcessor::setApiKey(const juce::String &key)
{
    apiKey = key;
    apiClient = DjIaClient(apiKey, serverUrl);
}

void DjIaVstProcessor::setServerUrl(const juce::String &url)
{
    serverUrl = url;
    writeToLog("Setting Server URL: [" + url + "]");
    apiClient = DjIaClient(apiKey, serverUrl);
}

void DjIaVstProcessor::startPlayback()
{
    const juce::ScopedLock lock(bufferLock);
    isNotePlaying = true;
    readPosition = 0;
    resetFilters();
    writeToLog("Playback started. isNotePlaying=true, readPosition=0");
}

void DjIaVstProcessor::stopPlayback()
{
    const juce::ScopedLock lock(bufferLock);
    isNotePlaying = false;
    writeToLog("Playback stopped. isNotePlaying=false");
}