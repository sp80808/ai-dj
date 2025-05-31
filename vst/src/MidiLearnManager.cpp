#include <JuceHeader.h>
#include "MidiLearnManager.h"

MidiLearnManager::MidiLearnManager()
{
}

MidiLearnManager::~MidiLearnManager()
{
    stopLearning();
}

void MidiLearnManager::startLearning(juce::Component *component, std::function<void(float)> callback, const juce::String &description)
{
    stopLearning();

    learningComponent = component;
    learningCallback = callback;
    learningDescription = description;
    isLearning = true;
    learnStartTime = juce::Time::currentTimeMillis();

    if (learningComponent)
    {
        juce::MessageManager::callAsync([this, component]()
                                        {
            if (auto *button = dynamic_cast<juce::Button *>(component))
            {
                originalColourId = juce::TextButton::buttonColourId;
                originalColour = button->findColour(originalColourId);
            }
            else if (auto *slider = dynamic_cast<juce::Slider *>(component))
            {
                originalColourId = juce::Slider::thumbColourId;
                originalColour = slider->findColour(originalColourId);
            } });

        startTimer(300);
    }

    DBG("MIDI Learn started for: " + description);
}

void MidiLearnManager::stopLearning()
{
    if (!isLearning)
        return;

    isLearning = false;
    stopTimer();

    if (learningComponent)
    {
        juce::MessageManager::callAsync([this, component = learningComponent,
                                         originalColour = this->originalColour,
                                         originalColourId = this->originalColourId]()
                                        {
            if (component != nullptr)
            {
                if (auto* button = dynamic_cast<juce::Button*>(component))
                {
                    button->setColour(originalColourId, originalColour);
                }
                else if (auto* slider = dynamic_cast<juce::Slider*>(component))
                {
                    slider->setColour(originalColourId, originalColour);
                }
                
                component->repaint();
            } });
    }

    learningComponent = nullptr;
    learningCallback = nullptr;
    learningDescription.clear();

    DBG("MIDI Learn stopped");
}

void MidiLearnManager::timerCallback()
{
    if (juce::Time::currentTimeMillis() - learnStartTime > LEARN_TIMEOUT_MS)
    {
        DBG("MIDI Learn timeout");
        stopLearning();
        return;
    }

    if (learningComponent)
    {
        flashState = !flashState;
        juce::Colour flashColour = flashState ? juce::Colours::red : juce::Colours::orange;

        if (auto *button = dynamic_cast<juce::Button *>(learningComponent))
        {
            button->setColour(originalColourId, flashColour);
        }
        else if (auto *slider = dynamic_cast<juce::Slider *>(learningComponent))
        {
            slider->setColour(originalColourId, flashColour);
        }

        learningComponent->repaint();
    }
}

bool MidiLearnManager::processMidiForLearning(const juce::MidiMessage &message)
{
    if (!isLearning || !learningComponent)
    {
        return false;
    }

    int midiType = -1;
    int midiNumber = 0;
    int midiChannel = message.getChannel() - 1;

    if (message.isNoteOn())
    {
        midiType = 0;
        midiNumber = message.getNoteNumber();
    }
    else if (message.isController())
    {
        midiType = 1;
        midiNumber = message.getControllerNumber();
    }
    else if (message.isPitchWheel())
    {
        midiType = 2;
        midiNumber = 0;
    }
    else
    {
        return false;
    }

    removeMapping(learningComponent);

    MidiMapping mapping;
    mapping.midiType = midiType;
    mapping.midiNumber = midiNumber;
    mapping.midiChannel = midiChannel;
    mapping.targetComponent = learningComponent;
    mapping.callback = learningCallback;
    mapping.description = learningDescription;

    mappings.push_back(mapping);

    juce::String midiDescription;
    switch (midiType)
    {
    case 0:
        midiDescription = "Note " + juce::MidiMessage::getMidiNoteName(midiNumber, true, true, 3);
        break;
    case 1:
        midiDescription = "CC " + juce::String(midiNumber);
        break;
    case 2:
        midiDescription = "Pitchbend";
        break;
    }

    DBG("MIDI mapping created: " + midiDescription + " → " + learningDescription);

    stopLearning();

    return true;
}

void MidiLearnManager::processMidiMappings(const juce::MidiMessage &message)
{
    int midiChannel = message.getChannel() - 1;

    for (auto &mapping : mappings)
    {
        bool matches = false;
        float value = 0.0f;

        if (mapping.midiType == 0 && message.isNoteOnOrOff() && mapping.midiChannel == midiChannel)
        {
            if (message.getNoteNumber() == mapping.midiNumber)
            {
                matches = true;
                value = message.isNoteOn() ? (message.getVelocity() / 127.0f) : 0.0f;
            }
        }
        else if (mapping.midiType == 1 && message.isController() && mapping.midiChannel == midiChannel)
        {
            if (message.getControllerNumber() == mapping.midiNumber)
            {
                matches = true;
                value = message.getControllerValue() / 127.0f;
            }
        }
        else if (mapping.midiType == 2 && message.isPitchWheel() && mapping.midiChannel == midiChannel)
        {
            matches = true;
            value = (message.getPitchWheelValue() + 8192) / 16383.0f;
        }

        if (matches && mapping.targetComponent && mapping.callback)
        {
            if (mapping.targetComponent != nullptr)
            {
                juce::MessageManager::callAsync([callback = mapping.callback, value]()
                                                { callback(value); });
            }
            else
            {
                removeMapping(mapping.targetComponent);
            }
        }
    }
}

void MidiLearnManager::removeMapping(juce::Component *component)
{
    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
                       [component](const MidiMapping &mapping)
                       {
                           return mapping.targetComponent == component;
                       }),
        mappings.end());
}

void MidiLearnManager::clearAllMappings()
{
    mappings.clear();
    DBG("All MIDI mappings cleared");
}

void MidiLearnManager::recreateMapping(int midiType, int midiNumber, int midiChannel,
                                       juce::Component *component, std::function<void(float)> callback,
                                       const juce::String &description)
{
    MidiMapping mapping;
    mapping.midiType = midiType;
    mapping.midiNumber = midiNumber;
    mapping.midiChannel = midiChannel;
    mapping.targetComponent = component;
    mapping.callback = callback;
    mapping.description = description;

    mappings.push_back(mapping);

    DBG("MIDI mapping recreated: " + description);
}