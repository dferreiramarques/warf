#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterIDs
{
    inline constexpr const char* trackingMode = "trackingMode";
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* gateThresholdDb = "gateThresholdDb";
    inline constexpr const char* transpose = "transpose";
    inline constexpr const char* midiChannel = "midiChannel";
    inline constexpr const char* fixedVelocity = "fixedVelocity";
    inline constexpr const char* fixedVelocityValue = "fixedVelocityValue";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

namespace TrackingModeState
{
    inline constexpr const char* versionProperty = "trackingModeVersion";
    inline constexpr int currentVersion = 2;
}

// Pre-mode states restore Stable; unversioned Fast=1 restores Percussive=2.
// Existing parameter IDs/properties survive; migration is idempotent.
void migrateTrackingModeState (juce::ValueTree& state);
