#pragma once
#include "JuceHeader.h"
#include "TrackManager.h"

class WaveformDisplay;

class DjIaVstProcessor;

class CustomInfoLabelLookAndFeel : public juce::LookAndFeel_V4
{
public:
	void drawLabel(juce::Graphics& g, juce::Label& label) override
	{
		auto bounds = label.getLocalBounds().toFloat();

		g.setColour(juce::Colours::black);
		g.fillRoundedRectangle(bounds, 4.0f);

		g.setColour(juce::Colour(0x6600aaff));
		g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

		g.setColour(juce::Colour(0xff44aaff));
		g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
		g.drawText(label.getText(), bounds.reduced(8, 2),
			juce::Justification::centredLeft, false);
	}
};

class TrackComponent : public juce::Component, public juce::Timer
{
public:
	TrackComponent(const juce::String& trackId, DjIaVstProcessor& processor);
	~TrackComponent();
	juce::String getTrackId() const
	{
		return trackId;
	}

	std::function<void(const juce::String&)> onDeleteTrack;
	std::function<void(const juce::String&)> onSelectTrack;
	std::function<void(const juce::String&)> onGenerateForTrack;
	std::function<void(const juce::String&, const juce::String&)> onTrackRenamed;
	juce::TextButton showWaveformButton;
	TrackData* getTrack() const { return track; }

	std::function<void(const juce::String&, const juce::String&)> onReorderTrack;

	void setSelected(bool selected);
	void setTrackData(TrackData* trackData);
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

	bool isEditingLabel = false;

	juce::TextButton* getGenerateButton() { return &generateButton; }
	juce::Slider* getBpmOffsetSlider() { return &bpmOffsetSlider; }

private:
	juce::String trackId;
	TrackData* track;
	bool isSelected = false;
	std::unique_ptr<WaveformDisplay> waveformDisplay;
	DjIaVstProcessor& audioProcessor;
	CustomInfoLabelLookAndFeel customLookAndFeel;
	juce::Label trackNumberLabel;

	juce::TextButton selectButton;
	juce::Label trackNameLabel;
	juce::TextButton deleteButton;
	juce::TextButton generateButton;
	juce::Label infoLabel;

	juce::ComboBox timeStretchModeSelector;

	juce::Slider bpmOffsetSlider;
	juce::Label bpmOffsetLabel;

	juce::ComboBox midiNoteSelector;

	bool isGenerating = false;
	bool blinkState = false;

	void calculateHostBasedDisplay();
	float calculateEffectiveBpm();
	void paint(juce::Graphics& g);
	void resized();
	void timerCallback() override;
	void setupUI();
	void adjustLoopPointsToTempo();
	void updateTrackInfo();
	void setupMidiLearn();
	juce::Colour getTrackColour(int trackIndex);
};