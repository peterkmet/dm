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
- `--saves DIR` selects the folder of the save files (`DMGAME.DAT`, `CSBGAME.DAT`, their `.BAK` files, `QUICK.DAT`, and `SETTINGS.TXT`). The default is the data folder. The folder must exist.
- `--control PATH` selects the socket of the `turnbased` mode. The default is `~/.dm/control.sock`.
- `--filter none|scale2x|scale3x` selects the pixel-art upscale filter. The default is `none`; the `advanced` mode takes the filter of the options menu. The filter makes the image 2 or 3 times larger before the window scaling; it smooths the mouse pointer together with the picture. It gives the sharpest image when the window scale is a multiple of the filter factor. Example: `build/dm --filter scale2x`.
- `--frames N` quits after N frames (50 frames per second).
- `--view` shows only the dungeon view at the party start. The keys are the game keys: W, S, Up, Down move; A, D, Left, Right move sideways; Q, E, Delete, Insert turn; the keypad works too. The view walks through walls but not off the map.
- `--show N` shows graphic N of `Graphics.DAT`.
- The environment variable `DM_AUTO_ENTER=1` skips the start screens and presses ENTER at the entrance. The tests use it.

The entrance and its credits fade the palette, as the original; the dialogs and the endgame screens do not.

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

1. Select NEW ADVENTURE on the CSB screen. The game shows the FTL logo and the intro of disk B (`disk-b/ANIM.DAT` with the music `disk-b/MIDI/NAKED.AMG`, about 3.6 minutes; a key or a click stops it: in the advanced mode at once; in vanilla, as in the original, the picture on the screen stays for the rest of its time, up to 11 s in the first minute; then the music fades out for about 2 s). Then the game starts in the prison (`game-csb/Dungeon.DAT`). `DM_AUTO_ENTER` and the turn-based mode skip the logo and the intro.
2. Recruit one to four champions at the 24 mirrors.
3. Save the game. In the prison, the save makes the new adventure: the save merges the party with `disk-b/MINI.DAT`, writes `CSBGAME.DAT`, loads it, and continues in the CSB dungeon. The original needs a restart with the utility disk for this step.

The new adventure starts on LEVEL 5 (the number that the slot list shows) at (22, 18), facing south. Each champion has full health, stamina, and mana, no objects, and food and water of 1500 to 1755. The save needs at least one champion, and each champion needs a unique first name. Else a dialog gives the reason and the game stays in the prison.

In the advanced mode, the stairs down of the prison also start the new adventure: the party steps onto the stairs, and the game continues in the CSB dungeon at once. The stairs merge the party as the save does, but write no file and do not change the current save slot; save when you want. The automap of the new adventure is empty, the running cheats stay as they are (only loading a file sets the cheats from the file), and THE END offers RESTART THIS GAME only after the first save. When the party cannot make the adventure (no champion, first names not unique, `MINI.DAT` cannot be read), a dialog gives the reason, and the stairs lead to the lower level of the prison as in the original. The prison has 4 stairs down; two of them can be reached from the start: east through the hidden passage at (15, 6) (it looks like a wall) and the door with a button at (18, 6) to the stairs at (19, 7), and south through the hidden passage at (8, 14) and the door with a button at (7, 15) to the stairs at (6, 15). The other two stairs, (19, 3) and (1, 15), are closed in. The automap shows the stairs down in orange and the hidden passages checkered. The advanced mode does not show the prison text that tells you to save and restart with the utility disk. Vanilla and the turn-based mode keep the stairs of the original.

- RESUME loads `CSBGAME.DAT`, or `CSBGAME.BAK` when `CSBGAME.DAT` is missing. When the file is missing or damaged, the dialogs are the same as in Dungeon Master, and the game shows the CSB screen again. In the advanced mode, RESUME opens the load list (see Saved games); CANCEL shows the CSB screen again.
- The save dialog shows CANCEL, SAVE AND PLAY, and SAVE AND QUIT. SAVE AND PLAY saves and continues the game. SAVE AND QUIT saves, shows the credits, and then shows the CSB screen. In the prison, SAVE AND QUIT makes the new adventure and then quits: RESUME starts it. The dialog closes after a save. CSB has no quit without a save, as in the original. The advanced mode has its own save dialog (see Save slots).
- After the credits (SAVE AND QUIT, the end of the game), the game shows the CSB screen. In turn-based mode, the game shows the entrance picture, and `load` or `new_game` over the socket starts the next game. QUIT on the CSB screen goes back to the first start screen, as QUIT on the DM entrance. Ctrl+Q or closing the window ends the program.
- The end of the game: when the party dies, THE END shows with RESTART THIS GAME only (no QUIT), then the credits. When the party wins, the game plays the ending animation `ENDA.DAT`. After the last frame, a key or a click shows the CSB screen (the original stays on the last frame).
- Quick save (F5) and quick load (F9) use `QUICK.DAT` in the CSB folder. A quick save in the prison is a plain save: it does not make the new adventure. DM and CSB use the same name `QUICK.DAT`; when `--saves` gives one folder to both, the load list of one adventure shows the quick save of the other as DAMAGED.
- Turn-based mode: `new_game` starts NEW ADVENTURE (the prison). `save` in the prison makes the new adventure. `load` loads `CSBGAME.DAT`.

