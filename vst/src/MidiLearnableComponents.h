/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include <JuceHeader.h>

template <typename ComponentType>
class MidiLearnable : public ComponentType
{
public:
	std::function<void()> onMidiLearn;
	std::function<void()> onMidiRemove;

	void mouseDown(const juce::MouseEvent &e) override
	{
		if (e.mods.isRightButtonDown() && !e.mods.isCtrlDown())
		{
			juce::PopupMenu menu;
			menu.addItem(1, "MIDI Learn");
			menu.addItem(2, "Remove MIDI", onMidiRemove != nullptr);

			menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
							   {
					if (result == 1 && onMidiLearn)
						onMidiLearn();
					else if (result == 2 && onMidiRemove)
						onMidiRemove(); });
		}
		else
		{
			ComponentType::mouseDown(e);
		}
	}
};

using MidiLearnableButton = MidiLearnable<juce::TextButton>;
using MidiLearnableSlider = MidiLearnable<juce::Slider>;
using MidiLearnableComboBox = MidiLearnable<juce::ComboBox>;
using MidiLearnableToggleButton = MidiLearnable<juce::ToggleButton>;