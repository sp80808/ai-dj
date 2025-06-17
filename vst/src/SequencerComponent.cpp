#include "SequencerComponent.h"
#include "PluginProcessor.h"
#include "ColourPalette.h"

SequencerComponent::SequencerComponent(const juce::String& trackId, DjIaVstProcessor& processor)
	: trackId(trackId), audioProcessor(processor)
{
	setupUI();
	updateFromTrackData();
}

void SequencerComponent::setupUI() {

	addAndMakeVisible(measureSlider);
	measureSlider.setRange(1, 4, 1);
	measureSlider.setValue(1);
	measureSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 30, 20);
	measureSlider.onValueChange = [this]() {
		isEditing = true;
		setNumMeasures((int)measureSlider.getValue());
		juce::Timer::callAfterDelay(500, [this]() {
			isEditing = false;
			});
		};


	addAndMakeVisible(prevMeasureButton);
	prevMeasureButton.setButtonText("<");
	prevMeasureButton.onClick = [this]() {
		isEditing = true;
		if (currentMeasure > 0) {
			setCurrentMeasure(currentMeasure - 1);
		}
		juce::Timer::callAfterDelay(500, [this]() {
			isEditing = false;
			});
		};

	addAndMakeVisible(nextMeasureButton);
	nextMeasureButton.setButtonText(">");
	nextMeasureButton.onClick = [this]() {
		isEditing = true;
		if (currentMeasure < numMeasures - 1) {
			setCurrentMeasure(currentMeasure + 1);
		}
		juce::Timer::callAfterDelay(500, [this]() {
			isEditing = false;
			});
		};

	addAndMakeVisible(measureLabel);
	measureLabel.setText("1/1", juce::dontSendNotification);
	measureLabel.setJustificationType(juce::Justification::centred);

	addAndMakeVisible(currentPlayingMeasureLabel);
	currentPlayingMeasureLabel.setText("M 1", juce::dontSendNotification);
	currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
	currentPlayingMeasureLabel.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDark);
	currentPlayingMeasureLabel.setJustificationType(juce::Justification::centred);
	currentPlayingMeasureLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
}

void SequencerComponent::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	juce::ColourGradient gradient(
		ColourPalette::backgroundDeep, 0, 0,
		ColourPalette::backgroundMid, 0, bounds.getHeight(),
		false);
	g.setGradientFill(gradient);
	g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

	juce::Colour accentColour = ColourPalette::sequencerAccent;
	juce::Colour beatColour = ColourPalette::sequencerBeat;
	juce::Colour subBeatColour = ColourPalette::sequencerSubBeat;

	TrackData* track = audioProcessor.getTrack(trackId);
	if (!track) {
		g.setColour(ColourPalette::textDanger);
		g.drawText("Track not found", getLocalBounds(), juce::Justification::centred);
		return;
	}

	juce::Colour trackColour = ColourPalette::getTrackColour(track->slotIndex);

	int stepsPerBeat = 4;
	int totalSteps = beatsPerMeasure * stepsPerBeat;
	int playingMeasure = track->sequencerData.currentMeasure;
	int safeMeasure = juce::jlimit(0, MAX_MEASURES - 1, currentMeasure);

	for (int i = 0; i < 16; ++i) {
		auto stepBounds = getStepBounds(i);
		bool isVisible = (i < totalSteps);
		bool isStrongBeat = (i % stepsPerBeat == 0);
		bool isBeat = (i % (stepsPerBeat / 2) == 0);

		juce::Colour stepColour;
		juce::Colour borderColour;

		if (!isVisible) {
			stepColour = ColourPalette::backgroundDeep;
			borderColour = ColourPalette::backgroundMid;
		}
		else if (track->sequencerData.steps[safeMeasure][i]) {
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

		if (i == currentStep && isPlaying && isVisible && currentMeasure == playingMeasure) {
			float pulseIntensity = 0.8f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f);
			stepColour = ColourPalette::textPrimary.withAlpha(pulseIntensity);
			borderColour = ColourPalette::textPrimary;
		}

		g.setColour(stepColour);
		g.fillRoundedRectangle(stepBounds.toFloat(), 3.0f);
		g.setColour(borderColour);
		g.drawRoundedRectangle(stepBounds.toFloat(), 3.0f, isVisible ? 1.0f : 0.5f);

		if (isVisible) {
			g.setColour(ColourPalette::textPrimary.withAlpha(isStrongBeat ? 0.9f : 0.6f));
			g.setFont(juce::FontOptions(9.0f, isStrongBeat ? juce::Font::bold : juce::Font::plain));
			g.drawText(juce::String(i + 1), stepBounds, juce::Justification::centred);
		}
	}

	if (isPlaying) {
		auto ledBounds = juce::Rectangle<int>(bounds.getWidth() - 30, 12, 15, 15);
		float pulseIntensity = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.008f);
		juce::Colour ledColour = ColourPalette::playActive.withAlpha(pulseIntensity);

		g.setColour(ledColour);
		g.fillEllipse(ledBounds.toFloat());
		g.setColour(ColourPalette::textPrimary.withAlpha(0.8f));
		g.drawEllipse(ledBounds.toFloat(), 1.0f);
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
	int numerator = audioProcessor.getTimeSignatureNumerator();
	int stepsToShow = numerator * 4;

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
		int safeMeasure = juce::jlimit(0, MAX_MEASURES - 1, currentMeasure);

		track->sequencerData.steps[safeMeasure][step] = !track->sequencerData.steps[safeMeasure][step];
		track->sequencerData.velocities[safeMeasure][step] = 0.8f;
	}

	// TODO: velocity
	if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
		// Shift+click to adjust velocity
	}
}