## Saved games

The game saves to `DMGAME.DAT` in the data folder (`game/`, or the folder of `--data`). The save file of CSB is `CSBGAME.DAT` (backup `CSBGAME.BAK`) in the CSB data folder (`game-csb/`, or the folder of `--data`). `--saves DIR` puts the save files and `QUICK.DAT` in `DIR` instead; the folder must exist. Before a save, the old `DMGAME.DAT` becomes `DMGAME.BAK`. The file has the format of the original Amiga save file, see [save file format](formats/save.md).

- Save: click the save icon in an inventory, or press Ctrl+S. The dialog shows CANCEL, SAVE, and QUIT. After SAVE the dialog stays open. CANCEL continues the game. QUIT ends the game. When the game was not saved or loaded in the last 100 ticks, QUIT asks first. The advanced mode has its own save dialog (see Save slots).
- Resume: click RESUME at the entrance. The game loads `DMGAME.DAT`, or `DMGAME.BAK` when `DMGAME.DAT` is missing. A damaged file shows "THE GAME IS DAMAGED!" and the entrance again.
- When all champions die, the game shows THE END. When a saved game exists, RESTART THIS GAME loads it. QUIT on this screen, the QUIT of the save dialog, and the end of a won game show the credits. A key or a click then shows the entrance.

### Save slots (advanced mode)

The advanced mode has 8 save slots. Slot 1 is the file of the other modes: `DMGAME.DAT` (backup `DMGAME.BAK`). Slots 2 to 8 are `DMGAME2.DAT` to `DMGAME8.DAT`, each with its own backup `DMGAME2.BAK` to `DMGAME8.BAK`. CSB uses `CSBGAME.DAT` and `CSBGAME2.DAT` to `CSBGAME8.DAT` in the same way. The vanilla and turn-based modes see only slot 1.

- The list shows one row per slot in the dialog: `4  LEVEL 3  2026-10-17 14:32` (the level of the party and the date and time of the file), `4  EMPTY` when the slot has no file, or `4  DAMAGED`. Click a row to choose it. CANCEL or Escape closes the list. Other keys do nothing.
- The save dialog (the save icon, Ctrl+S) shows LOAD, SAVE, CONTINUE, and QUIT, in Dungeon Master and in Chaos Strikes Back. LOAD opens the load list; a click on a row loads that game, and the game continues after GAME LOADED. CANCEL in a list goes back to the dialog. CONTINUE goes back to the game. QUIT asks first when the game was not saved or loaded in the last 100 ticks, then shows the credits and the entrance or the CSB screen.
- Save: SAVE in the save dialog opens the save list. A click on a row saves to that slot; the old file of the slot becomes the backup of the slot. Dungeon Master then shows GAME SAVED in the save dialog; Chaos Strikes Back goes back to the game (in the prison, the new adventure starts in that slot). There is no question before a save over a used slot: the backup keeps the previous game, and the next save over the slot replaces that backup.
- Resume: RESUME opens the load list: the 8 slots, then QUICK, the quick save `QUICK.DAT`. EMPTY and DAMAGED rows are gray; a click on them does nothing. The QUICK row loads the quick save as F9 does: it is not a saved game, so THE END offers RESTART THIS GAME only after the first save. CANCEL shows the entrance again.
- The current slot is the slot of the last save or load from a slot since the program start (slot 1 at the start). RESTART THIS GAME loads the current slot without the list. When its file and its backup are missing, the game asks CAN'T FIND SAVED GAME; LOAD SAVED GAME opens the load list.
- Quick save (F5) is not in the save list.
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
| Escape | Freeze the game. Escape again continues. The `advanced` mode opens the options menu instead (see Modes). |
| Return | Wake up a sleeping party |
| Click on the movement arrows | Move and turn |
| Click on a mirror | Show the champion. Then click RESURRECT, REINCARNATE, or CANCEL. |
| Name entry after REINCARNATE | Type the name and the title with the panel keyboard or the keys: letters, Space, `,` `.` `;` `:`, Backspace. Return goes from the name to the title. Click OK to end. A right click types a space. Other keys, also Ctrl+Q and the advanced keys, do nothing. |
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

