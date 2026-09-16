#include "SynthPluginEditor.h"
#include "SynthParameterLayout.h"

namespace
{
    void configureSlider (juce::Slider& slider, juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        parent.addAndMakeVisible (slider);
    }
}

WarfSynthAudioProcessorEditor::WarfSynthAudioProcessorEditor (WarfSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    subtitleLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (subtitleLabel);

    waveformBox.addItemList ({ "Sine", "Saw", "Square", "Triangle" }, 1);
    addAndMakeVisible (waveformBox);
    addAndMakeVisible (waveformLabel);

    configureSlider (attackSlider, *this);
    configureSlider (decaySlider, *this);
    configureSlider (sustainSlider, *this);
    configureSlider (releaseSlider, *this);
    configureSlider (volumeSlider, *this);

    for (auto* label : { &attackLabel, &decayLabel, &sustainLabel, &releaseLabel, &volumeLabel })
        addAndMakeVisible (*label);

    auto& apvts = processor.getAPVTS();
    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, SynthParameterIDs::waveform, waveformBox);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, SynthParameterIDs::attack, attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, SynthParameterIDs::decay, decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, SynthParameterIDs::sustain, sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, SynthParameterIDs::release, releaseSlider);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, SynthParameterIDs::masterVolume, volumeSlider);

    setResizable (true, true);
    setResizeLimits (300, 300, 600, 600);
    setSize (360, 340);
}

void WarfSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (48);
    titleLabel.setBounds (header.removeFromTop (28));
    subtitleLabel.setBounds (header);

    area.removeFromTop (12);

    auto waveRow = area.removeFromTop (24);
    waveformLabel.setBounds (waveRow.removeFromLeft (100));
    waveformBox.setBounds (waveRow);
    area.removeFromTop (8);

    auto row = [&] (juce::Label& label, juce::Component& control)
    {
        auto r = area.removeFromTop (24);
        label.setBounds (r.removeFromLeft (100));
        control.setBounds (r);
        area.removeFromTop (8);
    };

    row (attackLabel, attackSlider);
    row (decayLabel, decaySlider);
    row (sustainLabel, sustainSlider);
    row (releaseLabel, releaseSlider);
    row (volumeLabel, volumeSlider);
}
