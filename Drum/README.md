# INVERSO DRUM v0.6.2 — externally loaded samples, user sample collections, per-hit FX

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
- **Left mouse button (LMB)** adds a step (default velocity 105). Repeated clicks or left-dragging over a programmed step **never toggle or overwrite it**. LMB can paint multiple steps while dragging.
- **Right mouse button (RMB)** deletes a note (velocity 0) and clears its now-unused per-event FX. Right-drag can erase multiple steps. Adjacent notes, even on the same drum lane, are untouched.
- Velocity 0–127 remains individual to each event, independent of pad gain. Select the note and use **STEP VELOCITY** for exact entry; wheel +/-5, Shift+wheel +/-1. Those values are saved and exported as MIDI velocities.

## Automated validation
Windows GitHub Actions runs 9 tests, including:
- Real generated WAV file written to disk, decoded into pad 4 in place of a factory sample, played by a MIDI event through processBlock, and reloaded from the saved session path.
- The same production importer via **IMPORT PATH**, plus folder discovery by the production indexing algorithm.
- Per-step reverb-only, delay-only and dry samples from the **same** snare, with drive rendering difference.
- Reverb and delay output/tail audibility, independent steps, pattern/MIDI save state and precise 0–127 velocity.
- Clean-session reload clears old event effects; older project files remain readable.
- Mouse-edit regressions: LMB add, RMB erase, drag painting, no toggling of existing notes, and unchanged velocities/effects on other steps.

Successful CI does not prove native Cubase MediaBay drag-and-drop, native file chooser focus, Windows antivirus heuristics, or the end-user experience in Cubase 15. The installer is unsigned; verify provenance and SHA-256 before use. Close Cubase before replacing the old plugin and test in a duplicate project.

## Usability and timeline corrections (v0.6.2)

- **No mirrored notes**: the editor addresses each of up to 128 steps as an independent event. With a 4-bar view and 1/16 division, notes in bar 1 do not appear copied into bars 2, 3 and 4. Creating a note past the currently active length automatically expands only that instrument's lane, in multiples of 4.
- The audio engine still loops within each lane's explicitly configured **STEPS** length. Choose a shorter loop intentionally; this does not cause visual editing aliases in later bars.
- **RMB on the lane's instrument name** opens a context menu with **Load sample**, **Select all notes**, **Erase all notes in current pattern**, **Clear selection**, and **Undo last track edit**. Erase-all never changes another lane or pattern.
- **Shift + LMB** toggles selection of a loaded note. **Shift + drag** selects more loaded steps without creating or deleting notes. **Shift + click the lane name** selects all existing notes in that lane and pattern. Selected notes have a cream-colored border.
- Use the **STEP VELOCITY**, **STEP DRIVE**, **STEP REVERB**, and **STEP DELAY** controls to modify selected notes as a group while preserving independent values on each unselected step. Mouse wheel over a selected note adjusts selected notes; Shift+wheel provides precision 1-unit increments.
- The new **PAD VELOCITY %** sets a per-instrument amplitude multiplier (0..200%, 100% neutral), without overwriting individual 0..127 step velocities. **LEVEL** remains separate.
- A newly instantiated kit is **dry by default**: each instrument's Drive, Reverb Send and Delay Send are 0%. Each event inherits the zero sends unless explicitly changed. Explicit settings saved in older projects are not silently reset.
- The Undo stack is per lane and bounded at 64 actions. It restores notes, event FX, pad velocity, sample references and relevant pad settings. Undo history lives in the current editor session, not in saved projects.

The original production, sample-import and FX tests continue to run; added automated tests cover 4-bar edit aliasing, dotted/triplet coordinates, pad-wide velocity response, active-only selection, erase-all, undo and new-kit dry defaults. Results from CI do not replace mouse/keyboard verification in Cubase 15.
