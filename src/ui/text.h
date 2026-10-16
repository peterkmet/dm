/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_TEXT_H
#define DM_UI_TEXT_H
/* Prefixes: text_ */
#include "data/graphics.h"
#include "data/ids.h"
#include <stdbool.h>
#include <stdint.h>

#define TEXT_FONT_ITEM GFX_FONT

int text_init(void);
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s);
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s);
bool text_get_size(const char *s, int *w, int *h);
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, bool centered, uint8_t fg, uint8_t bg, const char *s);

#endif
