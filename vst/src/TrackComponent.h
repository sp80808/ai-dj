#pragma once
#include "JuceHeader.h"

class TrackComponent : public juce::Component, public juce::DragAndDropContainer, public juce::DragAndDropTarget, public juce::Timer
{
public:
    TrackComponent(const juce::String &trackId)
        : trackId(trackId), track(nullptr)
    {
        setupUI();
    }

    void setTrackData(TrackData *trackData)
    {
        track = trackData;
        updateFromTrackData();
    }

    juce::String getTrackId() const { return trackId; }

    std::function<void(const juce::String &)> onDeleteTrack;
    std::function<void(const juce::String &)> onSelectTrack;
    std::function<void(const juce::String &)> onGenerateForTrack;

    void updateFromTrackData()
    {
        if (!track)
            return;

        trackNameLabel.setText(track->trackName, juce::dontSendNotification);
        volumeSlider.setValue(track->volume.load(), juce::dontSendNotification);
        panSlider.setValue(track->pan.load(), juce::dontSendNotification);
        muteButton.setToggleState(track->isMuted.load(), juce::dontSendNotification);
        soloButton.setToggleState(track->isSolo.load(), juce::dontSendNotification);
        playingIndicator.setColour(juce::Label::textColourId,
                                   track->isPlaying.load() ? juce::Colours::green : juce::Colours::red);
        // Mettre à jour les infos
        if (!track->prompt.isEmpty())
        {
            infoLabel.setText("Prompt: " + track->prompt.substring(0, 30) + "...",
                              juce::dontSendNotification);
        }
        else
        {
            infoLabel.setText("Empty track", juce::dontSendNotification);
        }
        playingIndicator.setColour(juce::Label::textColourId,
                                   track->isPlaying.load() ? juce::Colours::green : juce::Colours::red);
    }

    void setSelected(bool selected)
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

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds();

        // Background avec clignotement si génération
        juce::Colour bgColour;
        if (isGenerating && blinkState)
        {
            bgColour = juce::Colours::orange.withAlpha(0.5f); // Orange clignotant
        }
        else if (isSelected)
        {
            bgColour = juce::Colours::orange.withAlpha(0.2f);
        }
        else
        {
            bgColour = juce::Colours::grey.withAlpha(0.1f);
        }

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Border
        juce::Colour borderColour = isGenerating ? juce::Colours::orange : (isSelected ? juce::Colours::orange : juce::Colours::grey);
        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 4.0f,
                               isGenerating ? 2.0f : 1.0f);
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(5);

        // Header avec nom et boutons
        auto headerArea = area.removeFromTop(25);
        selectButton.setBounds(headerArea.removeFromLeft(60));
        trackNameLabel.setBounds(headerArea.removeFromLeft(120));
        deleteButton.setBounds(headerArea.removeFromRight(30));
        generateButton.setBounds(headerArea.removeFromRight(40));
        trackNameEditor.setBounds(headerArea.removeFromLeft(100));
        playingIndicator.setBounds(headerArea.removeFromRight(20));

        area.removeFromTop(5);

        // Contrôles audio
        auto controlsArea = area.removeFromTop(25);
        muteButton.setBounds(controlsArea.removeFromLeft(40));
        soloButton.setBounds(controlsArea.removeFromLeft(40));
        volumeSlider.setBounds(controlsArea.removeFromLeft(80));
        panSlider.setBounds(controlsArea.removeFromLeft(80));

        midiNoteSelector.setBounds(controlsArea.removeFromRight(60));

        area.removeFromTop(3);

        // Info
        infoLabel.setBounds(area.removeFromTop(20));
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (e.getDistanceFromDragStart() > 10)
        {
            startDragging("track_" + trackId, this);
        }
    }

    bool isInterestedInDragSource(const SourceDetails &details) override
    {
        return details.description.toString().startsWith("track_");
    }

    void itemDropped(const SourceDetails &details) override
    {
        juce::String droppedTrackId = details.description.toString().substring(6);
        if (onReorderTrack)
        {
            onReorderTrack(droppedTrackId, trackId);
        }
    }

    void startGeneratingAnimation()
    {
        isGenerating = true;
        startTimer(200); // Clignoter toutes les 200ms
    }

    void stopGeneratingAnimation()
    {
        isGenerating = false;
        stopTimer();
        repaint();
    }

    void timerCallback() override
    {
        if (isGenerating)
        {
            blinkState = !blinkState;
            repaint();
        }

        // Update voyant de lecture aussi
        if (track)
        {
            playingIndicator.setColour(juce::Label::textColourId,
                                       track->isPlaying.load() ? juce::Colours::green : juce::Colours::red);
        }
    }

    std::function<void(const juce::String &, const juce::String &)> onReorderTrack;

