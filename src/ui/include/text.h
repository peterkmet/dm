/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_TEXT_H
#define DM_UI_TEXT_H
/* Prefixes: text_ */

/* The DM font: it draws text and measures it. It owns the font data; it knows no zone and no game state. */

#include "graphics.h"
#include "ids.h"
#include <stdbool.h>
#include <stdint.h>

/* The font graphic. */
#define TEXT_FONT_ITEM GFX_FONT

/* Loads the font graphic. The graphics file must be open. Returns true on success. */
bool text_init(void);
/* A pixel of a bitmap. */
typedef struct {
    int16_t x, y;
} Point;

/* Draws s from point, with its baseline at point.y. The sixth column of each cell is not touched. */
void text_draw(Bitmap *bm, Point point, uint8_t fg, uint8_t bg, const char *s);
/* Draws s on the screen with its baseline at y. */
void text_draw_screen(int16_t x, int16_t y, uint8_t fg, uint8_t bg, const char *s);
/* Pixel size of s. Returns false for an empty text. */
bool text_get_size(const char *s, int16_t *w, int16_t *h);
/* Draws s on the screen in box, left aligned or centered, vertically centered.
   The zone data gives this position in the original. */
void text_draw_box(const Box *box, bool centered, uint8_t fg, uint8_t bg, const char *s);

#endif
