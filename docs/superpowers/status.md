# Status of the C port

Updated: 2026-10-04 02:30. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`, all 756 KAOS functions named), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT and expand_graphic, Dungeon.DAT, text and message area, sound mixer with SDL3 audio, Scale2x/Scale3x filter, game state and RNG and timeline, save file codec, game tables, recruiting text and spell runes, dungeon view walls/doors/ornaments (`build/dm --view`).

## Done, not committed yet (in the working tree)

- Step 8 interface: `port/src/ui*.c`, `ids.h`, `tests/test_ui.py`, small changes in `video.*`, `platform.c`, `layout.*`.
- Step 9 rules: `port/src/map.*`, `move.*`, `sensor.*`, `timeline_process.*`, `tests/test_move.py`, `docs/rules-movement.md` (cleaned to the coding standard).

## In progress when this file was written

- Step 10 champions: `port/src/champion.*`, `inventory.*`, `tests/test_champion.py`, `docs/rules-champions.md`.
- Step 11 creatures, projectiles, magic: `port/src/group.*`, `projectile.*`, `magic.*`, `tests/test_creatures.py`, `docs/rules-creatures.md`.
- Step 7b things in the view: objects, creatures, projectiles, explosions, fields, light levels in `port/src/view.c`.

The working tree can fail to build while these files are incomplete (link errors between them).

## Snapshots

The branch `wip-snapshot` holds snapshots of the whole working tree, including unfinished files. Each snapshot is a commit on top of the previous one. To restore a file: `git checkout wip-snapshot -- <path>`.

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Integration: entrance screen, new game, game loop in `main.c` (input commands from `ui_command_from_mouse`, keyboard, `move_process_new_party_map`, `timeline_process`, view and interface drawing, sound pending). First playable version.
3. Step 13: resume a saved game, save, endgame.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
