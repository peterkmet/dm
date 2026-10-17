# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Small cleanups after the clean-code pass

- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.

## Port the name entry of a reincarnated champion

- Owner's wish (2026-10-17): REINCARNATE asks for a new name and title, as in the original.
- PRD: `docs/superpowers/2026-10-17-reincarnate-name.md`.

## Credits on the entrance

- The credits command (203) of the entrance is not ported; the original fades the palette.
- PRD: `docs/superpowers/2026-10-17-entrance-credits.md`.

## Windows build: the sound crackles under Wine

- Found in the manual test (2026-10-17): `dm.exe` under the local Wine CX build (`~/github/wine-cross`) plays, saves and loads, but the sounds crackle and pop (wall bump, door). The macOS build sounds clean. Kairosoft games in the same Wine sound clean.
- Ruled out:
  - the game output: the SDL `disk` driver records clean audio from `dm.exe`;
  - a dry queue: a log in `pump_audio` (`src/platform/platform.c`) found no empty queue during play, and 6 queued ticks did not help;
  - the device rate: `SDL_AUDIO_FREQUENCY=48000` did not help;
  - CPU load: 15-20 % for `dm.exe`.
- Wine CX does not pass macOS environment variables to the Windows process; set them with `wine cmd /c "set NAME=value&& dm.exe ..."`.
- Next step: run `dm.exe` on a real Windows. If it crackles there, look at the SDL3 WASAPI path; if not, it is a Wine CX problem.

## Advanced mode: show the damage twice as long

- Owner's wish (2026-10-16): in the advanced mode the damage that a champion deals stays in the action area twice as long; the vanilla mode keeps the original time.
- PRD: `docs/superpowers/2026-10-17-advanced-damage-time.md`.

## Advanced mode: action icons keep their colors on every level

- Owner's wish (2026-10-16): in the advanced mode the icons of the action area have the same colors on every level; the vanilla mode keeps the original behavior.
- PRD: `docs/superpowers/2026-10-17-advanced-icon-colors.md`.
