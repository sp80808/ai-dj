#include "./PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1300, 800);
    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png,
                                                BinaryData::logo_pngSize);
    DjIaVstProcessor::writeToLog("Logo size in binary: " + juce::String(BinaryData::logo_pngSize));
    DjIaVstProcessor::writeToLog("Logo image valid: " + juce::String(logoImage.isValid() ? "YES" : "NO"));
    setupUI();

    // Connecter le callback MIDI
    juce::WeakReference<DjIaVstEditor> weakThis(this);
    audioProcessor.setMidiIndicatorCallback([weakThis](const juce::String &noteInfo)
                                            {
        if (weakThis != nullptr) { // Vérifier que l'éditeur existe encore
            weakThis->updateMidiIndicator(noteInfo);
        } });
    refreshTracks();
    audioProcessor.onUIUpdateNeeded = [this]()
    {
        updateUIComponents();
    };
}

DjIaVstEditor::~DjIaVstEditor()
{
    audioProcessor.onUIUpdateNeeded = nullptr;
}

void DjIaVstEditor::updateUIComponents()
{
    // 1. Mettre à jour les tracks qui jouent
    for (auto &trackComp : trackComponents)
    {
        if (trackComp->isShowing())
        {
            TrackData *track = audioProcessor.getTrack(trackComp->getTrackId());
            if (track && track->isPlaying.load())
            {
                trackComp->updateFromTrackData();
            }
        }
    }

    // 2. Mettre à jour l'indicateur MIDI (clignotement)
    if (!lastMidiNote.isEmpty())
    {
        // Reset la couleur après un court délai
        static int midiBlinkCounter = 0;
        if (++midiBlinkCounter > 6) // ~200ms à 30fps
        {
            midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
            lastMidiNote.clear();
            midiBlinkCounter = 0;
        }
    }

    // 3. Mettre à jour le BPM host si activé
    if (hostBpmButton.getToggleState())
    {
        double currentHostBpm = audioProcessor.getHostBpm();
        if (currentHostBpm > 0.0 && std::abs(currentHostBpm - bpmSlider.getValue()) > 0.1)
        {
            bpmSlider.setValue(currentHostBpm, juce::dontSendNotification);
        }
    }

    // 4. Mettre à jour le bouton Load Sample si en mode manuel
    if (!autoLoadButton.getToggleState())
    {
        updateLoadButtonState();
    }

    // 5. Mettre à jour les waveforms en temps réel pour les tracks qui jouent
    for (auto &trackComp : trackComponents)
    {
        TrackData *track = audioProcessor.getTrack(trackComp->getTrackId());
        if (track && track->isPlaying.load() && track->numSamples > 0)
        {
            // Calculer la position actuelle pour la waveform
            double startSample = track->loopStart * track->sampleRate;
            double currentTimeInSection = (startSample + track->readPosition.load()) / track->sampleRate;

            // Mettre à jour la position de lecture dans la waveform
            trackComp->updatePlaybackPosition(currentTimeInSection);
        }
    }

    // 6. Vérifier si une génération vient de se terminer
    static bool wasGenerating = false;
    bool isCurrentlyGenerating = generateButton.isEnabled() == false;
    if (wasGenerating && !isCurrentlyGenerating)
    {
        // Une génération vient de se terminer, refresh les waveforms
        for (auto &trackComp : trackComponents)
        {
            trackComp->refreshWaveformIfNeeded();
        }
    }
    wasGenerating = isCurrentlyGenerating;
}

void DjIaVstEditor::refreshTracks()
{
    trackComponents.clear();
    tracksContainer.removeAllChildren();

    refreshTrackComponents();
    updateSelectedTrack();
    repaint();
}

void DjIaVstEditor::timerCallback()
{
    // Mettre à jour seulement les tracks qui jouent
    bool anyTrackPlaying = false;

    for (auto &trackComp : trackComponents)
    {
        if (trackComp->isShowing())
        {
            // Vérifier si cette track est en cours de lecture
            TrackData *track = audioProcessor.getTrack(trackComp->getTrackId());
            if (track && track->isPlaying.load())
            {
                trackComp->updateFromTrackData();
                anyTrackPlaying = true;
            }
        }
    }

    // Si aucune track ne joue, réduire la fréquence
    if (!anyTrackPlaying)
    {
        static int skipFrames = 0;
        skipFrames++;
        if (skipFrames < 10)
            return; // Skip 9 frames sur 10 si rien ne joue
        skipFrames = 0;
    }

    static double lastHostBpm = 0.0;
    double currentHostBpm = audioProcessor.getHostBpm();

    if (std::abs(currentHostBpm - lastHostBpm) > 0.1)
    { // BPM host a changé
        lastHostBpm = currentHostBpm;

        // Mettre à jour toutes les waveforms qui utilisent Host BPM
        for (auto &trackComp : trackComponents)
        {
            TrackData *track = audioProcessor.getTrack(trackComp->getTrackId());
            if (track && (track->timeStretchMode == 3 || track->timeStretchMode == 4))
            {
                trackComp->updateWaveformWithTimeStretch();
            }
        }
    }
}

