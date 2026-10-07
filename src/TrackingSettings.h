#pragma once

#include "NoteTracker.h"
#include "TrackingMode.h"
#include <algorithm>

// Shared by the processor and pipeline tests so timing tests exercise the shipped mapping.
inline NoteTracker::Settings makeTrackingSettings (float sensitivity, TrackingMode mode)
{
    sensitivity = std::clamp (sensitivity, 0.0f, 1.0f);
    NoteTracker::Settings settings;
    settings.minConfidence = 0.95 - 0.20 * (double) sensitivity;
    settings.pitchToleranceCents = 20.0 + 30.0 * (double) sensitivity;
    settings.attackHops = sensitivity < 0.5f ? 4 : 2;
    settings.releaseHops = 4;
    const bool fast = mode != TrackingMode::stable;
    settings.fastTracking = fast;
    if (fast)
    {
        settings.attackHops = 1;
        settings.releaseHops = 3;
        // YIN already requires ~0.85 confidence. Do not relax that acoustic safeguard for
        // single-hop attacks, and leave a dead band around semitone boundaries.
        settings.minConfidence = std::max (0.85, settings.minConfidence);
        settings.pitchToleranceCents = std::min (40.0, settings.pitchToleranceCents);
    }
    if (mode == TrackingMode::performance)
    {
        settings.melodicRelease = true;
        settings.releaseHops = 8;          // 40ms consecutive quiet, independent of attack
        settings.maxUnvoicedHops = 15;     // 75ms: 40ms consonant + up to one ~33ms analysis window
        settings.quietRetriggerHops = 3;   // 15ms quiet arms articulation, without sending off yet
    }
    return settings;
}
