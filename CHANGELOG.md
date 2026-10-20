# Changelog

## Unreleased

### Changed

- The project name is DMMac: the window title, the release zips `DMMac-macos-arm64.zip` and `DMMac-windows-x64.zip`, and the repository `peterkmet/dmmac`.
- The sound output passes a low-pass filter like the Amiga 500 output (the fixed RC filter and the LED filter).
- The save dialog shows GAME SAVED in every mode, not only in the advanced mode.
- Windows: no console window. The messages go to `dm.log` in the game folder, and a problem at the start shows a message box.

### Fixed

- Spell area: a cast left cyan blocks in the rune row, and a clicked rune box showed the next rune inverted. The click highlight now ends before the redraw, and the clicked box flashes as in the original.

## 1.0.0 - 2026-10-17

### Added

- `build/dm --help`; unknown options stop with an error.
- The game extracts the Amiga disk images (`.adf`) in the current folder into `game/`, `game-csb/`, and `game-csb/disk-b/` at start.
- Before an adventure loads, the game checks the CRC32 of its required files against the Amiga disks and names the first wrong file.
- Windows build: `make windows` (MinGW-w64) and `tools/dist/make-dist-win.sh` for a Windows zip. No LLM mode on Windows.
- REINCARNATE asks for a new name and title, as the original (DM and CSB). The turn-based mode keeps the old name.
- The entrance shows the credits; the entrance and the credits fade the palette as the original.
- The view draws the Thieves Eye holes: the wall ahead, the door ahead, and the door frame of the party square.
- A thing on a far square disappears when the records of its type run out, as the original.
- Advanced mode: the damage of a hit stays in the action area for two ticks; the action icons keep their colors in every light.
- The upscale filter smooths the mouse pointer with the picture.
- Advanced mode: Escape opens an options menu with the sound volume, the music volume (0 to 7), and the music choice. `SETTINGS.TXT` in the saves folder keeps the values.
- Advanced mode: the options menu has a FILTER row (NONE, SCALE2X, SCALE3X). The filter changes at once, and `SETTINGS.TXT` keeps it. `--filter` takes priority at a game start.
- Advanced mode: C opens a cheat screen with full light, immortality, food and water, and the new cheats one-hit kill, walk through walls, and reveal map; quick save and quick load; and a list of all spells with their runes. The save file keeps the cheats in a new extension block.
- Advanced mode: 8 save slots (`DMGAME.DAT`, `DMGAME2.DAT` to `DMGAME8.DAT`; CSB `CSBGAME`). The save dialog shows LOAD, SAVE, CONTINUE, and QUIT in DM and CSB; SAVE opens a save list, LOAD and RESUME a load list with the level and the file time of each slot and the quick save; RESTART THIS GAME loads the slot of the last save or load.
- Advanced mode, Chaos Strikes Back: the stairs down of the prison start the new adventure at once, without a save and without the utility disk. Two of the stairs are behind hidden passages, at (15, 6) and (8, 14) of the first level. The advanced prison does not show the text about the utility disk.
- Advanced mode: a key or a click ends the CSB intro at once; vanilla waits for the shown picture as the original.
- Advanced mode: the automap draws a wall that the party walks through (an imaginary wall) checkered.
- Advanced mode: up to 8 sounds play at once (vanilla keeps the 2 of the Amiga).

### Changed

- Final refactor, with no change of game behavior: the source is in `src/` with five layers (`data`, `engine`, `ui`, `control`, `platform`); a root `Makefile` with incremental builds, `-Werror` and `make check`; coding standard v2 with a check per rule; the record accessors in `engine/record.c`; the LLM protocol in `control/`; tests in folders with shared `dmtest` helpers; `docs/architecture.md`, `docs/manual.md`, `docs/development.md`; GPL-3.0 license.
- LLM mode: when the client closes the socket during an `act_many` batch, the batch stops one action earlier than before; a client that stays connected gets the same replies.
- The picture scales smoothly with the window size and follows a live resize.
- `tools/dist/make-dist.sh` no longer puts game files into the zip; the player adds the disk images.
- Clean-code pass of the C port, with no change of game behavior: `.clang-format` and a format check, named constants, `bool` truth values, typed accessors for the KAOS tables, verb-object names, game rules moved from `ui/` to `engine/`, and the dungeon view squares drawn from one table. `docs/coding-standard.md` lists every check of `tests/standard/test_standard.py`.
- Golden snapshot tests of the save file, the dungeon view, and the game rules.
- Windows: the ADF extraction stops with an error on a file name that is not safe on Windows; the turn-based mode is hidden.
- `--view` uses the game keys and walks through walls.
- Source layout: the public headers of a layer are in `src/<layer>/include/`; each layer compiles with the include paths of its own and the lower layers.
- Advanced mode: Escape in the dungeon view opens the options menu, not the freeze. The automap keys L, I, F (cheats) and 1, 2, 3 (music) are gone; the music choice is in the options menu.

### Fixed

- The status box shows the damage splat of a hurt champion.
- A door or a creature on another map than the party map makes no sound.
- The dungeon view draws the footprints of the Footprints spell on the floor, as the original.

### Removed

- `DM_FILTER` (use `--filter`).

## 0.14.1 - 2026-10-07

### Added

- Agent definitions `dm-coordinator` and `dm-player` in `.claude/agents/` for the LLM runs.

### Changed

- LLM player rules: read the bestiary before a fight with a known creature, check wounds before a timed action, keep the commentary of a call short.

## 0.14.0 - 2026-10-07

### Added

- `view.piles[]` has `objects`: every object of the pile, top first; `take_object` takes `objects[0]`. `dm-play --brief` prints the whole pile.

### Changed

- LLM player rules: when stuck, reread the own notes on locks, keys, and doors and try each key in each known lock; take every object of a pile.

## 0.13.2 - 2026-10-07

### Changed

- `reference/` holds the local research material: the original disks and archives (`media/`, was `origs/`), the music collection (`music/`), the extracted versions of the other platforms (`extracted/`), and the decompiled code (`decomp/`, was `decomp/` at the root).
- The research scripts of the other platforms are in `tools/platforms/` and in git (were in `origs/extracted/`).
- The LLM coordinator runs as a subagent: it starts a player subagent per session, plays a batch of sessions, and hands over in `coordinator/handover.md`.

## 0.13.1 - 2026-10-07

### Changed

- The run folders are in `playthroughs/` (was `.playthroughs/`), and the player templates in `playthroughs/template/` (was `playthrough_template/`).
- LLM player rules: saves before risks and after milestones, training before a new level (food and water first), food and water rates, route scripts, at most 80 lines per memory file, the player counts its calls, and asks for a checkpoint when the save is bad.
- LLM coordinator rules: the goal of the runs, the instruction changes that are allowed (general rules, no dungeon hints), the session preparation, and the checkpoint names `s<session>-level<level>`.

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
