#include "./PluginProcessor.h"
#include "PluginEditor.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1200, 800);
    setupUI();

    // Connecter le callback MIDI
    audioProcessor.setMidiIndicatorCallback([this](const juce::String &noteInfo)
                                            { updateMidiIndicator(noteInfo); });

    juce::Timer::callAfterDelay(100, [this]()
                                { updateUIFromProcessor(); });
}

DjIaVstEditor::~DjIaVstEditor()
{
}

void DjIaVstEditor::setupUI()
{
    // Preset selector - NOUVEAU !
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);
    addAndMakeVisible(promptPresetSelector);
    loadPromptPresets();
    promptPresetSelector.onChange = [this]
    { onPresetSelected(); };

    // Save preset button - NOUVEAU !
    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this]
    { onSavePreset(); };

    // Prompt input
    addAndMakeVisible(promptInput);
    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Enter custom prompt or select preset...",
                                       juce::Colours::grey);

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

    // Play button optimisé
    addAndMakeVisible(playButton);
    playButton.setButtonText("Play Loop");
    playButton.setClickingTogglesState(true);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    playButton.onClick = [this]()
    {
        if (playButton.getToggleState())
        {
            audioProcessor.startPlayback();
            playButton.setButtonText("Stop Loop");
            statusLabel.setText("Playing loop", juce::dontSendNotification);
        }
        else
        {
            audioProcessor.stopPlayback();
            playButton.setButtonText("Play Loop");
            statusLabel.setText("Loop stopped", juce::dontSendNotification);
        }
    };

    // Configuration serveur
    addAndMakeVisible(serverUrlLabel);
    serverUrlLabel.setText("Server URL:", juce::dontSendNotification);

    addAndMakeVisible(serverUrlInput);
    serverUrlInput.setText(audioProcessor.getServerUrl()); // Récupérer la valeur sauvegardée
    serverUrlInput.onReturnKey = [this]
    {
        audioProcessor.setServerUrl(serverUrlInput.getText());
        statusLabel.setText("Server URL updated", juce::dontSendNotification);
    };

    addAndMakeVisible(apiKeyLabel);
    apiKeyLabel.setText("API Key:", juce::dontSendNotification);

    addAndMakeVisible(apiKeyInput);
    apiKeyInput.setText(audioProcessor.getApiKey()); // Récupérer la valeur sauvegardée
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
    autoLoadButton.setToggleState(true, juce::dontSendNotification); // Activé par défaut
    autoLoadButton.onClick = [this]
    { onAutoLoadToggled(); };

    // Load Sample button manuel
    addAndMakeVisible(loadSampleButton);
    loadSampleButton.setButtonText("Load Sample");
    loadSampleButton.setEnabled(false); // Désactivé au début (mode auto)
    loadSampleButton.onClick = [this]
    { onLoadSampleClicked(); };

    addAndMakeVisible(testMidiButton);
    testMidiButton.setButtonText("Test MIDI");
    testMidiButton.onClick = [this]
    {
        // Simuler un Note On pour tester
        audioProcessor.startNotePlayback(60); // C4
        updateMidiIndicator("MANUAL: Note ON C4");

        // Arrêter après 1 seconde
        juce::Timer::callAfterDelay(1000, [this]()
                                    {
        audioProcessor.stopNotePlayback();
        updateMidiIndicator("MANUAL: Note OFF C4"); });
    };

    addAndMakeVisible(midiIndicator);
    midiIndicator.setText("MIDI: Waiting...", juce::dontSendNotification);
    midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    midiIndicator.setColour(juce::Label::textColourId, juce::Colours::green);
    midiIndicator.setJustificationType(juce::Justification::centred);
    midiIndicator.setFont(juce::Font(12.0f, juce::Font::bold));

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

    juce::Timer::callAfterDelay(100, [this]()
                                {
    refreshTrackComponents();
    updateSelectedTrack(); });
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
        promptPresetSelector.setSelectedId(promptPresets.size(), juce::dontSendNotification); // "Custom..."
    }

    // Host BPM button
    hostBpmButton.setToggleState(audioProcessor.getHostBpmEnabled(), juce::dontSendNotification);
    if (audioProcessor.getHostBpmEnabled())
    {
        bpmSlider.setEnabled(false);
    }
}

void DjIaVstEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff2d1b3d));
}

