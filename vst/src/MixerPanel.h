#pragma once
#include "JuceHeader.h"
#include "MixerChannel.h"
#include "PluginProcessor.h"

class MasterChannel : public juce::Component
{
public:
    MasterChannel()
    {
        setupUI();
    }

    void setupUI()
    {
        // Master Volume
        addAndMakeVisible(masterVolumeSlider);
        masterVolumeSlider.setRange(0.0, 1.0, 0.01);
        masterVolumeSlider.setValue(0.8);
        masterVolumeSlider.setSliderStyle(juce::Slider::LinearVertical);
        masterVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        masterVolumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff6600)); // Orange master
        masterVolumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));

        // Master Pan
        addAndMakeVisible(masterPanKnob);
        masterPanKnob.setRange(-1.0, 1.0, 0.01);
        masterPanKnob.setValue(0.0);
        masterPanKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        masterPanKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        masterPanKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

        // EQ 3 bandes (simple mais efficace)
        addAndMakeVisible(highKnob);
        highKnob.setRange(-12.0, 12.0, 0.1);
        highKnob.setValue(0.0);
        highKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        highKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        highKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

        addAndMakeVisible(midKnob);
        midKnob.setRange(-12.0, 12.0, 0.1);
        midKnob.setValue(0.0);
        midKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        midKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        midKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

        addAndMakeVisible(lowKnob);
        lowKnob.setRange(-12.0, 12.0, 0.1);
        lowKnob.setValue(0.0);
        lowKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        lowKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        lowKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

        // Labels
        addAndMakeVisible(masterLabel);
        masterLabel.setText("MASTER", juce::dontSendNotification);
        masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        masterLabel.setJustificationType(juce::Justification::centred);
        masterLabel.setFont(juce::Font(14.0f, juce::Font::bold));

        addAndMakeVisible(highLabel);
        highLabel.setText("HIGH", juce::dontSendNotification);
        highLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        highLabel.setJustificationType(juce::Justification::centred);
        highLabel.setFont(juce::Font(9.0f));

        addAndMakeVisible(midLabel);
        midLabel.setText("MID", juce::dontSendNotification);
        midLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        midLabel.setJustificationType(juce::Justification::centred);
        midLabel.setFont(juce::Font(9.0f));

        addAndMakeVisible(lowLabel);
        lowLabel.setText("LOW", juce::dontSendNotification);
        lowLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        lowLabel.setJustificationType(juce::Justification::centred);
        lowLabel.setFont(juce::Font(9.0f));

        addAndMakeVisible(panLabel);
        panLabel.setText("PAN", juce::dontSendNotification);
        panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        panLabel.setJustificationType(juce::Justification::centred);
        panLabel.setFont(juce::Font(9.0f));

        // Dans setupUI() de MasterChannel, ajouter les callbacks :
        masterVolumeSlider.onValueChange = [this]()
        {
            if (onMasterVolumeChanged)
            {
                onMasterVolumeChanged(masterVolumeSlider.getValue());
            }
        };

        highKnob.onValueChange = [this]()
        {
            if (onMasterEQChanged)
            {
                onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
            }
        };

        midKnob.onValueChange = [this]()
        {
            if (onMasterEQChanged)
            {
                onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
            }
        };

        lowKnob.onValueChange = [this]()
        {
            if (onMasterEQChanged)
            {
                onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
            }
        };

        masterPanKnob.onValueChange = [this]()
        {
            if (onMasterPanChanged)
            {
                onMasterPanChanged(masterPanKnob.getValue());
            }
        };
    }

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds();

        // Background master channel (plus large, orange)
        g.setColour(juce::Colour(0xff3a2a1a)); // Marron orangé
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

        // Border orange
        g.setColour(juce::Colour(0xffff6600));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 2.0f);

        // VU meter master à droite
        drawMasterVUMeter(g, bounds);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4);
        int width = area.getWidth();

        // Master label at top
        masterLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(10);

        // EQ section en TRIANGLE - VERSION PROPRE
        auto eqArea = area.removeFromTop(100);
        int knobSize = 35;

        // HIGH EQ - EN HAUT, PARFAITEMENT CENTRÉ
        auto highRow = eqArea.removeFromTop(50);
        highLabel.setBounds((width - 50) / 2, highRow.getY(), 50, 12);
        highKnob.setBounds((width - knobSize) / 2, highRow.getY() + 15, knobSize, knobSize);

        eqArea.removeFromTop(5); // Espace entre les rangées

        // RANGÉE DU BAS : MID et LOW
        auto bottomRow = eqArea.removeFromTop(50);
        int spacing = width / 4; // Espacement symétrique

        // MID à gauche
        midLabel.setBounds(spacing - 25, bottomRow.getY(), 50, 12);
        midKnob.setBounds(spacing - knobSize / 2, bottomRow.getY() + 15, knobSize, knobSize);

        // LOW à droite
        lowLabel.setBounds(width - spacing - 25, bottomRow.getY(), 50, 12);
        lowKnob.setBounds(width - spacing - knobSize / 2, bottomRow.getY() + 15, knobSize, knobSize);

        auto volumeArea = area.removeFromTop(140); // Encore plus grand
        int faderWidth = width / 3;                // Plus large aussi
        int centerX = (width - faderWidth) / 2;
        masterVolumeSlider.setBounds(centerX, volumeArea.getY() + 5, faderWidth, volumeArea.getHeight() - 10);

        area.removeFromTop(5);

        // Master pan at bottom
        auto panArea = area.removeFromTop(60);
        auto vuZone = panArea.removeFromRight(10);
        auto knobZone = panArea;
        panLabel.setBounds(knobZone.removeFromTop(12));
        masterPanKnob.setBounds(knobZone.reduced(2));
    }

    void drawMasterVUMeter(juce::Graphics &g, juce::Rectangle<int> bounds)
    {
        auto vuArea = juce::Rectangle<float>(bounds.getWidth() - 15, 40, 10, bounds.getHeight() - 80);

        // Background du VU meter master
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRoundedRectangle(vuArea, 2.0f);

        // Border
        g.setColour(juce::Colour(0xffff6600)); // Orange pour master
        g.drawRoundedRectangle(vuArea, 2.0f, 1.0f);

        // Segments du VU meter master
        int numSegments = 25; // Plus de résolution pour le master
        float segmentHeight = (vuArea.getHeight() - 4) / numSegments;

        for (int i = 0; i < numSegments; ++i)
        {
            float segmentY = vuArea.getBottom() - 2 - (i + 1) * segmentHeight;
            float segmentLevel = (float)i / numSegments;

            juce::Rectangle<float> segmentRect(
                vuArea.getX() + 1, segmentY, vuArea.getWidth() - 2, segmentHeight - 1);

            // Couleur selon le niveau (master)
            juce::Colour segmentColour;
            if (segmentLevel < 0.6f)
                segmentColour = juce::Colours::green;
            else if (segmentLevel < 0.8f)
                segmentColour = juce::Colours::orange;
            else if (segmentLevel < 0.95f)
                segmentColour = juce::Colours::red;
            else
                segmentColour = juce::Colours::white; // Clipping = blanc

            // Afficher le segment si le niveau l'atteint
            if (masterLevel >= segmentLevel)
            {
                g.setColour(segmentColour);
                g.fillRoundedRectangle(segmentRect, 1.0f);
            }
            else
            {
                // Segment éteint
                g.setColour(segmentColour.withAlpha(0.05f));
                g.fillRoundedRectangle(segmentRect, 1.0f);
            }
        }

        // Peak hold master
        if (masterPeakHold > 0.0f)
        {
            int peakSegment = (int)(masterPeakHold * numSegments);
            if (peakSegment < numSegments)
            {
                float peakY = vuArea.getBottom() - 2 - (peakSegment + 1) * segmentHeight;
                juce::Rectangle<float> peakRect(vuArea.getX() + 1, peakY, vuArea.getWidth() - 2, 3);

                g.setColour(juce::Colours::white);
                g.fillRect(peakRect);
            }
        }

        // Indicateur de clipping master (LED rouge clignotante)
        if (masterPeakHold >= 0.98f)
        {
            auto clipRect = juce::Rectangle<float>(vuArea.getX() - 2, vuArea.getY() - 12, vuArea.getWidth() + 4, 8);
            g.setColour(isClipping && (juce::Time::getCurrentTime().toMilliseconds() % 500 < 250)
                            ? juce::Colours::red
                            : juce::Colours::darkred);
            g.fillRoundedRectangle(clipRect, 4.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText("CLIP", clipRect, juce::Justification::centred);
        }
    }

    void setRealAudioLevel(float level)
    {
        realAudioLevel = juce::jlimit(0.0f, 1.0f, level);
        hasRealAudio = true;
    }

    void updateMasterLevels()
    {
        float instantLevel;

        if (hasRealAudio)
        {
            // Utiliser le vrai niveau audio
            instantLevel = realAudioLevel;
        }
        else
        {
            // Fallback : simulation pour debug
            static float phase = 0.0f;
            phase += 0.05f; // Plus lent
            instantLevel = (std::sin(phase) * 0.3f + 0.3f) * 0.5f;
        }

        // Attack/Release réaliste
        if (instantLevel > masterLevel)
        {
            masterLevel = masterLevel * 0.7f + instantLevel * 0.3f; // Attack rapide
        }
        else
        {
            masterLevel = masterLevel * 0.95f + instantLevel * 0.05f; // Release lent
        }

        // Peak hold
        if (masterLevel > masterPeakHold)
        {
            masterPeakHold = masterLevel;
            masterPeakHoldTimer = 60; // 2 secondes à 30 FPS
        }
        else if (masterPeakHoldTimer > 0)
        {
            masterPeakHoldTimer--;
        }
        else
        {
            masterPeakHold *= 0.98f; // Descente lente
        }

        // Détecter clipping
        isClipping = (masterPeakHold >= 0.95f);
    }

    // Interface publique pour contrôler le master
    std::function<void(float)> onMasterVolumeChanged;
    std::function<void(float)> onMasterPanChanged;
    std::function<void(float, float, float)> onMasterEQChanged; // high, mid, low

private:
    juce::Slider masterVolumeSlider;
    juce::Slider masterPanKnob;
    juce::Slider highKnob, midKnob, lowKnob;

    float realAudioLevel = 0.0f;
    bool hasRealAudio = false;

    juce::Label masterLabel;
    juce::Label highLabel, midLabel, lowLabel, panLabel;

    // Niveaux master
    float masterLevel = 0.0f;
    float masterPeakHold = 0.0f;
    int masterPeakHoldTimer = 0;
    bool isClipping = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterChannel)
};

