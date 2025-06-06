#include "SequencerComponent.h"
#include "PluginProcessor.h"


SequencerComponent::SequencerComponent(const juce::String& trackId, DjIaVstProcessor& processor)
	: trackId(trackId), audioProcessor(processor)
{
	for (int m = 0; m < MAX_MEASURES; ++m) {
		for (int s = 0; s < MAX_STEPS_PER_MEASURE; ++s) {
			steps[m][s].active = false;
			steps[m][s].velocity = 0.8f;
		}
	}

	addAndMakeVisible(measureSlider);
	measureSlider.setRange(1, 4, 1);
	measureSlider.setValue(1);
	measureSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 30, 20);
	measureSlider.onValueChange = [this]() {
		setNumMeasures((int)measureSlider.getValue());
		};


	addAndMakeVisible(prevMeasureButton);
	prevMeasureButton.setButtonText("<");
	prevMeasureButton.onClick = [this]() {
		if (currentMeasure > 0) {
			setCurrentMeasure(currentMeasure - 1);
		}
		};

	addAndMakeVisible(nextMeasureButton);
	nextMeasureButton.setButtonText(">");
	nextMeasureButton.onClick = [this]() {
		if (currentMeasure < numMeasures - 1) {
			setCurrentMeasure(currentMeasure + 1);
		}
		};

	addAndMakeVisible(measureLabel);
	measureLabel.setText("1/1", juce::dontSendNotification);
	measureLabel.setJustificationType(juce::Justification::centred);

	updateFromTrackData();
}

void SequencerComponent::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	juce::ColourGradient gradient(
		juce::Colour(0xff1a1a1a), 0, 0,
		juce::Colour(0xff2d2d2d), 0, bounds.getHeight(),
		false);
	g.setGradientFill(gradient);
	g.fillRoundedRectangle(bounds.toFloat(), 6.0f);


	juce::Colour accentColour = juce::Colour(0xffff6600);
	juce::Colour beatColour = juce::Colour(0xffccaa00);
	juce::Colour subBeatColour = juce::Colour(0xff666666);


	static const std::vector<juce::Colour> trackColours = {
		juce::Colour(0xff5a7a9a), juce::Colour(0xff9a7a5a), juce::Colour(0xff6a8a6a),
		juce::Colour(0xff8a5a6a), juce::Colour(0xff7a6a8a), juce::Colour(0xff5a8a8a),
		juce::Colour(0xff7a8a6a), juce::Colour(0xff8a6a7a), juce::Colour(0xff8a6a5a),
		juce::Colour(0xff6a7a8a)
	};
	juce::Colour trackColour = trackColours[0];

	int stepsPerBeat = 4;
	int totalSteps = beatsPerMeasure * stepsPerBeat;

	for (int i = 0; i < 16; ++i) {
		auto stepBounds = getStepBounds(i);

		bool isVisible = (i < totalSteps);
		bool isStrongBeat = (i % stepsPerBeat == 0);
		bool isBeat = (i % (stepsPerBeat / 2) == 0);

		juce::Colour stepColour;
		juce::Colour borderColour;

		if (!isVisible) {
			stepColour = juce::Colour(0xff1a1a1a);
			borderColour = juce::Colour(0xff333333);
		}
		else if (steps[currentMeasure][i].active) {
			stepColour = trackColour;
			borderColour = trackColour.brighter(0.4f);
		}
		else {
			if (isStrongBeat) {
				stepColour = accentColour.withAlpha(0.3f);
				borderColour = accentColour;
			}
			else if (isBeat) {
				stepColour = beatColour.withAlpha(0.3f);
				borderColour = beatColour;
			}
			else {
				stepColour = subBeatColour.withAlpha(0.3f);
				borderColour = subBeatColour;
			}
		}

		if (i == currentStep && isPlaying && isVisible) {
			float pulseIntensity = 0.8f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f);
			stepColour = juce::Colours::white.withAlpha(pulseIntensity);
			borderColour = juce::Colours::white;
		}

		g.setColour(stepColour);
		g.fillRoundedRectangle(stepBounds.toFloat(), 3.0f);
		g.setColour(borderColour);
		g.drawRoundedRectangle(stepBounds.toFloat(), 3.0f, isVisible ? 1.0f : 0.5f);

		if (isVisible) {
			g.setColour(juce::Colours::white.withAlpha(isStrongBeat ? 0.9f : 0.6f));
			g.setFont(juce::Font(9.0f, isStrongBeat ? juce::Font::bold : juce::Font::plain));
			g.drawText(juce::String(i + 1), stepBounds, juce::Justification::centred);
		}
	}
}

