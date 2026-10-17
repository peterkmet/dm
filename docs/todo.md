# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Advanced mode: options menu on Escape

- Escape opens an options menu instead of only freezing the game: sound volume in X steps, music volume in Y steps, the music choice (now keys 1-3 on the automap), and freeze.

## Advanced mode: cheat screen

- A cheat screen of its own, not on the automap. Cheats: full light, immortality, food and water (moved from the automap), one-hit kill, walk through walls, reveal the whole map, a list of all spells with their runes.
- The save file keeps the cheat state, as it keeps the explored map.

## Advanced mode: more sounds at once

- More than the 2 sounds at once of the two Amiga stereo pairs (`src/ui/audio.c`).

## Advanced mode: more save slots

- The save dialog offers a choice of several save slots.

## CSB: start without the two disks

- Chaos Strikes Back: choose the party, then stairs down into the dungeon where the game starts, without the two-disk start of the original.

## Release on GitHub

- Publish the source and the macOS and Windows builds on GitHub (the repository is private today).
- Test the disk install end to end: a clean folder with only the program and the Amiga disk images (`.adf`); the game extracts `game/`, `game-csb/`, `game-csb/disk-b/` and starts DM and CSB. On macOS and on Windows.
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