class MixerPanel : public juce::Component
{
public:
    MixerPanel(DjIaVstProcessor &processor) : audioProcessor(processor)
    {
        // Créer le master channel
        masterChannel = std::make_unique<MasterChannel>();
        addAndMakeVisible(*masterChannel);

        masterChannel->onMasterVolumeChanged = [this](float volume)
        {
            masterVolume = volume;
            audioProcessor.setMasterVolume(volume);
        };

        masterChannel->onMasterPanChanged = [this](float pan)
        {
            masterPan = pan;
            audioProcessor.setMasterPan(pan);
        };

        masterChannel->onMasterEQChanged = [this](float high, float mid, float low)
        {
            audioProcessor.setMasterEQ(high, mid, low);
        };

        // Container pour les channels
        addAndMakeVisible(channelsViewport);
        channelsViewport.setViewedComponent(&channelsContainer, false);
        channelsViewport.setScrollBarsShown(false, true); // Scroll horizontal

        // Rafraîchir les channels
        refreshMixerChannels();
        audioProcessor.onUIUpdateNeeded = [this]()
        {
            updateAllMixerComponents();
        };
    }

    void updateAllMixerComponents()
    {
        for (auto &channel : mixerChannels)
        {
            channel->updateVUMeters();
        }
        calculateMasterLevel();
        masterChannel->updateMasterLevels();
    }

