#include "TrackComponent.h"
#include "WaveformDisplay.h"
#include "PluginProcessor.h"

TrackComponent::TrackComponent(const juce::String& trackId, DjIaVstProcessor& processor)
	: trackId(trackId), track(nullptr), audioProcessor(processor)
{
	setupUI();
}

TrackComponent::~TrackComponent()
{
}

void TrackComponent::setTrackData(TrackData* trackData)
{
	track = trackData;
	updateFromTrackData();
}

bool TrackComponent::isWaveformVisible() const
{
	return showWaveformButton.getToggleState() && waveformDisplay && waveformDisplay->isVisible();
}

void TrackComponent::updateWaveformWithTimeStretch()
{
	calculateHostBasedDisplay();
}

void TrackComponent::calculateHostBasedDisplay()
{
	if (!track || track->numSamples == 0)
		return;

	float effectiveBpm = calculateEffectiveBpm();

	if (waveformDisplay)
	{
		waveformDisplay->setOriginalBpm(track->originalBpm);
		waveformDisplay->setSampleBpm(effectiveBpm);
	}
}

void TrackComponent::toggleWaveformDisplay()
{
	if (showWaveformButton.getToggleState())
	{
		if (!waveformDisplay)
		{
			waveformDisplay = std::make_unique<WaveformDisplay>();
			waveformDisplay->onLoopPointsChanged = [this](double start, double end)
				{
					if (track)
					{
						track->loopStart = start;
						track->loopEnd = end;
						if (track->isPlaying.load())
						{
							track->readPosition = 0.0;
						}
					}
				};
			addAndMakeVisible(*waveformDisplay);
		}

		if (track && track->numSamples > 0)
		{

			waveformDisplay->setAudioData(track->audioBuffer, track->sampleRate);
			waveformDisplay->setLoopPoints(track->loopStart, track->loopEnd);
			calculateHostBasedDisplay();
			waveformDisplay->setVisible(true);
		}
		else
		{
			waveformDisplay->setVisible(true);
		}
	}
	else
	{
		if (waveformDisplay)
		{
			waveformDisplay->setVisible(false);
		}
	}

	if (auto* parentViewport = findParentComponentOfClass<juce::Viewport>())
	{
		if (auto* parentContainer = parentViewport->getViewedComponent())
		{
			int totalHeight = 5;

			for (int i = 0; i < parentContainer->getNumChildComponents(); ++i)
			{
				if (auto* trackComp = dynamic_cast<TrackComponent*>(parentContainer->getChildComponent(i)))
				{
					int trackHeight = trackComp->showWaveformButton.getToggleState() ? 110 : 60;
					trackComp->setSize(trackComp->getWidth(), trackHeight);
					trackComp->setBounds(trackComp->getX(), totalHeight, trackComp->getWidth(), trackHeight);
					totalHeight += trackHeight + 5;
				}
			}
			parentContainer->setSize(parentContainer->getWidth(), totalHeight);
			parentContainer->resized();
		}
	}
	resized();
	repaint();
}

void TrackComponent::updatePlaybackPosition(double timeInSeconds)
{
	if (waveformDisplay && showWaveformButton.getToggleState())
	{
		bool isPlaying = track && track->isPlaying.load();
		waveformDisplay->setPlaybackPosition(timeInSeconds, isPlaying);
	}
}

void TrackComponent::updateFromTrackData()
{
	if (!track)
		return;

	showWaveformButton.setToggleState(track->showWaveform, juce::dontSendNotification);

	trackNameLabel.setText(track->trackName, juce::dontSendNotification);
	int midiIndex = track->midiNote - 59;
	if (midiIndex >= 1 && midiIndex <= midiNoteSelector.getNumItems())
	{
		midiNoteSelector.setSelectedId(midiIndex, juce::dontSendNotification);
	}

	bpmOffsetSlider.setValue(track->bpmOffset, juce::dontSendNotification);
	timeStretchModeSelector.setSelectedId(track->timeStretchMode, juce::dontSendNotification);
	trackNumberLabel.setText(juce::String(track->slotIndex + 1), juce::dontSendNotification);
	trackNumberLabel.setColour(juce::Label::backgroundColourId, getTrackColour(track->slotIndex));

	calculateHostBasedDisplay();

	bool isCurrentlyPlaying = track->isPlaying.load();
	bool isMuted = track->isMuted.load();

	if (waveformDisplay)
	{
		bool shouldLock = isCurrentlyPlaying && !isMuted;
		waveformDisplay->lockLoopPoints(shouldLock);

		if (track->numSamples > 0 && track->sampleRate > 0)
		{
			double startSample = track->loopStart * track->sampleRate;
			double currentTimeInSection = (startSample + track->readPosition.load()) / track->sampleRate;
			waveformDisplay->setPlaybackPosition(currentTimeInSection, isCurrentlyPlaying);
		}
	}
	updateTrackInfo();
}

