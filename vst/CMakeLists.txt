cmake_minimum_required(VERSION 3.16)
project(DjIaVst VERSION 1.0.0)

# Ajouter JUCE
add_subdirectory(JUCE)

# Configuration du plugin
juce_add_plugin(DjIaVst
    VERSION "1.0.0"
    PLUGIN_MANUFACTURER_CODE "DJIA"
    PLUGIN_CODE "Djia"
    FORMATS VST3 Standalone 
    PRODUCT_NAME "DJ-IA VST")

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

# Configurer les modules JUCE nécessaires
target_compile_definitions(DjIaVst
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DjIaVst
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)