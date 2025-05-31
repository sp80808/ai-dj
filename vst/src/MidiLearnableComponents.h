#pragma once
#include <JuceHeader.h>

template <typename ComponentType>
class MidiLearnable : public ComponentType
{
public:
    std::function<void()> onMidiLearn;

    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "MIDI Learn");

            menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
                               {
                if (result == 1 && onMidiLearn) {
                    onMidiLearn();
                } });
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