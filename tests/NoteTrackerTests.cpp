#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../src/NoteTracker.h"
#include "../src/TrackingSettings.h"
#include "../src/MidiNoteUtils.h"
#include "../src/ParameterLayout.h"

namespace
{
    class ParameterTestProcessor final : public juce::AudioProcessor
    {
    public:
        using juce::AudioProcessor::processBlock;
        const juce::String getName() const override { return "State test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return true; }
        double getTailLengthSeconds() const override { return 0; }
        bool hasEditor() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    PitchDetector::Result voicedHop (double freq, float rms = 0.5f, double confidence = 0.95)
    {
        PitchDetector::Result r;
        r.hasPitch = true;
        r.frequencyHz = freq;
        r.confidence = confidence;
        r.rms = rms;
        return r;
    }

    PitchDetector::Result silentHop()
    {
        return {};
    }

    struct RecordedMessages
    {
        int noteOns = 0, noteOffs = 0;
        int lastNoteOn = -1, lastNoteOff = -1;
        int lastVelocity = -1;

        void scan (const juce::MidiBuffer& buf)
        {
            for (const auto metadata : buf)
            {
                const auto msg = metadata.getMessage();
                if (msg.isNoteOn())
                {
                    ++noteOns;
                    lastNoteOn = msg.getNoteNumber();
                    lastVelocity = msg.getVelocity();
                }
                else if (msg.isNoteOff())
                {
                    ++noteOffs;
                    lastNoteOff = msg.getNoteNumber();
                }
            }
        }
    };

    void feed (NoteTracker& tracker, const PitchDetector::Result& hop, int times, RecordedMessages& out)
    {
        for (int i = 0; i < times; ++i)
        {
            juce::MidiBuffer buf;
            tracker.processHop (hop, 0, buf);
            out.scan (buf);
        }
    }
}

class NoteTrackerTests final : public juce::UnitTest
{
public:
    NoteTrackerTests() : UnitTest ("NoteTracker", "Warf") {}

