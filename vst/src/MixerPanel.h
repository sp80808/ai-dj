#pragma once
#include "JuceHeader.h"
#include "MixerChannel.h"
#include "MasterChannel.h"
#include "PluginProcessor.h"


class MixerPanel : public juce::Component
{
public:
	MixerPanel(DjIaVstProcessor& processor) : audioProcessor(processor)
	{
		masterChannel = std::make_unique<MasterChannel>();
		addAndMakeVisible(*masterChannel);

		masterChannel->onMasterVolumeChanged = [this](float volume)
			{
				masterVolume = volume;
				audioProcessor.setMasterVolume(volume);
			};

		masterChannel->onMasterPanChanged = [this](float pan)
			{
				masterPan = pan;
				audioProcessor.setMasterPan(pan);
			};

		masterChannel->onMasterEQChanged = [this](float high, float mid, float low)
			{
				audioProcessor.setMasterEQ(high, mid, low);
			};

		addAndMakeVisible(channelsViewport);
		channelsViewport.setViewedComponent(&channelsContainer, false);
		channelsViewport.setScrollBarsShown(false, true);

		refreshMixerChannels();
		audioProcessor.onUIUpdateNeeded = [this]()
			{
				updateAllMixerComponents();
			};
	}

	void updateTrackName(const juce::String& trackId, const juce::String& newName)
	{
		for (auto& channel : mixerChannels)
		{
			if (channel->getTrackId() == trackId)
			{
				channel->trackNameLabel.setText(newName, juce::dontSendNotification);
				break;
			}
		}
	}

	void updateAllMixerComponents()
	{
		for (auto& channel : mixerChannels)
		{
			channel->updateVUMeters();
		}
		calculateMasterLevel();
		masterChannel->updateMasterLevels();
	}

	float getMasterVolume() const { return masterVolume; }
	float getMasterPan() const { return masterPan; }

	void calculateMasterLevel()
	{
		float totalLevel = 0.0f;
		float maxLevel = 0.0f;
		int activeChannels = 0;
		for (auto& channel : mixerChannels)
		{
			float channelLevel = channel->getCurrentAudioLevel();
			if (channelLevel > 0.01f)
			{
				totalLevel += channelLevel * channelLevel;
				maxLevel = std::max(maxLevel, channelLevel);
				activeChannels++;
			}
		}

		if (activeChannels > 0)
		{
			float rmsLevel = std::sqrt(totalLevel / activeChannels);
			float finalLevel = (rmsLevel * 0.7f) + (maxLevel * 0.3f);
			finalLevel *= masterVolume;
			masterChannel->setRealAudioLevel(finalLevel);
		}
		else
		{
			masterChannel->setRealAudioLevel(0.0f);
		}
	}

	void refreshMixerChannels()
	{
		auto trackIds = audioProcessor.getAllTrackIds();

		channelsContainer.removeAllChildren();
		mixerChannels.clear();

		int xPos = 5;
		const int channelWidth = 90;
		const int channelSpacing = 5;

		for (const auto& trackId : trackIds)
		{
			TrackData* trackData = audioProcessor.getTrack(trackId);
			if (!trackData)
				continue;

			auto mixerChannel = std::make_unique<MixerChannel>(trackId);
			mixerChannel->setTrackData(trackData);
			auto* channelPtr = mixerChannel.get();
			refreshChannelsCallbacks(mixerChannel, channelPtr);
			positionMixer(mixerChannel, xPos, channelWidth, channelSpacing);
		}


		displayChannelsContainer(xPos);
	}

	void displayChannelsContainer(int xPos)
	{
		int finalHeight = std::max(400, getHeight() - 10);
		channelsContainer.setSize(xPos, finalHeight);
		channelsContainer.setVisible(true);
		channelsContainer.repaint();
	}

	void positionMixer(std::unique_ptr<MixerChannel>& mixerChannel, int& xPos, const int channelWidth, const int channelSpacing)
	{
		int containerHeight = std::max(400, channelsContainer.getHeight());
		mixerChannel->setBounds(xPos, 0, channelWidth, containerHeight);

		channelsContainer.addAndMakeVisible(mixerChannel.get());
		mixerChannels.push_back(std::move(mixerChannel));

		xPos += channelWidth + channelSpacing;
	}

