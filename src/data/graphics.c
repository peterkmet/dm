/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/graphics.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/csb_text.h"
#include "data/file.h"
#include "data/ids.h"
#include <stdlib.h>
#include <string.h>

#define GRAPHICS_DAT_FORMAT_1 0x8001
#define GRAPHIC_PAD_WORD      0xff81 /* skipped before the width and height */

/* Graphic command byte: bit 7 clear = short fill of (bits 4-6) + 1 pixels of color (bits 0-3).
   Bit 7 set = a long command: kind in bits 4-5, bit 6 a 16-bit count. */
#define CMD_LONG            0x80
#define CMD_WIDE_COUNT_FLAG 0x40
#define CMD_KIND_MASK       0x30
#define CMD_COLOR_MASK      0xf
#define KIND_FILL           0x00
#define KIND_RAW            0x10
#define KIND_SKIP           0x20
#define KIND_COPY_ABOVE     0x30
/* Skip lengths 29-31 read the length from the next bytes. Bit 6 of a skip adds 16 to its length. */
#define SKIP_BYTE           29
#define SKIP_BYTE_PLUS_256  30
#define SKIP_WORD           31
#define SKIP_WIDE           16
#define SKIP_BYTE_BASE      256

/* Graphics.DAT header: format word and item count, then the size words of the items. */
#define GFX_HEADER_BYTES      4
#define GFX_ITEM_ENTRY_BYTES  8
#define GFX_HEADER_ITEM_COUNT 2

typedef struct {
    char *data;
    const char **s;
    int16_t count;
} StringList;

static struct {
    uint8_t *data;
    size_t size;
    uint16_t item_count;
    uint32_t *offsets; /* item_count + 1 file offsets */
    Bitmap *decoded;   /* item_count bitmaps, decoded on first use */
    StringList strings, action_names, object_names;
} graphics;

bool box_contains(const Box *box, int16_t x, int16_t y)
{
    return x >= box->x && x < box->x + box->w && y >= box->y && y < box->y + box->h;
}

void gfx_close(void)
{
    for (int i = 0; graphics.decoded && i < graphics.item_count; i++)
        free(graphics.decoded[i].px);
    free(graphics.decoded);
    free(graphics.data);
    free(graphics.offsets);
    graphics.decoded = NULL;
    graphics.data = NULL;
    graphics.offsets = NULL;
    graphics.size = 0;
    graphics.item_count = 0;
}

bool gfx_open(const char *path)
{
    gfx_close();
    if (!(graphics.data = file_read(path, &graphics.size)) || graphics.size < GFX_HEADER_BYTES)
        goto fail;
    if (be16(graphics.data) != GRAPHICS_DAT_FORMAT_1)
        goto fail;
    uint16_t n = be16(graphics.data + GFX_HEADER_ITEM_COUNT);
    if (!n)
        goto fail;
    uint32_t off = GFX_HEADER_BYTES + n * (uint32_t)GFX_ITEM_ENTRY_BYTES;
    const uint8_t *sizes = graphics.data + GFX_HEADER_BYTES;
    if (off > graphics.size || !(graphics.offsets = malloc((n + 1) * sizeof *graphics.offsets)))
        goto fail;
    for (int i = 0; i < n; i++) {
        graphics.offsets[i] = off;
        off += be16(sizes + 2 * i);
    }
    graphics.offsets[n] = off;
    if (off > graphics.size || !(graphics.decoded = calloc(n, sizeof *graphics.decoded)))
        goto fail;
    graphics.item_count = n;
    return true;
fail:
    gfx_close();
    return false;
}

/* exported for the tests */
int gfx_count(void)
{
    return graphics.item_count;
}

const uint8_t *gfx_get_item(int i, uint16_t *size)
{
    if (i < 0 || i >= graphics.item_count)
        return NULL;
    *size = graphics.offsets[i + 1] - graphics.offsets[i];
    return graphics.data + graphics.offsets[i];
}

/* Skips the pad words and reads the size of a packed graphic. Returns the start of the commands or NULL. */
static const uint8_t *get_graphic_size(const uint8_t *p, const uint8_t *end, uint16_t *w, uint16_t *h)
{
    while (end - p >= 2 && be16(p) == GRAPHIC_PAD_WORD)
        p += 2;
    if (end - p < 4)
        return NULL;
    *w = be16(p);
    *h = be16(p + 2);
    return p + 4;
}

typedef struct {
    const uint8_t *p, *end;
} ByteReader;

typedef struct {
    uint8_t *px;
    size_t pos, n;
} PixelWriter;

/* Reads the next command byte; past the end it reads zeros. */
static inline uint8_t next_byte(ByteReader *r)
{
    return r->p < r->end ? *r->p++ : 0;
}

/* Writes the next pixel; past the bitmap it only counts. */
static inline void put_pixel(PixelWriter *w, uint8_t c)
{
    if (w->pos < w->n)
        w->px[w->pos] = c;
    w->pos++;
}

