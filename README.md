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

**Getting Warf's MIDI to a synth has gone through several iterations - this is the current, working
understanding, from an actual user's hands-on testing, not just what the code intends.** Warf stays
a normal Fx-category effect, inserted on an audio track like any other effect (an earlier attempt
recategorised it as an "Instrument," which broke that and got reverted). It has **two independent
MIDI output paths**, both active at once:

1. **The VST3 MIDI output bus** (`producesMidi() == true`). Confirmed load-bearing: a real user's
   working setup uses Studio One's own **"Instrument Input"** track routing - a separate
   Instrument/MIDI track's input set to Warf's audio track - which reads this bus directly. This
   works with Warf in its normal Fx category; recategorising as "Instrument" was never actually
   necessary for it. `WarfSynth` (below) is meant to be loaded on that receiving track.
2. **A MIDI Output Device picker** in Warf's own editor (`juce::MidiOutput`, enumerated and chosen
   from a dropdown - see `PluginProcessor::getMidiOutputDeviceNames()`/`setMidiOutputDeviceByIndex()`),
   sending directly to a system MIDI port. Useful for external hardware, or a virtual MIDI port
   (Windows has none built in - [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)
   is a free one) that something else is listening to. Independent of path 1 - works even in hosts
   that don't support Instrument Input-style routing at all.

**Known open issue: an unexplained extra "piano" sound reported alongside the intended synth**,
matching Windows' built-in Microsoft GS Wavetable Synth's default patch (Acoustic Grand Piano) -
observed with the MIDI Output Device picker set to "None," which ruled that path out as the source.
A first fix attempt removed the VST3 MIDI bus entirely on the theory that Studio One auto-previews
an Fx's declared MIDI output through its own default GM softsynth - this did stop the reported
sound, but it also broke Instrument Input routing, which turned out to be load-bearing for the
user's actual setup, so it was reverted. **The real source of the phantom piano is still
unconfirmed** - re-verify whether it's still present with the MIDI bus restored, and if so, look
for a Studio One-side per-track MIDI monitoring/output-device setting (separate from Warf's own
picker) before touching this again.

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
  minimal native GUI (live note/frequency readout, the MIDI Output Device picker, and the global
  parameters below). Mono-sums whatever's on the main input bus for analysis; the input passes
  through to the output dry and unmodified. Every generated note goes to both the host's own VST3
  MIDI bus and, if one's selected, directly to a system MIDI port via `juce::MidiOutput` - see the
  "read this before building" note above for why there are two paths and what's still unresolved.
- `src/ParameterLayout.{h,cpp}` — the handful of genuinely host-automatable parameters: Sensitivity
  (a single friendly knob mapped internally onto confidence/tolerance/attack-speed), Gate
  Threshold, Transpose, MIDI Channel, and a Fixed Velocity toggle + value. The MIDI Output Device
  selection is deliberately *not* one of these (it's a device choice, not something a DAW would
  automate) - it's persisted separately, as an extra attribute on the saved state's XML.
