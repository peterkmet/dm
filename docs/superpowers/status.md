# Status of the C port

Updated: 2026-10-04 03:35. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT, Dungeon.DAT, text and message area, sound with SDL3 audio, Scale2x/Scale3x, game state, RNG, timeline, save file codec, game tables, recruiting text, spell runes, full dungeon view (walls, doors, ornaments, objects, creatures, projectiles, explosions, fields, light), interface panels, movement and sensors, champions and inventory, creatures, projectiles, magic. 261 tests pass (commit `ab63882`).

## In progress

- Integration (agent started 03:05): `port/src/command.*`, `gameloop.*`, `main.c`, clickable boxes in `view.c`, `tests/test_gameloop.py`. Goal: first playable version.
- Champion actions (agent started 03:35): `port/src/action.*`, timeline events 11, 12, 13, 75 in `timeline_process.c`, `tests/test_actions.py`.

## Snapshots

The branch `wip-snapshot` holds snapshots of the whole working tree, including unfinished files. Each snapshot is a commit on top of the previous one. To restore a file: `git checkout wip-snapshot -- <path>`.

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Integration: entrance screen, new game, game loop in `main.c` (input commands from `ui_command_from_mouse`, keyboard, `move_process_new_party_map`, `timeline_process`, view and interface drawing, sound pending). First playable version.
3. Step 13: resume a saved game, save, endgame.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
