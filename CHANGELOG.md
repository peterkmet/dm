# Changelog

## 0.13.0 - 2026-10-07

### Added

- `--saves DIR` selects the folder of the save files (`DMGAME.DAT`, `CSBGAME.DAT`, their `.BAK` files, and `QUICK.DAT`). The default is the data folder.

### Changed

- All tools are in `tools/`: the Python package `dmtools` (was `src/dmtools/`), the Ghidra scripts, `decompile-all.sh`, and `csb_match.py`. The CSB save prototype is `dmtools.csb_mini`; its differential test is `tests/csb_diff.py`.
- The decompiled modules are in `decomp/dm/`, `decomp/csb-a/`, and `decomp/csb-b/`.
- The LLM runs and the personal play folder are in `.playthroughs/`. Each run has `player/`, `coordinator/`, `game/` (the live save), and `saves/`. `make -C port lego` copies only the program; the play folder keeps its saves and reads the game files from `game/`.

## 0.12.0 - 2026-10-07

### Changed

- The C port follows `docs/coding-standard.md`. The source is in four folders of `port/src/`: `data/`, `engine/`, `ui/`, and `platform/`. A file includes only headers of its own layer and of the layers before it. The game rules set redraw flags and do not draw.
- Each header states its function prefixes, and each prefix belongs to one header. Record accessors of `map.h` use `rec_`; the timeline event handlers use `event_`. `things.h` is part of `map.h`.
- Named constants replace the plain numbers, predicates return `bool`, and long functions are split.
- `tests/test_standard.py` checks the standard on each run. `tests/standard_allowlist.txt` holds the permanent exceptions.
- The test suite runs in parallel (`pytest-xdist`, one worker per test file).
- No change of game behavior.

## 0.11.0 - 2026-10-05

### Added

- `act`, `act_many`, and `wait` report `health_lost`: the health that each champion lost since the start of the request, only when a champion lost health. `dm-play --brief` shows it on the turn line.
- The save dialog shows GAME SAVED after a save outside vanilla. In CSB, SAVE AND PLAY prints GAME SAVED in the message area.
- Automap (advanced mode): rulers with the column and row numbers, and a white frame at the end of the map.

### Changed

- Automap (advanced mode): drag the map with the left mouse button to scroll it. A click without a move does not scroll.

## 0.10.0 - 2026-10-05

### Added

- Advanced mode: music of the FM-Towns and Super NES versions from MP3 files in `music/` of the game folder. On the automap, 1 selects Original (no music, the default), 2 Super NES, 3 FM-Towns. FM-Towns DM plays a track per level, FM-Towns CSB a track per zone, Super NES a track per trigger square. A track plays one time. The choice is not saved. The music stops at the start screens. The MP3 decoder is `dr_mp3` (`port/src/dr_mp3.h`, public domain).

## 0.9.0 - 2026-10-05

### Added

- Advanced mode: a third cheat on the automap. F switches food and water: they do not go down; eating and drinking still raise them. The automap shows its state. It is not saved: a new or loaded game turns it off. Not in the turn-based mode.

## 0.8.2 - 2026-10-05

### Fixed

- A free thing record (`THING_NONE`) could enter the thing list of a square. The save was then damaged: `load` stopped the game or refused the file (run 4, level 3 (1,30)). The list functions now refuse it and log `square list guard:` with the map, the square, the game time, and the call stack, so the source can be found.

## 0.8.1 - 2026-10-05

### Fixed

- `dm-play --brief` stopped with `KeyError` when the view left out a side square that the screen does not draw (since 0.8.0).

## 0.8.0 - 2026-10-05

### Changed

- Turn-based `get_map` shows what the automap shows: walls, floors, doors and their state, stairs, open pits, open teleporters, and pressure pads. It shows no wall features (`M L K H V A F ?`): a wall is `#`. The view keeps the wall features, with their side.
- The turn-based view lists a side square only when the screen draws it: not when the center square of its row and the side square one row nearer block the sight. Before, the view showed squares, creatures, and objects behind walls.
- `make -C port lego` copies the latest game into `.playthrough-lego/`, the personal play folder; the saves there stay.

### Added

- Automap (advanced mode): a pressure pad that the view draws is a gray square in the floor.

## 0.7.0 - 2026-10-05

### Added

- Turn-based view: `ornament` on a wall face names its picture (for example `creeper`, `manacles`), as `lock` does. All wall ornaments of the DM dungeon have a name.
- Player file templates in `playthrough_template/`, each with a header that says what to write and in which format. The start save with the party of 4 is kept there locally (git-ignored).