void DjIaVstEditor::setupUI()
{
    getLookAndFeel().setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3d3d3d));
    getLookAndFeel().setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    getLookAndFeel().setColour(juce::ComboBox::textColourId, juce::Colours::white);
    getLookAndFeel().setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    getLookAndFeel().setColour(juce::TextEditor::textColourId, juce::Colours::white);
    getLookAndFeel().setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2a2a2a));
    getLookAndFeel().setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ff88)); // Vert fluo
    getLookAndFeel().setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));
    // Preset selector
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);
    addAndMakeVisible(promptPresetSelector);
    loadPromptPresets();
    promptPresetSelector.onChange = [this]
    { onPresetSelected(); };

    // Save preset button
    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this]
    { onSavePreset(); };

    // Prompt input
    addAndMakeVisible(promptInput);
    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Enter custom prompt or select preset...",
                                       juce::Colours::grey);

    // BOUTON MAGIQUE DE DEBUG
    addAndMakeVisible(debugRefreshButton);
    debugRefreshButton.setButtonText("Refresh");
    debugRefreshButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    debugRefreshButton.onClick = [this]()
    {
        DjIaVstProcessor::writeToLog("=== MANUAL REFRESH CLICKED ===");

        // FORCER la recréation complète
        refreshTracks();
    };

    // Style selector
    addAndMakeVisible(styleSelector);
    styleSelector.addItem("Techno", 1);
    styleSelector.addItem("House", 2);
    styleSelector.addItem("Ambient", 3);
    styleSelector.addItem("Experimental", 4);
    styleSelector.addItem("Drum & Bass", 5);
    styleSelector.addItem("Minimal", 6);
    styleSelector.setSelectedId(1);

    // BPM slider and label
    addAndMakeVisible(bpmSlider);
    bpmSlider.setRange(60.0, 200.0, 1.0);
    bpmSlider.setValue(126.0);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.attachToComponent(&bpmSlider, true);

    addAndMakeVisible(hostBpmButton);
    hostBpmButton.setButtonText("Sync Host");
    hostBpmButton.setClickingTogglesState(true);
    hostBpmButton.onClick = [this]
    { updateBpmFromHost(); };

    // Key selector
    addAndMakeVisible(keySelector);
    keySelector.addItem("C minor", 1);
    keySelector.addItem("C major", 2);
    keySelector.addItem("G minor", 3);
    keySelector.addItem("F major", 4);
    keySelector.addItem("A minor", 5);
    keySelector.addItem("D minor", 6);
    keySelector.setSelectedId(1);

    // Generate button
    addAndMakeVisible(generateButton);
    generateButton.setButtonText("Generate Loop");
    generateButton.onClick = [this]
    { onGenerateButtonClicked(); };

    // Configuration serveur
    addAndMakeVisible(serverUrlLabel);
    serverUrlLabel.setText("Server URL:", juce::dontSendNotification);

    addAndMakeVisible(serverUrlInput);
    serverUrlInput.setText(audioProcessor.getServerUrl());
    serverUrlInput.onReturnKey = [this]
    {
        audioProcessor.setServerUrl(serverUrlInput.getText());
        statusLabel.setText("Server URL updated", juce::dontSendNotification);
    };

    addAndMakeVisible(apiKeyLabel);
    apiKeyLabel.setText("API Key:", juce::dontSendNotification);

    addAndMakeVisible(apiKeyInput);
    apiKeyInput.setText(audioProcessor.getApiKey());
    apiKeyInput.setPasswordCharacter('•');
    apiKeyInput.onReturnKey = [this]
    {
        audioProcessor.setApiKey(apiKeyInput.getText());
        statusLabel.setText("API Key updated", juce::dontSendNotification);
    };

    // Stems selection
    addAndMakeVisible(stemsLabel);
    stemsLabel.setText("Stems:", juce::dontSendNotification);

    addAndMakeVisible(drumsButton);
    drumsButton.setButtonText("Drums");
    drumsButton.setClickingTogglesState(true);

    addAndMakeVisible(bassButton);
    bassButton.setButtonText("Bass");
    bassButton.setClickingTogglesState(true);

    addAndMakeVisible(otherButton);
    otherButton.setButtonText("Other");
    otherButton.setClickingTogglesState(true);

    // Status label
    addAndMakeVisible(statusLabel);
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(autoLoadButton);
    autoLoadButton.setButtonText("Auto-Load Samples");
    autoLoadButton.setClickingTogglesState(true);
    autoLoadButton.setToggleState(true, juce::dontSendNotification);
    autoLoadButton.onClick = [this]
    { onAutoLoadToggled(); };

    // Load Sample button manuel
    addAndMakeVisible(loadSampleButton);
    loadSampleButton.setButtonText("Load Sample");
    loadSampleButton.setEnabled(false);
    loadSampleButton.onClick = [this]
    { onLoadSampleClicked(); };

    addAndMakeVisible(midiIndicator);
    midiIndicator.setText("MIDI: Waiting...", juce::dontSendNotification);
    midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    midiIndicator.setColour(juce::Label::textColourId, juce::Colours::green);
    midiIndicator.setJustificationType(juce::Justification::centred);
    midiIndicator.setFont(juce::Font(12.0f, juce::Font::bold));

    // Instructions MIDI
    addAndMakeVisible(midiInstructionLabel);
    midiInstructionLabel.setText("Play tracks with MIDI notes (C3-B3) in sync with your DAW!",
                                 juce::dontSendNotification);
    midiInstructionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    midiInstructionLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    midiInstructionLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(tracksLabel);
    tracksLabel.setText("Tracks:", juce::dontSendNotification);
    tracksLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    addAndMakeVisible(addTrackButton);
    addTrackButton.setButtonText("+ Add Track");
    addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    addTrackButton.onClick = [this]()
    { onAddTrack(); };

    // Viewport pour les pistes
    addAndMakeVisible(tracksViewport);
    tracksViewport.setViewedComponent(&tracksContainer, false);
    tracksViewport.setScrollBarsShown(true, false);

    // Session management
    addAndMakeVisible(saveSessionButton);
    saveSessionButton.setButtonText("Save Session");
    saveSessionButton.onClick = [this]()
    { onSaveSession(); };

    addAndMakeVisible(loadSessionButton);
    loadSessionButton.setButtonText("Load Session");
    loadSessionButton.onClick = [this]()
    { onLoadSession(); };

    mixerPanel = std::make_unique<MixerPanel>(audioProcessor);
    addAndMakeVisible(*mixerPanel);

    refreshTrackComponents();

    // Prompt input - ajouter callback
    promptInput.onTextChange = [this]
    {
        audioProcessor.setLastPrompt(promptInput.getText());
    };

    // Style selector - ajouter callback
    styleSelector.onChange = [this]
    {
        audioProcessor.setLastStyle(styleSelector.getText());
    };

    // Key selector - ajouter callback
    keySelector.onChange = [this]
    {
        audioProcessor.setLastKey(keySelector.getText());
    };

    // BPM slider - ajouter callback
    bpmSlider.onValueChange = [this]
    {
        audioProcessor.setLastBpm(bpmSlider.getValue());
    };

    // Preset selector - modifier le callback existant
    promptPresetSelector.onChange = [this]
    {
        onPresetSelected();
        audioProcessor.setLastPresetIndex(promptPresetSelector.getSelectedId() - 1);
    };

    // Host BPM button - modifier le callback existant
    hostBpmButton.onClick = [this]
    {
        updateBpmFromHost();
        audioProcessor.setHostBpmEnabled(hostBpmButton.getToggleState());
    };

    generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa44)); // Vert foncé
    generateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0066cc)); // Bleu

    loadSampleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff666666)); // Gris

    // Status avec fond noir
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff000000));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff88));
}

