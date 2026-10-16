/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/ftl.h"
#include "data/bytes.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FTL_MAX_SIZE (4u << 20)

/* FTL module file: header, then a table of section entries (tag, version, offset, size). */
#define FTL_MAGIC              0x6160
#define FTL_HEADER_CHECKSUM    2
#define FTL_HEADER_VERSION     4
#define FTL_HEADER_ENTRY_COUNT 18
#define FTL_HEADER_BYTES       20
#define FTL_ENTRY_BYTES        12
#define FTL_MAX_ENTRIES        100
#define FTL_CHECKSUM_WEIGHTS   256 /* entry byte i counts i % 256 + 1 times */
#define FTL_FORMAT_VERSION     2   /* word at FTL_HEADER_VERSION */
#define FTL_FORMAT_REVISION    1   /* byte after the version word */
#define FTL_FORMAT_LEVEL       0   /* byte after the revision */
#define FTL_SECTION_INFO       16
#define FTL_SECTION_DATA       17
#define FTL_SECTION_CODE       18

/* The number of section kinds: info, data, code. */
#define FTL_SECTION_KINDS 3

/* Fields of a section entry. */
#define FTL_ENTRY_TAG     0
#define FTL_ENTRY_VERSION 2
#define FTL_ENTRY_OFFSET  4
#define FTL_ENTRY_SIZE    8
#define FTL_ENTRY_FORMAT  1 /* the entry version that the loader accepts */

/* Info section fields. The entry point offsets follow the fixed fields. */
#define INFO_REGION_SIZE  0
#define INFO_DATA_SIZE    4
#define INFO_JUMP_TABLE   12
#define INFO_CODE_SIZE    20
#define INFO_CODE_PACKED  30
#define INFO_CHECKSUM     34
#define INFO_CODE_SUM     36
#define INFO_DATA_SUM     38
#define INFO_ENTRY_POINTS 40

/* Data section: 2 bytes, run-length data size, relocation table size, then both tables. */
#define DATA_RLE_SIZE     2
#define DATA_RELOC_SIZE   6
#define DATA_HEADER_BYTES 10

/* Jump table entry: 2 bytes, then a 68000 "jmp absolute long" to the entry point. */
#define JUMP_ENTRY_BYTES    8
#define JUMP_ENTRY_JMP      2
#define JMP_ABS_LONG_OPCODE 0x4ef9
#define JMP_ABS_LONG_BYTES  6 /* opcode word and address long */

/* Packed code section: magic, word count, a dictionary of 1920 words, then the nibble stream. */
#define RSHARP_MAGIC       0x5223
#define RSHARP_WORD_COUNT  4
#define RSHARP_DICTIONARY  8
#define RSHARP_SHORT_CODES 128
#define RSHARP_STREAM      (RSHARP_DICTIONARY + 2 * (RSHARP_SHORT_CODES + 7 * 256))
#define RSHARP_LITERAL     15
#define NIBBLE_BITS        4
#define NIBBLE_MASK        0xf

typedef struct {
    const uint8_t *s;
    size_t n, pos;
} Nibbles;

/* Reads count nibbles, high nibble first; past the end it reads zeros. */
static unsigned read_nibble(Nibbles *b, int count)
{
    unsigned v = 0;
    for (; count; count--, b->pos++) {
        unsigned nibble = 0;
        if (b->pos < b->n)
            nibble = b->s[b->pos / 2] >> (b->pos % 2 ? 0 : NIBBLE_BITS) & NIBBLE_MASK;
        v = v << NIBBLE_BITS | nibble;
    }
    return v;
}

/* Expands the R# nibble coding of a packed code section. Codes 0-7 take one more nibble and
   15 is a literal word; the other codes take two more nibbles. */
