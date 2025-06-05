// OBSIDIAN Neural Sound Engine - Original Author: InnerMost47
// This attribution should remain in derivative works

#include "./PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor& p)
	: AudioProcessorEditor(&p), audioProcessor(p)
{
	setSize(1300, 800);
	logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png,
		BinaryData::logo_pngSize);
	bannerImage = juce::ImageCache::getFromMemory(BinaryData::cyber_banner_png,
		BinaryData::cyber_banner_pngSize);
	if (audioProcessor.isStateReady())
	{
		initUI();
	}
	else
		startTimer(50);

	juce::Timer::callAfterDelay(300, [this]()
		{
			refreshTracks();
			loadPromptPresets();
			for (auto& trackComp : trackComponents)
			{
				if (trackComp->getTrack() && trackComp->getTrack()->showWaveform)
				{
					trackComp->toggleWaveformDisplay();
				}
			}
			if (audioProcessor.getIsGenerating())
			{
				generateButton.setEnabled(false);
				setAllGenerateButtonsEnabled(false);
				statusLabel.setText("Generation in progress...", juce::dontSendNotification);
				juce::String generatingId = audioProcessor.getGeneratingTrackId();
				for (auto& trackComp : trackComponents)
				{
					if (trackComp->getTrackId() == generatingId)
					{
						trackComp->startGeneratingAnimation();
						break;
					}
				}
			} });
}

DjIaVstEditor::~DjIaVstEditor()
{
	audioProcessor.onUIUpdateNeeded = nullptr;
}

void DjIaVstEditor::updateMidiIndicator(const juce::String& noteInfo)
{
	lastMidiNote = noteInfo;

	juce::MessageManager::callAsync([this, noteInfo]()
		{
			if (midiIndicator.isShowing())
			{
				midiIndicator.setText("MIDI: " + noteInfo, juce::dontSendNotification);
				auto greenWithOpacity = juce::Colours::green.withAlpha(0.3f);
				midiIndicator.setColour(juce::Label::backgroundColourId, greenWithOpacity);

				juce::Timer::callAfterDelay(200, [this]()
					{
						if (midiIndicator.isShowing())
						{
							midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
						}
					});
			} });
}

void DjIaVstEditor::updateUIComponents()
{
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->isShowing())
		{
			TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
			if (track && track->isPlaying.load() && !trackComp->isEditingLabel)
			{
				trackComp->updateFromTrackData();
			}
		}
	}

	if (mixerPanel)
	{
		mixerPanel->updateAllMixerComponents();
	}

	if (!lastMidiNote.isEmpty())
	{
		static int midiBlinkCounter = 0;
		if (++midiBlinkCounter > 6)
		{
			midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
			lastMidiNote.clear();
			midiBlinkCounter = 0;
		}
	}

	if (hostBpmButton.getToggleState())
	{
		double currentHostBpm = audioProcessor.getHostBpm();
		if (currentHostBpm > 0.0 && std::abs(currentHostBpm - bpmSlider.getValue()) > 0.1)
		{
			bpmSlider.setValue(currentHostBpm, juce::dontSendNotification);
		}
	}

	if (!autoLoadButton.getToggleState())
	{
		updateLoadButtonState();
	}

	for (auto& trackComp : trackComponents)
	{
		TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
		if (track && track->isPlaying.load() && track->numSamples > 0)
		{
			double startSample = track->loopStart * track->sampleRate;
			double currentTimeInSection = (startSample + track->readPosition.load()) / track->sampleRate;

			trackComp->updatePlaybackPosition(currentTimeInSection);
		}
	}

	static bool wasGenerating = false;
	bool isCurrentlyGenerating = generateButton.isEnabled() == false;
	if (wasGenerating && !isCurrentlyGenerating)
	{
		for (auto& trackComp : trackComponents)
		{
			trackComp->refreshWaveformIfNeeded();
		}
	}
	wasGenerating = isCurrentlyGenerating;
}

void DjIaVstEditor::onGenerationComplete(const juce::String& selectedTrackId, const juce::String& notification)
{
	juce::MessageManager::callAsync([this, selectedTrackId, notification]()
		{
			for (auto& trackComp : trackComponents) {
				if (trackComp->getTrackId() == selectedTrackId) {
					trackComp->stopGeneratingAnimation();
					trackComp->updateFromTrackData();
					trackComp->repaint();
					break;
				}
			}
			statusLabel.setText(notification,
				juce::dontSendNotification);
			generateButton.setEnabled(true);
			setAllGenerateButtonsEnabled(true); });
}

void DjIaVstEditor::refreshTracks()
{
	trackComponents.clear();
	tracksContainer.removeAllChildren();

	refreshTrackComponents();
	updateSelectedTrack();
	repaint();
}

void DjIaVstEditor::initUI()
{
	setupUI();
	serverUrlInput.setText(audioProcessor.getServerUrl(), juce::dontSendNotification);
	apiKeyInput.setText(audioProcessor.getApiKey(), juce::dontSendNotification);

	if (audioProcessor.getApiKey().isEmpty() || audioProcessor.getServerUrl().isEmpty())
	{
		juce::Timer::callAfterDelay(500, [this]()
			{ showFirstTimeSetup(); });
	}
	juce::WeakReference<DjIaVstEditor> weakThis(this);
	audioProcessor.setMidiIndicatorCallback([weakThis](const juce::String& noteInfo)
		{
			if (weakThis != nullptr) {
				weakThis->updateMidiIndicator(noteInfo);
			} });
			refreshTracks();
			audioProcessor.onUIUpdateNeeded = [this]()
				{
					juce::MessageManager::callAsync([this]()
						{ updateUIComponents(); });
				};
}