void DjIaVstEditor::updateUIFromProcessor()
{
    // Configuration serveur
    serverUrlInput.setText(audioProcessor.getServerUrl(), juce::dontSendNotification);
    apiKeyInput.setText(audioProcessor.getApiKey(), juce::dontSendNotification);

    // Contrôles musicaux
    promptInput.setText(audioProcessor.getLastPrompt(), juce::dontSendNotification);
    bpmSlider.setValue(audioProcessor.getLastBpm(), juce::dontSendNotification);

    // Style selector
    juce::String style = audioProcessor.getLastStyle();
    for (int i = 1; i <= styleSelector.getNumItems(); ++i)
    {
        if (styleSelector.getItemText(i - 1) == style)
        {
            styleSelector.setSelectedId(i, juce::dontSendNotification);
            break;
        }
    }

    // Key selector
    juce::String key = audioProcessor.getLastKey();
    for (int i = 1; i <= keySelector.getNumItems(); ++i)
    {
        if (keySelector.getItemText(i - 1) == key)
        {
            keySelector.setSelectedId(i, juce::dontSendNotification);
            break;
        }
    }

    // Preset selector
    int presetIndex = audioProcessor.getLastPresetIndex();
    if (presetIndex >= 0 && presetIndex < promptPresets.size())
    {
        promptPresetSelector.setSelectedId(presetIndex + 1, juce::dontSendNotification);
    }
    else
    {
        promptPresetSelector.setSelectedId(promptPresets.size(), juce::dontSendNotification);
    }

    // Host BPM button
    hostBpmButton.setToggleState(audioProcessor.getHostBpmEnabled(), juce::dontSendNotification);
    if (audioProcessor.getHostBpmEnabled())
    {
        bpmSlider.setEnabled(false);
    }
    DjIaVstProcessor::writeToLog("=== updateUIFromProcessor called ===");
    refreshTrackComponents();
}