### Changed

- `act_many` gives `action_damage` as a list, one value for each `choose_action`. Before, it gave only the value of the last step.
- A teleporter that sends the party back to the square where the turn started gives `stopped: moved`. Before, it gave `bumped`.
- The LLM docs: `docs/llm-playbook.md` holds everything the player needs (the game manual and how to play); `docs/llm-mode.md` is for the coordinator. Each run keeps its checkpoints, costs, and call log in `.playthrough_run<N>/`.
- `dm-play` logs its calls to `.playthrough_log/` (a link to the folder of the run), not into the player memory.

### Fixed

- Wall texts in the view started with a new line.
- `alcove`, `fountain`, and `vi_altar` kept their value from an older view when no wall was ahead. `drop_object` to the alcove checks the same.
- `add_rune` reported success when the caster had not enough mana; the game ignored the rune and the next cast failed.
- A border wall outside the map showed no wall face, also when the view draws a fountain on it.
- The tests wrote `dm-play` calls into the log of a run.

## 0.6.0 - 2026-10-05

### Changed

- Turn-based view: `lock` on a wall face names the picture of the keyhole or slot (for example `iron_lock`, `gold_lock`, `sun_lock`, `eye_socket`, `gem_hole`, `coin_slot`). A picture without a name gives its index in the wall ornament graphics. Before, `lock` was only `true`. `--brief` prints `lock <name>`.
- LLM playbook: the player reads wall texts and lock names from the view JSON. Screenshots are only for a suspected bug.

## 0.5.1 - 2026-10-05

### Fixed

- Turn-based mode: a `load` that arrived on the control socket before the first frame stopped the game clock. The program stayed at the entrance, and each `wait` and move returned `ticks 0`. Now the game starts before the program reads the first request.

### Removed

- `tools/snapshot.sh` and the `wip-snapshot` branch. The LLM playthrough keeps its saves in `.playthrough_checkpoints/`.

## 0.5.0 - 2026-10-05

### Added

- Advanced mode: cheats on the automap. L switches full light (the view is always bright; the creature rules keep the real light). I switches immortality (the health of a champion stops at 1; damage, wounds, poison, hunger, and thirst stay). The automap shows their state. They are not saved: a new or loaded game turns them off. Not in the turn-based mode.

## 0.4.1 - 2026-10-05

### Changed

- QUIT on the DM entrance and on the CSB screen goes back to the first start screen. Ctrl+Q or closing the window ends the program. In turn-based mode, and when `--adventure` and `--mode` skip every start screen, QUIT ends the program.

### Fixed

- Over the hands in the champion status boxes, the pointer shows the object in the hand, as in the original. Before, it showed the arrow.

## 0.4.0 - 2026-10-05

Intros of the original disks. The save format does not change.

### Added

- DM: the FTL logo of `SWSH.FTL` with its sound before the title animation.
- CSB: NEW ADVENTURE shows the FTL logo and the intro of disk B (`disk-b/ANIM.DAT`, about 3.6 minutes) with the music `disk-b/MIDI/NAKED.AMG` and the sounds of `disk-b/Sound/`. A key or a click stops the intro; the music fades out. `DM_AUTO_ENTER` and the turn-based mode skip it.

### Deviations from the original

- CSB: the original shows the intro from the menu of the utility disk (disk B); the port shows it after NEW ADVENTURE.

## 0.3.0 - 2026-10-05

Items of the todo list. The save format does not change: saves of 0.2.0 load, and 0.2.0 loads the new saves.

### Added

- DM title animation (`TITL.DAT`) before the entrance. A key or a click stops it. `DM_AUTO_ENTER` and the turn-based mode skip it.
- Entrance: ENTER and RESUME show the pressed button with the switch sound. After ENTER, the doors open with the door sound.
- Window and Dock icon: the mummy from `Graphics.DAT`.
- Advanced mode: a left click on the automap moves the clicked square to the center. Tab closes an open inventory. A click on a fountain with an empty hand lets every living champion drink.

### Changed

- Automap: corridors are light gray, walls are dark gray.

### Fixed

- The automap marked side squares that the party cannot see.
- Eating redraws the food and water bars at once, and the mouth blinks when the object is used up, as in the original.
- The name of the object in the leader hand follows a changed icon: a waterskin shows WATERSKIN after the last drink.

### Deviations from the original

- Behind the opening doors, the port shows the view of the start square; the original shows a made-up corridor.

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
