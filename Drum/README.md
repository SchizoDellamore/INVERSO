# INVERSO DRUM v0.6 — externally loaded samples, user sample collections, per-hit FX

Windows x64 VST3 / JUCE 8. Independent drum sampler and step sequencer for DAWs. Cubase/Studio One/Ableton compatibility must be verified in the actual host; CI does not substitute for testing the host GUI.

## External sample workflow
- Open **DAW LIBRARIES**. Pick the pad and a collection category: **Kicks, Snares, Hats, Claps, Toms, Percussion, FX, Other**.
- **ADD FOLDER** registers an existing directory of WAV/WAVE/AIFF samples, including subdirectories. **ADD SAMPLES** registers chosen individual audio files as items in this category.
- **IMPORT PATH** accepts an accessible full Windows WAV/AIFF path (including quoted paths copied from Explorer). It decodes and assigns the file directly, auditioning the newly loaded pad.
- Filter by category/DAW label, search names and folder paths, select a hit and press **LOAD TO PAD** or double-click. Scan on a background thread; no file scanning in the audio callback. Registrations persist in the user's local application data directory.
- File/Explorer drag-and-drop is supported where the host forwards native file paths; direct Cubase MediaBay/Studio One/Ableton browser drag is **not guaranteed**. Content trapped inside proprietary .vstsound/.soundset containers or Live devices is not decoded; ordinary accessible WAV/AIFF can be indexed.
- Source files remain in their original locations. Moving them after saving a session will break those references. No cloud account or network required.

## Per-step FX (new)
- 8 patterns × 16 instruments × up to 128 individual steps.
- Select a step and use **STEP DRIVE**, **STEP REVERB**, **STEP DELAY** controls. Each control is independent for that precise hit. The label **(PAD DEFAULT)** indicates inheritance; interacting with it creates an explicit value for that event, including explicit 0% = dry.
- **RESET STEP FX** switches the selected hit back to pad settings.
- On playback, each voice captures its effective Drive, Reverb Send and Delay Send at trigger time. FX of one hit cannot change the sends of a later hit or the same instrument's other steps. The shared reverb/delay tail may naturally overlap subsequent dry hits.
- Global reverb tone/decay and delay division/feedback remain common processor settings, **not independent delay-time or reverb algorithms per event**.
- Velocity 0–127 remains stored for each individual event, independent of pad gain. Right-drag adjusts the selected step; wheel +/-5 or Shift+wheel +/-1; exact text value below the timeline. Those values are saved and exported as MIDI velocities.

## Automated validation
Windows GitHub Actions runs 6 tests, including:
- Real generated WAV file written to disk, decoded into pad 4 in place of a factory sample, played by a MIDI event through processBlock, and reloaded from the saved session path.
- The same production importer via **IMPORT PATH**, plus folder discovery by the production indexing algorithm.
- Per-step reverb-only, delay-only and dry samples from the **same** snare, with drive rendering difference.
- Reverb and delay output/tail audibility, independent steps, pattern/MIDI save state and precise 0–127 velocity.
- Clean-session reload clears old event effects; older project files remain readable.

Successful CI does not prove native Cubase MediaBay drag-and-drop, native file chooser focus, Windows antivirus heuristics, or the end-user experience in Cubase 15. The installer is unsigned; verify provenance and SHA-256 before use. Close Cubase before replacing the old plugin and test in a duplicate project.
