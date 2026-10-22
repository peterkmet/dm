# Chaos Strikes Back data files

The data files of Chaos Strikes Back (CSB) for the Amiga compared with the files of Dungeon Master (DM) v3.60, and the port module that reads each CSB file.

Inputs:

- DM files: `game/`. DM engine: `KAOS.FTL`, decompiled to `reference/decomp/dm/KAOS.c`.
- CSB disk A (game disk): `game-csb/`. CSB engine: `APPB.FTL` (the disk label says V3.1), decompiled to `reference/decomp/csb-a/APPB.c`. Load base `0x100000`, data base (A5) `0x10511e`.
- CSB disk B (utility disk): `game-csb/disk-b/`.

All addresses in this page are image addresses after `dm-ftl` with base `0x100000`. "Verified" means a test output or a code address supports the statement. "Unverified" marks an inference.

## Summary

| Area | Difference | Port module |
|---|---|---|
| `Graphics.DAT` | 728 items, not 749. Same container format. Text items 699 and 700 are empty. Item 694 uses a different string encoding. | `gfx_open` and `gfx_expand` read and decode all items. `csb_text.c` builds the text items from `APPB.FTL`. |
| `Dungeon.DAT` | A 2-level start dungeon with 24 champion mirrors, dungeon ID 12. Same format. | `dungeon_load` loads it. |
| Save file | `CSBGAME.DAT`. Header fields are 2 bytes later. The payload key is at a different offset. Blocks, cipher, and dungeon part are the same. | `savefile.c` reads and writes the CSB header when `g_adventure` is `ADVENTURE_CSB`. |
| Endgame | `ENDA.DAT`, an animation file that `ANIM.FTL` plays. No credits screen. | `anim.c` plays `ENDA.DAT`. |
| Sounds | Same 35 sounds, same graphic items, same sound table. | `sound.c` works with the CSB items. |
| Engine tables | No `KAOS.FTL`. `APPB.FTL` holds the tables at different addresses. | `csb_image.c` builds a KAOS-layout image from the `APPB.FTL` tables. |

## Graphics.DAT

### Container

