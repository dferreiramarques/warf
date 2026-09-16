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

    // No VST3 MIDI output bus is declared (see producesMidi() in the header for why), so nothing
    // should ever be written to the host's own MIDI buffer - a host has no defined bus to route it
    // through, and at least one host (Studio One) was observed auto-previewing exactly this kind
    // of stray content through its own default General MIDI synth. Generated notes go only to
    // generatedEvents below, forwarded solely via sendToSelectedMidiOutput()'s direct connection.
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

    juce::MidiBuffer generatedEvents;
    const auto* monoData = monoScratch.getReadPointer (0);
    pitchDetector.pushSamples (monoData, numSamples, [this, &generatedEvents] (const PitchDetector::Result& hop, int sampleIndex)
    {
        noteTracker.processHop (hop, sampleIndex, generatedEvents);
        lastFrequencyHz.store (hop.frequencyHz);
        lastHopVoiced.store (hop.hasPitch);
        lastRms.store (hop.rms);
    });

    for (const auto metadata : generatedEvents)
        sendToSelectedMidiOutput (metadata.getMessage());
}

void WarfAudioProcessor::sendToSelectedMidiOutput (const juce::MidiMessage& message)
{
    // A short juce::CriticalSection lock on the audio thread isn't ideal real-time hygiene, but
    // device selection changes are rare (a user picking from the dropdown), MIDI events here are
    // sparse (note on/off, not per-sample), and juce::MidiOutput::sendMessageNow itself isn't
    // guaranteed lock-free either - this is the same pragmatic tradeoff most "plugin sends MIDI to
    // a system device" implementations make.
    const juce::ScopedLock lock (midiOutputLock);
    if (midiOutputDevice != nullptr)
        midiOutputDevice->sendMessageNow (message);
}

juce::StringArray WarfAudioProcessor::getMidiOutputDeviceNames() const
{
    juce::StringArray names;
    for (const auto& device : juce::MidiOutput::getAvailableDevices())
        names.add (device.name);
    return names;
}

void WarfAudioProcessor::setMidiOutputDeviceByIndex (int index)
{
    const auto devices = juce::MidiOutput::getAvailableDevices();

    std::unique_ptr<juce::MidiOutput> newDevice;
    juce::String newIdentifier;

    if (index >= 0 && index < devices.size())
    {
        newDevice = juce::MidiOutput::openDevice (devices[index].identifier);
        newIdentifier = devices[index].identifier;
    }

    const juce::ScopedLock lock (midiOutputLock);
    midiOutputDevice = std::move (newDevice);
    currentMidiOutputIndex = newDevice != nullptr ? index : -1;
    lastSelectedMidiOutputIdentifier = newIdentifier;
}

juce::AudioProcessorEditor* WarfAudioProcessor::createEditor()
{
    return new WarfAudioProcessorEditor (*this);
}

void WarfAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("midiOutputDeviceIdentifier", lastSelectedMidiOutputIdentifier, nullptr);

    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WarfAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    const auto savedIdentifier = tree.getProperty ("midiOutputDeviceIdentifier", "").toString();
    if (savedIdentifier.isEmpty())
        return;

    const auto devices = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].identifier == savedIdentifier)
        {
            setMidiOutputDeviceByIndex (i);
            break;
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WarfAudioProcessor();
}
