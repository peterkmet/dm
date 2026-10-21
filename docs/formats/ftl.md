# FTL module format

Dungeon Master v3.60 for the Amiga keeps its code in `.FTL` modules. The AmigaDOS executable `BJELoad_R` loads them. This page describes the format. The source is the decompiled `BJELoad_R` (function at code offset `0x34c`) and `APPA.FTL` (unpacker at code offset `0x22a`). `tools/dmtools/ftl.py` implements this format. All 10 modules on the v3.60 disk pass every checksum.

All values are big-endian.

## Disk contents

| File | Content |
|---|---|
| `BJELoad_R` | AmigaDOS hunk executable. Loads FTL modules and gives them a service table. Starts `APPA`. |
| `APPA.FTL` | Launcher. Runs `SWSH` (the FTL logo with a sound; the port reads it in `src/data/swsh.c`), then `ANIM` with `TITL.DAT` (the title animation), then `APPB` (the language menu), then `KAOS` with language 0, 1, or 2 (English, French, German). Each module writes the tag of the next module into the shared block. Contains the R# unpacker. |
| `KAOS.FTL` | Main game. Code is R# packed. |
| `ANIM.FTL`, `APPB.FTL`, `CNFG.FTL`, `GRF1.FTL`, `MEM1.FTL`, `SWSH.FTL`, `USIO.FTL`, `VDEO.FTL` | Other modules. `CNFG.FTL` code is R# packed. |
| `Graphics.DAT`, `Dungeon.DAT`, `DungeonF.DAT`, `DungeonG.DAT`, `TITL.DAT` | Game data. |

`s/Startup-sequence` runs `BJELoad_R`. The files `dungeonrel.txt`, `mouse`, and `Type` come from the group that made this disk image. They are not part of the game.

## File header

| Offset | Size | Field |
|---|---|---|
| `0x00` | 2 | Magic `0x6160` |
| `0x02` | 2 | Header checksum |
| `0x04` | 2 | Version, must be `2` |
| `0x06` | 1 | Must be `1` |
| `0x07` | 1 | Must be `0` |
| `0x08` | 10 | Not used by the loader |
| `0x12` | 2 | Section count, less than `0x65` |
| `0x14` | 12 × count | Section table |

Header checksum: the 16-bit sum of `byte[i] * i` for `i` from 4 to `0x13`, plus `table[i] * ((i & 0xff) + 1)` for every byte of the section table.

Each section table entry:

| Offset | Size | Field |
|---|---|---|
| `0x00` | 2 | Tag |
| `0x02` | 2 | Kind, must be `1` |
| `0x04` | 4 | File offset |
| `0x08` | 4 | Size |

A module has exactly one section of each tag:

| Tag | Section |
|---|---|
| `0x10` | Info |
| `0x11` | Data |
| `0x12` | Code |

## Info section

| Offset | Size | Field |
|---|---|---|
| `0x00` | 4 | Region size: bytes above the data base (jump table and uninitialized data) |
| `0x04` | 4 | Data size: bytes of initialized data below the data base |
| `0x08` | 4 | Jump table size in bytes |
| `0x0c` | 4 | Jump table offset from the data base |
| `0x10` | 4 | Stack size |
| `0x14` | 4 | Code size after unpacking |
| `0x18` | 4 | Data section size |
| `0x1c` | 2 | Always `1` on the v3.60 disk |
| `0x1e` | 2 | Code packed flag: `1` means R# packed |
| `0x20` | 2 | Not used by the loader |
| `0x22` | 2 | Info checksum |
| `0x24` | 2 | Code checksum: 16-bit byte sum of the unpacked code |
| `0x26` | 2 | Data checksum: 16-bit byte sum of the data section |
| `0x28` | 4 × n | Code offset of each exported function |

Info checksum: the 16-bit word sum of the whole info section, minus the checksum word, equals the checksum word.

## Data section

| Offset | Size | Field |
|---|---|---|
| `0x00` | 2 | Not used by the loader |
| `0x02` | 4 | RLE data length in bytes |
| `0x06` | 4 | Relocation list length in bytes |
| `0x0a` | | RLE data, then the relocation list |

RLE data is a list of 16-bit words. The loader copies each word. A word `0x0000` is followed by a count word. The loader then writes that number of extra zero bytes.

The relocation list is a list of signed 16-bit offsets from the data base. The loader adds the data base address to the 32-bit value at each offset.

## Code section

When the packed flag is `0`, the section is the code. When the flag is `1`, the section is R# packed:

| Offset | Size | Field |
|---|---|---|
| `0x00` | 2 | Magic `0x5223` (`R#`) |
| `0x02` | 2 | Not used by the unpacker |
| `0x04` | 4 | Output length in 16-bit words |
| `0x08` | `0xf00` | Table of `0x780` 16-bit words |
| `0xf08` | | Nibble stream, high nibble first |

For each output word, read one nibble `v`:

| `v` | Next nibbles | Output word |
|---|---|---|
| `0`–`7` | 1 nibble `a` | `table[(v << 4) \| a]` |
| `8`–`14` | 2 nibbles `ab` | `table[((v << 8) \| ab) - 0x780]` |
| `15` | 4 nibbles `abcd` | `abcd` (literal) |

`BJELoad_R` does not unpack code itself. Its service at offset `-0x64` only reports an error. `APPA.FTL` supplies the unpacker.

## Memory layout

The loader allocates one block. All three parts are word aligned.

```text
base                         initialized data (data size), from the RLE data
data base = base + data size region (region size), jump table at jump table offset
code base = data base + region size   code (code size)
```

The loader writes one 8-byte entry for each exported function at `data base + jump table offset + 8 × i`. Bytes 2–7 of the entry are `JMP abs.l` (`0x4ef9`) to `code base + export offset`. The loader does not relocate code. Code reaches its globals through register `A5`, which holds the data base.
