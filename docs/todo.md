# Ideas for later

User wishes for future versions. Not planned yet.

## Small cleanups after the clean-code pass

- `action_reset` (`src/engine/action.c`) also clears `g_redraw`, which is not action state.
- `ui/view_walls.c`: the thieves eye hole is not drawn; it needs the party spell state.
- `ui/command.c`: the credits command (203) is not ported.
- `ui/command.c` ignores the result of `inventory_fill_at_fountain`.
- `engine/group.c`: a parameter `group_cell` has the name of a function.
- `rec_find_unused_thing` (`src/engine/record.c`) returns `THING_NONE` when no record is free; the original then frees a record with `get_discarded_thing` (`docs/reference/symbols/range-4-6.md`). Port that rule.
- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.
- `sound_request_play` (`src/engine/sound.c`): the original skips a mode >= 1 sound when the current map is not the party map; the port does not check it yet. Port that rule.
- `tests/golden/test_golden_rules.py`: `check()` raises a bare `KeyError` on a missing key; the update mode needs `-n 0`; the dead-caster case of `menus_draw_spell_area` has no snapshot.

## Port the name entry of a reincarnated champion

- Owner's wish (2026-10-17): REINCARNATE asks for a new name and title, as in the original.
- Today: `champion_click_rebirth_panel` (`src/engine/champion.c`) keeps the name and prints "<name> REINCARNATED." (`docs/rules/champions.md:44`). `process_other_command` (`src/ui/command.c`) ignores the name entry commands (`docs/formats/dungeon.md:188`).
- The original: the rename panel is `g_panel_content` 8 (`docs/reference/symbols/range-4-6.md:242`); it swaps in its own mouse and keyboard input tables (`range-4-6.md:542`, `KAOS_MOUSE_INPUT_NAME_ENTRY` in `src/data/kaos_addresses.h`). CSB: `champion_rename` at `0x116312` (`docs/reference/csb-appb-symbols.csv`).

## Smooth the mouse pointer with the filter

- Owner's wish (2026-10-17): with `--filter scale2x` or `scale3x` the mouse pointer is smoothed like the picture.
- Today: `plat_present` (`src/platform/platform.c`) scales the game screen, then `overlay_pointer` draws the pointer into the scaled frame as blocks of 2x2 or 3x3 pixels, so the pointer stays blocky.
- Change: draw the pointer into the screen before the filter runs, so the filter smooths it too.

## Developer view: game keys and walking through walls

- Owner's wish (2026-10-17): `--view` uses the same keys as the game (Q, W, E, A, S, D, the cursor keys, the keypad; `docs/manual.md:148`), and the view can walk through walls.
- Today: `debug_view_key` (`src/platform/debug_view.c`) knows only the cursor keys, A and D, and stops at a wall or a closed fake wall.
- Change: map the game keys to the same moves; drop the wall check, keep the check for the map border.

## Advanced mode: show the damage twice as long

- Owner's wish (2026-10-16): in the advanced mode the damage that a champion deals stays in the action area twice as long; the vanilla mode keeps the original time.
- Today: `menus_draw_action_damage` (`src/ui/menus.c`) shows the damage in the action area until the area draws again. The time of that redraw also controls when the champion can act again, so the longer display needs its own timer, not a change of the action times.
- Not in scope: the damage received on the champion box (`HIDE_DAMAGE_TICKS`, `src/engine/champion.c:26`) keeps its time.

## Advanced mode: action icons keep their colors on every level

- Owner's wish (2026-10-16): in the advanced mode the icons of the action area (the empty hand) have the same colors on every level; the vanilla mode keeps the original behavior.
- Today: the action area is on the screen lines of the viewport, so it draws with the dungeon palette. Each map puts the replacement colors of its creatures into colors 9 and 10 of that palette (`set_creature_replacement_colors`, `src/ui/view.c:878`). The empty hand icon uses one of these colors: orange on level 11, dark red on level 12. The original game does the same.
- Change: in the advanced mode, draw the action area icons with fixed colors (e.g. the default colors 9 and 10 of the interface palette), without changing the creature colors of the viewport.

## Test audit against the test list of CLAUDE.md

- Check every test against the list of CLAUDE.md §2 (what to test, what not to test: UI and layout, CLI arguments, constants and data tables, wiring).
- Candidates: `tests/port/test_select.py`, the out-of-range and count tests of `tests/differential/test_tables.py`, parts of `tests/port/test_modes.py`.
- Result: a list with one reason per test to remove or keep; the user approves it before a test is removed.
