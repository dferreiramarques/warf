#include "SynthVoice.h"
#include "SynthSound.h"
#include <cmath>

bool WarfSynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<WarfSynthSound*> (sound) != nullptr;
}

void WarfSynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    level = velocity;
    currentAngle = 0.0;
    const auto freqHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    angleDelta = freqHz * 2.0 * juce::MathConstants<double>::pi / getSampleRate();
    adsr.reset();
    adsr.noteOn();
}

void WarfSynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        angleDelta = 0.0;
        adsr.reset();
    }
}

float WarfSynthVoice::renderSample()
{
    const auto phase = std::fmod (currentAngle, 2.0 * juce::MathConstants<double>::pi);
    float raw = 0.0f;

    switch (waveform)
    {
        case Waveform::sine:     raw = (float) std::sin (phase); break;
        case Waveform::saw:      raw = (float) (1.0 - phase / juce::MathConstants<double>::pi); break;
        case Waveform::square:   raw = phase < juce::MathConstants<double>::pi ? 1.0f : -1.0f; break;
        case Waveform::triangle: raw = (float) (2.0 / juce::MathConstants<double>::pi * std::asin (std::sin (phase))); break;
    }

    currentAngle += angleDelta;
    return raw;
}

void WarfSynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (angleDelta == 0.0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envValue = adsr.getNextSample();
        // 0.3f headroom - several overlapping voices (retrigger during a fast phrase) shouldn't clip.
        const auto sampleValue = renderSample() * level * envValue * 0.3f;

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample (ch, startSample + i, sampleValue);

        if (! adsr.isActive())
        {
            clearCurrentNote();
            angleDelta = 0.0;
            break;
        }
    }
}
