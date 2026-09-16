#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
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

    // No MIDI is consumed, and Warf deliberately does NOT declare a VST3 MIDI output bus either
    // (see CMakeLists.txt) - a real user found that Studio One auto-previews an Fx's declared MIDI
    // output through its own default General MIDI softsynth (Microsoft GS Wavetable Synth on
    // Windows, whose default patch is Acoustic Grand Piano - exactly the phantom "piano" sound
    // reported, heard even with no device selected in Warf's own MIDI Output Device picker). Since
    // that bus never served a purpose here anyway (Studio One doesn't let you route an Fx's MIDI
    // output bus anywhere useful, only Instrument-slot plugins), not declaring it removes the
    // side effect entirely without losing anything - generated notes only ever go out through
    // sendToSelectedMidiOutput()'s direct juce::MidiOutput connection now.
    bool acceptsMidi() const override { return false; }
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

    // Message-thread read access for the editor's live display, updated once per analysis hop on
    // the audio thread. Plain atomics are enough since these only ever feed a GUI label/meter.
    double getLastDetectedFrequencyHz() const { return lastFrequencyHz.load(); }
    bool isLastHopVoiced() const { return lastHopVoiced.load(); }
    float getLastRms() const { return lastRms.load(); }

    // MIDI Output Device picker: sends generated notes directly to a system MIDI port
    // (juce::MidiOutput) - the only MIDI output path Warf has (see producesMidi() above for why
    // there's no VST3 MIDI bus to fall back on). Point a MIDI/Instrument track's input at the same
    // virtual MIDI port (e.g. one created by loopMIDI) to get it into your DAW. Message-thread only.
    juce::StringArray getMidiOutputDeviceNames() const;
    void setMidiOutputDeviceByIndex (int index); // -1 = none
    int getMidiOutputDeviceIndex() const { return currentMidiOutputIndex; }

private:
    void sendToSelectedMidiOutput (const juce::MidiMessage& message);

    juce::AudioProcessorValueTreeState apvts;
    PitchDetector pitchDetector;
    NoteTracker noteTracker;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<double> lastFrequencyHz { 0.0 };
    std::atomic<bool> lastHopVoiced { false };
    std::atomic<float> lastRms { 0.0f };

    juce::CriticalSection midiOutputLock;
    std::unique_ptr<juce::MidiOutput> midiOutputDevice; // guarded by midiOutputLock
    int currentMidiOutputIndex = -1;
    juce::String lastSelectedMidiOutputIdentifier; // persisted so the choice survives project reload

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarfAudioProcessor)
};
