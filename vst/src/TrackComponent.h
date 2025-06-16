#pragma once
#include "JuceHeader.h"
#include "TrackManager.h"
#include "MidiLearnableComponents.h"

class WaveformDisplay;
class SequencerComponent;
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

class TrackComponent : public juce::Component, public juce::Timer, public juce::AudioProcessorParameter::Listener
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

	static const int BASE_HEIGHT = 60;
	static const int WAVEFORM_HEIGHT = 100;
	static const int SEQUENCER_HEIGHT = 100;

	juce::TextButton showWaveformButton;
	juce::TextButton sequencerToggleButton;

	TrackData* getTrack() const { return track; }

	std::function<void(const juce::String&, const juce::String&)> onReorderTrack;
	std::function<void(const juce::String&)> onPreviewTrack;

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
	void toggleSequencerDisplay();

	bool isEditingLabel = false;

	juce::TextButton* getGenerateButton() { return &generateButton; }
	juce::Slider* getBpmOffsetSlider() { return &bpmOffsetSlider; }

	SequencerComponent* getSequencer() const { return sequencer.get(); }

private:
	juce::String trackId;
	TrackData* track;
	bool isSelected = false;
	std::unique_ptr<WaveformDisplay> waveformDisplay;
	std::unique_ptr<SequencerComponent> sequencer;
	DjIaVstProcessor& audioProcessor;
	CustomInfoLabelLookAndFeel customLookAndFeel;
	juce::Label trackNumberLabel;

	juce::TextButton selectButton;
	juce::Label trackNameLabel;
	juce::TextButton deleteButton;
	MidiLearnableButton generateButton;
	juce::Label infoLabel;
	juce::TextButton previewButton;

	juce::ComboBox timeStretchModeSelector;

	juce::Slider bpmOffsetSlider;
	juce::Label bpmOffsetLabel;

	std::atomic<bool> isDestroyed{ false };

	bool isGenerating = false;
	bool blinkState = false;
	bool sequencerVisible = false;

	void calculateHostBasedDisplay();
	float calculateEffectiveBpm();
	void paint(juce::Graphics& g);
	void resized();
	void timerCallback() override;
	void parameterValueChanged(int parameterIndex, float newValue) override;
	void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
	void setupUI();
	void adjustLoopPointsToTempo();
	void updateTrackInfo();
	void setupMidiLearn();
	void learn(juce::String param, std::function<void(float)> uiCallback = nullptr);
	void removeMidiMapping(const juce::String& param);
	void addListener(juce::String name);
	void removeListener(juce::String name);
	void setButtonParameter(juce::String name, juce::Button& button);
	void updateUIFromParameter(const juce::String& paramName,
		const juce::String& slotPrefix,
		float newValue);

	juce::Colour getTrackColour(int trackIndex);
};