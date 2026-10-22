# INVERSO DRUM v0.4 — individual velocity per step

A 16-pad Windows VST3 sampler + host-synced sequencer. The new v0.4 editor explicitly displays its version to make stale Cubase instances recognizable.

## New: velocity editing, per pattern / lane / step

- **Each one of the 8 patterns × 16 lanes × 128 steps has its own velocity 0–127.** A velocity of 0 means that step is silent/disabled.
- **Left click** in the timeline toggles a step between Off and velocity 105 (quick composition).
- **Right click** selects a step without changing the value. **Right drag vertically** on that cell sets velocity continuously; SHIFT slows the adjustment.
- **Mouse wheel** over a step changes velocity in increments of 5; **SHIFT + wheel** changes exactly 1.
- **STEP VELOCITY** below the timeline is a numeric 0–127 slider with editable number, **CLEAR STEP**, and a label showing pattern, track, step number and velocity.
- The selected step gets a distinct outline. Active cells also show a thin intensity bar and a numeric velocity when there is sufficient on-screen space (zoom in with 1-bar View for detail).
- Each step's exact velocity is already used by the audio engine, survives project save/load and is retained as an exact MIDI note velocity when exporting/dragging patterns. No floating-point velocity rescaling or 4-level presets.

## Existing v0.3 functionality preserved

16 sample pads; per-lane length in steps of four up to 128; View 1/2/4/8/16 bars; independent rate and straight/dotted/triplet feel; 8 chainable patterns; MIDI file drag-out; WAV/AIFF drop onto pads, multiple-file drop; sample start/end, pitch, reverse, attack and decay; per-pad drive, delay and reverb sends; global feedback, reverb and delay mix.

## Important

- Uninstall the old version / close Cubase before upgrading. If the top-left plugin title doesn't say **v0.4**, Cubase is still showing an older VST3.
- Test with a duplicate session. The installer and binary are unsigned; don't bypass security checks to force installation.
- Building and passing JUCE host tests verifies program flow, but does not replace a real Cubase test.
- Samples refer to external WAV/AIFF paths, not embedded audio.
- MIDI drag and drop depends on Cubase supporting OS external file-drop onto tracks.
- The original Loopcloud DRUM is not affiliated with or used as source code for this software.
