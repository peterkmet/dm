# Status of the C port

Updated: 2026-10-04 10:50. Branch: `feature/2026-10-03-dm-tooling-disk-analysis`. Plan: `docs/superpowers/2026-10-04-dm-c-port-plan.md`. Code style: `docs/coding-standard.md`.

## Done and committed

- Tools: ADF extraction, FTL loader, R# unpacker, Ghidra headless decompile with names (`docs/kaos-symbols.csv`), 68000 emulator harness (`dmtools.emu`).
- C port: FTL/KAOS loader, Graphics.DAT, Dungeon.DAT, text and message area, sound with SDL3 audio, Scale2x/Scale3x, game state, RNG, timeline, save file codec, game tables, recruiting text, spell runes, full dungeon view (walls, doors, ornaments, objects, creatures, projectiles, explosions, fields, light), interface panels, movement and sensors, champions and inventory, creatures, projectiles, magic. 261 tests pass (commit `ab63882`).

## In progress

Vanilla, advanced and turn-based modes are committed (commit `5a24461`, 319 tests). Step 15 readability pass is done.

- Turn-based extras (agent started 10:50): control commands save/load/new_game, MCP tools, CLI client `dm-play`.
- Step 16 final review (agent started 10:50): ASan soak in all modes, protocol robustness, vanilla regression check.
- Next: fix review findings, CHANGELOG, manual checklist, merge to main, create the private GitHub repo and push (user chose private GitHub). Then the LLM playthrough: run `build/dm --mode turnbased` with a visible window; a subagent plays through `uv run dm-play` over the socket (user chose the socket, no MCP registration). Keep notes and save often.

## Known differences from the original

- The port never draws the party footprints on the floor (the footprint flag is never set).

## Snapshots

The branch `wip-snapshot` holds snapshots of the whole working tree, including unfinished files. Each snapshot is a commit on top of the previous one. To restore a file: `git checkout wip-snapshot -- <path>`.

## Next steps

1. Finish steps 7b, 10, 11. Make `make -C port` and `uv run pytest -q` green. Review and commit.
2. Step 13 and object panels, then fix the review findings.
4. Step 15: readability pass for modules written before the coding standard.
5. Step 16: full test, manual checklist, CHANGELOG, merge.
