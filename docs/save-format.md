# Save file format

This page describes the Dungeon Master v3.60 save file `DMGAME~.DAT` and the C code that reads and writes it. The C code is in `port/src/savefile.c` and `port/src/savefile.h`. The original routines are `save_game` (`0x125a44`) and `load_game` (`0x1265d4`) in `KAOS.FTL`.

The C code handles only the container: the header, the cipher, and the checksums. It does not copy blocks into the game state. Another part of the port does that.

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
| `0x12a` | 1 | Header byte. The original writes 1. |
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

`save_block_xor` (`0x125510`) encrypts and decrypts in place. XOR is symmetric, so one function does both. It also returns the checksum.

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
| 24 | 2 | `g_timeline_count` |
| 26 | 2 | `g_first_unused_event_index` |
| 28 | 2 | `g_event_max_count` |
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

`g_timeline_events`: 10 bytes per event, `g_event_max_count` entries.

### Block 4: timeline index

`g_timeline`: one word per event, `g_event_max_count` entries.

### Portraits

Four champion portrait bitmaps, not encrypted. Each bitmap has `((32 + 15) & ~15) / 2 × 29` = 464 bytes. `g_portrait_width` (`0x102958`) is 32. `g_portrait_height` (`0x10295a`) is 29. The bitmaps are in the interleaved form of `champion_portraits_to_interleaved` (`0x112f5c`).

### Dungeon

The dungeon in `Dungeon.DAT` format without compression: header (44 bytes), maps, column first-thing indices, square first things, text data, 16 thing arrays, and raw map data. Then one word: the 16-bit sum of all bytes of the dungeon parts. `load_dungeon` accepts the dungeon when the word is missing or equal to the sum. The dungeon header gives the part sizes. See [dungeon-format.md](dungeon-format.md).

## C API

The header `port/src/savefile.h` declares the API. All buffers keep the big-endian byte layout.

### `SaveFile`

| Field | Content |
|---|---|
| `data` | Plain copy of the whole file. `save_decode` allocates it. `save_free` frees it. |
| `header` | 512 bytes. The payload at `0x100` is plain. |
| `block[5]`, `block_size[5]` | Plain blocks 0–4 and their sizes in bytes. |
| `portraits` | 4 × 464 bytes. |
| `dungeon`, `dungeon_size` | Dungeon parts and the byte sum word. The C code does not parse them. |

### `int16_t save_decode(const uint8_t *file, uint32_t size, SaveFile *s)`

Copies the file, decrypts the header and the blocks, and sets the pointers in `s` into the copy. Returns 1 on success. Returns 0 and clears `s` when one of these checks fails:

- The file is shorter than the header, the blocks, or the portraits.
- The header chain does not equal the payload sum.
- The platform byte is 1 or the save format is not 3.
- The checksum of block 0, 1, 3, or 4 is not correct.

Call `save_free` after a successful decode.

### `uint8_t *save_encode(const SaveFile *s, uint32_t *size)`

Builds a save file from a plain header, the plain blocks, the portraits, and the dungeon. Returns a `malloc` buffer and sets `*size`. Free the buffer with `free`. The function does not change `s`.

The caller fills the header: the 256 random bytes, the 16 keys at `0x136`, and the fields at `0x12a`–`0x1ff`. Use `random16` for the random words and the keys, as the original does. The function then:

1. Computes the block checksums and writes them to `0x156`.
2. Sets `w3` of the last chain group.
3. Encrypts the payload and the blocks.

Returns `NULL` when a pointer is missing or a value in `block_size` does not match the size that block 0 gives.

### `uint16_t save_block_xor(uint8_t *bytes, uint16_t key, uint16_t word_count)`

The block cipher. Encrypts or decrypts `word_count` big-endian words in place. Returns the checksum.

### `void save_free(SaveFile *s)`

Frees the decoded copy and clears `s`.

## Tests

`tests/test_savefile.py` tests the C code through `build/libdm.dylib`:

- A round trip from encode to decode gives the same blocks, portraits, and dungeon.
- The original `save_block_xor` in `dmtools.emu` gives the same encrypted bytes and checksums as the C encoder.
- The original `save_read_header` and `save_read_checked_block` in `dmtools.emu` accept the C output. The test sets `g_load_from_memory` so that `save_file_read` reads from memory.
- Truncated files, changed bytes, a wrong format, platform 1, and random data fail.

## Limits

- No real save file was available. The tests use synthetic data and the original routines as the oracle.
- `save_decode` does not check the dungeon part. A file that is cut inside the dungeon part decodes. The dungeon loader must check the part sizes and the byte sum.
- The meaning of the header byte at `0x12a`, of `g_save_header_extra`, and of the long at block 0 offset 32 is not traced.
