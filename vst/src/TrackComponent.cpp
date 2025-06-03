#include "TrackComponent.h"
#include "WaveformDisplay.h"
#include "PluginProcessor.h"

TrackComponent::TrackComponent(const juce::String &trackId, DjIaVstProcessor &processor)
    : trackId(trackId), track(nullptr), audioProcessor(processor)
{
    setupUI();
}

TrackComponent::~TrackComponent()
{
}

void TrackComponent::setTrackData(TrackData *trackData)
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

    if (auto *parentViewport = findParentComponentOfClass<juce::Viewport>())
    {
        if (auto *parentContainer = parentViewport->getViewedComponent())
        {
            int totalHeight = 5;

            for (int i = 0; i < parentContainer->getNumChildComponents(); ++i)
            {
                if (auto *trackComp = dynamic_cast<TrackComponent *>(parentContainer->getChildComponent(i)))
                {
                    int trackHeight = trackComp->showWaveformButton.getToggleState() ? 160 : 80;
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
    updateBpmSliderVisibility();

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

void TrackComponent::paint(juce::Graphics &g)
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

    auto headerArea = area.removeFromTop(25);

    selectButton.setBounds(headerArea.removeFromLeft(60));
    trackNameLabel.setBounds(headerArea.removeFromLeft(100));
    trackNameEditor.setBounds(headerArea.removeFromLeft(100));

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

    area.removeFromTop(5);

    auto controlsArea = area.removeFromTop(25);

    if (bpmOffsetSlider.isVisible())
    {
        controlsArea.removeFromLeft(5);
        bpmOffsetLabel.setBounds(controlsArea.removeFromLeft(50));
        bpmOffsetSlider.setBounds(controlsArea.removeFromLeft(300));
    }

    area.removeFromTop(5);

    infoLabel.setBounds(area.removeFromTop(20));

    if (waveformDisplay && showWaveformButton.getToggleState())
    {
        area.removeFromTop(5);
        auto waveformArea = area.removeFromTop(80);
        waveformDisplay->setBounds(waveformArea);
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
            DBG("=== SELECT BUTTON CLICKED ===");
            DBG("TrackComponent trackId: " << trackId);
            DBG("Track data trackId: " << (track ? track->trackId : "NULL"));
            DBG("Track data trackName: " << (track ? track->trackName : "NULL"));
            DBG("Track data slotIndex: " << (track ? juce::String(track->slotIndex) : "NULL"));

            if (onSelectTrack)
                onSelectTrack(trackId);
        };

    addAndMakeVisible(trackNameLabel);
    trackNameLabel.setText(track ? track->trackName : "Track", juce::dontSendNotification);
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);

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
    infoLabel.setText("Empty track", juce::dontSendNotification);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    infoLabel.setFont(juce::Font(10.0f));

    addAndMakeVisible(trackNameEditor);
    trackNameEditor.setMultiLine(false);
    trackNameEditor.setText(track ? track->trackName : "Track");
    trackNameEditor.onReturnKey = [this]()
    {
        if (track)
        {
            juce::String newName = trackNameEditor.getText();
            track->trackName = newName;
            trackNameLabel.setText(track->trackName, juce::dontSendNotification);

            if (onTrackRenamed)
                onTrackRenamed(trackId, newName);
        }
    };

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
            updateBpmSliderVisibility();
            calculateHostBasedDisplay();
            adjustLoopPointsToTempo();
        }
    };
    timeStretchModeSelector.setSelectedId(3, juce::dontSendNotification);
    addAndMakeVisible(bpmOffsetSlider);
    bpmOffsetSlider.setRange(-20.0, 20.0, 00.1);
    bpmOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    bpmOffsetSlider.setTextValueSuffix(" BPM");
    bpmOffsetSlider.onValueChange = [this]()
    {
        if (track)
        {
            track->bpmOffset = bpmOffsetSlider.getValue();
            calculateHostBasedDisplay();
            updateTrackInfo();
            adjustLoopPointsToTempo();
        }
    };
    bpmOffsetSlider.setValue(0.0, juce::dontSendNotification);
    addAndMakeVisible(bpmOffsetLabel);
    bpmOffsetLabel.setText("BPM offset:", juce::dontSendNotification);
    bpmOffsetLabel.setFont(juce::Font(9.0f));
    bpmOffsetLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    updateBpmSliderVisibility();
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
            bpmInfo = " | Host+" + juce::String(track->bpmOffset, 1) + stretchIndicator;
            break;
        }

        infoLabel.setText("Prompt: " + track->prompt.substring(0, 15) + "..." + bpmInfo,
                          juce::dontSendNotification);
    }
}

void TrackComponent::updateBpmSliderVisibility()
{
    if (!track)
        return;

    bool shouldShow = (track->timeStretchMode == 2 || track->timeStretchMode == 4);
    bpmOffsetSlider.setVisible(shouldShow);
    bpmOffsetLabel.setVisible(shouldShow);
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