- Automap: a top-down map of the seen squares in place of the dungeon view. The game time stops while the map is open. A white frame shows the end of the map. The top line shows the level and the party position, for example `LEVEL 7 (30,27)`: the level number is the one of the saved games list. Corridors are light gray, walls are dark gray. A wall that the party can walk through (an imaginary wall) is checkered light and dark gray; an open fake wall is a corridor. Drag the map with the left mouse button to see the whole level; a click without a move does not scroll. The map opens centered on the party.
- Up to 8 sounds play at once, not 2 as in the original.
- Quick save and quick load: one save slot in `QUICK.DAT` in the data folder. `DMGAME.DAT` does not change. The message area shows GAME SAVED, GAME LOADED, NO QUICK SAVE, or QUICK SAVE DAMAGED.
- W, A, S, D, Q, and E move and turn the party. The cursor keys and the keypad also work.
- Rune symbols in red after each rune name on an open scroll.
- Action icons in their own colors.
- Stat meters in the object description (hold the eye with an object in the hand): attack for a weapon, defense for armour, and weight.
- A click on a fountain with an empty hand lets every living champion drink, not only the leader.
- Options menu: Escape in the dungeon view opens it in place of the dungeon view. The game time stops while the menu is open; the music and the sounds go on. Only in the `advanced` mode. The rows:
  - SOUND: the sound volume, 0 (off) to 7 (full).
  - MUSIC VOLUME: the music volume, 0 (off) to 7 (full).
  - MUSIC: ORIGINAL (no music, the default), SUPER NES, or FM-TOWNS. The new music starts at once.
  - FILTER: the pixel-art upscale filter, NONE (the default), SCALE2X, or SCALE3X, as `--filter`. The new filter shows at once. With `--filter`, each game start uses the filter of `--filter`, not the filter of the file.
  - CONTINUE: closes the menu.

  Mouse: click `-` or `+` to change a volume, click the MUSIC or FILTER row for the next value, click CONTINUE to close. Keys: Up and Down (also W and S) select a row; Left and Right (also A and D) change its value; Return on CONTINUE closes; Escape closes from every row.
- Cheat screen: C in the dungeon view opens it in place of the dungeon view. The game time stops while the screen is open. Only in the `advanced` mode. The rows of the cheat page:
  - FULL LIGHT: the view is always bright; the creature rules keep the real light.
  - IMMORTAL: the health of a champion stops at 1; damage, wounds, poison, hunger, and thirst stay.
  - FOOD AND WATER: food and water do not go down; eating and drinking still raise them.
  - ONE-HIT KILL: a hit of the party (melee, projectile, or explosion) with damage above 0 kills the creature. A closing door and a fall do normal damage. An invulnerable creature stays invulnerable.
  - WALK THROUGH WALLS: the party moves into any square inside the map, without damage: a wall, a solid fake wall, and also a door that is not open. The map border and a creature stop the party. A door that closes on the party does not hurt it; the door opens again, as normal. No wall sensor runs for the party in a wall. With the cheat off, the party can leave a wall or door square into a square that does not block.
  - REVEAL MAP: the automap shows all squares of the level. The explored squares do not change: with the cheat off, the automap shows only the explored squares again.
  - QUICK SAVE F5: a quick save; the screen stays open. QUICK LOAD F9: the screen closes, then a quick load.
  - SPELLS: the spell pages. Each line shows the runes of a spell as names, then as numbers, then the spell name, for example `FUL IR (X,4,4) FIREBALL`. A potion has the name of its effect, for example HEALTH POTION. A line that is too wide for the view shows only the numbers. X means any power; the number of a rune is its place in its row of runes, 1 to 6. A click on a spell line (or Return on it) casts the spell, then the screen closes so that you see the effect. The caster is the spell caster of the rune area, the power is MON (a spell with a fixed power rune keeps its power), and the cast costs no mana, always works, and gives no experience. The runes in the rune area stay. Other conditions stay as in a normal cast: a potion needs an empty flask in a hand, a CSB map spell needs the magic map in the action hand, and a dead caster casts nothing; the message area shows the normal message. NEXT shows the next page, BACK the page before, CLOSE closes the screen.
  - CLOSE: closes the screen.

  Mouse: click a row to switch the cheat, to run the item, or to cast the spell. Keys: Up and Down (also W and S) select a row; Return switches or runs it; F5 and F9 run the quick save and the quick load; C and Escape close. The save file of the `advanced` mode keeps the cheats (see below). A new game turns them off.