/* Decodes the graphic commands into px (w * h pixels); skip commands leave pixels unchanged. */
static void expand_commands(const uint8_t *p, const uint8_t *end, uint8_t *px, uint16_t w, size_t n)
{
    ByteReader in = {p, end};
    PixelWriter out = {px, 0, n};
    while (out.pos < n) {
        uint8_t cmd = next_byte(&in);
        uint8_t c = cmd & CMD_COLOR_MASK;
        unsigned cnt;
        if (!(cmd & CMD_LONG)) {
            for (cnt = (cmd >> 4) + 1; cnt; cnt--)
                put_pixel(&out, c);
            continue;
        }
        if ((cmd & CMD_KIND_MASK) == KIND_SKIP) { /* leave pixels transparent */
            unsigned k = c | (cmd & CMD_WIDE_COUNT_FLAG ? SKIP_WIDE : 0);
            if (k == SKIP_BYTE)
                k = next_byte(&in);
            else if (k == SKIP_BYTE_PLUS_256)
                k = SKIP_BYTE_BASE + next_byte(&in);
            else if (k == SKIP_WORD) {
                k = next_byte(&in) << BYTE_BITS;
                k |= next_byte(&in);
            }
            out.pos += k + 1;
            continue;
        }
        cnt = next_byte(&in);
        if (cmd & CMD_WIDE_COUNT_FLAG)
            cnt = cnt << BYTE_BITS | next_byte(&in);
        cnt++;
        switch (cmd & CMD_KIND_MASK) {
        case KIND_FILL:
            while (cnt--)
                put_pixel(&out, c);
            break;
        case KIND_RAW: /* two pixels per byte */
            if (cnt & 1)
                put_pixel(&out, c);
            for (cnt /= 2; cnt; cnt--) {
                uint8_t b = next_byte(&in);
                put_pixel(&out, b >> 4);
                put_pixel(&out, b & CMD_COLOR_MASK);
            }
            break;
        default: /* KIND_COPY_ABOVE, then one pixel of color c */
            for (; cnt; cnt--)
                put_pixel(&out, out.pos >= w && out.pos < n ? px[out.pos - w] : 0);
            put_pixel(&out, c);
        }
    }
}

bool gfx_expand(const uint8_t *p, size_t len, Bitmap *bm)
{
    const uint8_t *end = p + len;
    uint16_t w, h;
    if (!(p = get_graphic_size(p, end, &w, &h)))
        return false;
    size_t n = (size_t)w * h;
    uint8_t *px = calloc(n ? n : 1, 1);
    if (!px)
        return false;
    expand_commands(p, end, px, w, n);
    *bm = (Bitmap){w, h, px};
    return true;
}

bool gfx_expand_over(const uint8_t *p, size_t len, Bitmap *bm)
{
    const uint8_t *end = p + len;
    uint16_t w, h;
    if (!(p = get_graphic_size(p, end, &w, &h)) || w != bm->w || h != bm->h)
        return false;
    expand_commands(p, end, bm->px, w, (size_t)w * h);
    return true;
}

bool gfx_load_bitmap(int16_t i, Bitmap *bm)
{
    uint16_t size;
    const uint8_t *p = gfx_get_item(i, &size);
    return p && gfx_expand(p, size, bm);
}

void gfx_free_bitmap(Bitmap *bm)
{
    free(bm->px);
    bm->px = NULL;
}

const Bitmap *gfx_get_bitmap(int i)
{
    if (i < 0 || i >= graphics.item_count)
        return NULL;
    if (!graphics.decoded[i].px && !gfx_load_bitmap(i, &graphics.decoded[i]))
        return NULL;
    return &graphics.decoded[i];
}

#define NAME_END_BIT 0x80 /* CSB object names: set in the last character */

/* Splits a graphic of NUL-terminated strings into a list. CSB takes the strings and action names from
   the engine, and ends each object name at a byte with bit 7 set. */
static bool load_strings(int graphic, StringList *list)
{
    uint16_t size;
    bool csb = g_adventure == ADVENTURE_CSB;
    bool bit7_ends = csb && graphic == GFX_OBJECT_NAMES;
    const uint8_t *p = csb ? csb_text_get_item(graphic, &size) : NULL;
    if (!p)
        p = gfx_get_item(graphic, &size);
    if (!p)
        return false;
    free(list->data);
    free(list->s);
    list->data = malloc(2 * (size_t)size + 1);
    list->s = malloc(((size_t)size + 1) * sizeof *list->s);
    if (!list->data || !list->s)
        return false;
    size_t n = 0;
    for (uint16_t i = 0; i < size; i++) {
        list->data[n++] = (char)(bit7_ends ? p[i] & ~NAME_END_BIT : p[i]);
        if (bit7_ends && p[i] & NAME_END_BIT)
            list->data[n++] = 0;
    }
    list->data[n] = 0;
    list->count = 0;
    for (char *c = list->data; c < list->data + n; c += strlen(c) + 1)
        list->s[list->count++] = c;
    return true;
}

/* Loads the strings, action names, and object names. */
bool gfx_load_strings(void)
{
    return load_strings(GFX_STRINGS, &graphics.strings) && load_strings(GFX_ACTION_NAMES, &graphics.action_names) &&
           load_strings(GFX_OBJECT_NAMES, &graphics.object_names);
}

static const char *list_get(const StringList *l, int16_t i)
{
    return i >= 0 && i < l->count ? l->s[i] : "";
}
const char *gfx_string(int16_t index)
{
    return list_get(&graphics.strings, index);
}
const char *gfx_object_name(int16_t icon)
{
    return list_get(&graphics.object_names, icon);
}
const char *gfx_action_name(int16_t action)
{
    return list_get(&graphics.action_names, action);
}
