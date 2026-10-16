# Save file format

This page describes the Dungeon Master v3.60 save file `DMGAME~.DAT` and the C code that reads and writes it. The original routines are `save_game` (`0x125a44`) and `load_game` (`0x1265d4`) in `KAOS.FTL`.

The C code has two layers:

- `src/engine/savefile.c` handles the container: the header, the cipher, and the checksums.
- `src/engine/savegame.c` copies the game state into the blocks and back. `src/engine/dungeon.c` writes and reads the dungeon part.

The English version replaces `~` in the file name with nothing. The port writes `DMGAME.DAT` and `DMGAME.BAK`.

## Byte order

All words and longs are big-endian. The cipher and the checksums operate on big-endian 16-bit words. All sums use 16-bit wraparound.

## File layout

| Part | Size in bytes | Encrypted | Checked |
|---|---|---|---|
| Header | 512 | Bytes `0x100`–`0x1ff` | Header chain |
| Block 0: globals | 128 | Yes | Checksum |
| Block 1: active groups | `max_active_group_count × 16` | Yes | Checksum |
| Block 2: champions and party | 1408 (`0x580`) | Yes | Not checked on load |
| Block 3: timeline events | `event_max_count × 10` | Yes | Checksum |
| Block 4: timeline index | `event_max_count × 2` | Yes | Checksum |
| Portraits | 4 × 464 | No | No |
| Dungeon | Variable | No | Byte sum word at the end |

The globals block gives the sizes of blocks 1, 3, and 4. The reader decrypts block 0 first. Then it reads the two counts. The original computes the sizes as 16-bit values. The C code does the same.

## Header

The header has 512 bytes. Offsets are bytes from the start of the file.

| Offset | Size | Content |
|---|---|---|
| `0x000` | 256 | 32 groups of 4 random words `w0`, `w1`, `w2`, `w3`. Not encrypted. |
| `0x014` | 2 | Payload key. This is word `w2` of group 2. |
| `0x100` | 256 | Payload: 128 words, encrypted with the payload key. |
| `0x12a` | 1 | Header byte (`SAVE_HEADER_HEADER_BYTE`). The original writes 1. |
| `0x12b` | 1 | Platform. The original writes 5. A load fails when the value is 1. |
| `0x130` | 2 | Dialog choice. The original writes 1. A load does not read it. |
| `0x132` | 4 | Game ID (`g_game_id`). |
| `0x136` | 32 | 16 random keys. Keys 0–4 encrypt blocks 0–4. |
| `0x156` | 10 | Checksums of blocks 0–4. |
| `0x176` | 2 | Save format. A load fails when the value is not 3. |
| `0x178` | 2 | Dungeon ID. 10 for the Dungeon Master dungeon. |
| `0x17a` | 134 | `g_save_header_extra`. The original copies it unchanged. |

The offsets from `0x12a` to `0x1ff` are in the payload. You read them only after decryption.

### Header chain

The random words make a chain. The chain value must equal the 16-bit sum of the 128 plain payload words.

```c
acc = 0;
for (g = 0; g < 32; g++)
    acc = w3[g] ^ ((w1[g] ^ (w0[g] + acc)) - w2[g]);
```

The writer (`save_write_header`, `0x1257ca`) fills all words with `random16`. It sets `w3` of the last group so that the chain gives the payload sum. The reader (`save_read_header`, `0x125766`) decrypts the payload and compares the two values.

## Block cipher

`savefile_xor_block` (`0x125510`) encrypts and decrypts in place. XOR is symmetric, so one function does both. It also returns the checksum.

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

The checksum adds the plain word and the encrypted word. Thus the checksum is the same before and after encryption. The original `save_block_checksum` (`0x125542`) computes the same sum without a change to the data.

The original loop is a `do`/`while` loop. A count of 0 runs 65536 times. The C code runs 0 times for a count of 0. A valid save does not have an empty block.

The checksum is weak. A change that sets a bit in the plain word and clears the same bit in the encrypted word does not change the sum.

## Blocks

### Block 0: globals

128 bytes. Unused bytes are 0.