void DjIaVstEditor::showFirstTimeSetup()
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"OBSIDIAN Configuration",
		"Welcome! Please configure your API settings:",
		juce::MessageBoxIconType::InfoIcon);

	alertWindow->addTextEditor("serverUrl", audioProcessor.getServerUrl(), "Server URL:");
	alertWindow->addTextEditor("apiKey", "", "API Key:");

	if (auto* apiKeyEditor = alertWindow->getTextEditor("apiKey"))
	{
		apiKeyEditor->setPasswordCharacter('*');
	}

	alertWindow->addButton("Save & Continue", 1);
	alertWindow->addButton("Skip for now", 0);

	auto* windowPtr = alertWindow.get();

	alertWindow.release()->enterModalState(true, juce::ModalCallbackFunction::create([this, windowPtr](int result)
		{
			if (result == 1) {
				auto* urlEditor = windowPtr->getTextEditor("serverUrl");
				auto* keyEditor = windowPtr->getTextEditor("apiKey");

				if (urlEditor && keyEditor) {
					audioProcessor.setServerUrl(urlEditor->getText());
					audioProcessor.setApiKey(keyEditor->getText());
					audioProcessor.saveGlobalConfig();
					statusLabel.setText("Configuration saved!", juce::dontSendNotification);
					juce::Timer::callAfterDelay(2000, [this]() {
						statusLabel.setText("Ready", juce::dontSendNotification);
						});
				}
			}
			windowPtr->exitModalState(result);
			delete windowPtr; }));
}

void DjIaVstEditor::showConfigDialog()
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"Update API Configuration",
		"Update your API settings:",
		juce::MessageBoxIconType::QuestionIcon);

	alertWindow->addTextEditor("serverUrl", audioProcessor.getServerUrl(), "Server URL:");
	alertWindow->addTextEditor("apiKey", "", "API Key (leave blank to keep current):");

	if (auto* apiKeyEditor = alertWindow->getTextEditor("apiKey"))
	{
		apiKeyEditor->setPasswordCharacter('*');
	}

	alertWindow->addButton("Update", 1);
	alertWindow->addButton("Cancel", 0);

	auto* windowPtr = alertWindow.get();

	alertWindow.release()->enterModalState(true, juce::ModalCallbackFunction::create([this, windowPtr](int result)
		{
			if (result == 1) {
				auto* urlEditor = windowPtr->getTextEditor("serverUrl");
				auto* keyEditor = windowPtr->getTextEditor("apiKey");

				if (urlEditor && keyEditor) {
					juce::String newKey = keyEditor->getText();
					if (newKey.isEmpty()) {
						newKey = audioProcessor.getApiKey();
					}

					audioProcessor.setServerUrl(urlEditor->getText());
					audioProcessor.setApiKey(newKey);
					audioProcessor.saveGlobalConfig();
					statusLabel.setText("Configuration updated!", juce::dontSendNotification);
					juce::Timer::callAfterDelay(2000, [this]() {
						statusLabel.setText("Ready", juce::dontSendNotification);
						});
				}
			}
			windowPtr->exitModalState(result);
			delete windowPtr; }));
}