private:
    juce::String trackId;
    TrackData *track;
    bool isSelected = false;

    // Composants UI
    juce::TextButton selectButton;
    juce::Label trackNameLabel;
    juce::TextButton deleteButton;
    juce::TextButton generateButton;
    juce::ToggleButton muteButton;
    juce::ToggleButton soloButton;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::Label infoLabel;
    juce::Label playingIndicator;

    juce::TextEditor trackNameEditor;
    juce::ComboBox midiNoteSelector;

    bool isGenerating = false;
    bool blinkState = false;

    void setupUI()
    {
        // Select button
        addAndMakeVisible(selectButton);
        selectButton.setButtonText("Select");
        selectButton.onClick = [this]()
        {
            if (onSelectTrack)
                onSelectTrack(trackId);
        };

        // Track name
        addAndMakeVisible(trackNameLabel);
        trackNameLabel.setText(track ? track->trackName : "Track", juce::dontSendNotification);
        trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        // Delete button
        addAndMakeVisible(deleteButton);
        deleteButton.setButtonText("X");
        deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
        deleteButton.onClick = [this]()
        {
            if (onDeleteTrack)
                onDeleteTrack(trackId);
        };

        // Generate button
        addAndMakeVisible(generateButton);
        generateButton.setButtonText("Gen");
        generateButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
        generateButton.onClick = [this]()
        {
            if (onGenerateForTrack)
                onGenerateForTrack(trackId);
        };

        // Mute/Solo
        addAndMakeVisible(muteButton);
        muteButton.setButtonText("M");
        muteButton.setClickingTogglesState(true);
        muteButton.onClick = [this]()
        {
            if (track)
                track->isMuted = muteButton.getToggleState();
        };

        addAndMakeVisible(soloButton);
        soloButton.setButtonText("S");
        soloButton.setClickingTogglesState(true);
        soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        soloButton.onClick = [this]()
        {
            if (track)
                track->isSolo = soloButton.getToggleState();
        };

        // Volume slider
        addAndMakeVisible(volumeSlider);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(0.8);
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        volumeSlider.onValueChange = [this]()
        {
            if (track)
                track->volume = volumeSlider.getValue();
        };

        // Pan slider
        addAndMakeVisible(panSlider);
        panSlider.setRange(-1.0, 1.0, 0.01);
        panSlider.setValue(0.0);
        panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panSlider.onValueChange = [this]()
        {
            if (track)
                track->pan = panSlider.getValue();
        };

        addAndMakeVisible(playingIndicator);
        playingIndicator.setText("●", juce::dontSendNotification);
        playingIndicator.setColour(juce::Label::textColourId, juce::Colours::red);
        playingIndicator.setFont(juce::Font(16.0f));

        // Info label
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
                track->trackName = trackNameEditor.getText();
                trackNameLabel.setText(track->trackName, juce::dontSendNotification);
            }
        };

        addAndMakeVisible(midiNoteSelector);
        for (int note = 60; note < 72; ++note)
        { // C4 à B4
            juce::String noteName = juce::MidiMessage::getMidiNoteName(note, true, true, 3);
            midiNoteSelector.addItem(noteName, note - 59);
        }
        midiNoteSelector.onChange = [this]()
        {
            if (track)
            {
                track->midiNote = midiNoteSelector.getSelectedId() + 59; // Convertir back
            }
        };
    }
};