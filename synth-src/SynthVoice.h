#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

enum class Waveform { sine = 0, saw, square, triangle };

// A deliberately plain oscillator + ADSR voice - Warf Synth exists to make Warf's MIDI output
// audible out of the box, not to be a serious instrument in its own right.
class WarfSynthVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

    void setWaveform (Waveform w) { waveform = w; }
    void setAdsrParameters (const juce::ADSR::Parameters& p) { adsr.setParameters (p); }

private:
    float renderSample();

    double currentAngle = 0.0;
    double angleDelta = 0.0;
    float level = 0.0f;
    Waveform waveform = Waveform::sine;
    juce::ADSR adsr;
};
