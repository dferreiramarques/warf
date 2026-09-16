#include "SynthParameterLayout.h"

juce::AudioProcessorValueTreeState::ParameterLayout createSynthParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { SynthParameterIDs::waveform, 1 }, "Waveform",
        juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SynthParameterIDs::attack, 1 }, "Attack (s)",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.01f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SynthParameterIDs::decay, 1 }, "Decay (s)",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.1f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SynthParameterIDs::sustain, 1 }, "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SynthParameterIDs::release, 1 }, "Release (s)",
        juce::NormalisableRange<float> (0.001f, 3.0f, 0.001f, 0.4f), 0.2f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SynthParameterIDs::masterVolume, 1 }, "Master Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));

    return { params.begin(), params.end() };
}
