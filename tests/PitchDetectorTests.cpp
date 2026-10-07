#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include "../src/PitchDetector.h"
#include "../src/TrackingSettings.h"
#include "../src/MidiNoteUtils.h"

namespace
{
    std::vector<float> makeSineWave (double frequencyHz, double sampleRate, int numSamples, float amplitude = 0.5f)
    {
        std::vector<float> data ((size_t) numSamples);
        for (int i = 0; i < numSamples; ++i)
            data[(size_t) i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * frequencyHz * (double) i / sampleRate);
        return data;
    }


    struct NoteEvent { int note; int sample; bool on; };

    std::vector<NoteEvent> trackSignal (const std::vector<float>& signal, double sampleRate, TrackingMode mode, int blockSize = 128)
    {
        PitchDetector detector;
        detector.prepare (sampleRate);
        detector.setFastTracking (mode != TrackingMode::stable);
        NoteTracker tracker;
        tracker.prepare (makeTrackingSettings (0.8f, mode));
        std::vector<NoteEvent> events;
        for (int start = 0; start < (int) signal.size(); start += blockSize)
        {
            juce::MidiBuffer midi;
            detector.pushSamples (signal.data() + start, std::min (blockSize, (int) signal.size() - start),
                [&] (const PitchDetector::Result& r, int offset) { tracker.processHop (r, offset, midi); });
            for (const auto event : midi)
            {
                const auto message = event.getMessage();
                events.push_back ({ message.getNoteNumber(), start + event.samplePosition, message.isNoteOn() });
            }
        }
        return events;
    }

    void appendTone (std::vector<float>& signal, double frequency, int length, double rate)
    {
        // Short amplitude ramps and harmonics approximate a voiced attack/release, without
        // claiming to model microphone recordings or vocal formants.
        for (int i = 0; i < length; ++i)
        {
            const double phase = 2.0 * juce::MathConstants<double>::pi * frequency * i / rate;
            const float envelope = (float) std::min ({ 1.0, i / (0.002 * rate), (length - 1 - i) / (0.002 * rate) });
            signal.push_back (envelope * (float) (0.3 * std::sin (phase) + 0.12 * std::sin (2.0 * phase)
                                                                      + 0.06 * std::sin (3.0 * phase)));
        }
    }

    // Runs the whole signal through the detector and returns the last hop reported as voiced -
    // by the last hop, the detector's ring buffer holds nothing but steady-state signal, so this
    // is the fairest check of steady-state accuracy (the very first hop or two can be less
    // accurate while the window is still filling with real content).
    bool lastVoicedResult (PitchDetector& detector, const std::vector<float>& signal, PitchDetector::Result& out)
    {
        bool found = false;
        detector.pushSamples (signal.data(), (int) signal.size(), [&] (const PitchDetector::Result& r, int)
        {
            if (r.hasPitch)
            {
                out = r;
                found = true;
            }
        });
        return found;
    }
}

class PitchDetectorTests final : public juce::UnitTest
{
public:
    PitchDetectorTests() : UnitTest ("PitchDetector", "Warf") {}