void DjIaVstEditor::paint(juce::Graphics &g)
{
    // Thème dark pro au lieu du violet dégueulasse
    auto bounds = getLocalBounds();

    // Gradient dark moderne
    juce::ColourGradient gradient(
        juce::Colour(0xff1a1a1a), 0, 0,           // Noir foncé en haut
        juce::Colour(0xff2d2d2d), 0, getHeight(), // Gris anthracite en bas
        false);
    g.setGradientFill(gradient);
    g.fillAll();

    // Ajout de texture subtile (optionnel)
    g.setColour(juce::Colour(0xff404040));
    for (int i = 0; i < getWidth(); i += 3)
    {
        g.drawVerticalLine(i, 0, getHeight());
        g.setOpacity(0.02f); // Très subtil
    }
    if (logoImage.isValid())
    {
        auto logoArea = juce::Rectangle<int>(getWidth() - 150, 10, 130, 50);
        g.drawImage(logoImage, logoArea.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        // FALLBACK pour voir si c'est un problème d'image ou de zone
        auto logoArea = juce::Rectangle<int>(getWidth() - 150, 10, 130, 50);
        g.setColour(juce::Colours::red);
        g.drawRect(logoArea, 2);
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("OBSIDIAN", logoArea, juce::Justification::centred);
    }
}

void DjIaVstEditor::resized()
{
    static bool resizing = false;
    if (resizing)
        return;
    resizing = true;

    auto area = getLocalBounds();

    if (menuBar)
    {
        menuBar->setBounds(area.removeFromTop(24));
    }

    area = area.reduced(10);

    // Configuration (80px) - IDENTIQUE
    auto configArea = area.removeFromTop(80);

    auto serverRow = configArea.removeFromTop(25);
    serverUrlLabel.setBounds(serverRow.removeFromLeft(80));
    serverUrlInput.setBounds(serverRow.reduced(2));

    configArea.removeFromTop(5);

    auto keyRow = configArea.removeFromTop(25);
    apiKeyLabel.setBounds(keyRow.removeFromLeft(80));
    apiKeyInput.setBounds(keyRow.reduced(2));

    configArea.removeFromTop(5);

    auto instructionRow = configArea.removeFromTop(20);
    midiInstructionLabel.setBounds(instructionRow);

    area.removeFromTop(10);

    // Presets - IDENTIQUE
    auto presetRow = area.removeFromTop(35);
    promptPresetSelector.setBounds(presetRow.removeFromLeft(presetRow.getWidth() - 80));
    presetRow.removeFromLeft(5);
    savePresetButton.setBounds(presetRow);

    area.removeFromTop(5);

    // Prompt - IDENTIQUE
    promptInput.setBounds(area.removeFromTop(35));
    area.removeFromTop(5);

    // Contrôles musicaux - IDENTIQUE
    auto controlRow = area.removeFromTop(35);
    auto controlWidth = controlRow.getWidth() / 4;

    styleSelector.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
    keySelector.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
    hostBpmButton.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
    bpmSlider.setBounds(controlRow.reduced(2));

    area.removeFromTop(8);

    // Stems - IDENTIQUE
    auto stemsRow = area.removeFromTop(30);
    stemsLabel.setBounds(stemsRow.removeFromLeft(60));
    auto stemsArea = stemsRow.reduced(2);
    auto stemWidth = stemsArea.getWidth() / 3;
    drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
    bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
    otherButton.setBounds(stemsArea.reduced(1));

    area.removeFromTop(8);

    auto tracksHeaderArea = area.removeFromTop(30);
    addTrackButton.setBounds(tracksHeaderArea.removeFromRight(100));
    tracksHeaderArea.removeFromRight(5);
    debugRefreshButton.setBounds(tracksHeaderArea.removeFromRight(150));

    area.removeFromTop(10);

    auto tracksAndMixerArea = area.removeFromTop(area.getHeight() - 80);

    // Diviser : 65% tracks, 35% mixer
    auto tracksMainArea = tracksAndMixerArea.removeFromLeft(tracksAndMixerArea.getWidth() * 0.65f);

    // TRACKS : occupent leur zone à 100%
    tracksViewport.setBounds(tracksMainArea);

    // MIXER : prend le reste
    tracksAndMixerArea.removeFromLeft(5); // Petit espace
    if (mixerPanel)
    {
        mixerPanel->setBounds(tracksAndMixerArea);
        mixerPanel->setVisible(true);
    }

    // Boutons en bas - GARDÉS
    auto buttonsRow = area.removeFromTop(35);
    auto buttonWidth = buttonsRow.getWidth() / 2;
    generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
    loadSampleButton.setBounds(buttonsRow.reduced(5));

    area.removeFromTop(3);

    // Auto-Load - GARDÉ
    autoLoadButton.setBounds(area.removeFromTop(20));

    area.removeFromTop(2);

    // Status + MIDI - GARDÉS
    statusLabel.setBounds(area.removeFromTop(25));
    midiIndicator.setBounds(area.removeFromTop(20));

    resizing = false;
}

void DjIaVstEditor::toggleMixer()
{
    mixerVisible = !mixerVisible;
    showMixerButton.setToggleState(mixerVisible, juce::dontSendNotification);

    if (mixerVisible)
    {
        showMixerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa44)); // Vert
        statusLabel.setText("Mixer panel opened", juce::dontSendNotification);
    }
    else
    {
        showMixerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a4a4a)); // Gris
        statusLabel.setText("Mixer panel closed", juce::dontSendNotification);
    }

    resized(); // Refaire le layout
}

void DjIaVstEditor::updateMidiIndicator(const juce::String &noteInfo)
{
    lastMidiNote = noteInfo;

    // Mettre à jour dans le thread UI
    juce::MessageManager::callAsync([this, noteInfo]()
                                    {
        if (midiIndicator.isShowing())
        {
            midiIndicator.setText("MIDI: " + noteInfo, juce::dontSendNotification);
            midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::green);
            
            juce::Timer::callAfterDelay(200, [this]() 
            {
                if (midiIndicator.isShowing())
                {
                    midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
                }
            });
        } });

    // Mettre à jour les tracks immédiatement
    for (auto &trackComp : trackComponents)
    {
        if (trackComp->isShowing())
        {
            trackComp->updateFromTrackData();
        }
    }
}