- Settings file: at each change of a value in the menu, the port writes the values to `SETTINGS.TXT` in the saves folder (the data folder of the adventure, or `--saves DIR`). Each game start in the `advanced` mode reads the file again. The lines are `sound_volume=0..7`, `music_volume=0..7`, `music=original|super-nes|fm-towns`, and `filter=none|scale2x|scale3x`. A missing file, a file above 1 KiB, or a line that the port cannot read gives the default of that value: volume 7, ORIGINAL, and NONE. When the write fails, the message area shows UNABLE TO SAVE OPTIONS and the new values stay in use until the next game start. The `vanilla` and `turnbased` modes do not read the file: they play at full volume without music, with the filter of `--filter` or no filter.
- Music: a track plays one time and does not loop, as in the original versions:
  - FM-Towns DM: each level has a track. A new level stops the current track and starts its track. Game over and game won have a track.
  - FM-Towns CSB: each square of a zone has a track. A new zone requests its track; it starts when the current track ends. A level change stops the music. The prison and game over have a track.
  - Super NES (DM only; in CSB the menu skips it, and `music=super-nes` gives ORIGINAL): a step on one of 22 trigger squares stops the current track and starts its track. Game won plays the end credits.
  - The MP3 files go into `music/` of the game folder: `game/music/` gets `music/Game_Dungeon_Master_JP/*.mp3` and `music/Game_Dungeon_Master_US/Super_NES,*.mp3`, `game-csb/music/` gets `music/Game_Chaos_Strikes_Back_JP/*.mp3`. A missing file gives silence. The music stops at the entrance and at the CSB screen.
- Escape and Tab close an open inventory. In the sleep, the candidate panel, or the frozen game, Escape works as in vanilla.
- The damage that a champion deals stays in the action area for two ticks, not one. A click on the action area in the second tick shows the action icons and acts at once. Only in the `advanced` mode, not in `turnbased`.
- Smooth projectile flight: between two game ticks, the view shows each flying projectile part of the way to its new place. The size changes only at a tick.

Keys of the `advanced` mode:

| Key | Action |
|---|---|
| M, Tab | Open or close the automap. The map opens only in the dungeon view: no inventory, freeze, sleep, or candidate. While the map is open, only M, Tab, Escape, and a drag on the map work. Tab also closes an open inventory. |
| Escape | Close the automap, the cheat screen, or the open inventory. In the dungeon view, open the options menu; Escape closes it again. While the menu is open, M does not open the automap. |
| C | Open or close the cheat screen. It opens only in the dungeon view, when no other screen is open. |
| F5 | Quick save. Only in the dungeon view. |
| F9 | Quick load. Only in the dungeon view. |
| W, S | Move forward, move backward |
| A, D | Move left, move right (sideways) |
| Q, E | Turn left, turn right |

F1 to F4 open the inventories in every mode.

In the `advanced` and `turnbased` modes, the save file has extension blocks after the original save data. The automap block keeps the explored squares of the automap. In the `advanced` mode, a cheat block keeps the cheats that are on. The original part of the file is the same as in a vanilla save, and vanilla loads the file. A file without the automap block, or with a damaged one, loads with an empty automap. A load without the cheat block, with a damaged one, or in the `vanilla` and `turnbased` modes turns all cheats off. The format is in [formats/save.md](formats/save.md#extension-blocks).

In the `turnbased` mode, the game enters the dungeon at the start. The control socket opens after the start screens. A script that waits for the socket must pass `--mode turnbased` to skip the screens. The keyboard and the mouse do not control the game. Closing the window quits.


## Windows

Windows (x86-64): `make windows` cross-builds `build/dm.exe` with MinGW-w64 (`brew install mingw-w64`). The first run downloads the SDL3 MinGW package into `build/sdl-mingw/`. `make dist-win` builds `build/DungeonMaster-win.zip` with `dm.exe` and `SDL3.dll`. The player copies the disk images into the folder of the game.

The Windows build has no control socket, so the LLM mode cannot start there: the mode screen offers VANILLA and ADVANCED only, and `--mode turnbased` stops with an error. Test it with Wine.

On macOS, `make dist` builds `build/DungeonMaster.zip`: the game and SDL3 without game files.

## Library

`build/libdm.dylib` contains the game code without SDL. The tests load it with `ctypes`, see [development.md](development.md).
