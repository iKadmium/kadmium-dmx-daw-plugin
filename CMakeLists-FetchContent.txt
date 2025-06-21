# Alternative CMakeLists.txt using FetchContent (auto-downloads JUCE)
cmake_minimum_required(VERSION 3.22)

project(KadmiumDMXPlugin VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent to download JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Add our plugin target
juce_add_plugin(KadmiumDMXPlugin
    COMPANY_NAME "Kadmium"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Kadm
    PLUGIN_CODE KdmX
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Kadmium DMX Plugin"
)

# Source files
target_sources(KadmiumDMXPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Link JUCE modules
target_link_libraries(KadmiumDMXPlugin PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Compiler settings
target_compile_definitions(KadmiumDMXPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
