#pragma once
#include "JuceHeader.h"
#include "WaveformDisplay.h"
#include "TrackManager.h"
#include "MidiLearnableComponents.h"

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
	void refreshWaveformIfNeeded();
	void updatePlaybackPosition(double timeInSeconds);
	void toggleWaveformDisplay();

	MidiLearnableButton *getGenerateButton() { return &generateButton; }
	MidiLearnableSlider *getBpmOffsetSlider() { return &bpmOffsetSlider; }

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
	MidiLearnableButton generateButton;
	juce::Label infoLabel;

	juce::ComboBox timeStretchModeSelector;

	MidiLearnableSlider bpmOffsetSlider;
	juce::Label bpmOffsetLabel;

	juce::TextEditor trackNameEditor;
	juce::ComboBox midiNoteSelector;

	bool isGenerating = false;
	bool blinkState = false;

	void calculateHostBasedDisplay();
	void updateRealTimeDisplay();
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