void DjIaVstEditor::onGenerateButtonClicked()
{
    // Validation des inputs
    if (serverUrlInput.getText().isEmpty())
    {
        statusLabel.setText("Error: Server URL is required", juce::dontSendNotification);
        return;
    }

    if (apiKeyInput.getText().isEmpty())
    {
        statusLabel.setText("Error: API Key is required", juce::dontSendNotification);
        return;
    }

    if (promptInput.getText().isEmpty())
    {
        statusLabel.setText("Error: Prompt is required", juce::dontSendNotification);
        return;
    }

    // Désactiver le bouton pendant la génération
    generateButton.setEnabled(false);
    statusLabel.setText("Connecting to server...", juce::dontSendNotification);

    juce::String selectedTrackId = audioProcessor.getSelectedTrackId();
    for (auto &trackComp : trackComponents)
    {
        if (trackComp->getTrackId() == selectedTrackId)
        {
            trackComp->startGeneratingAnimation();
            break;
        }
    }

    // Lancer la génération dans un thread séparé
    juce::Thread::launch([this, selectedTrackId]()
                         {
        try
        {
            // Mettre à jour le status
            juce::MessageManager::callAsync([this]() {
                statusLabel.setText("Generating loop (this may take a few minutes)...", 
                    juce::dontSendNotification);
            });

            // Configurer le client API
            audioProcessor.setServerUrl(serverUrlInput.getText());
            audioProcessor.setApiKey(apiKeyInput.getText());

            // Attendre un peu pour la configuration
            juce::Thread::sleep(100);

            // Créer la requête
            DjIaClient::LoopRequest request;
            request.prompt = promptInput.getText();
            request.style = styleSelector.getText();
            request.bpm = (float)bpmSlider.getValue();
            request.key = keySelector.getText();
            request.measures = 4;

            // Collecter les stems sélectionnés
            if (drumsButton.getToggleState())
                request.preferredStems.push_back("drums");
            if (bassButton.getToggleState())
                request.preferredStems.push_back("bass");
            if (otherButton.getToggleState())
                request.preferredStems.push_back("other");

            // Générer la loop
            juce::String targetTrackId = audioProcessor.getSelectedTrackId();
            audioProcessor.generateLoop(request, targetTrackId);
            
            // Succès - mettre à jour l'UI
            juce::MessageManager::callAsync([this, selectedTrackId]() {
            for (auto& trackComp : trackComponents) {
                if (trackComp->getTrackId() == selectedTrackId) {
                    trackComp->stopGeneratingAnimation();
                    
                    // Forcer un rafraîchissement complet du composant
                    trackComp->updateFromTrackData();
                    trackComp->repaint();
                    
                    break;
                }
            }
            statusLabel.setText("Loop generated successfully! Press Play to listen.", 
                juce::dontSendNotification);
            generateButton.setEnabled(true);

        });
        }
        catch (const std::exception& e)
        {
            juce::MessageManager::callAsync([this, selectedTrackId, error = juce::String(e.what())]() {
                for (auto& trackComp : trackComponents) {
                    if (trackComp->getTrackId() == selectedTrackId) {
                        trackComp->stopGeneratingAnimation();
                        break;
                    }
                }
                
                statusLabel.setText("Error: " + error, juce::dontSendNotification);
                generateButton.setEnabled(true);
            });
        } });
}

// NOUVELLES FONCTIONS PRESETS
void DjIaVstEditor::loadPromptPresets()
{
    promptPresetSelector.clear();
    for (int i = 0; i < promptPresets.size(); ++i)
    {
        promptPresetSelector.addItem(promptPresets[i], i + 1);
    }
    promptPresetSelector.setSelectedId(promptPresets.size()); // "Custom..." par défaut
}

void DjIaVstEditor::onPresetSelected()
{
    int selectedId = promptPresetSelector.getSelectedId();
    if (selectedId > 0 && selectedId < promptPresets.size()) // Pas "Custom..."
    {
        juce::String selectedPrompt = promptPresets[selectedId - 1];
        promptInput.setText(selectedPrompt);
        statusLabel.setText("Preset loaded: " + selectedPrompt, juce::dontSendNotification);
    }
    else
    {
        promptInput.clear();
        statusLabel.setText("Custom prompt mode", juce::dontSendNotification);
    }
}

void DjIaVstEditor::onSavePreset()
{
    juce::String currentPrompt = promptInput.getText().trim();
    if (currentPrompt.isNotEmpty())
    {
        // Demander le nom du preset
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Save Preset")
                .withMessage("Enter name for this prompt preset:")
                .withButton("Save")
                .withButton("Cancel"),
            [this, currentPrompt](int result)
            {
                if (result == 1) // Save clicked
                {
                    // Ajouter le nouveau preset (avant "Custom...")
                    promptPresets.insert(promptPresets.size() - 1, currentPrompt);
                    loadPromptPresets();
                    statusLabel.setText("Preset saved: " + currentPrompt, juce::dontSendNotification);
                }
            });
    }
    else
    {
        statusLabel.setText("Enter a prompt first!", juce::dontSendNotification);
    }
}