	void refreshChannelsCallbacks(std::unique_ptr<MixerChannel>& mixerChannel, MixerChannel* channelPtr)
	{
		mixerChannel->onSoloChanged = [this, channelPtr](const juce::String& id, bool solo)
			{
				for (auto& channel : mixerChannels)
				{
					channel->updateFromTrackData();
					channel->repaint();
				}
			};

		mixerChannel->onMuteChanged = [this, channelPtr](const juce::String& id, bool mute)
			{
				channelPtr->updateFromTrackData();
				channelPtr->repaint();
			};

		mixerChannel->onPlayTrack = [this, channelPtr](const juce::String& id)
			{
				if (auto* track = audioProcessor.getTrack(id))
				{
					audioProcessor.startNotePlaybackForTrack(id, track->midiNote, 126.0);
				}
				juce::Timer::callAfterDelay(50, [channelPtr]()
					{
						if (channelPtr) {
							channelPtr->updateFromTrackData();
							channelPtr->repaint();
						} });
			};

		mixerChannel->onStopTrack = [this, channelPtr](const juce::String& id)
			{
				if (auto* track = audioProcessor.getTrack(id))
				{
					track->isPlaying = false;
					track->isArmed = false;
				}
				channelPtr->updateFromTrackData();
				channelPtr->repaint();
			};

		mixerChannel->onFineChanged = [this](const juce::String& id, float fine)
			{
				if (auto* track = audioProcessor.getTrack(id))
				{
					track->fineOffset = fine;
				}
			};

		mixerChannel->onMidiLearn = [this](const juce::String& trackId, int controlType)
			{

			};
	}

	void paint(juce::Graphics& g) override
	{
		auto bounds = getLocalBounds();
		juce::ColourGradient gradient(
			juce::Colour(0xff1a1a1a), 0, 0,
			juce::Colour(0xff2a2a2a), 0, getHeight(),
			false);
		g.setGradientFill(gradient);
		g.fillAll();
		g.setColour(juce::Colour(0xff333333));
		for (int i = 0; i < getWidth(); i += 2)
		{
			g.drawVerticalLine(i, 0, getHeight());
			g.setOpacity(0.1f);
		}
		int masterX = getWidth() - 100;
		g.setColour(juce::Colour(0xff666666));
		g.drawLine(masterX - 5, 10, masterX - 5, getHeight() - 10, 2.0f);
	}

	void resized() override
	{
		auto area = getLocalBounds();

		auto masterArea = area.removeFromRight(100);
		masterChannel->setBounds(masterArea.reduced(5));

		area.removeFromRight(10);

		channelsViewport.setBounds(area);

		int containerHeight = channelsViewport.getHeight() - 20;
		channelsContainer.setSize(channelsContainer.getWidth(), containerHeight);

		int xPos = 5;
		const int channelWidth = 90;
		const int channelSpacing = 5;

		for (auto& channel : mixerChannels)
		{
			channel->setBounds(xPos, 0, channelWidth, containerHeight);
			xPos += channelWidth + channelSpacing;
		}
	}

	void trackAdded(const juce::String& trackId)
	{
		refreshMixerChannels();
		resized();
	}

	void trackRemoved(const juce::String& trackId)
	{
		refreshMixerChannels();
		resized();
	}

	void trackSelected(const juce::String& trackId)
	{
		for (auto& channel : mixerChannels)
		{
			bool isThisTrackSelected = (channel->getTrackId() == trackId);
			channel->setSelected(isThisTrackSelected);
		}
	}

private:
	DjIaVstProcessor& audioProcessor;

	std::unique_ptr<MasterChannel> masterChannel;
	float masterVolume = 0.8f;
	float masterPan = 0.0f;

	juce::Viewport channelsViewport;
	juce::Component channelsContainer;
	std::vector<std::unique_ptr<MixerChannel>> mixerChannels;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};