/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/text.h"
#include "ui/video.h"
#include <string.h>

#define FONT_ROWS                6   /* pixel rows of a character cell */
#define FONT_STRIDE              128 /* bytes per font row, one per character code */
#define FONT_SIZE                (FONT_ROWS * FONT_STRIDE)
#define GLYPH_WIDTH              5 /* drawn pixels per character; bit 4 is the left pixel */
#define GLYPH_TOP_ABOVE_BASELINE 4 /* the cell starts 4 rows above the baseline */

static uint8_t font_data[FONT_SIZE];
static const uint8_t *font;

int text_init(void)
{
    uint16_t size;
    const uint8_t *p = gfx_get_item(TEXT_FONT_ITEM, &size);
    if (!p || size != FONT_SIZE)
        return -1;
    memcpy(font_data, p, FONT_SIZE);
    font = font_data;
    return 0;
}

/* Draws one font row of a character: set pixels get fg, the others bg; outside pixels are clipped. */
static void draw_glyph_row(Bitmap *bm, int x, int py, uint8_t bits, uint8_t fg, uint8_t bg)
{
    for (int k = 0; k < GLYPH_WIDTH; k++) {
        int px = x + k;
        if (px >= 0 && px < bm->w)
            bm->px[py * bm->w + px] = bits >> (GLYPH_WIDTH - 1 - k) & 1 ? fg : bg;
    }
}

void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s)
{
    if (!font)
        return;
    for (; *s; s++, x += FONT_CHAR_WIDTH) {
        unsigned c = (uint8_t)*s;
        for (int r = 0; r < FONT_ROWS; r++) {
            int py = y - GLYPH_TOP_ABOVE_BASELINE + r;
            if (py < 0 || py >= bm->h)
                continue;
            /* codes >= 128 read into the next font rows, as in the original */
            unsigned i = c + r * FONT_STRIDE;
            draw_glyph_row(bm, x, py, i < FONT_SIZE ? font[i] : 0, fg, bg);
        }
    }
}

void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s)
{
    Bitmap screen = video_get_screen();
    text_draw(&screen, x, y, fg, bg, s);
}

bool text_get_size(const char *s, int *w, int *h)
{
    int n = s ? (int)strlen(s) : 0;
    if (n * FONT_CHAR_WIDTH - FONT_CHAR_GAP < 1)
        return false;
    *w = n * FONT_CHAR_WIDTH - FONT_CHAR_GAP;
    *h = FONT_LINE_HEIGHT - FONT_BASELINE_GAP;
    return true;
}

void text_draw_box(Bitmap *bm, int x, int y, int w, int h, bool centered, uint8_t fg, uint8_t bg, const char *s)
{
    int tw, th;
    if (!text_get_size(s, &tw, &th))
        return;
    if (centered)
        x += (w - tw) / 2;
    y += (h - th) / 2;
    text_draw(bm, x, y + th - 1, fg, bg, s);
}
