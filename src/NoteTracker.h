#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PitchDetector.h"

// Turns a stream of PitchDetector::Result (one per analysis hop) into MIDI note-on/note-off
// events. Runs entirely on the audio thread; no allocations after prepare().
//
// State machine, evaluated per hop:
//  - unvoiced (no reliable pitch, or RMS below the noise gate): if a note is currently sounding,
//    count consecutive silent/unvoiced hops; once `releaseHops` of them have elapsed, send
//    note-off. A single stray silent hop mid-note does NOT end it - this absorbs brief dropouts
//    (a pick attack's noise burst, a breath in a vocal line) without chopping the note.
//  - voiced, no note currently sounding: require `attackHops` consecutive hops that all land on
//    the same nearest MIDI note (within `pitchToleranceCents`) before triggering note-on. This
//    debounces onset jitter and octave errors instead of firing on the very first noisy hop.
//  - voiced, a note IS sounding, new pitch matches the current note: nothing to do.
//  - voiced, a note IS sounding, new pitch stably lands on a DIFFERENT note for `attackHops`
//    hops: retrigger (note-off then note-on) - this is what lets a legato phrase produce separate
//    MIDI notes without a silence gap between them.
class NoteTracker
{
public:
    struct Settings
    {
        float noiseGate = 0.01f;            // RMS gate, linear (~ -40dBFS)
        double minConfidence = 0.85;
        int attackHops = 3;                 // consecutive stable hops required to (re)trigger a note
        int releaseHops = 4;                // consecutive silent/unvoiced hops required to end a note
        double pitchToleranceCents = 35.0;  // how far a hop's pitch may drift and still count as "the same note"
        int transposeSemitones = 0;
        int midiChannel = 1;
        bool fixedVelocity = false;
        juce::uint8 fixedVelocityValue = 100;
    };

    // Sets the settings and clears all tracking state - call once from prepareToPlay.
    void prepare (const Settings& newSettings);

    // Updates settings without touching tracking state (currently-sounding note, attack/release
    // counters) - call every block so host automation of e.g. transpose or the gate takes effect
    // immediately without cutting the note being tracked.
    void setLiveSettings (const Settings& newSettings) { settings = newSettings; }

    // Called once per PitchDetector hop. Appends any resulting note-on/off messages to `midiOut`,
    // timestamped at `sampleOffsetInBlock`.
    void processHop (const PitchDetector::Result& hop, int sampleOffsetInBlock, juce::MidiBuffer& midiOut);

    // Forces any currently-sounding note off and resets tracking state - call on prepareToPlay or panic.
    void allNotesOff (juce::MidiBuffer& midiOut, int sampleOffsetInBlock = 0);

private:
    void reset();

    Settings settings;

    bool noteIsSounding = false;
    int currentMidiNote = -1;
    int silentHops = 0;

    int candidateNote = -1;
    int candidateStableHops = 0;
    float candidateVelocityRms = 0.0f;
};
