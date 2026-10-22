# Architecture

The port is a reimplementation of Dungeon Master v3.60 (Amiga) and Chaos Strikes Back (CSB) in C11. The game
rules follow the decompiled original; the port does not reproduce the Amiga technical layer. The source in `src/` has
five layers. The build makes the game `build/dm` and one shared library `build/libdm.dylib` for the tests.

## Layers

```
platform/   SDL window, sound device, socket, options, the application, main
   │
control/    the turn-based protocol for a program player (JSON over the socket)
   │
ui/         screens, the dungeon view, input mapping, audio output
   │
engine/     game rules: dungeon, party, champions, creatures, timeline, saves
   │
data/       file formats, the engine image (KAOS.FTL, APPB.FTL) and its tables, run configuration
```

A file includes only headers of its own layer and of the layers below it in the diagram ([Coding standard](coding-standard.md), section 1).
This page names a header with its layer: `engine/game.h` is `src/engine/include/game.h`.

- `data/` reaches the operating system only through the hooks of `data/os.h`.
- `engine/` does not call the user interface. It sets redraw flags and calls a hook.
- `ui/` changes game state only through engine functions, except the redraw flags and the hooks.
- `control/` is a client of `ui/` and `engine/` and holds no socket.
- Only `platform/` includes SDL and the operating system headers.

## Modules

A module is one `.c` file with one public header. Each header states its prefixes and has one purpose
paragraph. A module with several `.c` files has one `<module>_internal.h` for its private declarations:
`ui/view` (`view.c`, `view_walls.c`, `view_things.c`), `ui/music` (`music.c`, `music_tables.c`) and
`control/control` (`control.c`, `control_actions.c`). The tables do not show the internal headers.

### data/

| Header | Prefixes | Purpose |
|---|---|---|
| `data/adf.h` | `adf_` | Reads an Amiga floppy disk image (.adf, 880 KB, OFS file system), the form of the original game disks. |
| `data/adventure.h` | `adventure_` | The adventure of the run: Dungeon Master or Chaos Strikes Back. |
| `data/bytes.h` | `be16` `be32` `wr16` `wr32` | Big-endian access to the Amiga data: game files, save files, module images. |
| `data/crc32.h` | `crc32_` | The CRC-32 checksum of PNG and zip: checks the game files and writes the screenshot chunks. |
| `data/csb_image.h` | `csb_image_` | The CSB engine (APPB.FTL) as a KAOS-layout image: each table that the port reads is at its DM address. |
| `data/csb_text.h` | `csb_text_` | The CSB texts as Graphics.DAT text items: CSB keeps them in the APPB data, not in items 699 and 700. |
| `data/file.h` | `file_` | Whole-file reads and writes and file names: the C standard library only, no folder services. |
| `data/ftl.h` | `ftl_` | The FTL module loader of the Amiga engine files: lays a module out in memory at its base address. |
| `data/graphics.h` | `gfx_` `box_` | Graphics.DAT: its packed items, their decoding into chunky bitmaps, and its text items. |
| `data/ids.h` | `GFX_` `ZONE_` `LAYOUT_` `ICON_` `TEXT_` | Graphic numbers (Graphics.DAT items), layout zone numbers (graphic 696), layout bitmap ids, object icons, and string numbers (graphic 700), one block per kind, sorted by value. |
| `data/kaos.h` | `kaos_` | The loaded engine image (KAOS.FTL, or the KAOS-layout image of the CSB APPB.FTL) and its readers at the addresses of the original. |
| `data/kaos_addresses.h` | `KAOS_` | Addresses of the KAOS.FTL image (and of the KAOS-layout CSB image). |
| `data/os.h` | `os_` | The operating system services that data/ and engine/ need: folders, file times, the log, and fatal errors. |
| `data/settings.h` | `settings_` | The settings file of the advanced mode: SETTINGS.TXT in the saves folder with the sound volume, the music volume, the music choice and the upscale filter. |
| `data/spells.h` | `spell_` | The spell rules of the spell table: the symbol strings, the mana cost and the skill draws. |
| `data/swsh.h` | `swsh_` | The FTL logo module SWSH.FTL: a palette script, the logo graphic, and its sound. |
| `data/tables.h` | `tables_` | Static rule tables of KAOS.FTL, decoded once from the loaded image. |
| `data/vdeo.h` | `vdeo_` | The video driver module VDEO.FTL: the step table of the palette fade. |

