# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Small cleanups after the clean-code pass

- `action_reset` (`src/engine/action.c:171`) also clears `g_redraw`, which is not action state. Move the clear into `game_reset_session` (`src/engine/game.c:62-72`); `menus_reset` cannot do it, because `ui/` may change `g_redraw` only with `|=` and `&=` (`tests/standard/test_standard.py:508-526`).
- `ui/view_walls.c`: the Thieves Eye (`g_party.thieves_eye_count`, `src/engine/party.h:24`) is not drawn. The original draws it in three places: the D1C front wall gets the hole graphic 41 over the squares behind it (KAOS.c:23050-23081); the D1C door gets the thieves eye door ornament (KAOS.c:21403-21405; loaded at `src/ui/view.c:1006`); the door frame of the party square (D0C) gets the frame with the hole (KAOS.c:23232-23245).
- `ui/command.c` ignores the result of `inventory_fill_at_fountain` (`src/engine/inventory.c:445-459`, the caller at `src/ui/command.c:563-565`). The frame refresh already draws the changed hand icon (`src/ui/gameloop.c:117-125`), so the function can return `void`.
- `engine/group.c`: the parameter `group_cell` of `order_attack_cells` (`src/engine/group.c:291-298`) has the name of a function; with `group_at` it becomes `attacker_cell` and `attacker_at`.
- `rec_find_unused_thing` (`src/engine/record.c`) returns `THING_NONE` when no record is free; the original then frees a record with `get_discarded_thing`. PRD: `docs/superpowers/2026-10-17-discard-thing.md`.
- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.
- `sound_request_play` (`src/engine/sound.c:130-148`): the original skips a mode >= 1 sound when the current map is not the party map (KAOS.c:7001); the port does not check it yet. Fix: return when `mode != SOUND_MODE_NOW` and `g_current_map_index != g_party_map_index`; a differential case in `tests/differential/test_sound.py`.
- `tests/golden/test_golden_rules.py`: `check()` raises a bare `KeyError` on a missing key; the update mode needs `-n 0`; the dead-caster case of `menus_draw_spell_area` has no snapshot. Fix: one `check` helper in `tests/dmtest/golden.py` for the five files in `tests/golden/`, with a message that names the update command, and an update mode that stops in an xdist worker.

## Port the name entry of a reincarnated champion

- Owner's wish (2026-10-17): REINCARNATE asks for a new name and title, as in the original.
- PRD: `docs/superpowers/2026-10-17-reincarnate-name.md`.

## Credits on the entrance

- The credits command (203) of the entrance is not ported; the original fades the palette.
- PRD: `docs/superpowers/2026-10-17-entrance-credits.md`.

## Smooth the mouse pointer with the filter

- Owner's wish (2026-10-17): with `--filter scale2x` or `scale3x` the mouse pointer is smoothed like the picture.
- Today: `plat_present` (`src/platform/platform.c:156-179`) scales the indexed game screen, then `overlay_pointer` draws the pointer into the scaled frame as blocks of 2x2 or 3x3 pixels, so the pointer stays blocky. The pointer has its own colors (`plat_set_pointer`, `src/platform/platform.c:103-116`), so it cannot go into `g_screen.px`.
- Change: build a 320x200 index frame (a screen pixel is its color plus 16 for the interface half, a pointer pixel is 32 plus its color, pointer color 0 is transparent), filter it, map it with one 48-entry color table. The screenshot (`src/control/png.c:51-61`) does not change.

## Developer view: game keys and walking through walls

- Owner's wish (2026-10-17): `--view` uses the same keys as the game (Q, W, E, A, S, D, the cursor keys, the keypad; `docs/manual.md:109-114`), and the view can walk through walls.
- Today: `debug_view_key` (`src/platform/debug_view.c:62-95`) knows only the cursor keys, A and D; Left and Right turn. The wall check is at `:87-90`, the map border check at `:85-86`.
- Change: Up, KP8, W forward; Down, KP2, KP5, S back; Left, KP4, A left; Right, KP6, D right; Delete, KP7, Q turn left; Insert, Help, KP9, E turn right. Drop the wall check, keep the map border check. Update `docs/manual.md:48` and `src/platform/debug_view.h:15`.

## Draw the damage splat on the champion box

- Found in the manual test (2026-10-17): a champion that takes damage shows no red splat with the number on the status box. The bars and `health_lost` are correct.
- Today: `champion_apply_pending_damage` (`src/engine/champion.c:816-848`) updates the health and queues the hide event, but nothing draws the splat. `status_draw_damage` (`src/ui/status.c:102-111`) has no caller in `src/`; only `tests/golden/test_golden_screens.py:48` calls it.
- The original: the pending-damage routine draws the splat of each living champion before the action-area refresh (KAOS.c:14776-14829, the draw at :14808 in the `else` of the kill at :14799).
- Change: the engine records the damage of the tick per living champion in `ChampionRuntime` (`src/engine/champion.h:61-67`; the ctypes mirror `tests/dmtest/game.py:101-105` gets the field too); the game loop calls `status_draw_damage` after `champion_apply_pending_damage` and before `release_dead_champions` (`src/ui/gameloop.c:329-330`).

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
