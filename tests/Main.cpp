#include <juce_core/juce_core.h>
#include <cstdio>

// Console runner for the engine's juce::UnitTest suite (PitchDetector/NoteTracker). Deliberately
// independent of any DAW: `cmake --build build --target WarfTests && build/WarfTests_artefacts/
// .../WarfTests` gives a fast, self-contained pass/fail signal for the audio-thread logic instead
// of needing to reload the plugin in a host for every change.
int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);
    runner.runAllTests();

    int totalPasses = 0, totalFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* result = runner.getResult (i);
        totalPasses += result->passes;
        totalFailures += result->failures;

        if (result->failures > 0)
        {
            std::printf ("FAILED: %s / %s (%d passes, %d failures)\n",
                         result->unitTestName.toRawUTF8(),
                         result->subcategoryName.toRawUTF8(),
                         result->passes, result->failures);

            for (const auto& message : result->messages)
                std::printf ("    %s\n", message.toRawUTF8());
        }
    }

    std::printf ("\n%d passed, %d failed\n", totalPasses, totalFailures);

    return totalFailures > 0 ? 1 : 0;
}
