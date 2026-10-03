#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>
#include "graphics.h"

typedef struct {
    int16_t x, y, w, h;
} Box;

enum { FLIP_H = 1, FLIP_V = 2 };

void draw_blit(const Bitmap *src, int16_t sx, int16_t sy, Bitmap *dst, const Box *box, int16_t transparent, int flip);
void draw_fill(Bitmap *dst, const Box *box, uint8_t color);
void draw_flip(Bitmap *bm, int flip);
int draw_shrink(const Bitmap *src, int16_t dw, int16_t dh, const uint8_t *palette_changes, Bitmap *out);

#endif
