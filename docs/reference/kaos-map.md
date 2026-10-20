# KAOS module map

This page uses the original KAOS names; the port names differ.

This page describes the structure of `KAOS.FTL`, the main game module of Dungeon Master v3.60 for the Amiga. The names come from [kaos-symbols.csv](kaos-symbols.csv). Many names follow the structure of ReDMCSB, because the function order in KAOS matches the order of the DM source files.

All addresses are for the image that `uv run dm-ftl game/KAOS.FTL <out.bin>` creates. The [FTL module format](../formats/ftl.md) page describes the loader.

## Memory layout

| Address | Content |
|---|---|
| `0x100000`–`0x10415f` | Initialized data (globals, tables, strings). |
| `0x104160` | Data base. Register `A5` holds this address in all code. |
| `0x104180`–`0x104e67` | Jump table: 413 entries of 8 bytes. |
| `0x104e68`–`0x127f00` | Code: 749 functions. |

Code reads globals with `d16(A5)`. A negative offset reaches the initialized data, for example `-0x3f72(A5)` is `0x1001ee`.

### Jump table

Entry `i` is at `0x104180 + 8 * i`. Bytes 2–7 hold `JMP abs.l` to export `i`. Code calls an export with `JSR d16(A5)`. The target of the call is the `JMP` instruction at `entry + 2`.

Ghidra shows this call as an indirect call through a global, for example `(*DAT_0010465a)()`. The slot `S` points to export `(S - 0x104182) / 8`. Examples:

| Ghidra global | Export | Target | Name |
|---|---|---|---|
| `DAT_0010465a` | 155 | `0x10a85c` | `update_map_coordinates_after_relative_movement` |
| `DAT_0010463a` | 151 | `0x10e784` | `get_string` |
| `DAT_00104a82` | 288 | `0x116296` | `get_zone` |
| `DAT_0010490a` | 241 | `0x11427a` | `copy_bytes` |

`kaos-symbols.csv` labels every slot as `jt_<target name>` (or `jt_fun_<address>` when the target has no name). The decompiled C shows `(*jt_timeline_process)()`.

Seven slots are also function pointers: interrupt handlers and callbacks (`0x1041d2`, `0x104282`, `0x104552`, `0x10456a`, `0x10457a`, `0x104582`, `0x104a32`). Ghidra turns them into thunk functions, so `reference/decomp/dm/KAOS.c` has 756 function headers.

Ghidra drops most arguments of a jump-table call. For example, `game_loop` pushes direction, X, and Y before `jt_draw_dungeon_view`, but the C output shows one argument. Read the disassembly for jump-table calls.

## Entry and startup

### How APPA starts KAOS

`APPA.FTL` runs modules in a chain through service `-0x68` of the `BJELoad_R` service table. The second argument is a small block. The long at offset 4 holds a 4-character tag. `ENGL`, `FNCH`, and `GRMN` start `KAOS` with the language value 0, 1, or 2 in that long. On return, the long holds the tag of the next module.

### KAOS entry