Both files use format 1 (`0x8001`), the header of [kaos-map.md](../reference/kaos-map.md#graphicsdat), and equal compressed and decompressed sizes for every item.

| File | Items | Size in bytes | CRC32 |
|---|---|---|---|
| DM `game/Graphics.DAT` | 749 | 458108 | `0x04598281` |
| CSB `game-csb/Graphics.DAT` | 728 | 402254 | `0x3bc83984` |

`dmcsb` (the Dungeon Master tools by Cycl0ne, see the README) calls the DM file `amiga_csb_ml` ("CSB Amiga Multilanguage"). The CRC32 of that variant is the CRC32 of the DM v3.60 file. `dmcsb` does not know the CSB file (no CRC32 match, no structural match for 728 items).

### Item comparison

An alignment by content hash matched the items (`difflib.SequenceMatcher` on MD5 of each item). Items 0–726 keep their index. DM items 727–747 are not in CSB. DM item 748 (2 bytes) is CSB item 727.

Removed items (DM 727–747): the empty item 727 and the French and German items (entrance, THE END label, rename panel, food, water, and poisoned labels, resurrect panel, object names, action names, texts).

Changed items (same index, different content):

| Index | DM content | CSB content |
|---|---|---|
| 2–5 | Entrance doors, entrance, credits | Same sizes, CSB art |
| 10 | Action area | Same size, different content |
| 11 | Entrance buttons, 14 × 39 | 14 × 26 (UNVERIFIED: 2 buttons, not 3) |
| 26 | Champion portraits, 256 × 87 | 24 CSB portraits, same size |
| 42–45, 47 | Object icon sheets (icons 0–127, 160–191) | Same sizes, CSB icons |
| 329–330 | Wall ornament 35 (fountain) | Different size |
| 371–378 | Wall ornaments 56–59 (amalgam, Lord Order) | 6-byte placeholders, 16 × 13 |
| 504, 550 | Objects 6 (drumstick), 52 (conduit) | Different size |
| 588–602, 607–608, 627–650, 653–662, 664 | Creature graphics | Different sizes. 600–602, 649 are 3 × 3 placeholders. |
| 669 | Creature 25 (Lord Order) | 3 × 3 placeholder |
| 671–693, 701–711 | Sound samples | Same size. The header width and the first word differ (see [Sounds](#sounds)). |
| 694 | Object names, NUL-terminated | Object names, last character has bit 7 set |
| 696 | Layout | Zones 437 and 438 are not present |
| 699 | Action names | Empty (0 bytes) |
| 700 | Texts | Empty (0 bytes) |

All other items in 0–726 are byte-identical.

### Port decoding test

A check decodes every item of both files with `gfx_load_bitmap`: the decoded width and height equal the header dimensions, and the 673 CSB image items (0–670, 676, 686) equal the `dmcsb` pixels. The items with fewer than 4 bytes are not bitmaps.

Result: `gfx_expand` decodes the CSB items without a CSB branch.

### Strings and layout

Object names (item 694). The CSB engine reads 199 names at `0x108280` (graphic index `0x2b6` pushed at `0x1082ac`). Each name ends at a byte with bit 7 set. The engine clears bit 7 and adds a NUL. `dmcsb` calls this encoding TXT1. The DM file uses NUL-terminated strings (TXT2). `load_strings` (`src/data/graphics.c`) ends each CSB object name at the byte with bit 7 set.

33 of the 199 CSB names differ from DM. Examples: 23–24 STORM (DM BOLT BLADE), 25–26 RA BLADE (FURY), 38 SIDE SPLITTER (DELTA), 41 DRAGON FANG (THE INQUISITOR), 62–64 STAFF OF IRRA, CROSS OF NETA, SERPENT STAFF, 174 SHANK (DRUMSTICK).

Action names and texts (items 699 and 700). CSB has no text in these items. The CSB engine has the texts in the `APPB.FTL` data area. Examples: the action names start at `0x1048b4` as one NUL-separated list. `WAKE UP` and `GAME FROZEN` are at `0x104680`. `GAME LOADED, READY TO PLAY.` is at `0x104d70`. A pointer table refers to some texts. [engine-diff.md "Text"](engine-diff.md#text) maps the CSB texts to the DM indices of item 700. The CSB engine does not push `0x2bb` (699). DM pushes it at `0x1220b8`.

Layout (item 696). A check compares the zone records with `dmcsb.graphics.decode_lay1`. The range `400–438` becomes `400–436`. All other records are equal. `startend.c` uses zone 437 (`ZONE_RESTART_BUTTON`) and zone 438 (`ZONE_QUIT_BUTTON`) for THE END screen.

### Hard-coded indices with different CSB content

| Port constant | Index | File | CSB effect |
|---|---|---|---|
| `GFX_ENTRANCE_LEFT_DOOR`, `GFX_ENTRANCE_RIGHT_DOOR`, `GFX_ENTRANCE` | 2, 3, 4 | `ids.h` | CSB entrance art. Button zones not checked. |
| `GFX_CREDITS` | 5 | `startend.c` | CSB credits art. The CSB engine does not show credits at the end. |
| `GFX_ACTION_AREA_NAMES` | 10 | `ids.h` | Different content, same size. |
| `GFX_CHAMPION_PORTRAITS` | 26 | `ids.h` | CSB portraits. `savegame.c` matches saved portraits against this sheet (see [Save file](#save-file)). |
| `GFX_OBJECT_ICONS_BEFORE_FIRST` + 1–4, 6 | 42–45, 47 | `ids.h` | CSB icons. The `ICON_*` names of DM items name the CSB items at the same indices, for example `ICON_BOLT_BLADE`, `ICON_FURY`, `ICON_DELTA`, `ICON_INQUISITOR`. |
| `GFX_OBJECT_NAMES` | 694 | `ids.h`, `ui.c` | TXT1 encoding. |
| `GFX_ACTION_NAMES` | 699 | `ids.h`, `ui.c` | Empty. |
| `GFX_STRINGS` | 700 | `ids.h`, `ui.c`, `champion.c` | Empty. |
| `GFX_LAYOUT` zones 437, 438 | 696 | `startend.c` | Zones are not present. |
| `GFX_CREATURE_FIRST` + aspect | 584–670 | `ids.h`, `view.c` | Different bitmaps and sizes. `view.c` reads the size from the bitmap. |
| Wall ornaments 56–59 | 371–378 | `view.c` | Placeholders. CSB maps must not use them. |

Unchanged indices that the port uses: 0, 6, 7–9, 12–25, 27–41, 46, 48–328, 346, 379–503, 695 (font).

## Dungeon.DAT

`game-csb/Dungeon.DAT` is the start dungeon of the game disk. It is not the CSB adventure.

| Field | Value |
|---|---|
| File size | 2098 bytes, compressed (`0x8104`) |
| Decompressed size | 2622 bytes, checksum present and correct |
| Dungeon ID | 12 (DM: 10) |
| Maps | 2: map 0 is 20 × 16 (level 0), map 1 is 21 × 16 (level 1, offset 1, 3) |
| Party start | Map 0, X 9, Y 0, direction 2 (south) |
| Thing counts in the file | 4 doors, 26 texts, 30 sensors, 12 groups |
| `g_timeline.capacity` after a new-game load | 297 |

A check loads the file with `kaos_load("game/KAOS.FTL")` and `dungeon_load`. The call returns true. The thing counts include the new-game extras (groups 87 = 12 + 75).

`dmcsb` identifies the file by CRC32 as `atarist_csb` (Atari ST CSB 2.x dungeon). The Amiga file is byte-identical to it.

Content:

- Map 0 has 24 mirrors: wall sensors of type 127 with data 0–23. Each mirror has a text with a champion sheet. The champions are AIRWING, AROC, TALON, LETA, DEMA, ALGOR, TOADROT, VEN, MANTIA, GNATU, SLOGAR, STING, SKELAR, DETH, NECRO, PLAGUE, TUNDA, LANA, BUZZZZZ, PETAL, ITZA, TULA, KAZAI, LOR.
- 6 mirrors have a second sensor: type 1, once only, effect 1, target the door at (1, 14) or (19, 2). The meaning in the CSB rules is unverified.
- One visible text on the floor of (9, 2), two squares south of the start, says "THERE IS ONLY ONE LEVEL HERE. FIRST CHOOSE YOUR CHAMPIONS...THEN SAVE THE GAME...THEN RESTART WITH THE UTILITY DISK." The game prints it when the party enters the square. The other 24 texts of map 0 are the champion sheets of the mirrors (not visible).
- Stairs down on map 0 (all to map 1): (19, 7) and (6, 15) can be reached from the start: through the imaginary wall at (15, 6) and the closed door with a button at (18, 6), and through the imaginary wall at (8, 14) and the closed door with a button at (7, 15). (19, 3) and (1, 15) are closed in, each with an open door north of it: (19, 1)–(19, 3) and (1, 13)–(1, 15) have no other exit (read with the port map accessors).
- Map 1 has no texts or sensors. It has 12 creature groups: 7 of type 10, 5 of type 7.

The CSB `load_dungeon` is at `0x1295a0`. It follows the DM loader and adds a copy-protection check ([CSB engine compared with the DM engine](engine-diff.md#copy-protection)).

Result: the port loader reads the file. The full CSB dungeon is not on disk A. The utility disk makes the adventure from a saved start game. The utility modules `CEDT.FTL` and `HINT.FTL` refer to `DF0:MINI.DAT`, `DF0:DMGAME.DAT`, and `DF0:CSBGAME.DAT`. `MINI.DAT` (42784 bytes) has the layout of a saved game, see [MINI.DAT format](new-adventure.md#minidat-format).

## Save file

The CSB `save_game` is at `0x128cc0` (jump table slot `0xb6a(A5)`). The CSB `load_game` starts at `0x129b3e`. The header writer is `0x128a42`, the header reader is `0x1289c4`, the block cipher is `0x128720`, and the checksum is `0x128752`.

### File names

`DF0:CSBGAME.DAT` and `DF0:CSBGAME.BAK` (strings at `0x10013a`, `0x10014a`). The utility modules use `DF0:CSBGAME.DAT` and `DF0:CSBGAME.BAK`.

### Same as DM

- The file layout: 512-byte header, blocks 0–4, 4 portraits of 464 bytes, uncompressed dungeon with a byte sum word.
- The header chain formula and the 128-word payload at `0x100`.
- The block cipher and the checksum (`0x128720` and `0x128752` equal `0x125510` and `0x125542`).
- Block 0: the same 21 globals at the same offsets.
- Block 1: `max_active_group_count × 16`. Block 2: `0x580` bytes (4 × `0x140` champions, then `0x80` party). Block 3: `event_max_count × 10`. Block 4: `event_max_count × 2`.
- Portraits: 32 × 29 (`0x1032bc`), interleaved before the write.
- Dungeon part: the same parts in the same order, thing record sizes as DM.
- The leader load does not count the hand object.

### Header differences

The offsets of the DM and CSB headers are in [save.md](../formats/save.md#csb-header).

Evidence: the writer at `0x128cc0` and the reader at `0x129b3e` use the same offsets. The machine code at `0x12928c` is `moveq #5,d0; move.b d0,$12d(a3); move.b -$421(a5),$12c(a3)`.

The load and save behavior of the CSB engine is in [CSB engine compared with the DM engine](engine-diff.md#save-and-load).

Result: `savefile.c` reads and writes the CSB header when `g_adventure` is `ADVENTURE_CSB`, and then also checks the block 2 checksum. `tests/port/test_csb_save.py` decodes `MINI.DAT` with the port. The original CEDT reader (`tests/differential/test_csb_mini.py`) accepts a file that the port writes.

## ENDA.DAT and the endgame

### File format

`ENDA.DAT` (91420 bytes, disk A), `TITL.DAT` (DM, and 15638 bytes on CSB disk B), and `ANIM.DAT` (264324 bytes, disk B) use one chunk format. All words are big-endian.

| Offset | Size | Content |
|---|---|---|
| 0 | 2 | Tag, two ASCII characters |
| 2 | 2 | Payload length in bytes |
| 4 | 2 | Parameter |
| 6 | length | Payload |

The next chunk follows the payload directly. `ANIM.FTL` `0x101356` walks the chunks with this step. A walk of all four files with this step reaches the exact file end. All payload lengths are even.

The chunk dispatcher of `ANIM.FTL` is `0x100d1a`: a switch table at `0x100d30` (18 entries: offset word, then tag word; each offset is relative to its own position; default offset at `0x100d7a`). The loop `0x100fca` runs the chunks from the first chunk to the file end. The disassembly (`machine68k`) confirms each handler:

| Tag | Handler | Meaning | Port (`src/ui/anim.c`) |
|---|---|---|---|
| `PL` | `0x100760` | Payload: count word, then count entries of 4 bytes: color index, red, green, blue (4 bits each). The colors go into a pending palette. The next frame applies it (`0x1016d8`). | Yes |
| `EN` | `0x100812` | Full frame: the `Graphics.DAT` packed graphic format (width, height, commands), decoded by the GRF1 function behind slot `0xea` into the back buffer. The parameter is the time in vertical blanks (50 Hz) that the frame stays. | Yes |
| `DL` | `0x100846` | Delta frame: the blitter first copies the shown screen into the back buffer (`0x1017bc`), then the same decoder draws the frame. Skip commands keep the old pixels. Pad word `0xff81` before the size. Parameter as `EN`. | Yes |
| `SD` | `0x10087a` | Sample: adds the payload to the sample table (max. 32). Payload: length word, then the 8-bit sample data. | Yes |
| `SO` | `0x1008f2` | Plays sample number parameter (1-based) at Paula period 650 (`0x101406`). The two bytes after the chunk header give the volumes: (byte + 1) >> 2. `SO` has no payload in `ENDA.DAT`, so the two volume bytes are the first two bytes of the next tag, `DL` (volumes 17 and 19). | Done. UNVERIFIED: which volume is the left channel. |
| `FO` | `0x100b64` | Loop start: pushes the loop (count parameter - 1, start = the second chunk after `FO`) onto a stack of 16 loops. The chunk after `FO` runs once. | Yes |
| `NE` | `0x100be2` | End of a pass: decrements the count. When the count was 0, it pops the loop and skips the two chunks after `NE`. | Yes |
| `BN` | `0x100c7e` | Goes back to the loop start. | Yes |
| `BR` | `0x10073a` | The parameter allows a stop by key or click (`0x10104e`, input types 1 and `0x11`). `ENDA.DAT` has 0: no stop. | Not used (always 0 in `ENDA.DAT`) |
| `AN` | `0x100722` | Skipped by the player. Payload: width 320, height 200, 4 planes, a count. | Skipped |
| `DO` | `0x100c9c` | Calls slot `0x182` (`0x102d52`): `ADCMD_FINISH` (command 11) on both audio channel pairs, so the playing samples end with their cycle. | No action: the port voices play once and stop |
| `MI` | `0x100946` | Music: plays music number parameter at the next frame (`0x101566`). | Not used (no `MI` in `ENDA.DAT`) |
| `MD`, `SF`, `MF` | `0x1008b6`, `0x100970`, `0x100a6a` | Music data, sample file, music file (loads the named file into the sample or music table). | Not used (`ANIM.DAT` only) |
| `TR`, `WA` | `0x100cf0`, `0x100cc6` | Empty functions. | Not used |
| `TD`, other tags | default `0x100e80` | Skipped. | Skipped |

`ENDA.DAT` starts with `BR`, then `AN`. It has 2 `SD`, 3 `PL`, 2 `EN`, 285 `DL`, 3 `SO`, 1 `FO`, 1 `NE`, 1 `BN`, and 1 `DO`. Every `EN` and `DL` payload decodes to exactly 320 × 200 pixels with 0 or 1 pad byte left (`EN` has no skip commands). The frames show the volcano, the mountain, and THE END. The loop (`FO` 8) repeats 19 frames 8 times at the end. With the loop, the animation has 426 frames.

The `SO` chunks play `SD` sample 2 (graphic item 683) three times.

### Engine use

The engine path from the endgame sensor to `ANIM.FTL` is in [engine-diff.md](engine-diff.md#end-of-the-game).

Port: `startend_run_endgame_sensor` in `src/ui/startend.c` sets `g_session.won`, plays `ENDA.DAT` from the disk A folder with `anim_play`, waits for a key or a click, and shows the CSB screen.

### TITL.DAT and ANIM.DAT on disk B

- `TITL.DAT` (disk B): the title animation of the utility disk. 1 `AN`, 1 `PL`, 1 `EN`, 31 `DL`, 1 `DO`.
- `ANIM.DAT` (disk B): the long intro animation. It loads 5 samples from `Sound/` by name and music from `MIDI/NAKED.AMG`. The 5 files in `Sound/` equal CSB graphic items 688, 675, 683, 684, and 685.

The port plays `ANIM.DAT` after NEW ADVENTURE (`play_csb_intro` in `src/platform/app.c`). It does not read `TITL.DAT` of disk B.

`ANIM.FTL` of disk B is a different build from disk A (all addresses below are disk B addresses). Dispatcher `0x100c76`, table at `0x100c8e`. Handlers:

| Tag | Handler | Meaning |
|---|---|---|
| `SF` | `0x100920` | Payload: a file name (`SOUND/TELE2.amg`). The file is a sample: length word, signed 8-bit data. It goes into the `SD` sample table; a file that cannot be read keeps its number. AmigaDOS ignores the case of the name; the files on disk are `Sound/tele2.amg`. |
| `MF` | `0x100a1a` | Payload: a music file name (`MIDI/NAKED.amg`), into the music table. `MD` (`0x100866`) puts its payload into the same table. |
| `MI` | `0x1008f6` | Plays music number parameter (1-based) immediately (`0x101424`). |
| `SO` | `0x1008a2` | As on disk A (period 650, volumes `(byte + 1) >> 2`). The word after the two volume bytes (`0x1388`) is not read. While the music plays, a sample goes to pair A (channels 3 and 2), and a new sample stops the previous sample. |
| `DO` | `0x100c4c` | Ends the samples after their cycle. It does not stop the music. |

`ANIM.DAT` has 851 frames, 10808 vertical blanks (216 s). After the last frame or a stop (`BR` 1), the cleanup fades the music: volume 29 down to 1, one step every 3 ticks (`0x102e4c`), then the music stops.

A stop is not immediate in the original. The shown frame stays for the rest of its time, one more frame shows for its full time, and then the music fades for 87 ticks (1.7 s). Frames of the title pictures last up to 536 ticks (10.7 s). The port in vanilla keeps only the shown frame for the rest of its time, then fades the music; in the advanced mode it stops immediately (`anim_set_stop_at_once`), then fades the music.

`NAKED.AMG` is not a tracker or MIDI file. All words are big-endian:

| Offset | Content |
|---|---|
| 0 | `n` = 6, the item count. `MI` rejects the file when bit 7 of byte 0 is set. |
| 2 | Table 1: `n` item sizes. `MI` checks only that the first entry equals the first entry of table 2. |
| `2 + 2n` | Table 2: the `n` item sizes in bytes. |
| `2 + 4n` | The items, one after another. |

Item 0 is the sequence: item numbers, ended by `0xff` (`1,1,2,2,1,1,2,2,3,4,3,4,3,5,3,4`). Items 1 to 5 are samples (length word, signed 8-bit data). `MI` queues the sequence 6 times (96 writes) on pair B (channels 0 and 1, the same data on both), period 328, volume 30, one after another without a gap. One pass is 48.9 s; the music ends after the sixth pass (293.5 s), so the animation ends first.

## Sounds

The CSB engine takes the sounds from `Graphics.DAT`, as DM does.

- The CSB sound table at `0x10122a` has 35 entries of 12 bytes. It is byte-identical to the DM table at `0x100d52` (all 35 entries are equal). The graphic indices are 671–712.
- The CSB init `0x1091ce` reads each sample graphic, sets the length to the graphic size - 2, and keeps the data after the first word. The play routine `0x109258` uses the length and the period `72800 / rate` (`0x11c60`).
- In 26 sample items, the header width and the first word are 1 or 2 smaller than in DM. The sample data is identical. The CSB engine does not read this word. `sound_get_sample` (`src/engine/sound.c`) uses the item size, not the header width or this word.

The CSB engine has no new sound in `Graphics.DAT`. New samples exist only in the animation files (`SD` chunks, `Sound/*.amg`) and in `MIDI/NAKED.AMG`.

## Engine tables

The port reads all rule and view tables from the `KAOS.FTL` image. CSB has no `KAOS.FTL`; `csb_image.c` builds a KAOS-layout image from the `APPB.FTL` tables. [engine-diff.md](engine-diff.md#equal-tables-at-new-addresses) lists the CSB addresses of the view tables, the input tables, and the escape tables. `g_csb_copies` (`src/data/csb_image.c`) copies them.

## Loaders

| File | Function | CSB behavior | Reason |
|---|---|---|---|
| `src/data/graphics.c` | `load_strings` | Ends each CSB object name at a byte with bit 7 set. | CSB object names end with bit 7, not NUL. |
| `src/data/csb_text.c` | `csb_text_build`, `csb_text_get_item` | Builds the text items from `APPB.FTL`, with a map to the DM text indices. | Items 699 and 700 are empty in CSB. |
| `src/ui/startend.c` | `csb_draw_restart_button` | Draws RESTART THIS GAME in the two boxes of APPB `0x100dcc` (image address `0x104248`) and keeps the click box of the CSB endgame mouse table. | The CSB layout has no zones 437 and 438. |
| `src/engine/savefile.c` | `savefile_decode`, `savefile_encode`, `savefile_init_header`, header accessors | Reads and writes the CSB header: key at `0x3a`, fields 2 bytes later, 132 extra bytes. | A CSB save fails the DM header check. |
| `src/engine/savegame.c` | `savegame_save`, `savegame_load`, portrait functions | Uses `CSBGAME.DAT` and `CSBGAME.BAK`. Keeps the saved portrait bitmaps. | CSB parties come from DM saves or the champion editor. Their portraits are not in graphic 26. |
| `src/data/kaos.c`, `src/data/csb_image.c` | `kaos_load`, `csb_image_build` | Loads `APPB.FTL` and builds a KAOS-layout image: each table is at its DM address. | CSB has no `KAOS.FTL`. Most tables are identical at different addresses. |
| `src/ui/anim.c` | Animation player | Plays the chunks of `ENDA.DAT`. | The CSB endgame plays `ENDA.DAT`. |

`layout.c` and `dungeon.c` (`dungeon_load`) have no CSB code. The animation player uses `gfx_expand_over` (`graphics.c`, a decode over an existing bitmap for the delta frames) and `audio_play_data` (`audio.c`, plays sample data that is not in `Graphics.DAT`).

## Open points

- The meaning of the `AN` count (the player skips `AN`).
- The CSB rule differences for renamed objects (for example STORM, RA BLADE).
