# Player manual

How to run DMMac and how to play it. The repository has no game files: you supply your own disk images.

## Run the game

Unpack a release (see the [README](../README.md)) or build the game (see [development.md](development.md)). Copy your `.adf` disk images next to the program and start it:

```sh
build/dm
```

On startup, the game extracts each disk image once: the Dungeon Master v3.60 disk to `game/`, Chaos Strikes Back (CSB) disk A to `game-csb/`, CSB disk B to `game-csb/disk-b/`. The game checks the files against the Amiga disks; a wrong file stops the game with a message in the log.

### Extract the disks by hand

Install the Python tools with `uv sync`, then extract each disk image:

```sh
uv run dm-extract <disk.adf> game
uv run dm-extract "<disk A>.adf" game-csb
uv run dm-extract "<disk B>.adf" game-csb/disk-b
```

The two Chaos Strikes Back (CSB) disks are optional. Each disk needs its own folder: the disks have files with the same names.

## Options

`build/dm --help` prints the options:

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

- `--adventure` and `--mode` skip the start screens.
- `--data DIR` selects the data folder of the selected adventure instead of `game/` or `game-csb/`.
- `--saves DIR` puts the save files, the quick save, and `SETTINGS.TXT` in `DIR` instead of the data folder. The folder must exist.
- `--control PATH` selects the socket of the turn-based mode. The default is `~/.dm/control.sock`.
- `--filter` selects the pixel-art upscale filter. The advanced mode takes the filter of the options menu. The image is sharpest when the window scale is a multiple of the filter factor.
- `--frames N` quits after N frames (50 frames per second).
- `--view` shows the dungeon view with free movement through walls; `--show N` shows one graphic. Both are developer tools.
- The environment variable `DM_AUTO_ENTER=1` skips the start screens and the entrance. The tests use it.

## Start screens

Without `--adventure` and `--mode`, the game asks for the adventure (DUNGEON MASTER or CHAOS STRIKES BACK) and the mode (VANILLA, ADVANCED, or LLM). CHAOS STRIKES BACK is gray when `game-csb/` is incomplete. Up and Down move the cursor, Return selects, a click selects, Escape goes back. Ctrl+Q or closing the window ends the program at any time.

Dungeon Master then shows the FTL logo, the title animation (a key or a click stops it), and the entrance. Chaos Strikes Back shows a third screen: NEW ADVENTURE, RESUME, and QUIT.

## Entrance

Click ENTER or press Return to start a new game. Click RESUME to continue the saved game. QUIT goes back to the start screens.

## Chaos Strikes Back

1. Select NEW ADVENTURE. The game plays the intro of disk B (about 3.6 minutes; a key or a click stops it). Then the game starts in the prison.
2. Recruit one to four champions at the 24 mirrors.
3. Save the game. In the prison, the save creates the new adventure and continues in the CSB dungeon. The original needs a restart with the utility disk for this step. The save needs at least one champion, and each champion needs a unique first name; otherwise a dialog gives the reason.

In the advanced mode, the stairs down of the prison also start the new adventure, without a save. The automap shows the stairs down in orange and the hidden passages to them checkered. In vanilla, the stairs lead to the lower level of the prison, as in the original.

- RESUME continues the saved adventure.
- In vanilla, the save dialog shows CANCEL, SAVE AND PLAY, and SAVE AND QUIT. CSB has no quit without a save, as in the original. SAVE AND QUIT shows the credits and the CSB screen.
- When the party dies, THE END shows with RESTART THIS GAME. When the party wins, the game plays the ending animation; a key or a click then shows the CSB screen.

## Saved games

The game saves to `DMGAME.DAT` in `game/`; Chaos Strikes Back saves to `CSBGAME.DAT` in `game-csb/`. The old file becomes the `.BAK` backup. The format is the original Amiga save file ([formats/save.md](formats/save.md)).

- Save: click the save icon in an inventory, or press Ctrl+S. The dialog shows CANCEL, SAVE, and QUIT. QUIT ends the game; when the game was not saved in the last 100 ticks, QUIT asks first.
- Resume: click RESUME at the entrance. A damaged file shows "THE GAME IS DAMAGED!" and the entrance again.
- When all champions die, THE END shows. RESTART THIS GAME loads the saved game. QUIT shows the credits and the entrance.
- The original quits the program after the credits and after a damaged save; the port shows the entrance.

### Save slots (advanced mode)

The advanced mode has 8 save slots. Slot 1 is the file of the other modes (`DMGAME.DAT`); slots 2 to 8 are `DMGAME2.DAT` to `DMGAME8.DAT`, each with its own backup. CSB uses `CSBGAME*.DAT` in the same way.

