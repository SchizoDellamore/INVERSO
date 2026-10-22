# INVERSO DRUM 0.1 — sequencer + sample instrument

Independent **VST3 instrument** for Windows x64 / Cubase, with its own step sequencer. Inspired by the **workflow**, not the code or interface assets, of Loopcloud DRUM. Not affiliated with Loopcloud or Loopmasters.

## Included in 0.1
- **16 pads and 16 independent sequencer lanes**, each with 16 steps; 8 patterns A–H, switchable or chainable.
- **Host transport + BPM sync** via the host-provided PPQ and tempo. Sequencing starts when Cubase plays. MIDI notes C1 (36) through 51 also trigger pads; pad clicks preview the sounds while stopped.
- Adjustable **swing**, **velocity per step** (right-click successive values), and per-pad **level/pan**.
- Load/drag WAV/AIFF files to the selected pad, or start from 16 **procedurally synthesized factory sounds** (no third-party samples).
- **Export MIDI** for selected pattern or complete eight-pattern chain.
- State/pattern/knob/sample-path persistence in DAW project.
- Zero latency, fixed voice count of 64, zero dynamic allocation for scheduling/playback inside `processBlock` (except shared_ptr reference counting and releasing completed voices). Sound-file import occurs outside the callback.
- Dark UI with orange accents; grid designed to stay visible without decorative animations.

## Limitations / planned follow-ups
- Imported audio is referenced by original *file path*, not embedded in the Cubase session. Moving/deleting original WAV/AIFF will require reloading it. Up to 12 seconds of each sample are loaded, stereo or mono. No sample browser yet.
- No individual output busses, transient shaping, pitch, probability, rolls, per-step microtiming or drag-and-drop audio export in 0.1. Those are planned, not claimed as implemented.
- The factory sounds are basic synthesized placeholders, not a commercial acoustic drum library.
- Pattern exports are MIDI files via the EXPORT MIDI button; drag-and-drop MIDI export is not implemented yet.
- Start/stop and tempo follow the DAW. There is no free-running internal sequencer clock by design in this release.
- True-peak limiting is not included; mind the DAW output meter.
- Does not require Loopcloud subscription or cloud access.

## Build and validation
`cmake -S Drum -B Drum/build-portable -DINVERSO_DSP_TESTS_ONLY=ON && cmake --build Drum/build-portable && ctest --test-dir Drum/build-portable`

Windows GitHub Actions uses JUCE 8, VS2022, CMake and Inno Setup 6 to produce a Windows installer and portable VST3. Successful tests do not replace actual Cubase testing.
