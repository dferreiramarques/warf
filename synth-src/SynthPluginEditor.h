#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SynthPluginProcessor.h"

class WarfSynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit WarfSynthAudioProcessorEditor (WarfSynthAudioProcessor&);
    ~WarfSynthAudioProcessorEditor() override = default;

    void resized() override;

private:
    WarfSynthAudioProcessor& processor;

    juce::Label titleLabel { {}, "Warf Synth" };
    juce::Label subtitleLabel { {}, "plays Warf's MIDI output" };

    juce::ComboBox waveformBox;
    juce::Label waveformLabel { {}, "Waveform" };
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider, volumeSlider;
    juce::Label attackLabel { {}, "Attack" }, decayLabel { {}, "Decay" }, sustainLabel { {}, "Sustain" },
                releaseLabel { {}, "Release" }, volumeLabel { {}, "Volume" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarfSynthAudioProcessorEditor)
};
