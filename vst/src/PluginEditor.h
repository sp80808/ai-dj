#pragma once
#include "PluginProcessor.h"

class DjIaVstEditor : public juce::AudioProcessorEditor
{
public:
    explicit DjIaVstEditor(DjIaVstProcessor &);
    ~DjIaVstEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    DjIaVstProcessor &audioProcessor;

    void setupUI();
    void onGenerateButtonClicked();

    // Composants UI
    juce::TextEditor promptInput;
    juce::ComboBox styleSelector;
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::ComboBox keySelector;
    juce::TextButton generateButton;

    // Configuration serveur
    juce::Label serverUrlLabel;
    juce::TextEditor serverUrlInput;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyInput;

    // Sélection des stems
    juce::Label stemsLabel;
    juce::ToggleButton drumsButton;
    juce::ToggleButton bassButton;
    juce::ToggleButton otherButton;

    juce::TextButton playButton;

    // Status
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstEditor)

private:
    DjIaClient apiClient;
    juce::CriticalSection apiLock;

    // Méthode pour générer une loop
    void generateLoop(const juce::String &prompt);
};
