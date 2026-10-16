/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/savefile.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "engine/random.h"
#include <stdlib.h>
#include <string.h>

/* Header offsets */
#define H_PAYLOAD          0x100
#define H_PAYLOAD_KEY      0x14
#define H_GROUP_BYTES      8
#define H_GROUP_COUNT      32
#define H_HEADER_BYTE      0x12a /* the original writes back the value of the last load; meaning not traced */
#define H_PLATFORM         0x12b
#define H_DIALOG_CHOICE    0x130
#define H_GAME_ID          0x132
#define H_KEY_COUNT        16
#define H_DUNGEON_ID       0x178
#define H_EXTRA            0x17a
#define H_KEYS             0x136
#define H_CHECKSUMS        0x156
#define H_FORMAT           0x176
#define H_CSB_PAYLOAD_KEY  0x3a
#define H_CSB_SHIFT        2  /* CSB moves the fields from 0x12a on by 2 bytes */
#define H_SAVED_GAME_BYTES 40 /* payload bytes that the CEDT merge copies from the saved game */

#define SAVE_FORMAT               3
#define PLATFORM_AMIGA            5 /* save_game writes 5; a load refuses 1 */
#define PLATFORM_REFUSED          1
#define HEADER_BYTE_DEFAULT       1 /* the header byte when no load set it */
#define HEADER_BYTE_NEW_ADVENTURE 2 /* CEDT writes 2 into a new CSB adventure */
#define DIALOG_CHOICE_SAVE        1

/* The offset of a payload field in the header of g_adventure; the DM offset is the argument. */
static int get_header_field_offset(int dm_offset)
{
    return g_adventure == ADVENTURE_CSB ? dm_offset + H_CSB_SHIFT : dm_offset;
}
static int payload_key(void)
{
    return g_adventure == ADVENTURE_CSB ? H_CSB_PAYLOAD_KEY : H_PAYLOAD_KEY;
}

uint16_t save_block_xor(uint8_t *bytes, uint16_t key, uint16_t word_count)
{
    uint16_t sum = key;
    for (uint16_t i = word_count; i != 0; i--, bytes += 2) {
        uint16_t old = be16(bytes);
        uint16_t new = old ^ key;
        wr16(bytes, new);
        sum = sum + new + old;
        key = key + i;
    }
    return sum;
}

/* Block sizes as 16-bit values, as the original computes them from the globals block */
static uint16_t block_size(int i, const uint8_t *globals)
{
    switch (i) {
    case SAVE_BLOCK_GLOBALS:
        return SAVE_GLOBALS_BYTES;
    case SAVE_BLOCK_GROUPS:
        return be16(globals + G_MAX_ACTIVE_GROUP_COUNT) * SAVE_ACTIVE_GROUP_BYTES;
    case SAVE_BLOCK_CHAMPIONS:
        return SAVE_CHAMPIONS_BYTES;
    case SAVE_BLOCK_EVENTS:
        return be16(globals + G_EVENT_MAX_COUNT) * SAVE_EVENT_BYTES;
    default:
        return be16(globals + G_EVENT_MAX_COUNT) * 2;
    }
}

/* Sum of the plain payload words; the chain must give this value. */
static uint16_t payload_sum(const uint8_t *h)
{
    uint16_t sum = 0;
    for (int i = H_PAYLOAD; i < SAVE_HEADER_BYTES; i += 2)
        sum = sum + be16(h + i);
    return sum;
}

/* Chain over the 32 random word groups; with set_last, picks the last w3 so the chain gives sum */
static uint16_t compute_header_chain(uint8_t *h, bool set_last, uint16_t sum)
{
    uint16_t acc = 0;
    for (int g = 0; g < H_GROUP_COUNT; g++, h += H_GROUP_BYTES) {
        uint16_t x = (uint16_t)((be16(h + 2) ^ (uint16_t)(be16(h) + acc)) - be16(h + 4)); /* 16-bit wraps */
        if (set_last && g == H_GROUP_COUNT - 1)
            wr16(h + 6, sum ^ x);
        acc = be16(h + 6) ^ x;
    }
    return acc;
}

bool save_decode(const uint8_t *file, uint32_t size, SaveFile *s)
{
    memset(s, 0, sizeof *s);
    if (!file || size < SAVE_HEADER_BYTES || !(s->data = malloc(size)))
        return false;
    s->header = s->data;
    uint8_t *d = s->header;
    memcpy(d, file, size);
    save_block_xor(d + H_PAYLOAD, be16(d + payload_key()), SAVE_PAYLOAD_WORDS);
    if (compute_header_chain(d, false, 0) != payload_sum(d) ||
        d[get_header_field_offset(H_PLATFORM)] == PLATFORM_REFUSED ||
        be16(d + get_header_field_offset(H_FORMAT)) != SAVE_FORMAT)
        goto fail;
    uint32_t pos = SAVE_HEADER_BYTES;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        uint16_t n = block_size(i, s->block[0]);
        if (size - pos < n)
            goto fail;
        s->block[i] = d + pos;
        s->block_size[i] = n;
        uint16_t sum = save_block_xor(d + pos, be16(d + get_header_field_offset(H_KEYS) + 2 * i), n >> 1);
        /* DM load_game ignores the champions result; CSB checks it */
        if ((i != SAVE_BLOCK_CHAMPIONS || g_adventure == ADVENTURE_CSB) &&
            sum != be16(d + get_header_field_offset(H_CHECKSUMS) + 2 * i))
            goto fail;
        pos += n;
    }
    if (size - pos < SAVE_PORTRAITS_BYTES)
        goto fail;
    s->portraits = d + pos;
    s->dungeon = d + pos + SAVE_PORTRAITS_BYTES;
    s->dungeon_size = size - pos - SAVE_PORTRAITS_BYTES;
    return true;
