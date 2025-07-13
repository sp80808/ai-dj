/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#include "SampleBankPanel.h"
#include "PluginProcessor.h"


SampleBankItem::SampleBankItem(SampleBankEntry* entry, DjIaVstProcessor& processor)
	: sampleEntry(entry), audioProcessor(processor), validityFlag(std::make_shared<std::atomic<bool>>(true))
{
	addAndMakeVisible(nameLabel);
	addAndMakeVisible(durationLabel);
	addAndMakeVisible(bpmLabel);
	addAndMakeVisible(usageLabel);
	addAndMakeVisible(playButton);
	addAndMakeVisible(deleteButton);

	nameLabel.setColour(juce::Label::textColourId, ColourPalette::textPrimary);
	nameLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

	durationLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
	durationLabel.setFont(juce::FontOptions(12.0f));

	bpmLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
	bpmLabel.setFont(juce::FontOptions(12.0f));

	usageLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
	usageLabel.setFont(juce::FontOptions(12.0f));

	playButton.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6"));
	playButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonSuccess);
	playButton.setTooltip("Preview sample");
	playButton.onClick = [this]()
		{
			if (isPlaying)
			{
				if (onStopRequested)
					onStopRequested();
			}
			else
			{
				if (onPreviewRequested)
					onPreviewRequested(sampleEntry);
			}
		};

	updatePlayButton();

	deleteButton.setButtonText(juce::String::fromUTF8("\xE2\x9C\x95"));
	deleteButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonDanger);
	deleteButton.setTooltip("Delete sample");
	deleteButton.onClick = [this]()
		{
			if (onDeleteRequested)
				onDeleteRequested(sampleEntry->id);
		};
	updateLabels();
	setSize(400, 80);
}

SampleBankItem::~SampleBankItem()
{
	isDestroyed.store(true);
	validityFlag->store(false);
	stopTimer();
	sampleEntry = nullptr;
}

void SampleBankItem::loadAudioDataIfNeeded()
{
	if (audioBuffer.getNumSamples() == 0)
	{
		loadAudioData();
		if (!waveformBounds.isEmpty())
		{
			generateThumbnail();
			repaint();
		}
	}
}

void SampleBankItem::setIsPlaying(bool playing)
{
	isPlaying = playing;
	updatePlayButton();

	if (isPlaying)
	{
		playbackPosition = 0.0f;
		lastTimerCall = juce::Time::getMillisecondCounterHiRes() / 1000.0;
		startTimer(30);
	}
	else
	{
		stopTimer();
		playbackPosition = 0.0f;
	}

	repaint();
}

void SampleBankItem::timerCallback()
{
	if (!isPlaying || !sampleEntry)
	{
		stopTimer();
		return;
	}
	double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	double deltaTime = currentTime - lastTimerCall;
	lastTimerCall = currentTime;

	playbackPosition += static_cast<float>(deltaTime);

	if (playbackPosition >= sampleEntry->duration)
	{
		playbackPosition = sampleEntry->duration;
		setIsPlaying(false);
		if (onStopRequested)
			onStopRequested();
	}

	repaint();
}

void SampleBankItem::updatePlayButton()
{
	if (isPlaying)
	{
		playButton.setButtonText(juce::String::fromUTF8("\xE2\x96\xA0"));
		playButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonDanger);
		playButton.setTooltip("Stop preview");
	}
	else
	{
		playButton.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6"));
		playButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonSuccess);
		playButton.setTooltip("Preview sample");
	}
}

void SampleBankItem::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	juce::Colour bgColour;

	if (isDragging)
	{
		bgColour = ColourPalette::buttonWarning.withAlpha(0.5f);
	}
	else if (isSelected)
	{
		bgColour = ColourPalette::trackSelected.withAlpha(0.2f);
	}
	else
	{
		bgColour = ColourPalette::backgroundDark.withAlpha(0.8f);
	}

	g.setColour(bgColour);
	g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

	g.setColour(isDragging ? ColourPalette::buttonWarning : ColourPalette::backgroundLight);
	g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f,
		isDragging ? 2.0f : 1.0f);
	drawMiniWaveform(g);
}

