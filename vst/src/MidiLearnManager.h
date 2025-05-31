#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>

class MidiLearnManager : public juce::Timer
{
public:
    struct MidiMapping
    {
        int midiType;
        int midiNumber;
        int midiChannel;
        juce::Component *targetComponent;
        std::function<void(float)> callback;
        juce::String description;
    };

    MidiLearnManager();
    ~MidiLearnManager();

    void startLearning(juce::Component *component, std::function<void(float)> callback, const juce::String &description = "");
    void recreateMapping(int midiType, int midiNumber, int midiChannel,
                         juce::Component *component, std::function<void(float)> callback,
                         const juce::String &description);
    void stopLearning();
    bool processMidiForLearning(const juce::MidiMessage &message);
    void processMidiMappings(const juce::MidiMessage &message);
    void removeMapping(juce::Component *component);
    void clearAllMappings();
    std::vector<MidiMapping> getAllMappings() const { return mappings; }
    bool isLearningActive() const { return isLearning; }
    juce::Component *getLearningComponent() const { return learningComponent; }

private:
    void timerCallback() override;
    bool isLearning = false;
    juce::Component *learningComponent = nullptr;
    std::function<void(float)> learningCallback;
    juce::String learningDescription;
    std::vector<MidiMapping> mappings;
    bool flashState = false;
    juce::Colour originalColour;
    int originalColourId = 0;
    static constexpr int LEARN_TIMEOUT_MS = 10000;
    juce::int64 learnStartTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};