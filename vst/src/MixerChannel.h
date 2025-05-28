#pragma once
#include "JuceHeader.h"
#include "PluginProcessor.h"

class MixerChannel : public juce::Component
{
public:
    MixerChannel(const juce::String &trackId) : trackId(trackId), track(nullptr)
    {
        setupUI();
    }

    ~MixerChannel() override
    {
    }

    void updateVUMeters()
    {
        updateVUMeter();
        repaint();
    }

    void setTrackData(TrackData *trackData)
    {
        track = trackData;
        updateFromTrackData();
    }

    juce::String getTrackId() const { return trackId; }

    std::function<void(const juce::String &, bool)> onSoloChanged;
    std::function<void(const juce::String &, bool)> onMuteChanged;
    std::function<void(const juce::String &)> onPlayTrack;
    std::function<void(const juce::String &)> onStopTrack;
    std::function<void(const juce::String &, int)> onMidiLearn;
    std::function<void(const juce::String &, float)> onPitchChanged;
    std::function<void(const juce::String &, float)> onFineChanged;
    std::function<void(const juce::String &, float)> onPanChanged;

    void updateFromTrackData()
    {
        if (!track)
            return;

        trackNameLabel.setText(track->trackName, juce::dontSendNotification);
        volumeSlider.setValue(track->volume.load(), juce::dontSendNotification);
        pitchKnob.setValue(track->bpmOffset, juce::dontSendNotification);
        fineKnob.setValue(track->fineOffset, juce::dontSendNotification);
        panKnob.setValue(track->pan.load(), juce::dontSendNotification);
        muteButton.setToggleState(track->isMuted.load(), juce::dontSendNotification);
        soloButton.setToggleState(track->isSolo.load(), juce::dontSendNotification);

        updateButtonColors();
    }

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds();

