# Ideas for later

User wishes for future versions. Not planned yet.

## Small cleanups after the clean-code pass

- `action_reset` (`src/engine/action.c`) also clears `g_redraw`, which is not action state.
- `ui/view_walls.c`: the thieves eye hole is not drawn; it needs the party spell state.
- ui ignores the result of `inventory_fill_at_fountain`.
- `engine/group.c`: a parameter `group_cell` has the name of a function.
- `rec_find_unused_thing` (`src/engine/record.c`) returns `THING_NONE` when no record is free; the original then frees a record with `get_discarded_thing` (`docs/reference/symbols/range-4-6.md`). Port that rule.
- `tests/golden/test_golden_rules.py`: `check()` raises a bare `KeyError` on a missing key; the update mode needs `-n 0`; the dead-caster case of `menus_draw_spell_area` has no snapshot.

## Advanced mode: show the damage twice as long

- Owner's wish (2026-10-16): in the advanced mode the damage that a champion deals stays in the action area twice as long; the vanilla mode keeps the original time.
- Today: `menus_draw_action_damage` (`src/ui/ui_menus.c:115`) shows the damage in the action area until the area draws again. The time of that redraw also controls when the champion can act again, so the longer display needs its own timer, not a change of the action times.
- Not in scope: the damage received on the champion box (`HIDE_DAMAGE_TICKS`, `src/engine/champion.c:26`) keeps its time.

## Advanced mode: action icons keep their colors on every level

- Owner's wish (2026-10-16): in the advanced mode the icons of the action area (the empty hand) have the same colors on every level; the vanilla mode keeps the original behavior.
- Today: the action area is on the screen lines of the viewport, so it draws with the dungeon palette. Each map puts the replacement colors of its creatures into colors 9 and 10 of that palette (`set_creature_replacement_colors`, `src/ui/view.c:878`). The empty hand icon uses one of these colors: orange on level 11, dark red on level 12. The original game does the same.
- Change: in the advanced mode, draw the action area icons with fixed colors (e.g. the default colors 9 and 10 of the interface palette), without changing the creature colors of the viewport.

## Test audit against the test list of CLAUDE.md

- Check every test against the list of CLAUDE.md §2 (what to test, what not to test: UI and layout, CLI arguments, constants and data tables, wiring).
- Candidates: `tests/port/test_select.py`, the out-of-range and count tests of `tests/differential/test_tables.py`, parts of `tests/port/test_modes.py`.
- Result: a list with one reason per test to remove or keep; the user approves it before a test is removed.
