/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_DRAW_H
#define DM_UI_DRAW_H
/* Prefixes: draw_ */
#include "data/graphics.h"
#include <stdint.h>

typedef struct {
    int16_t x, y, w, h;
} Box;

#define FLIP_H       1
#define FLIP_V       2
#define FILL_NO_MASK 0x80 /* draw_masked_fill transparent flags */
#define FILL_OPAQUE  0xff

void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip);
void draw_fill(Bitmap *dst, const Box *box, uint8_t color);
void draw_flip(Bitmap *bm, int flip);
void draw_masked_fill(const Bitmap *units, const Bitmap *mask, int mask_flip, Bitmap *dst, const Box *box,
                      int16_t last_unit, int16_t first_unit, int16_t transparent, int16_t sx, int16_t sy);
int draw_shrink(const Bitmap *src, int16_t width, int16_t height, const uint8_t *palette_changes, Bitmap *out);

#endif
