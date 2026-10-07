# CSB integration: audit and design

This page audits the C port for places that assume Dungeon Master (DM). It then proposes the minimum design for the adventure selection (DM or Chaos Strikes Back, CSB). It covers the adventure value, the selection screens, the argument parsing, the CSB entrance, the game loop, and the tests.

Status: proposal. Nothing is implemented. The user approved the user-facing flow (see [Approved flow](#approved-flow)). The design below is not approved yet.

## Approved flow

1. Title screen on a black background, in the DM font: DUNGEON MASTER, CHAOS STRIKES BACK. Click, or arrows and Return.
2. Mode screen: VANILLA, ADVANCED, LLM. LLM is `--mode turnbased`. Escape goes back to step 1.
3. DM: the existing entrance (ENTER, RESUME, QUIT), unchanged. CSB: a screen with NEW ADVENTURE, RESUME, QUIT. NEW ADVENTURE builds `CSBGAME.DAT` from `MINI.DAT` and opens the champion selection. RESUME loads `CSBGAME.DAT`.
4. New flag `--adventure dm|csb`. When `--adventure` and `--mode` are both given, the selection screens are skipped.
5. When the CSB data is missing, CHAOS STRIKES BACK is grey and cannot be selected.
6. LLM mode goes straight into the dungeon after the selection, as `turnbased` does today.
7. Data folders: DM in `game/`. CSB disk A in `game-csb/`. CSB disk B in `game-csb/disk-b/`. CSB save: `CSBGAME.DAT` and `CSBGAME.BAK` in `game-csb/`, or in the folder of `--saves`.

## Summary of the audit

- The selection layer is small. The loading and save code take a directory, so most of the file work needs only a file name per adventure.
- One finding is large and blocks CSB play, not the selection: CSB has no `KAOS.FTL`. The game engine of CSB is another module (see [Blocker](#blocker-the-csb-engine-module)). The port reads 147 constants as KAOS addresses.
- The selection screens, the argument parsing, the adventure value, and the save file names do not depend on the blocker. Build them first.
- `NEW ADVENTURE` depends on two unknowns: the format of `MINI.DAT` and the CSB champion selection. This page does not design them.

## Audit

### Blocker: the CSB engine module

Facts from the files:

| Item | DM (`game/`) | CSB disk A (`game-csb/`) |
|---|---|---|
| Engine module | `KAOS.FTL`, 116228 bytes | No `KAOS.FTL`. `APPB.FTL` has 123420 bytes (DM: 22038). It holds the strings `DF0:CSBGame.dat` and `DF0:CSBGame.bak`. |
| `Graphics.DAT` | 458108 bytes | 402254 bytes |
| `Dungeon.DAT` | 25006 bytes (compressed, magic `0x8104`) | 2098 bytes (magic `0x8104`) |
| Other | `DungeonF.DAT`, `DungeonG.DAT` | `ENDA.DAT`, `BETRAYAL!` |

CSB disk B (`game-csb/disk-b/`): `MINI.DAT` (42784 bytes, the first bytes are not the `0x8104` magic), `CEDT.FTL`, `HINT.FTL`, `HCSB.DAT`, `HCSB.HTC`, `ANIM.DAT`, `TITL.DAT`, `Portrait/*.CMP` (24 files), `Sound/`, `MIDI/`.

Conclusion (to verify with the decompilation in `decomp/csb-a/`): the main module of CSB is `APPB.FTL` of disk A. The port cannot run CSB until it loads that module and maps its addresses.

Code that depends on the KAOS image:

| Place | Dependency |
|---|---|
| `ftl.c`: `kaos_load`, `kaos_u8/u16/s16/u32/ptr`, `KAOS_BASE` | One global image. `kaos_*` abort when an address is outside the image. |
| 25 files with `kaos_*` calls | 147 `#define ... 0x10xxxx` data addresses (for example `gameloop.c`, `command.c`, `startend.c`, `dialog.c`, `ui.c`, `view.c`: 46 calls, `tables.c`: 26 calls). |
| `tables.c`, `tables.h` | Table sizes of DM: `CREATURE_TYPE_COUNT` 27, `OBJECT_INFO_COUNT` 180, `WEAPON_TYPE_COUNT` 46, `ARMOUR_TYPE_COUNT` 58, `JUNK_TYPE_COUNT` 53, `FOOD_TYPE_COUNT` 8, `DOOR_TYPE_COUNT` 4, `SPELL_COUNT` 25, `ACTION_COUNT` 44, `SLOT_MASK_COUNT` 38, `FIXED_POSSESSION_COUNT` 40. |
| `sound.h`, `sound.c` | `SOUND_COUNT` 35. Volume table at `0x100696`. |
| `ids.h` | 252 lines of `GFX_*`, `ZONE_*`, `TEXT_*` numbers for DM `Graphics.DAT`. |

This is a separate work item ("CSB engine"). It needs a decision first: one address table per adventure, or a rebuild of the tables from the CSB module. This page does not decide it.

### File names and paths

| File | Place | Hard-coded value |
|---|---|---|
| `port/src/platform/main.c` | `main` | `data = "game"` (default). |
| | `load_kaos_and_graphics` | `"%s/KAOS.FTL"`, `"%s/Graphics.DAT"`. Used by `--show` and `--view`. |
| | `view_start` | `"%s/Dungeon.DAT"`. |
| | `show` (`--show`) | The palette comes from the KAOS address `DUNGEON_PALETTE` (`0x100aa6`). |
| `port/src/ui/gameloop.c` | `gameloop_load_data` | `"%s/KAOS.FTL"`, `"%s/Graphics.DAT"`. Sets the save directory to `dir`. |
| | `gameloop_new_game` | `"%s/Dungeon.DAT"`. |
| `port/src/platform/control.c` | `handle_new_game` | The error text `"cannot load Dungeon.DAT"`. It uses `gameloop_data_dir()`, so the folder is right. |
| | `handle_load`, `handle_request` (`save`) | Use `startend_save_dir()` and `savegame_load/save`. The folder is right. |
| `port/src/engine/savegame.h` | `SAVE_FILE_NAME`, `SAVE_BACKUP_FILE_NAME` | `"DMGAME.DAT"`, `"DMGAME.BAK"`. Used in `savegame.c`: `savegame_save` and `savegame_load` (4 places). |
| `port/src/ui/startend.c` | `save_dir` | Default `"."`. `startend_set_save_dir` is called by `gameloop_load_data`. |
| `port/src/engine/dungeon.c` | `g_dungeon_id` | Default `10` (the DM dungeon id). The save header stores it. |
| `port/src/engine/savefile.h` | `SAVE_*_BYTES` | DM save layout, including `SAVE_HEADER_EXTRA_BYTES` 134. The CSB save format is not known. |
| `.gitignore` | | `game-csb/` and `decomp/` are ignored (the file has an uncommitted change). |
| `game/CSBGAME.DAT` | | An empty file (0 bytes) in the DM folder. The port ignores it. |

The DM font comes from `Graphics.DAT` (`GFX_FONT`, item 695). `text_init` reads only this item. It needs no KAOS image.

### Entrance

| Place | Assumption |
|---|---|
| `gameloop.c`: `entrance_draw` | Draws DM graphic 4 with doors. Palette `ENTRANCE_PALETTE` (`0x100a66`). The buttons ENTER, RESUME, QUIT are part of the graphic. |
| `command.c`: `command_set_entrance_input`, `KEYS_ENTRANCE` (`0x10393c`), `UI_INPUT_ENTRANCE` | Mouse table and key table come from the KAOS data. |
| `command.c`: `process_other_command` | `COMMAND_ENTRANCE_ENTER`, `COMMAND_ENTRANCE_ENTER_DUNGEON_B` set `g_new_game = ENTRANCE_NEW_GAME`. `COMMAND_ENTRANCE_RESUME` sets `ENTRANCE_RESUME`. The comment says credits (203) are not ported. |
| `main.c`: `show_entrance`, `entrance_frame` | Draw the DM entrance again and run `gameloop_new_game(data)` or `startend_load_saved_game()`. |
| `main.c`: `main` | `MODE_ENTRANCE` is the first mode after the start. `DM_AUTO_ENTER` (and `turnbased`) presses Return at the start. |

### Endgame and credits

| Place | Assumption |
|---|---|
| `startend.c`: `endgame`, `show_credits`, `draw_the_end`, `draw_champion_parade` | DM graphics `GFX_CREDITS` 5, `GFX_THE_END` 6, `GFX_CHAMPION_FRAME` 346. Palette `CREDITS_PALETTE`. |
| `startend.c`: `fuse_sequence`, `endgame_sensor`, `show_final_texts` | The fusion of Lord Chaos and Lord Order. Final texts on square (0, 0). CSB ends differently (`ENDA.DAT`). |
| `startend.c`: `endgame` sets `g_return_to_entrance` | `main.c` then shows the DM entrance. For CSB this must show the CSB entrance. |
| `startend.c`: `startend_load_saved_game` | Uses `savegame_load(save_dir)`. The texts come from DM `gfx_string`. |

### Mode handling

| Place | Assumption |
|---|---|
| `advanced.h`: `GAME_MODE_VANILLA`, `GAME_MODE_ADVANCED`, `GAME_MODE_TURNBASED`, `g_game_mode` | The value is set only by `--mode`. |
| `main.c`: `game_mode_of`, `main` | The parser reads `--mode` in a loop over `argv`. `g_advanced_features` and the local `turnbased` are set once, before the window opens. `auto_enter` and the control socket (`control_open`) depend on `turnbased` at the start. The loop uses `turnbased` in five places. |
| `main.c`: `read_input` | Drops all input when `turnbased` and `mode >= MODE_ENTRANCE`. Escape quits for `mode < MODE_ENTRANCE` (`--show`, `--view`). |
| `main.c`: `amiga_key` | Maps the advanced keys only outside vanilla. |
| `main.c`: `g_frame_hook` | `NULL` in `turnbased`, else `frame_hook`. |

### Hard-coded DM knowledge in game rules

These items are not selection work. The CSB engine work item must check them.

| Place | Value |
|---|---|
| `state_json.c`: `creature_names[CREATURE_TYPE_COUNT]`, `statistic_names`, `skill_names`, `slot_names`, `floor_ornament_names`, `wound_names` | DM names and counts. |
| `recruit.c`, `recruit.h` | The champion selection is the mirror text of the Hall of Champions in the dungeon. CSB has `Portrait/*.CMP` files on disk B and the editor `CEDT.FTL`. |
| `savegame.c`, `savefile.c` | DM save layout (5 blocks, 4 portraits of 464 bytes, automap extension block). |
| `dungeon.c` | `NEW_GAME_EXTRA_THINGS` (`0x101058`), `FREE_SQUARE_THINGS` 300, `NEW_GAME_EVENT_COUNT` 100, `MAX_THINGS` 1024. |
| `game.h` | `CHAMPION_MAX_COUNT` 4, `ACTIVE_GROUP_MAX_COUNT` 110. |

### LLM tools

| Place | Assumption |
|---|---|
| `tools/dmtools/control_client.py` | `DEFAULT_SOCKET = "~/.dm/control.sock"`. One socket for all adventures. No change needed. |
| `tools/dmtools/mcp_server.py`: `INSTRUCTIONS` | "Dungeon Master (1987)", "find the Firestaff, defeat Lord Chaos", "recruit up to four champions from the mirrors in the Hall of Champions on the first level". |
| `mcp_server.py`: `save_game` docstring | Names `DMGAME.DAT` and `DMGAME.BAK`. |
| `mcp_server.py`: `new_game`, `load_game` | Names the same files in the descriptions. The socket commands use the loaded adventure, so the code needs no change. |
| `tools/dmtools/play.py` | The prog description "Dungeon Master control socket". The log folder `playthroughs/current/coordinator`. The view and state text is generic. |
| `docs/llm-playbook.md` | Playthrough rules for DM (goal, mirrors, `playthroughs/current/player/progress.md`). The save names in the "Save, load" lines are not file names. |
| `docs/llm-mode.md` | The sections "Prerequisites" (`game/`), the socket table (`save`, `load`, `new_game` name `DMGAME.DAT`, `Dungeon.DAT`), and "Saved games" (save directory is `game/`). |
| `docs/superpowers/status.md` | The resume steps copy `DMGAME.DAT` to `game/`. |

The socket protocol needs no new command. The game starts one adventure for each run. The adventure is the one that the selection or `--adventure` chose.

### Tests that assume DM

| Test file | Assumption |
|---|---|
| `tests/test_gameloop.py`, `test_modes.py`, `test_port_data.py`, others | `GAME = ROOT / "game"`. They load the library and call `gameloop_load_data(GAME)` and `gameloop_new_game(GAME)`. They skip when `KAOS.FTL`, `Graphics.DAT`, or `Dungeon.DAT` is missing. |
| `tests/test_modes.py`, `test_save.py`, `test_control.py` | `SAVE = "DMGAME.DAT"`, `BACKUP = "DMGAME.BAK"`. |
| `tests/test_gameloop.py` (the `--frames 100` run) | Starts `build/dm` with `DM_AUTO_ENTER=1` and no `--mode`. |
| `tests/test_modes.py` (the turn-based program test) | Starts `build/dm --mode turnbased --control PATH` with no `--adventure`. |
| `tests/test_modes.py` | Starts `build/dm --mode fast`. It expects exit code 1. |
| `tests/test_port.py` (`test_smoke_run`) | Starts `build/dm --frames 5` with no flags. |

## Design

### Principles

- One new value: the adventure. One new small module for it. One new module for the screens.
- No table of function pointers, no adventure struct with callbacks. A `switch` or a `?:` on `g_adventure` is enough for two adventures.
- All new logic is in libdm (no SDL), so pytest can test it through `ctypes`. `main.c` keeps only the SDL glue.
- Existing DM behavior does not change. The DM entrance, the save file names, and the default folder stay as they are.

### The adventure value

New files `port/src/data/adventure.h` and `port/src/data/adventure.c`.

```c
enum { ADVENTURE_DM, ADVENTURE_CSB };

extern int16_t g_adventure;                 /* ADVENTURE_DM by default; not reset by game_new */

/* The adventure of a --adventure value, or -1. */
int adventure_of(const char *name);
/* The default disk A folder: "game" (DM) or "game-csb" (CSB). */
const char *adventure_default_dir(int16_t adventure);
/* The save and backup file names of the adventure: DMGAME.DAT/.BAK or CSBGAME.DAT/.BAK. */
const char *adventure_save_name(void);
const char *adventure_backup_name(void);
/* 1 when the CSB files exist in dir: APPB.FTL, Graphics.DAT, Dungeon.DAT, and disk-b/MINI.DAT. */
int adventure_csb_present(const char *dir);
```

`g_adventure` is a global like `g_game_mode` (`advanced.c`). It is not in `game.c`, because `game_new` resets the game state and the adventure must live longer than one game.

Changes that use it:

| File | Change |
|---|---|
| `savegame.h` | Remove `SAVE_FILE_NAME` and `SAVE_BACKUP_FILE_NAME`. |
| `savegame.c` | `savegame_save` and `savegame_load` call `adventure_save_name()` and `adventure_backup_name()`. The comment in `savegame.h` names both files. |
| `gameloop.c` | `gameloop_load_data` and `gameloop_new_game` stay as they are for DM. The CSB branch is part of the CSB engine work item (see [Dependencies](#dependencies-for-play)). |
| `control.c` | `handle_new_game` error text becomes `"cannot start the new game"` (no file name). |

The data folder is not stored in a second global. `gameloop_data_dir()` already returns the folder that `gameloop_load_data` read. `main` computes the folder once:

```c
data = data_override ? data_override : adventure_default_dir(g_adventure);
```

`--data DIR` overrides the disk A folder of the chosen adventure. Disk B is always `<disk A folder>/disk-b`.

### The selection module

New files `port/src/ui/select.h` and `port/src/ui/select.c`. The module has no SDL calls. It owns the three menu screens and their state.

```c
enum { SCREEN_ADVENTURE, SCREEN_MODE, SCREEN_CSB, SCREEN_DONE };
enum { CSB_NEW_ADVENTURE, CSB_RESUME, CSB_QUIT };
enum { SELECT_KEY_UP, SELECT_KEY_DOWN, SELECT_KEY_ENTER, SELECT_KEY_ESCAPE };

/* Starts at the first screen that --adventure and --mode do not skip. adventure and mode are -1 when the flag is absent. */
void select_start(int16_t adventure, int16_t mode, int csb_present);
/* Starts the CSB screen alone: after a game of CSB ends, and after Escape on the DM side. */
void select_start_csb(void);
int select_screen(void);               /* the current screen, or SCREEN_DONE */
int select_csb_choice(void);           /* CSB_NEW_ADVENTURE, CSB_RESUME, or CSB_QUIT after SCREEN_DONE */
void select_key(int key);              /* SELECT_KEY_* */
void select_click(int x, int y);       /* a left click in screen pixels */
void select_draw(void);                /* draws the current screen on fb */
```

Results go to `g_adventure` and `g_game_mode`. `select_start` with both values given returns at once with `select_screen() == SCREEN_DONE`.

State and rules (all in `select.c`):

- A screen has a list of items, a cursor index, and a per-item "enabled" flag. Only the item CHAOS STRIKES BACK can be disabled (when `csb_present` is 0).
- Up and Down move the cursor. A disabled item is skipped. Return activates the item at the cursor. A click on an item moves the cursor to it and activates it. A click on a disabled item or outside every item does nothing.
- The cursor starts on the first enabled item.
- `SCREEN_ADVENTURE`: activating sets `g_adventure`. The next screen is `SCREEN_MODE`, except when `--mode` was given (then `SCREEN_DONE`, or `SCREEN_CSB` for CSB).
- `SCREEN_MODE`: activating sets `g_game_mode` (VANILLA 0, ADVANCED 1, LLM = `GAME_MODE_TURNBASED` 2) and sets `g_advanced_features = g_game_mode != GAME_MODE_VANILLA`. The next screen is `SCREEN_DONE` for DM and `SCREEN_CSB` for CSB. Escape goes back to `SCREEN_ADVENTURE` (when `--adventure` was given, Escape does nothing).
- `SCREEN_CSB`: NEW ADVENTURE, RESUME, QUIT. Activating stores the choice and goes to `SCREEN_DONE`. QUIT sets `g_quit_requested`. Escape goes back to `SCREEN_MODE` (see [Open decisions](#open-decisions)).
- Cursor and click boxes use the same item rectangles, computed from `text_size` of the item. There are no mouse tables from the KAOS data.

#### Drawing with the DM font

Reuse these existing functions. Do not add a font renderer.

| Need | Function | File |
|---|---|---|
| Load the font without KAOS | `gfx_open(path)` and `text_init()` | `graphics.c`, `text.c` |
| Text on `fb` | `text_draw_screen(x, y, fg, bg, s)` | `text.c` |
| Centered text width | `text_size(s, &w, &h)`, `TEXT_CHAR_WIDTH`, `TEXT_LINE_HEIGHT` | `text.c`, `text.h` |
| Black background | `memset(fb, COLOR_BLACK, sizeof fb)` | `video.h` |
| Own palette, no KAOS | `ui_set_screen_palette(colors)` with a 16-entry array | `ui.c` |
| Color names | `COLOR_BLACK`, `COLOR_DARK_GRAY`, `COLOR_LIGHT_GRAY`, `COLOR_GOLD` | `ui.h` |

Colors: normal item `COLOR_LIGHT_GRAY`. Cursor item `COLOR_GOLD`. Disabled item `COLOR_DARK_GRAY`. The module fills the palette array itself (black, three grays, gold), so the screens work before any KAOS file is loaded.

The font data comes from DM `Graphics.DAT` (`game/`). When the file is missing, `main` tries `game-csb/Graphics.DAT`. Verify that `GFX_FONT` has the same item number in the CSB file. If neither file loads, the program exits with a log line, as it does today.

Open check before coding: `advanced_draw_debug` draws plain ASCII with `text_draw`. `endgame_print_text` in `startend.c` maps A-Z to the codes 1-26. Draw one test screen and look at the glyphs. If ASCII capitals look wrong, move `endgame_print_text` into `text.c` and use it.

Layout (320 x 200): the title is not drawn as a graphic. Each screen shows a heading line (`ADVENTURE`, `MODE`, `CHAOS STRIKES BACK`) at y = 56 and the items at y = 88, 104, 120 (16 pixels apart), centered. The item rectangle is the text width plus 8 pixels on each side and 12 pixels high. The numbers are first values. Adjust them at the first look.

### Argument parsing

All in `main.c`, in the existing `argv` loop. Add:

```c
if (!strcmp(argv[i], "--adventure") && (adventure = adventure_of(argv[i + 1])) < 0)
    return SDL_Log("--adventure must be dm or csb"), 1;
```

`--mode` writes to a local `mode_arg` (-1 when absent) instead of `g_game_mode`. `select_start(adventure, mode_arg, csb_present)` sets the globals.

Rules for the flags (see [Open decisions](#open-decisions), point 1):

| Flags given | Screens shown |
|---|---|
| Neither | Title, mode, then CSB screen for CSB. |
| `--adventure` and `--mode` | None. DM goes to the entrance as today. CSB goes to the CSB screen. |
| `--adventure` only | Mode screen only. |
| `--mode` only | None. The adventure is DM. This keeps `build/dm --mode turnbased` and the tests working. |
| `--adventure csb` when CSB is missing | Exit with the log line `CSB data is missing in <dir>`. |
| `--show`, `--view` | No screens. Always DM unless `--adventure csb`. |
| `DM_AUTO_ENTER` set | No screens. The adventure is DM unless `--adventure` says otherwise. The mode is vanilla unless `--mode` says otherwise. The tests use this. |

`csb_present` is `adventure_csb_present(adventure_default_dir(ADVENTURE_CSB))`. When `--data DIR` is given, the check uses `DIR` for the selected adventure only (see [Open decisions](#open-decisions), point 4).

### The game loop and the screen switch

Change only `main.c`. The enum becomes:

```c
enum { MODE_SHOW, MODE_VIEW, MODE_SELECT, MODE_ENTRANCE, MODE_GAME };
```

`MODE_SELECT` is below `MODE_ENTRANCE`. `read_input` already ignores game input for `mode < MODE_ENTRANCE`, so only one new branch is needed.

Changes in `main.c`:

1. `turnbased` becomes `static int turnbased`. A new function `apply_mode(void)` sets `turnbased`, `g_advanced_features`, `auto_enter`, `g_frame_hook`, and opens the control socket (the code that is now in `main` between `plat_init` and the loop). It runs once, when the selection ends. The default control path code moves into it.
2. Start: when no screen is needed, `main` runs `select_start` (it returns `SCREEN_DONE` at once) and `start_adventure()` directly. Otherwise `mode = MODE_SELECT` and `select_start_font()` loads the font. The window opens before the first frame, as today.
3. `read_input`, new first branch for `MODE_SELECT`: map the SDL key to `SELECT_KEY_*` (Up, Down, Return and keypad Enter, Escape), call `select_key`. For `PLAT_MOUSE` with the left button, call `select_click(e.x, e.y)`. Then `continue`. Ctrl+Q and window close still set `g_quit_requested`.
4. The frame loop: for `mode == MODE_SELECT`, call `select_draw()` when the screen changed (or every frame: it is a 320 x 200 `memset` and a few strings), then check `select_screen() == SCREEN_DONE`. When done, call `start_adventure()`.
5. `start_adventure()`:
   - DM: `apply_mode()`, `gameloop_load_data(data)`, `entrance_draw()`, `command_set_entrance_input()`, `mode = MODE_ENTRANCE`. This is the code of today's `if (mode == MODE_ENTRANCE)` block in `main`. `auto_enter` presses Return afterward, as today.
   - CSB, choice RESUME or NEW ADVENTURE: `apply_mode()`, then the CSB engine start (see [Dependencies](#dependencies-for-play)). Until that exists, the function logs `CSB play is not available yet` and returns to `SCREEN_CSB`.
   - CSB, choice QUIT: `g_quit_requested = 1`.
6. The pointer: `set_pointer(...)` runs only for `mode >= MODE_ENTRANCE`. In `MODE_SELECT` the pointer shows the arrow from the KAOS data, which is not loaded. Use `plat_set_pointer(NULL, ...)` (system pointer) in `MODE_SELECT`. The mouse position is mapped to game pixels by the platform layer, so clicks work without a pointer image.
7. `show_entrance` and the `g_return_to_entrance` branch: when `g_adventure == ADVENTURE_CSB`, call `select_start_csb()` and set `mode = MODE_SELECT` instead of `entrance_draw`. For DM nothing changes.
8. `entrance_frame` stays for DM. The CSB choice is handled by `start_adventure`, not by `g_new_game`.

`turnbased` and the selection: the `LLM` item sets `g_game_mode = GAME_MODE_TURNBASED`. `apply_mode` then sets `auto_enter = 1`, so the program enters the dungeon without the entrance, as today. The socket opens after the selection, not before. A script that starts `build/dm` and waits for the socket sees the socket only after the user chooses LLM. Scripts must pass `--adventure` and `--mode` (or only `--mode`) to skip the screens.

### CSB entrance screen

`SCREEN_CSB` in `select.c` (three items). It replaces the DM entrance graphic for CSB. It has no doors and no graphic. It uses the same drawing code as the other two screens.

- NEW ADVENTURE: `select_csb_choice() == CSB_NEW_ADVENTURE`. `start_adventure` calls `csb_new_adventure(dir)`: build `CSBGAME.DAT` from `disk-b/MINI.DAT`, then open the champion selection.
- RESUME: `CSB_RESUME`. `start_adventure` calls the CSB loader for `<dir>/CSBGAME.DAT`, or `CSBGAME.BAK` when the first file is missing (same rule as `savegame_load`).
- QUIT: ends the program.
- RESUME with no save file: show the existing dialog `TEXT_CANT_FIND_SAVED_GAME` (`startend_load_saved_game` does this for DM).

### Dependencies for play

These parts are outside the selection layer. They need their own research and plan.

1. The CSB engine module (see [Blocker](#blocker-the-csb-engine-module)). It decides how `gameloop_load_data` loads CSB.
2. The format of `MINI.DAT` and the build of `CSBGAME.DAT`. This page does not describe them.
3. The CSB champion selection (the data on disk B, `CEDT.FTL`, `Portrait/*.CMP`).
4. The CSB save format, the CSB credits, and the CSB endgame (`ENDA.DAT`). `startend.c` needs a CSB branch.
5. `--show` and `--view` for CSB: the palettes come from KAOS addresses.

Until items 1 and 2 exist, `start_adventure` for CSB returns to the CSB screen with a log line.

### LLM tools

| File | Change |
|---|---|
| `mcp_server.py` | `INSTRUCTIONS`: name the game as "Dungeon Master" or "Chaos Strikes Back", read from a new response field `adventure` of `get_state`. Use the Lord Chaos goal only for DM. Change the save file names in the docstrings of `save_game`, `load_game`, and `new_game` to "the save file of the adventure". |
| `state_json.c` | Add `"adventure": "dm"|"csb"` to the state object. |
| `play.py` | Change the description to "Sends one request to the game control socket." No code change. |
| `docs/llm-mode.md` | The socket table: `save`, `load`, and `new_game` use the files of the running adventure. Add `--adventure` to the start command. Describe that the selection screens are skipped by `--mode` and `--adventure`. |
| `docs/llm-playbook.md` | Mark the playbook as DM only. A CSB playbook is a later item. |
| `README.md` | Add a section "Adventures": `--adventure`, the screens, the folders, the CSB save file. |
| `docs/superpowers/status.md` | No change until CSB runs. |

The control socket and `dm-play` keep one default path. Two instances need `--control` and `DM_CONTROL`, as today.

### Tests select the adventure

- C-level tests set the global, as `test_modes.py` does for the mode: `c16.in_dll(lib, "g_adventure").value = 1`. The default is DM, so the existing tests do not change.
- Process-level tests pass `--adventure dm --mode vanilla` (or `DM_AUTO_ENTER=1`). The existing process tests work without edits (see the flag rules).
- The `--adventure csb` tests use a temporary folder with empty marker files (`APPB.FTL`, `Graphics.DAT`, `Dungeon.DAT`, `disk-b/MINI.DAT`). They check only the presence logic. They do not start CSB play.
- A test fixture `adventure_dir(tmp_path)` creates the folder. It lives in `tests/test_select.py`.

## Automated tests

New file `tests/test_select.py`. It builds `build/libdm.dylib` with `make -C port` (as `tests/test_port.py` does) and uses `ctypes`. It needs no game files, except the three tests marked "font". They skip when `game/Graphics.DAT` is missing.

Argument and adventure helpers:

| Test | Check |
|---|---|
| `test_adventure_of` | `adventure_of(b"dm") == 0`, `b"csb" == 1`, `b"DM"`, `b""`, and `b"x"` give -1. |
| `test_default_dirs` | `adventure_default_dir(0) == b"game"`, `(1) == b"game-csb"`. |
| `test_save_names` | With `g_adventure` 0: `DMGAME.DAT` and `DMGAME.BAK`. With 1: `CSBGAME.DAT` and `CSBGAME.BAK`. |
| `test_csb_present` | An empty folder gives 0. A folder with the four marker files gives 1. A folder with one file missing gives 0 (parametrized over the four files). |
| `test_save_writes_csb_name` | With `g_adventure` 1 and a started game, `savegame_save(tmp)` creates `CSBGAME.DAT` and not `DMGAME.DAT`. A second save makes `CSBGAME.BAK`. Reuse the game setup of `tests/test_save.py`. |

Selection logic (no drawing):

| Test | Check |
|---|---|
| `test_start_no_flags` | `select_start(-1, -1, 1)`: `select_screen() == SCREEN_ADVENTURE`. |
| `test_start_both_flags_done` | `select_start(1, 2, 1)`: `SCREEN_CSB`. `select_start(0, 2, 1)`: `SCREEN_DONE`, `g_adventure == 0`, `g_game_mode == 2`. |
| `test_start_adventure_only` | `select_start(0, -1, 1)`: `SCREEN_MODE`. |
| `test_start_mode_only` | The rule "`--mode` alone means DM": `select_start(-1, 2, 1)` gives `SCREEN_DONE` and `g_adventure == 0`. |
| `test_return_selects_dm` | Return on the first screen: `g_adventure == 0`, next `SCREEN_MODE`. |
| `test_arrows_select_csb` | Down, Return: `g_adventure == 1`. Up from the second item returns to the first. |
| `test_cursor_wraps_or_stops` | Up on the first item and Down on the last item do not leave the list (the chosen rule, see open decisions). |
| `test_mode_screen_values` | For each of VANILLA, ADVANCED, LLM: Return on the item gives `g_game_mode` 0, 1, 2 and `g_advanced_features` 0, 1, 1. |
| `test_escape_back` | On the mode screen Escape gives `SCREEN_ADVENTURE`. Escape on the first screen stays on it. With `--adventure` given, Escape on the mode screen does nothing. |
| `test_csb_disabled` | `select_start(-1, -1, 0)`: Down does not move onto CSB. Return selects DM. A click on the CSB item changes nothing and `g_adventure` stays 0. |
| `test_click_activates` | `select_click` on the center of item 2 selects it. A click outside all items does nothing. (Item boxes come from the module, see below.) |
| `test_csb_screen_choices` | After CSB and a mode: `SCREEN_CSB`. Return on item 1: `CSB_NEW_ADVENTURE`. Item 2: `CSB_RESUME`. Item 3: `CSB_QUIT` and `g_quit_requested == 1`. |
| `test_csb_screen_after_game` | `select_start_csb()` gives `SCREEN_CSB`. |
| `test_done_ignores_keys` | Keys on `SCREEN_DONE` change nothing. |

`select_click` needs the item boxes. Add one read function to `select.h`: `int select_item_box(int item, Box *box)`. The tests use it to click the center of an item. It is a test hook that `select_draw` also uses, so it adds no dead code.

Drawing (font):

| Test | Check |
|---|---|
| `test_draw_title_pixels` | With the font loaded: `select_draw()` leaves the corner pixels at color 0. The rectangle of item 1 has at least one pixel with the color of the cursor item. |
| `test_draw_disabled_color` | `select_start(-1, -1, 0)`: the CSB item has only pixels of `COLOR_DARK_GRAY` and black. With `csb_present` 1 it has `COLOR_LIGHT_GRAY` pixels. |
| `test_draw_cursor_moves` | After Down, the gold pixels are in item 2 and not in item 1. |

Process level (SDL dummy drivers, as `test_port.py`):

| Test | Check |
|---|---|
| `test_invalid_adventure_exits` | `build/dm --adventure x --frames 1` exits with code 1 and the log line. |
| `test_csb_missing_exits` | `build/dm --adventure csb --data <empty tmp> --frames 1` exits with code 1. |
| `test_flags_skip_screens` | `build/dm --adventure dm --mode vanilla --frames 5` exits with code 0. A new debug check (an exit code or a log line `entrance`) shows that the DM entrance was reached. Use the log line `SDL_Log("adventure %s mode %s")` that `main` prints once at the start of the game. |
| `test_selection_screen_runs` | `build/dm --frames 5` (no flags, no `DM_AUTO_ENTER`) exits with code 0. This is the existing smoke test, now on the selection screen. |

The existing process tests stay unchanged and must stay green: `test_smoke_run`, the `--frames 100` run with `DM_AUTO_ENTER=1`, `--mode fast` (exit code 1), and the turn-based program test (`--mode turnbased` only).

## Open decisions

These points need the user's answer before the implementation.

1. **One flag given.** The approved text covers only "both given". Proposal: `--mode` alone implies DM and skips all screens (keeps `build/dm --mode turnbased` and the tests); `--adventure` alone shows only the mode screen. Alternative: any single flag shows the other screen and the tests add `--adventure dm`.
2. **Escape on the CSB screen.** Proposal: back to the mode screen. Alternative: nothing. After the credits of a CSB game the program shows the CSB screen with the same rule.
3. **Escape on the title screen.** Proposal: nothing. Ctrl+Q and the window close quit, as today. Alternative: quit.
4. **`--data DIR` with the selection.** Proposal: `--data DIR` is the disk A folder of the adventure that is chosen. The CSB presence check on the title screen then uses the default `game-csb/` when the DM choice is made, and `DIR` when `--adventure csb` is given. Alternative: `--data` applies to DM only, and a new `--csb-data` flag sets the CSB folder.
5. **Cursor at the list end.** Proposal: stops at the first and last enabled item (no wrap).
6. **Font source.** Proposal: DM `Graphics.DAT`, with `game-csb/Graphics.DAT` as fallback. The fallback needs the check that `GFX_FONT` exists in the CSB file.
7. **LLM mode with CSB.** "Straight into the dungeon" needs a rule for CSB: the champion selection has no dungeon mirrors. Proposal: an LLM run of CSB starts with `new_game` over the socket, and the champion selection is defined with the CSB work item.
8. **The CSB engine module.** The blocker needs a design decision (one address table per adventure, or rebuild from `APPB.FTL`). It is a separate plan.

## Order of work

Each step has at least one automated test and is green before the next one.

1. `adventure.c/h`, the save name change in `savegame.c`, `tests/test_select.py` (helper tests only). Existing tests stay green.
2. `select.c/h` (logic, no drawing) and the logic tests.
3. `select_draw`, the drawing tests, and a first look at the glyphs.
4. `main.c`: `--adventure`, `MODE_SELECT`, `apply_mode`, `start_adventure`, `show_entrance` branch. The process tests.
5. LLM tools and docs: `adventure` in the state JSON, `INSTRUCTIONS`, docstrings, `llm-mode.md`, README.
6. Separate plans (not part of this design): the CSB engine module, `MINI.DAT` and `CSBGAME.DAT`, the CSB champion selection, the CSB endgame.