void SampleBankItem::setPlaybackPosition(float positionInSeconds)
{
	playbackPosition = positionInSeconds;
	repaint();
}

void SampleBankItem::resized()
{
	auto area = getLocalBounds().reduced(8);
	auto buttonArea = area.removeFromRight(65);

	int buttonSize = 28;
	int buttonY = 8;

	auto playButtonBounds = juce::Rectangle<int>(buttonArea.getX(), buttonY, buttonSize, buttonSize);
	auto deleteButtonBounds = juce::Rectangle<int>(buttonArea.getX() + buttonSize + 5, buttonY, buttonSize, buttonSize);

	playButton.setBounds(playButtonBounds);
	deleteButton.setBounds(deleteButtonBounds);

	area.removeFromRight(10);

	auto topRow = area.removeFromTop(16);
	nameLabel.setBounds(topRow.removeFromLeft(200));

	auto bottomRow = area.removeFromTop(16);
	durationLabel.setBounds(bottomRow.removeFromLeft(60));
	bottomRow.removeFromLeft(10);
	bpmLabel.setBounds(bottomRow.removeFromLeft(60));
	bottomRow.removeFromLeft(10);
	usageLabel.setBounds(bottomRow);

	area.removeFromTop(4);
	waveformBounds = area;

	generateThumbnail();
	repaint();

}

void SampleBankItem::loadAudioData()
{
	if (!sampleEntry) return;

	juce::File audioFile(sampleEntry->filePath);
	if (!audioFile.exists()) return;

	double currentSampleRate = audioProcessor.getSampleRate();
	auto validity = validityFlag;

	juce::Thread::launch([this, audioFile, currentSampleRate, validity]()
		{
			if (!validity->load()) return;

			juce::AudioFormatManager formatManager;
			formatManager.registerBasicFormats();

			auto reader = std::unique_ptr<juce::AudioFormatReader>(
				formatManager.createReaderFor(audioFile));

			if (reader && validity->load())
			{
				const int downsampleRatio = std::max(1, (int)(reader->lengthInSamples / 4096));
				const int numSamples = (int)(reader->lengthInSamples / downsampleRatio);

				auto tempBuffer = std::make_shared<juce::AudioBuffer<float>>(reader->numChannels, numSamples);

				for (int i = 0; i < numSamples; ++i)
				{
					if (!validity->load()) return;
					reader->read(tempBuffer.get(), i, 1, i * downsampleRatio, true, true);
				}

				if (validity->load())
				{
					juce::MessageManager::callAsync([this, tempBuffer, currentSampleRate, validity]()
						{
							if (validity->load() && !isDestroyed.load() && sampleEntry)
							{
								audioBuffer = *tempBuffer;
								sampleRate = currentSampleRate;

								if (!waveformBounds.isEmpty())
								{
									generateThumbnail();
									repaint();
								}
							}
						});
				}
			}
		});
}

