/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_DRAW_H
#define DM_UI_DRAW_H
/* Prefixes: draw_ */

/* Pixel operations on bitmaps: blit, fill, flip, shrink, and the masked fill. It owns no bitmap and knows no game rule. */

#include "data/graphics.h"
#include <stdbool.h>
#include <stdint.h>

/* Mirror flags of draw_blit. */
#define FLIP_H 0x01
#define FLIP_V 0x02

/* Transparent flags of draw_masked_fill. */
#define FILL_NO_MASK 0x80
#define FILL_OPAQUE  0xff

/* The pattern of draw_masked_fill: the units, the mask (NULL: none), the color key, and the source point. */
typedef struct {
    const Bitmap *units;
    const Bitmap *mask;
    bool mask_flip;
    int16_t last_unit, first_unit;
    int16_t transparent;
    int16_t sx, sy;
} MaskedFill;

/* Copies box from src at (sx, sy). transparent -1 is opaque,
   otherwise bits 0-3 are a color key. flip reads the mirrored source. Clips to both bitmaps. */
void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip);
/* Fills box of dst with one color, clipped to dst. */
void draw_fill(Bitmap *dst, const Box *box, uint8_t color);
/* Mirrors bm in place. */
void draw_flip(Bitmap *bm, int flip);
/* Samples the source at the pixel centers with the 16.16 step of
   the original. Rows after the first sample one row higher, as the original does. Returns true on success. */
bool draw_shrink(const Bitmap *src, int16_t width, int16_t height, const uint8_t *palette_changes, Bitmap *out);
/* Fills box of dst from a masked pattern. units holds one 16-pixel unit per word of its
   plane 0, in row order. The temp bitmap is box->w rounded up to words; each word takes the next unit,
   from first_unit, restarting at 0 when the index reaches last_unit. With a mask (bit 7 of transparent
   clear), the mask words are read in row order; a 0 bit gives the transparent color. mask_flip mirrors
   the mask by whole words, as the original does. The temp bitmap is then blitted from
   (sx, sy) with the color key. */
void draw_masked_fill(Bitmap *dst, const Box *box, const MaskedFill *fill);

#endif
