# INVERSO DE-MUDDER + DE-ANNOYANCE (v0.1.1)

Two **independent VST3 audio processors**, installed together. **INVERSO EQ remains installed and unchanged.**

## Philosophy

These are light-weight, dynamic, multi-band suppressors with stereo-linked detectors. They respond to the ratio of energy in each targeted band to overall signal energy, instead of static cuts at fixed dB values. They are **not** trained AI systems, psychoacoustic annoyance classifiers, or frequency-tracking resonant peak hunters. Their bank centres are fixed except for the global FOCUS shift.

### DE-MUDDER
- 3 overlapping bandpass detectors centered near **160 / 275 / 440 Hz**, with a modest Q.
- Intended for dynamic cleaning of low-mid buildup. Can reduce body and warmth at high settings.

### DE-ANNOYANCE
- 7 narrower detector bands covering approximately **1.15–8.2 kHz**.
- Intended to tame persistent painful/strident mid-high energy. It does not identify a perceptually unpleasant frequency automatically and can damp desired presence.

### Controls
- **AMOUNT** (0-100%): maximum strength.
- **SENSITIVITY** (0-100%): how readily attenuation begins.
- **FOCUS** (0-100%): shifts the detector bank below or above its nominal frequencies; 50% = nominal.
- **BYPASS**: smooth bypass.
- **LISTEN REMOVED**: audition what the processor suppresses; output may sound very quiet and is not level matched.
- **Shift + mouse wheel**: finer adjustment.
- Bar meters show estimated attenuation per fixed band, not a real-time FFT spectrum.

Default amount is 45%, sensitivity 50%, focus 50%. **Zero-latency** causal filters. No automatic output gain or clipping protection is included in these two processors; place your INVERSO EQ / meter or a safety stage after them when needed. Use them at modest intensity and audition in context.

## Development and tests

- C++17 JUCE 8, Windows 64-bit, VST3.
- Portable DSP: `cmake -S . -B build-test -DINVERSO_DSP_TESTS_ONLY=ON && cmake --build build-test && ctest --test-dir build-test --output-on-failure`
- Windows build: `BUILD_PROCESSORS.ps1 -JucePath C:\path\to\JUCE` using Visual Studio 2022, Inno Setup 6, and CMake.
- Two source-compatible plugin targets compile the same shared processor and DSP with different target constants and unique VST3 plugin IDs.
- Uninstaller only removes the two processors; it does not remove INVERSO EQ.

**Limitations:** tests do not establish professional mastering transparency or optimized CPU use across a loaded DAW project. Subjective effectiveness requires listening comparisons and measurement in the Cubase host. We will not claim these are equivalent to FabFilter/Soothe or automated resonant detection. No third-party binaries or JUCE source are included; license obligations for JUCE must be reviewed before distribution.

## 0.1.1: LISTEN REMOVED correction

- Corrected an internal control-precedence bug: enabling BYPASS and LISTEN REMOVED at the same time no longer sends the complete dry instrument; LISTEN REMOVED takes priority.
- The audition signal is calculated as `input - processed`; no loudness compensation is applied.
- A contrasting indicator in the bar panel makes the active audition mode visible, without adding a new large control.
- Added test `processor_delta` verifying the null/difference signal and bypass coexistence; now three DSP tests in the Windows pipeline.
- **Note**: Cubase's *host-level* bypass or parallel dry/wet routing can still pass unaffected audio around the plugin; this is outside the plugin's control. Listening to the removed signal does not necessarily make the source unrecognizable, because the removed frequencies can contain harmonics from the same instrument.


## Standalone installers (distribution revision)
The project also builds independent Windows installers `INVERSO_DE_MUDDER_Setup_0.1.1_x64.exe` and `INVERSO_DE_ANNOYANCE_Setup_0.1.1_x64.exe`, in addition to the combined installer and the portable VST3 package. Installers are unsigned and could still trigger browser reputation warnings; the portable VST3 is an alternative. If migrating from the combined installer, uninstall it first with Cubase closed, then install individual versions to avoid shared file ownership conflicts.