void SampleBankItem::drawMiniWaveform(juce::Graphics& g)
{
	if (thumbnail.empty() || waveformBounds.isEmpty()) return;

	g.saveState();

	juce::Path clipPath;
	clipPath.addRoundedRectangle(waveformBounds.toFloat(), 4.0f);
	g.reduceClipRegion(clipPath);

	g.setColour(juce::Colours::black);
	g.fillRoundedRectangle(waveformBounds.toFloat(), 4.0f);

	g.setColour(juce::Colours::lightblue);

	juce::Path waveformPath;
	juce::Path bottomPath;
	bool topPathStarted = false;
	bool bottomPathStarted = false;

	float centerY = static_cast<float>(waveformBounds.getCentreY());
	float pixelsPerPoint = static_cast<float>(waveformBounds.getWidth()) / static_cast<float>(thumbnail.size());

	for (size_t i = 0; i < thumbnail.size(); ++i)
	{
		float x = waveformBounds.getX() + (i * pixelsPerPoint);
		float amplitude = thumbnail[i];
		float waveHeight = amplitude * (waveformBounds.getHeight() * 0.4f);

		float topY = centerY - waveHeight;
		float bottomY = centerY + waveHeight;

		if (!topPathStarted)
		{
			waveformPath.startNewSubPath(x, centerY);
			topPathStarted = true;
		}
		waveformPath.lineTo(x, topY);

		if (!bottomPathStarted)
		{
			bottomPath.startNewSubPath(x, centerY);
			bottomPathStarted = true;
		}
		bottomPath.lineTo(x, bottomY);
	}

	g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
	g.strokePath(bottomPath, juce::PathStrokeType(1.0f));

	g.setColour(juce::Colours::lightblue.withAlpha(0.3f));
	g.drawLine(static_cast<float>(waveformBounds.getX()), centerY,
		static_cast<float>(waveformBounds.getRight()), centerY, 0.5f);

	if (isPlaying && sampleEntry)
	{
		float duration = sampleEntry->duration;
		if (duration > 0.0f && playbackPosition >= 0.0f && playbackPosition <= duration)
		{
			float progress = playbackPosition / duration;
			float headX = waveformBounds.getX() + (progress * waveformBounds.getWidth());

			g.setColour(juce::Colours::red);
			g.drawLine(headX, static_cast<float>(waveformBounds.getY()),
				headX, static_cast<float>(waveformBounds.getBottom()), 2.0f);

			juce::Path triangle;
			triangle.addTriangle(headX - 3, static_cast<float>(waveformBounds.getY()),
				headX + 3, static_cast<float>(waveformBounds.getY()),
				headX, static_cast<float>(waveformBounds.getY()) + 6);
			g.setColour(juce::Colours::yellow);
			g.fillPath(triangle);
		}
	}

	g.restoreState();

	g.setColour(ColourPalette::backgroundLight.withAlpha(0.5f));
	g.drawRoundedRectangle(waveformBounds.toFloat(), 4.0f, 1.0f);
}

void SampleBankItem::generateThumbnail()
{
	thumbnail.clear();

	if (audioBuffer.getNumSamples() == 0) return;

	int targetPoints = waveformBounds.getWidth();
	if (targetPoints <= 0) targetPoints = 100;

	int samplesPerPoint = juce::jmax(1, audioBuffer.getNumSamples() / targetPoints);

	for (int point = 0; point < targetPoints; ++point)
	{
		int sampleStart = point * samplesPerPoint;
		int sampleEnd = std::min(sampleStart + samplesPerPoint, audioBuffer.getNumSamples());

		if (sampleStart >= audioBuffer.getNumSamples()) break;

		float rmsSum = 0.0f;
		float peak = 0.0f;
		int count = 0;

		for (int sample = sampleStart; sample < sampleEnd; ++sample)
		{
			for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
			{
				float val = audioBuffer.getSample(ch, sample);
				rmsSum += val * val;
				peak = std::max(peak, std::abs(val));
				count++;
			}
		}

		float rms = count > 0 ? std::sqrt(rmsSum / count) : 0.0f;
		float finalValue = (rms * 0.7f) + (peak * 0.3f);

		thumbnail.push_back(finalValue);
	}
}

void SampleBankItem::mouseEnter(const juce::MouseEvent& event)
{
	if (!playButton.getBounds().contains(event.getPosition()) &&
		!deleteButton.getBounds().contains(event.getPosition()))
	{
		setMouseCursor(juce::MouseCursor::DraggingHandCursor);
	}
	else
	{
		setMouseCursor(juce::MouseCursor::NormalCursor);
	}
}

