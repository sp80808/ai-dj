#include <JuceHeader.h>
#include "MidiLearnManager.h"
#include "PluginProcessor.h"

MidiLearnManager::MidiLearnManager()
{
}

MidiLearnManager::~MidiLearnManager()
{
    stopLearning();
}

void MidiLearnManager::startLearning(const juce::String &parameterName,
                                     DjIaVstProcessor *processor,
                                     std::function<void(float)> uiCallback,
                                     const juce::String &description)
{
    stopLearning();
    learningParameter = parameterName;
    learningProcessor = processor;
    learningUiCallback = uiCallback;
    learningDescription = description;
    isLearning = true;
    learnStartTime = juce::Time::currentTimeMillis();
    DBG("MIDI Learn started for parameter: " + parameterName);
}

void MidiLearnManager::stopLearning()
{
    if (!isLearning)
        return;

    isLearning = false;
    stopTimer();
    learningUiCallback = nullptr;
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
}

bool MidiLearnManager::processMidiForLearning(const juce::MidiMessage &message)
{
    if (!isLearning)
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

    removeMapping(learningParameter);

    MidiMapping mapping;
    mapping.midiType = midiType;
    mapping.midiNumber = midiNumber;
    mapping.midiChannel = midiChannel;
    mapping.processor = learningProcessor;
    mapping.uiCallback = learningUiCallback;
    mapping.description = learningDescription;
    mapping.parameterName = learningParameter;

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

                if (message.isNoteOn() && isBooleanParameter(mapping.parameterName))
                {
                    auto *param = mapping.processor->getParameterTreeState().getParameter(mapping.parameterName);
                    if (param)
                    {
                        float currentValue = param->getValue();
                        value = (currentValue > 0.5f) ? 0.0f : 1.0f;
                    }
                }
                else if (message.isNoteOn())
                {
                    value = message.getVelocity() / 127.0f;
                }
                else
                {
                    if (isBooleanParameter(mapping.parameterName))
                        continue;
                    value = 0.0f;
                }
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

        if (matches && mapping.processor)
        {
            auto *param = mapping.processor->getParameterTreeState().getParameter(mapping.parameterName);

            if (param)
            {
                param->setValueNotifyingHost(value);
            }
        }
    }
}

bool MidiLearnManager::isBooleanParameter(const juce::String &parameterName)
{
    return parameterName.contains("Play") ||
           parameterName.contains("Stop") ||
           parameterName.contains("Mute") ||
           parameterName.contains("Solo") ||
           parameterName.contains("Generate");
}

void MidiLearnManager::clearUICallbacks()
{
    registeredUICallbacks.clear();
    for (auto &mapping : mappings)
    {
        mapping.uiCallback = nullptr;
    }
    DBG("UI callbacks cleared");
}

void MidiLearnManager::registerUICallback(const juce::String &parameterName,
                                          std::function<void(float)> callback)
{
    registeredUICallbacks[parameterName] = callback;
}

void MidiLearnManager::restoreUICallbacks()
{
    for (auto &mapping : mappings)
    {
        auto it = registeredUICallbacks.find(mapping.parameterName);
        if (it != registeredUICallbacks.end())
        {
            mapping.uiCallback = it->second;
        }
    }
}

void MidiLearnManager::addMapping(const MidiMapping &midiMapping)
{
    mappings.push_back(midiMapping);
}

void MidiLearnManager::removeMapping(juce::String parameterName)
{
    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
                       [parameterName](const MidiMapping &mapping)
                       {
                           return mapping.parameterName == parameterName;
                       }),
        mappings.end());
}

void MidiLearnManager::clearAllMappings()
{
    mappings.clear();
    DBG("All MIDI mappings cleared");
}
