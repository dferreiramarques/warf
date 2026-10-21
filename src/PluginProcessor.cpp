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
    monoScratch.setSize (1, samplesPerBlock);

    // Don't reset noteTracker here directly: hosts can call prepareToPlay again (sample rate or
    // block size change) while a note is still sounding, and a plain reset would forget it without
    // ever sending its note-off. The next processBlock sends that note-off, then resets.
    flushNotesOnNextBlock = true;
}

void WarfAudioProcessor::releaseResources()
{
    // processBlock won't run again until the next prepareToPlay, so a still-sounding note's
    // note-off can't go out through the VST3 bus from here - but the system MIDI device would
    // otherwise hang on it indefinitely, so send it there directly. The audio thread is stopped
    // at this point, so touching noteTracker from here is safe.
    juce::MidiBuffer noteOffs;
    noteTracker.allNotesOff (noteOffs);
    for (const auto metadata : noteOffs)
        sendToSelectedMidiOutput (metadata.getMessage());
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

    if (flushNotesOnNextBlock.exchange (false))
        noteTracker.allNotesOff (midiMessages);

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
    // 0.95..0.85: PitchDetector never reports a pitch below 0.85 confidence (its 0.15 YIN
    // threshold), so anything lower would make the top half of the knob do nothing here.
    settings.minConfidence = (double) juce::jmap (sensitivity, 0.0f, 1.0f, 0.95f, 0.85f);
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

    // Forward the same events to the directly-selected system MIDI device too, if one's chosen -
    // independent of the host's own VST3-bus routing above. See the MIDI Output Device picker.
    for (const auto metadata : midiMessages)
        sendToSelectedMidiOutput (metadata.getMessage());
}

void WarfAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages)
{
    // Audio is left untouched (dry pass-through, same as when active). Bypassing mid-note must
    // still end that note, on both output paths - otherwise it hangs until un-bypassed. A no-op
    // once nothing is sounding.
    midiMessages.clear();
    noteTracker.allNotesOff (midiMessages);

    for (const auto metadata : midiMessages)
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

        // Only persist a device that actually opened - otherwise a failed open would be silently
        // retried on every project reload.
        if (newDevice != nullptr)
            newIdentifier = devices[index].identifier;
    }

    // Read before the move below: once moved-from, newDevice is always null, which used to leave
    // currentMidiOutputIndex stuck at -1 - the editor then showed "None" while a restored device
    // (e.g. Microsoft GS Wavetable Synth, the "phantom piano") was still open and receiving notes.
    const auto opened = newDevice != nullptr;

    const juce::ScopedLock lock (midiOutputLock);
    midiOutputDevice = std::move (newDevice);
    currentMidiOutputIndex = opened ? index : -1;
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