void SampleBankItem::mouseExit(const juce::MouseEvent& /*event*/)
{
	setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SampleBankItem::mouseDown(const juce::MouseEvent& event)
{
	if (playButton.getBounds().contains(event.getPosition()) ||
		deleteButton.getBounds().contains(event.getPosition()))
	{
		return;
	}

	if (event.mods.isLeftButtonDown())
	{
		isSelected = true;
		repaint();
	}
}

void SampleBankItem::mouseDrag(const juce::MouseEvent& event)
{
	if (playButton.getBounds().contains(event.getMouseDownPosition()) ||
		deleteButton.getBounds().contains(event.getMouseDownPosition()))
	{
		return;
	}

	if (event.getDistanceFromDragStart() > 5 && !isDragging)
	{
		isDragging = true;
		repaint();

		if (event.mods.isCtrlDown() && sampleEntry)
		{
			juce::File sampleFile(sampleEntry->filePath);
			if (sampleFile.exists())
			{
				juce::StringArray files;
				files.add(sampleFile.getFullPathName());
				DBG("Starting external drag with: " + sampleFile.getFullPathName());
				performExternalDragDropOfFiles(files, false);
				return;
			}
		}

		juce::DragAndDropContainer* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this);
		if (dragContainer)
		{
			juce::var dragData;
			dragData = sampleEntry->id;
			dragContainer->startDragging(dragData, this);
		}
	}
}

void SampleBankItem::mouseUp(const juce::MouseEvent& /*event*/)
{
	isDragging = false;
	isSelected = false;
	repaint();
}

void SampleBankItem::updateLabels()
{
	if (!sampleEntry) return;

	nameLabel.setText(sampleEntry->originalPrompt, juce::dontSendNotification);
	durationLabel.setText(formatDuration(sampleEntry->duration), juce::dontSendNotification);
	bpmLabel.setText(juce::String(sampleEntry->bpm, 1) + " BPM", juce::dontSendNotification);
	usageLabel.setText(formatUsage(), juce::dontSendNotification);
}

juce::String SampleBankItem::formatDuration(float seconds)
{
	int mins = (int)(seconds / 60);
	int secs = (int)(seconds) % 60;
	int ms = (int)((seconds - (int)seconds) * 100);

	if (mins > 0)
		return juce::String::formatted("%d:%02d.%02d", mins, secs, ms);
	else
		return juce::String::formatted("%d.%02ds", secs, ms);
}

juce::String SampleBankItem::formatUsage()
{
	if (sampleEntry->usedInProjects.empty())
		return "Unused";
	else if (sampleEntry->usedInProjects.size() == 1)
		return "1 project";
	else
		return juce::String(sampleEntry->usedInProjects.size()) + " projects";
}

SampleBankPanel::SampleBankPanel(DjIaVstProcessor& processor)
	: audioProcessor(processor)
{
	categoryNames[SampleCategory::All] = "All Samples";
	categoryNames[SampleCategory::Drums] = "Drums";
	categoryNames[SampleCategory::Bass] = "Bass";
	categoryNames[SampleCategory::Melody] = "Melody";
	categoryNames[SampleCategory::Ambient] = "Ambient";
	categoryNames[SampleCategory::Percussion] = "Percussion";
	categoryNames[SampleCategory::Vocal] = "Vocal";
	categoryNames[SampleCategory::FX] = "FX";
	categoryNames[SampleCategory::Loop] = "Loops";
	categoryNames[SampleCategory::OneShot] = "One-shots";
	categoryNames[SampleCategory::House] = "House";
	categoryNames[SampleCategory::Techno] = "Techno";
	categoryNames[SampleCategory::HipHop] = "Hip-Hop";
	categoryNames[SampleCategory::Jazz] = "Jazz";
	categoryNames[SampleCategory::Rock] = "Rock";
	categoryNames[SampleCategory::Electronic] = "Electronic";
	categoryNames[SampleCategory::Piano] = "Piano";
	categoryNames[SampleCategory::Guitar] = "Guitar";
	categoryNames[SampleCategory::Synth] = "Synth";

	setupUI();
	refreshSampleList();

	juce::Timer::callAfterDelay(500, [this]()
		{
			refreshSampleList();
		});

	if (auto* bank = audioProcessor.getSampleBank())
	{
		bank->onBankChanged = [this]()
			{
				juce::MessageManager::callAsync([this]()
					{
						refreshSampleList();
					});
			};
	}
}

