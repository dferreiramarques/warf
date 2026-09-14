#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../src/NoteTracker.h"

namespace
{
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