### engine/

| Header | Prefixes | Purpose |
|---|---|---|
| `engine/action.h` | `action_` | Champion actions of the action area (KAOS range 8, MENU.C). |
| `engine/arith.h` | `min16` `max16` `abs16` `bounded_` `scale_` | The 16-bit arithmetic of the original rules: minimum, maximum, bounds, and the scaled product of the 68000. |
| `engine/champion.h` | `champion_` | Champion rules of KAOS (CHAMPION.C). |
| `engine/discard.h` | `discard_` | The record of a new thing: a free record, or the record of a thing that disappears from a far square when no record of its type is free. |
| `engine/dungeon.h` | `dungeon_` | The loaded dungeon: header, maps, squares and thing records, in the big-endian byte layout of the original memory. |
| `engine/event.h` | `event_` | Timeline event types and the handlers of the square events. |
| `engine/explored.h` | `explored_` | Explored squares: the squares the party has seen, for the automap. |
| `engine/game.h` | `game_` | The game session: the clock, the run flags, the modes and cheats, and the redraw flags that the rules set. |
| `engine/group.h` | `group_` `creature_` | Creature groups (KAOS range 4, GROUP.C, and the group helpers of range 7). |
| `engine/inventory.h` | `inventory_` `object_` | Objects, slots, and the leader hand. |
| `engine/magic.h` | `magic_` | Spell casting: the effect of a spell from the champion symbols (KAOS range 8, MENU.C). |
| `engine/map.h` | `door_` `square_` `map_` `direction_` | Current map, square bytes, and thing lists. |
| `engine/message.h` | `message_` | Rule-level model of the message area (zone 15): the text rows, their colors and expiry, and the cursor. |
| `engine/move.h` | `move_` | Party and object movement (KAOS range 8, MOVESENS.C and COMMAND.C). |
| `engine/party.h` | `party_` | The party as one body: its position, its scents, its light, and its movement delays. |
| `engine/projectile.h` | `projectile_` `explosion_` | Projectiles, explosions, fluxcages, and launchers (KAOS range 7, PROJEXPL.C). |
| `engine/random.h` | `random_` `random16` | The game random numbers: the seed and the random draws. |
| `engine/record.h` | `rec_` | The byte layout of the dungeon records: one accessor per field, no record byte outside this module. |
| `engine/recruit.h` | `recruit_` | Candidate champions: the champion data from the text of a mirror. |
| `engine/savefile.h` | `savefile_` | The save file container of DM v3.60 and CSB: the header and the blocks. |
| `engine/savegame.h` | `savegame_` | Reads and writes the save file of the adventure, `DMGAME.DAT` or `CSBGAME.DAT` (KAOS `save_game` and `load_game`, `LOADSAVE.C`). Also the extension blocks of the port (automap, cheats), the save slots of the advanced mode, and the slot info. |
| `engine/sensor.h` | `sensor_` | Floor and wall sensors. |
| `engine/sound.h` | `sound_` | The sound rules: the sound table, the volume by distance, the pending queue, and the play requests of the rules. |
| `engine/timeline.h` | `timeline_` | The timeline: the event slots and the min-heap that orders them by time. |

### ui/

The modules `advanced`, `automap`, `cheats`, `music`, `optmenu`, `rows`, and `slotlist` are the advanced features; they are not in the original game.