- The save dialog (the save icon, Ctrl+S) shows LOAD, SAVE, CONTINUE, and QUIT. LOAD and SAVE open the slot list: one row per slot with the level and the date of the file, `EMPTY`, or `DAMAGED`. Click a row to load or save; CANCEL or Escape goes back.
- A save over a used slot asks nothing: the backup of the slot keeps the previous game.
- RESUME at the entrance opens the slot list; the last row is QUICK, the quick save.
- RESTART THIS GAME loads the slot of the last save or load.

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
| Escape | Freeze the game. Escape again continues. The advanced mode opens the options menu instead. |
| Return | Wake up a sleeping party |
| Click on the movement arrows | Move and turn |
| Click on a mirror | Show the champion. Then click RESURRECT, REINCARNATE, or CANCEL. |
| Name entry after REINCARNATE | Type the name and the title, or click the panel keyboard. Return goes from the name to the title. Click OK to end. |
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

The game rules are the same in all modes.

| Mode | Behavior |
|---|---|
| `vanilla` | Default. The original game. The advanced keys do nothing. |
| `advanced` | The original game with the advanced features below. |
| `turnbased` | The advanced rules without the keys (8 sounds, colored icons, stat meters), and a program plays through a Unix socket. See [llm-mode.md](llm/llm-mode.md). Not on Windows. |

### Advanced features

| Key | Action |
|---|---|
| W, S | Move forward, move backward |
| A, D | Move left, move right (sideways) |
| Q, E | Turn left, turn right |
| M, Tab | Open or close the automap. Tab also closes an open inventory. |
| Escape | Open or close the options menu. Also closes the automap, the cheat screen, or an open inventory. |
| C | Open or close the cheat screen |
| F5, F9 | Quick save, quick load |

- Automap: a top-down map of the squares the party saw, in place of the dungeon view. The game time stops while the map is open. The top line shows the level and the party position. Corridors are light gray, walls dark gray, a wall that the party can walk through checkered. Drag the map with the mouse to see the whole level.
- Quick save and quick load: one extra save in `QUICK.DAT`. The saved game does not change.
- Up to 8 sounds play at the same time, not 2 as in the original.
- Rune symbols in red after each rune name on an open scroll.
- Action icons in their own colors.
- Stat meters in the object description (hold the eye with an object in the hand): attack, defense, and weight.
- A click on a fountain with an empty hand lets every living champion drink.
- The damage that a champion deals stays in the action area for two ticks. A click during the second tick starts the next action without a wait.
- Smooth projectile flight between two game ticks.
- The save file keeps the explored squares of the automap and the cheats that are on. Vanilla loads the file and ignores them.

### Options menu

Escape in the dungeon view opens the menu; the game time stops. Rows: SOUND (volume 0 to 7), MUSIC VOLUME (0 to 7), MUSIC (ORIGINAL, SUPER NES, or FM-TOWNS), FILTER (NONE, SCALE2X, or SCALE3X), CONTINUE. Click `-` or `+` to change a volume. Click a row for its next value. Keys: Up and Down (W, S) select a row, Left and Right (A, D) change it. Escape closes the menu.

The game writes the values to `SETTINGS.TXT` in the saves folder and reads them at each start of the advanced mode. The other modes play at full volume without music.

Music (ORIGINAL plays no music):

- FM-Towns Dungeon Master: each level has a track. FM-Towns Chaos Strikes Back: each zone has a track. Super NES (Dungeon Master only): 22 trigger squares start a track. A track plays once and does not loop, as in those versions.
- The MP3 files go into `music/` of the game folder (`game/music/`, `game-csb/music/`), named `FM-Towns,Track_01,Music.mp3` or `Super_NES,Track_01,Music.mp3` with the track number. A missing file gives silence.

### Cheat screen

C in the dungeon view opens the cheat screen; the game time stops. Click a row, or select it with Up and Down (W and S) and press Return. C and Escape close the screen. A new game turns all cheats off.

- FULL LIGHT: the view is always bright. The creatures keep the real light.
- IMMORTAL: the health of a champion stops at 1. Wounds, poison, hunger, and thirst stay.
- FOOD AND WATER: food and water do not go down.
- ONE-HIT KILL: a hit of the party kills the creature. Invulnerable creatures stay invulnerable.
- WALK THROUGH WALLS: the party moves into walls, fake walls, and closed doors. The map border and creatures stop the party.
- REVEAL MAP: the automap shows all squares of the level. The explored squares do not change.
- QUICK SAVE F5, QUICK LOAD F9.
- SPELLS: the spell pages, for example `FUL IR (X,4,4) FIREBALL`. X means any power; a number is the place of the rune in its row. A click on a line casts the spell with the caster of the rune area at power MON, without mana cost or experience. The normal conditions stay: a potion needs an empty flask, a CSB map spell needs the magic map in the action hand.
- CLOSE: closes the screen.
