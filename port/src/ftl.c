#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftl.h"

#define FTL_MAX_SIZE (4u << 20)

#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
#define BE32(p) ((uint32_t)(p)[0] << 24 | (uint32_t)(p)[1] << 16 | (uint32_t)(p)[2] << 8 | (p)[3])

FtlImage kaos;

uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf = NULL;
    long n;
    if (!f) return NULL;
    if (!fseek(f, 0, SEEK_END) && (n = ftell(f)) >= 0 && !fseek(f, 0, SEEK_SET) &&
        (buf = malloc(n ? n : 1)) && fread(buf, 1, n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = buf ? (size_t)n : 0;
    return buf;
}

typedef struct { const uint8_t *s; size_t n, pos; } Nibbles;

static unsigned nib(Nibbles *b, int count)
{
    unsigned v = 0;
    for (; count; count--, b->pos++)
        v = v << 4 | (b->pos < b->n ? b->s[b->pos / 2] >> (b->pos % 2 ? 0 : 4) & 15 : 0);
    return v;
}

/* Expands the R# nibble coding of a packed code section. */
static uint8_t *unpack(const uint8_t *p, size_t len, uint32_t *out_len)
{
    if (len < 0xF08 || BE16(p) != 0x5223) return NULL;
    uint32_t words = BE32(p + 4);
    Nibbles b = { p + 0xF08, (len - 0xF08) * 2, 0 };
    uint8_t *out = malloc((size_t)words * 2 + 1);
    if (!out) return NULL;
    for (uint32_t i = 0; i < words; i++) {
        unsigned v = nib(&b, 1), w;
        if (v == 15) w = nib(&b, 4);
        else {
            unsigned t = v < 8 ? v << 4 | nib(&b, 1) : (v << 8 | nib(&b, 2)) - 0x780;
            w = BE16(p + 8 + 2 * t);
        }
        out[2 * i] = w >> 8;
        out[2 * i + 1] = w;
    }
    if (b.pos > b.n) { free(out); return NULL; }
    *out_len = words * 2;
    return out;
}

static uint16_t sum8(const uint8_t *p, size_t n)
{
    uint32_t s = 0;
    while (n--) s += *p++;
    return s;
}

/* Lays out an FTL module in memory at base like BJELoad_R. Returns 0 or -1. */
int ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img)
{
    const uint8_t *sec[3] = { 0 };
    uint32_t secsz[3] = { 0 }, count, s = 0, csize;
    uint8_t *code = NULL, *mem = NULL;
    if (len < 0x14 || BE16(raw) != 0x6160 || BE16(raw + 4) != 2 || raw[6] != 1 || raw[7] != 0) return -1;
    count = BE16(raw + 0x12);
    if (count >= 0x65 || len < 0x14 + count * 12) return -1;
    for (int i = 4; i < 0x14; i++) s += raw[i] * i;
    for (uint32_t i = 0; i < count * 12; i++) s += raw[0x14 + i] * ((i & 0xFF) + 1);
    if ((uint16_t)s != BE16(raw + 2)) return -1;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = raw + 0x14 + i * 12;
        uint16_t tag = BE16(e);
        uint32_t off = BE32(e + 4), size = BE32(e + 8);
        if (tag < 0x10 || tag > 0x12) continue;
        if (BE16(e + 2) != 1 || sec[tag - 0x10] || off > len || size > len - off) return -1;
        sec[tag - 0x10] = raw + off;
        secsz[tag - 0x10] = size;
    }
    const uint8_t *info = sec[0], *data = sec[1];
    if (!info || !data || !sec[2] || secsz[0] < 0x28 || secsz[1] < 10) return -1;

    uint32_t region = BE32(info), dsize = BE32(info + 4), jt_off = BE32(info + 0xC);
    csize = BE32(info + 0x14);
    uint16_t info_chk = BE16(info + 0x22), ws = 0;
    for (uint32_t i = 0; i + 1 < secsz[0]; i += 2) ws += BE16(info + i);
    if ((uint16_t)(ws - info_chk) != info_chk || sum8(data, secsz[1]) != BE16(info + 0x26)) return -1;
    uint32_t code_len = secsz[2];
    const uint8_t *codep = sec[2];
    if (BE16(info + 0x1E) && !(codep = code = unpack(sec[2], secsz[2], &code_len))) return -1;
    if (code_len < csize || sum8(codep, csize) != BE16(info + 0x24)) goto fail;

    uint64_t data_base = base + ((dsize + 1ull) & ~1ull), code_base = (data_base + region + 1) & ~1ull;
    uint64_t size = code_base - base + csize;
    uint32_t rle_len = BE32(data + 2), reloc_len = BE32(data + 6);
    if (size > FTL_MAX_SIZE || base + size > UINT32_MAX || rle_len > secsz[1] - 10 || reloc_len > secsz[1] - 10 - rle_len || !(mem = calloc(size + 1, 1))) goto fail;
    const uint8_t *r = data + 10, *rend = r + rle_len;
    uint32_t n = 0;
    while (r + 1 < rend) { /* word copy; 0x0000 is followed by a count of extra zero bytes */
        int zero = !r[0] && !r[1];
        if (n + 2 > size) goto fail;
        mem[n++] = *r++;
        mem[n++] = *r++;
        if (zero && r + 1 < rend) { n += BE16(r); r += 2; }
    }
    if (n != dsize) goto fail;
    for (const uint8_t *p = rend; p + 1 < rend + reloc_len; p += 2) {
        int64_t at = (int64_t)(data_base - base) + (int16_t)BE16(p);
        if (at < 0 || (uint64_t)at + 4 > size) goto fail;
        uint32_t v = BE32(mem + at) + data_base;
        for (int k = 0; k < 4; k++) mem[at + k] = v >> (24 - 8 * k);
    }
    for (uint32_t i = 0; 0x28 + i * 4 + 4 <= secsz[0]; i++) {
        uint64_t at = data_base - base + jt_off + i * 8 + 2;
        uint32_t v = code_base + BE32(info + 0x28 + i * 4);
        if (at + 6 > size) goto fail;
        uint8_t jmp[6] = { 0x4E, 0xF9, v >> 24, v >> 16, v >> 8, v };
        memcpy(mem + at, jmp, 6);
    }
    memcpy(mem + code_base - base, codep, csize);
    free(code);
    *img = (FtlImage){ mem, (uint32_t)size, base, (uint32_t)data_base, (uint32_t)code_base };
    return 0;
