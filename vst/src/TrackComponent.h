#pragma once
#include "JuceHeader.h"
#include "WaveformDisplay.h"

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

        int midiIndex = track->midiNote - 59;
        if (midiIndex >= 1 && midiIndex <= midiNoteSelector.getNumItems())
        {
            midiNoteSelector.setSelectedId(midiIndex, juce::dontSendNotification);
        }

        // NOUVEAU : Mettre à jour les contrôles BPM
        bpmOffsetSlider.setValue(track->bpmOffset, juce::dontSendNotification);
        timeStretchModeSelector.setSelectedId(track->timeStretchMode, juce::dontSendNotification);
        updateBpmSliderVisibility();

        bool isCurrentlyPlaying = track->isPlaying.load();
        playingIndicator.setColour(juce::Label::textColourId,
                                   isCurrentlyPlaying ? juce::Colours::green : juce::Colours::red);

        // Verrouiller les loop points si la track est en cours de lecture
        if (waveformDisplay)
        {
            waveformDisplay->lockLoopPoints(isCurrentlyPlaying);

            // Mettre à jour la position de lecture en temps réel
            if (track->numSamples > 0 && track->sampleRate > 0)
            {
                double startSample = track->loopStart * track->sampleRate;
                double currentTimeInSection = (startSample + track->readPosition.load()) / track->sampleRate;
                waveformDisplay->setPlaybackPosition(currentTimeInSection, isCurrentlyPlaying);
            }
        }

        // Mettre à jour les infos avec BPM
        updateTrackInfo();
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
            bgColour = juce::Colour(0xffff6600).withAlpha(0.3f); // Orange fluo clignotant
        }
        else if (isSelected)
        {
            bgColour = juce::Colour(0xff00ff88).withAlpha(0.1f); // Vert fluo sélection
        }
        else
        {
            bgColour = juce::Colour(0xff2a2a2a).withAlpha(0.8f); // Gris foncé
        }

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f); // Coins plus arrondis

        // Border avec glow effect
        juce::Colour borderColour = isGenerating ? juce::Colour(0xffff6600) : (isSelected ? juce::Colour(0xff00ff88) : juce::Colour(0xff555555));

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f,
                               isGenerating ? 3.0f : (isSelected ? 2.0f : 1.0f));

        // Glow effect pour sélection
        if (isSelected)
        {
            g.setColour(borderColour.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.toFloat().reduced(-1), 8.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // === HEADER ROW ===
        auto headerArea = area.removeFromTop(25);

        selectButton.setBounds(headerArea.removeFromLeft(60));
        trackNameLabel.setBounds(headerArea.removeFromLeft(100));
        trackNameEditor.setBounds(headerArea.removeFromLeft(100));

        headerArea.removeFromLeft(10);

        playingIndicator.setBounds(headerArea.removeFromRight(20));
        deleteButton.setBounds(headerArea.removeFromRight(35));
        generateButton.setBounds(headerArea.removeFromRight(45));
        showWaveformButton.setBounds(headerArea.removeFromRight(50));
        timeStretchModeSelector.setBounds(headerArea.removeFromRight(80));
        midiNoteSelector.setBounds(headerArea.removeFromRight(65));

        area.removeFromTop(5);

        // === CONTROLS ROW ===
        auto controlsArea = area.removeFromTop(25);
        muteButton.setBounds(controlsArea.removeFromLeft(40));
        soloButton.setBounds(controlsArea.removeFromLeft(40));

        controlsArea.removeFromLeft(10);
        volumeSlider.setBounds(controlsArea.removeFromLeft(90));
        panSlider.setBounds(controlsArea.removeFromLeft(90));

        // NOUVEAU : Slider BPM (seulement si visible)
        if (bpmOffsetSlider.isVisible())
        {
            controlsArea.removeFromLeft(5);
            bpmOffsetLabel.setBounds(controlsArea.removeFromLeft(30));
            bpmOffsetSlider.setBounds(controlsArea.removeFromLeft(110));
        }

        area.removeFromTop(5);

        // === INFO ROW ===
        infoLabel.setBounds(area.removeFromTop(20));

        // === WAVEFORM (si visible) ===
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
        if (waveformDisplay && showWaveformButton.getToggleState() && track && track->numSamples > 0)
        {
            waveformDisplay->setAudioData(track->audioBuffer, track->sampleRate);
            waveformDisplay->setLoopPoints(track->loopStart, track->loopEnd);
        }

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
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    juce::TextButton showWaveformButton;

    // Composants UI
    juce::TextButton selectButton;
    juce::Label trackNameLabel;
    juce::TextButton deleteButton;
    juce::TextButton generateButton;
    juce::ToggleButton muteButton;
    juce::ToggleButton soloButton;
    juce::ToggleButton autoStretchButton;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::Label infoLabel;
    juce::Label playingIndicator;

    juce::ComboBox timeStretchModeSelector;

    juce::Slider bpmOffsetSlider;
    juce::Label bpmOffsetLabel;

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
        playingIndicator.setText("O", juce::dontSendNotification);
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

        addAndMakeVisible(showWaveformButton);
        showWaveformButton.setButtonText("Wave");
        showWaveformButton.setClickingTogglesState(true);
        showWaveformButton.onClick = [this]()
        {
            toggleWaveformDisplay();
        };

        addAndMakeVisible(timeStretchModeSelector);
        timeStretchModeSelector.addItem("Off", 1);           // Pas de time-stretch
        timeStretchModeSelector.addItem("Manual BPM", 2);    // Slider BPM
        timeStretchModeSelector.addItem("Host BPM", 3);      // BPM du DAW
        timeStretchModeSelector.addItem("Host + Manual", 4); // Les deux combinés
        timeStretchModeSelector.setSelectedId(3);            // Host BPM par défaut

        timeStretchModeSelector.onChange = [this]()
        {
            if (track)
            {
                track->timeStretchMode = timeStretchModeSelector.getSelectedId();
                updateBpmSliderVisibility(); // Mettre à jour visibilité
            }
        };

        addAndMakeVisible(bpmOffsetSlider);
        bpmOffsetSlider.setRange(-50.0, 50.0, 0.1); // +/- 50 BPM, précision 0.1
        bpmOffsetSlider.setValue(0.0);
        bpmOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        bpmOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        bpmOffsetSlider.setTextValueSuffix(" BPM");
        bpmOffsetSlider.onValueChange = [this]()
        {
            if (track)
            {
                track->bpmOffset = bpmOffsetSlider.getValue();
                updateTrackInfo(); // Mettre à jour l'affichage
            }
        };

        addAndMakeVisible(bpmOffsetLabel);
        bpmOffsetLabel.setText("BPM:", juce::dontSendNotification);
        bpmOffsetLabel.setFont(juce::Font(9.0f));
        bpmOffsetLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        updateBpmSliderVisibility();
    }

    void updateTrackInfo()
    {
        if (!track)
            return;

        if (!track->prompt.isEmpty())
        {
            // Calculer et afficher BPM effectif
            double effectiveBpm = track->originalBpm;
            juce::String bpmInfo = "";

            switch (track->timeStretchMode)
            {
            case 1: // Off
                bpmInfo = " | Original: " + juce::String(track->originalBpm, 1);
                break;
            case 2: // Manual BPM
                effectiveBpm = track->originalBpm + track->bpmOffset;
                bpmInfo = " | BPM: " + juce::String(effectiveBpm, 1);
                break;
            case 3: // Host BPM
                bpmInfo = " | Sync: Host BPM";
                break;
            case 4: // Host + Manual
                bpmInfo = " | Host+" + juce::String(track->bpmOffset, 1);
                break;
            }

            infoLabel.setText("Prompt: " + track->prompt.substring(0, 15) + "..." + bpmInfo,
                              juce::dontSendNotification);
        }
        else
        {
            infoLabel.setText("Empty - MIDI: " +
                                  juce::MidiMessage::getMidiNoteName(track->midiNote, true, true, 3),
                              juce::dontSendNotification);
        }
    }

    void updateBpmSliderVisibility()
    {
        if (!track)
            return;

        // Afficher le slider pour les modes Manual (2) et Host+Manual (4)
        bool shouldShow = (track->timeStretchMode == 2 || track->timeStretchMode == 4);
        bpmOffsetSlider.setVisible(shouldShow);
        bpmOffsetLabel.setVisible(shouldShow);

        resized(); // Refaire le layout
    }

    void toggleWaveformDisplay()
    {
        if (showWaveformButton.getToggleState())
        {
            // Créer la waveform si elle n'existe pas
            if (!waveformDisplay)
            {
                waveformDisplay = std::make_unique<WaveformDisplay>();
                waveformDisplay->onLoopPointsChanged = [this](double start, double end)
                {
                    if (track)
                    {
                        track->loopStart = start;
                        track->loopEnd = end;

                        // Reset position de lecture si on change les points pendant la lecture
                        if (track->isPlaying.load())
                        {
                            track->readPosition = 0.0;
                        }

                        DBG("Loop points changed: " + juce::String(start, 2) + "s to " + juce::String(end, 2) + "s");
                    }
                };
                addAndMakeVisible(*waveformDisplay);
            }

            // Charger les données audio si disponibles
            if (track && track->numSamples > 0)
            {
                DBG("Loading waveform data: " + juce::String(track->numSamples) + " samples at " + juce::String(track->sampleRate) + " Hz");

                waveformDisplay->setAudioData(track->audioBuffer, track->sampleRate);
                waveformDisplay->setLoopPoints(track->loopStart, track->loopEnd);
                waveformDisplay->setVisible(true);

                // Les instructions sont maintenant affichées directement dans la waveform
            }
            else
            {
                DBG("No audio data available for waveform display");
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

        // IMPORTANT: Notifier le parent que la taille a changé
        if (auto *parentViewport = findParentComponentOfClass<juce::Viewport>())
        {
            if (auto *parentContainer = parentViewport->getViewedComponent())
            {
                // Recalculer la hauteur totale de tous les composants tracks
                int totalHeight = 5; // Marge du haut

                for (int i = 0; i < parentContainer->getNumChildComponents(); ++i)
                {
                    if (auto *trackComp = dynamic_cast<TrackComponent *>(parentContainer->getChildComponent(i)))
                    {
                        int trackHeight = trackComp->showWaveformButton.getToggleState() ? 160 : 80;
                        trackComp->setSize(trackComp->getWidth(), trackHeight);
                        trackComp->setBounds(trackComp->getX(), totalHeight, trackComp->getWidth(), trackHeight);
                        totalHeight += trackHeight + 5; // 5px d'espacement
                    }
                }

                // Redimensionner le container parent
                parentContainer->setSize(parentContainer->getWidth(), totalHeight);
                parentContainer->resized();

                DBG("Track container resized to height: " + juce::String(totalHeight));
            }
        }

        // Forcer un repaint complet
        resized();
        repaint();
    }
};