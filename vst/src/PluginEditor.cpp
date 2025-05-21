#include "./PluginProcessor.h"
#include "PluginEditor.h"
#include "DjIaClient.h"

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
    // ... ajouter d'autres tonalités ...
    keySelector.setSelectedId(1);

    // Generate button
    addAndMakeVisible(generateButton);
    generateButton.setButtonText("Generate Loop");
    generateButton.onClick = [this]
    { onGenerateButtonClicked(); };

    // Bouton de lecture MIDI
    // Dans setupUI(), modifie la configuration du bouton de lecture comme ceci:
    addAndMakeVisible(playButton);
    playButton.setButtonText("Play Loop");
    playButton.setClickingTogglesState(true); // Utilise cette méthode au lieu de setToggleable
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    playButton.onClick = [this]()
    {
        if (playButton.getToggleState())
        {
            // Bouton actif = lecture
            audioProcessor.startPlayback();
            playButton.setButtonText("Stop Loop");
            statusLabel.setText("Playing loop", juce::dontSendNotification);
        }
        else
        {
            // Bouton inactif = arrêt
            audioProcessor.stopPlayback();
            playButton.setButtonText("Play Loop");
            statusLabel.setText("Loop stopped", juce::dontSendNotification);
        }
    };
    playButton.onStateChange = [this]()
    {
        audioProcessor.writeToLog("Button state changed: " +
                                  juce::String(playButton.getToggleState() ? "ON" : "OFF"));
    };

    addAndMakeVisible(serverUrlLabel);
    serverUrlLabel.setText("Server URL:", juce::dontSendNotification);

    addAndMakeVisible(serverUrlInput);
    serverUrlInput.setText("http://localhost:8000");
    serverUrlInput.onReturnKey = [this]
    {
        audioProcessor.setServerUrl(serverUrlInput.getText());
    };

    addAndMakeVisible(apiKeyLabel);
    apiKeyLabel.setText("API Key:", juce::dontSendNotification);

    addAndMakeVisible(apiKeyInput);
    apiKeyInput.setPasswordCharacter('•'); // Utilise des points pour cacher la clé
    apiKeyInput.onReturnKey = [this]
    {
        audioProcessor.setApiKey(apiKeyInput.getText());
    };

    addAndMakeVisible(stemsLabel);
    stemsLabel.setText("Stems:", juce::dontSendNotification);

    addAndMakeVisible(drumsButton);
    drumsButton.setButtonText("Drums");
    drumsButton.setToggleState(false, juce::dontSendNotification);

    addAndMakeVisible(bassButton);
    bassButton.setButtonText("Bass");
    bassButton.setToggleState(false, juce::dontSendNotification);

    addAndMakeVisible(otherButton);
    otherButton.setButtonText("Other");
    otherButton.setToggleState(false, juce::dontSendNotification);

    // Status label
    addAndMakeVisible(statusLabel);
    statusLabel.setText("Ready", juce::dontSendNotification);
}

void DjIaVstEditor::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void DjIaVstEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    // Section configuration serveur
    auto configArea = area.removeFromTop(80);

    // Première ligne : Server URL
    auto configRow = configArea.removeFromTop(30);
    serverUrlLabel.setBounds(configRow.removeFromLeft(80));
    serverUrlInput.setBounds(configRow.reduced(5, 0));

    // Deuxième ligne : API Key
    configRow = configArea.removeFromTop(30);
    apiKeyLabel.setBounds(configRow.removeFromLeft(80));
    apiKeyInput.setBounds(configRow.reduced(5, 0));

    area.removeFromTop(10); // Espacement

    // Section contrôles principaux
    promptInput.setBounds(area.removeFromTop(40));

    area.removeFromTop(10); // Spacing

    // Ligne des contrôles musicaux
    auto controlRow = area.removeFromTop(30);
    styleSelector.setBounds(controlRow.removeFromLeft(controlRow.getWidth() / 3).reduced(2));
    keySelector.setBounds(controlRow.removeFromLeft(controlRow.getWidth() / 2).reduced(2));
    bpmSlider.setBounds(controlRow.reduced(2));

    area.removeFromTop(10);

    // Section stems
    auto stemsRow = area.removeFromTop(30);
    stemsLabel.setBounds(stemsRow.removeFromLeft(80));
    auto stemsArea = stemsRow.reduced(5, 0);
    auto buttonWidth = stemsArea.getWidth() / 3;
    drumsButton.setBounds(stemsArea.removeFromLeft(buttonWidth));
    bassButton.setBounds(stemsArea.removeFromLeft(buttonWidth));
    otherButton.setBounds(stemsArea);

    area.removeFromTop(10);

    // Boutons et status
    auto buttonsRow = area.removeFromTop(40);
    generateButton.setBounds(buttonsRow.removeFromLeft(buttonsRow.getWidth() / 2).reduced(2, 0));
    playButton.setBounds(buttonsRow.reduced(2, 0));

    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(30));
}

void DjIaVstEditor::onGenerateButtonClicked()
{
    statusLabel.setText("Connecting to server...", juce::dontSendNotification);

    // Lancer la génération dans un thread séparé
    juce::Thread::launch([this]()
                         {
        try
        {
            juce::MessageManager::callAsync([this]() {
                statusLabel.setText("Generating loop (this may take a few minutes)...", 
                    juce::dontSendNotification);
            });
            // IMPORTANT: D'abord configurer le serveur et l'API key
            if (serverUrlInput.getText().isEmpty())
            {
                throw std::runtime_error("Server URL is required");
            }
            if (apiKeyInput.getText().isEmpty())
            {
                throw std::runtime_error("API Key is required");
            }

            // Mettre à jour la configuration serveur AVANT de créer la requête
            audioProcessor.setServerUrl(serverUrlInput.getText());
            audioProcessor.setApiKey(apiKeyInput.getText());

            // Attendre un peu que la configuration soit appliquée
            juce::Thread::sleep(100);

            // ENSUITE créer la requête avec le vrai prompt
            DjIaClient::LoopRequest request;
            request.prompt = promptInput.getText();  // Le vrai prompt de l'utilisateur
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
            
            // Mettre à jour l'UI dans le thread principal
            juce::MessageManager::callAsync([this]() {
                statusLabel.setText("Loop generated successfully!", juce::dontSendNotification);
                
                // Réinitialiser le bouton de lecture s'il était actif
                if (playButton.getToggleState()) {
                    playButton.setToggleState(false, juce::sendNotification);
                    playButton.setButtonText("Play Loop");
                }
            });
        }
        catch (const std::exception& e)
        {
            juce::MessageManager::callAsync([this, error = juce::String(e.what())]() {
                statusLabel.setText("Error: " + error, juce::dontSendNotification);
            });
        } });
}