| Header | Prefixes | Purpose |
|---|---|---|
| `ui/advanced.h` | `advanced_` | Game modes and the advanced features: automap, quick save and quick load, WASD/QE keys, rune symbols on scrolls, colored action icons, item stat meters. `g_advanced_screen` is the open viewport screen: automap, options menu, or cheat screen. At most one is open, and the game time stops. |
| `ui/anim.h` | `anim_` | The animation player of ANIM.FTL for the chunk files (CSB ENDA.DAT). |
| `ui/audio.h` | `audio_` | The audio output: eight stereo pairs of voices (the Amiga has two), the CSB intro music, and the mixer. `platform/` calls the mixer on the game thread. The mixer scales the mix by the sound volume and applies the low-pass filters of the Amiga 500 output (the fixed RC filter and the LED filter). |
| `ui/automap.h` | `automap_` | Automap: draws the squares the party has seen (engine/explored owns them) as a top-down map. |
| `ui/cheats.h` | `cheats_` | The cheat screen of the advanced mode: it switches the cheats, offers quick save and quick load, and lists the spells of the loaded game with their runes; a spell line casts its spell. |
| `ui/command.h` | `command_` | Player input as the command table of the original: clicks and keys become queued commands; `command_process_queue` runs them. |
| `ui/dialog.h` | `dialog_` | The message box with one to four choices: it draws the dialog in the viewport and waits for a click on a choice. |
| `ui/draw.h` | `draw_` | Pixel operations on bitmaps: blit, fill, flip, shrink, and the masked fill. |
| `ui/entrance.h` | `entrance_` | The entrance of Dungeon Master: the doors, the ENTER and RESUME buttons, the door animation, the credits. |
| `ui/frame.h` | `frame_` | The frame wait of the blocking sequences: dialogs, the entrance, the endgame, the animations. |
| `ui/gameloop.h` | `gameloop_` | Game start and the main loop: it loads the game data, starts a new or a loaded game, runs a tick, and draws what changed. |
| `ui/layout.h` | `layout_` | The screen layout from the layout graphic of Graphics.DAT: it finds the box of a zone on the screen. |
| `ui/logo.h` | `logo_` | The FTL logo of SWSH.FTL (not KAOS): the original shows it before the DM title. |
| `ui/menus.h` | `menus_` | The action area, the spell area and the movement arrows at the right of the screen. |
| `ui/music.h` | `music_` | Music of the advanced mode: the soundtracks of the FM-Towns and Super NES versions from MP3 files, scaled by the music volume. |
| `ui/optmenu.h` | `optmenu_` | The options menu of the advanced mode: sound volume, music volume, music choice, and upscale filter in the viewport. `platform/` sets the filter hook. A change writes `SETTINGS.TXT`. |
| `ui/palette.h` | `palette_` | The palette fade of the original: it changes both palette halves of `g_screen` step by step, one frame for each step through `g_frame_hook`. |
| `ui/panel.h` | `panel_` | The inventory panel: slots, the eye and the mouth, food and water, skills and statistics, the object description, the scroll, the chest. |
| `ui/rows.h` | `rows_` | The rows of text of the viewport screens: it draws a title and rows in the game font and maps a click to a row and a text column. |
| `ui/select.h` | `select_` | The start screens in the DM font: adventure, mode, and the CSB entrance. |
| `ui/slotlist.h` | `slotlist_` | The slot list of the advanced mode: the save slots, and in the load list also the quick save, as rows in the dialog graphic with CANCEL. It takes every click and key while it is open. |
| `ui/startend.h` | `startend_` | Sequences outside the game loop: the save dialog, resuming a saved game, the endgame screens, and the fuse sequence with Lord Chaos. |
| `ui/status.h` | `status_` | The champion status boxes at the top of the screen: name, bars, hands, shields, damage, the party icons. |
| `ui/text.h` | `text_` | The DM font: it draws text and measures it. |
| `ui/ui.h` | `ui_` | Interface drawing on `g_screen`: zones, blits, palette, object icons, hit tests, message area, action and spell areas. |
| `ui/video.h` | `video_` | The screen pixels and palettes (g_screen) and the Amiga color conversion. |
| `ui/view.h` | `view_` | Draws the dungeon view and the CSB magic map from game state and changes no game state. |

### control/

| Header | Prefixes | Purpose |
|---|---|---|
| `control/control.h` | `control_` | The turn-based protocol of a program player: one JSON request line in, one JSON reply out. |
| `control/json.h` | `json_` | A JSON text writer for the protocol replies. |
| `control/png.h` | `png_` | The screenshot of the program player: the game screen as an RGB PNG with stored deflate blocks. |
| `control/request.h` | `request_` | The reader of one request line. |
| `control/state_json.h` | `state_` | The game as JSON for the program player: the party, the champions, what the party sees, and the explored map. |
| `control/turn.h` | `turn_` | The turns of the program player. An action or a wait runs game ticks until the party can move again. A batch runs its actions as turns until a stop condition. The reply tells what the party saw. |