juce::Colour TrackComponent::getTrackColour(int trackIndex)
{
	static const std::vector<juce::Colour> trackColours = {
		juce::Colour(0xff5a7a9a),
		juce::Colour(0xff9a7a5a),
		juce::Colour(0xff6a8a6a),
		juce::Colour(0xff8a5a6a),
		juce::Colour(0xff7a6a8a),
		juce::Colour(0xff5a8a8a),
		juce::Colour(0xff7a8a6a),
		juce::Colour(0xff8a6a7a),
		juce::Colour(0xff8a6a5a),
		juce::Colour(0xff6a7a8a),
	};

	return trackColours[trackIndex % trackColours.size()];
}

float TrackComponent::calculateEffectiveBpm()
{
	if (!track)
		return 126.0f;

	float effectiveBpm = track->originalBpm;

	switch (track->timeStretchMode)
	{
	case 1:
		effectiveBpm = track->originalBpm;
		break;

	case 2:
		effectiveBpm = track->originalBpm + track->bpmOffset;
		break;

	case 3:
	{
		double hostBpm = audioProcessor.getHostBpm();
		if (hostBpm > 0.0 && track->originalBpm > 0.0)
		{
			float ratio = (float)hostBpm / track->originalBpm;
			effectiveBpm = track->originalBpm * ratio;
		}
	}
	break;

	case 4:
	{
		double hostBpm = audioProcessor.getHostBpm();
		if (hostBpm > 0.0 && track->originalBpm > 0.0)
		{
			float ratio = (float)hostBpm / track->originalBpm;
			effectiveBpm = track->originalBpm * ratio + track->bpmOffset;
		}
	}
	break;
	}

	return juce::jlimit(60.0f, 200.0f, effectiveBpm);
}

void TrackComponent::setSelected(bool selected)
{
	isSelected = selected;
	if (selected)
	{
		setColour(juce::TextButton::buttonColourId, juce::Colours::orange.withAlpha(0.3f));
	}
	else
	{
		setColour(juce::TextButton::buttonColourId, juce::Colours::grey.withAlpha(0.2f));
	}
	repaint();
}

void TrackComponent::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	juce::Colour bgColour;

	if (isGenerating && blinkState)
	{
		bgColour = juce::Colour(0xffff6600).withAlpha(0.3f);
	}
	else if (isSelected)
	{
		bgColour = juce::Colour(0xff00ff88).withAlpha(0.1f);
	}
	else
	{
		bgColour = juce::Colour(0xff2a2a2a).withAlpha(0.8f);
	}

	g.setColour(bgColour);
	g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

	juce::Colour borderColour = isGenerating ? juce::Colour(0xffff6600) : (isSelected ? juce::Colour(0xff00ff88) : juce::Colour(0xff555555));

	g.setColour(borderColour);
	g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f,
		isGenerating ? 3.0f : (isSelected ? 2.0f : 1.0f));

	if (isSelected)
	{
		g.setColour(borderColour.withAlpha(0.3f));
		g.drawRoundedRectangle(bounds.toFloat().expanded(1), 8.0f, 1.0f);
	}
}

