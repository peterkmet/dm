#include "bytes.h"
#include "graphics.h"
#include "ids.h"
#include "layout.h"

#define LAYOUT_SIGNATURE 0xfc0d
#define S16(p, i)        ((int16_t)BE16((p) + 2 * (i))) /* signed word i */

/* Zone record types: 0-8 a point with an alignment, 9 a size, 10-18 a point inside the rectangle of
   another point, with the alignment type - 10. Type 1 with a size parent is a translation. */
enum {
    ALIGN_CENTER,
    ALIGN_TOP_LEFT,
    ALIGN_TOP_RIGHT,
    ALIGN_BOTTOM_RIGHT,
    ALIGN_BOTTOM_LEFT,
    ALIGN_TOP_CENTER,
    ALIGN_CENTER_RIGHT,
    ALIGN_BOTTOM_CENTER,
    ALIGN_CENTER_LEFT
};
enum { RECORD_TRANSLATION = 1, RECORD_SIZE = 9, RECORD_INNER_POINT = 10, RECORD_INNER_POINT_LAST = 18 };
enum { CLIP_NONE = 20000 };
/* The layout graphic: signature and range count words, ranges of 2 words, then zone records of 4 words. */
enum { LAYOUT_HEADER_BYTES = 4, RANGE_BYTES = 4, ZONE_RECORD_BYTES = 8 };

static const uint8_t *data; /* graphic 696: signature, range count, ranges, zone records */
static uint16_t size;

/* Words of a zone record. */
static int16_t zone_type(const uint8_t *r)
{
    return S16(r, 0);
}
static int16_t zone_parent(const uint8_t *r)
{
    return S16(r, 1);
}
static int16_t zone_x(const uint8_t *r)
{
    return S16(r, 2);
} /* or the width of a size record */
static int16_t zone_y(const uint8_t *r)
{
    return S16(r, 3);
} /* or the height of a size record */

/* init_layout: keeps the raw layout graphic. Returns 0 on success. */
int layout_init(void)
{
    data = gfx_item(GFX_LAYOUT, &size);
    if (!data || size < LAYOUT_HEADER_BYTES || BE16(data) != LAYOUT_SIGNATURE ||
        LAYOUT_HEADER_BYTES + RANGE_BYTES * S16(data, 1) > size)
        return data = NULL, -1;
    return 0;
}

/* layout_find_zone: the 4-word record (type, parent, x, y) of zone n, or NULL. */
static const uint8_t *find(int16_t n)
{
    if (!data || n == 0)
        return NULL;
    int16_t ranges = S16(data, 1);
    const uint8_t *rec = data + LAYOUT_HEADER_BYTES + RANGE_BYTES * ranges;
    for (int16_t i = 0; i < ranges; i++) {
        int16_t first = S16(data, 2 + 2 * i), last = S16(data, 3 + 2 * i);
        if (first <= n && n <= last)
            return rec + ZONE_RECORD_BYTES * (n - first) + ZONE_RECORD_BYTES <= data + size
                       ? rec + ZONE_RECORD_BYTES * (n - first)
                       : NULL;
        rec += ZONE_RECORD_BYTES * (last - first + 1);
    }
    return NULL;
}

/* Top left corner of a w x h rectangle whose alignment point a is at (*x, *y). */
static void align(int16_t a, int16_t w, int16_t h, int16_t *x, int16_t *y)
{
    int16_t cx = (w + 1) >> 1, cy = (h + 1) >> 1;
    switch (a) {
    case ALIGN_CENTER:
        *x -= cx;
        *y -= cy;
        break;
    case ALIGN_TOP_RIGHT:
        *x = *x + 1 - w;
        break;
    case ALIGN_BOTTOM_RIGHT:
        *x = *x + 1 - w;
        *y = *y + 1 - h;
        break;
    case ALIGN_BOTTOM_LEFT:
        *y = *y + 1 - h;
        break;
    case ALIGN_TOP_CENTER:
        *x -= cx;
        break;
    case ALIGN_CENTER_RIGHT:
        *x = *x + 1 - w;
        *y -= cy;
        break;
    case ALIGN_BOTTOM_CENTER:
        *x -= cx;
        *y = *y + 1 - h;
        break;
    case ALIGN_CENTER_LEFT:
        *y -= cy;
        break;
    }
}

/* Alignment point a of a w x h rectangle with its top left corner at (*x, *y). */
static void anchor(int16_t a, int16_t w, int16_t h, int16_t *x, int16_t *y)
{
    int16_t cx = (w + 1) >> 1, cy = (h + 1) >> 1;
    switch (a) {
    case ALIGN_CENTER:
        *x += cx;
        *y += cy;
        break;
    case ALIGN_TOP_RIGHT:
        *x += w - 1;
        break;
    case ALIGN_BOTTOM_RIGHT:
        *x += w - 1;
        *y += h - 1;
        break;
    case ALIGN_BOTTOM_LEFT:
        *y += h - 1;
        break;
    case ALIGN_TOP_CENTER:
        *x += cx;
        break;
    case ALIGN_CENTER_RIGHT:
        *x += w - 1;
        *y += cy;
        break;
    case ALIGN_BOTTOM_CENTER:
        *x += cx;
        *y += h - 1;
        break;
    case ALIGN_CENTER_LEFT:
        *y += cy;
        break;
    }
}