void DjIaVstEditor::resized()
{
    auto area = getLocalBounds();
    if (menuBar)
    {
        menuBar->setBounds(area.removeFromTop(24));
    }

    area = area.reduced(15);

    auto configArea = area.removeFromTop(70);

    // Server URL
    auto serverRow = configArea.removeFromTop(30);
    serverUrlLabel.setBounds(serverRow.removeFromLeft(90));
    serverUrlInput.setBounds(serverRow.reduced(5, 0));

    // API Key
    auto keyRow = configArea.removeFromTop(30);
    apiKeyLabel.setBounds(keyRow.removeFromLeft(90));
    apiKeyInput.setBounds(keyRow.reduced(5, 0));

    area.removeFromTop(10);

    // Section PRESETS
    auto presetRow = area.removeFromTop(30);
    promptPresetSelector.setBounds(presetRow.removeFromLeft(presetRow.getWidth() - 80));
    savePresetButton.setBounds(presetRow.reduced(5, 0));

    area.removeFromTop(8);

    // Section prompt
    promptInput.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);

    // Ligne des contrôles musicaux
    auto controlRow = area.removeFromTop(30);
    auto styleWidth = controlRow.getWidth() / 4;
    styleSelector.setBounds(controlRow.removeFromLeft(styleWidth).reduced(3));
    keySelector.setBounds(controlRow.removeFromLeft(styleWidth).reduced(3));
    hostBpmButton.setBounds(controlRow.removeFromLeft(styleWidth).reduced(3));
    bpmSlider.setBounds(controlRow.reduced(3));

    area.removeFromTop(10);

    // Section stems
    auto stemsRow = area.removeFromTop(30);
    stemsLabel.setBounds(stemsRow.removeFromLeft(90));
    auto stemsArea = stemsRow.reduced(5, 0);
    auto stemWidth = stemsArea.getWidth() / 3;
    drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(2));
    bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(2));
    otherButton.setBounds(stemsArea.reduced(2));

    area.removeFromTop(10);

    // Section Tracks
    auto tracksHeaderArea = area.removeFromTop(25);
    tracksLabel.setBounds(tracksHeaderArea.removeFromLeft(80));
    addTrackButton.setBounds(tracksHeaderArea.removeFromRight(100));

    // Viewport des pistes
    auto tracksArea = area.removeFromTop(200);
    tracksViewport.setBounds(tracksArea);
    tracksContainer.setSize(tracksArea.getWidth() - 20, 200);

    // Boutons principaux
    auto buttonsRow = area.removeFromTop(35);
    auto buttonWidth = buttonsRow.getWidth() / 3;
    generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
    playButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
    loadSampleButton.setBounds(buttonsRow.reduced(5));

    area.removeFromTop(8);

    // Auto-Load
    auto autoLoadRow = area.removeFromTop(25);
    autoLoadButton.setBounds(autoLoadRow);

    area.removeFromTop(8);
    auto testRow = area.removeFromTop(25);
    testMidiButton.setBounds(testRow);

    area.removeFromTop(5);

    // Status en bas
    statusLabel.setBounds(area.removeFromTop(25));

    area.removeFromTop(5);

    // Voyant MIDI tout en bas
    midiIndicator.setBounds(area.removeFromTop(20));
}

void DjIaVstEditor::updateMidiIndicator(const juce::String &noteInfo)
{
    lastMidiNote = noteInfo;

    for (auto &trackComp : trackComponents)
    {
        trackComp->updateFromTrackData();
    }

    // Mettre à jour dans le thread UI
    juce::MessageManager::callAsync([this, noteInfo]()
                                    {
        midiIndicator.setText("MIDI: " + noteInfo, juce::dontSendNotification);
        midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::green);
        
        // Faire clignoter en remettant noir après 500ms
        juce::Timer::callAfterDelay(200, [this]() {
            midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
        }); });
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
                        break;
                    }
                }
                statusLabel.setText("Loop generated successfully! Press Play to listen.", 
                    juce::dontSendNotification);
                generateButton.setEnabled(true);
                
                // Réinitialiser le bouton de lecture
                if (playButton.getToggleState()) {
                    playButton.setToggleState(false, juce::dontSendNotification);
                    playButton.setButtonText("Play Loop");
                }
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
    juce::String previousSelectedId = audioProcessor.getSelectedTrackId();

    trackComponents.clear();

    auto trackIds = audioProcessor.getAllTrackIds();
    int yPos = 5;

    for (const auto &trackId : trackIds)
    {
        TrackData *trackData = audioProcessor.getTrack(trackId);
        if (!trackData)
            continue;

        auto trackComp = std::make_unique<TrackComponent>(trackId);
        trackComp->setTrackData(trackData);

        // Callbacks
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
                refreshTrackComponents();
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
            refreshTrackComponents();
        };

        trackComp->setBounds(5, yPos, tracksContainer.getWidth() - 10, 80);

        if (trackId == audioProcessor.getSelectedTrackId())
        {
            trackComp->setSelected(true);
        }

        tracksContainer.addAndMakeVisible(trackComp.get());
        trackComponents.push_back(std::move(trackComp));

        yPos += 85;
    }

    tracksContainer.setSize(tracksViewport.getWidth(), yPos + 5);

    tracksContainer.repaint();
    tracksViewport.repaint();
    repaint();

    for (auto &trackComp : trackComponents)
    {
        if (trackComp->getTrackId() == previousSelectedId)
        {
            trackComp->setSelected(true);
            break;
        }
    }
}

void DjIaVstEditor::onAddTrack()
{
    try
    {
        audioProcessor.createNewTrack();
        refreshTrackComponents();
        statusLabel.setText("New track created", juce::dontSendNotification);
    }
    catch (const std::exception &e)
    {
        statusLabel.setText("Error: " + juce::String(e.what()), juce::dontSendNotification);
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
                .withMessage("DJ-IA VST v1.0\n\nAI-powered music generation plugin\nwith real-time contextual loop creation.\n\nDeveloped with ❤️")
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