fail:
    free(code);
    free(mem);
    return -1;
}

void ftl_free(FtlImage *img)
{
    free(img->data);
    *img = (FtlImage){ 0 };
}

int kaos_load(const char *path)
{
    size_t len;
    uint8_t *raw = read_file(path, &len);
    ftl_free(&kaos);
    int r = raw ? ftl_load(raw, len, KAOS_BASE, &kaos) : -1;
    free(raw);
    return r;
}

/* Maps an original KAOS address (0x10xxxx) into the loaded image. */
static uint8_t *at(uint32_t addr, uint32_t n)
{
    if (!kaos.data || addr < kaos.base || (uint64_t)addr - kaos.base + n > kaos.size) {
        fprintf(stderr, "kaos address %#x outside image\n", addr);
        abort();
    }
    return kaos.data + (addr - kaos.base);
}

uint8_t *kaos_ptr(uint32_t addr) { return at(addr, 1); }
uint8_t kaos_u8(uint32_t addr) { return *at(addr, 1); }
uint16_t kaos_u16(uint32_t addr) { return BE16(at(addr, 2)); }
int16_t kaos_s16(uint32_t addr) { return (int16_t)kaos_u16(addr); }
uint32_t kaos_u32(uint32_t addr) { return BE32(at(addr, 4)); }