### platform/

| Header | Prefixes | Purpose |
|---|---|---|
| `platform/app.h` | `app_` | The application: the start screens, the choice of the adventure and the mode, the frame loop, and the control socket. |
| `platform/debug_view.h` | `debug_view_` | The developer modes: `--show` draws one graphic, `--view` draws the dungeon view with free movement. |
| `platform/options.h` | `options_` | The command line of the game: one table of options, --help, and an error for an unknown option or a bad value. |
| `platform/platform.h` | `plat_` | The SDL layer of the game: the window, the picture, the input events, the 50 Hz tick, the audio output, and the mouse pointer. |
| `platform/scale.h` | `scale2x` `scale3x` `output_` | The pixel-art upscale filters and the placement of the game screen in the window. |
| `platform/transport.h` | `transport_` | The control socket: a Unix domain socket with one request line in and one reply line out (POSIX). |

`platform/main.c` has no header, because it is the entry point. `platform/transport_none.c` is the build of
`transport.h` without a socket (Windows).

## The frame loop

`main` (`platform/main.c`) parses the options, calls `app_open`, and then calls `app_run_frame`
(`platform/app.c`) once per frame, 50 times per second. One frame does these steps in this order:

1. The input: `plat_poll` reads the SDL events. On the start screens they go to `select_click` and
   `select_key`; in the game they become commands (`command_process_key`, `command_process_click`). In
   the LLM mode the game ignores the keyboard and the mouse from the entrance on; the start screens still read them.
