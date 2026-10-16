#ifndef DRAW_H
#define DRAW_H
/* Prefixes: draw_ */
#include <stdint.h>
#include "graphics.h"

typedef struct {
    int16_t x, y, w, h;
} Box;

enum { FLIP_H = 1, FLIP_V = 2 };
enum { FILL_NO_MASK = 0x80, FILL_OPAQUE = 0xff }; /* draw_masked_fill transparent flags */

void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip);
void draw_fill(Bitmap *dst, const Box *box, uint8_t color);
void draw_flip(Bitmap *bm, int flip);
void draw_masked_fill(const Bitmap *units, const Bitmap *mask, int mask_flip, Bitmap *dst, const Box *box,
                      int16_t last_unit, int16_t first_unit, int16_t transparent, int16_t sx, int16_t sy);
int draw_shrink(const Bitmap *src, int16_t width, int16_t height, const uint8_t *palette_changes, Bitmap *out);

#endif
