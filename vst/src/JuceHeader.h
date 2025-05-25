#pragma once

// Laisser CMake définir les flags principaux (IsSynth, WantsMidiInput, etc.)
// Mais définir les infos basiques

#define JucePlugin_Name "DJ-IA VST"
#define JucePlugin_Desc "AI Loop Generator Instrument"
#define JucePlugin_Manufacturer "Anthony"
#define JucePlugin_ManufacturerCode 0x4D616E75
#define JucePlugin_PluginCode 0x44656D80
#define JucePlugin_VSTUniqueID JucePlugin_PluginCode
#define JucePlugin_Vst3Category "Instrument"

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