SampleBankPanel::~SampleBankPanel()
{
	stopPreview();
	if (auto* bank = audioProcessor.getSampleBank())
	{
		bank->onBankChanged = nullptr;
	}
}

void SampleBankPanel::playPreview(SampleBankEntry* entry)
{
	if (!entry) return;

	if (currentPreviewEntry && currentPreviewItem)
	{
		currentPreviewItem->setIsPlaying(false);
	}

	stopPreview();

	bool previewStarted = audioProcessor.previewSampleFromBank(entry->id);
	if (!previewStarted) {
		DBG("Failed to start preview for: " + entry->originalPrompt);
		return;
	}

	currentPreviewEntry = entry;

	for (auto& item : sampleItems)
	{
		if (item->getSampleEntry() == entry)
		{
			item->setIsPlaying(true);
			currentPreviewItem = item.get();
			break;
		}
	}

	startTimer(100);
}

void SampleBankPanel::stopPreview()
{
	audioProcessor.stopSamplePreview();

	if (currentPreviewItem)
	{
		currentPreviewItem->setIsPlaying(false);
		currentPreviewItem = nullptr;
	}

	currentPreviewEntry = nullptr;
	stopTimer();
}

void SampleBankPanel::timerCallback()
{
	if (currentPreviewEntry && !audioProcessor.isSamplePreviewing())
	{
		stopPreview();
	}
}

void SampleBankPanel::paint(juce::Graphics& g)
{
	juce::ColourGradient gradient(
		ColourPalette::backgroundDeep, 0.0f, 0.0f,
		ColourPalette::backgroundMid, 0.0f, static_cast<float>(getHeight()),
		false);
	g.setGradientFill(gradient);
	g.fillAll();

	g.setColour(ColourPalette::backgroundLight);
	g.drawRect(getLocalBounds(), 1);
}

void SampleBankPanel::resized()
{
	auto area = getLocalBounds().reduced(10);

	auto headerArea = area.removeFromTop(40);
	titleLabel.setBounds(headerArea.removeFromLeft(150));
	cleanupButton.setBounds(headerArea.removeFromRight(100).reduced(5));
	headerArea.removeFromRight(5);
	sortMenu.setBounds(headerArea.removeFromRight(150).reduced(5));

	auto infoArea = area.removeFromTop(40);
	infoLabel.setBounds(infoArea);
	area.removeFromTop(5);
	auto categoryArea = area.removeFromTop(35);
	categoryFilter.setBounds(categoryArea.removeFromLeft(150));
	categoryArea.removeFromLeft(10);
	categoryInput.setBounds(categoryArea);

	area.removeFromTop(5);
	auto buttonArea = area.removeFromTop(30);
	addCategoryButton.setBounds(buttonArea.removeFromLeft(60).reduced(5));
	buttonArea.removeFromLeft(5);
	editCategoryButton.setBounds(buttonArea.removeFromLeft(60).reduced(5));
	buttonArea.removeFromLeft(5);
	deleteCategoryButton.setBounds(buttonArea.removeFromLeft(60).reduced(5));

	area.removeFromTop(5);
	samplesViewport.setBounds(area);

	int totalHeight = 5;
	for (auto& item : sampleItems)
	{
		totalHeight += item->getHeight() + 5;
	}
	samplesContainer.setSize(area.getWidth() - 20, totalHeight);
}

