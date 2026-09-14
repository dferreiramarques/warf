#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterIDs
{
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* gateThresholdDb = "gateThresholdDb";
    inline constexpr const char* transpose = "transpose";
    inline constexpr const char* midiChannel = "midiChannel";
    inline constexpr const char* fixedVelocity = "fixedVelocity";
    inline constexpr const char* fixedVelocityValue = "fixedVelocityValue";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
