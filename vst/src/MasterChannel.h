#pragma once
#include "JuceHeader.h"
#include "PluginProcessor.h"

class MasterChannel : public juce::Component
{
public:
	MasterChannel(DjIaVstProcessor &processor);
	MasterChannel::~MasterChannel();
	void drawMasterVUMeter(juce::Graphics &g, juce::Rectangle<int> bounds) const;
	void drawPeakHoldLine(int numSegments, juce::Rectangle<float> &vuArea, float segmentHeight, juce::Graphics &g) const;
	void drawMasterClipping(juce::Rectangle<float> &vuArea, juce::Graphics &g) const;
	void drawMasterChanelSegments(juce::Rectangle<float> &vuArea, int i, float segmentHeight, int numSegments, juce::Graphics &g) const;
	void setRealAudioLevel(float level);
	void updateMasterLevels();

	std::function<void(float)> onMasterVolumeChanged;
	std::function<void(float)> onMasterPanChanged;
	std::function<void(float, float, float)> onMasterEQChanged;

private:
	DjIaVstProcessor &audioProcessor;
	juce::Slider masterVolumeSlider;
	juce::Slider masterPanKnob;
	juce::Slider highKnob, midKnob, lowKnob;

	float realAudioLevel = 0.0f;
	bool hasRealAudio = false;

	juce::Label masterLabel;
	juce::Label highLabel, midLabel, lowLabel, panLabel;

	float masterLevel = 0.0f;
	float masterPeakHold = 0.0f;
	int masterPeakHoldTimer = 0;
	bool isClipping = false;

	void setupUI();
	void paint(juce::Graphics &g) override;
	void resized() override;
	void setupMidiLearn();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterChannel)
};
