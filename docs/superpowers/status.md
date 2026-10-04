# Status of the C port

Updated: 2026-10-04 04:10. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT, Dungeon.DAT, text and message area, sound with SDL3 audio, Scale2x/Scale3x, game state, RNG, timeline, save file codec, game tables, recruiting text, spell runes, full dungeon view (walls, doors, ornaments, objects, creatures, projectiles, explosions, fields, light), interface panels, movement and sensors, champions and inventory, creatures, projectiles, magic. 261 tests pass (commit `ab63882`).

## In progress

The game is playable (commit `bd6ddbc`, 278 tests): entrance, new game, movement, recruiting, inventory, creatures, combat, actions, spells.

- Step 13 (agent started 04:10): save game, resume, party death, endgame. Files: `gameloop.c`, `command.c`, `main.c`, `tests/test_save.py`.
- Object panels (agent started 04:10): eye description, scroll and chest panels in `ui_champion.c`, `tests/test_panels.py`.
- Review of the playable version (agent started 04:10): read-only, ASan soak run in a separate worktree.

## Snapshots

The branch `wip-snapshot` holds snapshots of the whole working tree, including unfinished files. Each snapshot is a commit on top of the previous one. To restore a file: `git checkout wip-snapshot -- <path>`.

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Step 13 and object panels, then fix the review findings.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
