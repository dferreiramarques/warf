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

    // No MIDI is consumed. The VST3 MIDI output bus (producesMidi()) IS declared - turned out to
    // be load-bearing: a real user relies on Studio One's own "Instrument Input" track routing
    // (pointing a separate Instrument track's input at Warf's audio track) to get Warf's MIDI to a
    // synth, and that only works when this bus exists, Fx-category or not - removing it (an
    // earlier attempt at fixing a phantom-piano report) broke that entirely. The phantom piano
    // itself needs a different fix - see the MIDI Output Device picker below and the "read this
    // before building" note for the current understanding of where it's actually coming from.
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

    // MIDI Output Device picker: ALSO sends generated notes directly to a system MIDI port
    // (juce::MidiOutput), independent of the VST3 bus above - useful for reaching an external
    // hardware synth, or a virtual MIDI port (loopMIDI) that some other app/DAW instance is
    // listening to. Not needed for Studio One's own Instrument Input routing, which reads directly
    // from the VST3 bus regardless of what's selected here. Message-thread only.
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
