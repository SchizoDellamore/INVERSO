# INVERSO DRUM v0.5 — shared DAW sample library browser

A 16-pad Windows VST3 sampler and sequencer for Cubase, Studio One and Ableton hosts. The VST3 editor reports **v0.5** to distinguish it from cached installations.

## What's new: LIBRARIES

- **DAW LIBRARIES** opens an integrated browser of WAV, WAVE and AIFF files in *existing folders* that are also used by the host DAWs. The plugin does not need to duplicate the audio assets.
- Choose a source label **Cubase / MediaBay folders**, **Studio One / Files**, **Ableton / User Library**, or **Shared / Other**, and select **ADD FOLDER** to point to the actual folder on the computer. Multiple roots can be registered. This is a folder index, not an undocumented MediaBay database reader.
- Search filenames and relative paths by text; browse the results and double-click or select **LOAD TO PAD** to assign the selected file to the chosen pad. The assigned pad can be auditioned immediately.
- Sample indexing runs on a low-priority background worker with a 50,000-file cap, and shows up to 6,000 matching results to keep the interface responsive. **RESCAN** updates the catalogue. **REMOVE** deletes a configured root from this plugin's index; it does not delete the audio folder.
- The root folder configuration persists across projects and DAWs in the user's local application data under `INVERSO STUDIOS/DRUM/sample_libraries.txt`. No network transfer or user account is required.
- Existing system file drag-and-drop (WAV/AIFF filesystem file paths) remains available. Whether **Cubase MediaBay** or another host's in-app drag operation passes a native file path to third-party plugin windows **depends on the host** and must be tested in Cubase 15, Studio One, and Ableton.
- **Limit:** Proprietary library containers (`.vstsound`, `.soundset`, Live devices / presets) are **not** decoded and cannot be assumed to expose their internal samples to third-party VST3 plugins. Nor does this browser import the proprietary metadata/tags of MediaBay. Files inside protected packages are not supported.

## Existing v0.4 features preserved

- 16 pads, 8 chainable patterns, up to 128 steps per independent lane, dotted/triplet/straight time, 1–16 bar zoom, independent velocity 0–127 per step and MIDI export / MIDI OS drag.
- Per-pad sample start/end, pitch, reverse, attack, decay, volume/pan, Drive, Reverb Send and Delay Send, and global effects controls.
- Drag-and-drop of WAV/AIFF from Windows Explorer onto individual pad buttons and track rows.

## Important test plan

1. Close Cubase before installing. Use a duplicate project first. Confirm the header says **INVERSO / DRUM v0.5**.
2. Open **DAW LIBRARIES**. Select **Cubase / MediaBay folders**, ADD FOLDER, and choose one folder that contains ordinary WAV samples you have already indexed in MediaBay.
3. Search, select pad 2, double-click a WAV and confirm pad 2 changes and auditions.
4. Independently try dragging a WAV from Cubase MediaBay into the plugin's pad 3. This may fail because some Cubase builds don't pass an OS file path to third-party plug-ins; that does not indicate the internal library index is broken.
5. Repeat with accessible user sample folders used by Studio One or Ableton. Their proprietary packs require vendor support and are out of scope for this version.

JUCE and Windows tests do not replace real DAW integration testing. The installer is unsigned. Neither Steinberg, PreSonus nor Ableton is affiliated with INVERSO.
