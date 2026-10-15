#ifndef GRAPHICS_H
#define GRAPHICS_H
/* Prefixes: gfx_ */
#include <stddef.h>
#include <stdint.h>

/* Amiga planar graphics: 4 bitplanes, 16 pixels per word. */
#define BITPLANE_COUNT  4
#define PIXELS_PER_WORD 16

/* Text metrics of the built-in font. */
#define TEXT_CHAR_WIDTH   6 /* advance per character */
#define TEXT_CHAR_GAP     1 /* blank columns after each character */
#define TEXT_LINE_HEIGHT  6 /* rows of a character */
#define TEXT_BASELINE_GAP 1 /* blank rows below a line of text */

typedef struct {
    uint16_t w, h;
    uint8_t *px; /* w * h bytes, palette index 0-15 */
} Bitmap;

int gfx_open(const char *path);
int gfx_count(void);
const uint8_t *gfx_get_item(int i, uint16_t *size);
int gfx_expand(const uint8_t *packed, size_t len, Bitmap *bm);
int gfx_expand_over(const uint8_t *packed, size_t len, Bitmap *bm);
int gfx_load_bitmap(int i, Bitmap *bm);
void gfx_free_bitmap(Bitmap *bm);
const Bitmap *gfx_get_bitmap(int i);

/* Loads the string lists of the game data. Call after gfx_open and csb_text_build. */
int gfx_load_strings(void);
const char *gfx_string(int16_t index);       /* a text of graphic 700 */
const char *gfx_object_name(int16_t icon);   /* an object name of graphic 0x2b6 */
const char *gfx_action_name(int16_t action); /* an action name of graphic 699 */

#endif