void DjIaVstEditor::timerCallback()
{
	if (audioProcessor.isStateReady())
	{
		stopTimer();
		initUI();
	}
	bool anyTrackPlaying = false;

	for (auto& trackComp : trackComponents)
	{
		if (trackComp->isShowing())
		{
			TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
			if (track && track->isPlaying.load())
			{
				trackComp->updateFromTrackData();
				anyTrackPlaying = true;
			}
		}
	}

	if (!anyTrackPlaying)
	{
		static int skipFrames = 0;
		skipFrames++;
		if (skipFrames < 10)
			return;
		skipFrames = 0;
	}

	static double lastHostBpm = 0.0;
	double currentHostBpm = audioProcessor.getHostBpm();

	if (std::abs(currentHostBpm - lastHostBpm) > 0.1)
	{
		lastHostBpm = currentHostBpm;
		for (auto& trackComp : trackComponents)
		{
			TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
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
	getLookAndFeel().setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ff88));
	getLookAndFeel().setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));

	addAndMakeVisible(pluginNameLabel);
	pluginNameLabel.setText("NEURAL SOUND ENGINE", juce::dontSendNotification);
	pluginNameLabel.setFont(juce::Font("Courier New", 22.0f, juce::Font::bold));
	pluginNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00aaff));
	pluginNameLabel.setJustificationType(juce::Justification::left);

	addAndMakeVisible(developerLabel);
	developerLabel.setText("Developed by InnerMost47", juce::dontSendNotification);
	developerLabel.setFont(juce::Font("Courier New", 14.0f, juce::Font::italic));
	developerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
	developerLabel.setJustificationType(juce::Justification::left);

	menuBar = std::make_unique<juce::MenuBarComponent>(this);
	addAndMakeVisible(*menuBar);
	addAndMakeVisible(promptPresetSelector);

	addAndMakeVisible(savePresetButton);
	savePresetButton.setButtonText("Save");

	addAndMakeVisible(promptInput);
	promptInput.setMultiLine(false);
	promptInput.setTextToShowWhenEmpty("Enter custom prompt or select preset...",
		juce::Colours::grey);

	addAndMakeVisible(resetUIButton);
	resetUIButton.setButtonText("Reset UI");
	resetUIButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
	resetUIButton.setTooltip("Reset UI state if stuck in generation mode");

	addAndMakeVisible(bpmSlider);
	bpmSlider.setRange(60.0, 200.0, 1.0);
	bpmSlider.setValue(audioProcessor.getLastBpm(), juce::dontSendNotification);
	bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

	addAndMakeVisible(bpmLabel);
	bpmLabel.setText("BPM", juce::dontSendNotification);
	bpmLabel.attachToComponent(&bpmSlider, true);

	addAndMakeVisible(hostBpmButton);
	hostBpmButton.setButtonText("Sync Host");
	hostBpmButton.setClickingTogglesState(true);
	hostBpmButton.setToggleState(audioProcessor.getHostBpmEnabled(), juce::dontSendNotification);

	bpmSlider.setEnabled(!audioProcessor.getHostBpmEnabled());

	addAndMakeVisible(serverSidePreTreatmentButton);
	serverSidePreTreatmentButton.setButtonText("Server Side Pre Treatment");
	serverSidePreTreatmentButton.setClickingTogglesState(true);
	serverSidePreTreatmentButton.setToggleState(audioProcessor.getServerSidePreTreatment(), juce::dontSendNotification);

	addAndMakeVisible(keySelector);
	keySelector.addItem("C minor", 1);
	keySelector.addItem("C major", 2);
	keySelector.addItem("G minor", 3);
	keySelector.addItem("F major", 4);
	keySelector.addItem("A minor", 5);
	keySelector.addItem("D minor", 6);
	keySelector.setSelectedId(audioProcessor.getLastKeyIndex(), juce::dontSendNotification);

	addAndMakeVisible(durationSlider);
	durationSlider.setRange(4.0, 30.0, 1.0);
	durationSlider.setValue(audioProcessor.getLastDuration(), juce::dontSendNotification);
	durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
	durationSlider.setTextValueSuffix(" s");

	addAndMakeVisible(durationLabel);
	durationLabel.setText("Duration", juce::dontSendNotification);

	addAndMakeVisible(generateButton);
	generateButton.setButtonText("Generate Loop");

	addAndMakeVisible(configButton);
	configButton.setButtonText("API Config");
	configButton.setTooltip("Configure API settings globally");
	configButton.onClick = [this]()
		{ showConfigDialog(); };

	addAndMakeVisible(stemsLabel);
	stemsLabel.setText("Stems:", juce::dontSendNotification);

	addAndMakeVisible(drumsButton);
	drumsButton.setButtonText("Drums");
	drumsButton.setToggleState(audioProcessor.getDrumsEnabled(), juce::dontSendNotification);

	addAndMakeVisible(bassButton);
	bassButton.setButtonText("Bass");
	bassButton.setToggleState(audioProcessor.getBassEnabled(), juce::dontSendNotification);

	addAndMakeVisible(otherButton);
	otherButton.setButtonText("Other");
	otherButton.setToggleState(audioProcessor.getOtherEnabled(), juce::dontSendNotification);

	addAndMakeVisible(vocalsButton);
	vocalsButton.setButtonText("Vocals");
	vocalsButton.setToggleState(audioProcessor.getVocalsEnabled(), juce::dontSendNotification);

	addAndMakeVisible(guitarButton);
	guitarButton.setButtonText("Guitar");
	guitarButton.setToggleState(audioProcessor.getGuitarEnabled(), juce::dontSendNotification);

	addAndMakeVisible(pianoButton);
	pianoButton.setButtonText("Piano");
	pianoButton.setToggleState(audioProcessor.getPianoEnabled(), juce::dontSendNotification);

	addAndMakeVisible(statusLabel);
	statusLabel.setText("Ready", juce::dontSendNotification);
	statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

	addAndMakeVisible(autoLoadButton);
	autoLoadButton.setButtonText("Auto-Load Samples");
	autoLoadButton.setClickingTogglesState(true);
	autoLoadButton.setToggleState(true, juce::dontSendNotification);

	addAndMakeVisible(loadSampleButton);
	loadSampleButton.setButtonText("Load Sample");
	loadSampleButton.setEnabled(false);

	addAndMakeVisible(midiIndicator);
	midiIndicator.setText("MIDI: Waiting...", juce::dontSendNotification);
	midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::black);
	midiIndicator.setColour(juce::Label::textColourId, juce::Colours::green);
	midiIndicator.setJustificationType(juce::Justification::left);
	midiIndicator.setFont(juce::Font(12.0f, juce::Font::bold));

	addAndMakeVisible(tracksLabel);
	tracksLabel.setText("Tracks:", juce::dontSendNotification);
	tracksLabel.setFont(juce::Font(14.0f, juce::Font::bold));

	addAndMakeVisible(addTrackButton);
	addTrackButton.setButtonText("+ Add Track");
	addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);

	addAndMakeVisible(tracksViewport);
	tracksViewport.setViewedComponent(&tracksContainer, false);
	tracksViewport.setScrollBarsShown(true, false);

	addAndMakeVisible(saveSessionButton);
	saveSessionButton.setButtonText("Save Session");

	addAndMakeVisible(loadSessionButton);
	loadSessionButton.setButtonText("Load Session");

	mixerPanel = std::make_unique<MixerPanel>(audioProcessor);
	addAndMakeVisible(*mixerPanel);

	refreshTrackComponents();

	addEventListeners();

	generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa44));
	generateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

	addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0066cc));

	loadSampleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff666666));

	statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff000000));
	statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff88));
}

