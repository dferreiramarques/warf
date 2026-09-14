#include "PitchDetector.h"
#include <algorithm>
#include <cmath>

namespace
{
    int nextPowerOfTwo (int n)
    {
        int p = 1;
        while (p < n)
            p *= 2;
        return p;
    }
}

void PitchDetector::prepare (double newSampleRate, double minFrequencyHz)
{
    sampleRate = newSampleRate;

    // Need at least two full periods of the lowest trackable frequency inside the window for YIN's
    // difference function to find a reliable minimum.
    const auto maxPeriodSamples = (int) std::ceil (sampleRate / minFrequencyHz);
    windowSize = nextPowerOfTwo (maxPeriodSamples * 2);
    hopSize = windowSize / 4;

    ringBuffer.assign ((size_t) windowSize, 0.0f);
    analysisWindow.assign ((size_t) windowSize, 0.0f);
    cmndScratch.assign ((size_t) (windowSize / 2), 1.0f);

    writePos = 0;
    samplesSinceLastHop = 0;
    samplesWritten = 0;
}

PitchDetector::Result PitchDetector::analyseCurrentWindow()
{
    for (int i = 0; i < windowSize; ++i)
        analysisWindow[(size_t) i] = ringBuffer[(size_t) ((writePos + i) % windowSize)];

    double sumSquares = 0.0;
    for (float s : analysisWindow)
        sumSquares += (double) s * (double) s;
    const auto rms = (float) std::sqrt (sumSquares / (double) windowSize);

    const auto tauMax = (int) cmndScratch.size();

    // Step 1+2 combined: difference function d(tau), accumulated straight into the cumulative
    // mean normalised difference function d'(tau) = d(tau) * tau / sum(d(1..tau)) so there's no
    // need to keep the raw difference values around afterwards.
    cmndScratch[0] = 1.0f;
    double runningSum = 0.0;
    for (int tau = 1; tau < tauMax; ++tau)
    {
        double diff = 0.0;
        for (int j = 0; j < tauMax; ++j)
        {
            const auto delta = (double) analysisWindow[(size_t) j] - (double) analysisWindow[(size_t) (j + tau)];
            diff += delta * delta;
        }

        runningSum += diff;
        cmndScratch[(size_t) tau] = runningSum > 0.0 ? (float) (diff * (double) tau / runningSum) : 1.0f;
    }

    // Step 3: absolute threshold - the first local minimum that dips below yinThreshold.
    int tauEstimate = -1;
    for (int tau = 2; tau < tauMax - 1; ++tau)
    {
        if (cmndScratch[(size_t) tau] < yinThreshold)
        {
            while (tau + 1 < tauMax && cmndScratch[(size_t) (tau + 1)] < cmndScratch[(size_t) tau])
                ++tau;
            // Clamped so the parabolic interpolation below can always safely read tau-1 and tau+1.
            tauEstimate = std::min (tau, tauMax - 2);
            break;
        }
    }

    Result result;
    result.rms = rms;

    if (tauEstimate <= 0)
    {
        // Nothing crossed the threshold - report the best minimum found anyway (as a confidence
        // figure only) so a caller could log/display it, but don't claim a pitch.
        const auto best = std::min_element (cmndScratch.begin() + 2, cmndScratch.end() - 1);
        result.confidence = best != cmndScratch.end() ? std::clamp (1.0 - (double) *best, 0.0, 1.0) : 0.0;
        result.hasPitch = false;
        return result;
    }

    // Step 4: parabolic interpolation around tauEstimate for sub-sample period precision.
    auto betterTau = (double) tauEstimate;
    const auto x0 = cmndScratch[(size_t) (tauEstimate - 1)];
    const auto x1 = cmndScratch[(size_t) tauEstimate];
    const auto x2 = cmndScratch[(size_t) (tauEstimate + 1)];
    const auto denom = (double) (2.0f * x1 - x2 - x0);
    if (std::abs (denom) > 1.0e-9)
        betterTau += 0.5 * ((double) x0 - (double) x2) / denom;

    result.frequencyHz = sampleRate / betterTau;
    result.confidence = std::clamp (1.0 - (double) x1, 0.0, 1.0);
    result.hasPitch = true;
    return result;
}
