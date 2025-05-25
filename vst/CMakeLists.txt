cmake_minimum_required(VERSION 3.16)
project(DjIaVst VERSION 1.0.0)

# Ajouter JUCE
add_subdirectory(JUCE)

# Configuration du plugin avec defines EXPLICITES
juce_add_plugin(DjIaVst
    VERSION "1.0.0"
    PLUGIN_MANUFACTURER_CODE "DJIA"
    PLUGIN_CODE "Djia"
    FORMATS VST3 Standalone
    PRODUCT_NAME "DJ-IA VST"
    IS_SYNTH TRUE
    WANTS_MIDI_INPUT TRUE
    PRODUCES_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE)

# Ajouter les fichiers sources
target_sources(DjIaVst
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/PluginEntry.cpp)

# Ajouter le dossier src aux chemins d'inclusion
target_include_directories(DjIaVst
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src)

# FORCER LES DEFINES (CRUCIAL !)
target_compile_definitions(DjIaVst
    PRIVATE
        JucePlugin_IsSynth=1                    # INSTRUMENT !
        JucePlugin_WantsMidiInput=1             # MIDI INPUT !
        JucePlugin_ProducesMidiOutput=0         # PAS MIDI OUTPUT
        JucePlugin_IsMidiEffect=0               # PAS EFFET MIDI
        JucePlugin_VSTNumMidiInputs=16          # MIDI INPUTS
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DjIaVst
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)