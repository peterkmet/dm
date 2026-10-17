/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/layout.h"
#include "data/bytes.h"
#include "data/graphics.h"
#include "data/ids.h"

/* First word of the layout data. */
#define LAYOUT_SIGNATURE 0xfc0d
#define S16(p, i)        ((int16_t)be16((p) + 2 * (i))) /* signed word i */

/* Zone record types: 0-8 a point with an alignment, 9 a size, 10-18 a point inside the rectangle of
   another point, with the alignment type - 10. Type 1 with a size parent is a translation. */
#define ALIGN_CENTER            0
#define ALIGN_TOP_LEFT          1
#define ALIGN_TOP_RIGHT         2
#define ALIGN_BOTTOM_RIGHT      3
#define ALIGN_BOTTOM_LEFT       4
#define ALIGN_TOP_CENTER        5
#define ALIGN_CENTER_RIGHT      6
#define ALIGN_BOTTOM_CENTER     7
#define ALIGN_CENTER_LEFT       8
#define RECORD_TRANSLATION      1
#define RECORD_SIZE             9
#define RECORD_INNER_POINT      10
#define RECORD_INNER_POINT_LAST 18
#define CLIP_NONE               20000
/* The layout graphic: signature and range count words, ranges of 2 words, then zone records of 4 words. */
#define LAYOUT_HEADER_BYTES     4
#define RANGE_BYTES             4
#define ZONE_RECORD_BYTES       8

static const uint8_t *layout_data; /* graphic 696: signature, range count, ranges, zone records */
static uint16_t layout_size;

/* Words of a zone record. */
static int16_t get_zone_type(const uint8_t *r)
{
    return S16(r, 0);
}
static int16_t get_zone_parent(const uint8_t *r)
{
    return S16(r, 1);
}
static int16_t get_zone_x(const uint8_t *r)
{
    return S16(r, 2);
} /* or the width of a size record */
static int16_t get_zone_y(const uint8_t *r)
{
    return S16(r, 3);
} /* or the height of a size record */

bool layout_init(void)
{
    layout_data = gfx_get_item(GFX_LAYOUT, &layout_size);
    if (!layout_data || layout_size < LAYOUT_HEADER_BYTES || be16(layout_data) != LAYOUT_SIGNATURE ||
        LAYOUT_HEADER_BYTES + RANGE_BYTES * S16(layout_data, 1) > layout_size) {
        layout_data = NULL;
        return false;
    }
    return true;
}

/* The 4-word record (type, parent, x, y) of zone n, or NULL. */
static const uint8_t *find(int16_t n)
{
    if (!layout_data || n == 0)
        return NULL;
    int16_t ranges = S16(layout_data, 1);
    const uint8_t *rec = layout_data + LAYOUT_HEADER_BYTES + RANGE_BYTES * ranges;
    for (int16_t i = 0; i < ranges; i++) {
        int16_t first = S16(layout_data, 2 + 2 * i);
        int16_t last = S16(layout_data, 3 + 2 * i);
        if (first <= n && n <= last)
            return rec + ZONE_RECORD_BYTES * (n - first) + ZONE_RECORD_BYTES <= layout_data + layout_size
                       ? rec + ZONE_RECORD_BYTES * (n - first)
                       : NULL;
        rec += ZONE_RECORD_BYTES * (last - first + 1);
    }
    return NULL;
}

/* Top left corner of a width x height rectangle whose alignment point is at (*x, *y). */
static void align(int16_t alignment, int16_t width, int16_t height, int16_t *x, int16_t *y)
{
    int16_t cx = (width + 1) >> 1;
    int16_t cy = (height + 1) >> 1;
    switch (alignment) {
    case ALIGN_CENTER:
        *x -= cx;
        *y -= cy;
        break;
    case ALIGN_TOP_RIGHT:
        *x = *x + 1 - width;
        break;
    case ALIGN_BOTTOM_RIGHT:
        *x = *x + 1 - width;
        *y = *y + 1 - height;
        break;
    case ALIGN_BOTTOM_LEFT:
        *y = *y + 1 - height;
        break;
    case ALIGN_TOP_CENTER:
        *x -= cx;
        break;
    case ALIGN_CENTER_RIGHT:
        *x = *x + 1 - width;
        *y -= cy;
        break;
    case ALIGN_BOTTOM_CENTER:
        *x -= cx;
        *y = *y + 1 - height;
        break;
    case ALIGN_CENTER_LEFT:
        *y -= cy;
        break;
    }
}

/* The alignment point of a width x height rectangle with its top left corner at (*x, *y). */
static void anchor(int16_t alignment, int16_t width, int16_t height, int16_t *x, int16_t *y)
{
    int16_t cx = (width + 1) >> 1;
    int16_t cy = (height + 1) >> 1;
    switch (alignment) {
    case ALIGN_CENTER:
        *x += cx;
        *y += cy;
        break;
    case ALIGN_TOP_RIGHT:
        *x += width - 1;
        break;
    case ALIGN_BOTTOM_RIGHT:
        *x += width - 1;
        *y += height - 1;
        break;
    case ALIGN_BOTTOM_LEFT:
        *y += height - 1;
        break;
    case ALIGN_TOP_CENTER:
        *x += cx;
        break;
    case ALIGN_CENTER_RIGHT:
        *x += width - 1;
        *y += cy;
        break;
    case ALIGN_BOTTOM_CENTER:
        *x += cx;
        *y += height - 1;
        break;
    case ALIGN_CENTER_LEFT:
        *y += cy;
        break;
    }
}