void TrackComponent::resized()
{
	auto area = getLocalBounds().reduced(6);

	auto trackNumberArea = area.removeFromLeft(40);
	trackNumberLabel.setBounds(trackNumberArea);
	area.removeFromLeft(5);

	auto headerArea = area.removeFromTop(30);

	selectButton.setBounds(headerArea.removeFromLeft(70).reduced(2));
	trackNameLabel.setBounds(headerArea.removeFromLeft(55));
	infoLabel.setBounds(headerArea.removeFromLeft(250));

	headerArea.removeFromLeft(10);

	deleteButton.setBounds(headerArea.removeFromRight(35));
	headerArea.removeFromRight(5);
	generateButton.setBounds(headerArea.removeFromRight(45));
	headerArea.removeFromRight(5);
	showWaveformButton.setBounds(headerArea.removeFromRight(50));
	headerArea.removeFromRight(5);
	timeStretchModeSelector.setBounds(headerArea.removeFromRight(80));
	headerArea.removeFromRight(5);
	midiNoteSelector.setBounds(headerArea.removeFromRight(65));

	if (waveformDisplay && showWaveformButton.getToggleState())
	{
		area.removeFromTop(5);
		waveformDisplay->setBounds(area.removeFromTop(80));
		waveformDisplay->setVisible(true);
	}
	else if (waveformDisplay)
	{
		waveformDisplay->setVisible(false);
	}
}

void TrackComponent::startGeneratingAnimation()
{
	isGenerating = true;
	startTimer(200);
}

void TrackComponent::stopGeneratingAnimation()
{
	isGenerating = false;
	stopTimer();
	if (waveformDisplay && showWaveformButton.getToggleState() && track && track->numSamples > 0)
	{
		waveformDisplay->setAudioData(track->audioBuffer, track->sampleRate);
		waveformDisplay->setLoopPoints(track->loopStart, track->loopEnd);
	}

	repaint();
}

void TrackComponent::timerCallback()
{
	if (isGenerating)
	{
		blinkState = !blinkState;
		repaint();
	}
}

void TrackComponent::refreshWaveformDisplay()
{
	if (waveformDisplay && track && track->numSamples > 0)
	{
		waveformDisplay->setAudioData(track->audioBuffer, track->sampleRate);
		waveformDisplay->setLoopPoints(track->loopStart, track->loopEnd);
		calculateHostBasedDisplay();
	}
}

void TrackComponent::setGenerateButtonEnabled(bool enabled)
{
	generateButton.setEnabled(enabled);
}

void TrackComponent::setupUI()
{
	addAndMakeVisible(selectButton);
	selectButton.setButtonText("Select");
	selectButton.onClick = [this]()
		{
			if (onSelectTrack)
				onSelectTrack(trackId);
		};

	addAndMakeVisible(deleteButton);
	deleteButton.setButtonText("X");
	deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
	deleteButton.onClick = [this]()
		{
			if (onDeleteTrack)
				onDeleteTrack(trackId);
		};

	addAndMakeVisible(generateButton);
	generateButton.setButtonText("Gen");
	generateButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
	generateButton.onClick = [this]()
		{
			if (onGenerateForTrack)
				onGenerateForTrack(trackId);
		};

	addAndMakeVisible(infoLabel);
	infoLabel.setText("Empty track - Generate your sample!", juce::dontSendNotification);
	infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	infoLabel.setFont(juce::Font(12.0f));

	addAndMakeVisible(midiNoteSelector);
	for (int note = 60; note < 72; ++note)
	{
		juce::String noteName = juce::MidiMessage::getMidiNoteName(note, true, true, 3);
		midiNoteSelector.addItem(noteName, note - 59);
	}
	midiNoteSelector.onChange = [this]()
		{
			if (track)
			{
				track->midiNote = midiNoteSelector.getSelectedId() + 59;
			}
		};

	addAndMakeVisible(showWaveformButton);
	showWaveformButton.setButtonText("Wave");
	showWaveformButton.setClickingTogglesState(true);
	showWaveformButton.onClick = [this]()
		{
			if (track)
			{
				track->showWaveform = showWaveformButton.getToggleState();
				toggleWaveformDisplay();
			}
		};

	addAndMakeVisible(timeStretchModeSelector);
	timeStretchModeSelector.addItem("Off", 1);
	timeStretchModeSelector.addItem("Manual BPM", 2);
	timeStretchModeSelector.addItem("Host BPM", 3);
	timeStretchModeSelector.addItem("Host + Manual", 4);
	timeStretchModeSelector.onChange = [this]()
		{
			if (track)
			{
				track->timeStretchMode = timeStretchModeSelector.getSelectedId();
				calculateHostBasedDisplay();
				adjustLoopPointsToTempo();
			}
		};
	timeStretchModeSelector.setSelectedId(3, juce::dontSendNotification);

	addAndMakeVisible(trackNameLabel);
	trackNameLabel.setText(track ? track->trackName : "Track", juce::dontSendNotification);
	trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	trackNameLabel.setEditable(true);
	trackNameLabel.onEditorShow = [this]() {
		isEditingLabel = true;
		if (auto* editor = trackNameLabel.getCurrentTextEditor()) {
			editor->selectAll();
		}
		};

	trackNameLabel.onTextChange = [this]() {
		if (track) {
			track->trackName = trackNameLabel.getText();
			if (onTrackRenamed)
				onTrackRenamed(trackId, trackNameLabel.getText());
		}
		};
	trackNameLabel.onEditorHide = [this]() {
		isEditingLabel = false;
		};
	trackNameLabel.toFront(false);

	addAndMakeVisible(trackNumberLabel);
	trackNumberLabel.setText(juce::String(track ? track->slotIndex + 1 : 1), juce::dontSendNotification);
	trackNumberLabel.setJustificationType(juce::Justification::centred);
	trackNumberLabel.setFont(juce::Font(16.0f, juce::Font::bold));
	trackNumberLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	trackNumberLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff5a7a9a));
}