void SampleBankPanel::refreshSampleList()
{
	sampleItems.clear();
	samplesContainer.removeAllChildren();
	auto* bank = audioProcessor.getSampleBank();
	if (!bank) return;

	auto samples = bank->getAllSamples();

	if (currentCategory != SampleCategory::All)
	{
		juce::String categoryName = categoryNames[currentCategory];
		samples.erase(
			std::remove_if(samples.begin(), samples.end(),
				[&categoryName](const SampleBankEntry* entry)
				{
					return std::find(entry->categories.begin(), entry->categories.end(), categoryName) == entry->categories.end();
				}),
			samples.end()
		);
	}

	switch (currentSortType)
	{
	case SortType::Time:
		std::sort(samples.begin(), samples.end(),
			[](const SampleBankEntry* a, const SampleBankEntry* b)
			{
				return a->creationTime > b->creationTime;
			});
		break;

	case SortType::Prompt:
		std::sort(samples.begin(), samples.end(),
			[](const SampleBankEntry* a, const SampleBankEntry* b)
			{
				return a->originalPrompt.compareIgnoreCase(b->originalPrompt) < 0;
			});
		break;

	case SortType::Usage:
		std::sort(samples.begin(), samples.end(),
			[](const SampleBankEntry* a, const SampleBankEntry* b)
			{
				return a->usedInProjects.size() > b->usedInProjects.size();
			});
		break;

	case SortType::BPM:
		std::sort(samples.begin(), samples.end(),
			[](const SampleBankEntry* a, const SampleBankEntry* b)
			{
				return a->bpm > b->bpm;
			});
		break;

	case SortType::Duration:
		std::sort(samples.begin(), samples.end(),
			[](const SampleBankEntry* a, const SampleBankEntry* b)
			{
				return a->duration > b->duration;
			});
		break;
	}

	createSampleItems(samples);
}

void SampleBankPanel::setVisible(bool shouldBeVisible)
{
	Component::setVisible(shouldBeVisible);
	if (shouldBeVisible)
	{
		for (auto& item : sampleItems)
		{
			item->loadAudioDataIfNeeded();
		}
		juce::Timer::callAfterDelay(100, [this]()
			{
				refreshSampleList();
			});
	}
	else
	{
		stopPreview();
		sampleItems.clear();
		samplesContainer.removeAllChildren();
	}
}

void SampleBankPanel::setupUI()
{
	addAndMakeVisible(titleLabel);
	titleLabel.setText("Sample Bank", juce::dontSendNotification);
	titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
	titleLabel.setColour(juce::Label::textColourId, ColourPalette::textAccent);

	addAndMakeVisible(infoLabel);
	infoLabel.setText("Preview plays on channel 9 (Preview). Enable multioutput in DAW to hear it.\nDrag: Drop on track | Ctrl+Drag: Drop in DAW", juce::dontSendNotification);
	infoLabel.setFont(juce::FontOptions(12.0f));
	infoLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
	infoLabel.setJustificationType(juce::Justification::centredLeft);

	addAndMakeVisible(cleanupButton);
	cleanupButton.setButtonText("Clean Unused");
	cleanupButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonDanger);
	cleanupButton.onClick = [this]() { cleanupUnusedSamples(); };

	addAndMakeVisible(sortMenu);
	sortMenu.addItem("Sort by: Recent", SortType::Time);
	sortMenu.addItem("Sort by: Prompt", SortType::Prompt);
	sortMenu.addItem("Sort by: Usage", SortType::Usage);
	sortMenu.addItem("Sort by: BPM", SortType::BPM);
	sortMenu.addItem("Sort by: Duration", SortType::Duration);
	sortMenu.setSelectedId(SortType::Prompt);
	sortMenu.onChange = [this]()
		{
			currentSortType = static_cast<SortType>(sortMenu.getSelectedId());
			refreshSampleList();
		};

	addAndMakeVisible(samplesViewport);
	samplesViewport.setViewedComponent(&samplesContainer, false);
	samplesViewport.setScrollBarsShown(true, false);

	addAndMakeVisible(categoryFilter);
	for (const auto& info : categoryInfos)
	{
		categoryFilter.addItem(info.name, info.id + 1);
	}
	categoryFilter.setSelectedId(1);
	categoryFilter.onChange = [this]()
		{
			currentCategory = static_cast<SampleCategory>(categoryFilter.getSelectedId() - 1);
			refreshSampleList();
		};

	addAndMakeVisible(categoryInput);
	categoryInput.setTextToShowWhenEmpty("New category name...", ColourPalette::textSecondary);

	addAndMakeVisible(addCategoryButton);
	addCategoryButton.setButtonText("Add");
	addCategoryButton.setColour(juce::TextButton::buttonColourId, ColourPalette::emerald);

	addAndMakeVisible(editCategoryButton);
	editCategoryButton.setButtonText("Edit");
	editCategoryButton.setColour(juce::TextButton::buttonColourId, ColourPalette::amber);

	addAndMakeVisible(deleteCategoryButton);
	deleteCategoryButton.setButtonText("Delete");
	deleteCategoryButton.setColour(juce::TextButton::buttonColourId, ColourPalette::coral);
}