1. Export 0 (`module_entry_stub`, `0x104e6c`) jumps to export 77 (`kaos_main`, `0x1078de`). The bytes after the jump are an unused copy of a Macintosh-style data loader (A-line traps). The code never runs.
2. `kaos_main(services, args)` calls `rt_setjmp`. A later `rt_longjmp` returns here and ends the module through `module_cleanup`.
3. `init_module_args` stores `args` in `g_module_args` and `args[6] & 3` in `g_language`. It stores the service table (`set_service_table`). It writes `APPB` as the next module tag.
4. `initialize_game` (`0x127e34`) runs:
   - `set_language_file_names` replaces `~` in the file names with the language letter.
   - `system_init` opens libraries and devices, allocates memory, and installs interrupts (see [OS usage](#os-usage)).
   - `detect_floppy_drives`, `install_input_handler`.
   - `graphics_dat_read_header`, `init_layout`, `load_string_table`, special graphics, `text_init`, `sound_init`, dungeon view tables, `menus_init`, `load_object_names`, `command_init_mouse_zones`, `init_mouse_pointer`.
   - `process_entrance` shows the entrance. Then `load_game` runs until it succeeds.
   - `start_game`, then `move_get_move_result(party)` for a new game.
5. `kaos_main` runs `game_loop` and `endgame` in an endless loop.

## Main loop

`game_loop` (`0x10773c`) has the same shape as `F0002_MAIN_GameLoop` in ReDMCSB:

1. If `g_new_party_map_index != -1`: `process_new_party_map`, `move_get_move_result(party)`, `command_discard_all_input`.
2. `timeline_process`. Repeat step 1 if the party changed maps.
3. If the party is not sleeping: `game_unfreeze` (no candidate champion), then `draw_dungeon_view(direction, x, y)` when no inventory is open, then update the mouse pointer.
4. `highlight_box_disable`, `sound_play_pending`, `champion_apply_pending_damage`.
5. Leave the loop when `g_party_dead` is set.
6. Increment `g_game_time`. Every 512 ticks: `decrease_torches_light_power`. Decrement the freeze-life ticks.
7. `menus_refresh_action_area`. Every 64 ticks (16 when sleeping): `champion_apply_time_effects`. Optional `champion_draw_all_states`. Decrement the movement-disable counters.
8. `message_area_clear_expired_rows`.
9. Input loop: read each key from the key queue and call `command_process_key`. Call `command_process_queue`. Repeat until `g_stop_waiting_for_input` and `g_game_time_ticking` are set. The vertical-blank handler sets `g_stop_waiting_for_input` after `g_wait_for_input_max_vblank_count` (12) frames.

`process_new_party_map` calls `group_remove_all_active_groups`, `set_current_map_and_party_map`, `load_current_map_graphics`, `group_add_all_active_groups`, and `set_dungeon_view_palette`.

## OS usage

KAOS uses the Amiga OS through small stubs at `0x123ba6`–`0x1244d2`. The stubs are in alphabetical order, as in `amiga.lib`. Each stub loads a library base from a global and calls a negative offset (LVO).

| Global | Library base |
|---|---|
| `0x104020` `g_sys_base` | `exec.library` (copied from address 4 by `os_init_sys_base`) |
| `0x10400c` `g_dos_base` | `dos.library` |
| `0x104010` `g_gfx_base` | `graphics.library` |
| `0x104014` `g_intuition_base` | `intuition.library` |
| `0x104018` `g_layers_base` | `layers.library` (opened, not called) |
| `0x10401c` `g_console_device` | `console.device` base (`CDInputHandler`, `RawKeyConvert`) |

Calls in use:

- exec: `AllocMem`, `FreeMem`, `AvailMem`, `Forbid`, `Permit`, `Disable`, `Enable`, `AddIntServer`, `RemIntServer`, `OpenDevice`, `CloseDevice`, `DoIO`, `CheckIO`, `WaitIO`, `BeginIO` (through the device), `AllocSignal`, `FreeSignal`, `AddPort`, `RemPort`, `FindPort`, `FindTask`, `PutMsg`, `GetMsg`, `WaitPort`, `SetFunction`, `Alert`, `SuperState`, `OpenLibrary`, `CloseLibrary`. Also `CreatePort`, `DeletePort`, `CreateExtIO`, `DeleteExtIO`, `CreateStdIO`, `DeleteStdIO`, and `NewList` from `amiga.lib`.
- dos: `Open`, `Close`, `Read`, `Write`, `Seek`, `DeleteFile`, `Rename`, `Lock`, `UnLock`, `Examine`, `DateStamp`, `Delay`, `DeviceProc`.
- graphics: `InitBitMap`, `InitRastPort`, `SetAPen`, `SetDrMd`, `RectFill`, `SetRGB4`, `LoadView`, `WaitTOF`, `WaitBlit`, `OwnBlitter`, `DisownBlitter`, `GetSprite`, `FreeSprite`, `ChangeSprite`, `MoveSprite`.
- intuition: `DisplayAlert`, `MakeScreen`, `RethinkDisplay`. `patch_system_requesters` uses `SetFunction` on `AutoRequest` and `BuildSysRequest`, so the OS shows no disk requesters.

`system_init` (`0x105728`) opens the libraries, the `NIL:` output, `input.device`, and `console.device`. It allocates a `0x2d4`-byte chip block (`g_chip_block`), installs the copper interrupt, and opens `audio.device` and `trackdisk.device`. `connect_video_driver` finds the message port `FTL_DRIA_Video` that the `VDEO` module creates. `g_video` (`0x1024d6`) points to that structure. `system_shutdown` reverses these steps.

Fatal errors call `fatal_system_error(code)`. It shows the code with `DisplayAlert` and then calls `Alert`. The codes have the form `0x80ff00nn`.

## Interrupts and input

| Handler | Interrupt | Work |
|---|---|---|
| `copper_interrupt_handler` (`0x105092`) | `INTB_COPER` (4) | Calls `update_mouse_pointer_sprites`. |
| `vblank_interrupt_handler` (`0x106374`) | `INTB_VERTB` (5) | Counts frames, writes pending palettes into the copper list or `COLOR00`–`COLOR15`, scrolls the message area with `copy_bytes`. |
| `input_event_handler` (`0x105908`) | input.device handler chain | Mouse moves and buttons, keys through `RawKeyConvert`, disk change events. |

The mouse position is kept at 640×400 (`g_mouse_x_hires`, `g_mouse_y_hires`) and halved into `g_mouse_x`, `g_mouse_y`. Button presses call `command_process_click`. Keys go into a 64-entry queue at `g_chip_block + 0x108` (`key_queue_put`, `key_queue_get`).

## Hardware usage

KAOS touches the custom chips in few places. Most graphics go through the OS or through direct memory writes to bitplanes.

- Blitter: `blit_raw` (`0x116c06`) writes `BLTCON0` to `BLTSIZE` (`0xdff040`–`0xdff074`). `blit_bitmap` (`0x11739c`) is the high-level copy with clipping and a transparent color. It has 19 callers.
- Beam position: `wait_for_raster_line` and `blit_bitmap` read `VPOSR` (`0xdff004`).
- Palette: `vblank_interrupt_handler` writes `COLOR00`–`COLOR15` (`0xdff180`–`0xdff1bc`). `palette_fade` steps a palette in 15 frames.
- Sprites: the mouse pointer uses hardware sprites 4–7 through `GetSprite`, `ChangeSprite`, and `MoveSprite`.
- CIA: no direct access.
- Audio: through `audio.device` only.

Bitmaps have a 4-byte header before the pixel data: width at offset −4, height at offset −2. The pixel data has 4 bitplanes. The size is `((width + 15) & ~15) / 2 * height` bytes (`bitmap_byte_count`). The screen is 320×200 (`g_screen_width`, `g_screen_height`). The viewport is 224×136 (`g_viewport_width`, `g_viewport_height`).

## Memory management

`system_init` measures free chip and fast memory with `AvailMem`, allocates large blocks, and passes them to `heap_add_region`. `heap_add_region(0, 0)` closes the list.

- `mem_alloc(size, kind, pool)` allocates from the heap. The kind word selects the pool and the direction (top or bottom).
- `mem_free_top`, `mem_free_temp` release the last allocations.
- `alloc_bitmap(width, height, pool)` and `free_bitmap` manage bitmaps with the 4-byte header.
- `copy_bytes(src, dst, size)` is the general copy routine. `clear_memory(dst, size)` fills with zero.

The graphics cache (`cache_*`, `derived_cache_*`) keeps decoded graphics. `get_native_bitmap_or_graphic` returns a cached graphic and loads it when necessary. Derived bitmaps (scaled or recolored copies) use `is_derived_bitmap_in_cache`, `get_derived_bitmap`, and `add_derived_bitmap_to_cache`.

## File I/O

File wrappers (`0x1061b8`–`0x1062ca`) call dos: `file_open_read` (`MODE_OLDFILE`), `file_open_write` (`MODE_NEWFILE`), `file_close`, `file_seek`, `file_read`, `file_write`, `file_size`, `file_delete`, `file_rename`. `file_read` returns true when it read all bytes.

File names in the data section:

| Address | Name | Use |
|---|---|---|
| `0x101d70` | `DungeonMaster:Dungeon~.dat` | Dungeon. `~` becomes empty, `F`, or `G`. |
| `0x101d90` | `DungeonMaster:Dungeon~.FTL` | New-game dungeon. `load_game` opens it when the save disk has no `DMGAME~.DAT` and no `DMGAME~.BAK`. |
| `0x101db0` | `DungeonMaster:DungeonB~.dat` | Second dungeon. `load_game` opens it when `g_use_dungeon_b` is set (entrance command `0xc9`). |
| `0x101dd4` | `DF0:DMGAME~.DAT` | Saved game. `~` becomes the save drive number. |
| `0x101de8` | `DF0:DMGAME~.BAK` | Backup of the saved game. |
| `0x101dfc` | `DungeonMaster:graphics.dat` | Graphics. |

### Graphics.DAT

`graphics_dat_open` and `graphics_dat_close` use a reference count. `graphics_dat_read_header` reads:

1. A word: the graphic count. When bit 15 is set, the low bits give the format (1) and a second word gives the count.
2. The compressed size of each graphic (one word each).
3. The decompressed size of each graphic (one word each).
4. Format 1: width and height of each graphic (two words each). Format 0: the code reads the first 4 bytes of each graphic.

`graphics_dat_read_graphic` reads a graphic through a 1 KB buffer. `expand_graphic` (`0x113cc8`) decodes a compressed graphic into a bitmap.

### Dungeon.DAT

`load_dungeon` (`0x1260ee`) reads the dungeon. It follows `F0434_STARTEND_IsLoadDungeonSuccessful` in ReDMCSB:

1. Optional 8-byte header with signature `0x8104`: the file is compressed. `decompress_dungeon` expands it.
2. File header, 44 bytes (`g_dungeon_header`). Offset 2: raw map data size. Offset 4: map count. Offset 6: text data word count. Offset 8: start location of the party (X bits 0–4, Y bits 5–9, direction bits 10–11). Offset 10: square first thing count. Offset 12: thing count of each of the 16 thing types.
3. Map descriptors, 16 bytes each (`g_dungeon_maps`).
4. Column first-thing indices, square first things, text data.
5. Thing data for each type. The record sizes are in `g_thing_data_byte_count`: 4, 6, 4, 8, 16, 4, 4, 4, 4, 8, 4, 0, 0, 0, 8, 4.
6. Raw map data, then a checksum word.

For a new game, the loader adds 300 free square-first-thing entries and free thing records.

### Saved games

`save_game` (`0x125a44`) and `load_game` (`0x1265d4`) write and read blocks with `save_write_checked_block` and `save_read_checked_block`. Each block is obscured with `savefile_xor_block` (key added to each word) and protected with `save_block_checksum`. The blocks hold the party, the champions, the active groups, the timeline, and the dungeon. The trackdisk functions (`trackdisk_*`) detect drives and can format a save disk.

## Runtime library

| Address | Name | Purpose |
|---|---|---|
| `0x104efa` | `rt_switch_word` | `switch` on a table of word keys. |
| `0x104f0e` | `rt_switch_long` | `switch` on a table of long keys. |
| `0x104f22` | `rt_switch_range` | `switch` through a jump table with a range check. |
| `0x104f3e` | `rt_lmul` | 32×32 multiply. |
| `0x104f74` | `rt_uldiv` | Unsigned 32-bit divide (quotient). |
| `0x104f94` | `rt_ulmod` | Unsigned 32-bit remainder. |
| `0x104fb6` | `rt_ldiv` | Signed 32-bit divide (quotient). |
| `0x104fd6` | `rt_lmod` | Signed 32-bit remainder. |
| `0x104ff8` | `rt_ldivmod_core` | Signed divide core (D0/D1 in registers). |
| `0x105026` | `rt_uldivmod_core` | Unsigned divide core. |
| `0x106dee` | `rt_setjmp` | Saves D3–D7, A2–A4, A6, SP, and the return address. |
| `0x106dfc` | `rt_longjmp` | Jumps to the saved return address. |

The string functions (`str_length`, `str_copy`, `str_concat`, `str_compare`, `str_copy_n`, `str_find_char`) work like the C library functions. `int_to_decimal_string` converts a long to text.

## Random number generator

`g_random_seed` (`0x100ef8`) starts with the long at offset `0x4c` of `IntuitionBase` (a time field).

```c
seed = seed * 0xBB40E62D + 11;
random16    = (seed >> 8) & 0xFFFF;
random_bit  = (seed >> 8) & 1;
random_2bits = (seed >> 8) & 3;
```

## Main data structures

| Global | Type | Notes |
|---|---|---|
| `g_champions` (`0x101f54`) | 4 × `0x140` bytes | Champion records. Offset `0xd4`: 30 slot things (words). Offsets `0x34`–`0x3e`: health, stamina, mana (current and maximum). |
| `g_party_map_x`, `g_party_map_y`, `g_party_direction`, `g_party_map_index` | words at `0x1001ec`, `0x1001ee`, `0x1001ea`, `0x1001f0` | Party position. |
| `g_game_time` | long at `0x1001f8` | Game tick counter. |
| `g_dungeon_header` | 44 bytes | See [Dungeon.DAT](#dungeondat). |
| `g_dungeon_maps` | 16 bytes per map | Offset 0: raw data offset. Offset 8: width − 1 (bits 6–10), height − 1 (bits 11–15). Offsets 10–14: ornament, creature type, and door type counts, see [range-4-6.md](symbols/range-4-6.md#map-descriptor). |
| `g_current_map_data` | pointer per column | Square bytes of the current map. Bits 5–7: square type (0 wall, 1 corridor, 2 pit, 3 stairs, 4 door, 5 teleporter, 6 fake wall). Bit 4: the square has things. |
| `g_thing_data[16]` | pointers | Thing records per type. A thing is a word: bits 10–13 type, bits 0–9 index, bits 14–15 cell. `0xfffe` is the end of a list. |
| `g_active_groups` | 16 bytes per entry | Active creature groups. |
| `g_timeline.events` | 10 bytes per event | Offset 0: map index (bits 24–31) and time (bits 0–23). Offset 4: event type. |
| `g_creature_info` | `0x1a` bytes per creature type | Static creature attributes. |
| `g_object_info` | 6 bytes per object | Static object information. |
| `g_chip_block` | `0x2d4` bytes | Offset `0x108`: key queue. `0x188`–`0x208`: palettes. `0x248`: rastport. `0x2ac`: bitmap structure. |

## Function partition

The linker keeps the source file order. The export numbers form groups that follow file boundaries. The table splits the 749 functions into nine ranges.

| # | Start | End | Count | Subsystems | Confidence | Key evidence |
|---|---|---|---|---|---|---|
| 1 | `0x104e6c` | `0x106372` | 79 | Runtime, system init and shutdown, input device, key queue, disk detection, file I/O | High | Exports 0–31 and 122–125. Library opens, `input.device` handler, dos stubs. |
| 2 | `0x106374` | `0x1076f8` | 61 | Interrupts, palette, fills, strings, `setjmp`, blit wrappers, random, min/max | High | Exports 32–75. Writes to `COLOR` registers, LCG constant `0xBB40E62D`. |
| 3 | `0x10770e` | `0x10a3a6` | 80 | Main loop, object icons, mouse pointer, sound, text and message area, trackdisk | High | Exports 76–121. `game_loop`, `audio.device`, `trackdisk.device`. |
| 4 | `0x10a432` | `0x10e784` | 77 | Dungeon access, creature groups (AI events), string table | High | Exports 126–182. ReDMCSB DUNGEON.C and GROUP.C order. |
| 5 | `0x10e7b2` | `0x111f82` | 68 | Fill helpers, inventory panels, champion part 1 | High | Exports 183–224, 237–240. Torch light, chests, scroll text, skills, damage. |
| 6 | `0x11202c` | `0x116a42` | 101 | Champion part 2 (drawing, candidates), video driver, graphic decoding, memory heap, Graphics.DAT cache, layout zones | Medium | Exports 225–269, 278–293. `FTL_DRIA_Video`, `graphics.dat` header. The champion/video boundary is at `0x113c16`. |
| 7 | `0x116a4e` | `0x11d7f0` | 67 | Blitter, dungeon view, projectiles, explosions, group movement helpers | Medium | Exports 270–277, 307–320. Blitter registers, 16 square drawing functions. |
| 8 | `0x11d842` | `0x123ae2` | 101 | Movement, sensors, timeline, commands, action and spell menus | Medium | Exports 294–306, 321–336. `timeline_process`, `command_process_queue`. |
| 9 | `0x123ba6` | `0x127e34` | 115 | OS stubs (70), dialogs, endgame, save and load, entrance, start | High | Exports 337–412. Alphabetical LVO stubs, `DMGAME~.DAT`, checksums. |

## Open questions

