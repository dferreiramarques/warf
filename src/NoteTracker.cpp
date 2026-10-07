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

void NoteTracker::setLiveSettings (const Settings& newSettings)
{
    if (settings.fastTracking != newSettings.fastTracking || settings.melodicRelease != newSettings.melodicRelease)
    {
        // A gap measured under the previous timing policy must not re-arm or cut the note
        // when a mode is automated. Retain the sounding note itself.
        silentHops = quietHops = 0;
        articulationPending = false;
        clearCandidate();
    }
    settings = newSettings;
}

void NoteTracker::reset()
{
    noteIsSounding = false;
    currentMidiNote = -1;
    silentHops = 0;
    quietHops = 0;
    articulationPending = false;
    clearCandidate();
}

void NoteTracker::clearCandidate()
{
    candidateNote = -1;
    candidateStableHops = 0;
    candidateVelocityRms = 0.0f;
    candidateGapHops = 0;
    candidateExactMidi = 0.0;
}

void NoteTracker::processHop (const PitchDetector::Result& hop, int sampleOffsetInBlock, juce::MidiBuffer& midiOut)
{
    const float gateRms = settings.fastTracking && hop.recentRms >= 0.0f ? hop.recentRms : hop.rms;
    const bool quiet = gateRms < settings.noiseGate;
    const bool validPitch = hop.hasPitch && std::isfinite (hop.frequencyHz) && hop.frequencyHz > 0.0;
    const double exactMidi = validPitch ? MidiNoteUtils::midiNoteFromFrequency (hop.frequencyHz)
                                             + (double) settings.transposeSemitones : -1000.0;
    const bool voiced = validPitch && exactMidi > -0.5 && exactMidi < 127.5
                       && hop.confidence >= settings.minConfidence && ! quiet;

    if (! voiced)
    {
        // One above-gate ambiguity may interrupt a legato candidate, but never counts as
        // evidence. Silence and a second ambiguous hop discard it, preventing stale attacks.
        if (! settings.fastTracking || quiet || ++candidateGapHops > 1)
            clearCandidate();
        quietHops = quiet ? std::min (quietHops + 1, std::max (settings.releaseHops, settings.quietRetriggerHops)) : 0;
        if (settings.melodicRelease && noteIsSounding && quietHops >= settings.quietRetriggerHops)
            articulationPending = true;

        if (noteIsSounding)
            ++silentHops;
        // A noisy consonant or returning vowel may be above the gate before YIN recovers.
        // Give it a bounded recovery hold; true consecutive silence gets the shorter timeout.
        const bool releaseDue = settings.melodicRelease
                                    ? quietHops >= settings.releaseHops || silentHops >= settings.maxUnvoicedHops
                                    : silentHops >= settings.releaseHops;
        if (noteIsSounding && releaseDue)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);
            noteIsSounding = false;
            currentMidiNote = -1;
            silentHops = 0;
            articulationPending = false;
            clearCandidate();
        }
        return;
    }

    const auto nearestNote = (int) std::lround (exactMidi);
    // Performance keeps the old note on during the gap. Only same-pitch recovery consumes
    // the articulation latch; a different pitch still gets normal legato confirmation.
    const bool articulated = settings.fastTracking
                             && (settings.melodicRelease ? articulationPending && nearestNote == currentMidiNote
                                                         : quietHops >= settings.quietRetriggerHops);
    quietHops = 0;
    silentHops = 0;
    if (nearestNote < 0 || nearestNote > 127)
        return;

    if (noteIsSounding && ! articulated
        && (nearestNote == currentMidiNote
            || (settings.fastTracking && std::abs (exactMidi - currentMidiNote) < 0.65)))
    {
        clearCandidate();
        return;
    }

    const auto centsFromNearest = (exactMidi - (double) nearestNote) * 100.0;
    if (std::abs (centsFromNearest) > settings.pitchToleranceCents)
    {
        // Keep at most one ambiguous hop for the SAME pending note, without incrementing it.
        if (! settings.fastTracking || nearestNote != candidateNote || ++candidateGapHops > 1)
            clearCandidate();
        return;
    }

    candidateGapHops = 0;
    if (nearestNote == candidateNote
        && (! settings.fastTracking || std::abs (exactMidi - candidateExactMidi) <= 0.30))
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

    candidateExactMidi = exactMidi;

    // Mixed old/new audio can produce a confidently intermediate semitone. Require two
    // consistent observations for legato changes, including octave jumps. A new or clearly
    // articulated attack remains single-hop; this adds one 5ms hop in Performance/Percussive.
    const int requiredHops = settings.fastTracking && noteIsSounding && ! articulated
                                ? std::max (2, settings.attackHops) : settings.attackHops;
    if (candidateStableHops >= requiredHops)
    {
        if (noteIsSounding)
            midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);

        const auto velocity = settings.fixedVelocity ? settings.fixedVelocityValue : velocityFromRms (candidateVelocityRms);
        midiOut.addEvent (juce::MidiMessage::noteOn (settings.midiChannel, candidateNote, velocity), sampleOffsetInBlock);

        noteIsSounding = true;
        currentMidiNote = candidateNote;
        articulationPending = false;
        clearCandidate();
    }
}

void NoteTracker::allNotesOff (juce::MidiBuffer& midiOut, int sampleOffsetInBlock)
{
    if (noteIsSounding)
        midiOut.addEvent (juce::MidiMessage::noteOff (settings.midiChannel, currentMidiNote), sampleOffsetInBlock);

    reset();
}
