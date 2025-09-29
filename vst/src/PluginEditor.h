#pragma once
#include "PluginProcessor.h"
#include "TrackComponent.h"
#include "MixerPanel.h"
#include "MidiLearnableComponents.h"
#include "SampleBankPanel.h"
#include "CustomLookAndFeel.h"

class SequencerComponent;

class DjIaVstEditor : public juce::AudioProcessorEditor, public juce::MenuBarModel, public juce::Timer, public DjIaVstProcessor::GenerationListener, public juce::DragAndDropContainer
{
public:
	explicit DjIaVstEditor(DjIaVstProcessor&);
	~DjIaVstEditor() override;

	void paint(juce::Graphics&) override;
	void layoutPromptSection(juce::Rectangle<int> area, int spacing);
	void layoutConfigSection(juce::Rectangle<int> area, int reducing);
	void resized() override;
	void timerCallback() override;
	void refreshTrackComponents();
	void updateUIFromProcessor();
	void refreshTracks();
	void onGenerationComplete(const juce::String& trackId, const juce::String& message) override;
	void refreshMixerChannels();
	void initUI();

	juce::Label statusLabel;

	juce::StringArray getMenuBarNames() override;
	juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
	void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

	std::vector<std::unique_ptr<TrackComponent>>& getTrackComponents()
	{
		return trackComponents;
	}
	MixerPanel* getMixerPanel() { return mixerPanel.get(); }
	void toggleWaveFormButtonOnTrack();
	void setStatusWithTimeout(const juce::String& message, int timeoutMs = 2000);
	void* getSequencerForTrack(const juce::String& trackId);
	void stopGenerationUI(const juce::String& trackId, bool success = true, const juce::String& errorMessage = "");
	void startGenerationUI(const juce::String& trackId);
	juce::StringArray getBuiltInPrompts() const { return promptPresets; }
	void restoreUICallbacks();
	void updateSelectedTrack();
	void onGenerateButtonClicked();
	void toggleSampleBank();

private:
	DjIaVstProcessor& audioProcessor;
	CustomLookAndFeel customLookAndFeel;
	juce::Image logoImage;
	juce::Image bannerImage;
	juce::Rectangle<int> bannerArea;
	std::unique_ptr<juce::TooltipWindow> tooltipWindow;
	std::unique_ptr<SampleBankPanel> sampleBankPanel;
	juce::TextButton showSampleBankButton;
	bool sampleBankVisible = false;
	enum KeyboardLayout
	{
		QWERTY,
		AZERTY,
		QWERTZ
	};
	KeyboardLayout detectKeyboardLayout();
	bool keyMatches(const juce::KeyPress& pressed, const juce::KeyPress& expected);
	bool keyPressed(const juce::KeyPress& key) override;
	void setupUI();
	void addEventListeners();
	void loadPromptPresets();
	void onPresetSelected();
	void onSavePreset();
	void onAutoLoadToggled();
	void onLoadSampleClicked();
	void updateLoadButtonState();
	void updateMidiIndicator(const juce::String& noteInfo);
	void onAddTrack();
	void onSaveSession();
	void onLoadSession();
	void loadSessionList();
	void saveCurrentSession(const juce::String& sessionName);
	void loadSession(const juce::String& sessionName);
	void updateUIComponents();
	void setAllGenerateButtonsEnabled(bool enabled);
	void showFirstTimeSetup();
	void showConfigDialog();
	void mouseDown(const juce::MouseEvent& event) override;
	void editCustomPromptDialog(const juce::String& selectedPrompt);
	void toggleSEQButtonOnTrack();
	void startGenerationButtonAnimation();
	void stopGenerationButtonAnimation();
	void refreshUIForMode();
	void checkLocalModelsAndNotify();
	void notifyTracksPromptUpdate();
	void generateFromTrackComponent(const juce::String& trackId);

	juce::StringArray getAllPrompts() const;

	juce::File getSessionsDirectory();
	std::unique_ptr<MixerPanel> mixerPanel;
	juce::TextButton showMixerButton;
	bool mixerVisible = false;
	std::atomic<bool> isGenerating{ false };
	std::atomic<bool> wasGenerating{ false };
	std::atomic<bool> isInitialized{ false };

	bool isButtonBlinking = false;
	juce::String generatingTrackId;
	juce::String originalButtonText;
	int blinkCounter = 0;

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

	juce::Label pluginNameLabel;
	juce::Label developerLabel;
	juce::Label stabilityLabel;
	juce::Typeface::Ptr customFont;
	MidiLearnableComboBox promptPresetSelector;
	juce::TextButton savePresetButton;
	juce::TextEditor promptInput;
	juce::ComboBox styleSelector;
	juce::Label bpmLabel;
	juce::ComboBox keySelector;
	MidiLearnableButton generateButton;
	juce::TextButton configButton;
	juce::TextButton sponsorButton;
	juce::TextButton resetUIButton;
	juce::Label serverUrlLabel;
	juce::TextEditor serverUrlInput;
	juce::Label apiKeyLabel;
	juce::TextEditor apiKeyInput;
	juce::Label stemsLabel;
	juce::ToggleButton drumsButton;
	juce::ToggleButton bassButton;
	juce::ToggleButton otherButton;
	juce::ToggleButton vocalsButton;
	juce::ToggleButton guitarButton;
	juce::ToggleButton pianoButton;
	juce::TextButton playButton;
	juce::Slider durationSlider;
	juce::Label durationLabel;
	juce::ToggleButton autoLoadButton;
	juce::TextButton loadSampleButton;
	juce::Label midiIndicator;
	juce::String lastMidiNote;
	juce::TextButton testMidiButton;
	juce::Viewport tracksViewport;
	juce::Component tracksContainer;
	std::vector<std::unique_ptr<TrackComponent>> trackComponents;
	juce::TextButton addTrackButton;
	juce::Label tracksLabel;
	juce::TextButton saveSessionButton;
	juce::TextButton loadSessionButton;
	juce::ComboBox sessionSelector;
	juce::ToggleButton bypassSequencerButton;
	std::unique_ptr<juce::MenuBarComponent> menuBar;

	MidiLearnableButton nextTrackButton;
	MidiLearnableButton prevTrackButton;

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