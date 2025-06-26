/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#include "JuceHeader.h"

struct DummySound : public juce::SynthesiserSound
{
	bool appliesToNote(int) override { return true; }
	bool appliesToChannel(int) override { return true; }
};

class DummyVoice : public juce::SynthesiserVoice
{
public:
	bool canPlaySound(juce::SynthesiserSound *) override { return true; }

	void startNote(int /*midiNoteNumber*/, float /*velocity*/,
				   juce::SynthesiserSound *, int /*currentPitchWheelPosition*/) override
	{
	}

	void stopNote(float /*velocity*/, bool /*allowTailOff*/) override
	{
		clearCurrentNote();
	}

	void pitchWheelMoved(int /*newPitchWheelValue*/) override {}
	void controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) override {}

	void renderNextBlock(juce::AudioBuffer<float> & /*outputBuffer*/,
						 int /*startSample*/, int /*numSamples*/) override
	{
	}
};