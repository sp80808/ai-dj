#pragma once
#include "JuceHeader.h"

class MasterChannel : public juce::Component
{
public:
	MasterChannel()
	{
		setupUI();
	}

	void setupUI()
	{
		addAndMakeVisible(masterVolumeSlider);
		masterVolumeSlider.setRange(0.0, 1.0, 0.01);
		masterVolumeSlider.setValue(0.8);
		masterVolumeSlider.setSliderStyle(juce::Slider::LinearVertical);
		masterVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		masterVolumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff6600));
		masterVolumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));

		addAndMakeVisible(masterPanKnob);
		masterPanKnob.setRange(-1.0, 1.0, 0.01);
		masterPanKnob.setValue(0.0);
		masterPanKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
		masterPanKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		masterPanKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

		addAndMakeVisible(highKnob);
		highKnob.setRange(-12.0, 12.0, 0.1);
		highKnob.setValue(0.0);
		highKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
		highKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		highKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

		addAndMakeVisible(midKnob);
		midKnob.setRange(-12.0, 12.0, 0.1);
		midKnob.setValue(0.0);
		midKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
		midKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		midKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

		addAndMakeVisible(lowKnob);
		lowKnob.setRange(-12.0, 12.0, 0.1);
		lowKnob.setValue(0.0);
		lowKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
		lowKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		lowKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff6600));

		addAndMakeVisible(masterLabel);
		masterLabel.setText("MASTER", juce::dontSendNotification);
		masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
		masterLabel.setJustificationType(juce::Justification::centred);
		masterLabel.setFont(juce::Font(14.0f, juce::Font::bold));

		addAndMakeVisible(highLabel);
		highLabel.setText("HIGH", juce::dontSendNotification);
		highLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
		highLabel.setJustificationType(juce::Justification::centred);
		highLabel.setFont(juce::Font(9.0f));

		addAndMakeVisible(midLabel);
		midLabel.setText("MID", juce::dontSendNotification);
		midLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
		midLabel.setJustificationType(juce::Justification::centred);
		midLabel.setFont(juce::Font(9.0f));

		addAndMakeVisible(lowLabel);
		lowLabel.setText("LOW", juce::dontSendNotification);
		lowLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
		lowLabel.setJustificationType(juce::Justification::centred);
		lowLabel.setFont(juce::Font(9.0f));

		addAndMakeVisible(panLabel);
		panLabel.setText("PAN", juce::dontSendNotification);
		panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
		panLabel.setJustificationType(juce::Justification::centred);
		panLabel.setFont(juce::Font(9.0f));

		masterVolumeSlider.onValueChange = [this]()
			{
				if (onMasterVolumeChanged)
				{
					onMasterVolumeChanged(masterVolumeSlider.getValue());
				}
			};

		highKnob.onValueChange = [this]()
			{
				if (onMasterEQChanged)
				{
					onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
				}
			};

		midKnob.onValueChange = [this]()
			{
				if (onMasterEQChanged)
				{
					onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
				}
			};

		lowKnob.onValueChange = [this]()
			{
				if (onMasterEQChanged)
				{
					onMasterEQChanged(highKnob.getValue(), midKnob.getValue(), lowKnob.getValue());
				}
			};

		masterPanKnob.onValueChange = [this]()
			{
				if (onMasterPanChanged)
				{
					onMasterPanChanged(masterPanKnob.getValue());
				}
			};
	}



	void paint(juce::Graphics& g) override
	{
		auto bounds = getLocalBounds();

		g.setColour(juce::Colour(0xff3a2a1a));
		g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

		g.setColour(juce::Colour(0xffff6600));
		g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 2.0f);

		drawMasterVUMeter(g, bounds);
	}

	void resized() override
	{
		auto area = getLocalBounds().reduced(4);
		int width = area.getWidth();

		masterLabel.setBounds(area.removeFromTop(20));
		area.removeFromTop(10);

		auto eqArea = area.removeFromTop(100);
		int knobSize = 35;

		auto highRow = eqArea.removeFromTop(50);
		highLabel.setBounds((width - 50) / 2, highRow.getY(), 50, 12);
		highKnob.setBounds((width - knobSize) / 2, highRow.getY() + 15, knobSize, knobSize);

		eqArea.removeFromTop(5);

		auto bottomRow = eqArea.removeFromTop(50);
		int spacing = width / 4;

		midLabel.setBounds(spacing - 25, bottomRow.getY(), 50, 12);
		midKnob.setBounds(spacing - knobSize / 2, bottomRow.getY() + 15, knobSize, knobSize);

		lowLabel.setBounds(width - spacing - 25, bottomRow.getY(), 50, 12);
		lowKnob.setBounds(width - spacing - knobSize / 2, bottomRow.getY() + 15, knobSize, knobSize);

		auto volumeArea = area.removeFromTop(300);
		int faderWidth = width / 3;
		int centerX = (width - faderWidth) / 2;
		masterVolumeSlider.setBounds(centerX, volumeArea.getY() + 5, faderWidth, volumeArea.getHeight() - 10);

		area.removeFromTop(5);

		auto panArea = area.removeFromTop(60);
		auto vuZone = panArea.removeFromRight(10);
		auto knobZone = panArea;
		panLabel.setBounds(knobZone.removeFromTop(12));
		masterPanKnob.setBounds(knobZone.reduced(2));
	}

	void drawMasterVUMeter(juce::Graphics& g, juce::Rectangle<int> bounds) const
	{
		auto vuArea = juce::Rectangle<float>(bounds.getWidth() - 15, 40, 10, bounds.getHeight() - 80);

		g.setColour(juce::Colour(0xff0a0a0a));
		g.fillRoundedRectangle(vuArea, 2.0f);

		g.setColour(juce::Colour(0xffff6600));
		g.drawRoundedRectangle(vuArea, 2.0f, 1.0f);

		int numSegments = 25;
		float segmentHeight = (vuArea.getHeight() - 4) / numSegments;

		for (int i = 0; i < numSegments; ++i)
		{
			drawMasterChanelSegments(vuArea, i, segmentHeight, numSegments, g);
		}

		if (masterPeakHold > 0.0f)
		{
			drawPeakHoldLine(numSegments, vuArea, segmentHeight, g);
		}

		if (masterPeakHold >= 0.98f)
		{
			drawMasterClipping(vuArea, g);
		}
	}

	void drawPeakHoldLine(int numSegments, juce::Rectangle<float>& vuArea, float segmentHeight, juce::Graphics& g) const
	{
		int peakSegment = (int)(masterPeakHold * numSegments);
		if (peakSegment < numSegments)
		{
			float peakY = vuArea.getBottom() - 2 - (peakSegment + 1) * segmentHeight;
			juce::Rectangle<float> peakRect(vuArea.getX() + 1, peakY, vuArea.getWidth() - 2, 3);

			g.setColour(juce::Colours::white);
			g.fillRect(peakRect);
		}
	}

	void drawMasterClipping(juce::Rectangle<float>& vuArea, juce::Graphics& g) const
	{
		auto clipRect = juce::Rectangle<float>(vuArea.getX() - 2, vuArea.getY() - 12, vuArea.getWidth() + 4, 8);
		g.setColour(isClipping && (juce::Time::getCurrentTime().toMilliseconds() % 500 < 250)
			? juce::Colours::red
			: juce::Colours::darkred);
		g.fillRoundedRectangle(clipRect, 4.0f);

		g.setColour(juce::Colours::white);
		g.setFont(juce::Font(8.0f, juce::Font::bold));
		g.drawText("CLIP", clipRect, juce::Justification::centred);
	}

	void drawMasterChanelSegments(juce::Rectangle<float>& vuArea, int i, float segmentHeight, int numSegments, juce::Graphics& g) const
	{
		float segmentY = vuArea.getBottom() - 2 - (i + 1) * segmentHeight;
		float segmentLevel = (float)i / numSegments;

		juce::Rectangle<float> segmentRect(
			vuArea.getX() + 1, segmentY, vuArea.getWidth() - 2, segmentHeight - 1);

		juce::Colour segmentColour;
		if (segmentLevel < 0.6f)
			segmentColour = juce::Colours::green;
		else if (segmentLevel < 0.8f)
			segmentColour = juce::Colours::orange;
		else if (segmentLevel < 0.95f)
			segmentColour = juce::Colours::red;
		else
			segmentColour = juce::Colours::white;

		if (masterLevel >= segmentLevel)
		{
			g.setColour(segmentColour);
			g.fillRoundedRectangle(segmentRect, 1.0f);
		}
		else
		{
			g.setColour(segmentColour.withAlpha(0.05f));
			g.fillRoundedRectangle(segmentRect, 1.0f);
		}
	}

	void setRealAudioLevel(float level)
	{
		realAudioLevel = juce::jlimit(0.0f, 1.0f, level);
		hasRealAudio = true;
	}

	void updateMasterLevels()
	{
		float instantLevel;

		if (hasRealAudio)
		{
			instantLevel = realAudioLevel;
		}
		else
		{
			static float phase = 0.0f;
			phase += 0.05f;
			instantLevel = (std::sin(phase) * 0.3f + 0.3f) * 0.5f;
		}

		if (instantLevel > masterLevel)
		{
			masterLevel = masterLevel * 0.7f + instantLevel * 0.3f;
		}
		else
		{
			masterLevel = masterLevel * 0.95f + instantLevel * 0.05f;
		}

		if (masterLevel > masterPeakHold)
		{
			masterPeakHold = masterLevel;
			masterPeakHoldTimer = 60;
		}
		else if (masterPeakHoldTimer > 0)
		{
			masterPeakHoldTimer--;
		}
		else
		{
			masterPeakHold *= 0.98f;
		}

		isClipping = (masterPeakHold >= 0.95f);

		repaint();
	}

	std::function<void(float)> onMasterVolumeChanged;
	std::function<void(float)> onMasterPanChanged;
	std::function<void(float, float, float)> onMasterEQChanged;

private:
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

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterChannel)
};