| Offset | Size | Global |
|---|---|---|
| 0 | 4 | `g_game_time` |
| 4 | 4 | `g_random_seed` |
| 8 | 2 | `g_leader_hand_object` |
| 10 | 2 | `g_party_champion_count` |
| 12 | 2 | `g_party_map_x` |
| 14 | 2 | `g_party_map_y` |
| 16 | 2 | `g_party_direction` |
| 18 | 2 | `g_party_map_index` |
| 20 | 2 | `g_leader_index` |
| 22 | 2 | `g_magic_caster_champion_index` |
| 24 | 2 | `g_timeline.count` |
| 26 | 2 | `g_timeline.first_unused` |
| 28 | 2 | `g_timeline.capacity` |
| 30 | 2 | `g_active_group_count` |
| 32 | 4 | `g_last_creature_attack_time` (`0x10314e`) |
| 36 | 4 | `g_last_party_movement_time` |
| 40 | 2 | `g_disabled_movement_ticks` |
| 42 | 2 | `g_projectile_disabled_movement_ticks` |
| 44 | 2 | `g_last_projectile_disabled_movement_direction` |
| 46 | 2 | `g_max_active_group_count` |

### Block 1: active groups

`g_active_groups`: 16 bytes per active group, `g_max_active_group_count` entries. After the load, the original raises `g_max_active_group_count` to at least 110 (`0x6e`).

### Block 2: champions and party

1408 bytes: 4 champion records of `0x140` bytes (`g_champions`), then 128 bytes of `g_party` (`0x101ed4`).

Before the save, `save_game` removes the weight of the leader hand object from the leader load (champion offset `0x110`). `load_game` does not check the checksum of this block. It ignores the return value of `save_read_checked_block`. The C code does the same.

### Block 3: timeline events

`g_timeline.events`: 10 bytes per event, `g_timeline.capacity` entries.

### Block 4: timeline index

`g_timeline.heap`: one word per event, `g_timeline.capacity` entries.

### Portraits

Four champion portrait bitmaps, not encrypted. Each bitmap has `((32 + 15) & ~15) / 2 × 29` = 464 bytes. `g_portrait_width` (`0x102958`) is 32. `g_portrait_height` (`0x10295a`) is 29. The bitmaps are in the interleaved form of `champion_portraits_to_interleaved` (`0x112f5c`).

### Dungeon

The dungeon in `Dungeon.DAT` format without compression: header (44 bytes), maps, column first-thing indices, square first things, text data, 16 thing arrays, and raw map data. Then one word: the 16-bit sum of all bytes of the dungeon parts. `dungeon_load` accepts the dungeon when the word is missing or equal to the sum. The dungeon header gives the part sizes. See [dungeon-format.md](dungeon.md).

## C API

The header `src/engine/savefile.h` declares the API. All buffers keep the big-endian byte layout.

### `SaveFile`

| Field | Content |
|---|---|
| `data` | Plain copy of the whole file. `savefile_decode` allocates it. `savefile_free` frees it. |
| `header` | 512 bytes. The payload at `0x100` is plain. |
| `block[5]`, `block_size[5]` | Plain blocks 0–4 and their sizes in bytes. |
| `portraits` | 4 × 464 bytes. |
| `dungeon`, `dungeon_size` | Dungeon parts and the byte sum word. The C code does not parse them. |

### `int16_t savefile_decode(const uint8_t *file, uint32_t size, SaveFile *s)`

Copies the file, decrypts the header and the blocks, and sets the pointers in `s` into the copy. Returns 1 on success. Returns 0 and clears `s` when one of these checks fails:

- The file is shorter than the header, the blocks, or the portraits.
- The header chain does not equal the payload sum.
- The platform byte is 1 or the save format is not 3.
- The checksum of block 0, 1, 3, or 4 is not correct. CSB also checks block 2.

Call `savefile_free` after a successful decode.

### `uint8_t *savefile_encode(const SaveFile *s, uint32_t *size)`

Builds a save file from a plain header, the plain blocks, the portraits, and the dungeon. Returns a `malloc` buffer and sets `*size`. Free the buffer with `free`. The function does not change `s`.

The caller fills the header: the 256 random bytes, the 16 keys at `0x136`, and the fields at `0x12a`–`0x1ff`. Use `random16` for the random words and the keys, as the original does. The function then:

