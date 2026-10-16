/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/draw.h"
#include "ui/video.h"
#include <stdlib.h>
#include <string.h>

void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip)
{
    for (int y = 0; y < box->h; y++) {
        int dy = box->y + y;
        int ry = sy + y;
        if (flip & FLIP_V)
            ry = src->h - 1 - ry;
        if (dy < 0 || dy >= dst->h || ry < 0 || ry >= src->h)
            continue;
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x;
            int rx = sx + x;
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

void draw_fill(Bitmap *dst, const Box *box, uint8_t color)
{
    for (int y = box->y; y < box->y + box->h; y++)
        for (int x = box->x; x < box->x + box->w; x++)
            if (y >= 0 && y < dst->h && x >= 0 && x < dst->w)
                dst->px[y * dst->w + x] = color;
}

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
#define STEP_FRACTION_BITS   10
#define SAMPLE_FRACTION_BITS 16
#define SAMPLE_ROUNDING      0x7fff
/* Scales the step to the 16 fraction bits of a sample at half-pixel positions. */
#define SAMPLE_STEP_SCALE    (1 << (SAMPLE_FRACTION_BITS - STEP_FRACTION_BITS - 1))

int draw_shrink(const Bitmap *src, int16_t width, int16_t height, const uint8_t *palette_changes, Bitmap *out)
{
    if (width < 1 || height < 1 || !src->w || !src->h)
        return -1;
    uint8_t *px = malloc((size_t)width * height);
    if (!px)
        return -1;
    uint32_t step_x = (uint16_t)(((uint32_t)src->w << STEP_FRACTION_BITS) / width);  /* the step keeps 16 bits */
    uint32_t step_y = (uint16_t)(((uint32_t)src->h << STEP_FRACTION_BITS) / height); /* the step keeps 16 bits */
    for (int y = 0; y < height; y++) {
        int ry = (int)((SAMPLE_ROUNDING + step_y * SAMPLE_STEP_SCALE * (2 * y + 1)) >> SAMPLE_FRACTION_BITS) - (y > 0);
        if (ry >= src->h)
            ry = src->h - 1;
        for (int x = 0; x < width; x++) {
            int rx = (SAMPLE_ROUNDING + step_x * SAMPLE_STEP_SCALE * (2 * x + 1)) >> SAMPLE_FRACTION_BITS;
            if (rx >= src->w)
                rx = src->w - 1;
            uint8_t c = src->px[ry * src->w + rx];
            px[y * width + x] = palette_changes ? palette_changes[c] & COLOR_INDEX_MASK : c;
        }
    }
    *out = (Bitmap){width, height, px};
    return 0;
}

void draw_masked_fill(const Bitmap *units, const Bitmap *mask, int mask_flip, Bitmap *dst, const Box *box,
                      int16_t last_unit, int16_t first_unit, int16_t transparent, int16_t sx, int16_t sy)
{
    if (box->w < 1 || box->h < 1)
        return;
    if ((transparent & FILL_OPAQUE) == FILL_OPAQUE) {
        mask = NULL;
        transparent = -1;
    } else if (transparent & FILL_NO_MASK) {
        mask = NULL;
        transparent &= ~FILL_NO_MASK;
    }
    int words = (box->w - 1) / PIXELS_PER_WORD + 1;
    int temp_w = words * PIXELS_PER_WORD;
    int unit_words = (units->w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD;
    int mask_words = mask ? (mask->w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD : 0; /* words per row */
    uint8_t *temp = malloc((size_t)temp_w * box->h);
    if (!temp)
        return;
    int16_t unit = first_unit;
    for (int k = 0; k < words * box->h; k++) {
        for (int p = 0; p < PIXELS_PER_WORD; p++) {
            int ux = unit % unit_words * PIXELS_PER_WORD + p;
            int uy = unit / unit_words;
            uint8_t color = ux < units->w && uy < units->h ? units->px[uy * units->w + ux] : 0;
            if (mask) {
                int mx = k % mask_words * PIXELS_PER_WORD + p;
                int my = k / mask_words;
                if (mask_flip)
                    mx = mask_words * PIXELS_PER_WORD - 1 - mx;
                if (!(mx < mask->w && my < mask->h && mask->px[my * mask->w + mx] & 1))
                    color = transparent & COLOR_INDEX_MASK;
            }
            temp[k * PIXELS_PER_WORD + p] = color;
        }
        if (++unit == last_unit)
            unit = 0;
    }
    for (int y = 0; y < box->h; y++)
        for (int x = 0; x < box->w; x++) {
            int dx = box->x + x;
            int dy = box->y + y;
            int64_t i = (int64_t)(sy + y) * temp_w + sx + x;
            if (dx < 0 || dx >= dst->w || dy < 0 || dy >= dst->h || i < 0 || i >= (int64_t)temp_w * box->h)
                continue;
            if (transparent == -1 || temp[i] != (transparent & COLOR_INDEX_MASK))
                dst->px[dy * dst->w + dx] = temp[i];
        }
    free(temp);
}
