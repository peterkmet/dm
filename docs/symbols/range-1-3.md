# KAOS ranges 1–3: notes for the C port

This page covers the functions of `KAOS.FTL` from `0x104e6c` to `0x10a3a6` (ranges 1, 2, and 3 of the [function partition](../kaos-map.md#function-partition)). The symbols are in [range-1-3.csv](range-1-3.csv). The page describes what each group does, the data structures, and the facts that a C porter must know.

All addresses are for the image that `uv run dm-ftl game/KAOS.FTL <out.bin>` creates. `A5` holds `0x104160`.

## General facts

- The compiler uses 16-bit `int`. Word parameters are pushed as 2 bytes. Use `int16_t` and `uint16_t` for them.
- Most functions use `LINK A6` and caller cleanup. The runtime helpers `rt_lmul`, `rt_uldiv`, `rt_ulmod`, `rt_ldiv`, and `rt_lmod` remove their own 8 bytes of arguments and keep `D1`.
- Booleans are words with the value 0 or 1. Some functions extend the result to a long (`ext.l`), for example `file_read`.
- The error alert text says "Chaos Strikes Back". KAOS shares its code base with CSB.

## Runtime library (`0x104e6c`–`0x105090`)

- `module_entry_stub` is export 0. It jumps to `kaos_main` through the jump table.
- `rt_switch_word`, `rt_switch_long`, and `rt_switch_range` implement `switch`. The caller puts the key in `D0` and calls the helper with `JSR`. The case table follows the `JSR` instruction. The helper jumps to the case code. A zero offset makes an endless loop.
- `rt_ldiv` and `rt_lmod` truncate toward zero, as in C99. The remainder has the sign of the dividend. Division by zero causes a `DIVU` trap.
- `rt_setjmp(env)` saves `D3`–`D7`, the return address, `A2`–`A4`, `A6`, and `SP` in 11 longs. It returns 0. `rt_longjmp(env, value)` returns `value` from `rt_setjmp`. A value of 0 becomes 1. `kaos_main` uses `g_exit_jmp_buf` (`0x104032`).
- In C, use the standard operators and `setjmp`/`longjmp`.

## System init and shutdown (`0x105102`–`0x1058e6`)

`system_init` writes `APPB` as the next module tag. Then it calls these functions in order:

1. `open_system_resources`: `os_init_sys_base`, `store_interrupt_data_a5`, `alloc_chip_block`, intuition, `FindTask(0)`, dos, graphics, layers, `patch_system_requesters`, `input.device`, `console.device`, `NIL:`.
2. `install_vblank_interrupt`, `install_copper_interrupt`.
3. `trackdisk_init`, `audio_device_open`, `connect_video_driver`.
4. The random seed gets the long at offset `0x4c` of `IntuitionBase`.
5. Memory pools: chip memory from `get_usable_chip_memory`, fast memory (maximum `0x80000` bytes), a second chip pool. Each pool goes to `heap_add_region`. The third argument is `0x7f8` for chip pools and `0x700` for the fast pool.

`get_usable_chip_memory` keeps a reserve of `0x2000` bytes of largest chip block and `0x4000` bytes of total chip memory.

`system_shutdown` fades the display when `g_fade_display_on_exit` is set. Then it frees the mouse pointer, removes the input handler, and calls `system_release`.

`patch_system_requesters` replaces `AutoRequest` (LVO `-0x15c`) and `BuildSysRequest` (LVO `-0x168`) with `system_requester_stub`. The stub returns 0, so the OS shows no "insert disk" requesters.

Errors:

- `fatal_system_error(code)` shows `"Chaos Strikes Back Internal Error: XXXXXXXX"` with `DisplayAlert` and calls `Alert`.
- `fatal_error(code)` shows `"... System Error NN"` (two decimal digits), waits for a key, and quits.
- The alert texts are in the code section (`0x1054a8`, `0x105500`). `show_alert_*` writes the digits into them.

The C port replaces this group with SDL init and `exit`.

## Interrupts (`0x105092`, `0x106374`)

Both handlers run as interrupt servers. `A1` points to `g_interrupt_data`, which holds the `A5` value. The handlers load `A5` from it. They return 0 in `D0`.

`copper_interrupt_handler` (`INTB_COPER`), when `g_display_active` is set:

- If `g_copper_palette_request` is set, it clears it and sets `g_top_palette_pending`. The dungeon view uses this to change the palette in the same frame.
- Otherwise, if `g_viewport_blit_in_progress` is 0, it calls `update_mouse_pointer_sprites`.

`vblank_interrupt_handler` (`INTB_VERTB`):

1. Increments `g_vblank_counter`. When it exceeds `g_wait_for_input_max_vblank_count`, it sets `g_stop_waiting_for_input`. This sets the game speed: one game tick takes at least 12 frames when no input arrives (about 4 ticks per second on PAL).
2. When `g_display_active` is set:
   - `g_sprite_palette_pending`: writes 16 words from `g_sprite_palette` to `COLOR16`–`COLOR31`.
   - `g_top_palette_pending`: writes 16 colors from `g_top_palette` into the first copper list (the value word of each `MOVE`, offsets 2, 6, ..., `0x3e`) and copies them to `g_current_palette[0..15]`.
   - `g_bottom_palette_pending`: writes the colors into the second copper list and `COLOR00`–`COLOR15`, and copies them to `g_current_palette[16..31]`.
   - Message area scroll (see [Message area](#message-area)).

`g_palette_copper_lists` (`0x1024da`) points to three pointers into the copper lists. The video driver module provides them.

In C, run the vblank work once per 50 Hz frame from the platform layer. Apply the top palette to the viewport rows and the bottom palette to the rest of the screen.

## Input (`0x105908`–`0x106372`)

`input_event_handler` is in the `input.device` handler chain (priority `0x7f`). It runs on the input task, not on the game task. It reads the event list:

| Event class | Work |
|---|---|
| `IECLASS_RAWMOUSE` (2) | Adds the relative motion to `g_mouse_x_hires`, `g_mouse_y_hires` (clamped to 0–639, 0–399), unless `g_ignore_mouse_movements` is set. Stores the halves in `g_mouse_x`, `g_mouse_y`. Buttons call `command_process_click(x, y, kind)`. |
| `IECLASS_RAWKEY` (1) | Converts the key with `RawKeyConvert` and calls `key_queue_put`. |
| `IECLASS_DISKINSERTED` (`0x10`) | Calls `poll_disk_change`. |

Mouse button codes for `command_process_click`: 2 = left down, 4 = left up, 1 = right down, 8 = right up. `g_mouse_buttons` bit 1 is left, bit 0 is right. Left Amiga + Left Alt emulates the left button. Right Amiga + Right Alt emulates the right button.

Key words have the form `(rawkey << 8) | ascii`. The ASCII byte is 0 for keys without a printable character. Rules:

- Key-up events, repeated keys (`IEQUALIFIER_REPEAT`), and raw keys `>= 0x60` are ignored.
- Ctrl and Alt are removed before the conversion. With Ctrl, the result is `(key & 0xff3f) | 0x80`.
- Keypad keys map to cursor keys: 8 → `0x4c00` (up), 2 and 5 → `0x4d00` (down), 6 → `0x4e00` (right), 4 → `0x4f00` (left), 7 → `0x4600` (Del, turn left), 9 → `0x5f00` (Help, turn right).
- Left Amiga + `N` or `M` while another view is active: the handler loads the game view again, sets the sprite palette, and calls `view_restored_hook` (empty).

The handler removes all events from the chain while the game view is active.

Key queue: 64 words at `g_chip_block + 0x108`. `g_key_queue_head` and `g_key_queue_tail` wrap with `& 0x3f`. Access is inside `Forbid`/`Permit`. A full queue drops the key.

The C port gets the same key words and click kinds from SDL events. A mutex is not necessary when input and game run on one thread.

## Disk detection (`0x105f20`–`0x106148`)

- `detect_floppy_drives` probes units 1–3. The first unit that exists becomes `g_save_drive`. It clears `g_single_floppy_drive` and patches the drive letter in `g_save_drive_name`, `g_save_file_name`, and `g_save_backup_file_name`.
- `get_drive_disk_kind(which)` (1 = game drive, 2 = save drive, 0 = none) returns: 0 = `DungeonMaster:` disk, 1 = `DungeonSave:` disk, 2 = write-protected save disk, 3 = other disk, 4 = no disk. It reads the volume name with `Lock` and `Examine`. The `FileInfoBlock` is at the start of the chip block.
- `poll_disk_change` runs in the input handler. It compares the trackdisk change counters with the stored values and sets `g_game_disk_changed` and `g_save_disk_changed`. `has_disk_changed(which)` returns and clears the flag.
- `disk_prompt_hook`, `format_done_hook`, and `unused_stub_105f18` are empty.

The C port replaces this group with a save directory. These functions can return fixed values.

## File I/O (`0x1061b8`–`0x1062ca`)

- `file_open_read` and `file_open_write` return a dos file handle (`BPTR`), or 0.
- `file_seek(fh, pos)` seeks from the start and always returns −1.
- `file_tell(fh)` (`0x10625a`, before: `file_rewind`) returns the current position. It does not move. No code calls it.
- `file_size` seeks to the end and back.
- `file_read`, `file_write` return 1 when all bytes transfer. Note the parameter order: handle, size, buffer.

Map these functions to `fopen`, `fseek`, `ftell`, `fread`, `fwrite`.

## Palette and display (`0x1065c6`–`0x106ff6`)

- Colors are Amiga 12-bit RGB words `0x0RGB`.
- `palette_fade(target, start)` fades in 15 steps. `start` has 32 colors. `target` has 16 colors, used for both halves. Each step reads a delta table from the video driver (`*(g_video + 0x130)`, 15 × 16 bytes, indexed by step and component difference). It sets both palette-pending flags and busy-waits until the vblank handler clears them. The fade takes 15 frames. Work buffers are in the chip block: `0x1c8` (current) and `0x208` (copy of the start palette).
- `palette_fade_to(target)` fades from `g_current_palette`.
- `fade_display_out` fades to `g_dungeon_palettes[0]` (the normal palette), not to black.
- `connect_video_driver` finds the `VDEO` driver, points the palette globals at the driver data, waits for the vblank handler, calls `LoadView`, and loads `g_initial_colormap` (32 colors) into the screen color map.
- `wait_for_raster_line(line)` busy-waits until `VPOSR` reaches `line + 0x2c`. Fills and XOR boxes call it to avoid tearing. The C port can drop it.

## Fills and blit wrappers (`0x106a04`–`0x10760c`)

A box is 4 words: `x, y, width, height`. `box_to_bounds` converts it to `x1, x2, y1, y2` (inclusive).

- `fill_bitmap_box(bitmap, box, color, width, height)` fills with `RectFill` through a temporary rastport in the chip block (offset `0x248`, bitmap structure at `0x2ac`). A null bitmap means the screen.
- `fill_bitmap_box_pattern` uses the checkerboard pattern `0xaaaa, 0x5555` (`g_fill_pattern`).
- `fill_screen_box` and `complement_screen_box` draw on the display rastport (`g_video + 0xa0`).
- `clear_screen(color)` writes 8000 bytes to each of the 4 display planes (`g_video + 0x80`).
- `xor_screen_box(box)` inverts plane 2 of `g_screen_bitmap` (offset 16000). This toggles color bit 4.
- The `blit_*` wrappers call `blit_bitmap` with the viewport or the screen as destination. The `*_zone` forms get the box from `get_zone`. The `blit_graphic_*` forms get the bitmap from `get_layout_bitmap`. A negative graphic index uses the offset from the layout entry.
- `get_scaled_bitmap` and `get_cached_scaled_bitmap` shrink a graphic into a derived bitmap with `blit_shrink_with_palette_changes`.

Bitmap layout: 4 planes, not interleaved. Each plane is `((width + 15) & ~15) / 8 * height` bytes. The 4-byte header before the data holds width and height.

## Strings, math, random (`0x106e0e`–`0x1076f8`)

- `str_*` work like the C library. `str_find_char` returns null when the character is not found. `str_replace_char(src, insert, c, dst)` copies `src` to `dst` and replaces the first `c` with `insert`. It returns 1 when it found `c`. The language file names use it with `~`.
- `bounded_value(min, value, max)` clamps `value`.
- `scale_value(a, shift, b)` returns `(a * b) >> shift` with an unsigned 16×16 multiply. Callers use the low word.
- Random: `seed = seed * 0xBB40E62D + 11`; the results are `(seed >> 8)` masked to 16, 1, or 2 bits. Keep the 32-bit wrap-around.

Emulator checks (`dmtools.emu`): `bounded_value(0, 15, 10) = 10`, `scale_value(100, 3, 50) = 625`, `rt_ldiv(-7, 2) = -3`, `rt_lmod(-7, 2) = -1`.

## Main loop (`0x10770e`–`0x1078de`)

The [main loop](../kaos-map.md#main-loop) section of the map describes `game_loop`. Additional facts:

- `game_loop` also clears `g_pressing_closed_imaginary_fake_wall` when the square ahead is not a closed imaginary fake wall (type 6, bit 0 set, bit 2 clear). Then it shows the mouse pointer again.
- `g_party_freeze_life_ticks` is a byte. The loop decrements it each tick.
- Champion time effects run when `(g_game_time & 0x3f) == 0`, or `& 0xf` when the party sleeps.

## Objects and slot boxes (`0x10791c`–`0x107d56`)

- `load_object_names` reads graphic `0x2b6` (199 zero-terminated names) and builds `g_object_names`. It also allocates the icon bitmap for the mouse pointer.
- `object_get_icon_index(thing)` starts from `g_object_info[i].type` and adds an offset for some icons: compass (0) + party direction, lit torch (4) + `g_torch_charge_to_icon_offset[charges]`, and +1 for some items when charge bits are set.
- Icons are 16 per row in graphics `0x2a`–`0x30`. `g_icon_graphic_first_index` holds the first icon of each graphic (0, 32, ..., 192).
- `g_slot_boxes` has 4-byte entries: zone index, current icon index. Entries 0–7 are on the screen (action hands), higher entries are in the viewport (inventory and chest).
- `draw_leader_hand_object_name` writes the object name into zone `0x11`. For a champion bones object (icon `0x93`), it combines the champion name and a suffix. The order depends on `g_language` (French puts the suffix first).

## Memory helpers

- `clear_memory(dst, size)` clears from the end to the start with blocks of 52 bytes from `g_zero_block`.
- `fill_bytes_stride` and `fill_words_stride` write `count` values with a stride. `count` must not be 0 (the loop wraps to 65536).

## Mouse pointer (`0x107dbc`–`0x108710`)

The pointer uses hardware sprites 4–7 as two attached pairs (32 pixels wide, 18 lines, 16 colors).

- `g_mouse_sprite_data` (chip, `0x640` bytes) holds 5 shapes of `0x140` bytes. Slot 0 is empty (hidden pointer). Slots 1–4 are the shapes: 0 = arrow, 1 = hand, 2 = object, 3 = champion icon. Each shape has 4 sprites of `0x50` bytes (control words, 18 lines of 2 words, end words).
- `build_mouse_pointer_sprite(bitmap, shape)` converts a 4-plane bitmap to the sprite format.
- `build_mouse_pointer_bitmap` adds a shadow. It offsets the recolored copy by `g_pointer_shadow_dx`, `g_pointer_shadow_dy` and swaps colors 0 and 12 (`g_pointer_swap_colors_0_12`).
- `get_mouse_pointer_shape_for_position(x, y)` returns −1 (hidden), or the shape. It checks the 16 entries of `g_mouse_pointer_zones` (`x, y, width, height, shape, champion`). A champion value of 1–4 selects a champion; values 9–12 also select the champion name area.
- Tracking box: `mouse_set_tracking_box(box, command)` (before: `set_mouse_pointer_shape`) stores a box and a command. When the pointer leaves the box, the next shape check calls `command_process_click(x, y, command)`.
- `mouse_process_champion_icon_click(cell)` (before: `set_mouse_pointer_to_champion`) starts or ends the drag of a champion icon. The first click picks the icon up. The second click swaps the champions of the two cells. `mouse_cancel_champion_icon_drag` drops the icon on its start cell.
- `update_mouse_pointer_sprites` runs in the copper interrupt. It changes and moves the sprites. The hot spot comes from `g_mouse_pointer_hotspots`.
- `mouse_begin_change` and `mouse_end_change` are empty. The hardware sprites need no save and restore.

The C port draws the pointer into the frame after the game image.

## Sound (`0x10871a`–`0x109054`)

`audio_device_open` allocates all four channels (mask `0x0f`). It makes two stereo pairs:

| Pair | Control request | Left channel | Right channel |
|---|---|---|---|
| A | `g_audio_control_a` (channels 2, 3) | 3 (`g_audio_write_a_left`) | 2 (`g_audio_write_a_right`) |
| B | `g_audio_control_b` (channels 0, 1) | 0 (`g_audio_write_b_left`) | 1 (`g_audio_write_b_right`) |

`g_sounds` has 35 entries of 12 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | word | Graphics.DAT index of the sample. −1 = disabled. |
| 3 | byte | Rate value. Period = `0x11c60 / rate` (72800 / rate). |
| 4 | byte | Priority. |
| 6, 7 | byte | Not decoded. |
| 8 | long | Sample pointer (loaded on first use). |

The sample data starts 2 bytes after the graphic start. The length is the graphic size − 2.

- `sound_get_volume(map_x, map_y, volumes)` rotates the offset from the party into party space. It returns 0 when the source is more than 12 squares away. Otherwise it reads left and right volumes from `g_sound_volume_table` (25 × 25).
- `sound_request_play(sound, x, y, mode)`: mode 0 plays now, mode 1 queues the sound for `sound_play_pending` (two slots, louder or higher priority wins), mode ≥ 2 adds timeline event type 20 at `g_game_time + mode − 1`.
- `sound_play_sample(sound, volumes)` (high byte left, low byte right) picks a free pair. When both pairs play, it replaces the quieter pair (`sound_compare_loudness`). It stops the pair, writes both channels, and starts them together.
- `sound_compare_loudness(a, b)` (before: `sound_select_channel`) puts each left+right sum in a class (< 15, < 26, else) and returns 1, 0, or −1.
- `sound_disable_unloaded` disables the sound groups (19, 9, 7 entries) whose flag at `0x1001d7`–`0x1001d9` is 0.

## Text (`0x109116`–`0x109b4e`)

`text_print_to_bitmap(bitmap, width, x, y, fg, bg, text, height)` is hand-written assembly:

- Glyphs are 6 pixels wide and 6 lines high. `y` is the baseline; the glyph starts at line `y − 4`.
- `g_font_bitmap` (`0x300` bytes, graphic `0x2b7`) has 6 rows of 128 bytes, one byte per character.
- It writes directly to the 4 planes. It clears the character cell with the masks `g_glyph_masks_left` and `g_glyph_masks_right`, then sets the planes for `fg` (glyph pixels) and `bg` (other pixels in the cell). It uses a jump table per color.
- `text_print_to_viewport` and `text_print_to_viewport_zone` use background color 12.

Other functions:

- `text_get_zone_size` returns width `n * g_text_char_width − g_text_char_gap` and height `g_text_line_height − g_text_baseline_gap`.
- `text_get_next_word_line` copies words up to `max_width` pixels and returns the width.
- `text_print_to_bitmap_zone`, `text_print_padded_to_bitmap`, and `text_print_padded_to_bitmap_zone` were named `text_print_centered_in_zone`, `text_print_padded_to_viewport`, and `text_print_padded_to_screen`. They take any bitmap. The padded forms add spaces up to `length` (buffer of 80 bytes).

The text metric globals at `0x102962`–`0x102976` come from the layout data. Their names are provisional.

## Message area

The message area has 4 rows at the bottom of the screen (zone `0xf`). `g_message_area_cursor_row` and `g_message_area_cursor_x` hold the cursor. Each row has an expiry time in `g_message_area_row_expiry` (`g_game_time + 70`, −1 = empty). `message_area_clear_expired_rows` clears expired rows.

When the cursor is on row 3 and a new row is necessary:

1. `message_area_create_new_row` clears `g_message_area_row_bitmap` and sets `g_message_area_scroll_line` to `g_message_area_row_height − 1`.
2. Text goes into the row bitmap, not to the screen.
3. Each vblank scrolls the area one pixel line up (`copy_bytes` per plane) and copies one more line of the row bitmap to the bottom. A full row takes 7 frames.
4. When text arrives after the scroll, `g_message_area_copy_pending` tells the vblank handler to copy the whole row bitmap.

Functions that draw into the area busy-wait until the scroll ends. The C port must run the scroll from the frame timer, or the wait never ends.

## Trackdisk and save disk format (`0x109bb2`–`0x10a3a6`)

- `trackdisk_init` creates two `IOExtTD` requests. `g_trackdisk_request` is for the game task. `g_trackdisk_change_request` is for `poll_disk_change` in the input handler.
- `trackdisk_get_disk_state(unit)`: 0 = no disk, 1 = write-protected disk, 2 = writable disk.
- `dos_send_packet` sends a `StandardPacket` to a dos handler. `dos_inhibit_drive` (`ACTION_INHIBIT`) and `dos_relabel_disk` (`ACTION_RENAME_DISK`) return non-zero on failure.
- `trackdisk_format_disk(unit, name)` formats 160 tracks of `0x1600` bytes with `ETD_FORMAT`, reads each track back with `ETD_READ`, and compares. Each track tries 4 times. Then it writes an OFS root block (block 880) and a bitmap block (block 881). It returns the trackdisk error, 0 on success. `draw_format_progress` draws the progress bar.

The C port does not need this group.

## Not called

No caller was found for `unused_stub_105f18`, `file_tell`, `debug_print_char`, `complement_screen_box`, `get_scaled_bitmap`, `text_print_padded_to_bitmap`, `message_area_print_char`, `dos_relabel_disk`, and `trackdisk_get_change_count`. The `rt_switch_*` helpers have no Ghidra references, but compiled `switch` statements can call them.