void DjIaVstEditor::updateBpmFromHost()
{
    if (hostBpmButton.getToggleState())
    {
        // Récupérer le BPM de l'hôte
        double hostBpm = audioProcessor.getHostBpm();

        if (hostBpm > 0.0)
        {
            bpmSlider.setValue(hostBpm, juce::dontSendNotification);
            bpmSlider.setEnabled(false); // Désactiver le slider
            statusLabel.setText("BPM synced with host: " + juce::String(hostBpm, 1), juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("Host BPM not available", juce::dontSendNotification);
            hostBpmButton.setToggleState(false, juce::dontSendNotification);
        }
    }
    else
    {
        bpmSlider.setEnabled(true); // Réactiver le slider
        statusLabel.setText("Using manual BPM", juce::dontSendNotification);
    }
}

void DjIaVstEditor::onAutoLoadToggled()
{
    bool autoLoadOn = autoLoadButton.getToggleState();
    audioProcessor.setAutoLoadEnabled(autoLoadOn);

    // Activer/désactiver le bouton manuel
    loadSampleButton.setEnabled(!autoLoadOn);

    if (autoLoadOn)
    {
        statusLabel.setText("Auto-load enabled - samples load automatically", juce::dontSendNotification);
        loadSampleButton.setButtonText("Load Sample");
    }
    else
    {
        statusLabel.setText("Manual mode - click Load Sample when ready", juce::dontSendNotification);
        updateLoadButtonState(); // Vérifier s'il y a un sample en attente
    }
}

void DjIaVstEditor::onLoadSampleClicked()
{
    audioProcessor.loadPendingSample();
    statusLabel.setText("Sample loaded manually!", juce::dontSendNotification);
    updateLoadButtonState();
}

void DjIaVstEditor::updateLoadButtonState()
{
    if (!autoLoadButton.getToggleState()) // Mode manuel
    {
        if (audioProcessor.hasSampleWaiting())
        {
            loadSampleButton.setButtonText("Load Sample (Ready!)");
            loadSampleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        }
        else
        {
            loadSampleButton.setButtonText("Load Sample");
            loadSampleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
        }
    }
}

void DjIaVstEditor::refreshTrackComponents()
{
    auto trackIds = audioProcessor.getAllTrackIds();
    DjIaVstProcessor::writeToLog("=== refreshTrackComponents called ===");
    DjIaVstProcessor::writeToLog("Processor has " + juce::String(trackIds.size()) + " tracks");
    DjIaVstProcessor::writeToLog("Current trackComponents.size(): " + juce::String(trackComponents.size()));
    DjIaVstProcessor::writeToLog("tracksContainer visible: " + juce::String(tracksContainer.isVisible() ? "YES" : "NO"));
    DjIaVstProcessor::writeToLog("tracksViewport visible: " + juce::String(tracksViewport.isVisible() ? "YES" : "NO"));

    for (int i = 0; i < trackIds.size(); ++i)
    {
        DjIaVstProcessor::writeToLog("  Track " + juce::String(i) + " ID: " + trackIds[i]);
    }
    if (trackComponents.size() == trackIds.size())
    {
        // Vérifier si les composants sont vraiment affichés
        bool allVisible = true;
        for (auto &comp : trackComponents)
        {
            if (!comp->isVisible() || comp->getParentComponent() == nullptr)
            {
                allVisible = false;
                break;
            }
        }
        if (allVisible)
        {
            for (int i = 0; i < trackComponents.size() && i < trackIds.size(); ++i)
            {
                trackComponents[i]->setTrackData(audioProcessor.getTrack(trackIds[i]));
                trackComponents[i]->updateFromTrackData();
            }
            updateSelectedTrack();
            return;
        }
        DjIaVstProcessor::writeToLog("Components exist but not visible - forcing recreation");
    }

    setEnabled(false);
    juce::String previousSelectedId = audioProcessor.getSelectedTrackId();

    trackComponents.clear();
    tracksContainer.removeAllChildren();
    int yPos = 5;

    for (const auto &trackId : trackIds)
    {
        TrackData *trackData = audioProcessor.getTrack(trackId);
        if (!trackData)
            continue;

        auto trackComp = std::make_unique<TrackComponent>(trackId, audioProcessor);
        trackComp->setTrackData(trackData);

        // Callbacks (identiques)
        trackComp->onSelectTrack = [this](const juce::String &id)
        {
            audioProcessor.selectTrack(id);
            updateSelectedTrack();
        };

        trackComp->onDeleteTrack = [this](const juce::String &id)
        {
            if (audioProcessor.getAllTrackIds().size() > 1)
            {
                audioProcessor.deleteTrack(id);
                juce::Timer::callAfterDelay(10, [this]()
                                            { refreshTrackComponents(); });
            }
        };

        trackComp->onGenerateForTrack = [this](const juce::String &id)
        {
            audioProcessor.selectTrack(id);
            for (auto &comp : trackComponents)
            {
                if (comp->getTrackId() == id)
                {
                    comp->startGeneratingAnimation();
                    break;
                }
            }
            onGenerateButtonClicked();
        };

        trackComp->onReorderTrack = [this](const juce::String &fromId, const juce::String &toId)
        {
            audioProcessor.reorderTracks(fromId, toId);
            juce::Timer::callAfterDelay(10, [this]()
                                        { refreshTrackComponents(); });
        };

        int fullWidth = tracksContainer.getWidth() - 4;
        trackComp->setBounds(2, yPos, fullWidth, 80);

        if (trackId == audioProcessor.getSelectedTrackId())
        {
            trackComp->setSelected(true);
        }

        tracksContainer.addAndMakeVisible(trackComp.get());
        trackComponents.push_back(std::move(trackComp));

        yPos += 85;
    }

    // Redimensionner EN UNE FOIS
    tracksContainer.setSize(tracksViewport.getWidth() - 20, yPos + 5);

    DjIaVstProcessor::writeToLog("trackComponents created: " + juce::String(trackComponents.size()));
    DjIaVstProcessor::writeToLog("tracksContainer size: " + juce::String(tracksContainer.getWidth()) + "x" + juce::String(tracksContainer.getHeight()));
    if (mixerPanel)
    {
        mixerPanel->refreshMixerChannels();
    }

    setEnabled(true);
    juce::MessageManager::callAsync([this]()
                                    {
        resized();
        repaint(); });
    tracksContainer.repaint();
}

void DjIaVstEditor::onAddTrack()
{
    try
    {
        juce::String newTrackId = audioProcessor.createNewTrack();
        refreshTrackComponents();

        if (mixerPanel)
        {
            mixerPanel->trackAdded(newTrackId);
        }

        statusLabel.setText("New track created", juce::dontSendNotification);
    }
    catch (const std::exception &e)
    {
        statusLabel.setText("Error: " + juce::String(e.what()), juce::dontSendNotification);
    }
}

void DjIaVstEditor::onDeleteTrack(const juce::String &trackId)
{
    if (audioProcessor.getAllTrackIds().size() > 1)
    {
        audioProcessor.deleteTrack(trackId);

        // Notifier le mixer
        if (mixerPanel)
        {
            mixerPanel->trackRemoved(trackId);
        }

        juce::Timer::callAfterDelay(10, [this]()
                                    { refreshTrackComponents(); });
    }
}

void DjIaVstEditor::updateSelectedTrack()
{
    for (auto &trackComp : trackComponents)
    {
        trackComp->setSelected(false);
    }

    juce::String selectedId = audioProcessor.getSelectedTrackId();
    auto trackIds = audioProcessor.getAllTrackIds();
    for (int i = 0; i < trackIds.size() && i < trackComponents.size(); ++i)
    {
        if (trackIds[i] == selectedId)
        {
            trackComponents[i]->setSelected(true);
            break;
        }
    }
    if (mixerPanel)
    {
        mixerPanel->trackSelected(selectedId);
    }
}

void DjIaVstEditor::onSaveSession()
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Save Session")
            .withMessage("Enter session name:")
            .withButton("Save")
            .withButton("Cancel"),
        [this](int result)
        {
            if (result == 1)
            {
                juce::String sessionName = "Session_" + juce::String(juce::Time::getCurrentTime().toMilliseconds());

                // Demander le nom via AlertWindow avec TextEditor
                auto alertWindow = std::make_unique<juce::AlertWindow>("Save Session",
                                                                       "Enter session name:", juce::MessageBoxIconType::QuestionIcon);
                alertWindow->addTextEditor("sessionName", sessionName, "Session name:");
                alertWindow->addButton("Save", 1);
                alertWindow->addButton("Cancel", 0);

                alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([this](int modalResult)
                                                                                       {
                    if (modalResult == 1) {
                        auto* nameEditor = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())
                                           ->getTextEditor("sessionName");
                        if (nameEditor) {
                            saveCurrentSession(nameEditor->getText());
                        }
                    } }));
            }
        });
}

