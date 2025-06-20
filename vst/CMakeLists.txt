cmake_minimum_required(VERSION 3.16)
project(OBSIDIAN-Neural VERSION 0.7.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)
add_subdirectory(soundtouch)

set(LLAMA_AVAILABLE FALSE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/llama.cpp/CMakeLists.txt")
    message(STATUS "Setting up llama.cpp manually...")
    
    include(ExternalProject)
    
    ExternalProject_Add(
        llama_cpp_project
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/llama.cpp
        BINARY_DIR ${CMAKE_BINARY_DIR}/llama_cpp_build
        CMAKE_ARGS 
            -DCMAKE_BUILD_TYPE=Release
            -DBUILD_SHARED_LIBS=OFF
            -DLLAMA_BUILD_EXAMPLES=OFF
            -DLLAMA_BUILD_TESTS=OFF
            -DLLAMA_BUILD_SERVER=OFF
            -DLLAMA_CURL=OFF
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/llama_install
        INSTALL_COMMAND ""
        BUILD_ALWAYS FALSE
    )
    
    set(LLAMA_LIB_DIR ${CMAKE_BINARY_DIR}/llama_cpp_build)
    set(LLAMA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/llama.cpp/include)
    set(LLAMA_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/llama.cpp/common)
    set(GGML_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/llama.cpp/ggml/include)
    
    if(WIN32)
        file(GLOB LLAMA_LIBS_LLAMA "${LLAMA_LIB_DIR}/src/Release/*.lib")
        file(GLOB LLAMA_LIBS_GGML "${LLAMA_LIB_DIR}/ggml/src/Release/*.lib") 
        file(GLOB LLAMA_LIBS_COMMON "${LLAMA_LIB_DIR}/common/Release/*.lib")
    
        set(LLAMA_LIBRARIES 
            ${LLAMA_LIBS_LLAMA}
            ${LLAMA_LIBS_GGML} 
            ${LLAMA_LIBS_COMMON}
        )
    
        message(STATUS "Auto-detected llama libraries: ${LLAMA_LIBRARIES}")
    else()
        set(LLAMA_LIBRARIES 
            ${LLAMA_LIB_DIR}/src/libllama.a
            ${LLAMA_LIB_DIR}/ggml/src/libggml.a
        )
    endif()
    
    set(LLAMA_AVAILABLE TRUE)
    message(STATUS "llama.cpp configured manually")
else()
    message(WARNING "llama.cpp not found in deps/llama.cpp")
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

string(TIMESTAMP BUILD_NUMBER "%Y%m%d_%H%M") 
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

juce_add_plugin(ObsidianNeuralVST
    VERSION "1.0.0"
    PLUGIN_MANUFACTURER_CODE "OBSI"
    PLUGIN_CODE "Obsi"
    COMPANY_NAME "InnerMost47"
    BUNDLE_ID "com.innermost47.obsidian-neural-vst" 
    FORMATS VST3
    PRODUCT_NAME "OBSIDIAN-Neural"
    IS_SYNTH TRUE
    WANTS_MIDI_INPUT TRUE
    PRODUCES_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    NEEDS_CURL TRUE
)

target_sources(ObsidianNeuralVST PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/BinaryData.cpp  
    src/PluginEntry.cpp
    src/MidiLearnManager.cpp
    src/MixerChannel.cpp
    src/TrackComponent.cpp
    src/MasterChannel.cpp
    src/WaveformDisplay.cpp
    src/SequencerComponent.cpp
    src/ColourPalette.cpp
    src/MixerPanel.cpp
    src/LlamaEngine.cpp
    src/StableAudioEngine.cpp
)

target_include_directories(ObsidianNeuralVST PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    src
    soundtouch/include
)

if(LLAMA_AVAILABLE)
    target_include_directories(ObsidianNeuralVST PRIVATE
        ${LLAMA_INCLUDE_DIR}
        ${LLAMA_COMMON_INCLUDE_DIR} 
        ${GGML_INCLUDE_DIR}
    )
endif()

target_compile_definitions(ObsidianNeuralVST PRIVATE
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=1
    JucePlugin_IsMidiEffect=0
    JucePlugin_VSTNumMidiInputs=16
    OBSIDIAN_HAS_STABLE_AUDIO=1
    $<$<BOOL:${LLAMA_AVAILABLE}>:OBSIDIAN_HAS_LLAMA=1>
    
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ObsidianNeuralVST PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    SoundTouch
    nlohmann_json::nlohmann_json
    
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(LLAMA_AVAILABLE)
    add_dependencies(ObsidianNeuralVST llama_cpp_project)

    target_link_libraries(ObsidianNeuralVST PRIVATE ${LLAMA_LIBRARIES})
    
    message(STATUS "Will link against: ${LLAMA_LIBRARIES}")
endif()

if(WIN32)
    target_link_libraries(ObsidianNeuralVST PRIVATE ws2_32 winmm)
elseif(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(ACCELERATE_FRAMEWORK)
        target_link_libraries(ObsidianNeuralVST PRIVATE ${ACCELERATE_FRAMEWORK})
    endif()
elseif(UNIX)
    target_link_libraries(ObsidianNeuralVST PRIVATE pthread dl)
endif()

message(STATUS "OBSIDIAN Neural Build Configuration:")
message(STATUS "    LLama.cpp: ${LLAMA_AVAILABLE}")
message(STATUS "    Build Number: ${BUILD_NUMBER}")