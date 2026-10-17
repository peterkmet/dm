# Chaos Strikes Back data files

This page compares the data files of Chaos Strikes Back (CSB) for the Amiga with the files of Dungeon Master (DM) v3.60. It also states which port loaders read the CSB files and which loaders need changes.

Inputs:

- DM files: `game/`. DM engine: `KAOS.FTL`, decompiled to `reference/decomp/dm/KAOS.c`.
- CSB disk A (game disk): `game-csb/`. CSB engine: `APPB.FTL` (the disk label says V3.1), decompiled to `reference/decomp/csb-a/APPB.c`. Load base `0x100000`, data base (A5) `0x10511e`.
- CSB disk B (utility disk): `game-csb/disk-b/`.

All addresses in this page are image addresses after `dm-ftl` with base `0x100000`. "Verified" means a test output or a code address supports the statement. "Unverified" marks an inference.

## Summary

| Area | Difference | Port status |
|---|---|---|
| `Graphics.DAT` | 728 items, not 749. Same container format. Text items 699 and 700 are empty. Item 694 uses another string encoding. | `gfx_open` and `gfx_expand` read and decode all items. The text and layout users need changes. |
| `Dungeon.DAT` | A 2-level start dungeon with 24 champion mirrors, dungeon ID 12. Same format. | `dungeon_load` loads it. |
| Save file | `CSBGame.dat`. Header fields move 2 bytes up. The payload key is at another offset. Blocks, cipher, and dungeon part are the same. | `savefile_decode` rejects a CSB save. |
| Endgame | `ENDA.DAT`, an animation file that `ANIM.FTL` plays. No credits screen. | Not supported. |
| Sounds | Same 35 sounds, same graphic items, same sound table. | `sound.c` works with the CSB items. |
| Engine tables | No `KAOS.FTL`. `APPB.FTL` holds the tables at other addresses. | All `kaos_*` reads use DM addresses. |

## Graphics.DAT

### Container

