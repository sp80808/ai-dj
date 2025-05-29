#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AudioAnalyzer.h"

juce::AudioProcessor::BusesProperties DjIaVstProcessor::createBusLayout()
{
	auto layout = juce::AudioProcessor::BusesProperties();
	layout = layout.withOutput("Main", juce::AudioChannelSet::stereo(), true);
	for (int i = 0; i < MAX_TRACKS; ++i)
	{
		layout = layout.withOutput("Track " + juce::String(i + 1),
			juce::AudioChannelSet::stereo(), false);
	}
	return layout;
}

DjIaVstProcessor::DjIaVstProcessor()
	: AudioProcessor(createBusLayout()),
	parameters(*this, nullptr, "Parameters", {
												 std::make_unique<juce::AudioParameterBool>("generate", "Generate Loop", false),
												 std::make_unique<juce::AudioParameterBool>("play", "Play Loop", false),
												 std::make_unique<juce::AudioParameterBool>("autoload", "Auto-Load", true),
												 std::make_unique<juce::AudioParameterFloat>("bpm", "BPM", 60.0f, 200.0f, 126.0f),
		})
{
	loadParameters();
	initTracks();
	initDummySynth();
	startTimerHz(30);
}

void DjIaVstProcessor::initDummySynth()
{
	for (int i = 0; i < 4; ++i)
		synth.addVoice(new DummyVoice());
	synth.addSound(new DummySound());
}

void DjIaVstProcessor::initTracks()
{
	selectedTrackId = trackManager.createTrack("Track 1");
	individualOutputBuffers.resize(MAX_TRACKS);
	for (auto& buffer : individualOutputBuffers)
	{
		buffer.setSize(2, 512);
	}
}

void DjIaVstProcessor::loadParameters()
{
	generateParam = parameters.getRawParameterValue("generate");
	playParam = parameters.getRawParameterValue("play");
	autoLoadParam = parameters.getRawParameterValue("autoload");

	parameters.addParameterListener("generate", this);
	parameters.addParameterListener("play", this);
	parameters.addParameterListener("autoload", this);
}

DjIaVstProcessor::~DjIaVstProcessor()
{
	stopTimer();
	try
	{
		cleanProcessor();
	}
	catch (const std::exception& e)
	{
		std::cout << "Error: " << e.what() << std::endl;
	}
}

void DjIaVstProcessor::cleanProcessor()
{
	parameters.removeParameterListener("generate", this);
	parameters.removeParameterListener("play", this);
	parameters.removeParameterListener("autoload", this);
	isNotePlaying = false;
	hasPendingAudioData = false;
	hasUnloadedSample = false;
	midiIndicatorCallback = nullptr;
	individualOutputBuffers.clear();
	synth.clearVoices();
	synth.clearSounds();
}

void DjIaVstProcessor::timerCallback()
{
	if (!needsUIUpdate.load())
		return;
	if (onUIUpdateNeeded)
	{
		onUIUpdateNeeded();
	}
	needsUIUpdate = false;
}

void DjIaVstProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
	hostSampleRate = newSampleRate;
	synth.setCurrentPlaybackSampleRate(newSampleRate);
	for (auto& buffer : individualOutputBuffers)
	{
		buffer.setSize(2, samplesPerBlock);
		buffer.clear();
	}
	masterEQ.prepare(newSampleRate, samplesPerBlock);
}

void DjIaVstProcessor::releaseResources()
{
	for (auto& buffer : individualOutputBuffers)
	{
		buffer.setSize(0, 0);
	}
}

bool DjIaVstProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
	{
		return false;
	}
	for (int i = 1; i < layouts.outputBuses.size(); ++i)
	{
		if (!layouts.outputBuses[i].isDisabled() &&
			layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
		{
			return false;
		}
	}
	return true;
}

void DjIaVstProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	checkAndSwapStagingBuffers();
	for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
		buffer.clear(i, 0, buffer.getNumSamples());

	bool hostIsPlaying = false;
	auto playHead = getPlayHead();

	double hostBpm = 126.0;
	double hostPpqPosition = 0.0;

	if (playHead)
	{
		getDawInformations(playHead, hostIsPlaying, hostBpm, hostPpqPosition);
	}
	processMidiMessages(midiMessages, hostIsPlaying, hostBpm);
	if (hasPendingAudioData.load())
	{
		processIncomingAudio();
	}

	resizeIndividualsBuffers(buffer);
	clearOutputBuffers(buffer);

	auto mainOutput = getBusBuffer(buffer, false, 0);
	mainOutput.clear();

	updateTimeStretchRatios(hostBpm);

	trackManager.renderAllTracks(mainOutput, individualOutputBuffers, hostSampleRate);

	copyTracksToIndividualOutputs(buffer);
	applyMasterEffects(mainOutput);

	checkIfUIUpdateNeeded(midiMessages);
}

