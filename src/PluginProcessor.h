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

    // MIDI Output Device picker: sends generated notes directly to a system MIDI port
    // (juce::MidiOutput), independent of whether the host routes this plugin's own VST3 MIDI
    // output bus anywhere - Studio One notably doesn't for an audio-effect-slot plugin, so this is
    // the path that actually works there (point a MIDI/Instrument track's input at the same
    // virtual MIDI port, e.g. one created by loopMIDI). Message-thread only.
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
