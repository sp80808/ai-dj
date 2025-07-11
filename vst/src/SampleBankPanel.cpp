/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#include "SampleBankPanel.h"
#include "PluginProcessor.h"


SampleBankItem::SampleBankItem(SampleBankEntry* entry, DjIaVstProcessor& processor)
	: sampleEntry(entry), audioProcessor(processor)
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
	setSize(400, 45);
}

SampleBankItem::~SampleBankItem() = default;

void SampleBankItem::setIsPlaying(bool playing)
{
	isPlaying = playing;
	updatePlayButton();
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
}

void SampleBankItem::resized()
{
	auto area = getLocalBounds().reduced(8);
	auto buttonArea = area.removeFromRight(80);
	playButton.setBounds(buttonArea.removeFromLeft(35).reduced(3));
	buttonArea.removeFromLeft(5);
	deleteButton.setBounds(buttonArea.removeFromLeft(35).reduced(3));
	area.removeFromRight(10);

	auto topRow = area.removeFromTop(20);
	nameLabel.setBounds(topRow.removeFromLeft(200));

	auto bottomRow = area.removeFromTop(20);
	durationLabel.setBounds(bottomRow.removeFromLeft(60));
	bottomRow.removeFromLeft(10);
	bpmLabel.setBounds(bottomRow.removeFromLeft(60));
	bottomRow.removeFromLeft(10);
	usageLabel.setBounds(bottomRow);
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

	auto infoArea = area.removeFromTop(20);
	infoLabel.setBounds(infoArea);

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
	}

	createSampleItems(samples);
}

void SampleBankPanel::setVisible(bool shouldBeVisible)
{
	Component::setVisible(shouldBeVisible);
	if (shouldBeVisible)
	{
		juce::Timer::callAfterDelay(100, [this]()
			{
				refreshSampleList();
			});
	}
	else
	{
		stopPreview();
	}
}

void SampleBankPanel::setupUI()
{
	addAndMakeVisible(titleLabel);
	titleLabel.setText("Sample Bank", juce::dontSendNotification);
	titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
	titleLabel.setColour(juce::Label::textColourId, ColourPalette::textAccent);

	addAndMakeVisible(infoLabel);
	infoLabel.setText("Preview plays on channel 9 (Preview). Enable multioutput in DAW to hear it.", juce::dontSendNotification);
	infoLabel.setFont(juce::FontOptions(12.0f));
	infoLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
	infoLabel.setJustificationType(juce::Justification::centredLeft);

	addAndMakeVisible(cleanupButton);
	cleanupButton.setButtonText("Clean Unused");
	cleanupButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonPrimary);
	cleanupButton.onClick = [this]() { cleanupUnusedSamples(); };

	addAndMakeVisible(samplesViewport);
	samplesViewport.setViewedComponent(&samplesContainer, false);
	samplesViewport.setScrollBarsShown(true, false);
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

		item->setBounds(5, yPos, samplesContainer.getWidth() - 10, 45);
		samplesContainer.addAndMakeVisible(item.get());
		sampleItems.push_back(std::move(item));

		yPos += 50;
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