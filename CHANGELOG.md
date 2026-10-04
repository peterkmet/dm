# Changelog

## 0.1.0 - 2026-10-04

First complete version of the C port of Dungeon Master v3.60 (Amiga).

### Added

- Tools: extract the ADF, load and unpack FTL modules, decompile them with Ghidra headless (word parameters, jump-table calls, names from `docs/kaos-symbols.csv`), and run original 68000 functions in an emulator for differential tests.
- C port on SDL3: data loading (`KAOS.FTL`, `Graphics.DAT`, `Dungeon.DAT`), dungeon view, interface, text, sound, movement and sensors, champions and inventory, creatures, projectiles, magic, champion actions, save and resume in the original format, party death and endgame.
- Vanilla mode (default): the original game.
- Advanced mode (`--mode advanced`): automap (M or Tab), debug read-out (F5), feature toggle (F6), rune symbols on spell scrolls, coloured action icons, item stat meters, smooth projectile flight, Escape closes an open inventory. The automap is saved as an extension after the original save data.
- Turn-based mode (`--mode turnbased`): the game waits for turns that arrive over a Unix socket. JSON state, view and explored map for an LLM player. MCP server (`dm-mcp`) and CLI client (`dm-play`).
- Pixel-art upscaling: `--filter scale2x|scale3x` or `DM_FILTER`.

### Deviations from the original

- After game over or the credits, the game returns to the entrance and does not quit.
- Clear original bugs are fixed and marked in the code, for example the stale register in the spell experience.
