#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Minimal native JUCE GUI: a live pitch/note readout plus the handful of global parameters
// exposed as APVTS parameters. No waveform or per-note history view yet - see README for what's
// deliberately out of scope for this first version.
class WarfAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit WarfAudioProcessorEditor (WarfAudioProcessor&);
    ~WarfAudioProcessorEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    WarfAudioProcessor& processor;
    int midiDeviceRefreshCounter = 0;

    juce::Label titleLabel { {}, "Warf" };
    juce::Label subtitleLabel { {}, "audio to MIDI" };

    juce::Label detectedNoteLabel { {}, "--" };
    juce::Label statusLabel { {}, "Listening..." };

    // Not an APVTS parameter - this is a device selection (host-agnostic, chosen from whatever
    // system MIDI ports currently exist), not something a DAW would automate. See
    // WarfAudioProcessor's own comment for why this exists.
    juce::ComboBox midiOutputDeviceBox;
    juce::Label midiOutputDeviceLabel { {}, "MIDI Output Device" };
    void refreshMidiOutputDeviceList();

    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel { {}, "Sensitivity" };
    juce::Slider gateSlider;
    juce::Label gateLabel { {}, "Gate Threshold (dB)" };
    juce::Slider transposeSlider;
    juce::Label transposeLabel { {}, "Transpose (st)" };
    juce::Slider midiChannelSlider;
    juce::Label midiChannelLabel { {}, "MIDI Channel" };
    juce::ToggleButton fixedVelocityToggle { "Fixed Velocity" };
    juce::Slider fixedVelocityValueSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> transposeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midiChannelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fixedVelocityValueAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fixedVelocityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarfAudioProcessorEditor)
};
