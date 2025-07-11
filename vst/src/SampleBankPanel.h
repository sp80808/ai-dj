/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include "JuceHeader.h"
#include "SampleBank.h"
#include "ColourPalette.h"

class DjIaVstProcessor;

class SampleBankItem : public juce::Component
{
public:
	SampleBankItem(SampleBankEntry* entry, DjIaVstProcessor& processor);
	~SampleBankItem() override;

	void paint(juce::Graphics& g) override;
	void resized() override;
	void mouseDown(const juce::MouseEvent& event) override;
	void mouseDrag(const juce::MouseEvent& event) override;
	void mouseUp(const juce::MouseEvent& event) override;
	void mouseEnter(const juce::MouseEvent& event) override;
	void mouseExit(const juce::MouseEvent& event) override;
	void setIsPlaying(bool playing);

	SampleBankEntry* getSampleEntry() const { return sampleEntry; }

	std::function<void(const juce::String&)> onDeleteRequested;
	std::function<void(SampleBankEntry*)> onPreviewRequested;
	std::function<void()> onStopRequested;

private:
	SampleBankEntry* sampleEntry;
	DjIaVstProcessor& audioProcessor;

	juce::Label nameLabel;
	juce::Label durationLabel;
	juce::Label bpmLabel;
	juce::Label usageLabel;
	juce::TextButton playButton;
	juce::TextButton deleteButton;
	bool isPlaying = false;

	bool isSelected = false;
	bool isDragging = false;

	void updateLabels();
	juce::String formatDuration(float seconds);
	juce::String formatUsage();
	void updatePlayButton();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleBankItem)
};

class SampleBankPanel : public juce::Component,
	public juce::Timer
{
public:
	SampleBankPanel(DjIaVstProcessor& processor);
	~SampleBankPanel() override;

	void paint(juce::Graphics& g) override;
	void resized() override;
	void timerCallback() override;
	void refreshSampleList();
	void setVisible(bool shouldBeVisible) override;

	std::function<void(const juce::String&, const juce::String&)> onSampleDroppedToTrack;

private:
	DjIaVstProcessor& audioProcessor;

	juce::Label titleLabel;
	juce::TextButton cleanupButton;
	juce::Viewport samplesViewport;
	juce::Component samplesContainer;

	enum class SortType
	{
		Time,
		Prompt,
		Usage
	};
	SortType currentSortType = SortType::Prompt;

	std::vector<std::unique_ptr<SampleBankItem>> sampleItems;

	SampleBankEntry* currentPreviewEntry = nullptr;
	SampleBankItem* currentPreviewItem = nullptr;

	void setupUI();
	void createSampleItems(const std::vector<SampleBankEntry*>& samples);
	void playPreview(SampleBankEntry* entry);
	void stopPreview();
	void deleteSample(const juce::String& sampleId);
	void cleanupUnusedSamples();
	void showDeleteConfirmation(const juce::String& sampleId, const juce::String& sampleName);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleBankPanel)
};