        juce::Colour bgColour = isSelected ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2a2a2a);
        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

        juce::Colour borderColour = isSelected ? juce::Colour(0xff00ff88) : juce::Colour(0xff404040);
        float borderWidth = isSelected ? 2.0f : 1.0f;
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, borderWidth);

        if (isSelected)
        {
            g.setColour(borderColour.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.toFloat().reduced(1), 10.0f, 1.0f);
        }
        drawVUMeter(g, bounds);
    }

    void drawVUMeter(juce::Graphics &g, juce::Rectangle<int> bounds)
    {
        auto vuArea = juce::Rectangle<float>(bounds.getWidth() - 10, 110, 6, bounds.getHeight() - 120);

        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRoundedRectangle(vuArea, 2.0f);

        g.setColour(juce::Colour(0xff666666));
        g.drawRoundedRectangle(vuArea, 2.0f, 0.5f);

        if (!track)
            return;

        float currentLevel = getCurrentAudioLevel();
        float peakLevel = getPeakLevel();

        int numSegments = 20;
        float segmentHeight = (vuArea.getHeight() - 4) / numSegments;

        for (int i = 0; i < numSegments; ++i)
        {
            fillMeters(vuArea, i, segmentHeight, numSegments, currentLevel, g);
        }

        if (peakLevel > 0.0f)
        {
            int peakSegment = (int)(peakLevel * numSegments);
            if (peakSegment < numSegments)
            {
                float peakY = vuArea.getBottom() - 2 - (peakSegment + 1) * segmentHeight;
                juce::Rectangle<float> peakRect(
                    vuArea.getX() + 1, peakY, vuArea.getWidth() - 2, 2);

                g.setColour(juce::Colours::white);
                g.fillRect(peakRect);
            }
        }

        if (peakLevel >= 0.95f)
        {
            auto clipRect = juce::Rectangle<float>(vuArea.getX(), vuArea.getY() - 8, vuArea.getWidth(), 4);
            g.setColour(juce::Colours::red);
            g.fillRoundedRectangle(clipRect, 2.0f);
        }
    }

    void fillMeters(juce::Rectangle<float>& vuArea, int i, float segmentHeight, int numSegments, float currentLevel, juce::Graphics& g)
    {
        float segmentY = vuArea.getBottom() - 2 - (i + 1) * segmentHeight;
        float segmentLevel = (float)i / numSegments;

        juce::Rectangle<float> segmentRect(
            vuArea.getX() + 1, segmentY, vuArea.getWidth() - 2, segmentHeight - 1);

        juce::Colour segmentColour;
        if (segmentLevel < 0.7f)
            segmentColour = juce::Colours::green;
        else if (segmentLevel < 0.9f)
            segmentColour = juce::Colours::orange;
        else
            segmentColour = juce::Colours::red;

        if (currentLevel >= segmentLevel)
        {
            g.setColour(segmentColour);
            g.fillRoundedRectangle(segmentRect, 1.0f);
        }
        else
        {
            g.setColour(segmentColour.withAlpha(0.1f));
            g.fillRoundedRectangle(segmentRect, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4);
        int width = area.getWidth();

        // Track name at top
        trackNameLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(5);

        // Transport buttons (2x2 grid)
        auto transportArea = area.removeFromTop(60);
        auto topRow = transportArea.removeFromTop(28);
        auto bottomRow = transportArea;

        playButton.setBounds(topRow.removeFromLeft(width / 2 - 2).reduced(2));
        stopButton.setBounds(topRow.removeFromLeft(width / 2 - 2).reduced(2));
        muteButton.setBounds(bottomRow.removeFromLeft(width / 2 - 2).reduced(2));
        soloButton.setBounds(bottomRow.removeFromLeft(width / 2 - 2).reduced(2));

        area.removeFromTop(5);

        // Volume slider (vertical, takes most space)
        auto volumeArea = area.removeFromTop(100);
        volumeSlider.setBounds(volumeArea.reduced(width / 4, 0));

        area.removeFromTop(5);

        // Knobs section
        auto knobsArea = area.removeFromTop(170);

        // Pitch knob
        auto pitchArea = knobsArea.removeFromTop(50);
        pitchLabel.setBounds(pitchArea.removeFromTop(12));
        pitchKnob.setBounds(pitchArea.reduced(2));

        // Fine knob
        auto fineArea = knobsArea.removeFromTop(50);
        fineLabel.setBounds(fineArea.removeFromTop(12));
        fineKnob.setBounds(fineArea.reduced(2));

        area.removeFromTop(5);
        auto panArea = knobsArea.removeFromTop(50);
        panLabel.setBounds(panArea.removeFromTop(12));
        panKnob.setBounds(panArea.reduced(2));
        updateMidiLearnIndicators();
    }

    void handleMidiMessage(const juce::MidiMessage &message, int learnedControlType)
    {
        if (!track || bypassMidiFrames > 0)
            return;

        if (message.isController())
        {
            float value = message.getControllerValue() / 127.0f;

            switch (learnedControlType)
            {
            case MIDI_VOLUME:
                track->volume = value;
                volumeSlider.setValue(value, juce::dontSendNotification);
                break;
            case MIDI_PITCH:
                pitchKnob.setValue(juce::jmap(value, 0.0f, 1.0f, -12.0f, 12.0f), juce::dontSendNotification);
                break;
            case MIDI_FINE:
                fineKnob.setValue(juce::jmap(value, 0.0f, 1.0f, -100.0f, 100.0f), juce::dontSendNotification);
                break;
            case MIDI_PAN:
                track->pan = juce::jmap(value, 0.0f, 1.0f, -1.0f, 1.0f);
                panKnob.setValue(track->pan.load(), juce::dontSendNotification);
                break;
            }
        }
        else if (message.isNoteOn() || message.isNoteOff())
        {
            bool noteOn = message.isNoteOn();

            switch (learnedControlType)
            {
            case MIDI_MUTE:
                if (noteOn)
                {
                    track->isMuted = !track->isMuted.load();
                    muteButton.setToggleState(track->isMuted.load(), juce::dontSendNotification);
                }
                break;
            case MIDI_SOLO:
                if (noteOn)
                {
                    track->isSolo = !track->isSolo.load();
                    soloButton.setToggleState(track->isSolo.load(), juce::dontSendNotification);
                }
                break;
            case MIDI_PLAY:
                playButton.setToggleState(noteOn, juce::dontSendNotification);
                if (noteOn && onPlayTrack)
                    onPlayTrack(trackId);
                break;
            case MIDI_STOP:
                stopButton.setToggleState(noteOn, juce::dontSendNotification);
                if (noteOn && onStopTrack)
                    onStopTrack(trackId);
                break;
            }
        }
    }

    void updateVUMeter()
    {
        if (!track || !track->isPlaying.load())
        {
            currentAudioLevel *= 0.95f;
            if (peakHoldTimer > 0)
            {
                peakHoldTimer--;
                if (peakHoldTimer == 0)
                    peakHold *= 0.9f;
            }
            return;
        }

        float instantLevel = calculateInstantLevel();

        levelHistory.push_back(instantLevel);
        if (levelHistory.size() > 5)
            levelHistory.erase(levelHistory.begin());

        float smoothedLevel = 0.0f;
        for (float level : levelHistory)
            smoothedLevel += level;
        smoothedLevel /= levelHistory.size();

        if (smoothedLevel > currentAudioLevel)
        {
            currentAudioLevel = smoothedLevel;
        }
        else
        {
            currentAudioLevel = currentAudioLevel * 0.85f + smoothedLevel * 0.15f;
        }

        if (currentAudioLevel > peakHold)
        {
            peakHold = currentAudioLevel;
            peakHoldTimer = 30;
        }
    }

    float calculateInstantLevel()
    {
        if (!track || track->numSamples == 0)
            return 0.0f;

        double readPos = track->readPosition.load();
        int sampleIndex = (int)(readPos);

        if (sampleIndex >= 0 && sampleIndex < track->numSamples)
        {
            float level = 0.0f;
            int samples = std::min(32, track->numSamples - sampleIndex);

            for (int i = 0; i < samples; ++i)
            {
                for (int ch = 0; ch < track->audioBuffer.getNumChannels(); ++ch)
                {
                    float sample = track->audioBuffer.getSample(ch, sampleIndex + i);
                    level += std::abs(sample);
                }
            }

            level /= (samples * track->audioBuffer.getNumChannels());
            level *= track->volume.load();

            return juce::jlimit(0.0f, 1.0f, level * 3.0f);
        }

        return 0.0f;
    }

    float getCurrentAudioLevel() const { return currentAudioLevel; }
    float getPeakLevel() const { return peakHold; }

    void setSelected(bool selected)
    {
        isSelected = selected;
        repaint();
    }

    void setCurrentLevel(float level)
    {
        currentAudioLevel = level;
    }

private:
    juce::String trackId;
    TrackData *track;
    bool isSelected = false;
    int bypassMidiFrames = 0;

    // VU Meter variables
    float currentAudioLevel = 0.0f;
    float peakHold = 0.0f;
    int peakHoldTimer = 0;
    std::vector<float> levelHistory; // Pour le lissage

    // UI Components
    juce::Label trackNameLabel;

    // Transport buttons
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;

    // Volume (vertical slider)
    juce::Slider volumeSlider;

    // Pitch & Fine knobs
    juce::Slider pitchKnob;
    juce::Label pitchLabel;
    juce::Slider fineKnob;
    juce::Label fineLabel;

    // Pan knob
    juce::Slider panKnob;
    juce::Label panLabel;

    // MIDI Learn state
    enum MidiLearnControls
    {
        MIDI_VOLUME = 1,
        MIDI_PITCH = 2,
        MIDI_FINE = 3,
        MIDI_PAN = 4,
        MIDI_MUTE = 5,
        MIDI_SOLO = 6,
        MIDI_PLAY = 7,
        MIDI_STOP = 8
    };

    std::set<int> midiLearnActive;

    void setupUI()
    {
        // Track name
        addAndMakeVisible(trackNameLabel);
        trackNameLabel.setText("Track", juce::dontSendNotification);
        trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        trackNameLabel.setJustificationType(juce::Justification::centred);
        trackNameLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        // Play button
        addAndMakeVisible(playButton);
        playButton.setButtonText("ARM");
        playButton.setClickingTogglesState(true);
        playButton.onClick = [this]()
        {
            if (track)
            {
                bool shouldArm = playButton.getToggleState();
                track->isArmed = shouldArm;
                if (!shouldArm)
                {
                    track->isPlaying = false;
                }
                updateButtonColors();
            }
        };
        setupMidiLearn(playButton, MIDI_PLAY);

        // Stop button
        addAndMakeVisible(stopButton);
        stopButton.setButtonText("STP");
        stopButton.setClickingTogglesState(false);
        stopButton.onClick = [this]()
        {
            if (track)
            {
                track->isArmed = false;
                track->isPlaying = false;
                playButton.setToggleState(false, juce::dontSendNotification);
                updateButtonColors();
            }
        };
        setupMidiLearn(stopButton, MIDI_STOP);

        // Mute button
        addAndMakeVisible(muteButton);
        muteButton.setButtonText("M");
        muteButton.setClickingTogglesState(true);
        muteButton.onClick = [this]()
        {
            if (track)
            {
                track->isMuted = muteButton.getToggleState();
                updateButtonColors();
            }
        };
        setupMidiLearn(muteButton, MIDI_MUTE);

        // Solo button
        addAndMakeVisible(soloButton);
        soloButton.setButtonText("S");
        soloButton.setClickingTogglesState(true);
        soloButton.onClick = [this]()
        {
            if (track)
            {
                bool newSoloState = soloButton.getToggleState();
                track->isSolo = newSoloState;
                updateButtonColors();
            }
        };
        setupMidiLearn(soloButton, MIDI_SOLO);

        // Volume slider (vertical)
        addAndMakeVisible(volumeSlider);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(0.8);
        volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
        volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        volumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ff88));
        volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));
        volumeSlider.onValueChange = [this]()
        {
            if (track)
            {
                track->volume = volumeSlider.getValue();
            }
        };
        setupMidiLearn(volumeSlider, MIDI_VOLUME);

        // Pitch knob
        addAndMakeVisible(pitchKnob);
        pitchKnob.setRange(-12.0, 12.0, 0.1);
        pitchKnob.setValue(0.0);
        pitchKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        pitchKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pitchKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));
        setupMidiLearn(pitchKnob, MIDI_PITCH);

        addAndMakeVisible(pitchLabel);
        pitchLabel.setText("PITCH", juce::dontSendNotification);
        pitchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        pitchLabel.setJustificationType(juce::Justification::centred);
        pitchLabel.setFont(juce::Font(9.0f));

        // Fine knob
        addAndMakeVisible(fineKnob);
        fineKnob.setRange(-100.0, 100.0, 1.0);
        fineKnob.setValue(0.0);
        fineKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        fineKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fineKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));
        setupMidiLearn(fineKnob, MIDI_FINE);

        addAndMakeVisible(fineLabel);
        fineLabel.setText("FINE", juce::dontSendNotification);
        fineLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        fineLabel.setJustificationType(juce::Justification::centred);
        fineLabel.setFont(juce::Font(9.0f));

        // Pan knob
        addAndMakeVisible(panKnob);
        panKnob.setRange(-1.0, 1.0, 0.01);
        panKnob.setValue(0.0);
        panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));
        panKnob.onValueChange = [this]()
        {
            if (track)
            {
                track->pan = panKnob.getValue();
            }
        };
        setupMidiLearn(panKnob, MIDI_PAN);

        addAndMakeVisible(panLabel);
        panLabel.setText("PAN", juce::dontSendNotification);
        panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        panLabel.setJustificationType(juce::Justification::centred);
        panLabel.setFont(juce::Font(9.0f));

        pitchKnob.onValueChange = [this]()
        {
            if (track)
            {
                track->bpmOffset = pitchKnob.getValue();
                if (onPitchChanged)
                {
                    onPitchChanged(trackId, pitchKnob.getValue());
                }
            }
        };

        fineKnob.onValueChange = [this]()
        {
            if (track)
            {
                float fineBpmOffset = fineKnob.getValue() / 100.0f;
                track->fineOffset = fineBpmOffset;
                if (onFineChanged)
                {
                    onFineChanged(trackId, fineBpmOffset);
                }
            }
        };
        panKnob.onValueChange = [this]()
        {
            if (track)
            {
                track->pan = panKnob.getValue();

                if (onPanChanged)
                {
                    onPanChanged(trackId, track->pan.load());
                }
            }
        };
    }

    void updateButtonColors()
    {
        if (!track)
            return;

        bool isArmed = track->isArmed.load();
        bool isPlaying = track->isPlaying.load();
        bool isMuted = track->isMuted.load();
        bool isSolo = track->isSolo.load();

        if (isPlaying)
        {
            playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00ff44));
            playButton.setButtonText("PLY");
        }
        else if (isArmed)
        {
            playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff6600));
            playButton.setButtonText("ARM");
        }
        else
        {
            playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff404040));
            playButton.setButtonText("ARM");
        }

        muteButton.setColour(juce::TextButton::buttonOnColourId,
                             isMuted ? juce::Colour(0xffaa0000) : juce::Colour(0xff404040));
        muteButton.setColour(juce::TextButton::buttonColourId,   
            juce::Colour(0xff404040));

        soloButton.setColour(juce::TextButton::buttonOnColourId, 
            juce::Colour(0xffffff00));
        soloButton.setColour(juce::TextButton::textColourOnId,  
            juce::Colours::black);
        soloButton.setColour(juce::TextButton::buttonColourId, 
            juce::Colour(0xff404040));
        soloButton.setColour(juce::TextButton::textColourOffId,  
            juce::Colours::white);

        stopButton.setColour(juce::TextButton::buttonColourId,
                             (isArmed || isPlaying) ? juce::Colour(0xffaa4400) : juce::Colour(0xff404040));
    }

    void setupMidiLearn(juce::Component &component, int controlType)
    {
        component.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        auto mouseHandler = [this, controlType](const juce::MouseEvent &e)
        {
            if (e.mods.isRightButtonDown())
            {
                startMidiLearn(controlType);
            }
        };
        component.addMouseListener(this, false);
    }

    void startMidiLearn(int controlType)
    {
        midiLearnActive.insert(controlType);
        if (onMidiLearn)
        {
            onMidiLearn(trackId, controlType);
        }
        repaint();
        juce::Timer::callAfterDelay(5000, [this, controlType]()
                                    {
            midiLearnActive.erase(controlType);
            repaint(); });
    }

    void updateMidiLearnIndicators()
    {
        // Small red dots for active MIDI learn
        // Will be drawn in paint() method
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerChannel)
};