void DjIaVstEditor::addEventListeners()
{
	addTrackButton.onClick = [this]()
		{ onAddTrack(); };

	saveSessionButton.onClick = [this]()
		{ onSaveSession(); };

	loadSessionButton.onClick = [this]()
		{ onLoadSession(); };

	autoLoadButton.onClick = [this]
		{ onAutoLoadToggled(); };

	loadSampleButton.onClick = [this]
		{ onLoadSampleClicked(); };

	hostBpmButton.onClick = [this]
		{ updateBpmFromHost(); };

	serverSidePreTreatmentButton.onClick = [this]
		{ updateServerSidePreTreatment(); };

	generateButton.onClick = [this]
		{ onGenerateButtonClicked(); };

	savePresetButton.onClick = [this]
		{ onSavePreset(); };

	promptPresetSelector.onChange = [this]
		{ onPresetSelected(); };

	promptPresetSelector.addMouseListener(this, false);

	promptInput.onTextChange = [this]
		{
			audioProcessor.setLastPrompt(promptInput.getText());
		};

	keySelector.onChange = [this]
		{
			audioProcessor.setLastKeyIndex(keySelector.getSelectedId());
		};

	bpmSlider.onValueChange = [this]
		{
			audioProcessor.setLastBpm(bpmSlider.getValue());
		};

	promptPresetSelector.onChange = [this]
		{
			onPresetSelected();
			audioProcessor.setLastPresetIndex(promptPresetSelector.getSelectedId() - 1);
		};

	hostBpmButton.onClick = [this]
		{
			updateBpmFromHost();
			audioProcessor.setHostBpmEnabled(hostBpmButton.getToggleState());
		};

	durationSlider.onValueChange = [this]
		{
			audioProcessor.setLastDuration(durationSlider.getValue());
		};

	resetUIButton.onClick = [this]()
		{
			if (!audioProcessor.getIsGenerating()) {
				audioProcessor.setIsGenerating(false);
				audioProcessor.setGeneratingTrackId("");

				generateButton.setEnabled(true);
				setAllGenerateButtonsEnabled(true);
				toggleWaveFormButtonOnTrack();
				statusLabel.setText("UI Reset - Ready", juce::dontSendNotification);

				for (auto& trackComp : trackComponents)
				{
					trackComp->stopGeneratingAnimation();
				}
				refreshTracks();
			}
			else {
				statusLabel.setText("Please refresh after sample generation", juce::dontSendNotification);
			}

		};
}

void DjIaVstEditor::mouseDown(const juce::MouseEvent& event)
{
	if (event.eventComponent == &promptPresetSelector && event.mods.isPopupMenu())
	{
		juce::String selectedPrompt = promptPresetSelector.getText();
		auto customPrompts = audioProcessor.getCustomPrompts();

		if (customPrompts.contains(selectedPrompt))
		{
			juce::PopupMenu menu;
			menu.addItem(1, "Edit");
			menu.addItem(2, "Delete");

			menu.showMenuAsync(juce::PopupMenu::Options(), [this, selectedPrompt](int result)
				{
					if (result == 1) {
						editCustomPromptDialog(selectedPrompt);
					}
					else if (result == 2) {
						juce::MessageManager::callAsync([this, selectedPrompt]()
							{
								juce::AlertWindow::showAsync(
									juce::MessageBoxOptions()
									.withIconType(juce::MessageBoxIconType::WarningIcon)
									.withTitle("Delete Custom Prompt")
									.withMessage("Are you sure you want to delete this prompt?\n\n'" + selectedPrompt + "'")
									.withButton("Delete")
									.withButton("Cancel"),
									[this, selectedPrompt](int result)
									{
										if (result == 1)
										{
											audioProcessor.removeCustomPrompt(selectedPrompt);
											loadPromptPresets();
										}
									});
							});

					} });
		}
	}
}

void DjIaVstEditor::editCustomPromptDialog(const juce::String& selectedPrompt)
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"Edit Custom Prompt",
		"Edit your prompt:",
		juce::MessageBoxIconType::InfoIcon);

	alertWindow->addTextEditor("promptText", selectedPrompt, "Prompt text:");
	alertWindow->addButton("Save", 1);
	alertWindow->addButton("Cancel", 0);

	auto* windowPtr = alertWindow.get();
	alertWindow.release()->enterModalState(true,
		juce::ModalCallbackFunction::create([this, windowPtr, selectedPrompt](int result)
			{
				if (result == 1) {
					auto* promptEditor = windowPtr->getTextEditor("promptText");
					if (promptEditor) {
						juce::String newPrompt = promptEditor->getText();
						if (!newPrompt.isEmpty()) {
							audioProcessor.editCustomPrompt(selectedPrompt, newPrompt);
							loadPromptPresets();
						}
					}
				}
				windowPtr->exitModalState(result);
				delete windowPtr; }));
}

