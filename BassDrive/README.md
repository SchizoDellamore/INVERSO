# INVERSO BASS DRIVE v0.1

Standalone Windows x64 **VST3** designed for bass guitar and bass synth, to complement rather than overwrite INVERSO EQ and INVERSO Processors.

## Signal path

Audio input (mono or stereo) -> 4× oversampling (JUCE half-band IIR) -> adjustable 4th order high-pass split -> saturate the high band -> sum with complementary clean low band -> dry/wet blend -> output gain -> oversampling downfilter.

- **DRIVE**: non-Fuzz warm saturation; works even in fuzz mode Off.
- **FUZZ AMOUNT**: mix toward a fuzz character; **0%** removes fuzz regardless of mode.
- **FUZZ CHARACTER**: Off, Vintage (asymmetric soft clipping), Silicon (firmer symmetrical clipping), Gated (threshold-dependent gate on the distorted signal). These are algorithms inspired by common characteristics, **not authentic circuit/pedal models**.
- **CROSSOVER**: 60–500 Hz; low band preserved by default; the low signal is defined as input minus high-pass branch, so it reconstructs exactly before saturation.
- **PRESERVE LOW**: enabled by default; switching off processes the full signal through drive/fuzz.
- **FUZZ TONE**: high-frequency rolloff of the fuzz character only.
- **MIX**: wet/dry blend. **0%** is unprocessed audio, except for selected Output gain and the 4× oversampling reconstruction.
- **OUTPUT**: -24 to +12 dB.
- **SHIFT + mouse wheel**: finer editing on sliders.
- **Peak alert**: host output/sample-domain and internal 4x oversampling peak readouts; these are indicative only, not certified true-peak measurement.

Parameter transition smoothing is approximately 20 ms. Latency from oversampling is reported to the host for compensation. Gated mode can intentionally truncate sustain; avoid excessive amounts where that effect is unwanted.

## Installation

The workflow exports **both** a Windows Inno Setup installer and a **portable VST3-only ZIP**. Earlier Chrome problems affected the Processors installer, **not the previously successful INVERSO EQ installer**. The portable option is a convenience and not proof of malware safety.

- Installer: close Cubase, extract the downloaded ZIP from GitHub Actions Artifacts, run the setup EXE.
- Portable: close Cubase, extract ZIP, copy `INVERSO BASS DRIVE.vst3` **folder** to `C:\Program Files\Common Files\VST3`, reopen Cubase.

## Verification and known limitations

- Portable C++17 test suite runs functional checks for dry neutrality, band preservation and each fuzz type, plus 16 / 32 / 64 simulated instances with parameter automation.
- 4× oversampling reduces **but does not completely eliminate** aliasing. No formal measurements of THD+N or standard true peak compliance yet.
- **No output limiter**: if the peak indicator turns red, reduce Output; the plugin is not designed to guarantee clipping-free output.
- No claim of identical behavior to any vintage fuzz pedal. Test in Cubase on a copy of an existing project and compare at matched loudness.
- JUCE framework licensing obligations apply before third-party distribution.