void DjIaVstEditor::saveCurrentSession(const juce::String &sessionName)
{
    try
    {
        juce::File sessionsDir = getSessionsDirectory();
        if (!sessionsDir.exists())
        {
            sessionsDir.createDirectory();
        }

        juce::File sessionFile = sessionsDir.getChildFile(sessionName + ".djiasession");

        // Sauvegarder état complet
        juce::MemoryBlock stateData;
        audioProcessor.getStateInformation(stateData);

        juce::FileOutputStream stream(sessionFile);
        if (stream.openedOk())
        {
            stream.write(stateData.getData(), stateData.getSize());
            statusLabel.setText("Session saved: " + sessionName, juce::dontSendNotification);
            loadSessionList(); // Refresh la liste
        }
        else
        {
            statusLabel.setText("Failed to save session file", juce::dontSendNotification);
        }
    }
    catch (const std::exception &e)
    {
        statusLabel.setText("Failed to save session: " + juce::String(e.what()),
                            juce::dontSendNotification);
    }
}

void DjIaVstEditor::onLoadSession()
{
    int selectedIndex = sessionSelector.getSelectedItemIndex();
    if (selectedIndex >= 0)
    {
        juce::String sessionName = sessionSelector.getItemText(selectedIndex);
        if (sessionName != "No sessions found")
        {
            loadSession(sessionName);
        }
    }
    else
    {
        statusLabel.setText("Please select a session to load", juce::dontSendNotification);
    }
}

