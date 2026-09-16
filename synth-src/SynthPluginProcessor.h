#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthParameterLayout.h"

// Warf Synth is a companion, not a serious instrument: load it on the MIDI/Instrument track that
// receives Warf's detected notes (via that track's Instrument Input routing) so there's something
// to actually hear without needing a third-party synth. Plain MIDI-in/audio-out, no audio input
// bus - unlike Warf itself, this is a completely ordinary VST3 instrument.
class WarfSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    WarfSynthAudioProcessor();
    ~WarfSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    static constexpr int numVoices = 8;

    juce::AudioProcessorValueTreeState apvts;
    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarfSynthAudioProcessor)
};