- `synth-src/` (`WarfSynth` target) — a small, deliberately unambitious companion instrument: one
  selectable oscillator waveform (`SynthVoice.cpp`'s `renderSample()`) through a `juce::ADSR`, built
  on `juce::Synthesiser`/`SynthesiserVoice` rather than anything from Warf's own engine. A plain
  MIDI-in/audio-out Instrument, no audio input bus at all - load it on whichever track's MIDI input
  is set to listen to the same port Warf is sending to.
- `app.html` — a browser PWA with the same detection engine (a direct JS port of
  `PitchDetector.cpp` and `NoteTracker.cpp` — same algorithm, same state machine, kept in sync by
  hand since there's no shared code between C++ and JS). Two ways to use it:
  - **Live**: captures the microphone via `getUserMedia`/`ScriptProcessorNode`, runs YIN pitch
    tracking in the browser, and sends the resulting MIDI notes to a Web MIDI output port.
    Chrome/Edge only (Web MIDI isn't supported in Firefox or Safari); needs a virtual MIDI port
    (e.g. loopMIDI on Windows) to actually reach a DAW.
  - **Record**: a second, independent control next to Start Listening. Start Recording captures
    the same live session's note events (starting the mic itself if it isn't already running) into
    a separate `NoteTracker` instance with its own fresh state, so a note already sounding before
    the recording began doesn't leak into it. Stop Recording flushes any still-sounding note,
    builds a downloadable MIDI file from what was captured, and - only if recording was the one
    that started the mic - stops the mic too (if you were already Listening first, Stop Recording
    leaves that session running). No MIDI output device needed for this path either.
  - **File**: upload a WAV (or anything `AudioContext.decodeAudioData` supports), and it runs the
    same engine offline over the whole buffer — processed in chunks via `setTimeout` so a long
    file doesn't freeze the tab — then writes the resulting notes out as a downloadable Standard
    MIDI File (format 0, fixed 120 BPM / 480 ticks-per-quarter, written by hand with no library).
    Uses whichever Detection settings (Sensitivity/Gate/Transpose/Channel/Velocity) are currently
    set on the page. No MIDI output device needed for this path.
- `index.html` — the product/landing page (served at warf.monco.io), linking to the Windows VST3
  installer, the Tauri desktop app, and `app.html`.
- `manifest.json` / `service-worker.js` — PWA installability and offline caching for `app.html`.
- `src-tauri/` — a Tauri desktop wrapper around `app.html` (not the VST3 plugin), same recipe as
  Kerf's own desktop app: `scripts/copy-frontend-for-tauri.js` copies `app.html` (renamed to
  `index.html`, which is what Tauri's `frontendDist` expects) plus `manifest.json` into
  `src-tauri/dist/` before each build. No file-system or MIDI Tauri plugins - `getUserMedia` and
  `navigator.requestMIDIAccess()` both work directly in Tauri's WebView2 on Windows, confirmed by
  Kerf's own desktop app already relying on Web MIDI for its input the same way.

## Prerequisites (same recipe as Kerf Lite)

1. An MSVC C++ toolset + Windows SDK (VS Build Tools 2019 or newer, or full Visual Studio).
2. CMake 3.22+: `winget install --id Kitware.CMake -e`.

## Build (VST3, Debug)

From a shell where `cmake` and `cl` are both reachable (a Developer Command Prompt, or full paths):
```
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Debug --target Warf_VST3
cmake --build build --config Debug --target WarfSynth_VST3
```
First configure clones JUCE 8.0.15 via FetchContent — takes a while, needs network access.
`COPY_PLUGIN_AFTER_BUILD` is on for both targets, so a successful build also installs to
`C:\Program Files\Common Files\VST3\Warf (Beta).vst3` and `...\Warf Synth (Beta).vst3` automatically
— close any host that has either one loaded first, or the copy step fails with "Permission denied"
(a locked file, not a build error - happened during this exact rework because Studio One was open).

## Building the desktop app (Tauri)

```
npm install
npm run tauri build
```
Output lands in `src-tauri/target/release/bundle/` (an NSIS `-setup.exe` and an `.msi`, same as
Kerf's own desktop app produces) - copy whichever you're distributing into `downloads/`.

## Building the installer (for distribution)

Build Release for both plugins first, then compile the Inno Setup script - it bundles both into
one installer:
```
cmake --build build --config Release --target Warf_VST3
cmake --build build --config Release --target WarfSynth_VST3
"C:\Users\<you>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\Warf.iss
```
Output lands in `downloads/Warf_<version>_Beta_VST3_x64-setup.exe`.

## Tests (no DAW needed)

`WarfTests` is a standalone console app running the engine's `juce::UnitTest` suite —
`PitchDetector` (accuracy against known-frequency sine waves), `NoteTracker` (the onset/offset
debounce state machine), and `SynthVoice` (waveform range, envelope on/off/tail-off behaviour) —
independent of any host:
```
cmake --build build --config Debug --target WarfTests
build/WarfTests_artefacts/Debug/WarfTests.exe
```

## Status

`WarfTests` passes (35/35 assertions: `PitchDetector` accuracy, `NoteTracker`'s onset/offset state
machine, and `SynthVoice`'s waveform/envelope behaviour). `Warf_VST3`, `WarfSynth_VST3`, and
`app.html` build/render cleanly in both Debug and Release, and the Tauri desktop app builds clean
too (`npm run tauri build`, both the `.msi` and NSIS `.exe` bundles) - the Release/bundle builds are
what everything in `downloads/` was built from. Confirmed via a real Studio One 5 install (checking
`Plugins-en.settings` directly, each time this rework changed) that Warf registers as
`category="AudioEffect"` - a normal audio-effect insert, as intended - and isn't blacklisted; also
confirmed by launching the built `warf.exe` directly that `app.html` renders correctly inside the
Tauri window (a `PrintWindow` capture, since there's no interactive desktop session in this dev
loop). This has genuinely been exercised end-to-end by a real user, not just built and assumed
working: Instrument Input routing from Warf's track into a synth (Mojito, Warf Synth) has been
confirmed working when the VST3 MIDI bus is declared, and confirmed broken when it isn't - that's
exactly why it's back. **Still open: an extra "piano" sound reported alongside the intended synth**,
not yet root-caused - see the "read this before building" note above for the current understanding
and what to check next before changing this area again. The MIDI Output Device picker's direct
`juce::MidiOutput` path hasn't been separately exercised against a real output port.

Not started / explicitly out of scope for this version: polyphonic (chord) detection, pitch bend /
portamento output for slides, a waveform or pitch-history display, macOS/AU build.
