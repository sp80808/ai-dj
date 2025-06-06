#pragma once
#include "JuceHeader.h"

class DjIaVstProcessor;

class SequencerComponent : public juce::Component
{
public:
	struct StepData {
		bool active = false;
		float velocity = 0.8f;
	};

	SequencerComponent(const juce::String& trackId, DjIaVstProcessor& processor);

	void paint(juce::Graphics& g) override;
	void resized() override;
	void mouseDown(const juce::MouseEvent& event) override;

	void setCurrentStep(int step);
	void setPlaying(bool playing);
	void setNumMeasures(int measures);
	void setCurrentMeasure(int measure);

	void updateFromTrackData();

	bool isSequencerPlaying() const { return isPlaying; }

private:
	juce::String trackId;
	DjIaVstProcessor& audioProcessor;

	static const int MAX_STEPS_PER_MEASURE = 16;
	static const int MAX_MEASURES = 4;

	bool isEditing = false;

	StepData steps[MAX_MEASURES][MAX_STEPS_PER_MEASURE];

	int currentStep = 0;
	int currentMeasure = 0;
	int numMeasures = 1;
	int beatsPerMeasure = 4;
	bool isPlaying = false;

	juce::Slider measureSlider;
	juce::Slider timeSignatureSlider;

	juce::Timer* editingTimer = nullptr;

	juce::TextButton prevMeasureButton, nextMeasureButton;

	juce::Label measureLabel;

	juce::Rectangle<int> getStepBounds(int step);

	void toggleStep(int step);

	double samplesPerStep;
	double stepAccumulator;
};