2. `transport_poll` reads the control socket (see [The control socket and the turn](#the-control-socket-and-the-turn)).
3. The screen of the run: a start screen (`select_screen`, `select_draw`), the entrance, or
   `gameloop_run_frame` (`ui/gameloop.c`). `gameloop_run_frame` runs the queued commands and tells if the
   game time must advance. In the LLM mode it runs only while `control_is_turn_pending()` is true.
4. `plat_present` shows `g_screen` in the window.
5. `gameloop_tick` advances the game by one tick, when step 3 asked for it.
6. `plat_wait_tick` pumps the audio (`audio_mix`, `music_mix`) and waits for the next 50 Hz tick.

A dialog, the endgame and the fuse sequence wait for the player inside a rule. They run their own frames
through the frame hook `g_frame_hook` (`ui/frame.h`). `platform/app.c` sets the hook. The hook shows one frame and
reads the input into the command queue. The hook runs no game logic, so a wait inside a game tick is safe. In
the LLM mode the hook is NULL: a wait ends immediately.

## The tick

`gameloop_tick` (`ui/gameloop.c`) advances the game by one tick:

- A pending map change of the party (`move_process_new_party_map`).
- The expired events of the timeline (`event_process_expired`, `engine/event.c`). The timeline
  (`engine/timeline.c`, prefix `timeline_`) holds the events; the event handlers (prefix `event_`) run them.
  The events move the creatures (`group_`), the projectiles and explosions (`projectile_`), the doors, and the
  square and sensor effects (`sensor_`).
- The music and the pending sounds (`music_update`, `sound_play_pending`).
- The champions: the pending damage (`champion_apply_pending_damage`) and the dead champions. A dead party
  starts the endgame.
- The game time (`game_advance_time`) and the time effects on the champions (`game_apply_tick_effects`).
- The expired message rows (`message_clear_expired_rows`).
- The fuse sequence, when a rule requested it (`projectile_take_fuse_request`).

The rules do not draw. They set redraw flags that `ui/` reads and draws:

- `g_redraw` (`engine/game.h`): the `REDRAW_*` bits for the action area and the spell area.
- `Champion.attributes` (`engine/champion.h`): the `CHAMPION_ATTR_*` bits for the name and title, the
  statistics, the load, the icon, the panel and the status box of one champion.

## The control socket and the turn

In the LLM mode a program player plays the game over a Unix domain socket. The protocol is in
[LLM mode: turn-based play over MCP](llm/llm-mode.md).

1. In each frame `transport_poll` (`platform/transport.c`) first calls `control_end_turn` when a turn is
   pending. It advances the turn by one frame. When the turn ends, it returns true and the reply goes out on
   the socket.
2. `transport_poll` then reads one request line from the socket.
3. `control_handle_line` (`control/control.c`) answers the request immediately, or it starts a turn.
4. While `control_is_turn_pending()` is true, the frame loop runs game frames and ticks.

Between two turns the game time stands still, because `app_run_frame` does not call `gameloop_run_frame`.

### The C API of the control layer

`control/control.h` (no socket, no SDL, in `libdm`):

| Function | Purpose |
|---|---|
| `bool control_handle_line(const char *line, Json *reply)` | Answers one request line. Returns true when `reply` holds the answer, false when a turn started. |
| `bool control_end_turn(Json *reply)` | Advances the pending turn by one frame. Returns true when the request ended and `reply` holds the answer. |
| `bool control_is_turn_pending(void)` | True while a turn runs. |
| `void control_drop_client(void)` | The client is gone: a pending turn ends without a reply. |

`platform/transport.h`:

| Function | Purpose |
|---|---|
| `bool transport_is_available(void)` | True where the control socket exists (macOS). |
| `bool transport_open(const char *path)` | Listens on the socket. Replaces an old socket file. Returns false on an error. |
| `void transport_poll(void)` | Call once per frame. Advances a pending turn, accepts a client, answers requests. |
| `void transport_close(void)` | Closes the sockets and removes the socket file. |

The commands `save`, `load`, and `new_game` use `startend_save_dir()` and `gameloop_get_data_dir()`. `gameloop_load_data` sets both directories.

`control/state_json.h` (in `libdm`, no SDL): `state_write_state`, `state_write_view`, `state_write_map` append one JSON object to a `Json` buffer. `state_get_string(part)` returns a new string. Release it with `free`.

## Global state

The globals of the game:

- The game state that the save file holds: `g_party` (`engine/party.h`), `g_champions`
  (`engine/champion.h`), the dungeon (`g_dungeon_*`, `g_thing_data`, `engine/dungeon.h`), the timeline
  `g_timeline` (`engine/timeline.h`), and the game time `g_game_time` (`engine/game.h`).
- `g_session` (`engine/game.h`): the state of one run. A new game or a load resets it; the save file does not
  hold it.
- `g_screen` (`ui/video.h`): the frame buffer and the two palettes.
- The redraw flags: `g_redraw` and the `CHAMPION_ATTR_*` bits of `Champion.attributes`.

The state of one module is `static` in its `.c` file ([Coding standard](coding-standard.md), rules 6.6 and 7.1).

## Hooks between layers

A hook is a function pointer that a higher layer sets, so that a lower layer can call it without an include.

| Hook | Header | Set by | Called by | A NULL hook means |
|---|---|---|---|---|
| `g_endgame_sensor_hook` | `engine/event.h` | `ui/gameloop.c` (`startend_run_endgame_sensor`) | `engine/event.c`, an endgame sensor | no endgame runs |
| `g_sound_play_hook` | `engine/sound.h` | `ui/audio.c` (`audio_play_data`) | `engine/sound.c` | no sound plays |
| `g_frame_hook` | `ui/frame.h` | `platform/app.c`, the tests | `frame_wait` (`ui/frame.c`) | a wait ends immediately (tests, LLM mode) |
| `g_os_log_hook` | `data/os.h` | `platform/app.c` | `os_log` (`data/os.c`) | the message goes to stderr |
| `g_os_fatal_hook` | `data/os.h` | `platform/app.c` | `os_fatal` (`data/os.c`) | the message goes to stderr |
| `g_os_make_folder_hook` | `data/os.h` | `platform/app.c`, the tests | `os_make_folder` (`data/os.c`) | the folder is not made; the call returns false |
| `g_os_list_folder_hook` | `data/os.h` | `platform/app.c`, the tests | `os_list_folder` (`data/os.c`) | no file is listed |
| `g_os_file_time_hook` | `data/os.h` | `platform/app.c` | `os_get_file_time` (`data/os.c`) | the slot list shows no file time |
