#include "./PluginProcessor.h"
#include "PluginEditor.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(600, 400);
    setupUI();
}

DjIaVstEditor::~DjIaVstEditor()
{
}

void DjIaVstEditor::setupUI()
{
    // Prompt input
    addAndMakeVisible(promptInput);
    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Enter prompt for loop generation...",
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
    generateButton.onClick = [this] { onGenerateButtonClicked(); };

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
    serverUrlInput.setText("http://localhost:8000");
    serverUrlInput.onReturnKey = [this]
    {
        audioProcessor.setServerUrl(serverUrlInput.getText());
        statusLabel.setText("Server URL updated", juce::dontSendNotification);
    };

    addAndMakeVisible(apiKeyLabel);
    apiKeyLabel.setText("API Key:", juce::dontSendNotification);

    addAndMakeVisible(apiKeyInput);
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
}

void DjIaVstEditor::paint(juce::Graphics &g)
{
    // Gradient background moderne
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    auto area = getLocalBounds();
    juce::ColourGradient gradient(juce::Colour(0xff2a2a2a), 0, 0,
                                  juce::Colour(0xff1a1a1a), 0, (float)getHeight(), false);
    g.setGradientFill(gradient);
    g.fillRect(area);
    
    // Titre
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("DJ-IA VST", area.removeFromTop(30), juce::Justification::centred);
}

void DjIaVstEditor::resized()
{
    auto area = getLocalBounds().reduced(15);
    
    // Titre
    area.removeFromTop(35);

    // Section configuration serveur
    auto configArea = area.removeFromTop(80);
    
    // Server URL
    auto serverRow = configArea.removeFromTop(35);
    serverUrlLabel.setBounds(serverRow.removeFromLeft(90));
    serverUrlInput.setBounds(serverRow.reduced(5, 0));

    // API Key  
    auto keyRow = configArea.removeFromTop(35);
    apiKeyLabel.setBounds(keyRow.removeFromLeft(90));
    apiKeyInput.setBounds(keyRow.reduced(5, 0));

    area.removeFromTop(15); // Espacement

    // Section contrôles principaux
    promptInput.setBounds(area.removeFromTop(35));
    area.removeFromTop(10);

    // Ligne des contrôles musicaux
    auto controlRow = area.removeFromTop(35);
    auto styleWidth = controlRow.getWidth() / 3;
    styleSelector.setBounds(controlRow.removeFromLeft(styleWidth).reduced(3));
    keySelector.setBounds(controlRow.removeFromLeft(styleWidth).reduced(3));
    bpmSlider.setBounds(controlRow.reduced(3));

    area.removeFromTop(15);

    // Section stems
    auto stemsRow = area.removeFromTop(35);
    stemsLabel.setBounds(stemsRow.removeFromLeft(90));
    auto stemsArea = stemsRow.reduced(5, 0);
    auto stemWidth = stemsArea.getWidth() / 3;
    drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(2));
    bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(2));
    otherButton.setBounds(stemsArea.reduced(2));

    area.removeFromTop(15);

    // Boutons principaux
    auto buttonsRow = area.removeFromTop(45);
    auto buttonWidth = buttonsRow.getWidth() / 2;
    generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
    playButton.setBounds(buttonsRow.reduced(5));

    area.removeFromTop(10);
    
    // Status en bas
    statusLabel.setBounds(area.removeFromTop(30));
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

    // Lancer la génération dans un thread séparé
    juce::Thread::launch([this]()
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
            audioProcessor.generateLoop(request);
            
            // Succès - mettre à jour l'UI
            juce::MessageManager::callAsync([this]() {
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
            // Erreur - mettre à jour l'UI
            juce::MessageManager::callAsync([this, error = juce::String(e.what())]() {
                statusLabel.setText("Error: " + error, juce::dontSendNotification);
                generateButton.setEnabled(true);
            });
        }
    });
}