static uint8_t *unpack(const uint8_t *p, size_t len, uint32_t *out_len)
{
    if (len < RSHARP_STREAM || be16(p) != RSHARP_MAGIC)
        return NULL;
    uint32_t words = be32(p + RSHARP_WORD_COUNT);
    Nibbles b = {p + RSHARP_STREAM, (len - RSHARP_STREAM) * 2, 0};
    uint8_t *out = malloc((size_t)words * 2 + 1);
    if (!out)
        return NULL;
    for (uint32_t i = 0; i < words; i++) {
        unsigned v = read_nibble(&b, 1), w;
        if (v == RSHARP_LITERAL)
            w = read_nibble(&b, 4);
        else {
            unsigned t = v < 8 ? v << NIBBLE_BITS | read_nibble(&b, 1)
                               : RSHARP_SHORT_CODES + ((v - 8) << BYTE_BITS | read_nibble(&b, 2));
            w = be16(p + RSHARP_DICTIONARY + 2 * t);
        }
        wr16(out + 2 * i, w);
    }
    if (b.pos > b.n) {
        free(out);
        return NULL;
    }
    *out_len = words * 2;
    return out;
}

/* The 16-bit sum of the bytes of a section: the FTL checksum. */
static uint16_t sum8(const uint8_t *p, size_t n)
{
    uint32_t s = 0;
    while (n--)
        s += *p++;
    return s;
}

/* Checks the file header and finds the info, data, and code sections. False when the header or an entry is wrong. */
static bool find_sections(const uint8_t *raw, size_t len, const uint8_t *sec[FTL_SECTION_KINDS],
                          uint32_t secsz[FTL_SECTION_KINDS])
{
    uint32_t count, s = 0;
    if (len < FTL_HEADER_BYTES || be16(raw) != FTL_MAGIC || be16(raw + FTL_HEADER_VERSION) != FTL_FORMAT_VERSION ||
        raw[FTL_HEADER_VERSION + 2] != FTL_FORMAT_REVISION || raw[FTL_HEADER_VERSION + 3] != FTL_FORMAT_LEVEL)
        return false;
    count = be16(raw + FTL_HEADER_ENTRY_COUNT);
    if (count > FTL_MAX_ENTRIES || len < FTL_HEADER_BYTES + count * FTL_ENTRY_BYTES)
        return false;
    for (int i = FTL_HEADER_VERSION; i < FTL_HEADER_BYTES; i++)
        s += raw[i] * i;
    for (uint32_t i = 0; i < count * FTL_ENTRY_BYTES; i++)
        s += raw[FTL_HEADER_BYTES + i] * (i % FTL_CHECKSUM_WEIGHTS + 1);
    if ((uint16_t)s != be16(raw + FTL_HEADER_CHECKSUM))
        return false; /* the checksum is the low word of the sum */
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = raw + FTL_HEADER_BYTES + i * FTL_ENTRY_BYTES;
        uint16_t tag = be16(e + FTL_ENTRY_TAG);
        uint32_t off = be32(e + FTL_ENTRY_OFFSET);
        uint32_t size = be32(e + FTL_ENTRY_SIZE);
        if (tag < FTL_SECTION_INFO || tag > FTL_SECTION_CODE)
            continue;
        if (be16(e + FTL_ENTRY_VERSION) != FTL_ENTRY_FORMAT || sec[tag - FTL_SECTION_INFO] || off > len ||
            size > len - off)
            return false;
        sec[tag - FTL_SECTION_INFO] = raw + off;
        secsz[tag - FTL_SECTION_INFO] = size;
    }
    return true;
}

/* Expands the run-length data into mem: words are copied, and a zero word is followed by a count of
   extra zero bytes. Returns the number of bytes written, or -1 when it overflows size. */
static int64_t expand_data(const uint8_t *r, uint32_t rle_len, uint8_t *mem, uint64_t size)
{
    const uint8_t *rend = r + rle_len;
    uint32_t n = 0;
    while (r + 1 < rend) {
        bool zero = !r[0] && !r[1];
        if (n + 2 > size)
            return -1;
        mem[n++] = *r++;
        mem[n++] = *r++;
        if (zero && r + 1 < rend) {
            n += be16(r);
            r += 2;
        }
    }
    return n;
}

/* Adds data_base to each long word that the relocation table names. False when an offset is outside the image. */
static bool relocate(const uint8_t *table, uint32_t reloc_len, uint8_t *mem, uint64_t size, uint64_t data_offset,
                     uint32_t data_base)
{
    for (const uint8_t *p = table; p + 1 < table + reloc_len; p += 2) {
        int64_t at = (int64_t)data_offset + (int16_t)be16(p); /* signed offset */
        if (at < 0 || (uint64_t)at + 4 > size)
            return false;
        uint32_t v = be32(mem + at) + data_base;
        wr32(mem + at, v);
    }
    return true;
}

