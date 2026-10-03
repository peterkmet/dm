#include <stdlib.h>
#include <string.h>
#include "draw.h"

/* blit_bitmap on chunky pixels: copies box from src at (sx, sy). transparent -1 is opaque,
   otherwise bits 0-3 are a color key. flip reads the mirrored source. Clips to both bitmaps. */
void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip)
{
    for (int y = 0; y < box->h; y++) {
        int dy = box->y + y, ry = sy + y;
        if (flip & FLIP_V) ry = src->h - 1 - ry;
        if (dy < 0 || dy >= dst->h || ry < 0 || ry >= src->h) continue;
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x, rx = sx + x;
            if (flip & FLIP_H) rx = src->w - 1 - rx;
            if (dx < 0 || dx >= dst->w || rx < 0 || rx >= src->w) continue;
            uint8_t c = src->px[ry * src->w + rx];
            if (transparent == -1 || c != (transparent & 15)) dst->px[dy * dst->w + dx] = c;
        }
    }
}

void draw_fill(Bitmap *dst, const Box *box, uint8_t color)
{
    for (int y = box->y; y < box->y + box->h; y++)
        for (int x = box->x; x < box->x + box->w; x++)
            if (y >= 0 && y < dst->h && x >= 0 && x < dst->w) dst->px[y * dst->w + x] = color;
}

/* Mirrors bm in place. */
void draw_flip(Bitmap *bm, int flip)
{
    size_t n = (size_t)bm->w * bm->h;
    uint8_t *copy = malloc(n ? n : 1);
    if (!copy) return;
    memcpy(copy, bm->px, n);
    draw_blit(&(Bitmap){ bm->w, bm->h, copy }, 0, 0, bm, &(Box){ 0, 0, (int16_t)bm->w, (int16_t)bm->h }, -1, flip);
    free(copy);
}

/* blit_shrink_with_palette_changes: samples the source at the pixel centers with the 16.16 step of
   the original. Rows after the first sample one row higher, as the original does. */
int draw_shrink(const Bitmap *src, int16_t dw, int16_t dh, const uint8_t *palette_changes, Bitmap *out)
{
    if (dw < 1 || dh < 1 || !src->w || !src->h) return -1;
    uint8_t *px = malloc((size_t)dw * dh);
    if (!px) return -1;
    uint32_t stx = ((uint32_t)src->w << 10) / (uint16_t)dw & 0xffff, sty = ((uint32_t)src->h << 10) / (uint16_t)dh & 0xffff;
    for (int y = 0; y < dh; y++) {
        int ry = (int)((0x7fff + sty * 32 * (2 * y + 1)) >> 16) - (y > 0);
        if (ry >= src->h) ry = src->h - 1;
        for (int x = 0; x < dw; x++) {
            int rx = (int)((0x7fff + stx * 32 * (2 * x + 1)) >> 16);
            if (rx >= src->w) rx = src->w - 1;
            uint8_t c = src->px[ry * src->w + rx];
            px[y * dw + x] = palette_changes ? palette_changes[c] & 15 : c;
        }
    }
    *out = (Bitmap){ (uint16_t)dw, (uint16_t)dh, px };
    return 0;
}