void SequencerComponent::setCurrentStep(int step)
{
	int numerator = audioProcessor.getTimeSignatureNumerator();
	currentStep = step % (numerator * 4);
	repaint();
}


void SequencerComponent::resized()
{
	auto bounds = getLocalBounds();

	bounds.removeFromTop(10);
	bounds.removeFromLeft(13);

	auto topArea = bounds.removeFromTop(30);
	topArea.removeFromRight(5);

	auto controlArea = topArea.removeFromLeft(230);
	auto pageArea = controlArea.removeFromLeft(120);
	prevMeasureButton.setBounds(pageArea.removeFromLeft(25));
	measureLabel.setBounds(pageArea.removeFromLeft(40));
	nextMeasureButton.setBounds(pageArea.removeFromLeft(25));
	currentPlayingMeasureLabel.setBounds(topArea.removeFromLeft(50));

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
	int oldNumMeasures = numMeasures;
	numMeasures = juce::jlimit(1, MAX_MEASURES, measures);

	if (currentMeasure >= numMeasures) {
		setCurrentMeasure(numMeasures - 1);
	}

	TrackData* track = audioProcessor.getTrack(trackId);
	if (track) {
		track->sequencerData.numMeasures = numMeasures;

		if (numMeasures < oldNumMeasures) {
			for (int m = numMeasures; m < oldNumMeasures; ++m) {
				for (int s = 0; s < 16; ++s) {
					track->sequencerData.steps[m][s] = false;
					track->sequencerData.velocities[m][s] = 0.8f;
				}
			}
		}
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
		currentStep = juce::jlimit(0, 15, track->sequencerData.currentStep);
		isPlaying = track->isCurrentlyPlaying;
		numMeasures = track->sequencerData.numMeasures;

		if (isPlaying) {
			int playingMeasure = track->sequencerData.currentMeasure + 1;
			currentPlayingMeasureLabel.setText("M " + juce::String(playingMeasure),
				juce::dontSendNotification);
			currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::playActive);
		}
		else {
			track->sequencerData.currentStep = 0;
			track->sequencerData.currentMeasure = 0;
			currentPlayingMeasureLabel.setText("M " + juce::String(track->sequencerData.currentMeasure + 1),
				juce::dontSendNotification);
			measureSlider.setValue(track->sequencerData.numMeasures);
			currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
		}
		repaint();
	}
}

