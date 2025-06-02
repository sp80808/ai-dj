#pragma once
#include "JuceHeader.h"
#include "WaveformDisplay.h"
#include "TrackManager.h"

class DjIaVstProcessor;

class TrackComponent : public juce::Component, public juce::Timer
{
public:
	TrackComponent(const juce::String &trackId, DjIaVstProcessor &processor);
	~TrackComponent();
	juce::String getTrackId() const
	{
		return trackId;
	}

	std::function<void(const juce::String &)> onDeleteTrack;
	std::function<void(const juce::String &)> onSelectTrack;
	std::function<void(const juce::String &)> onGenerateForTrack;
	std::function<void(const juce::String &, const juce::String &)> onTrackRenamed;

	TrackData *getTrack() const { return track; }

	std::function<void(const juce::String &, const juce::String &)> onReorderTrack;

	void setSelected(bool selected);
	void setTrackData(TrackData *trackData);
	void refreshWaveformDisplay();
	bool isWaveformVisible() const;
	void startGeneratingAnimation();
	void stopGeneratingAnimation();
	void updateFromTrackData();
	void setGenerateButtonEnabled(bool enabled);
	void updateWaveformWithTimeStretch();
	void updatePlaybackPosition(double timeInSeconds);
	void toggleWaveformDisplay();
	void refreshWaveformIfNeeded();

	juce::TextButton *getGenerateButton() { return &generateButton; }
	juce::Slider *getBpmOffsetSlider() { return &bpmOffsetSlider; }

private:
	juce::String trackId;
	TrackData *track;
	bool isSelected = false;
	std::unique_ptr<WaveformDisplay> waveformDisplay;
	juce::TextButton showWaveformButton;
	DjIaVstProcessor &audioProcessor;

	juce::TextButton selectButton;
	juce::Label trackNameLabel;
	juce::TextButton deleteButton;
	juce::TextButton generateButton;
	juce::Label infoLabel;

	juce::ComboBox timeStretchModeSelector;

	juce::Slider bpmOffsetSlider;
	juce::Label bpmOffsetLabel;

	juce::TextEditor trackNameEditor;
	juce::ComboBox midiNoteSelector;

	bool isGenerating = false;
	bool blinkState = false;

	void calculateHostBasedDisplay();
	float calculateEffectiveBpm();
	void paint(juce::Graphics &g);
	void resized();
	void timerCallback() override;
	void setupUI();
	void adjustLoopPointsToTempo();
	void updateTrackInfo();
	void updateBpmSliderVisibility();
	void setupMidiLearn();
};