#include "ParameterLayout.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // A single friendly "Sensitivity" knob rather than exposing minConfidence/attackHops/
    // pitchToleranceCents directly - see PluginProcessor::processBlock for the mapping. 0 = strict/
    // clean (fewer false triggers, slightly slower to respond), 1 = fast/loose (quicker to trigger,
    // more prone to jitter on a noisy or ambiguous source).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::sensitivity, 1 }, "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::gateThresholdDb, 1 }, "Gate Threshold (dB)",
        juce::NormalisableRange<float> (-60.0f, -10.0f), -40.0f));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::transpose, 1 }, "Transpose (st)",
        -24, 24, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::midiChannel, 1 }, "MIDI Channel",
        1, 16, 1));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::fixedVelocity, 1 }, "Fixed Velocity", false));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::fixedVelocityValue, 1 }, "Fixed Velocity Value",
        1, 127, 100));

    return { params.begin(), params.end() };
}
