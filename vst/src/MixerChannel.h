#pragma once
#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "MidiLearnableComponents.h"

class MixerChannel : public juce::Component, public juce::Timer
{
public:
	MixerChannel(const juce::String &trackId, DjIaVstProcessor &processor);
	~MixerChannel() override;
	juce::String getTrackId() const { return trackId; }
	juce::Label trackNameLabel;
	std::function<void(const juce::String &, bool)> onSoloChanged;
	std::function<void(const juce::String &, bool)> onMuteChanged;
	std::function<void(const juce::String &)> onPlayTrack;
	std::function<void(const juce::String &)> onStopTrack;
	std::function<void(const juce::String &, int)> onMidiLearn;
	std::function<void(const juce::String &, float)> onPitchChanged;
	std::function<void(const juce::String &, float)> onFineChanged;
	std::function<void(const juce::String &, float)> onPanChanged;

	float getCurrentAudioLevel() const { return currentAudioLevel; }
	float getPeakLevel() const { return peakHold; }
	void setSelected(bool selected);
	void updateFromTrackData();
	void updateVUMeters();
	void setTrackData(TrackData *trackData);

private:
	DjIaVstProcessor &audioProcessor;
	juce::String trackId;
	TrackData *track;
	bool isSelected = false;
	int bypassMidiFrames = 0;
	std::atomic<bool> isUpdatingButtons{false};

	float currentAudioLevel = 0.0f;
	float peakHold = 0.0f;
	int peakHoldTimer = 0;
	std::vector<float> levelHistory;

	bool isBlinking = false;
	bool blinkState = false;

	MidiLearnableButton playButton;
	MidiLearnableButton stopButton;
	MidiLearnableButton muteButton;
	MidiLearnableButton soloButton;

	MidiLearnableSlider volumeSlider;

	MidiLearnableSlider pitchKnob;
	juce::Label pitchLabel;
	MidiLearnableSlider fineKnob;
	juce::Label fineLabel;

	MidiLearnableSlider panKnob;
	juce::Label panLabel;

	void paint(juce::Graphics &g) override;
	void drawVUMeter(juce::Graphics &g, juce::Rectangle<int> bounds);
	void fillMeters(juce::Rectangle<float> &vuArea, int i, float segmentHeight, int numSegments, float currentLevel, juce::Graphics &g);
	void resized() override;
	void updateVUMeter();
	float calculateInstantLevel();
	void setCurrentLevel(float level);
	void armToStop();
	void updateButtonColors();
	void timerCallback() override;
	void setupMidiLearn();
	void setupUI();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerChannel)
	JUCE_DECLARE_WEAK_REFERENCEABLE(MixerChannel)
};