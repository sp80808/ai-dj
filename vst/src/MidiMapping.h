/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include <JuceHeader.h>

class DjIaVstProcessor;

struct MidiMapping
{
    int midiType;
    int midiNumber;
    int midiChannel;
    juce::String parameterName;
    juce::String description;
    DjIaVstProcessor *processor;
    std::function<void(float)> uiCallback;
};