# Status of the C port

Updated: 2026-10-04 05:05. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT, Dungeon.DAT, text and message area, sound with SDL3 audio, Scale2x/Scale3x, game state, RNG, timeline, save file codec, game tables, recruiting text, spell runes, full dungeon view (walls, doors, ornaments, objects, creatures, projectiles, explosions, fields, light), interface panels, movement and sensors, champions and inventory, creatures, projectiles, magic. 261 tests pass (commit `ab63882`).

## In progress

Vanilla is playable with save/resume/endgame (commit `2f9cc2c`, 297 tests).

- Step 15 readability pass, 4 agents (started 05:00): (a) ftl/graphics/dungeon/savefile/tables, (b) text/message/sound/game/random/recruit/spells, (c) view/draw/layout, (d) savegame/startend/dialog/gameloop/main/platform/video/scale/command.
- New modes (`docs/superpowers/2026-10-04-modes-plan.md`, approved without PRD): automap module agent (`automap.*`) and turn-based control agent (`control.*`, `state_json.*`, `src/dmtools/mcp_server.py`) started 05:05, new files only.
- After step 15: wire advanced features and modes into existing files; then Step 16 (final test, checklist, CHANGELOG, merge).

## Snapshots

The branch `wip-snapshot` holds snapshots of the whole working tree, including unfinished files. Each snapshot is a commit on top of the previous one. To restore a file: `git checkout wip-snapshot -- <path>`.

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Step 13 and object panels, then fix the review findings.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
