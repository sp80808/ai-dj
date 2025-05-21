#pragma once

#define JucePlugin_Name                   "DJ-IA VST"
#define JucePlugin_Desc                   "DJ-IA VST Plugin"
#define JucePlugin_Manufacturer           "DJ-IA"
#define JucePlugin_ManufacturerWebsite    ""
#define JucePlugin_ManufacturerEmail      ""
#define JucePlugin_ManufacturerCode       0x4D616E75
#define JucePlugin_PluginCode             0x44656D6F
#define JucePlugin_IsSynth                0
#define JucePlugin_WantsMidiInput         0
#define JucePlugin_ProducesMidiOutput     0
#define JucePlugin_IsMidiEffect           0
#define JucePlugin_EditorRequiresKeyboardFocus  0
#define JucePlugin_VSTUniqueID            JucePlugin_PluginCode
#define JucePlugin_VSTCategory            kPlugCategEffect

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>