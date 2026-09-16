#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Accepts every note on every channel - this is a one-sound synth, no keyboard splits/layers.
class WarfSynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};
