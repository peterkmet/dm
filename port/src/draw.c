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

/* blit_box_filled_with_masked_bitmap on chunky pixels. units holds one 16-pixel unit per word of its
   plane 0, in row order. The temp bitmap is box->w rounded up to words; each word takes the next unit,
   from first_unit, restarting at 0 when the index reaches last_unit. With a mask (bit 7 of transparent
   clear), the mask words are read in row order; a 0 bit gives the transparent color. mask_flip mirrors
   the mask by whole words, as flip_bitmap_horizontal does. The temp bitmap is then blitted from
   (sx, sy) with the color key. */
void draw_masked_fill(const Bitmap *units, const Bitmap *mask, int mask_flip, Bitmap *dst, const Box *box, int16_t last_unit, int16_t first_unit, int16_t transparent, int16_t sx, int16_t sy)
{
    if (box->w < 1 || box->h < 1) return;
    if ((transparent & FILL_OPAQUE) == FILL_OPAQUE) mask = NULL, transparent = -1;
    else if (transparent & FILL_NO_MASK) mask = NULL, transparent &= ~FILL_NO_MASK;
    int words = ((box->w - 1) >> 4) + 1, tw = words * 16, uw = (units->w + 15) >> 4, mw = mask ? (mask->w + 15) >> 4 : 0;
    uint8_t *temp = malloc((size_t)tw * box->h);
    if (!temp) return;
    int16_t u = first_unit;
    for (int k = 0; k < words * box->h; k++) {
        for (int p = 0; p < 16; p++) {
            int ux = u % uw * 16 + p, uy = u / uw;
            uint8_t c = ux < units->w && uy < units->h ? units->px[uy * units->w + ux] : 0;
            if (mask) {
                int mx = k % mw * 16 + p, my = k / mw;
                if (mask_flip) mx = mw * 16 - 1 - mx;
                if (!(mx < mask->w && my < mask->h && mask->px[my * mask->w + mx] & 1)) c = transparent & 15;
            }
            temp[k * 16 + p] = c;
        }
        if (++u == last_unit) u = 0;
    }
    for (int y = 0; y < box->h; y++)
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x, dy = box->y + y;
            long i = (long)(sy + y) * tw + sx + x;
            if (dx < 0 || dx >= dst->w || dy < 0 || dy >= dst->h || i < 0 || i >= (long)tw * box->h) continue;
            if (transparent == -1 || temp[i] != (transparent & 15)) dst->px[dy * dst->w + dx] = temp[i];
        }
    free(temp);
}
