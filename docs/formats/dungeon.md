# Dungeon.DAT format

This page describes the `Dungeon.DAT` format as `src/engine/dungeon.c` implements it. The port follows `load_dungeon` (`0x1260ee`) in `KAOS.FTL`. The language variants `DungeonF.DAT` and `DungeonG.DAT` use the same format.

All words and longs are big-endian. "Word" means 16 bits.

## Load a dungeon

```c
#include "dungeon.h"

if (!dungeon_load("game/Dungeon.DAT"))
    return 1;
```

`dungeon_load` loads the dungeon for a new game. It returns 1 on success and 0 on error. Call `tables_load("game/KAOS.FTL")` (`tables.h`) first: the loader and `dungeon_decode_text` read the new-game extras table (`0x101058`) and the escape tables (`0x1016ea`, `0x1017ea`, `0x10182a`) from the KAOS image. Without KAOS, `dungeon_load` returns 0. A new call frees the previous dungeon.

## Compressed file

A compressed file starts with an 8-byte header:

| Offset | Size | Content |
|---|---|---|
| 0 | word | Signature `0x8104`. |
| 2 | long | Decompressed size in bytes. |
| 6 | word | Dungeon ID (`g_dungeon_id`). The DM dungeon has ID 10. |

When the first word is not `0x8104`, the file is not compressed. The loader then reads the file from offset 0.

`dungeon_decompress(src, src_size, dst, byte_count)` ports `0x12712c`. `src` is the data after the 8-byte header:

- Bytes 0–3 are table `t4`. Bytes 4–19 are table `t16`.
- The bit stream starts at byte 20. The decoder reads it in big-endian words, most significant bit first.
- `0` + 2 bits `i`: output `t4[i]`.
- `10` + 4 bits `i`: output `t16[i]`.
- `11` + 8 bits: output the literal byte.
- The decoder stops after `byte_count` output bytes.

The original has no `src_size` argument. The port adds it. The function returns 0 when the bit stream ends before `byte_count` bytes. It never reads past the end of the input.

All three files on the disk are compressed.

## Dungeon data

The decompressed data has these parts in this order:

| Part | Size in bytes | Global |
|---|---|---|
| File header | 44 | `g_dungeon_header` |
| Map descriptors | map count × 16 | `g_dungeon_maps` |
| Column first-thing indices | column count × 2 | `g_dungeon_column_first_thing_index` |
| Square first things | square first thing count × 2 | `g_square_first_things` |
| Text data | text word count × 2 | `g_dungeon_text_data` |
| Thing data, types 0–15 | count × record size, for each type | `g_thing_data[type]` |
| Raw map data | raw map data size | `g_dungeon_raw_map_data` |
| Checksum | 2, optional | — |

The checksum is the 16-bit sum of all bytes of the parts before it. The loader accepts the dungeon when the checksum is missing or correct. `Dungeon.DAT` has a checksum. `DungeonF.DAT` and `DungeonG.DAT` have none.

All buffers keep the big-endian byte layout of the original memory. The game code reads a word field with two bytes and a byte field at its original offset.

### File header

| Offset | Size | Content |
|---|---|---|
| 0 | word | Seed for random ornaments. |
| 2 | word | Raw map data size in bytes. |
| 4 | byte | Map count. |
| 6 | word | Text data word count. |
| 8 | word | Party start: X bits 0–4, Y bits 5–9, direction bits 10–11. |
| 10 | word | Square first thing count. |
| 12 | 16 words | Thing count of each thing type. |

The loader sets `g_party_map_x`, `g_party_map_y`, and `g_party_direction` from offset 8. It sets `g_party_map_index` to 0.

### Map descriptor

