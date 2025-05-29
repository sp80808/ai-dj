#include "./PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor& p)
	: AudioProcessorEditor(&p), audioProcessor(p)
{
	setSize(1300, 800);
	logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png,
		BinaryData::logo_pngSize);
	setupUI();
	juce::WeakReference<DjIaVstEditor> weakThis(this);
	audioProcessor.setMidiIndicatorCallback([weakThis](const juce::String& noteInfo)
		{
			if (weakThis != nullptr) {
				weakThis->updateMidiIndicator(noteInfo);
			} });
			refreshTracks();
			audioProcessor.onUIUpdateNeeded = [this]()
				{
					updateUIComponents();
				};
			juce::Timer::callAfterDelay(300, [this]()
				{ refreshTracks(); });
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
				midiIndicator.setColour(juce::Label::backgroundColourId, juce::Colours::green);

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
			if (track && track->isPlaying.load())
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

	menuBar = std::make_unique<juce::MenuBarComponent>(this);
	addAndMakeVisible(*menuBar);
	addAndMakeVisible(promptPresetSelector);
	loadPromptPresets();

	addAndMakeVisible(savePresetButton);
	savePresetButton.setButtonText("Save");

	addAndMakeVisible(promptInput);
	promptInput.setMultiLine(false);
	promptInput.setTextToShowWhenEmpty("Enter custom prompt or select preset...",
		juce::Colours::grey);

	addAndMakeVisible(debugRefreshButton);
	debugRefreshButton.setButtonText("Refresh");
	debugRefreshButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);

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

	addAndMakeVisible(serverSidePreTreatmentButton);
	serverSidePreTreatmentButton.setButtonText("Server Side Pre Treatment");
	serverSidePreTreatmentButton.setClickingTogglesState(true);
	serverSidePreTreatmentButton.setToggleState(true, juce::dontSendNotification);

	addAndMakeVisible(keySelector);
	keySelector.addItem("C minor", 1);
	keySelector.addItem("C major", 2);
	keySelector.addItem("G minor", 3);
	keySelector.addItem("F major", 4);
	keySelector.addItem("A minor", 5);
	keySelector.addItem("D minor", 6);
	keySelector.setSelectedId(1, juce::dontSendNotification);

	addAndMakeVisible(durationSlider);
	durationSlider.setRange(4.0, 30.0, 1.0);
	durationSlider.setValue(6.0, juce::dontSendNotification);
	durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
	durationSlider.setTextValueSuffix(" s");

	addAndMakeVisible(durationLabel);
	durationLabel.setText("Duration", juce::dontSendNotification);

	addAndMakeVisible(generateButton);
	generateButton.setButtonText("Generate Loop");

	addAndMakeVisible(serverUrlLabel);
	serverUrlLabel.setText("Server URL:", juce::dontSendNotification);

	addAndMakeVisible(serverUrlInput);
	serverUrlInput.setText(audioProcessor.getServerUrl());

	addAndMakeVisible(apiKeyLabel);
	apiKeyLabel.setText("API Key:", juce::dontSendNotification);

	addAndMakeVisible(apiKeyInput);
	apiKeyInput.setText(audioProcessor.getApiKey());
	apiKeyInput.setPasswordCharacter('*');

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

	addAndMakeVisible(vocalsButton);
	vocalsButton.setButtonText("Vocals");
	vocalsButton.setClickingTogglesState(true);

	addAndMakeVisible(guitarButton);
	guitarButton.setButtonText("Guitar");
	guitarButton.setClickingTogglesState(true);

	addAndMakeVisible(pianoButton);
	pianoButton.setButtonText("Piano");
	pianoButton.setClickingTogglesState(true);

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

	apiKeyInput.onReturnKey = [this]
		{
			audioProcessor.setApiKey(apiKeyInput.getText());
			statusLabel.setText("API Key updated", juce::dontSendNotification);
		};

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

	serverUrlInput.onReturnKey = [this]
		{
			audioProcessor.setServerUrl(serverUrlInput.getText());
			statusLabel.setText("Server URL updated", juce::dontSendNotification);
		};

	savePresetButton.onClick = [this]
		{ onSavePreset(); };

	debugRefreshButton.onClick = [this]()
		{
			refreshTracks();
		};

	promptPresetSelector.onChange = [this]
		{ onPresetSelected(); };

	promptInput.onTextChange = [this]
		{
			audioProcessor.setLastPrompt(promptInput.getText());
		};


	keySelector.onChange = [this]
		{
			audioProcessor.setLastKey(keySelector.getText());
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

	if (logoImage.isValid())
	{
		auto logoArea = juce::Rectangle<int>(getWidth() - 110, 35, 100, 30);
		g.drawImage(logoImage, logoArea.toFloat(),
			juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
	}
}

void DjIaVstEditor::resized()
{
	static bool resizing = false;
	if (resizing)
		return;
	resizing = true;
	auto bottomArea = getLocalBounds().removeFromBottom(70);
	auto area = getLocalBounds();

	if (menuBar)
	{
		menuBar->setBounds(area.removeFromTop(24));
	}

	area = area.reduced(10);

	auto configArea = area.removeFromTop(40);

	auto logoSpace = configArea.removeFromRight(120);
	auto configRow = configArea.removeFromTop(20);

	auto serverSection = configRow.removeFromLeft(configRow.getWidth() * 0.45f);
	serverUrlLabel.setBounds(serverSection.removeFromLeft(50));
	serverUrlInput.setBounds(serverSection.reduced(1));

	configRow.removeFromLeft(10);

	apiKeyLabel.setBounds(configRow.removeFromLeft(50));
	apiKeyInput.setBounds(configRow.reduced(1));

	area.removeFromTop(5);

	auto presetRow = area.removeFromTop(35);
	promptPresetSelector.setBounds(presetRow.removeFromLeft(presetRow.getWidth() - 80));
	presetRow.removeFromLeft(5);
	savePresetButton.setBounds(presetRow);

	area.removeFromTop(5);

	promptInput.setBounds(area.removeFromTop(35));

	area.removeFromTop(5);

	auto controlRow = area.removeFromTop(35);
	auto controlWidth = controlRow.getWidth() / 5;

	keySelector.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
	durationSlider.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
	serverSidePreTreatmentButton.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
	hostBpmButton.setBounds(controlRow.removeFromLeft(controlWidth).reduced(2));
	bpmSlider.setBounds(controlRow.reduced(2));

	area.removeFromTop(5);

	auto stemsRow = area.removeFromTop(30);
	auto stemsSection = stemsRow.removeFromLeft(600);
	stemsLabel.setBounds(stemsSection.removeFromLeft(60));
	auto stemsArea = stemsSection.reduced(2);
	auto stemWidth = stemsArea.getWidth() / 6;
	drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
	bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
	vocalsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
	guitarButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
	pianoButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(1));
	otherButton.setBounds(stemsArea.reduced(1));

	area.removeFromTop(5);

	auto tracksHeaderArea = area.removeFromTop(30);
	addTrackButton.setBounds(tracksHeaderArea.removeFromRight(100));

	area.removeFromTop(5);

	auto tracksAndMixerArea = area.removeFromTop(area.getHeight() - 100);

	auto tracksMainArea = tracksAndMixerArea.removeFromLeft(tracksAndMixerArea.getWidth() * 0.65f);

	tracksViewport.setBounds(tracksMainArea);

	tracksAndMixerArea.removeFromLeft(5);
	if (mixerPanel)
	{
		mixerPanel->setBounds(tracksAndMixerArea);
		mixerPanel->setVisible(true);
	}

	auto buttonsRow = area.removeFromTop(35);
	auto buttonWidth = buttonsRow.getWidth() / 2;
	generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	loadSampleButton.setBounds(buttonsRow.reduced(5));

	autoLoadButton.setBounds(bottomArea.removeFromTop(20));
	bottomArea.removeFromTop(10);
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
			allPrompts.insert(-1, customPrompt);
		}
	}
	for (int i = 0; i < allPrompts.size(); ++i)
	{
		promptPresetSelector.addItem(allPrompts[i], i + 1);
	}
	promptPresets = allPrompts;
}

void DjIaVstEditor::onPresetSelected()
{
	int selectedId = promptPresetSelector.getSelectedId();
	if (selectedId > 0 && selectedId <= promptPresets.size())
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
		trackComp->setBounds(2, yPos, fullWidth, 80);

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
	catch (const std::exception& e)
	{
		statusLabel.setText("Error: " + juce::String(e.what()), juce::dontSendNotification);
	}
}

void DjIaVstEditor::onDeleteTrack(const juce::String& trackId)
{
	if (audioProcessor.getAllTrackIds().size() > 1)
	{
		audioProcessor.deleteTrack(trackId);
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
	for (auto& trackComp : trackComponents)
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
		.getChildFile("DJ-IA VST")
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