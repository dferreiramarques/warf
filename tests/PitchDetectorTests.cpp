#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include "../src/PitchDetector.h"

namespace
{
    std::vector<float> makeSineWave (double frequencyHz, double sampleRate, int numSamples, float amplitude = 0.5f)
    {
        std::vector<float> data ((size_t) numSamples);
        for (int i = 0; i < numSamples; ++i)
            data[(size_t) i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * frequencyHz * (double) i / sampleRate);
        return data;
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
