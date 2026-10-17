# Player manual

How to run the Dungeon Master C port and how to play it. The repository has no game files: you supply your own disk images.

## Run the game

Build the game first, see [development.md](development.md). Dungeon Master needs `KAOS.FTL`, `Graphics.DAT`, and `Dungeon.DAT` in `game/`. `--data DIR` selects another folder for the selected adventure.

```sh
build/dm
```

At start, the game extracts each `.adf` disk image in the current folder once: the Dungeon Master v3.60 disk to `game/`, CSB disk A to `game-csb/`, CSB disk B to `game-csb/disk-b/`. A folder that already has the files stays as it is. Before an adventure loads, the CRC32 of its required files must match the Amiga disks; else the game logs the first wrong file and exits.

To extract the disks by hand, see [decompiling.md](decompiling.md). The `.gitignore` file excludes `*.adf`, `game/`, `game-csb/`, and `reference/`. Do not commit game content.

The personal game: `make play` copies a new build to `playthroughs/local/dm`. The binary and the save files are in that folder.

```sh
playthroughs/local/dm --data game --saves playthroughs/local
```

## Options

`build/dm --help` prints the options. An unknown option stops the program with an error.

```text
Usage: dm [options]
  --data       DIR   the game files (DM; CSB with --adventure csb)
  --saves      DIR   the folder of the saved games
  --adventure  dm|csb  skip the adventure screen
  --mode       vanilla|advanced|turnbased  skip the mode screen
  --control    PATH  the control socket of the turn-based mode
  --filter     none|scale2x|scale3x  the pixel-art upscale filter
  --frames     N     stop after N frames
  --show       N     show graphic N of Graphics.DAT (developer)
  --view             the dungeon view with free movement (developer)
  --help             print this help
```

- `--adventure dm|csb` selects the adventure: Dungeon Master (data in `game/`) or Chaos Strikes Back (data in `game-csb/`). With `--adventure csb`, the program stops with exit code 1 when the CSB files are missing.
- `--mode vanilla|advanced|turnbased` selects the game mode.
- `--data DIR` selects the data folder (disk A) of the selected adventure. The CSB file check uses `DIR` only with `--adventure csb`. Else it uses `game-csb/`.
- `--saves DIR` selects the folder of the save files (`DMGAME.DAT`, `CSBGAME.DAT`, their `.BAK` files, and `QUICK.DAT`). The default is the data folder. The folder must exist.
- `--control PATH` selects the socket of the `turnbased` mode. The default is `~/.dm/control.sock`.
- `--filter none|scale2x|scale3x` selects the pixel-art upscale filter. The default is `none`. The filter makes the image 2 or 3 times larger before the window scaling; it smooths the mouse pointer together with the picture. It gives the sharpest image when the window scale is a multiple of the filter factor. Example: `build/dm --filter scale2x`.
- `--frames N` quits after N frames (50 frames per second).
- `--view` shows only the dungeon view at the party start. The keys are the game keys: W, S, Up, Down move; A, D, Left, Right move sideways; Q, E, Delete, Insert turn; the keypad works too. The view walks through walls but not off the map.
- `--show N` shows graphic N of `Graphics.DAT`.
- The environment variable `DM_AUTO_ENTER=1` skips the start screens and presses ENTER at the entrance. The tests use it.

Not available yet: the rename of a reincarnated champion. The entrance and its credits fade the palette, as the original; the dialogs and the endgame screens do not.

## Start screens

Without `--adventure` and `--mode`, the game shows two screens in the game font:

1. Adventure: DUNGEON MASTER or CHAOS STRIKES BACK. CHAOS STRIKES BACK is gray and cannot be selected when `game-csb/` does not have `APPB.FTL`, `Graphics.DAT`, `Dungeon.DAT`, and `disk-b/MINI.DAT`.
2. Mode: VANILLA, ADVANCED, or LLM (`turnbased`). Escape goes back to the adventure screen.

Up and Down move the cursor. Return or keypad Enter selects the item. A click on an item selects it. Ctrl+Q or closing the window quits.

