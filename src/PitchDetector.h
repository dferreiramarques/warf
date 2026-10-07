#pragma once

#include <vector>
#include <cstddef>

// Real-time monophonic pitch tracker using the YIN algorithm (de Cheveigne & Kawahara, 2002).
// Audio is pushed one processBlock's worth at a time; internally it accumulates into a ring
// buffer, and every `hopSize` samples it reruns YIN over the last `windowSize` samples. All
// buffers are sized once in `prepare()` - `pushSamples()` never allocates, so it's safe to call
// directly from the audio thread.
//
// `windowSize` (and therefore latency and CPU cost) is derived from `minFrequencyHz`: tracking
// lower notes needs a longer window to see at least two full periods. At the default 60Hz floor
// and a 44.1kHz sample rate, Stable uses a 46ms window / 11.6ms hop. Performance and Percussive
// share an unpadded two-period window (~33ms at 60Hz) and 5ms hops; this implementation uses
// no FFT and does not require power-of-two padding. Both modes anchor YIN comparisons in the
// newest half-window to favour recent audio.
class PitchDetector
{
public:
    struct Result
    {
        bool hasPitch = false;   // false when confidence was too low - unvoiced, silence, or noise
        double frequencyHz = 0.0;
        double confidence = 0.0; // 0..1, higher means more certain
        float recentRms = -1.0f; // latest hop RMS; negative means unavailable (synthetic/legacy callers)
        float rms = 0.0f;        // RMS of the analysed window - used for the noise gate and velocity
    };

    void prepare (double sampleRate, double minFrequencyHz = 60.0);

    // Selects the shared Performance/Percussive detector configuration when true, Stable
    // when false. Buffers/history are retained, with no allocation or warm-up reset.
    void setFastTracking (bool fast) { fastTracking = fast; }
    int getHopSize() const { return fastTracking ? fastHopSize : hopSize; }
    int getWindowSize() const { return fastTracking ? fastWindowSize : windowSize; }

    // Pushes `numSamples` of mono audio. Calls `callback(result, sampleIndexWithinThisCall)` once
    // per hop boundary crossed - zero or more times depending on how many hops fit in this call -
    // with the index letting callers place any resulting MIDI events at roughly the right sample
    // offset within their own processBlock buffer, rather than lumping everything at the start.
    template <typename Callback>
    void pushSamples (const float* samples, int numSamples, Callback&& callback)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            ringBuffer[(size_t) writePos] = samples[i];
            writePos = (writePos + 1) % windowSize;
            ++samplesSinceLastHop;

            if (samplesWritten < windowSize)
                ++samplesWritten;

            if (samplesSinceLastHop >= getHopSize() && samplesWritten >= getWindowSize())
            {
                samplesSinceLastHop = 0;
                callback (analyseCurrentWindow(), i);
            }
        }
    }

private:
    Result analyseCurrentWindow();

    std::vector<float> ringBuffer;
    std::vector<float> analysisWindow; // linearised copy of ringBuffer, oldest sample first
    std::vector<float> cmndScratch;    // cumulative mean normalised difference function, reused per hop

    int windowSize = 2048;
    int hopSize = 512;
    int fastWindowSize = 1604;
    int fastHopSize = 240;
    bool fastTracking = false;
    int writePos = 0;
    int samplesSinceLastHop = 0;
    int samplesWritten = 0;
    double sampleRate = 44100.0;

    static constexpr double yinThreshold = 0.15;
};
