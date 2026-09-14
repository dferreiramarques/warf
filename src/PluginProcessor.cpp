#include "PluginProcessor.h"
#include "PluginEditor.h"

WarfAudioProcessor::WarfAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void WarfAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pitchDetector.prepare (sampleRate);
    noteTracker.prepare (NoteTracker::Settings {});
    monoScratch.setSize (1, samplesPerBlock);
}

void WarfAudioProcessor::releaseResources()
{
}

bool WarfAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void WarfAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Nothing consumes incoming MIDI; only the events generated below should reach the host.
    midiMessages.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Mono-sum for analysis into scratch space without touching `buffer` - the input passes
    // through to the output dry and unmodified, so a host can monitor the source while also
    // routing this plugin's MIDI output elsewhere.
    monoScratch.setSize (1, numSamples, false, false, true);
    monoScratch.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        monoScratch.addFrom (0, 0, buffer, ch, 0, numSamples, 1.0f / (float) numChannels);

    // Sensitivity (0=strict/clean, 1=fast/loose) maps to the underlying confidence/tolerance/
    // attack-hop knobs NoteTracker actually uses - see NoteTracker.h for what each one does.
    const auto sensitivity = apvts.getRawParameterValue (ParameterIDs::sensitivity)->load();

    NoteTracker::Settings settings;
    settings.noiseGate = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParameterIDs::gateThresholdDb)->load());
    settings.minConfidence = (double) juce::jmap (sensitivity, 0.0f, 1.0f, 0.95f, 0.75f);
    settings.pitchToleranceCents = (double) juce::jmap (sensitivity, 0.0f, 1.0f, 20.0f, 50.0f);
    settings.attackHops = sensitivity < 0.5f ? 4 : 2;
    settings.releaseHops = 4;
    settings.transposeSemitones = (int) apvts.getRawParameterValue (ParameterIDs::transpose)->load();
    settings.midiChannel = (int) apvts.getRawParameterValue (ParameterIDs::midiChannel)->load();
    settings.fixedVelocity = apvts.getRawParameterValue (ParameterIDs::fixedVelocity)->load() > 0.5f;
    settings.fixedVelocityValue = (juce::uint8) (int) apvts.getRawParameterValue (ParameterIDs::fixedVelocityValue)->load();
    noteTracker.setLiveSettings (settings);

    const auto* monoData = monoScratch.getReadPointer (0);
    pitchDetector.pushSamples (monoData, numSamples, [this, &midiMessages] (const PitchDetector::Result& hop, int sampleIndex)
    {
        noteTracker.processHop (hop, sampleIndex, midiMessages);
        lastFrequencyHz.store (hop.frequencyHz);
        lastHopVoiced.store (hop.hasPitch);
        lastRms.store (hop.rms);
    });
}

juce::AudioProcessorEditor* WarfAudioProcessor::createEditor()
{
    return new WarfAudioProcessorEditor (*this);
}

void WarfAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WarfAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WarfAudioProcessor();
}
