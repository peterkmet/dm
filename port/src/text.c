#include <string.h>
#include "text.h"
#include "video.h"

#define FONT_SIZE 0x300 /* 6 rows of 128 bytes, one byte per character */
#define FONT_STRIDE 128

static uint8_t font_data[FONT_SIZE];
static const uint8_t *font;

/* Port of the font part of text_init: the font is raw item 0x2b7 of Graphics.DAT (gfx_open first). */
int text_init(void)
{
    uint16_t size;
    const uint8_t *p = gfx_item(TEXT_FONT_ITEM, &size);
    if (!p || size != FONT_SIZE) return -1;
    memcpy(font_data, p, FONT_SIZE);
    font = font_data;
    return 0;
}

/* Port of text_print_to_bitmap. y is the baseline; the cell is 5 x 6 pixels from (x, y - 4).
   Bits 4-0 of a font byte are the 5 pixels; set pixels get fg, the others bg. The sixth column
   is not touched. Pixels outside bm are clipped. */
void text_draw(Bitmap *bm, int x, int y, uint8_t fg, uint8_t bg, const char *s)
{
    if (!font) return;
    for (; *s; s++, x += TEXT_CHAR_WIDTH) {
        unsigned c = (uint8_t)*s;
        for (int r = 0; r < 6; r++) {
            int py = y - 4 + r;
            if (py < 0 || py >= bm->h) continue;
            /* codes >= 128 read into the next font rows, as in the original */
            unsigned i = c + r * FONT_STRIDE;
            uint8_t bits = i < FONT_SIZE ? font[i] : 0;
            for (int k = 0; k < 5; k++) {
                int px = x + k;
                if (px >= 0 && px < bm->w) bm->px[py * bm->w + px] = bits >> (4 - k) & 1 ? fg : bg;
            }
        }
    }
}

/* Port of text_print_to_screen. */
void text_draw_screen(int x, int y, uint8_t fg, uint8_t bg, const char *s)
{
    Bitmap screen = { SCREEN_W, SCREEN_H, &fb[0][0] };
    text_draw(&screen, x, y, fg, bg, s);
}

/* Port of text_get_zone_size. Returns 0 for an empty text. */
int text_size(const char *s, int *w, int *h)
{
    int n = s ? (int)strlen(s) : 0;
    if (n * TEXT_CHAR_WIDTH - TEXT_CHAR_GAP < 1) return 0;
    *w = n * TEXT_CHAR_WIDTH - TEXT_CHAR_GAP;
    *h = TEXT_LINE_HEIGHT - TEXT_BASELINE_GAP;
    return 1;
}

/* Draws s in the box (x, y, w, h), left aligned or centered, vertically centered.
   The original gets this position from the zone data (text_print_to_bitmap_zone). */
void text_draw_box(Bitmap *bm, int x, int y, int w, int h, int centered, uint8_t fg, uint8_t bg, const char *s)
{
    int tw, th;
    if (!text_size(s, &tw, &th)) return;
    if (centered) x += (w - tw) / 2;
    y += (h - th) / 2;
    text_draw(bm, x, y + th - 1, fg, bg, s);
}

/* Port of text_get_next_word_line: copies from s + *pos the words that fit in max_width
   pixels into line, moves *pos, and returns the line width. A line ends at '\n' or '\0'
   (*pos stays on it) or after the last space that fits (*pos skips the space). */
int text_next_word_line(const char *s, char *line, int *pos, int max_width)
{
    int w = 0, space = -1, space_w = 0, space_n = 0, p = *pos, n = 0;
    for (;; p++, n++) {
        line[n] = 0;
        if (s[p] == '\n' || s[p] == 0) {
            *pos = p;
            return w;
        }
        if (s[p] == ' ') {
            space = p;
            space_w = w;
            space_n = n;
        }
        if (max_width <= w + TEXT_CHAR_WIDTH) break;
        w += TEXT_CHAR_WIDTH;
        line[n] = s[p];
    }
    if (space == -1) {
        *pos = p;
        return w;
    }
    *pos = space + 1;
    line[space_n] = 0;
    return space_w;
}