After DUNGEON MASTER, the game shows the FTL logo (`SWSH.FTL`, about 1 second), plays the title animation (`TITL.DAT`, about 1 second; a key or a click stops it), and shows the entrance. `DM_AUTO_ENTER` and the turn-based mode skip the logo and the title. After CHAOS STRIKES BACK, the game shows a third screen: NEW ADVENTURE, RESUME, and QUIT. Escape goes back to the mode screen until the first game starts. After that, Escape does nothing on the CSB screen. See [Chaos Strikes Back](#chaos-strikes-back).

| Flags | Screens |
|---|---|
| None | Adventure and mode |
| `--adventure` only | Mode |
| `--mode` only | None. The adventure is Dungeon Master. |
| `--adventure` and `--mode` | None. CSB shows its third screen. In `turnbased` mode, the game selects NEW ADVENTURE. |
| `--show`, `--view`, or `DM_AUTO_ENTER=1` | None. The adventure is Dungeon Master unless `--adventure` selects CSB. The mode is `vanilla` unless `--mode` selects another mode. With `DM_AUTO_ENTER=1` and `--adventure csb`, the game selects NEW ADVENTURE. |

The game font comes from `Graphics.DAT` in the `--data` folder, else in `game/`, else in `game-csb/`.

## Entrance

The entrance of Dungeon Master: click ENTER or press Return to start a new game. The doors open with a sound, then the game starts. Click RESUME to continue the saved game. QUIT goes back to the first start screen (adventure, then mode). Ctrl+Q or closing the window closes the program. When `--adventure` and `--mode` skip every start screen, and in turn-based mode, QUIT closes the program.

## Chaos Strikes Back

1. Select NEW ADVENTURE on the CSB screen. The game shows the FTL logo and the intro of disk B (`disk-b/ANIM.DAT` with the music `disk-b/MIDI/NAKED.AMG`, about 3.6 minutes; a key or a click stops it). Then the game starts in the prison (`game-csb/Dungeon.DAT`). `DM_AUTO_ENTER` and the turn-based mode skip the logo and the intro.
2. Recruit one to four champions at the 24 mirrors.
3. Save the game. In the prison, the save makes the new adventure: the save merges the party with `disk-b/MINI.DAT`, writes `CSBGAME.DAT`, loads it, and continues in the CSB dungeon. The original needs a restart with the utility disk for this step.

The new adventure starts on level 4 at (22, 18), facing south. Each champion has full health, stamina, and mana, no objects, and food and water of 1500 to 1755. The save needs at least one champion, and each champion needs a unique first name. Else a dialog gives the reason and the game stays in the prison.

- RESUME loads `CSBGAME.DAT`, or `CSBGAME.BAK` when `CSBGAME.DAT` is missing. When the file is missing or damaged, the dialogs are the same as in Dungeon Master, and the game shows the CSB screen again.
- The save dialog shows CANCEL, SAVE AND PLAY, and SAVE AND QUIT. SAVE AND PLAY saves and continues the game. SAVE AND QUIT saves, shows the credits, and then shows the CSB screen. In the prison, SAVE AND QUIT makes the new adventure and then quits: RESUME starts it. The dialog closes after a save. CSB has no quit without a save, as in the original.
- After the credits (SAVE AND QUIT, the end of the game), the game shows the CSB screen. In turn-based mode, the game shows the entrance picture, and `load` or `new_game` over the socket starts the next game. QUIT on the CSB screen goes back to the first start screen, as QUIT on the DM entrance. Ctrl+Q or closing the window ends the program.
- The end of the game: when the party dies, THE END shows with RESTART THIS GAME only (no QUIT), then the credits. When the party wins, the game plays the ending animation `ENDA.DAT`. After the last frame, a key or a click shows the CSB screen (the original stays on the last frame).
- Quick save (F5) and quick load (F9) use `QUICK.DAT` in the CSB folder. A quick save in the prison is a plain save: it does not make the new adventure.
- Turn-based mode: `new_game` starts NEW ADVENTURE (the prison). `save` in the prison makes the new adventure. `load` loads `CSBGAME.DAT`.

## Saved games

The game saves to `DMGAME.DAT` in the data folder (`game/`, or the folder of `--data`). The save file of CSB is `CSBGAME.DAT` (backup `CSBGAME.BAK`) in the CSB data folder (`game-csb/`, or the folder of `--data`). `--saves DIR` puts the save files and `QUICK.DAT` in `DIR` instead; the folder must exist. Before a save, the old `DMGAME.DAT` becomes `DMGAME.BAK`. The file has the format of the original Amiga save file, see [save-format.md](formats/save.md).

- Save: click the save icon in an inventory, or press Ctrl+S. The dialog shows CANCEL, SAVE, and QUIT. After SAVE the dialog stays open. CANCEL continues the game. QUIT ends the game. When the game was not saved or loaded in the last 100 ticks, QUIT asks first.
- Resume: click RESUME at the entrance. The game loads `DMGAME.DAT`, or `DMGAME.BAK` when `DMGAME.DAT` is missing. A damaged file shows "THE GAME IS DAMAGED!" and the entrance again.
- When all champions die, the game shows THE END. When a saved game exists, RESTART THIS GAME loads it. QUIT on this screen, the QUIT of the save dialog, and the end of a won game show the credits. A key or a click then shows the entrance.
- Deliberate deviations from the original: the original quits the program after the credits and after a damaged save. The port shows the entrance. QUIT on the entrance goes back to the start screens. Only Ctrl+Q or closing the window ends the program.

## Controls

The controls are the controls of the original Amiga game.

| Input | Action |
|---|---|
| Up, keypad 8 | Move forward |
| Down, keypad 2 or 5 | Move backward |
| Left, keypad 4 | Move left (sideways) |
| Right, keypad 6 | Move right (sideways) |
| Delete, keypad 7 | Turn left |
| Help, Insert, keypad 9 | Turn right |
| F1 to F4 | Open or close the inventory of champion 1 to 4 |
| Escape | Freeze the game. Escape again continues. |
| Return | Wake up a sleeping party |
| Click on the movement arrows | Move and turn |
| Click on a mirror | Show the champion. Then click RESURRECT, REINCARNATE, or CANCEL. |
| Click on an object in the view | Take the object into the leader hand |
| Click on the floor with an object in the hand | Drop the object |
| Click on the front wall or a door button | Use the wall switch or the door button |
| Click on a hand in a status box | Take or put an object |
| Click on a status box name | Select the leader |
| Right-click on a status box | Open the inventory |
| Inventory: click on a slot | Take or put an object |
| Inventory: click on the mouth | Eat or drink the object in the hand |
| Inventory: hold the eye | Show skills and statistics |
| Inventory: ZZZ or the X | Sleep or close the inventory |
| Inventory: the save icon, Ctrl+S | Save the game |
| Action area | Click a champion hand, then an action |
| Spell area | Click runes, then the cast button. Click a tab to select another caster. |

## Modes

`--mode` selects the game mode. The game rules are the same in all modes.

| Mode | Behavior |
|---|---|
| `vanilla` | Default. The original game. The advanced keys do nothing. |
| `advanced` | The original game with the advanced features. |
| `turnbased` | The advanced features, and a program plays through a Unix socket. The game time stands still between turns. See [llm-mode.md](llm/llm-mode.md). |

The advanced features are not in the original game:

- Automap: a top-down map of the seen squares in place of the dungeon view. The game time stops while the map is open. The top ruler numbers each second column, the left ruler each row; a white frame shows the end of the map. The top-left corner of the map shows the level, X and Y, facing, and light amount. Corridors are light gray, walls are dark gray. Drag the map with the left mouse button to see the whole level; a click without a move does not scroll. The map opens centered on the party.
- Quick save and quick load: one save slot in `QUICK.DAT` in the data folder. `DMGAME.DAT` does not change. The message area shows GAME SAVED, GAME LOADED, NO QUICK SAVE, or QUICK SAVE DAMAGED.
- W, A, S, D, Q, and E move and turn the party. The cursor keys and the keypad also work.
- Rune symbols in red after each rune name on an open scroll.
- Action icons in their own colors.
- Stat meters in the object description (hold the eye with an object in the hand): attack for a weapon, defense for armour, and weight.
- A click on a fountain with an empty hand lets every living champion drink, not only the leader.
- Cheats on the automap: L switches full light (the view is always bright; the creatures still see the real light), I switches immortality (the health of a champion stops at 1; damage, wounds, poison, hunger, and thirst stay), F switches food and water (they do not go down; eating and drinking still raise them). The map shows their state under the read-out. They are not saved: a new or loaded game turns them off. Only in the `advanced` mode, not in `turnbased`.
- Music on the automap: 1 selects Original (no music, the default), 2 Super NES, 3 FM-Towns. The map shows the choice under the cheats. The choice is not saved: a new or loaded game selects Original. Only in the `advanced` mode. A track plays one time and does not loop, as in the original versions:
  - FM-Towns DM: each level has a track. A new level stops the current track and starts its track. Game over and game won have a track.
  - FM-Towns CSB: each square of a zone has a track. A new zone requests its track; it starts when the current track ends. A level change stops the music. The prison and game over have a track.
  - Super NES (DM only, 2 does nothing in CSB): a step on one of 22 trigger squares stops the current track and starts its track. Game won plays the end credits.
  - The MP3 files go into `music/` of the game folder: `game/music/` gets `music/Game_Dungeon_Master_JP/*.mp3` and `music/Game_Dungeon_Master_US/Super_NES,*.mp3`, `game-csb/music/` gets `music/Game_Chaos_Strikes_Back_JP/*.mp3`. A missing file gives silence. The music stops at the entrance and at the CSB screen.
- Escape and Tab close an open inventory. With no inventory open, Escape freezes the game as in vanilla.
- The damage that a champion deals stays in the action area for two ticks, not one. A click on the action area in the second tick shows the action icons and acts at once. Only in the `advanced` mode, not in `turnbased`.
- Smooth projectile flight: between two game ticks, the view shows each flying projectile part of the way to its new place. The size changes only at a tick.

Keys of the `advanced` mode:

| Key | Action |
|---|---|
| M, Tab | Open or close the automap. The map opens only in the dungeon view: no inventory, freeze, sleep, or candidate. While the map is open, only M, Tab, Escape, L, I, and F (cheats), 1, 2, and 3 (music), and a drag on the map work. Tab also closes an open inventory. |
| Escape | Close the automap or the open inventory, else freeze the game |
| F5 | Quick save. Only in the dungeon view. |
| F9 | Quick load. Only in the dungeon view. |
| W, S | Move forward, move backward |
| A, D | Move left, move right (sideways) |
| Q, E | Turn left, turn right |

F1 to F4 open the inventories in every mode.

In the `advanced` and `turnbased` modes, the save file has an extension block after the original save data. The block keeps the explored squares of the automap. The original part of the file is the same as in a vanilla save, and vanilla loads the file. A file without the block, or with a damaged block, loads with an empty automap.

In the `turnbased` mode, the game enters the dungeon at the start. The control socket opens after the start screens. A script that waits for the socket must pass `--mode turnbased` to skip the screens. The keyboard and the mouse do not control the game. Closing the window quits.


## Windows

Windows (x86-64): `make windows` cross-builds `build/dm.exe` with MinGW-w64 (`brew install mingw-w64`). The first run downloads the SDL3 MinGW package into `build/sdl-mingw/`. `make dist-win` builds `build/DungeonMaster-win.zip` with `dm.exe` and `SDL3.dll`. The player copies the disk images into the folder of the game.

The Windows build has no control socket, so the LLM mode cannot start there: the mode screen offers VANILLA and ADVANCED only, and `--mode turnbased` stops with an error. Test it with Wine.

On macOS, `make dist` builds `build/DungeonMaster.zip`: the game and SDL3 without game files.

## Library

`build/libdm.dylib` contains the game code without SDL. The tests load it with `ctypes`, see [development.md](development.md).
