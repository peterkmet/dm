#include <stdlib.h>
#include <string.h>
#include "ftl.h"
#include "graphics.h"

#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))

#define GRAPHICS_DAT_FORMAT_1 0x8001
#define GRAPHIC_PAD_WORD 0xff81 /* skipped before the width and height */

/* Graphic command byte: bit 7 clear = short fill of (bits 4-6) + 1 pixels of color (bits 0-3).
   Bit 7 set = a long command: kind in bits 4-5, bit 6 a 16-bit count. */
enum { CMD_LONG = 0x80, CMD_WIDE_COUNT = 0x40, CMD_KIND_MASK = 0x30, CMD_COLOR_MASK = 15 };
enum { KIND_FILL = 0x00, KIND_RAW = 0x10, KIND_SKIP = 0x20, KIND_COPY_ABOVE = 0x30 };
/* Skip lengths 29-31 read the length from the next bytes. */
enum { SKIP_BYTE = 29, SKIP_BYTE_PLUS_256 = 30, SKIP_WORD = 31 };

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
    if (BE16(dat) != GRAPHICS_DAT_FORMAT_1) return -1;
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

/* Packed bytes of item i. */
const uint8_t *gfx_item(int i, uint16_t *size)
{
    if (i < 0 || i >= count) return NULL;
    *size = offsets[i + 1] - offsets[i];
    return dat + offsets[i];
}

/* Skips the pad words and reads the size of a packed graphic. Returns the start of the commands or NULL. */
static const uint8_t *graphic_size(const uint8_t *p, const uint8_t *end, uint16_t *w, uint16_t *h)
{
    while (end - p >= 2 && BE16(p) == GRAPHIC_PAD_WORD) p += 2;
    if (end - p < 4) return NULL;
    *w = BE16(p);
    *h = BE16(p + 2);
    return p + 4;
}

/* Decodes the graphic commands into px (w * h pixels); skip commands leave pixels unchanged. */
static void expand_commands(const uint8_t *p, const uint8_t *end, uint8_t *px, uint16_t w, size_t n)
{
    size_t pos = 0;
#define NEXT() (p < end ? *p++ : 0)
#define PUT(c) do { if (pos < n) px[pos] = (c); pos++; } while (0)
    while (pos < n) {
        uint8_t cmd = NEXT(), c = cmd & CMD_COLOR_MASK;
        unsigned cnt;
        if (!(cmd & CMD_LONG)) {
            for (cnt = (cmd >> 4) + 1; cnt; cnt--) PUT(c);
            continue;
        }
        if ((cmd & CMD_KIND_MASK) == KIND_SKIP) { /* leave pixels transparent */
            unsigned k = c | (cmd & CMD_WIDE_COUNT ? 16 : 0);
            if (k == SKIP_BYTE) k = NEXT();
            else if (k == SKIP_BYTE_PLUS_256) k = 256 + NEXT();
            else if (k == SKIP_WORD) { k = NEXT() << 8; k |= NEXT(); }
            pos += k + 1;
            continue;
        }
        cnt = NEXT();
        if (cmd & CMD_WIDE_COUNT) cnt = cnt << 8 | NEXT();
        cnt++;
        switch (cmd & CMD_KIND_MASK) {
        case KIND_FILL:
            while (cnt--) PUT(c);
            break;
        case KIND_RAW: /* two pixels per byte */
            if (cnt & 1) PUT(c);
            for (cnt /= 2; cnt; cnt--) {
                uint8_t b = NEXT();
                PUT(b >> 4);
                PUT(b & 15);
            }
            break;
        default: /* KIND_COPY_ABOVE, then one pixel of color c */
            for (; cnt; cnt--) PUT(pos >= w && pos < n ? px[pos - w] : 0);
            PUT(c);
        }
    }
#undef NEXT
#undef PUT
}

/* Port of expand_graphic: decodes a packed graphic into a chunky bitmap. */
int gfx_expand(const uint8_t *p, size_t len, Bitmap *bm)
{
    const uint8_t *end = p + len;
    uint16_t w, h;
    if (!(p = graphic_size(p, end, &w, &h))) return -1;
    size_t n = (size_t)w * h;
    uint8_t *px = calloc(n ? n : 1, 1);
    if (!px) return -1;
    expand_commands(p, end, px, w, n);
    *bm = (Bitmap){ w, h, px };
    return 0;
}

/* Decodes a packed graphic over a bitmap of the same size: skipped pixels keep their color. */
int gfx_expand_over(const uint8_t *p, size_t len, Bitmap *bm)
{
    const uint8_t *end = p + len;
    uint16_t w, h;
    if (!(p = graphic_size(p, end, &w, &h)) || w != bm->w || h != bm->h) return -1;
    expand_commands(p, end, bm->px, w, (size_t)w * h);
    return 0;
}

/* Decodes graphic i into a new bitmap. Returns 0 or -1. */
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
