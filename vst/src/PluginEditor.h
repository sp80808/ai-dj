#pragma once
#include "PluginProcessor.h"
#include "TrackComponent.h"
#include "MixerPanel.h"

class DjIaVstEditor : public juce::AudioProcessorEditor, public juce::MenuBarModel, public juce::Timer
{
public:
	explicit DjIaVstEditor(DjIaVstProcessor&);
	~DjIaVstEditor() override;

	void paint(juce::Graphics&) override;
	void resized() override;
	void timerCallback() override;
	void refreshTrackComponents();
	void updateUIFromProcessor();
	void refreshTracks();

	juce::Label statusLabel;

	juce::StringArray getMenuBarNames() override;
	juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
	void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

	std::vector<std::unique_ptr<TrackComponent>>& getTrackComponents()
	{
		return trackComponents;
	}

private:
	DjIaVstProcessor& audioProcessor;
	juce::Image logoImage;
	void setupUI();
	void addEventListeners();
	void onGenerateButtonClicked();
	void loadPromptPresets();
	void onPresetSelected();
	void onSavePreset();
	void updateBpmFromHost();
	void onAutoLoadToggled();
	void onLoadSampleClicked();
	void updateLoadButtonState();
	void updateMidiIndicator(const juce::String& noteInfo);
	void onAddTrack();
	void updateSelectedTrack();
	void onSaveSession();
	void onLoadSession();
	void loadSessionList();
	void saveCurrentSession(const juce::String& sessionName);
	void loadSession(const juce::String& sessionName);
	void toggleMixer();
	void onDeleteTrack(const juce::String& trackId);
	void updateUIComponents();
	void setAllGenerateButtonsEnabled(bool enabled);

	juce::File getSessionsDirectory();
	std::unique_ptr<MixerPanel> mixerPanel;
	juce::TextButton showMixerButton;
	bool mixerVisible = false;
	// Presets de prompts
	juce::StringArray promptPresets = {
		"Techno kick rhythm",
		"Hardcore kick pattern",
		"Drum and bass rhythm",
		"Dub kick rhythm",
		"Acidic 303 bassline",
		"Deep rolling bass",
		"Ambient flute psychedelic",
		"Dark atmospheric pad",
		"Industrial noise texture",
		"Glitchy percussion loop",
		"Vintage analog lead",
		"Distorted noise chops" };

	// Composants UI
	juce::ComboBox promptPresetSelector;
	juce::TextButton savePresetButton;
	juce::TextEditor promptInput;
	juce::ComboBox styleSelector;
	juce::Slider bpmSlider;
	juce::Label bpmLabel;
	juce::ComboBox keySelector;
	juce::TextButton generateButton;
	juce::TextButton debugRefreshButton;
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
	juce::ToggleButton vocalsButton;
	juce::ToggleButton guitarButton;
	juce::ToggleButton pianoButton;

	juce::TextButton playButton;
	juce::ToggleButton hostBpmButton;

	juce::Slider durationSlider;
	juce::Label durationLabel;

	juce::ToggleButton autoLoadButton;
	juce::TextButton loadSampleButton;
	// Status
	juce::Label midiIndicator;
	juce::String lastMidiNote;

	juce::TextButton testMidiButton;

	juce::Viewport tracksViewport;
	juce::Component tracksContainer;
	std::vector<std::unique_ptr<TrackComponent>> trackComponents;
	juce::TextButton addTrackButton;
	juce::Label tracksLabel;

	// Session management
	juce::TextButton saveSessionButton;
	juce::TextButton loadSessionButton;
	juce::ComboBox sessionSelector;

	std::unique_ptr<juce::MenuBarComponent> menuBar;

	enum MenuIDs
	{
		newSession = 1,
		saveSession,
		saveSessionAs,
		loadSessionMenu,
		exportSession,

		aboutDjIa = 100,
		showHelp,

		addTrack = 200,
		deleteAllTracks,
		resetTracks
	};

	JUCE_DECLARE_WEAK_REFERENCEABLE(DjIaVstEditor)
};