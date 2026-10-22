# INVERSO DRUM 0.2 — independent step sequencer and drum sampler

VST3 instrument for Windows 64-bit / Cubase. Independent from Loopcloud services; inspired by the **workflow**, not proprietary code or audio libraries. The original Loopcloud company is not affiliated with this product.

## Sequencer
- **16 independent tracks/pads**, **1–128 steps per track**, **eight patterns A–H**.
- Grid shows 16 steps at a time, with **eight page buttons** covering steps 1–128. Darker cells are outside the individual lane length.
- Every lane has its own step duration (**1/4, 1/8, 1/16, 1/32**) and rhythmic feel (**Straight, Dotted ×1.5, Triplet ×2/3**).
- BPM/transport locked to Cubase PPQ. Swing affects alternating steps. Pattern A–H can be selected or chained. When CHAIN is enabled, pattern length expands to the longest lane cycle, rounded up to a four-quarter-note bar, allowing long sequences to complete before switching. In selected-pattern mode each lane repeats at its own length.
- Click a step to toggle; right-click to cycle velocity. MIDI notes 36–51 trigger the 16 pads. EXPORT MIDI outputs the selected or chained pattern, including per-lane subdivisions, swing and velocities.

## Per-pad sample editing (nondestructive)
- Load WAV/AIFF for each pad independently; built-in synthetic factory samples let the plugin play without third-party content.
- SAMPLE EDIT tab: waveform overview, **Start / End** sample trim, **Pitch −24 to +24 semitones**, **Reverse**, **Attack**, **Decay**, level and pan. The original audio file is never overwritten.
- Trim/start/end, pitch, and reverse settings apply when a voice is triggered. Adjusting the sample's settings does not retroactively change notes already playing.

## Effects
- EFFECTS tab: per-pad **Drive, Reverb Send and Delay Send**.
- Global **Reverb Mix / Reverb Tail**, **Delay Mix / Delay Feedback**, delay note division 1/4 through 1/32 and straight/dotted/triplet timing, synced to Cubase BPM.
- Simple algorithmic stereo reverb and feedback delay use buffers allocated **only during prepareToPlay**, not on the audio thread. Drive is a basic nonlinear waveshaper per pad (not a Darkglass amp model).
- No true-peak limiter: effects and layered samples can clip. Monitor the Cubase master and reduce pad gains/effect sends if required.

## Compatibility and honest limitations
- **State version 2**, reads 0.1 patterns (16 steps) and sample paths, preserving compatible settings. The saved project stores file **paths**, not embedded WAV/AIFF data. Keep imported samples in a fixed folder.
- Zero reported latency. Resampling is linear; pitch/reverse and high-frequency drive can alias. Time-stretch, per-step microtiming/probability, multiple DAW output buses, audio drag/export, and host-automation exposure of every per-pad control are not included in 0.2.
- Chain length assumes a four-quarter-note musical bar for alignment; non-4/4 projects may need future time-signature-aware behavior.
- Native/plugin JUCE tests and DSP tests check function and stability, not professional mix quality or behavior in a loaded Cubase project.
- Installer is unsigned; ZIP portable VST3 remains an option if the `.exe` encounters browser reputation warnings.

## Build
`cmake -S Drum -B Drum/build-test -DINVERSO_DSP_TESTS_ONLY=ON && cmake --build Drum/build-test && ctest --test-dir Drum/build-test --output-on-failure`

GitHub Actions builds the actual Windows VST3 and installer with JUCE 8 + Visual Studio 2022 + Inno Setup 6, and runs the JUCE host smoke test before publishing. Do not describe a build as verified until GitHub Actions has completed successfully.