void TrackComponent::adjustLoopPointsToTempo()
{
	if (!track || track->numSamples == 0)
		return;

	float effectiveBpm = calculateEffectiveBpm();
	if (effectiveBpm <= 0)
		return;

	double beatDuration = 60.0 / effectiveBpm;
	double barDuration = beatDuration * 4.0;
	double originalDuration = track->numSamples / track->sampleRate;
	double stretchRatio = effectiveBpm / track->originalBpm;
	double effectiveDuration = originalDuration / stretchRatio;

	track->loopStart = 0.0;

	int maxWholeBars = (int)(effectiveDuration / barDuration);
	maxWholeBars = juce::jlimit(1, 8, maxWholeBars);

	track->loopEnd = maxWholeBars * barDuration;

	if (track->loopEnd > effectiveDuration)
	{
		maxWholeBars = std::max(1, maxWholeBars - 1);
		track->loopEnd = maxWholeBars * barDuration;
	}
}

void TrackComponent::updateTrackInfo()
{
	if (!track)
		return;

	if (!track->prompt.isEmpty())
	{
		float effectiveBpm = calculateEffectiveBpm();
		float originalBpm = track->originalBpm;

		juce::String bpmInfo = "";
		juce::String stretchIndicator = "";

		switch (track->timeStretchMode)
		{
		case 1:
			bpmInfo = " | Original: " + juce::String(originalBpm, 1);
			break;
		case 2:
			stretchIndicator = (effectiveBpm > originalBpm) ? " +" : (effectiveBpm < originalBpm) ? " -"
				: " =";
			bpmInfo = " | BPM: " + juce::String(effectiveBpm, 1) + stretchIndicator;
			break;
		case 3:
			stretchIndicator = " =";
			bpmInfo = " | Sync: " + juce::String(effectiveBpm, 1) + stretchIndicator;
			break;
		case 4:
			stretchIndicator = (track->bpmOffset > 0) ? " +" : (track->bpmOffset < 0) ? " -"
				: "";
			bpmInfo = " | Host+ " + juce::String(track->bpmOffset, 1) + stretchIndicator;
			break;
		}

		infoLabel.setText(track->prompt.substring(0, 30) + "..." + bpmInfo,
			juce::dontSendNotification);
	}
}

void TrackComponent::refreshWaveformIfNeeded()
{
	if (waveformDisplay && showWaveformButton.getToggleState() && track && track->numSamples > 0)
	{
		static int lastNumSamples = 0;
		if (track->numSamples != lastNumSamples)
		{
			refreshWaveformDisplay();
			lastNumSamples = track->numSamples;
		}
	}
}