void DjIaVstProcessor::checkIfUIUpdateNeeded(juce::MidiBuffer& midiMessages)
{
	bool anyTrackPlaying = false;
	auto trackIds = trackManager.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = trackManager.getTrack(trackId);
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

void DjIaVstProcessor::applyMasterEffects(juce::AudioSampleBuffer& mainOutput)
{
	updateMasterEQ();
	masterEQ.processBlock(mainOutput);
	float masterVol = masterVolume.load();
	mainOutput.applyGain(masterVol);
	float masterPanVal = masterPan.load();

	if (mainOutput.getNumChannels() >= 2 && std::abs(masterPanVal) > 0.01f)
	{
		if (masterPanVal < 0.0f)
		{
			mainOutput.applyGain(1, 0, mainOutput.getNumSamples(), 1.0f + masterPanVal);
		}
		else
		{
			mainOutput.applyGain(0, 0, mainOutput.getNumSamples(), 1.0f - masterPanVal);
		}
	}
}

void DjIaVstProcessor::copyTracksToIndividualOutputs(juce::AudioSampleBuffer& buffer)
{
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

void DjIaVstProcessor::clearOutputBuffers(juce::AudioSampleBuffer& buffer)
{
	for (int busIndex = 0; busIndex < getTotalNumOutputChannels() / 2; ++busIndex)
	{
		if (busIndex * 2 + 1 < getTotalNumOutputChannels() && busIndex <= MAX_TRACKS)
		{
			auto busBuffer = getBusBuffer(buffer, false, busIndex);
			busBuffer.clear();
		}
	}
}

void DjIaVstProcessor::resizeIndividualsBuffers(juce::AudioSampleBuffer& buffer)
{
	for (auto& indivBuffer : individualOutputBuffers)
	{
		if (indivBuffer.getNumSamples() != buffer.getNumSamples())
		{
			indivBuffer.setSize(2, buffer.getNumSamples(), false, false, true);
		}
		indivBuffer.clear();
	}
}

void DjIaVstProcessor::getDawInformations(juce::AudioPlayHead* playHead, bool& hostIsPlaying, double& hostBpm, double& hostPpqPosition)
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

void DjIaVstProcessor::updateMasterEQ()
{
	masterEQ.setHighGain(masterHighEQ.load());
	masterEQ.setMidGain(masterMidEQ.load());
	masterEQ.setLowGain(masterLowEQ.load());
}

void DjIaVstProcessor::processMidiMessages(juce::MidiBuffer& midiMessages, bool hostIsPlaying, double hostBpm)
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
			playTrack(message, hostBpm);
		}
		else if (message.isNoteOff())
		{
			int noteNumber = message.getNoteNumber();
			stopNotePlaybackForTrack(noteNumber);
		}
	}
}

void DjIaVstProcessor::playTrack(const juce::MidiMessage& message, double hostBpm)
{
	int noteNumber = message.getNoteNumber();
	juce::String noteName = juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 3);
	bool trackFound = false;
	auto trackIds = trackManager.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = trackManager.getTrack(trackId);
		if (track && track->midiNote == noteNumber)
		{
			if (trackId == trackIdWaitingForLoad)
			{
				correctMidiNoteReceived = true;
			}
			if (track->numSamples > 0)
			{
				startNotePlaybackForTrack(trackId, noteNumber, hostBpm);
				trackFound = true;

				if (midiIndicatorCallback && track->isPlaying)
				{
					midiIndicatorCallback(">> " + track->trackName + " (" + noteName + ") - BPM:" + juce::String(hostBpm, 0));
				}
			}
			break;
		}
	}
}

void DjIaVstProcessor::updateTimeStretchRatios(double hostBpm)
{
	auto trackIds = trackManager.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = trackManager.getTrack(trackId);
		if (!track)
			continue;

		double ratio = 1.0;

		switch (track->timeStretchMode)
		{
		case 1:
		case 3:
			ratio = 1.0;
			break;

		case 2:
		case 4:
			if (track->originalBpm > 0.0f && hostBpm > 0.0)
			{
				double hostRatio = hostBpm / track->originalBpm;
				double manualAdjust = track->bpmOffset / track->originalBpm;
				ratio = hostRatio + manualAdjust;
			}
			break;
		}

		ratio = juce::jlimit(0.25, 4.0, ratio);
		track->cachedPlaybackRatio = ratio;
	}
}