void DjIaVstEditor::updateUIFromProcessor()
{
	serverUrlInput.setText(audioProcessor.getServerUrl(), juce::dontSendNotification);
	apiKeyInput.setText(audioProcessor.getApiKey(), juce::dontSendNotification);

	promptInput.setText(audioProcessor.getLastPrompt(), juce::dontSendNotification);
	bpmSlider.setValue(audioProcessor.getLastBpm(), juce::dontSendNotification);

	durationSlider.setValue(audioProcessor.getLastDuration(), juce::dontSendNotification);

	juce::String key = audioProcessor.getLastKey();
	for (int i = 1; i <= keySelector.getNumItems(); ++i)
	{
		if (keySelector.getItemText(i - 1) == key)
		{
			keySelector.setSelectedId(i, juce::dontSendNotification);
			break;
		}
	}

	int presetIndex = audioProcessor.getLastPresetIndex();
	if (presetIndex >= 0 && presetIndex < promptPresets.size())
	{
		promptPresetSelector.setSelectedId(presetIndex + 1, juce::dontSendNotification);
	}
	else
	{
		promptPresetSelector.setSelectedId(promptPresets.size(), juce::dontSendNotification);
	}

	hostBpmButton.setToggleState(audioProcessor.getHostBpmEnabled(), juce::dontSendNotification);
	if (audioProcessor.getHostBpmEnabled())
	{
		bpmSlider.setEnabled(false);
	}
	serverSidePreTreatmentButton.setToggleState(audioProcessor.getServerSidePreTreatment(), juce::dontSendNotification);
	refreshTrackComponents();
}

void DjIaVstEditor::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	juce::ColourGradient gradient(
		juce::Colour(0xff1a1a1a), 0, 0,
		juce::Colour(0xff2d2d2d), 0, getHeight(),
		false);
	g.setGradientFill(gradient);
	g.fillAll();

	if (bannerImage.isValid())
	{
		int sourceWidth = bannerImage.getWidth();
		int sourceHeight = (int)((bannerImage.getHeight() - 300) * 0.1f);
		auto sourceArea = juce::Rectangle<int>(0, 0, sourceWidth, sourceHeight);

		juce::Path roundedRect;
		roundedRect.addRoundedRectangle(bannerArea.toFloat(), 6.0f);

		g.saveState();
		g.reduceClipRegion(roundedRect);

		g.drawImage(bannerImage,
			bannerArea.getX(), bannerArea.getY(),
			bannerArea.getWidth(), bannerArea.getHeight(),
			0, 0,
			sourceWidth, sourceHeight,
			false);

		g.restoreState();
	}

	if (logoImage.isValid())
	{
		auto logoArea = juce::Rectangle<int>(0, 40, 100, 60);
		g.drawImage(logoImage, logoArea.toFloat(),
			juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
	}
}

void DjIaVstEditor::layoutPromptSection(juce::Rectangle<int> area, int spacing)
{
	auto row1 = area.removeFromTop(35);
	int saveButtonWidth = 50;
	promptPresetSelector.setBounds(row1.removeFromLeft(area.getWidth() - saveButtonWidth - spacing));
	row1.removeFromLeft(spacing);
	savePresetButton.setBounds(row1.removeFromLeft(saveButtonWidth));

	area.removeFromTop(spacing);

	auto row2 = area.removeFromTop(35);
	promptInput.setBounds(row2.removeFromLeft(area.getWidth()));
}

void DjIaVstEditor::layoutConfigSection(juce::Rectangle<int> area, int reducing)
{
	auto controlRow = area.removeFromTop(35);
	auto controlWidth = controlRow.getWidth() / 5;

	keySelector.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));
	durationSlider.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));
	serverSidePreTreatmentButton.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));
	hostBpmButton.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));
	bpmSlider.setBounds(controlRow.reduced(reducing));

	auto stemsRow = area.removeFromTop(30);
	auto stemsSection = stemsRow.removeFromLeft(600);
	stemsLabel.setBounds(stemsSection.removeFromLeft(60));
	auto stemsArea = stemsSection.reduced(reducing);
	auto stemWidth = stemsArea.getWidth() / 6;
	drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	vocalsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	guitarButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	pianoButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	otherButton.setBounds(stemsArea.reduced(reducing));
}

void DjIaVstEditor::resized()
{
	static bool resizing = false;

	const int spacing = 5;
	const int margin = 10;
	const int padding = 10;
	const int reducing = 2;

	if (resizing)
		return;

	resizing = true;

	auto bottomArea = getLocalBounds().removeFromBottom(45);
	auto area = getLocalBounds();

	if (menuBar)
	{
		menuBar->setBounds(area.removeFromTop(24));
	}

	area = area.reduced(padding);
	auto configArea = area.removeFromTop(70);

	this->bannerArea = configArea;

	auto logoSpace = configArea.removeFromLeft(80);
	auto nameArea = configArea.removeFromLeft(300);
	auto titleArea = nameArea.removeFromTop(30);
	auto devArea = nameArea.removeFromTop(10);
	pluginNameLabel.setBounds(titleArea);
	developerLabel.setBounds(devArea);

	auto configButtonArea = configArea.removeFromRight(150);
	configButton.setBounds(configButtonArea.reduced(16));

	area = area.reduced(padding);

	auto promptAndConfigArea = area.removeFromTop(80);
	auto leftSection = promptAndConfigArea.removeFromLeft(promptAndConfigArea.getWidth() / 2);
	promptAndConfigArea.removeFromLeft(20);
	auto rightSection = promptAndConfigArea;

	layoutPromptSection(rightSection, spacing);
	layoutConfigSection(leftSection, reducing);

	area.removeFromTop(spacing);

	auto tracksAndMixerArea = area.removeFromTop(area.getHeight() - 70);

	auto tracksMainArea = tracksAndMixerArea.removeFromLeft(tracksAndMixerArea.getWidth() * 0.6f);

	tracksViewport.setBounds(tracksMainArea);

	tracksAndMixerArea.removeFromLeft(5);
	if (mixerPanel)
	{
		mixerPanel->setBounds(tracksAndMixerArea);
		mixerPanel->setVisible(true);
	}

	auto buttonsRow = area.removeFromTop(35);
	auto buttonWidth = buttonsRow.getWidth() / 5;
	auto tracksHeaderArea = area.removeFromTop(30);
	autoLoadButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	addTrackButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	loadSampleButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	resetUIButton.setBounds(buttonsRow.reduced(5));

	bottomArea.removeFromTop(spacing);
	statusLabel.setBounds(bottomArea.removeFromTop(20));
	midiIndicator.setBounds(bottomArea.removeFromTop(20));

	resizing = false;
}

