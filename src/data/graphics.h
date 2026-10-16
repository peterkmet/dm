/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_GRAPHICS_H
#define DM_DATA_GRAPHICS_H
/* Prefixes: gfx_ box_ */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Amiga planar graphics: 4 bitplanes, 16 pixels per word. */
#define BITPLANE_COUNT  4
#define PIXELS_PER_WORD 16

/* Text metrics of the built-in font. */
#define FONT_CHAR_WIDTH   6 /* advance per character */
#define FONT_CHAR_GAP     1 /* blank columns after each character */
#define FONT_LINE_HEIGHT  6 /* rows of a character */
#define FONT_BASELINE_GAP 1 /* blank rows below a line of text */

typedef struct {
    uint16_t w, h;
    uint8_t *px; /* w * h bytes, palette index 0-15 */
} Bitmap;

/* A rectangle of the screen or of a bitmap, in pixels. */
typedef struct {
    int16_t x, y, w, h;
} Box;
/* True when the pixel x, y is inside the box. */
bool box_contains(const Box *box, int16_t x, int16_t y);

/* Frees Graphics.DAT and the decoded bitmaps. */
void gfx_close(void);
/* Reads a format 1 Graphics.DAT like graphics_dat_read_header. False when the file is missing, damaged or has no items. */
bool gfx_open(const char *path);
/* The item count of the open Graphics.DAT; exported for the tests. */
int gfx_count(void);
/* Packed bytes of item i. */
const uint8_t *gfx_get_item(int i, uint16_t *size);
/* Port of expand_graphic: decodes a packed graphic into a chunky bitmap. */
bool gfx_expand(const uint8_t *packed, size_t len, Bitmap *bm);
/* Decodes a packed graphic over a bitmap of the same size: skipped pixels keep their color. */
bool gfx_expand_over(const uint8_t *packed, size_t len, Bitmap *bm);
/* Decodes graphic i into a new bitmap. False when it does not decode. */
bool gfx_load_bitmap(int16_t i, Bitmap *bm);
void gfx_free_bitmap(Bitmap *bm);
/* Decoded graphic i, kept until the next gfx_open. NULL when it does not decode. */
const Bitmap *gfx_get_bitmap(int i);

/* Loads the string lists of the game data. Call after gfx_open and csb_text_build. */
bool gfx_load_strings(void);
/* A text of graphic GFX_STRINGS. */
const char *gfx_get_string(int16_t index);
/* An object name of graphic GFX_OBJECT_NAMES. */
const char *gfx_get_object_name(int16_t icon);
/* An action name of graphic GFX_ACTION_NAMES. */
const char *gfx_get_action_name(int16_t action);

#endif