    void runTest() override
    {
        constexpr double sampleRate = 44100.0;

        beginTest ("Performance/Percussive detector retains low-frequency accuracy at 44.1, 48 and 96 kHz");
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
            for (const double frequency : { 60.0, 82.4069, 110.0, 220.0, 440.0, 1000.0 })
            {
                PitchDetector detector;
                detector.prepare (rate);
                const int stableWindow = detector.getWindowSize();
                detector.setFastTracking (true);
                expect (detector.getWindowSize() <= stableWindow);
                expectWithinAbsoluteError (detector.getHopSize() / rate, 0.005, 1.0 / rate);
                std::vector<float> signal;
                appendTone (signal, frequency, (int) (rate * 0.15), rate);
                PitchDetector::Result result;
                expect (lastVoicedResult (detector, signal, result));
                expect (std::abs (1200.0 * std::log2 (result.frequencyHz / frequency)) < 10.0,
                        juce::String (rate) + " Hz sample rate, tone " + juce::String (frequency));
            }

        beginTest ("Performance/Percussive detector switch retains history and callback sample offsets");
        {
            PitchDetector detector;
            detector.prepare (48000.0);
            const auto tone = makeSineWave (220.0, 48000.0, 4336);
            detector.pushSamples (tone.data(), 4096, [] (const auto&, int) {});
            detector.setFastTracking (true);
            int count = 0;
            detector.pushSamples (tone.data() + 4096, 240, [&] (const auto& result, int offset)
            {
                ++count;
                expect (result.hasPitch);
                expectEquals (offset, 239);
            });
            expectEquals (count, 1);
        }

        beginTest ("pipeline onset latency improves across hop phases at 48 kHz / 128 samples");
        for (const double frequency : { 110.0, 220.0, 440.0 })
        {
            int minFast = 100000, maxFast = 0, minStable = 100000, maxStable = 0;
            for (const int phase : { 0, 73, 239, 371, 511 })
            {
                const int onset = 4096 + phase;
                std::vector<float> signal ((size_t) onset, 0.0f);
                appendTone (signal, frequency, 4800, 48000.0);
                signal.resize (signal.size() + 4096, 0.0f);
                const auto fast = trackSignal (signal, 48000.0, TrackingMode::percussive);
                const auto performance = trackSignal (signal, 48000.0, TrackingMode::performance);
                expectEquals ((int) performance.size(), 2);
                if (! fast.empty() && ! performance.empty())
                {
                    expectEquals (performance.front().sample, fast.front().sample);
                    expectEquals (performance.front().note, fast.front().note);
                }
                const auto stable = trackSignal (signal, 48000.0, TrackingMode::stable);
                expectEquals ((int) fast.size(), 2);
                expectEquals ((int) stable.size(), 2);
                if (fast.empty() || stable.empty()) continue;
                expect (fast.front().on && ! fast.back().on);
                expectEquals (fast.front().note, (int) std::lround (MidiNoteUtils::midiNoteFromFrequency (frequency)));
                const int fastLatency = fast.front().sample - onset;
                const int stableLatency = stable.front().sample - onset;
                expect (fastLatency + 240 <= stableLatency, "Performance/Percussive should save at least 5ms");
                minFast = std::min (minFast, fastLatency); maxFast = std::max (maxFast, fastLatency);
                minStable = std::min (minStable, stableLatency); maxStable = std::max (maxStable, stableLatency);
            }
            logMessage ("Tone " + juce::String (frequency) + " Hz: Performance/Percussive " + juce::String (minFast / 48.0, 2)
                        + ".." + juce::String (maxFast / 48.0, 2) + " ms; Stable "
                        + juce::String (minStable / 48.0, 2) + ".." + juce::String (maxStable / 48.0, 2) + " ms");
        }

        beginTest ("pipeline captures 30ms harmonic notes and same-pitch staccato repetitions");
        for (const double frequency : { 110.0, 220.0, 440.0 })
        {
            std::vector<float> signal (4096, 0.0f);
            for (int i = 0; i < 4; ++i)
            {
                appendTone (signal, frequency, 1440, 48000.0);
                signal.resize (signal.size() + 1200, 0.0f);
            }
            signal.resize (signal.size() + 4096, 0.0f);
            const auto events = trackSignal (signal, 48000.0, TrackingMode::percussive);
            expectEquals ((int) events.size(), 8, juce::String (frequency) + " Hz");
            for (size_t i = 0; i < events.size(); ++i)
            {
                expectEquals (events[i].note, (int) std::lround (MidiNoteUtils::midiNoteFromFrequency (frequency)));
                expect (events[i].on == (i % 2 == 0));
            }
            const auto unevenBlocks = trackSignal (signal, 48000.0, TrackingMode::percussive, 73);
            expectEquals ((int) unevenBlocks.size(), (int) events.size());
            for (size_t i = 0; i < std::min (events.size(), unevenBlocks.size()); ++i)
                expectEquals (events[i].sample, unevenBlocks[i].sample);
        }

        beginTest ("Performance and Percussive follow 60ms legato notes without intermediate retriggers");
        for (const auto mode : { TrackingMode::performance, TrackingMode::percussive })
        {
            std::vector<float> signal (4096, 0.0f);
            double phase = 0.0;
            const std::vector<int> notes { 57, 59, 60, 57 };
            for (const int note : notes)
                for (int i = 0; i < 2880; ++i)
                {
                    signal.push_back ((float) (0.3 * std::sin (phase) + 0.12 * std::sin (2 * phase)));
                    phase += 2.0 * juce::MathConstants<double>::pi * MidiNoteUtils::frequencyFromMidiNote (note) / 48000.0;
                }
            signal.resize (signal.size() + 4096, 0.0f);
            const auto events = trackSignal (signal, 48000.0, mode);
            std::vector<int> detected;
            for (const auto& event : events)
                if (event.on) detected.push_back (event.note);
            expectEquals ((int) detected.size(), (int) notes.size());
            for (size_t i = 0; i < std::min (notes.size(), detected.size()); ++i)
                expectEquals (detected[i], notes[i]);
        }

        beginTest ("Performance audio pipeline bridges consonant-sized gaps and ends true silence");
        for (const double frequency : { 65.4064, 110.0, 220.0, 440.0 })
            for (const int gapMs : { 5, 10, 15, 20, 25, 30, 35, 40 })
                for (const bool noisy : { false, true })
                {
                    std::vector<float> signal (4096, 0.0f);
                    juce::Random random (34567);
                    for (int syllable = 0; syllable < 4; ++syllable)
                    {
                        appendTone (signal, frequency, 4800, 48000.0);
                        if (syllable < 3)
                            for (int i = 0; i < gapMs * 48; ++i)
                                signal.push_back (noisy ? 0.04f * (2.0f * random.nextFloat() - 1.0f) : 0.0f);
                    }
                    const int audioEnd = (int) signal.size();
                    signal.resize (signal.size() + 6000, 0.0f);
                    const auto events = trackSignal (signal, 48000.0, TrackingMode::performance);
                    const auto label = juce::String (frequency) + "Hz / " + juce::String (gapMs)
                                       + "ms / " + (noisy ? "noise" : "quiet");
                    expect (events.size() >= 2, label);
                    if (events.size() < 2) continue;
                    expect (events.front().on && ! events.back().on, label);
                    expect (events.back().sample >= audioEnd && events.back().sample <= audioEnd + 2400,
                            "True silence should end the note within 50ms: " + label);
                    for (size_t i = 0; i < events.size(); ++i)
                    {
                        expectEquals (events[i].note, (int) std::lround (MidiNoteUtils::midiNoteFromFrequency (frequency)), label);
                        expect (events[i].on == (i % 2 == 0), label);
                        // Exactly 40ms quiet reaches the intentional release boundary. Shorter
                        // quiet gaps and all tested noisy consonants must have no MIDI dead zone.
                        if (! events[i].on && i + 1 < events.size() && (noisy || gapMs < 40))
                            expectEquals (events[i].sample, events[i + 1].sample, "No MIDI gap: " + label);
                    }
                    // Clear quiet syllables must remain articulated; very short closures may
                    // intentionally merge, and noisy gaps alone should not force a retrigger.
                    if (! noisy && gapMs >= 20 && gapMs < 40)
                        expectEquals ((int) events.size(), 8, "Repeated same-pitch syllables: " + label);
                }

        beginTest ("Percussive pipeline rejects silence and deterministic broadband noise");
        {
            std::vector<float> signal (12000, 0.0f);
            expect (trackSignal (signal, 48000.0, TrackingMode::percussive).empty());
            juce::Random random (12345);
            for (auto& sample : signal)
                sample = 0.3f * (2.0f * random.nextFloat() - 1.0f);
            expect (trackSignal (signal, 48000.0, TrackingMode::percussive).empty());
        }


        beginTest ("detects a steady 440Hz sine within 10 cents");
        {
            PitchDetector detector;
            detector.prepare (sampleRate);
            const auto signal = makeSineWave (440.0, sampleRate, (int) (sampleRate * 2));

            PitchDetector::Result result;
            expect (lastVoicedResult (detector, signal, result));

            const auto cents = 1200.0 * std::log2 (result.frequencyHz / 440.0);
            expect (std::abs (cents) < 10.0);
        }

        beginTest ("detects a steady 110Hz sine (near a guitar low E) within 10 cents");
        {
            PitchDetector detector;
            detector.prepare (sampleRate);
            const auto signal = makeSineWave (110.0, sampleRate, (int) (sampleRate * 2));

            PitchDetector::Result result;
            expect (lastVoicedResult (detector, signal, result));

            const auto cents = 1200.0 * std::log2 (result.frequencyHz / 110.0);
            expect (std::abs (cents) < 10.0);
        }

        beginTest ("detects a steady 1000Hz sine within 10 cents");
        {
            PitchDetector detector;
            detector.prepare (sampleRate);
            const auto signal = makeSineWave (1000.0, sampleRate, (int) (sampleRate * 1));

            PitchDetector::Result result;
            expect (lastVoicedResult (detector, signal, result));

            const auto cents = 1200.0 * std::log2 (result.frequencyHz / 1000.0);
            expect (std::abs (cents) < 10.0);
        }

        beginTest ("reports no pitch on digital silence");
        {
            PitchDetector detector;
            detector.prepare (sampleRate);
            const std::vector<float> silence ((size_t) sampleRate, 0.0f);

            bool anyVoiced = false;
            detector.pushSamples (silence.data(), (int) silence.size(), [&] (const PitchDetector::Result& r, int)
            {
                if (r.hasPitch)
                    anyVoiced = true;
            });

            expect (! anyVoiced);
        }

        beginTest ("hop size and window size scale with a lower minFrequencyHz");
        {
            PitchDetector highFloor, lowFloor;
            highFloor.prepare (sampleRate, 200.0);
            lowFloor.prepare (sampleRate, 40.0);

            expect (lowFloor.getWindowSize() > highFloor.getWindowSize());
            expect (lowFloor.getHopSize() > highFloor.getHopSize());
        }
    }
};

static PitchDetectorTests pitchDetectorTests;
