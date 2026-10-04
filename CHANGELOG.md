# Changelog

## 0.2.0 - 2026-10-04

Chaos Strikes Back (Amiga) support, and changes to the advanced mode.

### Added

- Chaos Strikes Back from the user's own disks: disk A in `game-csb/`, disk B in `game-csb/disk-b/`.
  - The port builds a KAOS-layout image from the CSB engine (`APPB.FTL`) and reads the CSB texts, the CSB save format (`CSBGAME.DAT`), and the CSB rules: removed INVOKE, FLUXCAGE and FUSE actions, sensor priority, version checker, reincarnation.
  - NEW ADVENTURE starts the prison with 24 champion mirrors. A save in the prison merges the party with `disk-b/MINI.DAT`, as the original utility disk does, and the game continues in the CSB dungeon. RESUME loads `CSBGAME.DAT`.
  - Save dialog CANCEL / SAVE AND PLAY / SAVE AND QUIT.
  - Magic map: the map spells, events 80 to 83, the MAGICMAP and CREANAME scrolls.
  - End of the game: the CSB death screen with RESTART THIS GAME, and the `ENDA.DAT` animation after the end sensor.
- Start screens in the DM font: adventure (DUNGEON MASTER / CHAOS STRIKES BACK), then mode (VANILLA / ADVANCED / LLM). `--adventure dm|csb` and `--mode` skip them.
- Turn-based mode and the LLM tools work for CSB. The state JSON has the field `adventure`.
- Advanced mode: quick save (F5) and quick load (F9) to `QUICK.DAT`; W, A, S, D, Q, E move and turn the party.

### Changed

- Advanced mode: the advanced features are always on. F6 is gone. The automap shows the level, position, facing and light; F5 no longer shows them in the dungeon view.

### Fixed

- Each champion keeps its own portrait bitmap, as in the original. Before, all champions showed portrait 0.

### Deviations from the original

- CSB: the save in the prison makes the new adventure at once; the original needs a restart with the utility disk. After the credits or the `ENDA.DAT` animation, the game shows the CSB screen; the original freezes or loops.

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