juce::Rectangle<int> SequencerComponent::getStepBounds(int step)
{
	int stepWidth = 37;
	int stepHeight = 37;
	int margin = 4;
	int startY = 50;

	int x = 13 + step * (stepWidth + margin);
	int y = startY;

	return juce::Rectangle<int>(x, y, stepWidth, stepHeight);
}

void SequencerComponent::mouseDown(const juce::MouseEvent& event)
{
	int stepsToShow = beatsPerMeasure * 4;

	for (int i = 0; i < stepsToShow; ++i) {
		if (getStepBounds(i).contains(event.getPosition())) {
			isEditing = true;
			toggleStep(i);
			repaint();
			juce::Timer::callAfterDelay(50, [this]() {
				isEditing = false;
				});

			return;
		}
	}
}

void SequencerComponent::toggleStep(int step)
{
	TrackData* track = audioProcessor.getTrack(trackId);
	if (track) {
		track->sequencerData.steps[currentMeasure][step] = !track->sequencerData.steps[currentMeasure][step];
		track->sequencerData.velocities[currentMeasure][step] = 0.8f;

		// Synchronise l'affichage local
		steps[currentMeasure][step].active = track->sequencerData.steps[currentMeasure][step];
		steps[currentMeasure][step].velocity = track->sequencerData.velocities[currentMeasure][step];
	}

	// TODO: velocity
	if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
		// Shift+click to adjust velocity
	}
}

void SequencerComponent::setCurrentStep(int step)
{
	currentStep = step % (beatsPerMeasure * 4);
	repaint();
}


void SequencerComponent::resized()
{
	auto bounds = getLocalBounds();

	bounds.removeFromTop(10);
	bounds.removeFromLeft(13);

	auto controlArea = bounds.removeFromTop(30).removeFromLeft(230);
	auto pageArea = controlArea.removeFromLeft(120);
	prevMeasureButton.setBounds(pageArea.removeFromLeft(25));
	measureLabel.setBounds(pageArea.removeFromLeft(40));
	nextMeasureButton.setBounds(pageArea.removeFromLeft(25));

	controlArea.removeFromLeft(5);

	auto measureArea = controlArea.removeFromLeft(80);
	measureSlider.setBounds(measureArea);
}

void SequencerComponent::setCurrentMeasure(int measure)
{
	currentMeasure = juce::jlimit(0, numMeasures - 1, measure);
	measureLabel.setText(juce::String(currentMeasure + 1) + "/" + juce::String(numMeasures),
		juce::dontSendNotification);
	repaint();
}

void SequencerComponent::setNumMeasures(int measures)
{
	numMeasures = juce::jlimit(1, MAX_MEASURES, measures);
	if (currentMeasure >= numMeasures) {
		setCurrentMeasure(numMeasures - 1);
	}

	// Met à jour TrackData
	TrackData* track = audioProcessor.getTrack(trackId);
	if (track) {
		track->sequencerData.numMeasures = numMeasures;
	}

	measureLabel.setText(juce::String(currentMeasure + 1) + "/" + juce::String(numMeasures),
		juce::dontSendNotification);
	repaint();
}

void SequencerComponent::updateFromTrackData()
{
	if (isEditing) return;
	TrackData* track = audioProcessor.getTrack(trackId);
	if (track) {
		// Copie les données depuis TrackData
		for (int m = 0; m < 4; ++m) {
			for (int s = 0; s < 16; ++s) {
				steps[m][s].active = track->sequencerData.steps[m][s];
				steps[m][s].velocity = track->sequencerData.velocities[m][s];
			}
		}
		currentStep = track->sequencerData.currentStep;
		currentMeasure = track->sequencerData.currentMeasure;
		isPlaying = track->sequencerData.isPlaying;
		repaint();
	}
}