1. Computes the block checksums and writes them to `0x156`.
2. Sets `w3` of the last chain group.
3. Encrypts the payload and the blocks.

Returns `NULL` when a pointer is missing or a value in `block_size` does not match the size that block 0 gives.

### `uint16_t savefile_xor_block(uint8_t *bytes, uint16_t key, uint16_t word_count)`

The block cipher. Encrypts or decrypts `word_count` big-endian words in place. Returns the checksum.

### `void savefile_free(SaveFile *s)`

Frees the decoded copy and clears `s`.

### Header fields

| Function | Work |
|---|---|
| `savefile_init_header(h, game_id, dungeon_id, extra)` | Clears the header. Writes 16 keys with `random16`, then 127 chain words with `random16`, in the order of `save_game` and `save_write_header`. Writes the header byte 1, platform 5, dialog choice 1, the game ID, format 3, the dungeon ID, and the 134 extra bytes (CSB: 132). |
| `savefile_get_game_id(h)`, `savefile_get_dungeon_id(h)`, `savefile_get_header_extra(h)` | Read the fields of a plain header. |
| `savefile_get_extra_size()` | The number of extra bytes: 134, or 132 for CSB. |

### CSB header

With `g_adventure == ADVENTURE_CSB`, all functions use the CSB header: the payload key is at `0x3a` (word 29), and the fields from `0x12a` on are 2 bytes later. The extra bytes start at `0x17c`. `savefile_decode` also checks the block 2 checksum. See [csb-data.md](../csb/data.md) "Save file".

## Game state (`savegame.c`)

### Files

The save directory is the game data directory (`--data`, default `game/`). `startend_set_save_dir` changes it.

`int16_t savegame_save(const char *dir)`:

1. `timeline_reset`. Then pack the globals, the active groups, the champions, the party, the timeline events, and the timeline index into the blocks.
2. The leader load in the file does not count the object in the leader hand, as in `save_game`. The live state does not change.
3. Write the portraits and the dungeon (`dungeon_save`).
4. `savefile_init_header` with `g_game_id`, `g_dungeon_id`, and the extra bytes of the last load. `savefile_encode`.
5. Delete `DMGAME.BAK`. Rename `DMGAME.DAT` to `DMGAME.BAK`. Write `DMGAME.DAT`.
6. Set `g_saved_game_exists` and `g_last_save_game_time`. Return 1, or 0 on a write error.

The random header words come after the copy of the globals. Thus the saved random seed is the seed before the save, as in the original.

`int16_t savegame_load(const char *dir)` returns `SAVEGAME_LOADED` (1), `SAVEGAME_MISSING` (0), or `SAVEGAME_DAMAGED` (−1):

1. Read `DMGAME.DAT`. When it is missing, read `DMGAME.BAK`.
2. `savefile_decode`. Then `dungeon_load_saved` on the dungeon part. A failure gives `SAVEGAME_DAMAGED`.
3. Copy block 0 into the globals. `g_last_load_game_time` gets the game time.
4. Allocate the timeline for `g_timeline.capacity` events and the active groups for at least 110 entries. Copy blocks 1–4. Raise `g_max_active_group_count` to at least 110. `timeline_reset`. CSB allocates exactly the saved count and keeps it. A new CSB game has 60 entries.
5. Find the portrait index of each party champion (see Portraits). Copy the game ID, the dungeon ID, and the extra header bytes.
6. Clear `g_party_dead` and the sleep flag. Set `g_saved_game_exists`.
7. When the file was the backup, rename it to `DMGAME.DAT`.

After a damaged file, the dungeon is not loaded and the globals can be partly changed. The entrance starts a new game from `Dungeon.DAT`, which sets all state again.

`gameloop_start_loaded_game` then does the rest of `start_game`: it makes the party map current (`group_remove_all_active_groups`, `map_set_current_map_and_party_map`, `group_add_all_active_groups`), shows "GAME LOADED, READY TO PLAY.", draws the interface, and runs `champion_reset_data_to_start_game`: the hand object goes back into the hand, `champion_set_leader` adds its weight to the leader load again, and the magic caster comes back.