    float getMasterVolume() const { return masterVolume; }
    float getMasterPan() const { return masterPan; }

    void calculateMasterLevel()
    {
        float totalLevel = 0.0f;
        float maxLevel = 0.0f;
        int activeChannels = 0;

        // Sommer les niveaux de toutes les channels actives
        for (auto &channel : mixerChannels)
        {
            float channelLevel = channel->getCurrentAudioLevel();
            if (channelLevel > 0.01f)
            {                                                // Seuil minimal pour éviter le bruit de fond
                totalLevel += channelLevel * channelLevel;   // RMS
                maxLevel = std::max(maxLevel, channelLevel); // Peak
                activeChannels++;
            }
        }

        if (activeChannels > 0)
        {
            // Calculer le niveau RMS
            float rmsLevel = std::sqrt(totalLevel / activeChannels);

            // Utiliser un mix entre RMS et peak pour un rendu plus musical
            float finalLevel = (rmsLevel * 0.7f) + (maxLevel * 0.3f);

            // Appliquer le volume master
            finalLevel *= masterVolume;

            // Envoyer au master channel
            masterChannel->setRealAudioLevel(finalLevel);
        }
        else
        {
            // Aucune track active = silence
            masterChannel->setRealAudioLevel(0.0f);
        }
    }

    void refreshMixerChannels()
    {
        auto trackIds = audioProcessor.getAllTrackIds();

        // Clear existing channels - ORDRE IMPORTANT
        channelsContainer.removeAllChildren(); // D'abord retirer de l'affichage
        mixerChannels.clear();                 // Puis vider le vector

        int xPos = 5;
        const int channelWidth = 90;
        const int channelSpacing = 5;

        for (const auto &trackId : trackIds)
        {
            TrackData *trackData = audioProcessor.getTrack(trackId);
            if (!trackData)
                continue;

            auto mixerChannel = std::make_unique<MixerChannel>(trackId);
            mixerChannel->setTrackData(trackData);

            // CORRECTION : Capturer le raw pointer AVANT de move
            auto *channelPtr = mixerChannel.get();

            // === CALLBACKS SOLO/MUTE ===
            mixerChannel->onSoloChanged = [this, channelPtr](const juce::String &id, bool solo)
            {
                // Rafraîchir TOUTES les channels car solo affecte les autres
                for (auto &channel : mixerChannels)
                {
                    channel->updateFromTrackData();
                    channel->repaint();
                }
            };

            mixerChannel->onMuteChanged = [this, channelPtr](const juce::String &id, bool mute)
            {
                channelPtr->updateFromTrackData();
                channelPtr->repaint();
            };

            // === CALLBACKS TRANSPORT ===
            mixerChannel->onPlayTrack = [this, channelPtr](const juce::String &id)
            {
                if (auto *track = audioProcessor.getTrack(id))
                {
                    audioProcessor.startNotePlaybackForTrack(id, track->midiNote, 126.0);
                }
                // Délai pour voir le changement ARM->PLAY
                juce::Timer::callAfterDelay(50, [channelPtr]()
                                            {
                if (channelPtr) { // Vérifier que le pointeur est encore valide
                    channelPtr->updateFromTrackData();
                    channelPtr->repaint();
                } });
            };

            mixerChannel->onStopTrack = [this, channelPtr](const juce::String &id)
            {
                if (auto *track = audioProcessor.getTrack(id))
                {
                    track->isPlaying = false;
                    track->isArmed = false;
                }
                channelPtr->updateFromTrackData();
                channelPtr->repaint();
            };

            // === NOUVEAUX CALLBACKS PITCH/FINE/PAN ===
            mixerChannel->onPitchChanged = [this](const juce::String &id, float pitch)
            {
                DBG("🎵 Pitch changed: " + id + " = " + juce::String(pitch, 1) + " BPM");
                // Le TrackData.bpmOffset est déjà mis à jour dans le callback du knob
            };

            mixerChannel->onFineChanged = [this](const juce::String &id, float fine)
            {
                DBG("🎼 Fine changed: " + id + " = " + juce::String(fine, 1) + " cents");
                if (auto *track = audioProcessor.getTrack(id))
                {
                    track->fineOffset = fine;
                }
            };

            mixerChannel->onPanChanged = [this](const juce::String &id, float pan)
            {
                DBG("🔊 Pan changed: " + id + " = " + juce::String(pan, 2));
                // Le TrackData.pan est déjà mis à jour dans le callback du knob
            };

            // === CALLBACK MIDI LEARN ===
            mixerChannel->onMidiLearn = [this](const juce::String &trackId, int controlType)
            {
                DBG("🎹 MIDI Learn requested for track " + trackId + ", control " + juce::String(controlType));
                // TODO: Implémenter MIDI learn
            };

            // === POSITIONNEMENT ===
            // CORRECTION : Utiliser la hauteur du container, pas getHeight()
            int containerHeight = std::max(400, channelsContainer.getHeight()); // Hauteur minimale
            mixerChannel->setBounds(xPos, 0, channelWidth, containerHeight);

            // CORRECTION : Ajouter AVANT de move
            channelsContainer.addAndMakeVisible(mixerChannel.get());
            mixerChannels.push_back(std::move(mixerChannel));

            xPos += channelWidth + channelSpacing;
        }

        // Redimensionner le container avec une hauteur minimale
        int finalHeight = std::max(400, getHeight() - 10);
        channelsContainer.setSize(xPos, finalHeight);

        // Forcer visibilité et repaint
        channelsContainer.setVisible(true);
        channelsContainer.repaint();

        DBG("✅ Mixer refreshed: " + juce::String(mixerChannels.size()) + " channels, container size: " +
            juce::String(channelsContainer.getWidth()) + "x" + juce::String(channelsContainer.getHeight()));
    }

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds();

