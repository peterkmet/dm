#include "graphics.h"
#include "layout.h"

#define LAYOUT_GRAPHIC 696
#define S16(p, i) ((int16_t)((p)[2 * (i)] << 8 | (p)[2 * (i) + 1]))

static const uint8_t *data; /* graphic 696: 0xfc0d, range count, ranges, zone records */
static uint16_t size;

/* init_layout: keeps the raw layout graphic. Returns 0 on success. */
int layout_init(void)
{
    data = gfx_item(LAYOUT_GRAPHIC, &size);
    if (!data || size < 4 || (uint16_t)S16(data, 0) != 0xfc0d || 4 + 4 * S16(data, 1) > size) return data = NULL, -1;
    return 0;
}

/* layout_find_zone: the 4-word record (type, parent, a, b) of zone n, or NULL. */
static const uint8_t *find(int16_t n)
{
    if (!data || n == 0) return NULL;
    int16_t ranges = S16(data, 1);
    const uint8_t *rec = data + 4 + 4 * ranges;
    for (int16_t i = 0; i < ranges; i++) {
        int16_t first = S16(data, 2 + 2 * i), last = S16(data, 3 + 2 * i);
        if (first <= n && n <= last) return rec + 8 * (n - first) + 8 <= data + size ? rec + 8 * (n - first) : NULL;
        rec += 8 * (last - first + 1);
    }
    return NULL;
}

/* Reference point of a w x h rectangle at (*x, *y) for alignment code a (0-8). */
static void align(int16_t a, int16_t w, int16_t h, int16_t *x, int16_t *y)
{
    int16_t cx = (int16_t)((w + 1) >> 1), cy = (int16_t)((h + 1) >> 1);
    switch (a) {
    case 0: *x -= cx; *y -= cy; break;
    case 2: *x = *x + 1 - w; break;
    case 3: *x = *x + 1 - w; *y = *y + 1 - h; break;
    case 4: *y = *y + 1 - h; break;
    case 5: *x -= cx; break;
    case 6: *x = *x + 1 - w; *y -= cy; break;
    case 7: *x -= cx; *y = *y + 1 - h; break;
    case 8: *y -= cy; break;
    }
}

/* Intersects the clip box with the rectangle (x, y, w, h), as get_zone does. */
static void clip_to(Box *c, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (c->x < x) c->x = x;
    if (x + w <= c->w + c->x - 1) c->w = (int16_t)(x + w - c->x);
    if (c->y < y) c->y = y;
    if (y + h <= c->h + c->y - 1) c->h = (int16_t)(y + h - c->y);
}

/* get_zone: places an item of *w x *h pixels (0 = bitmap size) in zone and clips it to the
   parent rectangles. Bit 15 of zone adds *w, *h to the position and takes the bitmap size.
   On return *w, *h hold the source offset of the clipped box. Returns 0 when nothing is visible. */
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h)
{
    if (zone == -1) return 0;
    int relative = zone & 0x8000;
    const uint8_t *r = find(zone & 0x7fff), *p, *s;
    if (!r) return 0;
    Box clip = { 0, 0, 20000, 20000 };
    int16_t type = S16(r, 0), x = 0, y = 0, pending = 0;
    if (type < 9) {
        x = S16(r, 2);
        y = S16(r, 3);
    } else {
        if (type == 9) return 0;
        type -= 10;
    }
    if (type < 0 || type > 8) return 0;
    if (relative) {
        x += *w;
        y += *h;
        *w = *h = 0;
    }
    while (S16(r, 1)) {
        int16_t t = S16(r, 0);
        if (t < 10 || t > 18) {
            if (!(p = find(S16(r, 1)))) break;
            int16_t a = S16(p, 2), b = S16(p, 3);
            if (S16(p, 0) == 1) { /* translation */
                x += a; y += b; clip.x += a; clip.y += b;
            } else if (S16(p, 0) == 9) { /* size: the rectangle of the point r */
                if (t <= 8) {
                    a = S16(r, 2); b = S16(r, 3);
                    align(t, S16(p, 2), S16(p, 3), &a, &b);
                }
                if (pending) {
                    pending = 0;
                    x += a; y += b; clip.x += a; clip.y += b;
                }
                clip_to(&clip, a, b, S16(p, 2), S16(p, 3));
            } else if (S16(p, 0) < 9) {
                pending = 1;
            }
            r = p;
        } else { /* a point inside the rectangle of another point */
            if (!(p = find(S16(r, 1))) || !(s = find(S16(p, 1)))) break;
            int16_t a = S16(p, 2), b = S16(p, 3), sw = S16(s, 2), sh = S16(s, 3);
            if (S16(p, 0) > 8) return 0;
            align(S16(p, 0), sw, sh, &a, &b);
            clip.x += a;
            clip.y += b;
            clip_to(&clip, a, b, sw, sh);
            switch (t) {
            case 10: a += (sw + 1) >> 1; b += (sh + 1) >> 1; break;
            case 12: a += sw - 1; break;
            case 13: a += sw - 1; b += sh - 1; break;
            case 14: b += sh - 1; break;
            case 15: a += (sw + 1) >> 1; break;
            case 16: a += sw - 1; b += (sh + 1) >> 1; break;
            case 17: a += (sw + 1) >> 1; b += sh - 1; break;
            case 18: b += (sh + 1) >> 1; break;
            }
            x += a + S16(r, 2);
            y += b + S16(r, 3);
            r = s;
        }
    }
    int16_t iw = *w ? *w : bitmap_w, ih = *h ? *h : bitmap_h;
    box->x = x;
    box->y = y;
    align(type, iw, ih, &box->x, &box->y);
    int16_t dx = (int16_t)(clip.x - box->x), dy = (int16_t)(clip.y - box->y);
    if (dx < 1) {
        *w = 0;
        box->w = iw < dx + clip.w ? iw : (int16_t)(dx + clip.w);
    } else {
        *w = dx;
        box->x = clip.x;
        box->w = iw - dx < clip.w ? (int16_t)(iw - dx) : clip.w;
    }
    if (dy < 1) {
        *h = 0;
        box->h = ih < dy + clip.h ? ih : (int16_t)(dy + clip.h);
    } else {
        *h = dy;
        box->y = clip.y;
        box->h = ih - dy < clip.h ? (int16_t)(ih - dy) : clip.h;
    }
    return box->w > 0 && box->h > 0;
}
