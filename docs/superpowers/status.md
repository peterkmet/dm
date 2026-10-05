# Status of the C port

Updated: 2026-10-04 16:50. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT, Dungeon.DAT, text and message area, sound with SDL3 audio, Scale2x/Scale3x, game state, RNG, timeline, save file codec, game tables, recruiting text, spell runes, full dungeon view (walls, doors, ornaments, objects, creatures, projectiles, explosions, fields, light), interface panels, movement and sensors, champions and inventory, creatures, projectiles, magic. 261 tests pass (commit `ab63882`).

## Open interface notes from the last session

- A teleporter that sends the party back to the square it came from is reported as `bumped: true`, not `stopped: moved`.
- The view shows such teleporters as `teleporter` only on some ticks; they still teleport when the view shows `floor`. Check against the original drawing (visible teleporter field animation).
- `--brief` prints a wall text that starts with a newline as an empty `text "`.

## Known differences from the original

- The port never draws the party footprints on the floor (the footprint flag is never set).

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Step 13 and object panels, then fix the review findings.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
