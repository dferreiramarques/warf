#include "PluginEditor.h"
#include "MidiNoteUtils.h"
#include <cmath>

namespace
{
    juce::String noteNameFromMidiNote (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto octave = midiNote / 12 - 1;
        return juce::String (names[midiNote % 12]) + juce::String (octave);
    }

    void configureSlider (juce::Slider& slider, juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        parent.addAndMakeVisible (slider);
    }
}

WarfAudioProcessorEditor::WarfAudioProcessorEditor (WarfAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    subtitleLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (subtitleLabel);

    detectedNoteLabel.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
    detectedNoteLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectedNoteLabel);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (statusLabel);

    configureSlider (sensitivitySlider, *this);
    configureSlider (gateSlider, *this);
    configureSlider (transposeSlider, *this);
    configureSlider (midiChannelSlider, *this);
    configureSlider (fixedVelocityValueSlider, *this);

    for (auto* label : { &sensitivityLabel, &gateLabel, &transposeLabel, &midiChannelLabel })
        addAndMakeVisible (*label);

    addAndMakeVisible (fixedVelocityToggle);

    auto& apvts = processor.getAPVTS();
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParameterIDs::sensitivity, sensitivitySlider);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParameterIDs::gateThresholdDb, gateSlider);
    transposeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParameterIDs::transpose, transposeSlider);
    midiChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParameterIDs::midiChannel, midiChannelSlider);
    fixedVelocityValueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParameterIDs::fixedVelocityValue, fixedVelocityValueSlider);
    fixedVelocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParameterIDs::fixedVelocity, fixedVelocityToggle);

    setSize (420, 420);
    startTimerHz (20);
}

WarfAudioProcessorEditor::~WarfAudioProcessorEditor()
{
    stopTimer();
}

void WarfAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (32));
    subtitleLabel.setBounds (header);

    area.removeFromTop (8);
    detectedNoteLabel.setBounds (area.removeFromTop (56));
    statusLabel.setBounds (area.removeFromTop (20));

    area.removeFromTop (16);

    auto row = [&] (juce::Label& label, juce::Component& control)
    {
        auto r = area.removeFromTop (24);
        label.setBounds (r.removeFromLeft (140));
        control.setBounds (r);
        area.removeFromTop (8);
    };

    row (sensitivityLabel, sensitivitySlider);
    row (gateLabel, gateSlider);
    row (transposeLabel, transposeSlider);
    row (midiChannelLabel, midiChannelSlider);

    auto velocityRow = area.removeFromTop (24);
    fixedVelocityToggle.setBounds (velocityRow.removeFromLeft (140));
    fixedVelocityValueSlider.setBounds (velocityRow);
}

void WarfAudioProcessorEditor::timerCallback()
{
    if (processor.isLastHopVoiced())
    {
        const auto exactMidi = MidiNoteUtils::midiNoteFromFrequency (processor.getLastDetectedFrequencyHz());
        const auto nearestNote = (int) std::lround (exactMidi);
        const auto cents = (int) std::lround ((exactMidi - (double) nearestNote) * 100.0);

        detectedNoteLabel.setText (noteNameFromMidiNote (nearestNote), juce::dontSendNotification);
        statusLabel.setText (juce::String (processor.getLastDetectedFrequencyHz(), 1) + " Hz  ("
                                 + (cents >= 0 ? "+" : "") + juce::String (cents) + "c)",
                             juce::dontSendNotification);
    }
    else
    {
        detectedNoteLabel.setText ("--", juce::dontSendNotification);
        statusLabel.setText ("Listening...", juce::dontSendNotification);
    }
}