fail:
    save_free(s);
    return false;
}

uint8_t *save_encode(const SaveFile *s, uint32_t *size)
{
    if (!s->header || !s->portraits || (!s->dungeon && s->dungeon_size))
        return NULL;
    uint32_t total = SAVE_HEADER_BYTES + SAVE_PORTRAITS_BYTES + s->dungeon_size;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        if (!s->block[i] || s->block_size[i] != block_size(i, s->block[0]))
            return NULL;
        total += s->block_size[i];
    }
    uint8_t *out = malloc(total);
    if (!out)
        return NULL;
    memcpy(out, s->header, SAVE_HEADER_BYTES);
    uint32_t pos = SAVE_HEADER_BYTES;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        uint16_t n = s->block_size[i];
        memcpy(out + pos, s->block[i], n);
        uint16_t sum = save_block_xor(out + pos, be16(out + get_header_field_offset(H_KEYS) + 2 * i), n >> 1);
        wr16(out + get_header_field_offset(H_CHECKSUMS) + 2 * i, sum);
        pos += n;
    }
    compute_header_chain(out, true, payload_sum(out));
    save_block_xor(out + H_PAYLOAD, be16(out + payload_key()), SAVE_PAYLOAD_WORDS);
    memcpy(out + pos, s->portraits, SAVE_PORTRAITS_BYTES);
    if (s->dungeon_size)
        memcpy(out + pos + SAVE_PORTRAITS_BYTES, s->dungeon, s->dungeon_size);
    *size = total;
    return out;
}

void save_free(SaveFile *s)
{
    free(s->data);
    memset(s, 0, sizeof *s);
}

void save_header_randomize(uint8_t *h)
{
    /* WR16 reads its value twice: one random16 call per word. */
    for (int i = 0; i < H_KEY_COUNT; i++) {
        uint16_t key = random16();
        wr16(h + get_header_field_offset(H_KEYS) + 2 * i, key);
    }
    for (int i = 0; i < H_GROUP_COUNT * H_GROUP_BYTES / 2 - 1; i++) {
        uint16_t word = random16();
        wr16(h + 2 * i, word);
    }
}

/* save_game and save_write_header: the 16 block keys, then the random chain words (the last w3
   comes from save_encode), then the fields of the payload. */
void save_header_init(uint8_t *h, uint32_t game_id, uint16_t dungeon_id, const uint8_t *extra)
{
    memset(h, 0, SAVE_HEADER_BYTES);
    save_header_randomize(h);
    h[get_header_field_offset(H_HEADER_BYTE)] = HEADER_BYTE_DEFAULT;
    h[get_header_field_offset(H_PLATFORM)] = PLATFORM_AMIGA;
    wr16(h + get_header_field_offset(H_DIALOG_CHOICE), DIALOG_CHOICE_SAVE);
    wr32(h + get_header_field_offset(H_GAME_ID), game_id);
    wr16(h + get_header_field_offset(H_FORMAT), SAVE_FORMAT);
    wr16(h + get_header_field_offset(H_DUNGEON_ID), dungeon_id);
    if (extra)
        memcpy(h + get_header_field_offset(H_EXTRA), extra, save_header_extra_size());
}

/* CEDT merge: the first payload bytes come from the saved game (0 in a port save), the game ID is
   the ID of the saved game + 1, the header byte is 2. New keys and chain words follow. */
void save_header_new_adventure(uint8_t *h, uint32_t game_id)
{
    memset(h + H_PAYLOAD, 0, H_SAVED_GAME_BYTES);
    wr32(h + get_header_field_offset(H_GAME_ID), game_id + 1);
    wr16(h + get_header_field_offset(H_FORMAT), SAVE_FORMAT);
    h[get_header_field_offset(H_HEADER_BYTE)] = HEADER_BYTE_NEW_ADVENTURE;
    save_header_randomize(h);
}

uint32_t save_header_game_id(const uint8_t *h)
{
    return be32(h + get_header_field_offset(H_GAME_ID));
}
uint16_t save_header_dungeon_id(const uint8_t *h)
{
    return be16(h + get_header_field_offset(H_DUNGEON_ID));
}
const uint8_t *save_header_extra(const uint8_t *h)
{
    return h + get_header_field_offset(H_EXTRA);
}
uint16_t save_header_extra_size(void)
{
    return SAVE_HEADER_BYTES - get_header_field_offset(H_EXTRA);
}