void DjIaVstProcessor::startNotePlaybackForTrack(const juce::String& trackId, int noteNumber, double hostBpm)
{
	TrackData* track = trackManager.getTrack(trackId);
	if (!track || track->numSamples == 0)
		return;
	if (track->isArmedToStop.load()) {
		track->isPlaying = false;
		track->isArmedToStop = false;
		if (onUIUpdateNeeded)
			onUIUpdateNeeded();
		return;
	}
	if (!track->isArmed.load())
	{
		return;
	}
	if (track->isPlaying.load())
	{
		return;
	}

	track->readPosition = 0.0;
	track->setPlaying(true);

	playingTracks[noteNumber] = trackId;
}

void DjIaVstProcessor::stopNotePlaybackForTrack(int noteNumber)
{
	auto it = playingTracks.find(noteNumber);
	if (it != playingTracks.end())
	{
		TrackData* track = trackManager.getTrack(it->second);
		if (track)
		{
			track->isPlaying = false;
		}
		playingTracks.erase(it);
	}
}

juce::String DjIaVstProcessor::createNewTrack(const juce::String& name)
{
	auto trackIds = trackManager.getAllTrackIds();
	if (trackIds.size() >= MAX_TRACKS)
	{
		throw std::runtime_error("Maximum number of tracks reached (" + std::to_string(MAX_TRACKS) + ")");
	}

	juce::String trackId = trackManager.createTrack(name);
	return trackId;
}

void DjIaVstProcessor::reorderTracks(const juce::String& fromTrackId, const juce::String& toTrackId)
{
	trackManager.reorderTracks(fromTrackId, toTrackId);
}

void DjIaVstProcessor::deleteTrack(const juce::String& trackId)
{
	TrackData* trackToDelete = trackManager.getTrack(trackId);
	if (!trackToDelete)
		return;

	juce::String trackName = trackToDelete->trackName;

	juce::MessageManager::callAsync([this, trackId, trackName]()
		{ juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::QuestionIcon)
			.withTitle("Delete Track")
			.withMessage("Are you sure you want to delete '" + trackName + "'?\n\n"
				"This action cannot be undone.")
			.withButton("Delete")
			.withButton("Cancel"),
			[this, trackId](int result)
			{
				if (result == 1)
				{
					performTrackDeletion(trackId);
				}
			}); });
}

void DjIaVstProcessor::performTrackDeletion(const juce::String& trackId)
{
	auto trackIds = trackManager.getAllTrackIds();
	int deletedTrackIndex = -1;

	for (int i = 0; i < trackIds.size(); ++i)
	{
		if (trackIds[i] == trackId)
		{
			deletedTrackIndex = i;
			break;
		}
	}

	if (trackId == selectedTrackId)
	{
		if (trackIds.size() > 1)
		{
			if (deletedTrackIndex < trackIds.size() - 1)
			{
				selectedTrackId = trackIds[deletedTrackIndex + 1];
			}
			else if (deletedTrackIndex > 0)
			{
				selectedTrackId = trackIds[deletedTrackIndex - 1];
			}
		}
		else
		{
			selectedTrackId = trackManager.createTrack("Track");
		}
	}
	trackManager.removeTrack(trackId);
	reassignTrackOutputsAndMidi();

	juce::MessageManager::callAsync([this]()
		{
			if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
			{
				editor->refreshTrackComponents();
			} });
}

void DjIaVstProcessor::reassignTrackOutputsAndMidi()
{
	auto trackIds = trackManager.getAllTrackIds();
	for (int i = 0; i < trackIds.size(); ++i)
	{
		TrackData* track = trackManager.getTrack(trackIds[i]);
		if (track)
		{
			track->midiNote = 60 + i;
		}
	}
}

void DjIaVstProcessor::selectTrack(const juce::String& trackId)
{
	if (trackManager.getTrack(trackId))
	{
		selectedTrackId = trackId;
	}
}

