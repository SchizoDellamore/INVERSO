# INVERSO BASS DRIVE v0.3 (sonic redesign)

## Why the engine changed

The previous build was stable but its 4th-order crossover starved the drive stage of low-mid signal. High drive values also lost weight owing to strong compression. v0.3 replaces the character engine while retaining the safe, block-size-independent, zero-latency JUCE wrapper of v0.2.

**Core controls:** DRIVE (asymmetric harmonics with some direct signal preserved), BODY (adds controlled 65-310 Hz body when processing is active), CROSSOVER (60-500 Hz; 2nd-order high-pass into distortion), PRESERVE LOW (default ON), MIX, OUTPUT.

**Optional Fuzz:** Mode Off/Vintage/Silicon/Gated, Fuzz Amount, Fuzz Tone. Fuzz is OFF by default, and **DRIVE works independently of fuzz**. Fuzz Amount works even with Drive at zero if the mode is selected. Vintage is asymmetric and rounded, Silicon is harder and harmonically aggressive, Gated intentionally truncates tails.

The lower path is reconstructed as input minus high-pass, avoiding holes at the crossover frequency. The body control adds a band-limited 65-310 Hz signal, not a sub-bass boost; it is inactive when Drive and Fuzz are off. All parameter values except new BODY retain their VST3 IDs and ranges for session compatibility.

**IMPORTANT LIMITATIONS:** Host-rate waveshaping is used for stability; this version does not use oversampling, so extreme distortion can alias, particularly at 44.1/48kHz. The output is **not limited**; adjust Output to prevent clipping. This is a custom musical design, not a circuit-accurate pedal emulation. Quantitative harmonic tests do not establish subjective quality. It must be heard in Cubase to confirm that the new sound fits the intended instrument.

## Verification

Portable C++ tests: harmonic generation from moderate inputs; drive as standalone processor; body at ~120 Hz; three distinctly aggressive fuzz sounds with Drive=0; no output silence; automation/stress tests (16,32,64 simulated instances at 44.1/48/96 kHz); normal bypass/dry throughput.

Windows JUCE host smoke test: mono/stereo buffers, varying callback sizes, bypass, 44.1/48/96kHz, save/restore. Builds through CMake/JUCE 8 and Inno Setup 6. The installer and portable VST3 are published separately.

### Trial settings
- Warm bass: Drive 45%, Body 65%, Crossover 135 Hz, Fuzz Off, Mix 100%, Output adjusted down to avoid clipping.
- Dense drive: Drive 70%, Body 50%, Crossover 100 Hz, Fuzz Off.
- Fuzz: Drive 20%, Body 60%, Crossover 120 Hz, choose Vintage / Silicon / Gated, Fuzz Amount 55-80%; adjust Fuzz Tone to taste, lower Output.
