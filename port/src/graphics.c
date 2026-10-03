#include <stdlib.h>
#include <string.h>
#include "ftl.h"
#include "graphics.h"

#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))

static uint8_t *dat;
static size_t dat_len;
static uint16_t count;
static uint32_t *offsets; /* count + 1 file offsets */
static Bitmap *decoded;   /* count bitmaps, decoded on first use */

/* Reads a format 1 Graphics.DAT like graphics_dat_read_header. Returns the item count or -1. */
int gfx_open(const char *path)
{
    for (int i = 0; decoded && i < count; i++)
        free(decoded[i].px);
    free(decoded);
    free(dat);
    free(offsets);
    offsets = NULL;
    decoded = NULL;
    count = 0;
    if (!(dat = read_file(path, &dat_len)) || dat_len < 4) return -1;
    if (BE16(dat) != 0x8001) return -1;
    uint16_t n = BE16(dat + 2);
    uint32_t off = 4 + n * 8u;
    const uint8_t *sizes = dat + 4;
    if (off > dat_len || !(offsets = malloc((n + 1) * sizeof *offsets))) return -1;
    for (int i = 0; i < n; i++) {
        offsets[i] = off;
        off += BE16(sizes + 2 * i);
    }
    offsets[n] = off;
    if (off > dat_len || !(decoded = calloc(n, sizeof *decoded))) return -1;
    return count = n;
}

int gfx_count(void) { return count; }

/* Raw (packed) bytes of item i. */
const uint8_t *gfx_item(int i, uint16_t *size)
{
    if (i < 0 || i >= count) return NULL;
    *size = offsets[i + 1] - offsets[i];
    return dat + offsets[i];
}

/* Port of expand_graphic: decodes a packed graphic into a chunky bitmap. */
int gfx_expand(const uint8_t *p, size_t len, Bitmap *bm)
{
    const uint8_t *end = p + len;
    while (end - p >= 2 && BE16(p) == 0xff81) p += 2;
    if (end - p < 4) return -1;
    uint16_t w = BE16(p), h = BE16(p + 2);
    size_t n = (size_t)w * h, pos = 0;
    uint8_t *px = calloc(n ? n : 1, 1);
    if (!px) return -1;
    p += 4;
#define NEXT() (p < end ? *p++ : 0)
#define PUT(c) do { if (pos < n) px[pos] = (c); pos++; } while (0)
    while (pos < n) {
        uint8_t cmd = NEXT(), c = cmd & 15;
        unsigned cnt;
        if (!(cmd & 0x80)) {
            for (cnt = (cmd >> 4) + 1; cnt; cnt--) PUT(c);
            continue;
        }
        if ((cmd & 0x30) == 0x20) { /* skip */
            unsigned k = c | (cmd & 0x40 ? 16 : 0);
            if (k == 29) k = NEXT();
            else if (k == 30) k = 256 + NEXT();
            else if (k == 31) { k = NEXT() << 8; k |= NEXT(); }
            pos += k + 1;
            continue;
        }
        cnt = NEXT();
        if (cmd & 0x40) cnt = cnt << 8 | NEXT();
        cnt++;
        switch (cmd & 0x30) {
        case 0x00: /* fill */
            while (cnt--) PUT(c);
            break;
        case 0x10: /* raw pixels, two per byte */
            if (cnt & 1) PUT(c);
            for (cnt /= 2; cnt; cnt--) {
                uint8_t b = NEXT();
                PUT(b >> 4);
                PUT(b & 15);
            }
            break;
        default: /* copy from the row above, then one pixel of color c */
            for (; cnt; cnt--) PUT(pos >= w && pos < n ? px[pos - w] : 0);
            PUT(c);
        }
    }
#undef NEXT
#undef PUT
    *bm = (Bitmap){ w, h, px };
    return 0;
}

int gfx_load_bitmap(int i, Bitmap *bm)
{
    uint16_t size;
    const uint8_t *p = gfx_item(i, &size);
    return p ? gfx_expand(p, size, bm) : -1;
}

void gfx_free_bitmap(Bitmap *bm)
{
    free(bm->px);
    bm->px = NULL;
}

/* Decoded graphic i, kept until the next gfx_open. NULL when it does not decode. */
const Bitmap *gfx_bitmap(int i)
{
    if (i < 0 || i >= count) return NULL;
    if (!decoded[i].px && gfx_load_bitmap(i, &decoded[i])) return NULL;
    return &decoded[i];
}