void DjIaVstProcessor::generateLoop(const DjIaClient::LoopRequest& request, const juce::String& targetTrackId)
{
	juce::String trackId = targetTrackId.isEmpty() ? selectedTrackId : targetTrackId;

	try
	{
		auto response = apiClient.generateLoop(request);
		{
			const juce::ScopedLock lock(apiLock);
			pendingTrackId = trackId;
			pendingAudioData = response.audioData;
			audioSampleRate = response.sampleRate;
			hasPendingAudioData = true;
			waitingForMidiToLoad = true;
			trackIdWaitingForLoad = trackId;
			correctMidiNoteReceived = false;
		}
		if (TrackData* track = trackManager.getTrack(trackId))
		{
			track->prompt = request.prompt;
			track->bpm = request.bpm;

			juce::String stems;
			for (const auto& stem : request.preferredStems)
			{
				if (!stems.isEmpty())
					stems += ", ";
				stems += stem;
			}
			track->stems = stems;
		}
	}
	catch (const std::exception& e)
	{
		hasPendingAudioData = false;
		waitingForMidiToLoad = false;
		trackIdWaitingForLoad.clear();
		correctMidiNoteReceived = false;
		juce::MessageManager::callAsync([this, error = juce::String(e.what())]()
			{
				if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
				{
					editor->statusLabel.setText("Error: " + error, juce::dontSendNotification);
				} });
	}
}

void DjIaVstProcessor::processIncomingAudio()
{
	if (!hasPendingAudioData.load() || pendingTrackId.isEmpty())
		return;

	if (waitingForMidiToLoad.load() && !correctMidiNoteReceived.load())
	{
		hasUnloadedSample = true;
		return;
	}
	juce::MessageManager::callAsync([this]()
		{
			if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor())) {
				editor->statusLabel.setText("Loading sample...", juce::dontSendNotification);
			} });
			juce::Thread::launch([this, trackId = pendingTrackId, audioData = pendingAudioData]()
				{ loadAudioDataAsync(trackId, audioData); });

			clearPendingAudio();
			hasUnloadedSample = false;
			waitingForMidiToLoad = false;
			correctMidiNoteReceived = false;
			trackIdWaitingForLoad.clear();
}

void DjIaVstProcessor::checkAndSwapStagingBuffers()
{
	auto trackIds = trackManager.getAllTrackIds();

	for (const auto& trackId : trackIds)
	{
		TrackData* track = trackManager.getTrack(trackId);
		if (!track)
			continue;
		if (track->swapRequested.exchange(false))
		{
			if (track->hasStagingData.load())
			{
				performAtomicSwap(track, trackId);
			}
		}
	}
}

void DjIaVstProcessor::performAtomicSwap(TrackData* track, const juce::String& trackId)
{
	std::swap(track->audioBuffer, track->stagingBuffer);

	track->numSamples = track->stagingNumSamples.load();
	track->sampleRate = track->stagingSampleRate.load();
	track->originalBpm = track->stagingOriginalBpm;

	double sampleDuration = track->numSamples / track->sampleRate;
	if (sampleDuration <= 8.0)
	{
		track->loopStart = 0.0;
		track->loopEnd = sampleDuration;
	}
	else
	{
		double beatDuration = 60.0 / track->originalBpm;
		double fourBars = beatDuration * 16.0;
		track->loopStart = 0.0;
		track->loopEnd = std::min(fourBars, sampleDuration);
	}

	track->readPosition = 0.0;

	track->hasStagingData = false;
	track->stagingBuffer.setSize(0, 0);
	juce::MessageManager::callAsync([this, trackId]()
		{ updateWaveformDisplay(trackId); });
}

void DjIaVstProcessor::updateWaveformDisplay(const juce::String& trackId)
{
	if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
	{
		for (auto& trackComp : editor->getTrackComponents())
		{
			if (trackComp->getTrackId() == trackId)
			{
				if (trackComp->isWaveformVisible())
				{
					trackComp->refreshWaveformDisplay();
				}
				break;
			}
		}
	}
}

void DjIaVstProcessor::loadAudioDataAsync(const juce::String& trackId, const juce::MemoryBlock& audioData)
{
	TrackData* track = trackManager.getTrack(trackId);
	if (!track)
	{
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
			return;
		}
		loadAudioToStagingBuffer(reader, track);
		processAudioBPMAndSync(track);
		track->hasStagingData = true;
		track->swapRequested = true;
		juce::MessageManager::callAsync([this]()
			{
				if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor())) {
					editor->statusLabel.setText("Sample loaded! Ready to play.", juce::dontSendNotification);
					juce::Timer::callAfterDelay(2000, [this]() {
						if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor())) {
							editor->statusLabel.setText("Ready", juce::dontSendNotification);
						}
						});
				} });
	}
	catch (const std::exception& e)
	{
		track->hasStagingData = false;
		track->swapRequested = false;
	}
}

