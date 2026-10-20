# Chaos Strikes Back: new adventure

This page describes how the original Chaos Strikes Back (CSB) for the Amiga starts a new game. It covers the menu flow, the format of `MINI.DAT`, the merge that makes `CSBGAME.DAT`, and the champion selection in the prison.

The sources are the decompiled modules in `reference/decomp/csb-a/` (disk A) and `reference/decomp/csb-b/` (disk B) (base `0x100000`). All addresses are code addresses in these images. "CEDT" means `reference/decomp/csb-b/CEDT.bin` (disk B). "APPB A" means `reference/decomp/csb-a/APPB.bin` (disk A). Ghidra drops the arguments of calls through the `A5` jump table, so the claims come from the disassembly, not from the `.c` files.

## Summary

1. The player starts disk A. A new game loads `Dungeon.dat`: the prison, dungeon ID 12. The player picks champions at the mirrors.
2. The player saves the game. The save is `CSBGame.dat` with dungeon ID 12.
3. The player starts disk B (utility disk), opens the champion editor (`CEDT.FTL`), loads the saved game, and clicks MAKE NEW ADVENTURE.
4. CEDT loads `MINI.DAT` from the utility disk. `MINI.DAT` is a complete CSB saved game (dungeon ID 13) with one placeholder champion.
5. CEDT copies the champions of the saved game into the `MINI.DAT` state, resets them, and writes `CSBGAME.DAT` to the save disk.
6. The player starts disk A again and resumes. The engine loads `CSBGame.dat` and plays the CSB dungeon.

The dungeon text in the prison says the same: "FIRST CHOOSE YOUR CHAMPIONS...THEN SAVE THE GAME...THEN RESTART WITH THE UTILITY DISK." (text strings 24 and 25 of disk A `Dungeon.DAT`, read with `dmcsb`).

`MINI.DAT` is not encrypted with a special scheme and is not compressed. It uses the cipher of the DM save file ([save-format.md](../formats/save.md)) with a header that is moved by 2 bytes. CEDT does not change or randomize the dungeon.

## Modules

| Disk | Module | Role |
|---|---|---|
| A | `APPB.FTL` | The CSB engine. New game: `Dungeon.dat`. Resume: `CSBGame.dat`, then `CSBGame.bak`. |
| B | `APPB.FTL` | Launcher. Holds the module names `ANIM`, `CEDT`, `HINT` (`0x101989`). Its menu logic is not traced. |
| B | `CEDT.FTL` | Champion editor. LOAD CHAMPIONS, SAVE CHAMPIONS, MAKE NEW ADVENTURE, REVERT, UNDO, QUIT. |
| B | `HINT.FTL` | Hint oracle. It names `CSBGAME.DAT` and `MINI.DAT`. Not traced. |

## Disk A: new game and resume

`load_game` of APPB A is `0x129b3e`.

- The entrance command `0xc8` sets the new-game flag (`A5-0x4fa4`). Command `0xca` clears it (resume). See `0x125104`–`0x125130`.
- New game (`0x129bc2`): open `DungeonMaster:Dungeon.dat` (`A5-0x144`). When the flag `A5-0xde` is set (command `0xc9`), open `DungeonMaster:DungeonB.dat` (`A5-0x160`) first. `DungeonB.dat` is not on the disk.
- The dungeon loader `0x1295a0` reads the 8-byte compressed header. It copies the dungeon ID word (file offset 6) to `A5-0x42c`. The prison `Dungeon.DAT` has ID 12 (`0x000c`).
- Resume (`0x129d42`): open `DF0:CSBGame.dat` (`A5-0x5022`), then `DF0:CSBGame.bak` (`A5-0x501e`).
- The loader reads the header with the CSB layout: keys at `+0x138`, checksums at `+0x158`, format at `+0x178`, dungeon ID at `+0x17a`. It rejects platform 1 and a format other than 3 (`0x129e4e`–`0x129e62`). It does not check the dungeon ID.
- The save writer stores the header byte, platform 5, format, and dungeon ID at `0x12928c`–`0x12929e`. Thus a prison save has dungeon ID 12.

Not traced: what command `0xc9` is bound to, and the `Dungeon.FTL` fallback at `0x129d74` (resume without a saved game).

