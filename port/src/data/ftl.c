#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adventure.h"
#include "bytes.h"
#include "csbimage.h"
#include "file.h"
#include "ftl.h"
#include "kaos_addresses.h"

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
enum { SECTION_INFO = 16, SECTION_DATA, SECTION_CODE, SECTION_COUNT = 3 };

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

FtlImage kaos;

typedef struct {
    const uint8_t *s;
    size_t n, pos;
} Nibbles;

/* Reads count nibbles, high nibble first; past the end it reads zeros. */
static unsigned read_nibble(Nibbles *b, int count)
{
    unsigned v = 0;
    for (; count; count--, b->pos++)
        v = v << NIBBLE_BITS | (b->pos < b->n ? b->s[b->pos / 2] >> (b->pos % 2 ? 0 : NIBBLE_BITS) & NIBBLE_MASK : 0);
    return v;
}

/* Expands the R# nibble coding of a packed code section. Codes 0-7 take one more nibble and
   15 is a literal word; the other codes take two more nibbles. */
static uint8_t *unpack(const uint8_t *p, size_t len, uint32_t *out_len)
{
    if (len < RSHARP_STREAM || BE16(p) != RSHARP_MAGIC)
        return NULL;
    uint32_t words = BE32(p + RSHARP_WORD_COUNT);
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
            w = BE16(p + RSHARP_DICTIONARY + 2 * t);
        }
        WR16(out + 2 * i, w);
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

/* Checks the file header and finds the info, data, and code sections. Returns 0 or -1. */
static int find_sections(const uint8_t *raw, size_t len, const uint8_t *sec[SECTION_COUNT],
                         uint32_t secsz[SECTION_COUNT])
{
    uint32_t count, s = 0;
    if (len < FTL_HEADER_BYTES || BE16(raw) != FTL_MAGIC || BE16(raw + FTL_HEADER_VERSION) != 2 ||
        raw[FTL_HEADER_VERSION + 2] != 1 || raw[FTL_HEADER_VERSION + 3] != 0)
        return -1;
    count = BE16(raw + FTL_HEADER_ENTRY_COUNT);
    if (count > FTL_MAX_ENTRIES || len < FTL_HEADER_BYTES + count * FTL_ENTRY_BYTES)
        return -1;
    for (int i = FTL_HEADER_VERSION; i < FTL_HEADER_BYTES; i++)
        s += raw[i] * i;
    for (uint32_t i = 0; i < count * FTL_ENTRY_BYTES; i++)
        s += raw[FTL_HEADER_BYTES + i] * (i % FTL_CHECKSUM_WEIGHTS + 1);
    if ((uint16_t)s != BE16(raw + FTL_HEADER_CHECKSUM))
        return -1; /* the checksum is the low word of the sum */
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = raw + FTL_HEADER_BYTES + i * FTL_ENTRY_BYTES;
        uint16_t tag = BE16(e);
        uint32_t off = BE32(e + 4);
        uint32_t size = BE32(e + 8);
        if (tag < SECTION_INFO || tag > SECTION_CODE)
            continue;
        if (BE16(e + 2) != 1 || sec[tag - SECTION_INFO] || off > len || size > len - off)
            return -1;
        sec[tag - SECTION_INFO] = raw + off;
        secsz[tag - SECTION_INFO] = size;
    }
    return 0;
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
            n += BE16(r);
            r += 2;
        }
    }
    return n;
}

/* Adds data_base to each long word that the relocation table names. Returns 0 or -1. */
static int relocate(const uint8_t *table, uint32_t reloc_len, uint8_t *mem, uint64_t size, uint64_t data_offset,
                    uint32_t data_base)
{
    for (const uint8_t *p = table; p + 1 < table + reloc_len; p += 2) {
        int64_t at = (int64_t)data_offset + (int16_t)BE16(p); /* signed offset */
        if (at < 0 || (uint64_t)at + 4 > size)
            return -1;
        uint32_t v = BE32(mem + at) + data_base;
        WR32(mem + at, v);
    }
    return 0;
}

/* Writes a jmp to each entry point into the jump table. Returns 0 or -1. */
static int write_jump_table(const uint8_t *info, uint32_t info_size, uint8_t *mem, uint64_t size, uint64_t table_offset,
                            uint32_t code_base)
{
    for (uint32_t i = 0; INFO_ENTRY_POINTS + i * 4 + 4 <= info_size; i++) {
        uint64_t at = table_offset + i * JUMP_ENTRY_BYTES + JUMP_ENTRY_JMP;
        uint32_t v = code_base + BE32(info + INFO_ENTRY_POINTS + i * 4);
        if (at + JMP_ABS_LONG_BYTES > size)
            return -1;
        WR16(mem + at, JMP_ABS_LONG_OPCODE);
        WR32(mem + at + 2, v);
    }
    return 0;
}