void DjIaVstEditor::setAllGenerateButtonsEnabled(bool enabled)
{
	for (auto& trackComp : trackComponents)
	{
		trackComp->setGenerateButtonEnabled(enabled);
	}
}

void DjIaVstEditor::onGenerateButtonClicked()
{
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

	generateButton.setEnabled(false);
	setAllGenerateButtonsEnabled(false);
	statusLabel.setText("Connecting to server...", juce::dontSendNotification);

	juce::String selectedTrackId = audioProcessor.getSelectedTrackId();
	audioProcessor.setIsGenerating(true);
	audioProcessor.setGeneratingTrackId(selectedTrackId);

	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == selectedTrackId)
		{
			trackComp->startGeneratingAnimation();
			break;
		}
	}

	juce::Thread::launch([this, selectedTrackId]()
		{
			try
			{
				juce::MessageManager::callAsync([this]() {
					statusLabel.setText("Generating loop (this may take a few minutes)...",
						juce::dontSendNotification);
					});

				audioProcessor.setServerUrl(serverUrlInput.getText());
				audioProcessor.setApiKey(apiKeyInput.getText());

				juce::Thread::sleep(100);

				DjIaClient::LoopRequest request;
				request.prompt = promptInput.getText();
				request.bpm = (float)bpmSlider.getValue();
				request.key = keySelector.getText();
				request.measures = 4;
				request.generationDuration = (int)durationSlider.getValue();
				request.serverSidePreTreatment = audioProcessor.getServerSidePreTreatment();

				if (drumsButton.getToggleState())
					request.preferredStems.push_back("drums");
				if (bassButton.getToggleState())
					request.preferredStems.push_back("bass");
				if (otherButton.getToggleState())
					request.preferredStems.push_back("other");
				if (vocalsButton.getToggleState())
					request.preferredStems.push_back("vocals");
				if (guitarButton.getToggleState())
					request.preferredStems.push_back("guitar");
				if (pianoButton.getToggleState())
					request.preferredStems.push_back("piano");

				juce::String targetTrackId = audioProcessor.getSelectedTrackId();
				audioProcessor.generateLoop(request, targetTrackId);

				juce::MessageManager::callAsync([this, selectedTrackId]() {
					for (auto& trackComp : trackComponents) {
						if (trackComp->getTrackId() == selectedTrackId) {
							trackComp->stopGeneratingAnimation();
							trackComp->updateFromTrackData();
							trackComp->repaint();
							break;
						}
					}
					statusLabel.setText("Loop generated successfully! Press Play to listen.",
						juce::dontSendNotification);
					generateButton.setEnabled(true);
					setAllGenerateButtonsEnabled(true);
					audioProcessor.setIsGenerating(false);
					audioProcessor.setGeneratingTrackId("");
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
					setAllGenerateButtonsEnabled(true);
					audioProcessor.setIsGenerating(false);
					audioProcessor.setGeneratingTrackId("");
					});
			} });
}

void DjIaVstEditor::loadPromptPresets()
{
	promptPresetSelector.clear();
	juce::StringArray allPrompts = promptPresets;
	auto customPrompts = audioProcessor.getCustomPrompts();
	for (const auto& customPrompt : customPrompts)
	{
		if (!allPrompts.contains(customPrompt))
		{
			allPrompts.add(customPrompt);
		}
	}
	for (int i = 0; i < allPrompts.size(); ++i)
	{
		promptPresetSelector.addItem(allPrompts[i], i + 1);
	}
	int lastPresetIndex = audioProcessor.getLastPresetIndex();
	if (lastPresetIndex >= 1 && lastPresetIndex <= allPrompts.size())
	{
		promptPresetSelector.setSelectedId(lastPresetIndex + 1, juce::dontSendNotification);
	}
	else
	{
		promptPresetSelector.setSelectedId(1, juce::dontSendNotification);
	}
	juce::String selectedPresetText = promptPresetSelector.getText();
	promptInput.setText(selectedPresetText, juce::dontSendNotification);
}

void DjIaVstEditor::onPresetSelected()
{
	int selectedId = promptPresetSelector.getSelectedId();
	audioProcessor.setLastPresetIndex(selectedId);
	juce::String selectedPrompt = promptPresetSelector.getText();
	if (!selectedPrompt.isEmpty())
	{
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
		audioProcessor.addCustomPrompt(currentPrompt);
		if (!promptPresets.contains(currentPrompt))
		{
			promptPresets.insert(promptPresets.size(), currentPrompt);
		}
		loadPromptPresets();
		int newPresetIndex = promptPresets.indexOf(currentPrompt);
		if (newPresetIndex >= 0)
		{
			promptPresetSelector.setSelectedId(newPresetIndex + 1, juce::dontSendNotification);
		}
		statusLabel.setText("Preset saved: " + currentPrompt, juce::dontSendNotification);
	}
	else
	{
		statusLabel.setText("Enter a prompt first!", juce::dontSendNotification);
	}
}

