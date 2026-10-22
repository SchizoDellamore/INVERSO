# INVERSO BASS DRIVE 0.4 (experimental B3K-inspired voicing)

This independent VST3 is a **voicing experiment**, not an authentic Darkglass B3K circuit model, affiliated product, or measured replica. The goal is to keep note definition while producing more forceful harmonic drive on bass. The optional fuzz is separate.

## Drive signal path
- Complementary low-band preservation: clean input remains in the output; nonlinear *difference* is mixed onto it.
- DRIVE controls parallel asymmetric/odd saturation; **DRIVE TONE** controls the distorted branch's 3–8 kHz low-pass character (not a dry signal EQ).
- **GRUNT: Tight / Balanced / Thick** controls how much low-frequency content enters the saturation. It does not simply boost output bass.
- **MID BOOST** emphasizes approximately 300 Hz–1 kHz *before* the nonlinear stage.
- BODY enhances approximately 65–310 Hz when the drive or optional fuzz is engaged.
- CROSSOVER 60–500 Hz sets low-band split; PRESERVE LOW selects how much bass is isolated from drive.
- FUZZ MODE Off / Vintage / Silicon / Gated; FUZZ AMOUNT and FUZZ TONE act independently of DRIVE.
- MIX controls processed vs dry signal. OUTPUT (-24 to +12 dB) is not a limiter; reduce on peak warnings.

## Compatibility and risks
- Same VST3 plugin identity and earlier parameter IDs; new parameters are appended for session compatibility.
- Fixed-memory DSP at host sample rate with no dynamic allocation in the audio callback; zero reported latency.
- No oversampling in this stabilisation line; high distortion can alias at some sample rates.
- DSP and JUCE host smoke tests must pass on Windows before releasing a binary. A successful build is not proof of subjective sound quality or real-world Cubase stability.
- Builds include unsigned installer and portable VST3 with SHA-256 checksums. Check third-party licensing before redistributing.
- Reference: Darkglass Microtubes B3K v2 (manual); no original circuit data, impulse responses, copyrighted firmware or binary code has been used.
