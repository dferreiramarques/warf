#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include "../synth-src/SynthVoice.h"
#include "../synth-src/SynthSound.h"

namespace
{
    juce::ADSR::Parameters fastAdsr (float release = 0.01f)
    {
        juce::ADSR::Parameters p;
        p.attack = 0.001f;
        p.decay = 0.01f;
        p.sustain = 1.0f;
        p.release = release;
        return p;
    }

    float maxAbsSample (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        float maxAbs = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            maxAbs = std::max (maxAbs, std::abs (buffer.getSample (channel, i)));
        return maxAbs;
    }
}

class SynthVoiceTests final : public juce::UnitTest
{
public:
    SynthVoiceTests() : UnitTest ("SynthVoice", "Warf") {}

    void runTest() override
    {
        beginTest ("a started note renders non-silent audio");
        {
            WarfSynthVoice voice;
            voice.setCurrentPlaybackSampleRate (44100.0);
            voice.setAdsrParameters (fastAdsr());
            voice.setWaveform (Waveform::sine);

            WarfSynthSound sound;
            voice.startNote (69, 1.0f, &sound, 0);

            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();
            voice.renderNextBlock (buffer, 0, 512);

            expect (maxAbsSample (buffer) > 0.01f);
        }

        beginTest ("stopNote without tail-off silences the voice immediately");
        {
            WarfSynthVoice voice;
            voice.setCurrentPlaybackSampleRate (44100.0);
            voice.setAdsrParameters (fastAdsr (0.5f)); // long release - would still be audible if tail-off ran
            voice.setWaveform (Waveform::square);

            WarfSynthSound sound;
            voice.startNote (60, 1.0f, &sound, 0);

            juce::AudioBuffer<float> warmup (2, 256);
            warmup.clear();
            voice.renderNextBlock (warmup, 0, 256);

            voice.stopNote (1.0f, false);

            juce::AudioBuffer<float> after (2, 256);
            after.clear();
            voice.renderNextBlock (after, 0, 256);

            expectWithinAbsoluteError (maxAbsSample (after), 0.0f, 1.0e-6f);
        }

        beginTest ("a released note eventually goes silent on its own");
        {
            WarfSynthVoice voice;
            voice.setCurrentPlaybackSampleRate (44100.0);
            voice.setAdsrParameters (fastAdsr (0.01f));
            voice.setWaveform (Waveform::sine);

            WarfSynthSound sound;
            voice.startNote (69, 1.0f, &sound, 0);

            juce::AudioBuffer<float> sounding (2, 256);
            sounding.clear();
            voice.renderNextBlock (sounding, 0, 256);
            expect (maxAbsSample (sounding) > 0.01f);

            voice.stopNote (1.0f, true); // allow tail-off

            // 0.01s release at 44.1kHz is ~441 samples - render well past that. The buffer's early
            // samples are the release curve itself (not yet silent, by design), so only the tail
            // end - well after the release should have finished - is checked for silence.
            juce::AudioBuffer<float> afterRelease (2, 4096);
            afterRelease.clear();
            voice.renderNextBlock (afterRelease, 0, 4096);

            float tailMaxAbs = 0.0f;
            for (int i = afterRelease.getNumSamples() - 512; i < afterRelease.getNumSamples(); ++i)
                tailMaxAbs = std::max (tailMaxAbs, std::abs (afterRelease.getSample (0, i)));
            expectWithinAbsoluteError (tailMaxAbs, 0.0f, 1.0e-6f);
        }

        beginTest ("every waveform stays within -1..1 before level/envelope scaling is even applied");
        {
            for (auto wf : { Waveform::sine, Waveform::saw, Waveform::square, Waveform::triangle })
            {
                WarfSynthVoice voice;
                voice.setCurrentPlaybackSampleRate (44100.0);
                voice.setAdsrParameters (fastAdsr());
                voice.setWaveform (wf);

                WarfSynthSound sound;
                voice.startNote (69, 1.0f, &sound, 0);

                juce::AudioBuffer<float> buffer (2, 2048);
                buffer.clear();
                voice.renderNextBlock (buffer, 0, 2048);

                // 0.3f headroom factor in renderNextBlock means output should never exceed that,
                // even with a full-scale velocity and a fully-open envelope.
                expect (maxAbsSample (buffer) <= 0.3f + 1.0e-4f);
            }
        }

        beginTest ("a voice that can't play a foreign sound type is rejected");
        {
            WarfSynthVoice voice;
            struct OtherSound final : public juce::SynthesiserSound
            {
                bool appliesToNote (int) override { return true; }
                bool appliesToChannel (int) override { return true; }
            } other;

            expect (! voice.canPlaySound (&other));

            WarfSynthSound ownSound;
            expect (voice.canPlaySound (&ownSound));
        }
    }
};

static SynthVoiceTests synthVoiceTests;