/* Lays out an FTL module in memory at base like BJELoad_R: data, region, code. Returns 0 or -1. */
int ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img)
{
    const uint8_t *sec[SECTION_COUNT] = {0};
    uint32_t secsz[SECTION_COUNT] = {0};
    uint8_t *code = NULL;
    uint8_t *mem = NULL;
    if (find_sections(raw, len, sec, secsz))
        return -1;
    const uint8_t *info = sec[0];
    const uint8_t *data = sec[1];
    if (!info || !data || !sec[2] || secsz[0] < INFO_ENTRY_POINTS || secsz[1] < DATA_HEADER_BYTES)
        return -1;

    uint32_t region = BE32(info + INFO_REGION_SIZE);
    uint32_t dsize = BE32(info + INFO_DATA_SIZE);
    uint32_t jt_off = BE32(info + INFO_JUMP_TABLE);
    uint32_t csize = BE32(info + INFO_CODE_SIZE);
    uint16_t info_chk = BE16(info + INFO_CHECKSUM);
    uint16_t ws = 0;
    for (uint32_t i = 0; i + 1 < secsz[0]; i += 2)
        ws += BE16(info + i);
    if ((uint16_t)(ws - info_chk) != info_chk || sum8(data, secsz[1]) != BE16(info + INFO_DATA_SUM))
        return -1; /* 16-bit wrap */
    uint32_t code_len = secsz[2];
    const uint8_t *codep = sec[2];
    if (BE16(info + INFO_CODE_PACKED)) {
        code = unpack(sec[2], secsz[2], &code_len);
        if (!code)
            return -1;
        codep = code;
    }
    if (code_len < csize || sum8(codep, csize) != BE16(info + INFO_CODE_SUM))
        goto fail;

    uint64_t data_base = base + ((dsize + 1ull) & ~1ull);
    uint64_t code_base = (data_base + region + 1) & ~1ull;
    uint64_t size = code_base - base + csize;
    uint32_t rle_len = BE32(data + DATA_RLE_SIZE);
    uint32_t reloc_len = BE32(data + DATA_RELOC_SIZE);
    uint32_t tables_len = secsz[1] - DATA_HEADER_BYTES;
    if (size > FTL_MAX_SIZE || base + size > UINT32_MAX || rle_len > tables_len || reloc_len > tables_len - rle_len ||
        !(mem = calloc(size + 1, 1)))
        goto fail;
    const uint8_t *rle = data + DATA_HEADER_BYTES;
    if (expand_data(rle, rle_len, mem, size) != dsize ||
        relocate(rle + rle_len, reloc_len, mem, size, data_base - base, (uint32_t)data_base) ||
        write_jump_table(info, secsz[0], mem, size, data_base - base + jt_off, (uint32_t)code_base))
        goto fail;
    memcpy(mem + code_base - base, codep, csize);
    free(code);
    *img = (FtlImage){mem, (uint32_t)size, base, (uint32_t)data_base, (uint32_t)code_base};
    return 0;
fail:
    free(code);
    free(mem);
    return -1;
}

void ftl_free(FtlImage *img)
{
    free(img->data);
    *img = (FtlImage){0};
}

int kaos_load(const char *path)
{
    size_t len;
    uint8_t *raw = file_read(path, &len);
    FtlImage appb = {0};
    bool csb = g_adventure == ADVENTURE_CSB;
    ftl_free(&kaos);
    int r = raw ? ftl_load(raw, len, KAOS_BASE, csb ? &appb : &kaos) : -1;
    free(raw);
    if (!r && csb)
        r = csb_image_build(&appb, &kaos);
    ftl_free(&appb);
    return r;
}

/* Maps an original KAOS address into the loaded image; aborts outside the image. */
static uint8_t *get_image_pointer(uint32_t addr, uint32_t n)
{
    if (!kaos.data || addr < kaos.base || (uint64_t)addr - kaos.base + n > kaos.size) {
        fprintf(stderr, "kaos address %#x outside image\n", addr);
        abort();
    }
    return kaos.data + (addr - kaos.base);
}

uint8_t *kaos_ptr(uint32_t addr)
{
    return get_image_pointer(addr, 1);
}
uint8_t kaos_u8(uint32_t addr)
{
    return *get_image_pointer(addr, 1);
}
uint16_t kaos_u16(uint32_t addr)
{
    return BE16(get_image_pointer(addr, 2));
}
int16_t kaos_s16(uint32_t addr)
{
    return kaos_u16(addr);
}
uint32_t kaos_u32(uint32_t addr)
{
    return BE32(get_image_pointer(addr, 4));
}