/* Writes a jmp to each entry point into the jump table. False when an offset is outside the image. */
static bool write_jump_table(const uint8_t *info, uint32_t info_size, uint8_t *mem, uint64_t size,
                             uint64_t table_offset, uint32_t code_base)
{
    for (uint32_t i = 0; INFO_ENTRY_POINTS + i * 4 + 4 <= info_size; i++) {
        uint64_t at = table_offset + i * JUMP_ENTRY_BYTES + JUMP_ENTRY_JMP;
        uint32_t v = code_base + be32(info + INFO_ENTRY_POINTS + i * 4);
        if (at + JMP_ABS_LONG_BYTES > size)
            return false;
        wr16(mem + at, JMP_ABS_LONG_OPCODE);
        wr32(mem + at + 2, v);
    }
    return true;
}

/* The address rounded up to the next word: the loader aligns each part of the image to a word. */
static uint64_t align_to_word(uint64_t address)
{
    return (address + 1) & ~(uint64_t)1;
}

bool ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img)
{
    const uint8_t *sec[FTL_SECTION_KINDS] = {0};
    uint32_t secsz[FTL_SECTION_KINDS] = {0};
    uint8_t *code = NULL;
    uint8_t *mem = NULL;
    if (!find_sections(raw, len, sec, secsz))
        return false;
    const uint8_t *info = sec[0];
    const uint8_t *data = sec[1];
    if (!info || !data || !sec[2] || secsz[0] < INFO_ENTRY_POINTS || secsz[1] < DATA_HEADER_BYTES)
        return false;

    uint32_t region = be32(info + INFO_REGION_SIZE);
    uint32_t dsize = be32(info + INFO_DATA_SIZE);
    uint32_t jt_off = be32(info + INFO_JUMP_TABLE);
    uint32_t csize = be32(info + INFO_CODE_SIZE);
    uint16_t info_chk = be16(info + INFO_CHECKSUM);
    uint16_t ws = 0;
    for (uint32_t i = 0; i + 1 < secsz[0]; i += 2)
        ws += be16(info + i);
    if ((uint16_t)(ws - info_chk) != info_chk || sum8(data, secsz[1]) != be16(info + INFO_DATA_SUM))
        return false; /* 16-bit wrap */
    uint32_t code_len = secsz[2];
    const uint8_t *codep = sec[2];
    if (be16(info + INFO_CODE_PACKED)) {
        code = unpack(sec[2], secsz[2], &code_len);
        if (!code)
            return false;
        codep = code;
    }
    if (code_len < csize || sum8(codep, csize) != be16(info + INFO_CODE_SUM))
        goto fail;

    uint64_t data_base = align_to_word(base + dsize);
    uint64_t code_base = align_to_word(data_base + region);
    uint64_t size = code_base - base + csize;
    uint32_t rle_len = be32(data + DATA_RLE_SIZE);
    uint32_t reloc_len = be32(data + DATA_RELOC_SIZE);
    uint32_t tables_len = secsz[1] - DATA_HEADER_BYTES;
    if (size > FTL_MAX_SIZE || base + size > UINT32_MAX || rle_len > tables_len || reloc_len > tables_len - rle_len ||
        !(mem = calloc(size + 1, 1)))
        goto fail;
    const uint8_t *rle = data + DATA_HEADER_BYTES;
    if (expand_data(rle, rle_len, mem, size) != dsize ||
        !relocate(rle + rle_len, reloc_len, mem, size, data_base - base, (uint32_t)data_base) ||
        !write_jump_table(info, secsz[0], mem, size, data_base - base + jt_off, (uint32_t)code_base))
        goto fail;
    memcpy(mem + code_base - base, codep, csize);
    free(code);
    *img = (FtlImage){mem, (uint32_t)size, base, (uint32_t)data_base, (uint32_t)code_base};
    return true;
fail:
    free(code);
    free(mem);
    return false;
}

void ftl_free(FtlImage *img)
{
    free(img->data);
    *img = (FtlImage){0};
}

uint8_t *ftl_at(const FtlImage *image, uint32_t address, uint32_t bytes)
{
    if (!image->data || address < image->base || (uint64_t)address - image->base + bytes > image->size)
        return NULL;
    return image->data + (address - image->base);
}
