#ifndef TEXT_H
#define TEXT_H
/* Prefixes: text_ */
#include <stdint.h>
#include "graphics.h"
#include "ids.h"

#define TEXT_FONT_ITEM GFX_FONT

int text_init(void);
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s);
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s);
int text_size(const char *s, int *w, int *h);
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, int centered, uint8_t fg, uint8_t bg, const char *s);

#endif
