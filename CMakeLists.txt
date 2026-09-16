cmake_minimum_required(VERSION 3.22)

project(WarfPlugin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15 # matches the version Kerf Lite (the sibling plugin this project's setup was ported from) uses
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Warf
    COMPANY_NAME "Monco"
    PRODUCT_NAME "Warf (Beta)"
    PLUGIN_NAME "Warf (Beta)"
    DESCRIPTION "Free monophonic audio-to-MIDI converter plugin"
    PLUGIN_MANUFACTURER_CODE Monc
    PLUGIN_CODE Wrf1
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    # Load-bearing, don't set FALSE again: a real user's actual workflow depends on Studio One's
    # own "Instrument Input" track routing (pointing another Instrument track's input at Warf's
    # track) to get Warf's MIDI to a synth, and that only works when this bus is declared - true
    # regardless of VST3_CATEGORIES being Fx here, contrary to an earlier assumption in this file's
    # history. A previous attempt to fix a "phantom piano" report by setting this FALSE broke that
    # routing entirely - the phantom piano needs a different fix; see PluginProcessor's own
    # MIDI Output Device picker and README's "read this before building" note for the live
    # understanding of where that sound is actually coming from.
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Analyzer Tools
)

target_sources(Warf PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ParameterLayout.cpp
    src/PitchDetector.cpp
    src/NoteTracker.cpp
)

target_compile_definitions(Warf PUBLIC
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Warf
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Warf Synth: a minimal companion instrument (plain MIDI-in/audio-out, no audio input bus) meant
# to be loaded on whichever track receives Warf's MIDI output, so there's something to actually
# hear without needing a third-party synth. Completely independent of Warf's own engine.
juce_add_plugin(WarfSynth
    COMPANY_NAME "Monco"
    PRODUCT_NAME "Warf Synth (Beta)"
    PLUGIN_NAME "Warf Synth (Beta)"
    DESCRIPTION "Minimal companion synth for playing Warf's MIDI output"
    PLUGIN_MANUFACTURER_CODE Monc
    PLUGIN_CODE Wrf2
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
)

target_sources(WarfSynth PRIVATE
    synth-src/SynthPluginProcessor.cpp
    synth-src/SynthPluginEditor.cpp
    synth-src/SynthParameterLayout.cpp
    synth-src/SynthVoice.cpp
)

target_compile_definitions(WarfSynth PUBLIC
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(WarfSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Console-only test runner for the engine's juce::UnitTest suite (PitchDetector/NoteTracker/
# SynthVoice) - no DAW or plugin host involved. `cmake --build build --target WarfTests` then run
# the resulting exe. See tests/Main.cpp.
juce_add_console_app(WarfTests
    PRODUCT_NAME "WarfTests"
)

target_sources(WarfTests PRIVATE
    tests/Main.cpp
    tests/PitchDetectorTests.cpp
    tests/NoteTrackerTests.cpp
    tests/SynthVoiceTests.cpp
    src/PitchDetector.cpp
    src/NoteTracker.cpp
    src/ParameterLayout.cpp
    synth-src/SynthVoice.cpp
)

target_compile_definitions(WarfTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(WarfTests
    PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