### Block contents

- Block 0: the globals of the table above. The unused bytes are 0.
- Block 1: `ActiveGroup` in its original 16-byte order. `group_index` is a big-endian word.
- Block 2: `champion_pack` for the 4 champions, then the party record (`docs/rules/game-state.md`). The bytes from `0x58` to `0x7f` of the party part are not traced. The port writes 0 and ignores them on load.
- Block 3: 10 bytes per event: `map_time` (long), `type`, `priority`, `b`, `c`. A free slot has the next free index in the high word, as `timeline_reset` writes it.
- Block 4: the heap of slot indices.

### Portraits

The port does not keep portrait bitmaps. It keeps the index of each portrait in graphic 26 (`g_champion_portrait_indices`). The save writes the portrait of each party champion in the interleaved form: per row and per 16-pixel column, one word for each of the 4 bit planes (`champion_portraits_to_interleaved`). The other portraits are 0. The load encodes each portrait of graphic 26 in the same form and takes the index of the equal one. A portrait that has no equal gets index 0.

### Dungeon part (`dungeon.c`)

- `uint8_t *dungeon_save(uint32_t *size)` writes the parts in the order of `save_game`: header, maps, column first-thing indices, square first things, text data, 16 thing arrays, raw map data. Then the 16-bit sum of all bytes. The header has the counts with the free records of the new game, so the load needs no extras.
- `int16_t dungeon_load_saved(const uint8_t *data, uint32_t size)` reads these parts like `dungeon_load` for a saved game: no compression, no party start from the header, no free records, and no new `g_timeline.capacity`. It checks the byte sum when the word is there.

## Tests

`tests/port/test_save.py` tests the game state layer:

- Round trip: a new game, a recruited champion with an object in the hand, then a save. A new game, then a load. The globals, the packed champions (the leader load less the hand object), the party, the timeline, the active groups, the portrait indices, and `dungeon_save` are equal.
- The second save keeps the first file as `DMGAME.BAK`. A load without `DMGAME.DAT` uses the backup and renames it.
- A missing file gives `SAVEGAME_MISSING`. A truncated file, a changed dungeon end, a changed header byte, and zeros give `SAVEGAME_DAMAGED`.
- The original `save_read_header` and `save_read_checked_block` in `dmtools.emu` accept the file of `savegame_save`. The dungeon sum word is correct.
- Ctrl+S runs the save dialog. A frame hook clicks SAVE, then CANCEL. The file exists and the game is not frozen.
- Party death without a frame hook ends with `g_return_to_entrance`. With a frame hook, RESTART THIS GAME loads the saved game and the game continues. QUIT on THE END shows the credits and then sets `g_return_to_entrance`.

`tests/differential/test_savefile.py` tests the container through `build/libdm.dylib`:

- A round trip from encode to decode gives the same blocks, portraits, and dungeon.
- The original `savefile_xor_block` in `dmtools.emu` gives the same encrypted bytes and checksums as the C encoder.
- The original `save_read_header` and `save_read_checked_block` in `dmtools.emu` accept the C output. The test sets `g_load_from_memory` so that `save_file_read` reads from memory.
- Truncated files, changed bytes, a wrong format, platform 1, and random data fail.

## Limits

- No real save file was available. The tests use synthetic data, the port's own saves, and the original routines as the oracle. The original `load_dungeon` reads a file handle, so the tests do not run it on the dungeon part.
- Deliberate deviation: the original quits the program after the credits of the endgame and after "THE GAME IS DAMAGED!". The port sets `g_return_to_entrance`, and `main.c` shows the entrance.
- A restart does not check that the saved game has the same game ID ("THAT'S NOT THE SAME GAME").
- The header byte at `0x12a` (`SAVE_HEADER_HEADER_BYTE`) is always written as 1 (`HEADER_BYTE_DEFAULT`). The original writes back the value of the last load.
- `savefile_decode` does not check the dungeon part. A file that is cut inside the dungeon part decodes. The dungeon loader must check the part sizes and the byte sum.
- The meaning of the header byte at `0x12a`, of `g_save_header_extra`, and of the long at block 0 offset 32 is not traced.
