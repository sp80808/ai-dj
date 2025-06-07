// OBSIDIAN-Neural - Neural Sound Engine - Original Author: InnerMost47
// This attribution should remain in derivative works

#include "PluginProcessor.h"

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DjIaVstProcessor();
}