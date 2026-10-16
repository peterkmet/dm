# Ideas for later

User wishes for future versions. Not planned yet.

## Uninitialized bytes of free active groups in a save

- `game_init_active_groups` (`port/src/engine/game.c`) allocates `g_active_groups` with `malloc` and sets only `group_index = -1` in each entry. The other bytes of a free entry are not initialized.
- `pack_active_group` (`port/src/engine/savegame.c`) copies every entry into save block 1, the free entries too. Two saves of the same game can have different bytes in block 1.
- Change: clear the array after `malloc` (`calloc`, or `memset` to 0), then set `group_index = -1`. Game behavior stays; saves become deterministic.
- Then remove `scrub_free_groups` from `tests/test_golden_save.py` and `tests/test_golden_rules.py`; the golden files stay the same.