void DjIaVstProcessor::processAudioBPMAndSync(TrackData* track)
{
	float detectedBPM = AudioAnalyzer::detectBPM(track->stagingBuffer, track->stagingSampleRate);

	track->stagingOriginalBpm = (detectedBPM > 60.0f && detectedBPM < 200.0f) ? detectedBPM : track->bpm;

	double hostBpm = cachedHostBpm.load();

	if (hostBpm > 0.0 && track->stagingOriginalBpm > 0.0f &&
		std::abs(hostBpm - track->stagingOriginalBpm) > 1.0)
	{
		double stretchRatio = hostBpm / track->stagingOriginalBpm;

		AudioAnalyzer::timeStretchBuffer(track->stagingBuffer, stretchRatio, track->stagingSampleRate);
		track->stagingOriginalBpm = hostBpm;
	}
}

void DjIaVstProcessor::loadAudioToStagingBuffer(std::unique_ptr<juce::AudioFormatReader>& reader, TrackData* track)
{
	int numChannels = reader->numChannels;
	int numSamples = static_cast<int>(reader->lengthInSamples);
	double sampleRate = reader->sampleRate;

	track->stagingBuffer.setSize(2, numSamples, false, false, true);
	track->stagingBuffer.clear();

	reader->read(&track->stagingBuffer, 0, numSamples, 0, true, numChannels == 1);

	if (numChannels == 1 && track->stagingBuffer.getNumChannels() > 1)
	{
		track->stagingBuffer.copyFrom(1, 0, track->stagingBuffer, 0, 0, numSamples);
	}

	track->stagingNumSamples = numSamples;
	track->stagingSampleRate = sampleRate;
}

void DjIaVstProcessor::loadPendingSample()
{
	if (hasUnloadedSample.load() && !pendingTrackId.isEmpty())
	{
		waitingForMidiToLoad = true;
		trackIdWaitingForLoad = pendingTrackId;
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
}

void DjIaVstProcessor::setServerSidePreTreatment(bool enabled)
{
	serverSidePreTreatment = enabled;
}

void DjIaVstProcessor::setApiKey(const juce::String& key)
{
	apiKey = key;
	apiClient = DjIaClient(apiKey, serverUrl);
}

void DjIaVstProcessor::setServerUrl(const juce::String& url)
{
	serverUrl = url;
	apiClient = DjIaClient(apiKey, serverUrl);
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
				return bpm;
			}
		}
	}
	return 0.0;
}

juce::AudioProcessorEditor* DjIaVstProcessor::createEditor()
{
	return new DjIaVstEditor(*this);
}