## Disk B: MAKE NEW ADVENTURE

The menu handler is at `0x107d0a`. CEDT keeps two slots of `0x3a6` bytes at `*(A5-0x4030)`. Slot 0 is the loaded saved game. Slot 1 is the new adventure.

1. `0x105ba2` checks slot 0. The game must be loaded (`slot+0x20`), the champion count (`A5-0x4032`) must not be 0, and `0x10a31c(slot, 3)` must accept the game: format 3 and dungeon ID 10 (Dungeon Master) or 12 (prison). Otherwise CEDT shows "THE NEW ADVENTURE CAN ONLY BE MADE AFTER A SAVED GAME IS LOADED." or "CAN'T MAKE NEW ADVENTURE FROM THIS SAVED GAME.".
2. `0x1053ec` checks the champion names. Each first name must not be empty and must be unique.
3. When slot 1 is empty, `0x108030(1)` asks for the utility disk (`0x1053aa`) and loads `DF0:MINI.DAT` (`A5-0x59c`) into slot 1. It stores `DF0:CSBGAME.DAT` as the output name of the slot.
4. `0x105c7e` merges slot 0 into slot 1. See [Merge](#merge).
5. `0x1085da(1)` asks for a save disk (`0x105462`: MAKE NEW ADVENTURE, FORMAT, CANCEL) and writes slot 1 as `DF0:CSBGAME.DAT`.

LOAD CHAMPIONS (`0x108030(0)`) tries `DF0:DMGAME.DAT`, `DF0:DMGAME.BAK`, `DF0:CSBGAME.DAT`, and `DF0:CSBGAME.BAK`. When a DM and a CSB game are both on the disk, CEDT asks "LOAD WHICH SAVED GAME?". Thus a party from Dungeon Master can also start CSB.

## MINI.DAT format

`MINI.DAT` (42784 bytes) has the layout of a saved game. All values are big-endian.

| Offset | Size | Part |
|---|---|---|
| `0x0000` | 512 | Header |
| `0x0200` | 128 | Block 0: globals |
| `0x0280` | 960 | Block 1: active groups, 60 × 16 |
| `0x0640` | 1408 | Block 2: 4 champions × `0x140`, then the party (128) |
| `0x0bc0` | 4360 | Block 3: timeline events, 436 × 10 |
| `0x1cc8` | 872 | Block 4: timeline index, 436 × 2 |
| `0x2030` | 1856 | 4 portraits × 464, not encrypted |
| `0x2770` | 32686 | Dungeon parts, not encrypted, not compressed |
| `0xa71e` | 2 | 16-bit byte sum of the dungeon parts |

### Header

The header has the same structure as the DM header: 32 groups of 4 noise words, then a payload of 128 words at `0x100`. The differences:

- The payload key is word 29 (offset `0x3a`). DM uses word 10 (offset `0x14`).
- The payload fields from `0x12a` on are 2 bytes later than in DM.

| Field | DM offset | CSB offset | Value in MINI.DAT |
|---|---|---|---|
| Header byte | `0x12a` | `0x12c` | 1 |
| Platform | `0x12b` | `0x12d` | 5 |
| Dialog choice | `0x130` | `0x132` | 1 |
| Game ID | `0x132` | `0x134` | `0x2f633134` |
| 16 block keys | `0x136` | `0x138` | |
| Block checksums | `0x156` | `0x158` | |
| Save format | `0x176` | `0x178` | 3 |
| Dungeon ID | `0x178` | `0x17a` | 13 |
| Extra bytes | `0x17a` | `0x17c` | |

`0x108030` reads the header (`0x108d78`) and tries `0x108da2(header, 29)`. When the check fails, it restores the header and tries `0x108da2(header, 10)`. The result sets the layout in `slot+0x22`: 2 is CSB, 1 is DM. `0x10a1a2` then reads the format, the dungeon ID, and the platform. For format 3 with platform 4 or more, block 2 has `0x580` bytes and the file has 4 portraits of `0x1d0` bytes. Older platforms use a block 2 of `0xd00` bytes and no portraits.

### Decode algorithm

1. Header check (`0x108da2`): compute the chain over the 32 groups: `acc = w3 ^ ((w1 ^ (w0 + acc)) - w2)`. Decrypt the 128 payload words with the cipher below and the key word. The chain must equal the 16-bit sum of the plain payload words.
2. Blocks (`0x108c62`): read each block, decrypt it with `0x108bfc(buf, key[i], words)`, and compare the returned sum with `checksum[i]`. Block sizes come from block 0: offset 46 is the maximum active group count, offset 28 is the event maximum count.
3. Cipher (`0x108bfc`), the same as DM `savefile_xor_block`:

   ```c
   sum = key;
   for (i = count; i != 0; i--) {
       old = *w;
       *w ^= key;
       sum += *w + old;
       key += i;
       w++;
   }
   ```

4. Portraits: read 4 × 464 bytes without a change.
5. Dungeon (`0x108efe`): read the parts in the order of `Dungeon.DAT` ([dungeon-format.md](../formats/dungeon.md)). The thing record sizes are the table at `0x100568`. Add every byte to a 16-bit sum. Then read 2 more bytes. When the read gives 2 bytes, they must equal the sum.

`MINI.DAT` passes all checks. All five block checksums and the dungeon sum (`0x7cc6`) match.

### Content

| Value | MINI.DAT |
|---|---|
| Game time | 469 |
| Random seed | `0xa5349232` |
| Party | map 4, x 22, y 18, direction 2 |
| Champions | 1: HALK, GONZO BARBARIAN |
| Leader, magic caster | 0, 0 |
| Leader hand object | `0xffff` (empty) |
| Event maximum count | 436 |
| Active groups | 8 of 60 |
| Dungeon | 11 maps (10 levels and one 1 × 1 map), 48 text strings |

## Merge

`0x105c7e` builds the new adventure in slot 1. "Game" is slot 0. "Mini" is slot 1.

### Header

- Copy game header bytes `0x100`–`0x11f` and `0x120`–`0x127` to mini.
- Mini game ID = game game ID + 1.
- Mini format = game format.
- Mini header byte = 2 (`HEADER_BYTE_NEW_ADVENTURE`). The disk A engine keeps this byte and writes it back on save.

### Champions

- `0x109e4c` copies the 4 champion records (`0x500` bytes) and the 4 portraits of the game to mini. The party record (`0x80`), block 0, the timeline, the active groups, and the dungeon stay from `MINI.DAT`.
- Mini champion count (block 0, offset 10) = game champion count.
- The load step `0x109c20` clears the name bytes after the first NUL (8 bytes) and the title bytes after the first NUL (20 bytes).

Then for each champion (record offsets of [game-state.md](../rules/game-state.md)):

| Field | New value |
|---|---|
| `cell`, `direction` | Turned by `(mini party direction − game party direction)`, when that difference is not 0 |
| `action_index` | 255 |
| `symbol_step`, `symbols[0]` | 0 |
| `direction_maximum_damage_received`, `maximum_damage_received`, `poison_event_count` | 0 |
| `enable_action_event_index`, `hide_damage_received_event_index` | −1 |
| `attributes` | `attributes & 0x10` |
| `wounds`, `action_defense`, `load`, `shield_defense` | 0 |
| Current health, stamina, mana | The maximum value |
| `food`, `water` | `1500 + (random & 0xff)` each |
| Each statistic | Current = maximum. Minimum = 30. Luck minimum = 10. |
| `temporary_experience` of each skill | 0 |
| All 30 slots | `0xffff`: the champion has no objects |

Before a slot is cleared, `0x104cca` removes the stat bonus of the object from the champion. This call changes the record in slot 0, not the copy in slot 1. Thus a DM champion keeps an object bonus in the new adventure. The prison has no objects, so a prison party is not affected. This is an observation from the code. The test below does not cover an object with a bonus.

At the end, `0x1062b8` gives a champion a new cell when another champion has the same cell. The new cell is the lowest cell that no champion uses (`0x105bf0`).

### Random numbers

`0x1064be`: `seed = seed * 0x4cb2f + 11`, result `seed >> 8`. The first seed comes from `A5-0x3e30 + 0xf9c25`. The merge uses it for food and water. The save uses it for the keys and the header noise.

## CSBGAME.DAT

`0x1085da` writes slot 1. The file has the `MINI.DAT` layout.

1. `0x10a0f2` clears the portrait pointer (champion offset `0x114`) of the 4 records.
2. Make 16 new random keys (`0x1087aa`).
3. Compute the 5 block checksums without a change to the data (`0x108c2e`).
4. Write the header (`0x108e16`): fill the noise words with random values, set `w3` of the last group so that the chain gives the payload sum, encrypt the payload with word 29.
5. Write each block encrypted (`0x108c9e`), then the portraits, then the dungeon parts and their byte sum (`0x108bce`).

Values in the new `CSBGAME.DAT`:

| Value | Source |
|---|---|
| Game time, random seed | `MINI.DAT` (469, `0xa5349232`) |
| Party position | `MINI.DAT`: map 4, x 22, y 18, direction 2 |
| Leader, magic caster, leader hand | `MINI.DAT`: 0, 0, empty |
| Champion count, champion records, portraits | Saved game, reset as in [Merge](#merge) |
| Timeline, active groups, dungeon | `MINI.DAT`, unchanged |
| Game ID | Saved game ID + 1 |
| Dungeon ID | 13 (`MINI.DAT`) |

## Prototype

The prototype is the module `dmtools.csb_mini`, and its differential test is a helper of the tests.

| File | Content |
|---|---|
| `tools/dmtools/csb_mini.py` | `decode_save`, `dungeon_dat`, `can_make_adventure`, `merge`, `encode_save`. |
| `tests/dmtest/cedt.py` | Harness that runs the original CEDT routines in `dmtools.emu`. |
| `tests/differential/test_csb_mini.py` | Differential test against the original CEDT routines. |

Write the CSB dungeon as an uncompressed `Dungeon.DAT`, and optionally build a new adventure. Run the commands from the repository root:

```sh
uv run python -m dmtools.csb_mini game-csb/disk-b/MINI.DAT csb_dungeon.dat
uv run python -m dmtools.csb_mini game-csb/disk-b/MINI.DAT csb_dungeon.dat <saved game> CSBGAME.DAT
```

`<saved game>` is a CSB prison save or a DM save.

## Verification

`tests/dmtest/cedt.py` runs the original CEDT routines in `dmtools.emu`. It replaces three things: the file read service (`A5+0x20a`) with a memory reader, `0x105020` (allocate) with a bump allocator, and `0x105036` (free) with `RTS`.

| Check | Result |
|---|---|
| `0x108da2`, `0x108c62` (5 blocks), and `0x108efe` on `MINI.DAT`: plain header, blocks, and dungeon parts equal `decode_save` | Pass |
| A changed byte in block 3: the original and `decode_save` reject the file | Pass |
| A synthetic game from `encode_save`: the original reader accepts it | Pass |
| `0x10a31c(slot, 3)` and `0x105c7e` on two loaded slots, with the same random seed: header, champion records, and block 0 equal `merge` | Pass. The portrait pointer at `0x114` is a memory address and is masked. |
| The new `CSBGAME.DAT` from `encode_save`: the original reader accepts it | Pass |
| `src/` `dungeon_load_saved` on the decoded dungeon, then `dungeon_save` | Pass: 11 maps, the output equals the input |
| `dmcsb` `DungeonFile._extract` on the decoded dungeon | Pass: the parts end exactly at the checksum word, which matches |

The synthetic game has two champions in the same cell and a different party direction. Thus the test covers the turn and the cell fix.

`dmcsb.open_dungeon` rejects the uncompressed file. Its format detection expects an uncompressed dungeon to start with `0x0063`. The CSB dungeon starts with the ornament seed 13.

## Not verified

- No real prison save is available. The merge test uses a synthetic save that is made from `MINI.DAT` with dungeon ID 12.
- The original save routine `0x1085da` was not run. Only its output format is checked, through the original reader.
- The disk A engine was not run on a generated `CSBGAME.DAT`.
- The menu of disk B `APPB.FTL`, `HINT.FTL`, the disk prompts, and the `.BAK` handling of CEDT are not traced.
- The meaning of header bytes `0x100`–`0x127` (copied from the saved game) and of command `0xc9` (`DungeonB.dat`) is not known.
- Conversion of an old champion block (`0xd00` bytes, `0x109cfc`) is not covered.
