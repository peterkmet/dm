#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "video.h"

/* blit_bitmap on chunky pixels: copies box from src at (sx, sy). transparent -1 is opaque,
   otherwise bits 0-3 are a color key. flip reads the mirrored source. Clips to both bitmaps. */
void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip)
{
    for (int y = 0; y < box->h; y++) {
        int dy = box->y + y, ry = sy + y;
        if (flip & FLIP_V)
            ry = src->h - 1 - ry;
        if (dy < 0 || dy >= dst->h || ry < 0 || ry >= src->h)
            continue;
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x, rx = sx + x;
            if (flip & FLIP_H)
                rx = src->w - 1 - rx;
            if (dx < 0 || dx >= dst->w || rx < 0 || rx >= src->w)
                continue;
            uint8_t c = src->px[ry * src->w + rx];
            if (transparent == -1 || c != (transparent & COLOR_INDEX_MASK))
                dst->px[dy * dst->w + dx] = c;
        }
    }
}

/* Fills box of dst with one color, clipped to dst. */
void draw_fill(Bitmap *dst, const Box *box, uint8_t color)
{
    for (int y = box->y; y < box->y + box->h; y++)
        for (int x = box->x; x < box->x + box->w; x++)
            if (y >= 0 && y < dst->h && x >= 0 && x < dst->w)
                dst->px[y * dst->w + x] = color;
}

/* Mirrors bm in place. */
void draw_flip(Bitmap *bm, int flip)
{
    size_t n = (size_t)bm->w * bm->h;
    uint8_t *copy = malloc(n ? n : 1);
    if (!copy)
        return;
    memcpy(copy, bm->px, n);
    draw_blit(&(Bitmap){bm->w, bm->h, copy}, 0, 0, bm, &(Box){0, 0, bm->w, bm->h}, -1, flip);
    free(copy);
}

/* Step of draw_shrink: source pixels per destination pixel with 10 fraction bits, truncated to 16 bits.
   A sample position adds 15 more fraction bits and rounds to the pixel. */
enum { STEP_FRACTION_BITS = 10, SAMPLE_FRACTION_BITS = 16, SAMPLE_ROUNDING = 0x7fff };
/* Scales the step to the 16 fraction bits of a sample at half-pixel positions. */
enum { SAMPLE_STEP_SCALE = 1 << (SAMPLE_FRACTION_BITS - STEP_FRACTION_BITS - 1) };

/* blit_shrink_with_palette_changes: samples the source at the pixel centers with the 16.16 step of
   the original. Rows after the first sample one row higher, as the original does. */
int draw_shrink(const Bitmap *src, int16_t dw, int16_t dh, const uint8_t *palette_changes, Bitmap *out)
{
    if (dw < 1 || dh < 1 || !src->w || !src->h)
        return -1;
    uint8_t *px = malloc((size_t)dw * dh);
    if (!px)
        return -1;
    uint32_t step_x = (uint16_t)(((uint32_t)src->w << STEP_FRACTION_BITS) / dw); /* the step keeps 16 bits */
    uint32_t step_y = (uint16_t)(((uint32_t)src->h << STEP_FRACTION_BITS) / dh); /* the step keeps 16 bits */
    for (int y = 0; y < dh; y++) {
        int ry = (int)((SAMPLE_ROUNDING + step_y * SAMPLE_STEP_SCALE * (2 * y + 1)) >> SAMPLE_FRACTION_BITS) - (y > 0);
        if (ry >= src->h)
            ry = src->h - 1;
        for (int x = 0; x < dw; x++) {
            int rx = (SAMPLE_ROUNDING + step_x * SAMPLE_STEP_SCALE * (2 * x + 1)) >> SAMPLE_FRACTION_BITS;
            if (rx >= src->w)
                rx = src->w - 1;
            uint8_t c = src->px[ry * src->w + rx];
            px[y * dw + x] = palette_changes ? palette_changes[c] & COLOR_INDEX_MASK : c;
        }
    }
    *out = (Bitmap){dw, dh, px};
    return 0;
}

/* blit_box_filled_with_masked_bitmap on chunky pixels. units holds one 16-pixel unit per word of its
   plane 0, in row order. The temp bitmap is box->w rounded up to words; each word takes the next unit,
   from first_unit, restarting at 0 when the index reaches last_unit. With a mask (bit 7 of transparent
   clear), the mask words are read in row order; a 0 bit gives the transparent color. mask_flip mirrors
   the mask by whole words, as flip_bitmap_horizontal does. The temp bitmap is then blitted from
   (sx, sy) with the color key. */
void draw_masked_fill(const Bitmap *units, const Bitmap *mask, int mask_flip, Bitmap *dst, const Box *box,
                      int16_t last_unit, int16_t first_unit, int16_t transparent, int16_t sx, int16_t sy)
{
    if (box->w < 1 || box->h < 1)
        return;
    if ((transparent & FILL_OPAQUE) == FILL_OPAQUE)
        mask = NULL, transparent = -1;
    else if (transparent & FILL_NO_MASK)
        mask = NULL, transparent &= ~FILL_NO_MASK;
    int words = (box->w - 1) / PIXELS_PER_WORD + 1, temp_w = words * PIXELS_PER_WORD;
    int unit_words = (units->w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD;
    int mask_words = mask ? (mask->w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD : 0; /* words per row */
    uint8_t *temp = malloc((size_t)temp_w * box->h);
    if (!temp)
        return;
    int16_t u = first_unit;
    for (int k = 0; k < words * box->h; k++) {
        for (int p = 0; p < PIXELS_PER_WORD; p++) {
            int ux = u % unit_words * PIXELS_PER_WORD + p, uy = u / unit_words;
            uint8_t c = ux < units->w && uy < units->h ? units->px[uy * units->w + ux] : 0;
            if (mask) {
                int mx = k % mask_words * PIXELS_PER_WORD + p, my = k / mask_words;
                if (mask_flip)
                    mx = mask_words * PIXELS_PER_WORD - 1 - mx;
                if (!(mx < mask->w && my < mask->h && mask->px[my * mask->w + mx] & 1))
                    c = transparent & COLOR_INDEX_MASK;
            }
            temp[k * PIXELS_PER_WORD + p] = c;
        }
        if (++u == last_unit)
            u = 0;
    }
    for (int y = 0; y < box->h; y++)
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x, dy = box->y + y;
            int64_t i = (int64_t)(sy + y) * temp_w + sx + x;
            if (dx < 0 || dx >= dst->w || dy < 0 || dy >= dst->h || i < 0 || i >= (int64_t)temp_w * box->h)
                continue;
            if (transparent == -1 || temp[i] != (transparent & COLOR_INDEX_MASK))
                dst->px[dy * dst->w + dx] = temp[i];
        }
    free(temp);
}
