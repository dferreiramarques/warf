#pragma once

#include <cmath>

// Plain frequency<->MIDI-note math, deliberately free of any JUCE dependency so it can be reused
// verbatim from both the plugin and the console test target without pulling in juce_core.
namespace MidiNoteUtils
{
    constexpr double a4FrequencyHz = 440.0;
    constexpr int a4MidiNote = 69;

    // Exact (fractional) MIDI note number for a frequency - e.g. 466.16 Hz (A#4) returns ~70.0.
    inline double midiNoteFromFrequency (double frequencyHz)
    {
        return (double) a4MidiNote + 12.0 * std::log2 (frequencyHz / a4FrequencyHz);
    }

    inline double frequencyFromMidiNote (double midiNote)
    {
        return a4FrequencyHz * std::pow (2.0, (midiNote - (double) a4MidiNote) / 12.0);
    }
}
