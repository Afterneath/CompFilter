cmake_minimum_required(VERSION 3.15)

project(CompFilter VERSION 0.0.1)

# Point this to your JUCE installation
add_subdirectory(JUCE) 

juce_add_plugin(CompFilter
    COMPANY_NAME "Etherealmess"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_CODE "CpFl"
    # Enable VST3
    FORMATS VST3 Standalone
    PRODUCT_NAME "CompFilter"
    PLUGIN_NAME "CompFilter"
)

# Disable the VST2 compatibility check error
target_compile_definitions(CompFilter PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)
#added to prevent some compilation error?
juce_generate_juce_header(CompFilter)

target_sources(CompFilter
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(CompFilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(CompFilter PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
)