void DjIaVstEditor::updateServerSidePreTreatment()
{
	bool serverSidePreTreatment = serverSidePreTreatmentButton.getToggleState();
	audioProcessor.setServerSidePreTreatment(serverSidePreTreatment);
}

void DjIaVstEditor::updateBpmFromHost()
{
	if (hostBpmButton.getToggleState())
	{
		double hostBpm = audioProcessor.getHostBpm();
		if (hostBpm > 0.0)
		{
			bpmSlider.setValue(hostBpm, juce::dontSendNotification);
			bpmSlider.setEnabled(false);
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
		bpmSlider.setEnabled(true);
		statusLabel.setText("Using manual BPM", juce::dontSendNotification);
	}
}

void DjIaVstEditor::onAutoLoadToggled()
{
	bool autoLoadOn = autoLoadButton.getToggleState();
	audioProcessor.setAutoLoadEnabled(autoLoadOn);
	loadSampleButton.setEnabled(!autoLoadOn);

	if (autoLoadOn)
	{
		statusLabel.setText("Auto-load enabled - samples load automatically", juce::dontSendNotification);
		loadSampleButton.setButtonText("Load Sample");
	}
	else
	{
		statusLabel.setText("Manual mode - click Load Sample when ready", juce::dontSendNotification);
		updateLoadButtonState();
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
	if (!autoLoadButton.getToggleState())
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
	DBG("🔍 BEFORE sorting - Raw track order:");
	for (const auto& trackId : trackIds)
	{
		TrackData* track = audioProcessor.getTrack(trackId);
		if (track)
		{
			DBG("  Track " << track->trackName << " → slotIndex: " << track->slotIndex << " (ID: " << trackId.substring(0, 8) << ")");
		}
	}

	std::sort(trackIds.begin(), trackIds.end(),
		[this](const juce::String& a, const juce::String& b)
		{
			TrackData* trackA = audioProcessor.getTrack(a);
			TrackData* trackB = audioProcessor.getTrack(b);
			if (!trackA || !trackB)
				return false;
			if (trackA->slotIndex == trackB->slotIndex)
			{
				DBG("⚠️ SLOT CONFLICT: " << trackA->trackName << " and " << trackB->trackName << " both have slotIndex " << trackA->slotIndex);
			}

			return trackA->slotIndex < trackB->slotIndex;
		});
	if (trackComponents.size() == trackIds.size())
	{
		bool allVisible = true;
		for (auto& comp : trackComponents)
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
	}

	setEnabled(false);
	juce::String previousSelectedId = audioProcessor.getSelectedTrackId();

	trackComponents.clear();
	tracksContainer.removeAllChildren();
	int yPos = 5;

	for (const auto& trackId : trackIds)
	{
		TrackData* trackData = audioProcessor.getTrack(trackId);
		if (!trackData)
			continue;

		auto trackComp = std::make_unique<TrackComponent>(trackId, audioProcessor);
		trackComp->setTrackData(trackData);

		trackComp->onSelectTrack = [this](const juce::String& id)
			{
				audioProcessor.selectTrack(id);
				updateSelectedTrack();
			};

		trackComp->onDeleteTrack = [this](const juce::String& id)
			{
				if (audioProcessor.getAllTrackIds().size() > 1)
				{
					audioProcessor.deleteTrack(id);
					juce::Timer::callAfterDelay(10, [this]()
						{ refreshTrackComponents(); });
				}
			};

		trackComp->onTrackRenamed = [this](const juce::String& id, const juce::String& newName)
			{
				if (mixerPanel)
				{
					mixerPanel->updateTrackName(id, newName);
				}
			};

		trackComp->onGenerateForTrack = [this](const juce::String& id)
			{
				audioProcessor.selectTrack(id);
				onGenerateButtonClicked();
			};

		trackComp->onReorderTrack = [this](const juce::String& fromId, const juce::String& toId)
			{
				audioProcessor.reorderTracks(fromId, toId);
				juce::Timer::callAfterDelay(10, [this]()
					{ refreshTrackComponents(); });
			};

		int fullWidth = tracksContainer.getWidth() - 4;
		trackComp->setBounds(2, yPos, fullWidth, 60);

		if (trackId == audioProcessor.getSelectedTrackId())
		{
			trackComp->setSelected(true);
		}
		tracksContainer.addAndMakeVisible(trackComp.get());
		trackComponents.push_back(std::move(trackComp));

		yPos += 85;
	}

	tracksContainer.setSize(tracksViewport.getWidth() - 20, yPos + 5);
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

void DjIaVstEditor::toggleWaveFormButtonOnTrack()
{
	auto trackIds = audioProcessor.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = audioProcessor.getTrack(trackId);
		if (track)
		{
			track->showWaveform = false;
		}
	}
	for (auto& trackComponent : trackComponents)
	{
		trackComponent->showWaveformButton.setToggleState(false, juce::dontSendNotification);
	}
}

void DjIaVstEditor::setStatusWithTimeout(const juce::String& message, int timeoutMs)
{
	statusLabel.setText(message, juce::dontSendNotification);
	juce::Timer::callAfterDelay(timeoutMs, [safeThis = juce::Component::SafePointer<DjIaVstEditor>(this)]()
		{
			if (auto* editor = safeThis.getComponent())
				editor->statusLabel.setText("Ready", juce::dontSendNotification); });
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
		toggleWaveFormButtonOnTrack();
		setStatusWithTimeout("New track created");
	}
	catch (const std::exception& e)
	{
		setStatusWithTimeout("Error: " + juce::String(e.what()));
	}
}

void DjIaVstEditor::updateSelectedTrack()
{
	DBG("🔍 updateSelectedTrack called");

	for (auto& trackComp : trackComponents)
	{
		trackComp->setSelected(false);
	}

	juce::String selectedId = audioProcessor.getSelectedTrackId();
	DBG("🎯 Looking for selectedId: " << selectedId);

	for (int i = 0; i < trackComponents.size(); ++i)
	{
		DBG("📦 TrackComponent[" << i << "] trackId: " << trackComponents[i]->getTrackId() << " (track name: " << (trackComponents[i]->getTrack() ? trackComponents[i]->getTrack()->trackName : "NULL") << ")");
	}

	bool found = false;
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == selectedId)
		{
			trackComp->setSelected(true);
			found = true;
			DBG("✅ Found and selected: " << trackComp->getTrackId());
			break;
		}
	}

	if (!found)
	{
		DBG("❌ Could not find trackComponent with selectedId: " << selectedId);
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

void DjIaVstEditor::saveCurrentSession(const juce::String& sessionName)
{
	try
	{
		juce::File sessionsDir = getSessionsDirectory();
		if (!sessionsDir.exists())
		{
			sessionsDir.createDirectory();
		}

		juce::File sessionFile = sessionsDir.getChildFile(sessionName + ".djiasession");

		juce::MemoryBlock stateData;
		audioProcessor.getStateInformation(stateData);

		juce::FileOutputStream stream(sessionFile);
		if (stream.openedOk())
		{
			stream.write(stateData.getData(), stateData.getSize());
			statusLabel.setText("Session saved: " + sessionName, juce::dontSendNotification);
			loadSessionList();
		}
		else
		{
			statusLabel.setText("Failed to save session file", juce::dontSendNotification);
		}
	}
	catch (const std::exception& e)
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

void DjIaVstEditor::loadSession(const juce::String& sessionName)
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
	catch (const std::exception& e)
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

		for (const auto& file : sessionFiles)
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
		sessionSelector.setSelectedItemIndex(0);
	}
}

