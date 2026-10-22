# INVERSO DRUM v0.3 — sequencing ergonomics and audible effects

Independent 16-pad drum sampler and sequencer (VST3, Windows x64, JUCE 8). Inspired by the general workflow of Loopcloud DRUM; no proprietary code or content is included.

## What's new
- **Steps in increments of four**: each lane length is now 4, 8, 12, …, 128. Old session lane lengths are rounded to the closest multiple of four when loaded. Step contents are preserved.
- **Up to 16 bars in the timeline viewport**, with View 1 / 2 / 4 / 8 / 16 bars. 4/4 musical display uses host PPQ; each lane retains its own beat subdivision and dotted/triplet feel. The timeline repeats shorter patterns visually and does not magically add beyond 128 unique steps to a lane.
- **Jump to step** buttons reposition the viewport; bar-length zoom allows precise editing and a song-length overview. At 1/8 straight, 128 steps cover 16 bars; at 1/16 straight, 128 steps cover 8 bars, so the 16-bar overview displays the phrase repeated.
- **Sample file drag-and-drop**: drag WAV/AIFF from Windows Explorer onto the specific pad button or lane row, rather than only onto the selected pad. Multiple dragged files populate consecutive pads. File paths continue to be referenced in the session; do not move samples after saving.
- **MIDI drag-out**: drag the 'EXPORT / DRAG MIDI' control to a Cubase MIDI track to try native OS file drop; clicking continues to export a `.mid` file normally. Host support for external file drag/drop can vary; the MIDI export path is always available.
- **Reverb/delay audibility correction**: global FX wet gains and reverb diffusion gain have been raised from the previous extremely quiet values. New kits default to nonzero Delay Send and a more audible Reverb Send. Each pad still has its own independent Drive, Reverb Send, and Delay Send. Tempo-synced Delay Note and Feel controls are preserved.
- Existing 0.1/0.2 project states continue to load. **Existing zero delay sends remain zero intentionally**; to hear delay on an older project, open Effects and raise BOTH Delay Send on the pad AND global Delay Mix (likewise Reverb Send / Reverb Mix).

## Usage / verification
1. Close Cubase, install v0.3 and reopen in a duplicate test project.
2. Drag a WAV file onto the snare lane (track 2) and verify that only its sample changes. Alternatively import with Load Sample.
3. Choose 24 or 128 Steps for that lane, select 16 Bars in View, and add hits across the timeline.
4. Effects: select a snare pad, set Reverb Send to 70% + Reverb Mix 65%; on another trial use Delay Send 75% + Delay Mix 70%, 1/8 Straight at 120 BPM. Play one isolated snare to listen for audible tails.
5. Test MIDI drag-out onto a Cubase MIDI track. If the host refuses the native file drop, click the same control to export a file, then import it into the arrangement.

## Constraints
- 16 bars is a **viewport** in 4/4; per-lane pattern length remains limited to 128 unique events and repeats as needed. Non-4/4 bar ruler support is not yet available.
- Reverb and delay are lightweight algorithmic effects, not sampled impulse-response processors. They may raise peaks; watch Cubase meters.
- Audio drag-out and per-lane multiple outputs have not been implemented. MIDI file drag-out depends on host acceptance of native Windows file drops.
- The original request regarding *note controls* was incomplete. This update retains left-click step on/off and right-click velocity cycling, rather than guessing another desired interaction.
- Windows installer is unsigned. GitHub Actions builds Windows VST3 and runs DSP/host tests; the user must still audition effects and synchronization in Cubase.
