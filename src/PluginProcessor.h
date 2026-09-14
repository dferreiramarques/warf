#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "ParameterLayout.h"
#include "PitchDetector.h"
#include "NoteTracker.h"

class WarfAudioProcessor final : public juce::AudioProcessor
{
public:
    WarfAudioProcessor();
    ~WarfAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    // No MIDI is consumed - `acceptsMidi() == false` and any incoming MIDI buffer is cleared
    // before use so only the events this plugin generates reach the host.
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
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

    // Message-thread read access for the editor's live display, updated once per analysis hop on
    // the audio thread. Plain atomics are enough since these only ever feed a GUI label/meter.
    double getLastDetectedFrequencyHz() const { return lastFrequencyHz.load(); }
    bool isLastHopVoiced() const { return lastHopVoiced.load(); }
    float getLastRms() const { return lastRms.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    PitchDetector pitchDetector;
    NoteTracker noteTracker;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<double> lastFrequencyHz { 0.0 };
    std::atomic<bool> lastHopVoiced { false };
    std::atomic<float> lastRms { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarfAudioProcessor)
};