void SampleBankPanel::createSampleItems(const std::vector<SampleBankEntry*>& samples)
{
	int yPos = 5;

	for (auto* sampleEntry : samples)
	{
		auto item = std::make_unique<SampleBankItem>(sampleEntry, audioProcessor);

		item->onPreviewRequested = [this](SampleBankEntry* entry)
			{
				playPreview(entry);
			};
		item->onStopRequested = [this]()
			{
				stopPreview();
			};
		item->onDeleteRequested = [this](const juce::String& sampleId)
			{
				auto* entry = audioProcessor.getSampleBank()->getSample(sampleId);
				if (entry)
				{
					showDeleteConfirmation(sampleId, entry->originalPrompt);
				}
			};

		item->setBounds(5, yPos, samplesContainer.getWidth() - 10, 80);
		samplesContainer.addAndMakeVisible(item.get());
		if (isVisible())
		{
			item->loadAudioDataIfNeeded();
		}
		sampleItems.push_back(std::move(item));

		yPos += 85;
	}

	samplesContainer.setSize(samplesViewport.getWidth() - 20, yPos + 5);
	resized();
}

void SampleBankPanel::deleteSample(const juce::String& sampleId)
{
	auto* bank = audioProcessor.getSampleBank();
	if (bank && bank->removeSample(sampleId))
	{
		refreshSampleList();
	}
}

void SampleBankPanel::cleanupUnusedSamples()
{
	auto* bank = audioProcessor.getSampleBank();
	if (!bank) return;

	auto unusedSamples = bank->getUnusedSamples();
	if (unusedSamples.empty())
	{
		juce::AlertWindow::showMessageBoxAsync(
			juce::MessageBoxIconType::InfoIcon,
			"Clean Unused Samples",
			"No unused samples found.",
			"OK");
		return;
	}

	juce::String message = "Found " + juce::String(unusedSamples.size()) +
		" unused samples.\n\nDelete them all?";

	juce::AlertWindow::showAsync(
		juce::MessageBoxOptions()
		.withIconType(juce::MessageBoxIconType::WarningIcon)
		.withTitle("Clean Unused Samples")
		.withMessage(message)
		.withButton("Delete All")
		.withButton("Cancel"),
		[this, bank](int result)
		{
			if (result == 1)
			{
				int removed = bank->removeUnusedSamples();
				refreshSampleList();

				juce::AlertWindow::showMessageBoxAsync(
					juce::MessageBoxIconType::InfoIcon,
					"Cleanup Complete",
					"Removed " + juce::String(removed) + " unused samples.",
					"OK");
			}
		});
}

void SampleBankPanel::showDeleteConfirmation(const juce::String& sampleId, const juce::String& sampleName)
{
	auto* entry = audioProcessor.getSampleBank()->getSample(sampleId);
	if (!entry) return;

	juce::String message = "Delete sample:\n'" + sampleName + "'";

	if (!entry->usedInProjects.empty())
	{
		message += "\n\nWarning: This sample is used in " +
			juce::String(entry->usedInProjects.size()) + " project(s).";
	}

	juce::AlertWindow::showAsync(
		juce::MessageBoxOptions()
		.withIconType(juce::MessageBoxIconType::WarningIcon)
		.withTitle("Delete Sample")
		.withMessage(message)
		.withButton("Delete")
		.withButton("Cancel"),
		[this, sampleId](int result)
		{
			if (result == 1)
			{
				deleteSample(sampleId);
			}
		});
}