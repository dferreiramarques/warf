#include "NoteTracker.h"
#include "MidiNoteUtils.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Rough perceptual mapping from linear RMS to MIDI velocity: -40dBFS -> ~1, 0dBFS -> 127,
    // matching the noise gate's own -40dBFS default floor at the bottom end.
    juce::uint8 velocityFromRms (float rms)
    {
        const auto db = juce::Decibels::gainToDecibels (rms, -60.0f);
        const auto normalised = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -40.0f, 0.0f, 0.0f, 1.0f));
        return (juce::uint8) (juce::jlimit (0, 126, (int) std::lround (normalised * 126.0f)) + 1);
    }
}

void NoteTracker::prepare (const Settings& newSettings)
{
    settings = newSettings;
    reset();
}

void NoteTracker::reset()
{
    noteIsSounding = false;
    currentMidiNote = -1;
    silentHops = 0;
    candidateNote = -1;
    candidateStableHops = 0;
    candidateVelocityRms = 0.0f;
}

void NoteTracker::processHop (const PitchDetector::Result& hop, int sampleOffsetInBlock, juce::MidiBuffer& midiOut)
{
    const bool voiced = hop.hasPitch && hop.confidence >= settings.minConfidence && hop.rms >= settings.noiseGate;

    if (! voiced)
    {
        candidateNote = -1;
        candidateStableHops = 0;

        if (noteIsSounding && ++silentHops >= settings.releaseHops)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);
            noteIsSounding = false;
            currentMidiNote = -1;
            silentHops = 0;
        }

        return;
    }

    silentHops = 0;

    const auto exactMidi = MidiNoteUtils::midiNoteFromFrequency (hop.frequencyHz) + (double) settings.transposeSemitones;
    const auto nearestNote = (int) std::lround (exactMidi);

    if (nearestNote < 0 || nearestNote > 127)
        return;

    if (noteIsSounding && nearestNote == currentMidiNote)
    {
        // Same note continuing to sound - clear any unrelated pending-retrigger candidate.
        candidateNote = -1;
        candidateStableHops = 0;
        return;
    }

    const auto centsFromNearest = (exactMidi - (double) nearestNote) * 100.0;
    if (std::abs (centsFromNearest) > settings.pitchToleranceCents)
    {
        // Too far from any semitone centre to trust yet (e.g. mid-slide/bend) - don't let it
        // count as a stable hop, but don't treat it as unvoiced either.
        candidateStableHops = 0;
        return;
    }

    if (nearestNote == candidateNote)
    {
        ++candidateStableHops;
        candidateVelocityRms = std::max (candidateVelocityRms, hop.rms);
    }
    else
    {
        candidateNote = nearestNote;
        candidateStableHops = 1;
        candidateVelocityRms = hop.rms;
    }

    if (candidateStableHops >= settings.attackHops)
    {
        if (noteIsSounding)
            midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);

        const auto velocity = settings.fixedVelocity ? settings.fixedVelocityValue : velocityFromRms (candidateVelocityRms);
        midiOut.addEvent (juce::MidiMessage::noteOn (settings.midiChannel, candidateNote, velocity), sampleOffsetInBlock);

        noteIsSounding = true;
        currentMidiNote = candidateNote;
        candidateNote = -1;
        candidateStableHops = 0;
        candidateVelocityRms = 0.0f;
    }
}

void NoteTracker::allNotesOff (juce::MidiBuffer& midiOut, int sampleOffsetInBlock)
{
    if (noteIsSounding)
        midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);

    reset();
}