/* Intersects the clip box with the rectangle (x, y, w, h), as get_zone does. */
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

/* The parent walk of get_zone: moves the point (*x, *y) and the clip box by the translations and the
   rectangles of the parents of record r. Returns 0 when nothing is visible. */
static int place_in_parents(const uint8_t *r, int16_t *x, int16_t *y, Box *clip)
{
    const uint8_t *p, *s;
    int16_t pending = 0;
    while (zone_parent(r)) {
        int16_t t = zone_type(r);
        if (t < RECORD_INNER_POINT || t > RECORD_INNER_POINT_LAST) {
            if (!(p = find(zone_parent(r))))
                break;
            int16_t a = zone_x(p), b = zone_y(p);
            if (zone_type(p) == RECORD_TRANSLATION) {
                *x += a;
                *y += b;
                clip->x += a;
                clip->y += b;
            } else if (zone_type(p) == RECORD_SIZE) { /* the rectangle of the point r */
                if (t < RECORD_SIZE) {
                    a = zone_x(r);
                    b = zone_y(r);
                    align(t, zone_x(p), zone_y(p), &a, &b);
                }
                if (pending) {
                    pending = 0;
                    *x += a;
                    *y += b;
                    clip->x += a;
                    clip->y += b;
                }
                clip_to(clip, a, b, zone_x(p), zone_y(p));
            } else if (zone_type(p) < RECORD_SIZE) {
                pending = 1;
            }
            r = p;
        } else { /* a point inside the rectangle of another point */
            if (!(p = find(zone_parent(r))) || !(s = find(zone_parent(p))))
                break;
            int16_t a = zone_x(p), b = zone_y(p), sw = zone_x(s), sh = zone_y(s);
            if (zone_type(p) >= RECORD_SIZE)
                return 0;
            align(zone_type(p), sw, sh, &a, &b);
            clip->x += a;
            clip->y += b;
            clip_to(clip, a, b, sw, sh);
            anchor(t - RECORD_INNER_POINT, sw, sh, &a, &b);
            *x += a + zone_x(r);
            *y += b + zone_y(r);
            r = s;
        }
    }
    return 1;
}

/* get_zone: places an item of *w x *h pixels (0 = bitmap size) in zone and clips it to the
   parent rectangles. ZONE_SHIFTED adds *w, *h to the position and takes the bitmap size.
   On return *w, *h hold the source offset of the clipped box. Returns 0 when nothing is visible. */
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h)
{
    if (zone == -1)
        return 0;
    int shifted = zone & ZONE_SHIFTED;
    const uint8_t *r = find(zone & (ZONE_SHIFTED - 1));
    if (!r)
        return 0;
    Box clip = {0, 0, CLIP_NONE, CLIP_NONE};
    int16_t type = zone_type(r), x = 0, y = 0;
    if (type < RECORD_SIZE) {
        x = zone_x(r);
        y = zone_y(r);
    } else {
        if (type == RECORD_SIZE)
            return 0;
        type -= RECORD_INNER_POINT;
    }
    if (type < ALIGN_CENTER || type > ALIGN_CENTER_LEFT)
        return 0;
    if (shifted) {
        x += *w;
        y += *h;
        *w = *h = 0;
    }
    if (!place_in_parents(r, &x, &y, &clip))
        return 0;
    int16_t iw = *w ? *w : bitmap_w, ih = *h ? *h : bitmap_h;
    box->x = x;
    box->y = y;
    align(type, iw, ih, &box->x, &box->y);
    int16_t dx = clip.x - box->x, dy = clip.y - box->y;
    if (dx < 1) {
        *w = 0;
        box->w = iw < dx + clip.w ? iw : dx + clip.w;
    } else {
        *w = dx;
        box->x = clip.x;
        box->w = iw - dx < clip.w ? iw - dx : clip.w;
    }
    if (dy < 1) {
        *h = 0;
        box->h = ih < dy + clip.h ? ih : dy + clip.h;
    } else {
        *h = dy;
        box->y = clip.y;
        box->h = ih - dy < clip.h ? ih - dy : clip.h;
    }
    return box->w > 0 && box->h > 0;
}

/* get_zone_box_clipped: the box of zone with the size record of its parent, scaled by
   wp / ZONE_SCALE_FULL and hp / ZONE_SCALE_FULL. Returns 0 when nothing is visible. */
int layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box)
{
    const uint8_t *r = find(zone), *p;
    if (!r || !zone_parent(r) || !(p = find(zone_parent(r))) || zone_type(p) != RECORD_SIZE)
        return 0;
    int16_t w = wp == ZONE_SCALE_FULL ? zone_x(p) : (int32_t)zone_x(p) * wp / ZONE_SCALE_FULL;
    int16_t h = hp == ZONE_SCALE_FULL ? zone_y(p) : (int32_t)zone_y(p) * hp / ZONE_SCALE_FULL;
    if (w <= 0 || h <= 0)
        return 0;
    return layout_get_zone(zone, 0, 0, box, &w, &h);
}
