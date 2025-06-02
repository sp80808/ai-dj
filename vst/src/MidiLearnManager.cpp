#include <JuceHeader.h>
#include "MidiLearnManager.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    startTimerHz(10);
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
        juce::MessageManager::callAsync([this]()
                                        {
            if (auto* editor = dynamic_cast<DjIaVstEditor*>(learningProcessor->getActiveEditor()))
            {
                editor->statusLabel.setText("MIDI Learn timeout - no controller received", juce::dontSendNotification);
                juce::Timer::callAfterDelay(2000, [this]() {
                    if (auto* editor = dynamic_cast<DjIaVstEditor*>(learningProcessor->getActiveEditor())) {
                        editor->statusLabel.setText("Ready", juce::dontSendNotification);
                    }
                });
            } });

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
    DBG("MIDI received: " + message.getDescription());
    int midiType = -1;
    int midiNumber = 0;
    int midiChannel = message.getChannel() - 1;

    if (message.isController())
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

    juce::String fullMessage = "MIDI mapping created: " + midiDescription + " → " + learningDescription;
    DBG(fullMessage);
    juce::MessageManager::callAsync([mapping, fullMessage]()
                                    {
                    if (auto* editor = dynamic_cast<DjIaVstEditor*>(mapping.processor->getActiveEditor()))
                    {
                        editor->statusLabel.setText(fullMessage, juce::dontSendNotification);
                        juce::Timer::callAfterDelay(2000, [mapping]() {
                            if (auto* editor = dynamic_cast<DjIaVstEditor*>(mapping.processor->getActiveEditor())) {
                                editor->statusLabel.setText("Ready", juce::dontSendNotification);
                            }
                        });
                    } });

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
        juce::String statusMessage = "";

        if (mapping.midiType == 0 && message.isNoteOnOrOff() && mapping.midiChannel == midiChannel)
        {
            if (message.getNoteNumber() == mapping.midiNumber)
            {
                matches = true;
                statusMessage = "Note " + juce::String(mapping.midiNumber) + " → " + mapping.parameterName;

                if (message.isNoteOn() && isBooleanParameter(mapping.parameterName))
                {
                    auto *param = mapping.processor->getParameterTreeState().getParameter(mapping.parameterName);
                    if (param)
                    {
                        float currentValue = param->getValue();
                        value = (currentValue > 0.5f) ? 0.0f : 1.0f;
                        statusMessage += " (toggle: " + juce::String(value > 0.5f ? "ON" : "OFF") + ")";
                    }
                }
                else if (message.isNoteOn())
                {
                    value = message.getVelocity() / 127.0f;
                    statusMessage += " (vel: " + juce::String(message.getVelocity()) + ")";
                }
                else
                {
                    if (isBooleanParameter(mapping.parameterName))
                        mustCheckForMidiEvent.store(true);
                    continue;
                }
            }
        }
        else if (mapping.midiType == 1 && message.isController() && mapping.midiChannel == midiChannel)
        {
            if (message.getControllerNumber() == mapping.midiNumber)
            {
                matches = true;
                value = message.getControllerValue() / 127.0f;
                statusMessage = "CC" + juce::String(mapping.midiNumber) + " → " + mapping.parameterName +
                                " (" + juce::String(message.getControllerValue()) + ")";
            }
        }
        else if (mapping.midiType == 2 && message.isPitchWheel() && mapping.midiChannel == midiChannel)
        {
            matches = true;
            value = (message.getPitchWheelValue() + 8192) / 16383.0f;
            statusMessage = "Pitch Wheel → " + mapping.parameterName +
                            " (" + juce::String(message.getPitchWheelValue()) + ")";
        }

        if (matches && mapping.processor)
        {
            auto *param = mapping.processor->getParameterTreeState().getParameter(mapping.parameterName);
            if (param)
            {
                param->setValueNotifyingHost(value);
                juce::MessageManager::callAsync([mapping, statusMessage]()
                                                {
                    if (auto* editor = dynamic_cast<DjIaVstEditor*>(mapping.processor->getActiveEditor()))
                    {
                        editor->statusLabel.setText(statusMessage, juce::dontSendNotification);
                        juce::Timer::callAfterDelay(2000, [mapping]() {
                            if (auto* editor = dynamic_cast<DjIaVstEditor*>(mapping.processor->getActiveEditor())) {
                                editor->statusLabel.setText("Ready", juce::dontSendNotification);
                            }
                        });
                    } });

                if (mapping.parameterName.contains("slot") && mapping.parameterName.contains("Play"))
                {
                    juce::String slotStr = mapping.parameterName.substring(4, 5);
                    int slotNumber = slotStr.getIntValue();
                    if (slotNumber >= 1 && slotNumber <= 8)
                    {
                        changedSlotIndex.store(slotNumber - 1);
                        mustCheckForMidiEvent.store(true);
                    }
                }
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

bool MidiLearnManager::removeMappingForParameter(const juce::String &parameterName)
{
    auto mappingIt = std::find_if(mappings.begin(), mappings.end(),
                                  [parameterName](const MidiMapping &mapping)
                                  {
                                      return mapping.parameterName == parameterName;
                                  });

    if (mappingIt == mappings.end())
    {
        return false;
    }

    DjIaVstProcessor *processor = mappingIt->processor;
    juce::String description = mappingIt->description;

    mappings.erase(mappingIt);
    juce::String statusMessage = "MIDI mapping removed: " + description;
    DBG(statusMessage);
    juce::MessageManager::callAsync([processor, statusMessage]()
                                    {
        if (auto* editor = dynamic_cast<DjIaVstEditor*>(processor->getActiveEditor()))
        {
            editor->statusLabel.setText(statusMessage, juce::dontSendNotification);
            juce::Timer::callAfterDelay(2000, [processor]() {
                if (auto* editor = dynamic_cast<DjIaVstEditor*>(processor->getActiveEditor())) {
                    editor->statusLabel.setText("Ready", juce::dontSendNotification);
                }
            });
        } });

    return true;
}

bool MidiLearnManager::hasMappingForParameter(const juce::String &parameterName) const
{
    return std::any_of(mappings.begin(), mappings.end(),
                       [parameterName](const MidiMapping &mapping)
                       {
                           return mapping.parameterName == parameterName;
                       });
}

juce::String MidiLearnManager::getMappingDescription(const juce::String &parameterName) const
{
    auto it = std::find_if(mappings.begin(), mappings.end(),
                           [parameterName](const MidiMapping &mapping)
                           {
                               return mapping.parameterName == parameterName;
                           });

    if (it != mappings.end())
    {
        juce::String midiDescription;
        switch (it->midiType)
        {
        case 0:
            midiDescription = "Note " + juce::MidiMessage::getMidiNoteName(it->midiNumber, true, true, 3);
            break;
        case 1:
            midiDescription = "CC " + juce::String(it->midiNumber);
            break;
        case 2:
            midiDescription = "Pitchbend";
            break;
        }
        return midiDescription + " (Ch." + juce::String(it->midiChannel + 1) + ")";
    }

    return juce::String();
}