        // Background mixer style hardware
        juce::ColourGradient gradient(
            juce::Colour(0xff1a1a1a), 0, 0,
            juce::Colour(0xff2a2a2a), 0, getHeight(),
            false);
        g.setGradientFill(gradient);
        g.fillAll();

        // Texture métallique subtile
        g.setColour(juce::Colour(0xff333333));
        for (int i = 0; i < getWidth(); i += 2)
        {
            g.drawVerticalLine(i, 0, getHeight());
            g.setOpacity(0.1f);
        }

        // Séparateur entre channels et master
        int masterX = getWidth() - 100;
        g.setColour(juce::Colour(0xff666666));
        g.drawLine(masterX - 5, 10, masterX - 5, getHeight() - 10, 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Master channel à droite (100px)
        auto masterArea = area.removeFromRight(100);
        masterChannel->setBounds(masterArea.reduced(5));

        // Séparateur
        area.removeFromRight(10);

        // Channels viewport prend le reste
        channelsViewport.setBounds(area);

        // Redimensionner le container des channels
        int containerHeight = channelsViewport.getHeight() - 20;
        channelsContainer.setSize(channelsContainer.getWidth(), containerHeight);

        // Repositionner tous les channels
        int xPos = 5;
        const int channelWidth = 90;
        const int channelSpacing = 5;

        for (auto &channel : mixerChannels)
        {
            channel->setBounds(xPos, 0, channelWidth, containerHeight);
            xPos += channelWidth + channelSpacing;
        }
    }

    // Interface publique
    void trackAdded(const juce::String &trackId)
    {
        refreshMixerChannels();
        resized();
    }

    void trackRemoved(const juce::String &trackId)
    {
        refreshMixerChannels();
        resized();
    }

    void trackSelected(const juce::String &trackId)
    {
        // Highlight la channel correspondante
        for (auto &channel : mixerChannels)
        {
            // TODO: Ajouter méthode setSelected dans MixerChannel
        }
    }

private:
    DjIaVstProcessor &audioProcessor;

    // Master section
    std::unique_ptr<MasterChannel> masterChannel;
    float masterVolume = 0.8f;
    float masterPan = 0.0f;

    // Channels
    juce::Viewport channelsViewport;
    juce::Component channelsContainer;
    std::vector<std::unique_ptr<MixerChannel>> mixerChannels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};