/* Intersects the clip box with the rectangle (x, y, w, h), as the zone placement does. */
static void clip_to(Box *c, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (c->x < x)
        c->x = x;
    if (x + w <= c->w + c->x - 1)
        c->w = x + w - c->x;
    if (c->y < y)
        c->y = y;
    if (y + h <= c->h + c->y - 1)
        c->h = y + h - c->y;
}

/* The parent walk of the zone placement: moves the point (*x, *y) and the clip box by the translations and the
   rectangles of the parents of the zone record. Returns false when nothing is visible. */
static bool place_in_parents(const uint8_t *record, int16_t *x, int16_t *y, Box *clip)
{
    const uint8_t *parent, *grandparent;
    int16_t pending = 0;
    while (get_zone_parent(record)) {
        int16_t type = get_zone_type(record);
        if (type < RECORD_INNER_POINT || type > RECORD_INNER_POINT_LAST) {
            if (!(parent = find(get_zone_parent(record))))
                break;
            int16_t point_x = get_zone_x(parent);
            int16_t point_y = get_zone_y(parent);
            if (get_zone_type(parent) == RECORD_TRANSLATION) {
                *x += point_x;
                *y += point_y;
                clip->x += point_x;
                clip->y += point_y;
            } else if (get_zone_type(parent) == RECORD_SIZE) { /* the rectangle of the point record */
                if (type < RECORD_SIZE) {
                    point_x = get_zone_x(record);
                    point_y = get_zone_y(record);
                    align(type, get_zone_x(parent), get_zone_y(parent), &point_x, &point_y);
                }
                if (pending) {
                    pending = 0;
                    *x += point_x;
                    *y += point_y;
                    clip->x += point_x;
                    clip->y += point_y;
                }
                clip_to(clip, point_x, point_y, get_zone_x(parent), get_zone_y(parent));
            } else if (get_zone_type(parent) < RECORD_SIZE) {
                pending = 1;
            }
            record = parent;
        } else { /* a point inside the rectangle of another point */
            if (!(parent = find(get_zone_parent(record))) || !(grandparent = find(get_zone_parent(parent))))
                break;
            int16_t point_x = get_zone_x(parent);
            int16_t point_y = get_zone_y(parent);
            int16_t rect_width = get_zone_x(grandparent);
            int16_t rect_height = get_zone_y(grandparent);
            if (get_zone_type(parent) >= RECORD_SIZE)
                return false;
            align(get_zone_type(parent), rect_width, rect_height, &point_x, &point_y);
            clip->x += point_x;
            clip->y += point_y;
            clip_to(clip, point_x, point_y, rect_width, rect_height);
            anchor(type - RECORD_INNER_POINT, rect_width, rect_height, &point_x, &point_y);
            *x += point_x + get_zone_x(record);
            *y += point_y + get_zone_y(record);
            record = grandparent;
        }
    }
    return true;
}

bool layout_get_zone(int16_t zone, const ZoneItem *sizes, ZoneClip *placed)
{
    if (zone == -1)
        return false;
    int shifted = zone & ZONE_SHIFTED;
    const uint8_t *record = find(zone & (ZONE_SHIFTED - 1));
    if (!record)
        return false;
    Box clip = {0, 0, CLIP_NONE, CLIP_NONE};
    int16_t type = get_zone_type(record);
    int16_t x = 0;
    int16_t y = 0;
    if (type < RECORD_SIZE) {
        x = get_zone_x(record);
        y = get_zone_y(record);
    } else {
        if (type == RECORD_SIZE)
            return false;
        type -= RECORD_INNER_POINT;
    }
    if (type < ALIGN_CENTER || type > ALIGN_CENTER_LEFT)
        return false;
    int16_t width = sizes->w;
    int16_t height = sizes->h;
    if (shifted) {
        x += width;
        y += height;
        width = 0;
        height = 0;
    }
    if (!place_in_parents(record, &x, &y, &clip))
        return false;
    int16_t item_width = width ? width : sizes->bitmap_w;
    int16_t item_height = height ? height : sizes->bitmap_h;
    Box *box = &placed->box;
    box->x = x;
    box->y = y;
    align(type, item_width, item_height, &box->x, &box->y);
    int16_t dx = clip.x - box->x;
    int16_t dy = clip.y - box->y;
    if (dx < 1) {
        placed->source_x = 0;
        box->w = item_width < dx + clip.w ? item_width : dx + clip.w;
    } else {
        placed->source_x = dx;
        box->x = clip.x;
        box->w = item_width - dx < clip.w ? item_width - dx : clip.w;
    }
    if (dy < 1) {
        placed->source_y = 0;
        box->h = item_height < dy + clip.h ? item_height : dy + clip.h;
    } else {
        placed->source_y = dy;
        box->y = clip.y;
        box->h = item_height - dy < clip.h ? item_height - dy : clip.h;
    }
    return box->w > 0 && box->h > 0;
}

bool layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box)
{
    const uint8_t *r = find(zone), *p;
    if (!r || !get_zone_parent(r) || !(p = find(get_zone_parent(r))) || get_zone_type(p) != RECORD_SIZE)
        return false;
    int16_t w = wp == ZONE_SCALE_FULL ? get_zone_x(p) : (int32_t)get_zone_x(p) * wp / ZONE_SCALE_FULL;
    int16_t h = hp == ZONE_SCALE_FULL ? get_zone_y(p) : (int32_t)get_zone_y(p) * hp / ZONE_SCALE_FULL;
    if (w <= 0 || h <= 0)
        return false;
    ZoneClip placed;
    if (!layout_get_zone(zone, &(ZoneItem){0, 0, w, h}, &placed))
        return false;
    *box = placed.box;
    return true;
}
