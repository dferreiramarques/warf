# Warf (Beta)

Free JUCE 8 VST3 plugin that listens to a **monophonic** audio signal (voice, guitar/bass playing
single notes, a wind instrument, ...) and converts what it hears into MIDI note on/off events in
real time. Architecture and build setup are a direct port of [Kerf Lite](https://github.com/dferreiramarques/kerf/tree/main/plugin)'s
JUCE/CMake project (native GUI, `juce::UnitTest`-based console test runner, Inno Setup installer)
— no code is shared between the two, but the recipe is deliberately identical so anyone who's set
one up can set up the other.

## Read this before building anything

**Pitch detection is monophonic only.** Feed it a chord or more than one instrument at once and
it will report whichever note the algorithm judges dominant, chattering between notes rather than
tracking a chord — this is an inherent limitation of the approach (YIN-style autocorrelation), not
a bug to fix later. Point it at a single melodic line.

**A VST3 plugin outputting a separate MIDI stream from an audio input is a real but host-dependent
workflow.** Ableton Live's own "audio to MIDI" is a built-in DAW feature and does *not* apply to
third-party VST3 MIDI-output routing the way this plugin needs. Hosts that do support a VST3
effect placed on an audio track producing its own MIDI output bus for routing elsewhere (a virtual
MIDI port, a MIDI track, another instrument) include Cubase, Studio One, Bitwig, and REAPER. If
your DAW doesn't support that routing, Warf's audio pass-through still lets you hear the untouched
input, but there's nowhere for the MIDI to go — check your host's docs first.

## Architecture

- `src/PitchDetector.{h,cpp}` — real-time monophonic pitch tracker (YIN algorithm). Pushed mono
  audio accumulates into a ring buffer; every hop (a quarter of the analysis window) it reruns YIN
  over the last full window and reports a frequency + confidence + RMS. Window size (and therefore
  latency: ~46ms/23ms hop by default at 44.1kHz) scales with the lowest frequency you want to
  track — see the class comment for the tradeoff.
- `src/NoteTracker.{h,cpp}` — turns that stream of pitch estimates into actual MIDI note on/off
  events. Debounces onset jitter (`attackHops` consecutive stable hops before triggering),
  survives brief dropouts without ending a note (`releaseHops` consecutive silent hops before
  ending it), and retriggers cleanly when a legato phrase moves to a new note. See the class
  comment for the full state machine.
- `src/MidiNoteUtils.h` — plain frequency↔MIDI-note math, no JUCE dependency.
- `src/PluginProcessor.{h,cpp}` / `src/PluginEditor.{h,cpp}` — the JUCE plugin wrapper and a
  minimal native GUI (live note/frequency readout + the global parameters below). Mono-sums
  whatever's on the main input bus for analysis; the input passes through to the output dry and
  unmodified.
- `src/ParameterLayout.{h,cpp}` — the handful of genuinely host-automatable parameters: Sensitivity
  (a single friendly knob mapped internally onto confidence/tolerance/attack-speed), Gate
  Threshold, Transpose, MIDI Channel, and a Fixed Velocity toggle + value.
- `app.html` — a browser PWA with the same detection engine (a direct JS port of
  `PitchDetector.cpp` and `NoteTracker.cpp` — same algorithm, same state machine, kept in sync by
  hand since there's no shared code between C++ and JS). Captures the microphone via
  `getUserMedia`/`ScriptProcessorNode`, runs YIN pitch tracking in the browser, and sends the
  resulting MIDI notes to a Web MIDI output port. Chrome/Edge only (Web MIDI isn't supported in
  Firefox or Safari); needs a virtual MIDI port (e.g. loopMIDI on Windows) to actually reach a DAW.
- `index.html` — the product/landing page (served at warf.monco.io), linking to both the Windows
  VST3 installer and `app.html`.
- `manifest.json` / `service-worker.js` — PWA installability and offline caching for `app.html`.

## Prerequisites (same recipe as Kerf Lite)

1. An MSVC C++ toolset + Windows SDK (VS Build Tools 2019 or newer, or full Visual Studio).
2. CMake 3.22+: `winget install --id Kitware.CMake -e`.

## Build (VST3, Debug)

From a shell where `cmake` and `cl` are both reachable (a Developer Command Prompt, or full paths):
```
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Debug --target Warf_VST3
```
First configure clones JUCE 8.0.15 via FetchContent — takes a while, needs network access.
`COPY_PLUGIN_AFTER_BUILD` is on, so a successful build also installs to
`C:\Program Files\Common Files\VST3\Warf (Beta).vst3` automatically.

## Building the installer (for distribution)

Build Release first, then compile the Inno Setup script:
```
cmake --build build --config Release --target Warf_VST3
"C:\Users\<you>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\Warf.iss
```
Output lands in `downloads/Warf_<version>_Beta_VST3_x64-setup.exe`.

## Tests (no DAW needed)

`WarfTests` is a standalone console app running the engine's `juce::UnitTest` suite —
`PitchDetector` (accuracy against known-frequency sine waves) and `NoteTracker` (the onset/offset
debounce state machine) — independent of any host:
```
cmake --build build --config Debug --target WarfTests
build/WarfTests_artefacts/Debug/WarfTests.exe
```

## Status

`WarfTests` passes (25/25 assertions covering `PitchDetector` accuracy and `NoteTracker`'s
onset/offset state machine), and both `Warf_VST3` (Debug and Release) and `app.html` build/render
cleanly — the Release build is what `downloads/Warf_0.1.0_Beta_VST3_x64-setup.exe` and the
matching `.zip` in this repo were built from. **Still not verified against a real instrument or
voice in an actual DAW** — the automated tests check the algorithm against synthesized sine waves
and synthetic pitch sequences, not a live mic or the audio pass-through path with real host
automation. Try it in a host that supports VST3 MIDI-output routing (see the caveat above) before
trusting it for anything real.

Not started / explicitly out of scope for this version: polyphonic (chord) detection, pitch bend /
portamento output for slides, a waveform or pitch-history display, macOS/AU build, a native
(Tauri-style) desktop wrapper for the PWA.