    void runTest() override
    {
        beginTest ("Mode parameter endpoints, legacy Fast migration and new mode round-trips");
        {
            juce::ScopedJuceInitialiser_GUI initialiseJuce;
            ParameterTestProcessor processor;
            juce::AudioProcessorValueTreeState apvts (processor, nullptr, "PARAMETERS", createParameterLayout());
            auto* parameter = apvts.getParameter (ParameterIDs::trackingMode);
            expectEquals (parameter->getText (0.0f, 64), juce::String ("Stable"));
            expectEquals (parameter->getText (0.5f, 64), juce::String ("Performance"));
            expectEquals (parameter->getText (1.0f, 64), juce::String ("Percussive"));
            for (const int legacyMode : { -1, 0, 1 })
            {
                parameter->setValueNotifyingHost (0.5f); // Reused instance previously in Performance.
                juce::ValueTree state ("PARAMETERS");
                state.setProperty ("midiOutputDeviceIdentifier", "saved-device", nullptr);
                if (legacyMode >= 0)
                {
                    juce::ValueTree mode ("PARAM");
                    mode.setProperty ("id", ParameterIDs::trackingMode, nullptr);
                    mode.setProperty ("value", (float) legacyMode, nullptr);
                    state.appendChild (mode, nullptr);
                }
                migrateTrackingModeState (state);
                migrateTrackingModeState (state);
                apvts.replaceState (state);
                expectEquals ((int) apvts.getRawParameterValue (ParameterIDs::trackingMode)->load(), legacyMode == 1 ? 2 : 0);
                expectEquals (apvts.state.getProperty ("midiOutputDeviceIdentifier").toString(), juce::String ("saved-device"));
            }
            for (const float normalised : { 0.0f, 0.5f, 1.0f })
            {
                parameter->setValueNotifyingHost (normalised);
                auto saved = apvts.copyState();
                saved.setProperty (TrackingModeState::versionProperty, TrackingModeState::currentVersion, nullptr);
                auto xml = saved.createXml();
                auto restored = juce::ValueTree::fromXml (*xml);
                migrateTrackingModeState (restored);
                parameter->setValueNotifyingHost (0.0f);
                apvts.replaceState (restored);
                expectWithinAbsoluteError (parameter->getValue(), normalised, 0.0001f);
            }
        }

        beginTest ("Performance holds 5-35ms quiet gaps, retriggers on recovery without a MIDI dead zone");
        for (int gap = 1; gap <= 7; ++gap)
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::performance));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 1); // Initial attack remains single-hop.
            feed (tracker, silentHop(), gap, r);
            expectEquals (r.noteOffs, 0);
            juce::MidiBuffer midi;
            tracker.processHop (voicedHop (440.0), 73, midi);
            expectEquals (midi.getNumEvents(), gap >= 3 ? 2 : 0);
            int index = 0;
            for (const auto event : midi)
            {
                expectEquals (event.samplePosition, 73);
                expect (index++ == 0 ? event.getMessage().isNoteOff() : event.getMessage().isNoteOn());
                expectEquals (event.getMessage().getNoteNumber(), 69);
            }
        }

        beginTest ("Performance absorbs 5-40ms above-gate consonants without re-articulating");
        for (int gap = 1; gap <= 8; ++gap)
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::performance));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, voicedHop (440.0, 0.2f, 0.5), gap, r);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
        }

        beginTest ("Performance separates quiet release from bounded unvoiced recovery");
        for (const bool quiet : { true, false })
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::performance));
            const auto gap = quiet ? silentHop() : voicedHop (440.0, 0.2f, 0.5);
            const int limit = quiet ? 8 : 15;
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, gap, limit - 1, r);
            expectEquals (r.noteOffs, 0);
            feed (tracker, gap, 1, r);
            expectEquals (r.noteOffs, 1);
            feed (tracker, gap, 30, r);
            expectEquals (r.noteOffs, 1);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 2);
        }

        beginTest ("Performance da-da articulation latch survives noisy recovery and is consumed once");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::performance));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            for (int syllable = 0; syllable < 4; ++syllable)
            {
                feed (tracker, silentHop(), 4, r); // 20ms closure.
                feed (tracker, voicedHop (440.0, 0.2f, 0.5), 4, r); // 20ms noisy consonant/recovery.
                expectEquals (r.noteOffs, syllable);
                juce::MidiBuffer midi;
                tracker.processHop (voicedHop (440.0), 19, midi);
                expectEquals (midi.getNumEvents(), 2);
                int index = 0;
                for (const auto event : midi)
                {
                    expectEquals (event.samplePosition, 19);
                    expect (index++ == 0 ? event.getMessage().isNoteOff() : event.getMessage().isNoteOn());
                }
                r.scan (midi);
                feed (tracker, voicedHop (440.0), 20, r);
                expectEquals (r.noteOns, syllable + 2);
                expectEquals (r.noteOffs, syllable + 1);
            }
        }

        beginTest ("Performance pending articulation retains legato confirmation for a changed pitch");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::performance));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, silentHop(), 4, r);
            feed (tracker, voicedHop (493.883), 1, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
            feed (tracker, voicedHop (493.883), 1, r);
            expectEquals (r.noteOns, 2);
            expectEquals (r.noteOffs, 1);
            feed (tracker, voicedHop (493.883), 50, r);
            expectEquals (r.noteOns, 2);
        }

        beginTest ("Stable and Percussive retain attack, release and short-gap retrigger policies");
        for (const auto mode : { TrackingMode::stable, TrackingMode::percussive })
        {
            NoteTracker tracker;
            const auto settings = makeTrackingSettings (0.8f, mode);
            tracker.prepare (settings);
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), settings.attackHops - 1, r);
            expectEquals (r.noteOns, 0);
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, silentHop(), 2, r);
            expectEquals (r.noteOffs, 0);
            feed (tracker, voicedHop (440.0), 1, r);
            const int repeats = mode == TrackingMode::stable ? 0 : 1;
            expectEquals (r.noteOns, 1 + repeats);
            expectEquals (r.noteOffs, repeats);
            feed (tracker, silentHop(), settings.releaseHops - 1, r);
            expectEquals (r.noteOffs, repeats);
            feed (tracker, silentHop(), 1, r);
            expectEquals (r.noteOffs, repeats + 1);
        }

        beginTest ("All modes sustain without retriggers and clear pending articulation on panic/mode switch");
        for (const auto mode : { TrackingMode::stable, TrackingMode::performance, TrackingMode::percussive })
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, mode));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 200, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
            tracker.setLiveSettings (makeTrackingSettings (0.8f, TrackingMode::performance));
            feed (tracker, silentHop(), 3, r);
            tracker.setLiveSettings (makeTrackingSettings (0.8f, TrackingMode::percussive));
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
            tracker.setLiveSettings (makeTrackingSettings (0.8f, TrackingMode::performance));
            feed (tracker, silentHop(), 3, r);
            juce::MidiBuffer midi;
            tracker.allNotesOff (midi);
            r.scan (midi);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 2);
            expectEquals (r.noteOffs, 1);
        }

        beginTest ("Legacy state migration defaults to Stable and preserves existing parameters/device");
        {
            juce::ValueTree oldState ("PARAMETERS");
            oldState.setProperty ("midiOutputDeviceIdentifier", "test-device", nullptr);
            juce::ValueTree sensitivity ("PARAM");
            sensitivity.setProperty ("id", ParameterIDs::sensitivity, nullptr);
            sensitivity.setProperty ("value", 0.81f, nullptr);
            oldState.appendChild (sensitivity, nullptr);
            migrateTrackingModeState (oldState);
            auto mode = oldState.getChildWithProperty ("id", ParameterIDs::trackingMode);
            expectEquals ((float) mode.getProperty ("value"), 0.0f);
            expectEquals (oldState.getProperty ("midiOutputDeviceIdentifier").toString(), juce::String ("test-device"));
            expectWithinAbsoluteError ((float) sensitivity.getProperty ("value"), 0.81f, 0.0001f);
            // Save/reload a versioned Performance state: migration must preserve the new index.
            mode.setProperty ("value", 1.0f, nullptr);
            auto xml = oldState.createXml();
            auto restored = juce::ValueTree::fromXml (*xml);
            migrateTrackingModeState (restored);
            expectEquals (restored.getNumChildren(), 2);
            expectEquals ((float) restored.getChildWithProperty ("id", ParameterIDs::trackingMode).getProperty ("value"), 1.0f);
        }

        beginTest ("Percussive mapping permits single-hop onset without loosening confidence below YIN");
        {
            const auto settings = makeTrackingSettings (0.8f, TrackingMode::percussive);
            expectEquals (settings.attackHops, 1);
            expect (settings.minConfidence >= 0.85);
            expectEquals (makeTrackingSettings (0.8f, TrackingMode::stable).attackHops, 2);
            expectEquals (makeTrackingSettings (0.4f, TrackingMode::stable).attackHops, 4);
            NoteTracker tracker;
            tracker.prepare (settings);
            juce::MidiBuffer midi;
            tracker.processHop (voicedHop (440.0), 37, midi);
            expectEquals (midi.getNumEvents(), 1);
            for (const auto event : midi)
            {
                expect (event.getMessage().isNoteOn());
                expectEquals (event.samplePosition, 37);
            }
        }

        beginTest ("Percussive captures a one-hop short note and releases on exactly the third silent hop");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, silentHop(), 2, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
            feed (tracker, silentHop(), 1, r);
            expectEquals (r.noteOffs, 1);
        }

        beginTest ("Percussive legato changes take two hops and send off before on at the same timestamp");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            for (const int note : { 71, 72, 69, 67 })
            {
                juce::MidiBuffer midi;
                tracker.processHop (voicedHop (MidiNoteUtils::frequencyFromMidiNote (note)), 3, midi);
                expectEquals (midi.getNumEvents(), 0);
                tracker.processHop (voicedHop (MidiNoteUtils::frequencyFromMidiNote (note)), 11, midi);
                expectEquals (midi.getNumEvents(), 2);
                int index = 0;
                for (const auto event : midi)
                {
                    expectEquals (event.samplePosition, 11);
                    expect (index++ == 0 ? event.getMessage().isNoteOff() : event.getMessage().isNoteOn());
                }
                r.scan (midi);
            }
            expectEquals (r.noteOns, 5);
            expectEquals (r.noteOffs, 4);
        }

        beginTest ("Percussive semitone hysteresis resists boundary jitter in both directions");
        for (const int startNote : { 69, 70 })
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (1.0f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (MidiNoteUtils::frequencyFromMidiNote (startNote)), 1, r);
            for (int i = 0; i < 20; ++i)
                for (const double pitch : { 69.45, 69.51, 69.59, 69.41 })
                    feed (tracker, voicedHop (MidiNoteUtils::frequencyFromMidiNote (pitch)), 1, r);
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
            feed (tracker, voicedHop (MidiNoteUtils::frequencyFromMidiNote (startNote == 69 ? 70 : 69)), 2, r);
            expectEquals (r.noteOns, 2);
        }

        beginTest ("Percussive absorbs brief confidence dropouts and one quiet hop without false retriggers");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            for (int i = 0; i < 8; ++i)
            {
                feed (tracker, voicedHop (440.0, 0.5f, 0.7), 2, r);
                feed (tracker, voicedHop (440.0), 1, r);
                feed (tracker, silentHop(), 1, r);
                feed (tracker, voicedHop (440.0), 1, r);
            }
            expectEquals (r.noteOns, 1);
            expectEquals (r.noteOffs, 0);
        }

        beginTest ("Percussive same-note articulation needs two quiet hops, using recent RMS not window tail");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0), 1, r);
            auto tail = voicedHop (440.0);
            tail.recentRms = 0.0f;
            feed (tracker, tail, 2, r);
            expectEquals (r.noteOffs, 0);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 2);
            expectEquals (r.noteOffs, 1);
        }

        beginTest ("Percussive ignores intermediate semitones drifting through the same rounded candidate");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (1.0f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (220.0), 1, r);
            for (const double pitch : { 57.8, 58.39 })
                feed (tracker, voicedHop (MidiNoteUtils::frequencyFromMidiNote (pitch)), 1, r);
            expectEquals (r.noteOns, 1);
            feed (tracker, voicedHop (MidiNoteUtils::frequencyFromMidiNote (59)), 2, r);
            expectEquals (r.noteOns, 2);
            expectEquals (r.lastNoteOn, 59);
        }

        beginTest ("Percussive rejects isolated octave errors but accepts a confirmed octave jump");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (220.0), 1, r);
            feed (tracker, voicedHop (440.0), 1, r);
            feed (tracker, voicedHop (220.0), 1, r);
            expectEquals (r.noteOns, 1);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 1);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, 2);
        }

        beginTest ("Percussive octave candidate survives one ambiguous hop but not silence or two gaps");
        for (const int gapKind : { 0, 1, 2, 3 })
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (0.8f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (220.0), 1, r);
            feed (tracker, voicedHop (440.0), 1, r);
            const auto ambiguity = gapKind == 3 ? voicedHop (MidiNoteUtils::frequencyFromMidiNote (69.45))
                                               : voicedHop (440.0, 0.5f, 0.7);
            feed (tracker, gapKind == 1 ? silentHop() : ambiguity, gapKind == 2 ? 2 : 1, r);
            feed (tracker, voicedHop (440.0), 1, r);
            expectEquals (r.noteOns, gapKind == 0 || gapKind == 3 ? 2 : 1);
        }

        beginTest ("Percussive preserves channel, transpose and both velocity paths");
        for (const bool fixed : { false, true })
        {
            auto settings = makeTrackingSettings (0.8f, TrackingMode::percussive);
            settings.midiChannel = 5;
            settings.transposeSemitones = -12;
            settings.fixedVelocity = fixed;
            settings.fixedVelocityValue = 77;
            NoteTracker tracker;
            tracker.prepare (settings);
            juce::MidiBuffer midi;
            tracker.processHop (voicedHop (440.0), 0, midi);
            tracker.allNotesOff (midi, 1);
            expectEquals (midi.getNumEvents(), 2);
            for (const auto event : midi)
            {
                const auto message = event.getMessage();
                expectEquals (message.getChannel(), 5);
                expectEquals (message.getNoteNumber(), 57);
                if (message.isNoteOn())
                    expectEquals ((int) message.getVelocity(), fixed ? 77 : 108);
            }
        }

        beginTest ("Percussive rejects subthreshold confidence, gate noise and invalid frequencies");
        {
            NoteTracker tracker;
            tracker.prepare (makeTrackingSettings (1.0f, TrackingMode::percussive));
            RecordedMessages r;
            feed (tracker, voicedHop (440.0, 0.5f, 0.84), 6, r);
            feed (tracker, voicedHop (440.0, 0.001f), 6, r);
            feed (tracker, voicedHop (0.0), 1, r);
            feed (tracker, voicedHop (std::numeric_limits<double>::infinity()), 1, r);
            feed (tracker, voicedHop (std::numeric_limits<double>::quiet_NaN()), 1, r);
            expectEquals (r.noteOns, 0);
        }

        beginTest ("triggers exactly one note-on after attackHops of stable 440Hz (A4)");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 3;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 5, recorded);

            expectEquals (recorded.noteOns, 1);
            expectEquals (recorded.lastNoteOn, 69);
        }

        beginTest ("ends the note after releaseHops of silence");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.releaseHops = 3;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 3, recorded);
            feed (tracker, silentHop(), 3, recorded);

            expectEquals (recorded.noteOns, 1);
            expectEquals (recorded.noteOffs, 1);
            expectEquals (recorded.lastNoteOff, 69);
        }

        beginTest ("a single silent hop mid-note does not end it");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.releaseHops = 5;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 3, recorded);
            feed (tracker, silentHop(), 1, recorded);
            feed (tracker, voicedHop (440.0), 3, recorded);

            expectEquals (recorded.noteOns, 1);
            expectEquals (recorded.noteOffs, 0);
        }

        beginTest ("retriggers when a stably-held pitch jumps to a different note mid-sound");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 3, recorded);   // A4 = 69
            feed (tracker, voicedHop (493.883), 3, recorded); // B4 = 71

            expectEquals (recorded.noteOns, 2);
            expectEquals (recorded.noteOffs, 1);
            expectEquals (recorded.lastNoteOn, 71);
            expectEquals (recorded.lastNoteOff, 69);
        }

        beginTest ("respects a transpose offset");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.transposeSemitones = 12;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 3, recorded);

            expectEquals (recorded.lastNoteOn, 81); // A4 + one octave = A5
        }

        beginTest ("a hop below the noise gate is treated as unvoiced");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.noiseGate = 0.1f;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0, 0.01f), 5, recorded); // rms below the gate

            expectEquals (recorded.noteOns, 0);
        }

        beginTest ("fixed velocity mode uses the configured value instead of one derived from level");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.fixedVelocity = true;
            settings.fixedVelocityValue = 77;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0, 0.9f), 3, recorded);

            expectEquals (recorded.lastVelocity, 77);
        }

        beginTest ("allNotesOff ends a sounding note immediately, ignoring releaseHops");
        {
            NoteTracker tracker;
            NoteTracker::Settings settings;
            settings.attackHops = 2;
            settings.releaseHops = 100;
            tracker.prepare (settings);

            RecordedMessages recorded;
            feed (tracker, voicedHop (440.0), 3, recorded);

            juce::MidiBuffer buf;
            tracker.allNotesOff (buf);
            recorded.scan (buf);

            expectEquals (recorded.noteOffs, 1);
            expectEquals (recorded.lastNoteOff, 69);
        }
    }
};

static NoteTrackerTests noteTrackerTests;
