# INVERSO BASS DRIVE v0.2 — stability recovery build

**DO NOT install v0.1** if it froze, muted or stalled Cubase. This release is a conservative stability rewrite after a reported host freeze and loss of audio. Root cause of the Cubase-specific failure has not been confirmed.

## Main changes
- Removed JUCE 4x oversampling from the realtime path. Host sample-rate processing is now direct, allocation-free and zero-latency, independent of Cubase's varying block size. This is a deliberate temporary trade-off: distortion can alias at high drive levels until oversampling is reintroduced and host-tested.
- Fail-open processor: callbacks received before preparation keep their audio. Nonfinite DSP samples revert to the original finite sample rather than muting the whole buffer.
- Added a real, smoothed Bypass parameter. Default remains Drive 25%, Fuzz Off. Existing parameter IDs and ranges are retained for session compatibility.
- Drive remains the primary effect, while Fuzz Off/Vintage/Silicon/Gated stays optional with independent strength. Orange accent highlights Drive.
- Meters show sample peaks, NOT true peak. No limiter. Output -24..+12 dB.

## Signal path
Input -> host-rate split (variable 60–500 Hz fourth-order highpass + complementary low path) -> Drive -> optional Fuzz -> Mix -> Output -> smoothed Bypass -> output.

## Tests
- Portable C++ tests: functional filters/fuzz; 16/32/64 simulated instances; silence/passthrough at 44.1/48/96 kHz.
- Windows JUCE host smoke: actual AudioProcessor in mono and stereo, varying buffer sizes **up to 2048 after preparing for only 32**, multiple sample rates, modes, bypass and state save/restore.
- Final Cubase 15 Pro confirmation still requires user testing. Do not test on your only copy of a production project.

## Download
- Installer ZIP -> extract and run `INVERSO_BASS_DRIVE_Setup_0.2.0_x64.exe` with Cubase closed.
- Portable ZIP -> extract and copy `INVERSO BASS DRIVE.vst3` folder to `C:\Program Files\Common Files\VST3` with Cubase closed.
- Both are unsigned, so security reputation warnings are possible; do not disable antivirus.

## Notes
Fuzz is an independent optional stage. Changing fuzz mode may introduce audible transitions at extreme settings. This release does not include oversampling while the host stability issue is being investigated. If this release still freezes Cubase, disable the plugin and supply the Cubase crash dump for diagnosis.