| Offset | Size | Content |
|---|---|---|
| 0 | word | Raw map data offset of the map. |
| 6 | byte | X offset of the map in the dungeon. |
| 7 | byte | Y offset of the map. |
| 8 | word | Bits 0–5: level. Bits 6–10: width − 1. Bits 11–15: height − 1. |
| 10–15 | bytes | Ornament, creature, and door counts and sets. See [range-4-6.md](../reference/symbols/range-4-6.md#map-descriptor). |

### Squares

The raw map data of a map starts at the map offset. The squares are in column order: column `x` starts at `offset + x × height`. Each square is one byte. Bits 5–7 hold the square type. Bit 4 is set when the square has a thing list.

The loader builds `g_dungeon_map_data`. `g_dungeon_map_data[map][x]` points to column `x` of the map. `g_dungeon_map_first_column[map]` is the index of the first column of the map in the column table. `g_dungeon_column_count` is the column count of all maps.

### Thing lists

Each word of the column table is the index of the first square-first-thing entry of that column. To find the first thing of square (x, y), start at the column entry and add 1 for each square above `y` that has bit 4 set. `dungeon_get_square_first_thing(map, x, y)` does this. It returns `0xfffe` when the square has no things.

A thing is a word: bits 0–9 index, bits 10–13 type, bits 14–15 cell. Each record starts with the word of the next thing. `0xfffe` ends a list. `dungeon_get_thing_data(thing)` returns the record.

Record sizes (`g_thing_data_byte_count`):

| Type | Name | Bytes |
|---|---|---|
| 0 | Door | 4 |
| 1 | Teleporter | 6 |
| 2 | Text string | 4 |
| 3 | Sensor | 8 |
| 4 | Group | 16 |
| 5 | Weapon | 4 |
| 6 | Armour | 4 |
| 7 | Scroll | 4 |
| 8 | Potion | 4 |
| 9 | Container | 8 |
| 10 | Junk | 4 |
| 11–13 | Unused | 0 |
| 14 | Projectile | 8 |
| 15 | Explosion | 4 |

### New-game changes

For a new game the loader changes the header in memory:

- It adds 300 to the square first thing count. The new entries are `0xffff`.
- It adds extra records to each thing count. The byte table at KAOS `0x101058` (`g_new_game_extra_thing_count`) gives the extra record count of each type. The loader reads it from the KAOS image. The count is limited to 1024 (768 for explosions). The first word of each new record is `0xffff` (free record).
- It sets `g_timeline.capacity` to 100 plus the counts of types 4, 14, and 15.

The original writes all extra records, also past a limited count. The port stops at the limit. The DM dungeons never reach the limit.

## Validation

`dungeon_load` returns 0 and leaves no dungeon in memory (all globals `NULL`, `dungeon_map_count()` returns 0) in these cases:

- KAOS is not loaded, or the file cannot be read.
- The file is shorter than 8 bytes, or the decompressed size is 0 or larger than 1 MB.
- A compressed file ends before the decompressed size is reached.
- A part is longer than the remaining data.
- The map count is 0.
- The square first thing count is larger than `0xffff − 300`.
- A thing count is larger than the limit (1024, 768 for explosions).
- A map does not fit in the raw map data (offset + width × height > raw map data size).
- The checksum is present and wrong.
- A memory allocation fails.

`dungeon_get_square_first_thing` returns `0xfffe` when the index from the column table is outside the square first things.

## Text decoding

`dungeon_decode_text(dst, dst_size, text_thing, mode)` ports `0x10b312`. The port adds `dst_size`. It returns 1 on success. It returns 0 when the text offset is not in the text data, when the text has no end code, or when the text does not fit in `dst_size` bytes. It then cuts the text and still writes the end byte. The text string record has a word at offset 2: bit 0 is the visible flag, bits 3–15 are the word offset into the text data.

The function decodes only a visible text, or any text when bit 15 of `mode` is set. Modes:

| Mode | Text | Start | Code 28 | End byte |
|---|---|---|---|---|
| 0 | Wall inscription | — | `0x80` | `0x81` |
| 1 | Message | newline | space | NUL |
| 2 | Scroll | — | newline | NUL |

Each text word holds three 5-bit codes: bits 10–14, 5–9, 0–4.

- Codes 0–25: letters `A`–`Z`. Code 26: space. Code 27: `.`. An inscription keeps the raw code 0–27, because the wall font uses these codes.
- Code 28: the separator from the table above.
- Code 29: the next code is an index into the replacement character table at KAOS `0x1017ea` (32 entries of 2 bytes).
- Code 30: the next code is an index into an escape string table (32 entries of 8 bytes). Messages and scrolls use the table at KAOS `0x1016ea`. Inscriptions use the table at `0x10182a`, which holds the same strings as wall font codes. Indices 2 and 3 are common words. Most other entries are single glyph codes or empty.
- Code 31: end of the text.

The function reads all three tables from the KAOS image. The single characters from code 30 are glyph codes of the game font, not ASCII text.

## Differences from dmcsb

`dmcsb` 0.2.1 agrees on the map count, map sizes and offsets, all square bytes, thing counts, and thing lists. It differs on escape code 30: it maps the single-glyph entries to display characters (`?`, `'`) or to an empty string. The original code emits the glyph codes from the KAOS table. `tests/differential/test_dungeon.py` compares `dungeon_decode_text` with the original function in `dmtools.emu` for every text and every mode.

## Tests

`tests/differential/test_dungeon.py` needs the files in `game/` and skips when they are missing.

- Map count, map descriptors, all square bytes, first things, thing counts, and next-thing words against `dmcsb`.
- Decoded texts against `dmcsb`, without the texts that use escape 30 with an index other than 2 or 3.
- `dungeon_decode_text` against the original `0x10b312` in the emulator.
- `dungeon_decompress` against the original `0x12712c` in the emulator, on all of `Dungeon.DAT`.
- An uncompressed copy of `Dungeon.DAT` loads, and a wrong checksum fails.
- Truncated files, a thing count above the limit, a bad map offset, a bad raw map data size, and a map count of 0 fail. A bad text offset and a small output buffer make `dungeon_decode_text` return 0.

## Not ported

- Loading a dungeon from a saved game (no new-game extras) and the restart path that reuses buffers.
- `timeline_init` and `group_init_active_groups` at the end of `load_dungeon`.
- The current-map functions (`get_square`, `get_square_first_thing` on `g_current_map_data`). The port uses functions with a map argument for now.
