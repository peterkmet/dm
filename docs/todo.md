# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Small cleanups after the clean-code pass

- `ui/view_walls.c`: the Thieves Eye (`g_party.thieves_eye_count`, `src/engine/party.h:24`) is not drawn. The original draws it in three places: the D1C front wall gets the hole graphic 41 over the squares behind it (KAOS.c:23050-23081); the D1C door gets the thieves eye door ornament (KAOS.c:21403-21405; loaded at `src/ui/view.c:1006`); the door frame of the party square (D0C) gets the frame with the hole (KAOS.c:23232-23245).
- `rec_find_unused_thing` (`src/engine/record.c`) returns `THING_NONE` when no record is free; the original then frees a record with `get_discarded_thing`. PRD: `docs/superpowers/2026-10-17-discard-thing.md`.
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
