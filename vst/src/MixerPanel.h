/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include "JuceHeader.h"

class MixerChannel;
class DjIaVstProcessor;
class MasterChannel;

class MixerPanel : public juce::Component
{
public:
	MixerPanel(DjIaVstProcessor& processor);
	~MixerPanel();

	void updateTrackName(const juce::String& trackId, const juce::String& newName);
	void updateAllMixerComponents();

	float getMasterVolume() const;
	float getMasterPan() const;

	void calculateMasterLevel();
	void refreshMixerChannels();
	void refreshAllChannels();

	void trackAdded(const juce::String& trackId);
	void trackRemoved(const juce::String& trackId);
	void trackSelected(const juce::String& trackId);

	void paint(juce::Graphics& g) override;
	void resized() override;

private:
	void displayChannelsContainer(int xPos);
	void positionMixer(std::unique_ptr<MixerChannel>& mixerChannel, int& xPos,
		const int channelWidth, const int channelSpacing);

	DjIaVstProcessor& audioProcessor;

	std::unique_ptr<MasterChannel> masterChannel;
	float masterVolume = 0.8f;
	float masterPan = 0.0f;

	juce::Viewport channelsViewport;
	juce::Component channelsContainer;
	std::vector<std::unique_ptr<MixerChannel>> mixerChannels;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};