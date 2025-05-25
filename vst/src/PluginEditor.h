#pragma once
#include "PluginProcessor.h"

class DjIaVstEditor : public juce::AudioProcessorEditor
{
public:
    explicit DjIaVstEditor(DjIaVstProcessor &);
    ~DjIaVstEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void updateUIFromProcessor();

private:
    DjIaVstProcessor &audioProcessor;

    void setupUI();
    void onGenerateButtonClicked();
    void loadPromptPresets();
    void onPresetSelected();
    void onSavePreset();
    void updateBpmFromHost();
    void onAutoLoadToggled();
    void onLoadSampleClicked();
    void updateLoadButtonState();
    void updateMidiIndicator(const juce::String &noteInfo);

    // Presets de prompts
    juce::StringArray promptPresets = {
        // Drums & Percussion
        "Dark acidic kick",
        "Crushing industrial drums",
        "Glitchy percussions",
        "Deep punchy 909 kick",
        "Crispy snare with reverb",
        "Analog clap with distortion",
        "Metallic hi-hats sequence",
        "Tribal percussion loop",
        "Broken beat with swing",

        // Bass & Low End
        "Deep rolling bass",
        "Acidic 303 bassline",
        "Wobbly dubstep bass",
        "Vintage Moog bass",
        "Growling reese bass",
        "Plucky upright bass",
        "Sub bass rumble",
        "Funky slap bass",

        // Leads & Melodies
        "Warm analog lead",
        "Melodic arpeggiated sequence",
        "Screaming acid lead",
        "Dreamy pluck melody",
        "Aggressive saw lead",
        "Nostalgic 80s synth",
        "Detuned vintage keys",
        "Crystalline bell sequence",

        // Pads & Atmosphere
        "Ethereal ambient pad",
        "Dark atmospheric pad",
        "Warm string ensemble",
        "Lush choir pad",
        "Metallic drone texture",
        "Cinematic rising pad",
        "Vintage analog strings",
        "Spacey reverb pad",

        // Textures & FX
        "Distorted vocal chops",
        "Granular texture sweep",
        "Vinyl crackle atmosphere",
        "Industrial noise burst",
        "Reversed cymbal swell",
        "Pitched down vocals",
        "Glitchy stutter effect",
        "Analog filter sweep",
        "Tape delay echoes",

        // Experimental
        "Chaotic granular clouds",
        "Algorithmic polyrhythm",
        "Microtonal drone",
        "Field recording texture",
        "Modular patch experiment",

        "Custom..." // Dernier élément pour custom
    };

    // Composants UI
    juce::ComboBox promptPresetSelector;
    juce::TextButton savePresetButton;
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
    juce::ToggleButton hostBpmButton;

    juce::ToggleButton autoLoadButton;
    juce::TextButton loadSampleButton;
    // Status
    juce::Label statusLabel;

    juce::Label midiIndicator;
    juce::String lastMidiNote;

    juce::TextButton testMidiButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstEditor)
};