/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_TEXT_H
#define DM_UI_TEXT_H
/* Prefixes: text_ */
#include "data/graphics.h"
#include "data/ids.h"
#include <stdbool.h>
#include <stdint.h>

#define TEXT_FONT_ITEM GFX_FONT

/* Loads the font graphic. The graphics file must be open. */
int text_init(void);
/* Draws s with its baseline at y. The sixth column of each cell is not touched. */
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s);
/* Draws s on the screen with its baseline at y. */
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s);
/* Pixel size of s. Returns false for an empty text. */
bool text_get_size(const char *s, int *w, int *h);
/* Draws s in the box (x, y, w, h), left aligned or centered, vertically centered.
   The original gets this position from the zone data (text_print_to_bitmap_zone). */
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, bool centered, uint8_t fg, uint8_t bg, const char *s);

#endif
