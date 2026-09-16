#include "SynthPluginProcessor.h"
#include "SynthPluginEditor.h"
#include "SynthVoice.h"
#include "SynthSound.h"

WarfSynthAudioProcessor::WarfSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createSynthParameterLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new WarfSynthVoice());
    synth.addSound (new WarfSynthSound());
}

void WarfSynthAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

bool WarfSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WarfSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto waveform = (Waveform) (int) apvts.getRawParameterValue (SynthParameterIDs::waveform)->load();

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = apvts.getRawParameterValue (SynthParameterIDs::attack)->load();
    adsrParams.decay = apvts.getRawParameterValue (SynthParameterIDs::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue (SynthParameterIDs::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue (SynthParameterIDs::release)->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<WarfSynthVoice*> (synth.getVoice (i)))
        {
            voice->setWaveform (waveform);
            voice->setAdsrParameters (adsrParams);
        }
    }

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    const auto masterVolume = apvts.getRawParameterValue (SynthParameterIDs::masterVolume)->load();
    buffer.applyGain (masterVolume);
}

juce::AudioProcessorEditor* WarfSynthAudioProcessor::createEditor()
{
    return new WarfSynthAudioProcessorEditor (*this);
}

void WarfSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WarfSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WarfSynthAudioProcessor();
}
