#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace SynthParameterIDs
{
    inline constexpr const char* waveform = "waveform";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* decay = "decay";
    inline constexpr const char* sustain = "sustain";
    inline constexpr const char* release = "release";
    inline constexpr const char* masterVolume = "masterVolume";
}

juce::AudioProcessorValueTreeState::ParameterLayout createSynthParameterLayout();
