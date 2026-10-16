#ifndef TEXT_H
#define TEXT_H
/* Prefixes: text_ */
#include <stdbool.h>
#include <stdint.h>
#include "data/graphics.h"
#include "data/ids.h"

#define TEXT_FONT_ITEM GFX_FONT

int text_init(void);
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s);
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s);
bool text_get_size(const char *s, int *w, int *h);
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, bool centered, uint8_t fg, uint8_t bg, const char *s);

#endif