void DjIaVstProcessor::addCustomPrompt(const juce::String& prompt)
{
	if (!prompt.isEmpty() && !customPrompts.contains(prompt))
	{
		customPrompts.add(prompt);
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

void DjIaVstProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto trackIds = trackManager.getAllTrackIds();

	for (const auto& id : trackIds)
	{
		TrackData* track = trackManager.getTrack(id);
	}

	juce::ValueTree state("DjIaVstState");

	state.setProperty("apiKey", juce::var(apiKey), nullptr);
	state.setProperty("serverUrl", juce::var(serverUrl), nullptr);
	state.setProperty("lastPrompt", juce::var(lastPrompt), nullptr);
	state.setProperty("lastKey", juce::var(lastKey), nullptr);
	state.setProperty("lastBpm", juce::var(lastBpm), nullptr);
	state.setProperty("lastPresetIndex", juce::var(lastPresetIndex), nullptr);
	state.setProperty("hostBpmEnabled", juce::var(hostBpmEnabled), nullptr);
	state.setProperty("lastDuration", juce::var(lastDuration), nullptr);
	state.setProperty("selectedTrackId", juce::var(selectedTrackId), nullptr);

	juce::ValueTree promptsState("CustomPrompts");
	for (int i = 0; i < customPrompts.size(); ++i)
	{
		promptsState.setProperty("prompt_" + juce::String(i), customPrompts[i], nullptr);
	}
	state.appendChild(promptsState, nullptr);
	auto tracksState = trackManager.saveState();
	state.appendChild(tracksState, nullptr);

	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void DjIaVstProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
	if (!xml || !xml->hasTagName("DjIaVstState"))
	{
		return;
	}
	juce::ValueTree state = juce::ValueTree::fromXml(*xml);

	lastPrompt = state.getProperty("lastPrompt", "").toString();
	lastKey = state.getProperty("lastKey", "C minor").toString();
	lastBpm = state.getProperty("lastBpm", 126.0);
	lastPresetIndex = state.getProperty("lastPresetIndex", -1);
	hostBpmEnabled = state.getProperty("hostBpmEnabled", false);
	lastDuration = state.getProperty("lastDuration", 6.0);

	auto promptsState = state.getChildWithName("CustomPrompts");
	if (promptsState.isValid())
	{
		customPrompts.clear();
		if (promptsState.hasProperty("count"))
		{
			loadCustomPromptsByCountProperty(promptsState);
		}
		else
		{
			juce::Array<std::pair<int, juce::String>> indexedPrompts;

			addCustomPromptsToIndexedPrompts(promptsState, indexedPrompts);

			std::sort(indexedPrompts.begin(), indexedPrompts.end(),
				[](const auto& a, const auto& b)
				{ return a.first < b.first; });

			for (const auto& pair : indexedPrompts)
			{
				customPrompts.add(pair.second);
			}
		}
	}

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

	auto tracksState = state.getChildWithName("TrackManager");
	if (tracksState.isValid())
	{
		trackManager.loadState(tracksState);
	}

	selectedTrackId = state.getProperty("selectedTrackId", "").toString();
	auto loadedTrackIds = trackManager.getAllTrackIds();

	if (selectedTrackId.isEmpty() || !trackManager.getTrack(selectedTrackId))
	{
		if (!loadedTrackIds.empty())
		{
			selectedTrackId = loadedTrackIds[0];
		}
		else
		{
			selectedTrackId = trackManager.createTrack("Main");
		}
	}
	juce::MessageManager::callAsync([this]()
		{ updateUI(); });
}

void DjIaVstProcessor::updateUI()
{
	if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
	{
		editor->updateUIFromProcessor();
		juce::Timer::callAfterDelay(50, [this]()
			{
				if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
				{
					editor->refreshTrackComponents();
					juce::Timer::callAfterDelay(100, [this]()
						{
							updateAllWaveformsAfterLoad();
						});
				} });
	}
}

void DjIaVstProcessor::addCustomPromptsToIndexedPrompts(juce::ValueTree& promptsState, juce::Array<std::pair<int, juce::String>>& indexedPrompts)
{
	for (int i = 0; i < promptsState.getNumProperties(); ++i)
	{
		auto propertyName = promptsState.getPropertyName(i);
		if (propertyName.toString().startsWith("prompt_"))
		{
			juce::String indexStr = propertyName.toString().substring(7);
			int index = indexStr.getIntValue();
			juce::String prompt = promptsState.getProperty(propertyName, "").toString();

			if (prompt.isNotEmpty())
			{
				indexedPrompts.add({ index, prompt });
			}
		}
	}
}

void DjIaVstProcessor::loadCustomPromptsByCountProperty(juce::ValueTree& promptsState)
{
	int count = promptsState.getProperty("count", 0);

	for (int i = 0; i < promptsState.getNumChildren(); ++i)
	{
		auto promptNode = promptsState.getChild(i);
		if (promptNode.hasType("Prompt"))
		{
			juce::String prompt = promptNode.getProperty("text", "").toString();
			if (prompt.isNotEmpty())
			{
				customPrompts.add(prompt);
			}
		}
	}
}

void DjIaVstProcessor::updateAllWaveformsAfterLoad()
{
	if (auto* editor = dynamic_cast<DjIaVstEditor*>(getActiveEditor()))
	{
		auto trackIds = trackManager.getAllTrackIds();
		for (const auto& trackId : trackIds)
		{
			TrackData* track = trackManager.getTrack(trackId);
			if (track && track->numSamples > 0)
			{
				updateWaveformDisplay(trackId);
			}
		}
	}
}

void DjIaVstProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
	if (parameterID == "generate" && newValue > 0.5f)
	{
		juce::MessageManager::callAsync([this]()
			{ parameters.getParameter("generate")->setValueNotifyingHost(0.0f); });
	}
	else if (parameterID == "autoload")
	{
		bool enabled = newValue > 0.5f;
		setAutoLoadEnabled(enabled);
	}
}