void DjIaVstEditor::loadSession(const juce::String &sessionName)
{
    try
    {
        juce::File sessionFile = getSessionsDirectory().getChildFile(sessionName + ".djiasession");

        if (sessionFile.existsAsFile())
        {
            juce::FileInputStream stream(sessionFile);
            if (stream.openedOk())
            {
                juce::MemoryBlock stateData;
                stream.readIntoMemoryBlock(stateData);

                audioProcessor.setStateInformation(stateData.getData(),
                                                   static_cast<int>(stateData.getSize()));

                refreshTrackComponents();
                updateUIFromProcessor();
                statusLabel.setText("Session loaded: " + sessionName, juce::dontSendNotification);
            }
            else
            {
                statusLabel.setText("Failed to read session file", juce::dontSendNotification);
            }
        }
        else
        {
            statusLabel.setText("Session file not found: " + sessionName, juce::dontSendNotification);
        }
    }
    catch (const std::exception &e)
    {
        statusLabel.setText("Failed to load session: " + juce::String(e.what()),
                            juce::dontSendNotification);
    }
}

void DjIaVstEditor::loadSessionList()
{
    sessionSelector.clear();

    juce::File sessionsDir = getSessionsDirectory();
    if (sessionsDir.exists())
    {
        auto sessionFiles = sessionsDir.findChildFiles(juce::File::findFiles, false, "*.djiasession");

        for (const auto &file : sessionFiles)
        {
            sessionSelector.addItem(file.getFileNameWithoutExtension(),
                                    sessionSelector.getNumItems() + 1);
        }
    }

    if (sessionSelector.getNumItems() == 0)
    {
        sessionSelector.addItem("No sessions found", 1);
    }
    else
    {
        sessionSelector.setSelectedItemIndex(0); // Sélectionner le premier
    }
}

juce::File DjIaVstEditor::getSessionsDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("DJ-IA VST")
        .getChildFile("Sessions");
}

juce::StringArray DjIaVstEditor::getMenuBarNames()
{
    return {"File", "Tracks", "Help"};
}

juce::PopupMenu DjIaVstEditor::getMenuForIndex(int topLevelMenuIndex, const juce::String &menuName)
{
    juce::PopupMenu menu;

    if (topLevelMenuIndex == 0)
    { // File
        menu.addItem(newSession, "New Session", true);
        menu.addSeparator();
        menu.addItem(saveSession, "Save Session", true);
        menu.addItem(saveSessionAs, "Save Session As...", true);
        menu.addItem(loadSessionMenu, "Load Session...", true);
        menu.addSeparator();
        menu.addItem(exportSession, "Export Session", true);
    }
    else if (topLevelMenuIndex == 1)
    { // Tracks
        menu.addItem(addTrack, "Add New Track", true);
        menu.addSeparator();
        menu.addItem(deleteAllTracks, "Delete All Tracks", audioProcessor.getAllTrackIds().size() > 1);
        menu.addItem(resetTracks, "Reset All Tracks", true);
    }
    else if (topLevelMenuIndex == 2)
    { // Help
        menu.addItem(aboutDjIa, "About DJ-IA", true);
        menu.addItem(showHelp, "Show Help", true);
    }

    return menu;
}

void DjIaVstEditor::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    switch (menuItemID)
    {
    case newSession:
        // Clear tout et créer nouvelle session
        statusLabel.setText("New session created", juce::dontSendNotification);
        break;

    case saveSession:
        onSaveSession();
        break;

    case saveSessionAs:
        onSaveSession(); // Même fonction pour l'instant
        break;

    case loadSessionMenu:
        onLoadSession();
        break;

    case exportSession:
        statusLabel.setText("Export - Coming soon!", juce::dontSendNotification);
        break;

    case addTrack:
        onAddTrack();
        break;

    case deleteAllTracks:
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete All Tracks")
                .withMessage("Are you sure you want to delete all tracks?")
                .withButton("Delete")
                .withButton("Cancel"),
            [this](int result)
            {
                if (result == 1)
                { // Delete clicked
                    auto trackIds = audioProcessor.getAllTrackIds();
                    for (int i = 1; i < trackIds.size(); ++i)
                    {
                        audioProcessor.deleteTrack(trackIds[i]);
                    }
                    refreshTrackComponents();
                    statusLabel.setText("All tracks deleted except one", juce::dontSendNotification);
                }
            });
        break;

    case resetTracks:
        statusLabel.setText("Reset tracks - Coming soon!", juce::dontSendNotification);
        break;

    case aboutDjIa:
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("About DJ-IA VST")
                .withMessage("DJ-IA VST v1.0\n\nAI-powered music generation plugin\nwith real-time contextual loop creation.\n\nDeveloped with love.")
                .withButton("OK"),
            nullptr);
        break;

    case showHelp:
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("DJ-IA Help")
                .withMessage("Quick Start:\n"
                             "1. Configure server URL and API key\n"
                             "2. Add tracks and assign MIDI notes\n"
                             "3. Generate loops with prompts\n"
                             "4. Play with MIDI keyboard!\n\n"
                             "Each track can be triggered by its assigned MIDI note.")
                .withButton("OK"),
            nullptr);
        break;
    }
}