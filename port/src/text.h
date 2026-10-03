#ifndef TEXT_H
#define TEXT_H
#include <stdint.h>
#include "graphics.h"

/* Text metrics: the asm of text_print_to_bitmap and the globals at 0x102962-0x10296a. */
#define TEXT_CHAR_WIDTH 6  /* g_text_char_width, advance per character */
#define TEXT_CHAR_GAP 1    /* g_text_char_gap */
#define TEXT_LINE_HEIGHT 6 /* g_text_line_height */
#define TEXT_BASELINE_GAP 1 /* g_text_baseline_gap */
#define TEXT_FONT_ITEM 0x2b7 /* Graphics.DAT item of g_font_bitmap */

int text_init(void);
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s);
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s);
int text_size(const char *s, int *w, int *h);
int text_next_word_line(const char *s, char *line, int *pos, int max_width);
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, int centered, uint8_t fg, uint8_t bg, const char *s);

#endif
