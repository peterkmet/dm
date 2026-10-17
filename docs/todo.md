# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## CSB intro: a pause after a click

- Owner (2026-10-17, manual test): a click stops the CSB intro (as designed), but then about 5 seconds pass with
  nothing on the screen before the prison starts. Find the cause (`src/platform/app.c`, `src/ui/audio.c`).

## CSB start: no stairs down on the first prison level

- Owner (2026-10-17, manual test): the first level of the CSB prison (map 0) has no stairs down that the party
  can reach, so the advanced stairs start (`docs/superpowers/2026-10-17-csb-start.md`) never fires. The PRD says
  map 0 has 4 stairs down to map 1: check this against the dungeon data first. If there are none, put stairs down
  into the prison map in the port (advanced only), at a square the owner can reach.
- With the stairs in place, the advanced mode drops the start text of the prison that says it is only the first
  level and that the party must be saved.

## Advanced save dialog: LOAD, SAVE, CONTINUE, QUIT

- Owner (2026-10-17, manual test): in the advanced mode the save dialog (Ctrl+S, the save icon; DM: CANCEL, SAVE,
  QUIT; CSB: CANCEL, SAVE AND PLAY, SAVE AND QUIT) shows four buttons in DM and in CSB: LOAD (the load list),
  SAVE (the save list), CONTINUE (back to the game), QUIT (to the start screen: the DM entrance or the CSB screen;
  the quit question of today stays). The port prints the button texts with the game font, so the texts can
  change. Vanilla keeps the original dialog.

## Release on GitHub

- Publish the source and the macOS and Windows builds on GitHub (the repository is private today).
- Version 0.15.0 (CHANGELOG). The disk install is tested end to end (2026-10-17): a clean folder with only the program and the three `.adf` disks extracts files equal to the reference data and starts DM and CSB, from the macOS zip and from the Windows zip under Wine CX; not on a real Windows.
- Left: make the repository public, create the GitHub release 0.15.0 with `build/DungeonMaster.zip` and `build/DungeonMaster-win.zip` (owner command).
- The history holds no original game files (no `.adf`, `.ftl`, `.dat`, `game/` or `reference/` path was ever added; checked 2026-10-17).

## Small cleanups after the clean-code pass

- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.

## Windows build: the sound crackles under Wine

- Found in the manual test (2026-10-17): `dm.exe` under the local Wine CX build (`~/github/wine-cross`) plays, saves and loads, but the sounds crackle and pop (wall bump, door). The macOS build sounds clean. Kairosoft games in the same Wine sound clean.
- Ruled out:
  - the game output: the SDL `disk` driver records clean audio from `dm.exe`;
  - a dry queue: a log in `pump_audio` (`src/platform/platform.c`) found no empty queue during play, and 6 queued ticks did not help;
  - the device rate: `SDL_AUDIO_FREQUENCY=48000` did not help;
  - CPU load: 15-20 % for `dm.exe`.
- Wine CX does not pass macOS environment variables to the Windows process; set them with `wine cmd /c "set NAME=value&& dm.exe ..."`.
- Next step: run `dm.exe` on a real Windows. If it crackles there, look at the SDL3 WASAPI path; if not, it is a Wine CX problem.
