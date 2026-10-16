# Ideas for later

User wishes for future versions. Not planned yet.

## Uninitialized bytes of free active groups in a save

- `game_init_active_groups` (`src/engine/game.c`) allocates `g_active_groups` with `malloc` and sets only `group_index = -1` in each entry. The other bytes of a free entry are not initialized.
- `pack_active_group` (`src/engine/savegame.c`) copies every entry into save block 1, the free entries too. Two saves of the same game can have different bytes in block 1.
- Change: clear the array after `malloc` (`calloc`, or `memset` to 0), then set `group_index = -1`. Game behavior stays; saves become deterministic.
- Then remove `scrub_free_groups` from `tests/golden/test_golden_save.py` and `tests/golden/test_golden_rules.py`; the golden files stay the same.

## NULL read of creature attributes on a corrupt dungeon

- `map_get_creature_attributes` (`src/engine/map.c`) reads the creature table through the cached accessor of `data/tables`. A creature type above the table (a corrupt dungeon) reads a NULL pointer.
- Change: a range check with a defined value, a short comment, and a deviation entry in the formats doc.

## Truth values that are still int16_t

- `savegame_quick_save` (`src/engine/savegame.h`) returns 1 or 0: make it `bool` (coding standard rule 3). `savegame_quick_load` keeps its `SAVEGAME_*` codes.
- `music_play_end(int16_t won)` (`src/ui/music.h`): make the parameter `bool`.

## Small cleanups after the clean-code pass

- `action_reset` (`src/engine/action.c`) also clears `g_redraw`, which is not action state.
- `ui/view.c` square table: the D2L2, D2R2 and D0 rows leave unread "none" fields at 0, a valid id; the D0C right stairs zone is a literal in its branch; the three shape `if` blocks can be one `switch`.
- ui ignores the result of `inventory_fill_leader_hand_at_fountain`.
- `engine/group.c`: a parameter `group_cell` has the name of a function.
- `tests/golden/test_golden_rules.py`: `check()` raises a bare `KeyError` on a missing key; the update mode needs `-n 0`; the dead-caster case of `menus_draw_spell_area` has no snapshot.

## Advanced mode: show the damage twice as long

- Owner's wish (2026-10-16): in the advanced mode the damage that a champion deals stays in the action area twice as long; the vanilla mode keeps the original time.
- Today: `menus_draw_action_damage` (`src/ui/ui_menus.c:115`) shows the damage in the action area until the area draws again. The time of that redraw also controls when the champion can act again, so the longer display needs its own timer, not a change of the action times.
- Not in scope: the damage received on the champion box (`HIDE_DAMAGE_TICKS`, `src/engine/champion.c:26`) keeps its time.

## Advanced mode: action icons keep their colors on every level

- Owner's wish (2026-10-16): in the advanced mode the icons of the action area (the empty hand) have the same colors on every level; the vanilla mode keeps the original behavior.
- Today: the action area is on the screen lines of the viewport, so it draws with the dungeon palette. Each map puts the replacement colors of its creatures into colors 9 and 10 of that palette (`set_creature_replacement_colors`, `src/ui/view.c:2038`). The empty hand icon uses one of these colors: orange on level 11, dark red on level 12. The original game does the same.
- Change: in the advanced mode, draw the action area icons with fixed colors (e.g. the default colors 9 and 10 of the interface palette), without changing the creature colors of the viewport.

## Test audit against the test list of CLAUDE.md

- Check every test against the list of CLAUDE.md §2 (what to test, what not to test: UI and layout, CLI arguments, constants and data tables, wiring).
- Candidates: `tests/port/test_select.py`, the out-of-range and count tests of `tests/differential/test_tables.py`, parts of `tests/port/test_modes.py`.
- Result: a list with one reason per test to remove or keep; the user approves it before a test is removed.