juce::File DjIaVstEditor::getSessionsDirectory()
{
	return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
		.getChildFile("OBSIDIAN")
		.getChildFile("Sessions");
}

juce::StringArray DjIaVstEditor::getMenuBarNames()
{
	return { "File", "Tracks", "Help" };
}

juce::PopupMenu DjIaVstEditor::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
	juce::PopupMenu menu;

	if (topLevelMenuIndex == 0)
	{
		menu.addItem(newSession, "New Session", true);
		menu.addSeparator();
		menu.addItem(saveSession, "Save Session", true);
		menu.addItem(saveSessionAs, "Save Session As...", true);
		menu.addItem(loadSessionMenu, "Load Session...", true);
		menu.addSeparator();
		menu.addItem(exportSession, "Export Session", true);
	}
	else if (topLevelMenuIndex == 1)
	{
		menu.addItem(addTrack, "Add New Track", true);
		menu.addSeparator();
		menu.addItem(deleteAllTracks, "Delete All Tracks", audioProcessor.getAllTrackIds().size() > 1);
		menu.addItem(resetTracks, "Reset All Tracks", true);
	}
	else if (topLevelMenuIndex == 2)
	{
		menu.addItem(aboutDjIa, "About OBSIDIAN", true);
		menu.addItem(showHelp, "Show Help", true);
	}

	return menu;
}

void DjIaVstEditor::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
	switch (menuItemID)
	{
	case newSession:
		statusLabel.setText("New session created", juce::dontSendNotification);
		break;

	case saveSession:
		onSaveSession();
		break;

	case saveSessionAs:
		onSaveSession();
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
				{
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
			.withTitle("About OBSIDIAN")
			.withMessage("OBSIDIAN v1.0\n\n"
				"Because writing melodies is hard\n"
				"Let the robots do the work while you take credit\n\n"
				"GitHub: https://github.com/innermost47/ai-dj\n\n"
				"InnerMost47 - Probably overthinking this")
			.withButton("OK"),
			nullptr);
		break;

	case showHelp:
		juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::InfoIcon)
			.withTitle("OBSIDIAN Help")
			.withMessage("Quick Start:\n"
				"1. Configure server URL and API key\n"
				"2. Add tracks and assign MIDI notes\n"
				"3. Generate loops with prompts\n"
				"4. Play with MIDI keyboard!\n\n"
				"Each track can be triggered by its assigned MIDI note.\n\n"
				"MIDI Learn:\n"
				"- Samples: C3-G3 (notes 60-67) reserved for track triggers\n"
				"- Controls: Use notes 0-59 or 68-127 for MIDI mapping\n"
				"- Assignable controls: Play, Mute, Solo, Pitch, Fine, Pan\n"
				"- Right-click any control, Enable MIDI Learn, Press MIDI control\n\n"
				"Play Button Behavior:\n"
				"- First click: Arms sample to start on next MIDI note\n"
				"- Second click: Arms sample to stop on next MIDI note\n"
				"- Stop button: Arms for immediate stop on next MIDI note\n"
				"- No separate Stop in MIDI Learn - Play button handles both!")
			.withButton("OK"),
			nullptr);
		break;
	}
}