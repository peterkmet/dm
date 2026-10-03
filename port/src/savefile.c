#include "savefile.h"

#include <stdlib.h>
#include <string.h>

/* Header offsets */
#define H_PAYLOAD 0x100
#define H_PLATFORM 0x12b
#define H_KEYS 0x136
#define H_CHECKSUMS 0x156
#define H_FORMAT 0x176

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* XOR big-endian words in place; returns the sum of key, plain and cipher words (0x125510) */
uint16_t save_block_xor(uint8_t *bytes, uint16_t key, uint16_t word_count) {
    uint16_t sum = key;
    for (uint16_t i = word_count; i != 0; i--, bytes += 2) {
        uint16_t old = rd16(bytes), new = old ^ key;
        wr16(bytes, new);
        sum = (uint16_t)(sum + new + old);
        key = (uint16_t)(key + i);
    }
    return sum;
}

/* Block sizes as 16-bit values, as the original computes them from the globals block */
static uint16_t block_size(int i, const uint8_t *globals) {
    switch (i) {
    case 0: return 0x80;
    case 1: return (uint16_t)(rd16(globals + 46) << 4);
    case 2: return 0x580;
    case 3: return (uint16_t)(rd16(globals + 28) * 10);
    default: return (uint16_t)(rd16(globals + 28) * 2);
    }
}

static uint16_t payload_sum(const uint8_t *h) {
    uint16_t sum = 0;
    for (int i = H_PAYLOAD; i < SAVE_HEADER_BYTES; i += 2) sum = (uint16_t)(sum + rd16(h + i));
    return sum;
}

/* Chain over the 32 random word groups; with set_last, picks the last w3 so the chain gives sum */
static uint16_t header_chain(uint8_t *h, int set_last, uint16_t sum) {
    uint16_t acc = 0;
    for (int g = 0; g < 32; g++, h += 8) {
        uint16_t x = (uint16_t)((rd16(h + 2) ^ (uint16_t)(rd16(h) + acc)) - rd16(h + 4));
        if (set_last && g == 31) wr16(h + 6, sum ^ x);
        acc = rd16(h + 6) ^ x;
    }
    return acc;
}

int16_t save_decode(const uint8_t *file, uint32_t size, SaveFile *s) {
    memset(s, 0, sizeof *s);
    if (!file || size < SAVE_HEADER_BYTES || !(s->data = malloc(size))) return 0;
    uint8_t *d = s->header = s->data;
    memcpy(d, file, size);
    save_block_xor(d + H_PAYLOAD, rd16(d + 0x14), 0x80);
    if (header_chain(d, 0, 0) != payload_sum(d) || d[H_PLATFORM] == 1 || rd16(d + H_FORMAT) != 3) goto fail;
    uint32_t pos = SAVE_HEADER_BYTES;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        uint16_t n = block_size(i, s->block[0]);
        if (size - pos < n) goto fail;
        s->block[i] = d + pos;
        s->block_size[i] = n;
        uint16_t sum = save_block_xor(d + pos, rd16(d + H_KEYS + 2 * i), n >> 1);
        if (i != 2 && sum != rd16(d + H_CHECKSUMS + 2 * i)) goto fail; /* load_game ignores the champions result */
        pos += n;
    }
    if (size - pos < SAVE_PORTRAITS_BYTES) goto fail;
    s->portraits = d + pos;
    s->dungeon = d + pos + SAVE_PORTRAITS_BYTES;
    s->dungeon_size = size - pos - SAVE_PORTRAITS_BYTES;
    return 1;
fail:
    save_free(s);
    return 0;
}

uint8_t *save_encode(const SaveFile *s, uint32_t *size) {
    if (!s->header || !s->portraits || (!s->dungeon && s->dungeon_size)) return NULL;
    uint32_t total = SAVE_HEADER_BYTES + SAVE_PORTRAITS_BYTES + s->dungeon_size;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        if (!s->block[i] || s->block_size[i] != block_size(i, s->block[0])) return NULL;
        total += s->block_size[i];
    }
    uint8_t *out = malloc(total);
    if (!out) return NULL;
    memcpy(out, s->header, SAVE_HEADER_BYTES);
    uint32_t pos = SAVE_HEADER_BYTES;
    for (int i = 0; i < SAVE_BLOCKS; i++) {
        uint16_t n = s->block_size[i];
        memcpy(out + pos, s->block[i], n);
        wr16(out + H_CHECKSUMS + 2 * i, save_block_xor(out + pos, rd16(out + H_KEYS + 2 * i), n >> 1));
        pos += n;
    }
    header_chain(out, 1, payload_sum(out));
    save_block_xor(out + H_PAYLOAD, rd16(out + 0x14), 0x80);
    memcpy(out + pos, s->portraits, SAVE_PORTRAITS_BYTES);
    if (s->dungeon_size) memcpy(out + pos + SAVE_PORTRAITS_BYTES, s->dungeon, s->dungeon_size);
    *size = total;
    return out;
}

void save_free(SaveFile *s) {
    free(s->data);
    memset(s, 0, sizeof *s);
}