Both files use format 1 (`0x8001`), the header of [kaos-map.md](../reference/kaos-map.md#graphicsdat), and equal compressed and decompressed sizes for every item.

| File | Items | Size in bytes | CRC32 |
|---|---|---|---|
| DM `game/Graphics.DAT` | 749 | 458108 | `0x04598281` |
| CSB `game-csb/Graphics.DAT` | 728 | 402254 | `0x3bc83984` |

`dmcsb` calls the DM file `amiga_csb_ml` ("CSB Amiga Multilanguage"). The CRC32 of that variant is the CRC32 of the DM v3.60 file. `dmcsb` does not know the CSB file (no CRC32 match, no structural match for 728 items).

### Item comparison

The scratch script `align.py` aligned the items by content hash (`difflib.SequenceMatcher` on MD5 of each item). Items 0–726 keep their index. DM items 727–747 are not in CSB. DM item 748 (2 bytes) is CSB item 727.

Removed items (DM 727–747): the empty item 727 and the French and German items (entrance, THE END label, rename panel, food, water, and poisoned labels, resurrect panel, object names, action names, texts).

Changed items (same index, other content):

| Index | DM content | CSB content |
|---|---|---|
| 2–5 | Entrance doors, entrance, credits | Same sizes, CSB art |
| 10 | Action area | Same size, other content |
| 11 | Entrance buttons, 14 × 39 | 14 × 26 (unverified: 2 buttons, not 3) |
| 26 | Champion portraits, 256 × 87 | 24 CSB portraits, same size |
| 42–45, 47 | Object icon sheets (icons 0–127, 160–191) | Same sizes, CSB icons |
| 329–330 | Wall ornament 35 (fountain) | Other size |
| 371–378 | Wall ornaments 56–59 (amalgam, Lord Order) | 6-byte placeholders, 16 × 13 |
| 504, 550 | Objects 6 (drumstick), 52 (conduit) | Other size |
| 588–602, 607–608, 627–650, 653–662, 664 | Creature graphics | Other sizes. 600–602, 649 are 3 × 3 placeholders. |
| 669 | Creature 25 (Lord Order) | 3 × 3 placeholder |
| 671–693, 701–711 | Sound samples | Same size. Only the first word differs (see [Sounds](#sounds)). |
| 694 | Object names, NUL-terminated | Object names, last character has bit 7 set |
| 696 | Layout | Zones 437 and 438 are not present |
| 699 | Action names | Empty (0 bytes) |
| 700 | Texts | Empty (0 bytes) |

All other items in 0–726 are byte-identical.

### Port decoding test

The scratch script `port_gfx.py` loads `build/libdm.dylib` with `ctypes`, calls `gfx_open`, and calls `gfx_load_bitmap` for every item. Output:

```text
DM gfx_open -> 749
 decoded 731 failed 18 [697, 698, 713, ..., 727, 748]
CSB gfx_open -> 728
 decoded 709 failed 19 [697, 698, 699, 700, 713, ..., 727]
```

The failed items have fewer than 4 bytes (empty or 2-byte items). They are not bitmaps. In both files, the decoded width and height equal the header dimensions of each item.

The scratch script `pix.py` compares the port pixels with `dmcsb.graphics.decode_img1` for the 673 CSB image items (0–670, 676, 686). All 673 are equal.

Result: `graphics.c` needs no change for CSB.

### Strings and layout

Object names (item 694). The CSB engine reads 199 names at `0x108280` (graphic index `0x2b6` pushed at `0x1082ac`). Each name ends at a byte with bit 7 set. The engine clears bit 7 and adds a NUL. `dmcsb` calls this encoding TXT1. The DM file uses NUL-terminated strings (TXT2). `ui.c` `load_strings` splits on NUL, so it gives one long string for CSB.

33 of the 199 CSB names differ from DM. Examples: 23–24 STORM (DM BOLT BLADE), 25–26 RA BLADE (FURY), 38 SIDE SPLITTER (DELTA), 41 DRAGON FANG (THE INQUISITOR), 62–64 STAFF OF IRRA, CROSS OF NETA, SERPENT STAFF, 174 SHANK (DRUMSTICK).

Action names and texts (items 699 and 700). CSB has no text in these items. The CSB engine has the texts in the `APPB.FTL` data area. Examples: the action names start near `0x1048c0` as one NUL-separated list. `WAKE UP` and `GAME FROZEN` are at `0x104680`. `GAME LOADED, READY TO PLAY.` is at `0x104d70`. A pointer table refers to some texts. `docs/csb/engine-diff.md` "Text" maps the CSB texts to the DM indices of item 700. The CSB engine does not push `0x2bb` (699). DM pushes it at `0x1220b8`.

Layout (item 696). The scratch script `txt.py` compares the zone records with `dmcsb.graphics.decode_lay1`. The range `400–438` becomes `400–436`. All other records are equal. `startend.c` uses zone 437 (`ZONE_RESTART_BUTTON`) and zone 438 (`ZONE_QUIT_BUTTON`) for THE END screen.

### Hard-coded indices with other CSB content

| Port constant | Index | File | CSB effect |
|---|---|---|---|
| `GFX_ENTRANCE_LEFT_DOOR`, `GFX_ENTRANCE_RIGHT_DOOR`, `GFX_ENTRANCE` | 2, 3, 4 | `ids.h` | CSB entrance art. Button zones not checked. |
| `GFX_CREDITS` | 5 | `startend.c` | CSB credits art. The CSB engine does not show credits at the end. |
| `GFX_ACTION_AREA_NAMES` | 10 | `ids.h` | Other content, same size. |
| `GFX_CHAMPION_PORTRAITS` | 26 | `ids.h` | CSB portraits. `savegame.c` matches saved portraits against this sheet (see [Save file](#save-file)). |
| `GFX_OBJECT_ICONS_BEFORE_FIRST` + 1–4, 6 | 42–45, 47 | `ids.h` | CSB icons. The `ICON_*` names of DM items now name CSB items, for example `ICON_BOLT_BLADE`, `ICON_FURY`, `ICON_DELTA`, `ICON_INQUISITOR`. |
| `GFX_OBJECT_NAMES` | 694 | `ids.h`, `ui.c` | TXT1 encoding. |
| `GFX_ACTION_NAMES` | 699 | `ids.h`, `ui.c` | Empty. |
| `GFX_STRINGS` | 700 | `ids.h`, `ui.c`, `champion.c` | Empty. |
| `GFX_LAYOUT` zones 437, 438 | 696 | `startend.c` | Zones are not present. |
| `GFX_CREATURE_FIRST` + aspect | 584–670 | `ids.h`, `view.c` | Other bitmaps and sizes. `view.c` reads the size from the bitmap. |
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

The scratch script `dun.py` loads the file with `kaos_load("game/KAOS.FTL")` and `dungeon_load`. The call returns 1. The thing counts include the new-game extras (groups 87 = 12 + 75). The CSB engine uses the same thing record sizes (`0x10154a`) and the same new-game extra counts (`0x10155a`) as DM, so the DM KAOS tables give the correct result.

`dmcsb` identifies the file by CRC32 as `atarist_csb` (Atari ST CSB 2.x dungeon). The Amiga file is byte-identical to it.

Content (scratch scripts `dun2.py`, `dun4.py`):

- Map 0 has 24 mirrors: wall sensors of type 127 with data 0–23. Each mirror has a text with a champion sheet. The champions are AIRWING, AROC, TALON, LETA, DEMA, ALGOR, TOADROT, VEN, MANTIA, GNATU, SLOGAR, STING, SKELAR, DETH, NECRO, PLAGUE, TUNDA, LANA, BUZZZZZ, PETAL, ITZA, TULA, KAZAI, LOR.
- 6 mirrors have a second sensor: type 1, once only, effect 1, target the door at (1, 14) or (19, 2). The meaning in the CSB rules is unverified.
- Two visible inscriptions say: "THERE IS ONLY ONE LEVEL HERE. FIRST CHOOSE YOUR CHAMPIONS...THEN SAVE THE GAME...THEN RESTART WITH THE UTILITY DISK."
- Map 1 has no texts or sensors. It has 12 creature groups: 7 of type 10, 5 of type 7.

The CSB `load_dungeon` is at `0x1295a0`. It follows the DM loader. One addition: before it builds the column pointers, it compares a value from `0xa02(A5)` with `DAT_00129a42`. When the values differ, it does not build the column pointers. This is probably a copy-protection check (unverified).

Result: the port loader reads the file. The full CSB dungeon is not on disk A. The utility disk makes the adventure from a saved start game. The utility modules `CEDT.FTL` and `HINT.FTL` refer to `DF0:MINI.DAT`, `DF0:DMGAME.DAT`, and `DF0:CSBGAME.DAT`. `MINI.DAT` (42784 bytes) has no known header. Its format is not analysed.

## Save file

The CSB `save_game` is at `0x128cc0` (jump table slot `0xb6a(A5)`). The CSB `load_game` starts at `0x129b3e`. The header writer is `0x128a42`, the header reader is `0x1289c4`, the block cipher is `0x128720`, and the checksum is `0x128752`.

### File names

`DF0:CSBGame.dat` and `DF0:CSBGame.bak` (strings at `0x10013a`, `0x10014a`). The utility modules use `DF0:CSBGAME.DAT` and `DF0:CSBGAME.BAK`.

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

| Field | DM offset | CSB offset | CSB value |
|---|---|---|---|
| Payload key | `0x014` (group 2, `w2`) | `0x03a` (group 7, `w1`) | Random |
| Header byte (`SAVE_HEADER_HEADER_BYTE`) | `0x12a` | `0x12c` | Byte of the last load, default 1 |
| Platform | `0x12b` | `0x12d` | 5. A load fails when the value is 1. |
| Dialog choice | `0x130` | `0x132` | Not read by the load |
| Game ID | `0x132` | `0x134` | `g_game_id` |
| 16 keys | `0x136` | `0x138` | Keys 0–4 encrypt blocks 0–4 |
| 5 checksums | `0x156` | `0x158` | |
| Save format | `0x176` | `0x178` | 3. A load fails on other values. |
| Dungeon ID | `0x178` | `0x17a` | From the dungeon header, default 10 |
| Extra bytes | `0x17a`, 134 bytes | `0x17c`, 132 bytes | Copied unchanged |

Evidence: the writer at `0x128cc0` and the reader at `0x129b3e` use the same offsets. The machine code at `0x12928c` is `moveq #5,d0; move.b d0,$12d(a3); move.b -$421(a5),$12c(a3)`.

### Load differences

- The CSB load checks the checksum of block 2. DM ignores it.
- On a restart (`-0xe2(A5)` set), the CSB load compares the game ID and shows THAT'S NOT THE SAME GAME on a mismatch.
- `save_game` starts with a disk check that reads raw MFM data from the drive (`0x128cc0`, custom chip and CIA access). This is probably copy protection (unverified).
- After a save, the dialog closes. When the player chose save and quit, the save calls the endgame `0x128374(0)`.

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

The next chunk follows the payload directly. `ANIM.FTL` `0x101356` walks the chunks with this step. The scratch script `anim.py` walks all four files to the exact file end. All payload lengths are even.

The chunk dispatcher of `ANIM.FTL` is `0x100d1a`: a switch table at `0x100d30` (18 entries: offset word, then tag word; each offset is relative to its own position; default offset at `0x100d7a`). The loop `0x100fca` runs the chunks from the first chunk to the file end. Step 8 checked each handler in the disassembly (`machine68k`, scratch script):

| Tag | Handler | Meaning | Port (`src/ui/anim.c`) |
|---|---|---|---|
| `PL` | `0x100760` | Payload: count word, then count entries of 4 bytes: color index, red, green, blue (4 bits each). The colors go into a pending palette. The next frame applies it (`0x1016d8`). | Done |
| `EN` | `0x100812` | Full frame: the `Graphics.DAT` packed graphic format (width, height, commands), decoded by the GRF1 function behind slot `0xea` into the back buffer. The parameter is the time in vertical blanks (50 Hz) that the frame stays. | Done |
| `DL` | `0x100846` | Delta frame: the blitter first copies the shown screen into the back buffer (`0x1017bc`), then the same decoder draws the frame. Skip commands keep the old pixels. Pad word `0xff81` before the size. Parameter as `EN`. | Done |
| `SD` | `0x10087a` | Sample: adds the payload to the sample table (max. 32). Payload: length word, then the 8-bit sample data. | Done |
| `SO` | `0x1008f2` | Plays sample number parameter (1-based) at Paula period 650 (`0x101406`). The two bytes after the chunk header give the volumes: (byte + 1) >> 2. In `ENDA.DAT` the `SO` chunks have no payload, so these are the bytes `DL` of the next tag (volumes 17 and 19). | Done. UNVERIFIED: which volume is the left channel. |
| `FO` | `0x100b64` | Loop start: pushes the loop (count parameter − 1, start = the second chunk after `FO`) onto a stack of 16 loops. The chunk after `FO` runs once. | Done |
| `NE` | `0x100be2` | End of a pass: decrements the count. When the count was 0, it pops the loop and skips the two chunks after `NE`. | Done |
| `BN` | `0x100c7e` | Goes back to the loop start. | Done |
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

1. A wall sensor of type 18 (endgame, C`0x123970`) sets the game-won flag (`-0x4f9c(A5)`) and calls `0x1285e8` through slot `0xb42(A5)`. There is no delay. DM waits the sensor delay, clears the saved-game flag, and calls `endgame(1)`.
2. `0x1285e8` branches to `0x1285ee` (`quit_game_to_ending`). It waits until the pointer is hidden, closes `Graphics.DAT`, stops in an endless loop while the won flag is 0 (C`0x128600`), fills zones 401, 13, 11, and 9 with color 0, writes `'ENDA'` into the next-module field of the shared FTL block (`*(-4(A5)) + 4`, C`0x12863a`), and leaves `APPB` (`system_shutdown`, `rt_longjmp`).
3. The launcher `APPA.FTL` (`APPA.c` line 355) reads the field. For `ANIM`, `TITL`, or `ENDA`, it starts `ANIM.FTL` with that tag.
4. `ANIM.FTL` `0x100e96` builds the file name from the tag and `.DAT` (`0x2e444154`) and plays `ENDA.DAT`. After `ENDA`, it loops forever with a delay (`ANIM.c` line 579). The last frame stays on the screen. The player must reset the computer.

The endgame `0x128374` (party dead) goes to `0x1285e8` when there is no saved game. The won flag is 0 then, so the engine stops at C`0x128600` on the credits screen.

Port: `startend_run_endgame_sensor` in `src/ui/startend.c` sets `g_session.won`, plays `ENDA.DAT` from the disk A folder with `anim_play`, waits for a key or a click, and shows the CSB screen.

### TITL.DAT and ANIM.DAT on disk B

- `TITL.DAT` (disk B): the title animation of the utility disk. 1 `AN`, 1 `PL`, 1 `EN`, 31 `DL`, 1 `DO`.
- `ANIM.DAT` (disk B): the long intro animation. It loads 5 samples from `Sound/` by name and music from `MIDI/NAKED.AMG`. The 5 files in `Sound/` equal CSB graphic items 688, 675, 683, 684, and 685.

The port plays `ANIM.DAT` after NEW ADVENTURE (`play_csb_intro` in `src/platform/app.c`). It does not read `TITL.DAT` of disk B.

`ANIM.FTL` of disk B is another build than disk A (all addresses below are disk B addresses). Dispatcher `0x100c76`, table at `0x100c8e`. Handlers:

| Tag | Handler | Meaning |
|---|---|---|
| `SF` | `0x100920` | Payload: a file name (`SOUND/TELE2.amg`). The file is a sample: length word, signed 8-bit data. It goes into the `SD` sample table; a file that cannot be read keeps its number. AmigaDOS ignores the case of the name; the files on disk are `Sound/tele2.amg`. |
| `MF` | `0x100a1a` | Payload: a music file name (`MIDI/NAKED.amg`), into the music table. `MD` (`0x100866`) puts its payload into the same table. |
| `MI` | `0x1008f6` | Plays music number parameter (1-based) at once (`0x101424`). |
| `SO` | `0x1008a2` | As disk A (period 650, volumes `(byte + 1) >> 2`). The word after the two volume bytes (`0x1388`) is not read. While the music plays, a sample goes to pair A (channels 3 and 2), and a new sample stops the sample before. |
| `DO` | `0x100c4c` | Ends the samples after their cycle. It does not stop the music. |

`ANIM.DAT` has 851 frames, 10808 vertical blanks (216 s). After the last frame or a stop (`BR` 1), the cleanup fades the music: volume 29 down to 1, one step every 3 ticks (`0x102e4c`), then the music stops.

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

- The CSB sound table at `0x10122a` has 35 entries of 12 bytes. It is byte-identical to the DM table at `0x100d52` (scratch script output: all 35 entries equal). The graphic indices are 671–712.
- The CSB init `0x1091ce` reads each sample graphic, sets the length to the graphic size − 2, and keeps the data after the first word. The play routine `0x109258` uses the length and the period `72800 / rate` (`0x11c60`).
- In 26 sample items, the first word differs from DM by 1 or 2. The sample data is identical. The CSB engine does not read this word. `sound.c` does not read it.

The CSB engine has no new sound in `Graphics.DAT`. New samples exist only in the animation files (`SD` chunks, `Sound/*.amg`) and in `MIDI/NAKED.AMG`.

## Engine tables

The port reads all rule and view tables from the `KAOS.FTL` image. CSB has no `KAOS.FTL`. The scratch scripts searched `APPB.FTL` for the DM table bytes:

| Table | DM address | CSB address | Content |
|---|---|---|---|
| Sounds | `0x100d52` | `0x10122a` | Identical |
| Creature info | `0x10192e` | `0x101e32` | Identical |
| Object info | `0x101068` | `0x10156a` | Identical |
| Weapon info | `0x1014a0` | `0x1019a2` | Identical |
| Armour info | `0x1015b4` | `0x101ab6` | Identical |
| Food values | `0x1016d2` | `0x101bd6` | Identical |
| Spells | `0x103c84` | `0x1046c2` | Identical |
| Object aspects | `0x102c68` | `0x1037fa` | Identical |
| Creature aspects | `0x102e58` | `0x1039e8` | Identical |
| Thing record sizes | `0x101048` | `0x10154a` | Identical |
| New-game extras | `0x101058` | `0x10155a` | Identical |
| Action sets | `0x103d4c` | Not found as one block | Not analysed |

The other tables of `tables.md`, the view tables, the input tables, and the escape tables of `dungeon_decode_text` are not checked.

## Loader changes

| File | Function | Change | Reason |
|---|---|---|---|
| `src/ui/ui.c` | `load_strings`, `ui_init` | Read item 694 in TXT1 encoding for CSB. | CSB object names end with bit 7, not NUL. |
| `src/ui/ui.c`, `src/engine/champion.c` | `ui_init`, the `GFX_STRINGS` reader | Read the texts and action names from `APPB.FTL` for CSB, with an index map to the DM text indices. | Items 699 and 700 are empty in CSB. |
| `src/ui/startend.c` | THE END screen | Done (Step 8): CSB draws RESTART THIS GAME in the two boxes of APPB `0x100dcc` (image address `0x104248`) and keeps the click box of the CSB endgame mouse table. | The CSB layout has no zones 437 and 438. |
| `src/engine/savefile.c` | `savefile_decode`, `savefile_encode`, `savefile_init_header`, header accessors | Add a CSB header variant: key at `0x3a`, fields 2 bytes up, 132 extra bytes. Check the block 2 checksum. | A CSB save fails the DM header check. |
| `src/engine/savegame.c` | `savegame_save`, `savegame_load`, portrait functions | Use `CSBGAME.DAT` and `CSBGAME.BAK`. Keep the saved portrait bitmaps. | CSB parties come from DM saves or the champion editor. Their portraits are not in graphic 26. |
| `src/data/kaos.c` and every `kaos_*` user (`tables.c`, `sound.c`, `view.c`, `dungeon.c`, and others) | `kaos_load`, address constants | Load `APPB.FTL` and translate the addresses. | CSB has no `KAOS.FTL`. Most tables are identical at other addresses. |
| New `src/ui/anim.c` | Animation player | Done (Step 8): plays the chunks of `ENDA.DAT`. | The CSB endgame plays `ENDA.DAT`. |

No change is needed in `layout.c` or `dungeon.c` (`dungeon_load`). Step 8 added `gfx_expand_over` (`graphics.c`, a decode over an existing bitmap for the delta frames) and `audio_play_data` (`audio.c`, plays sample data that is not in `Graphics.DAT`).

## Scratch scripts

The scripts are in the session scratchpad, folder `data/`. They are not part of the repository.

| Script | Purpose |
|---|---|
| `hdr.py`, `align.py` | Read both headers and align the items |
| `snd.py` | Compare the sample items and the `Sound/*.amg` files |
| `port_gfx.py`, `pix.py` | Decode all items through `libdm.dylib` and compare with `dmcsb` |
| `txt.py` | Compare object names and layout zones |
| `dun.py`–`dun4.py` | Load `Dungeon.DAT` with the port and with `dmcsb` |
| `anim.py`, `anim2.py` | Walk the animation chunks |

## Open points

- The order of the CSB texts in `APPB.FTL` against the DM text indices.
- The meaning of the `AN` count (the player skips `AN`). The music chunks (`MI`, `MD`, `MF`) and `SF` of `ANIM.DAT`.
- The format of `MINI.DAT` and the way the utility disk builds the CSB adventure.
- The CSB entrance buttons (graphic 11 and the entrance mouse zones).
- The CSB rule differences for renamed objects (for example STORM, RA BLADE).
- A test with a real CSB save file.
