/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_VIDEO_H
#define DM_UI_VIDEO_H
/* Prefixes: video_ */

/* The screen pixels and palettes (g_screen) and the Amiga color conversion. It must not draw and must not call SDL. */

#include "data/graphics.h"
#include <stdint.h>

/* Screen size, palette size and frame rate. */
#define SCREEN_W          320
#define SCREEN_H          200
#define PALETTE_COLORS    16 /* colors of one palette half */
#define FRAMES_PER_SECOND 50 /* PAL vertical blank */

/* The palette index bits of a pixel. */
#define COLOR_INDEX_MASK     (PALETTE_COLORS - 1)
/* A pixel bit above the index: the pixel uses the interface palette half on any row. */
#define COLOR_INTERFACE_FLAG 0x10

/* Bit positions of the color components. */
#define ARGB_RED_SHIFT    16 /* bit position of the red byte in a video_convert_argb color */
#define ARGB_GREEN_SHIFT  8
#define AMIGA_RED_SHIFT   8 /* bit position of the red nibble in an Amiga 12-bit color */
#define AMIGA_GREEN_SHIFT 4

/* The channels of a video_convert_argb color. */
static inline uint8_t video_argb_red(uint32_t argb)
{
    return (uint8_t)(argb >> ARGB_RED_SHIFT); /* the low byte is the channel */
}
static inline uint8_t video_argb_green(uint32_t argb)
{
    return (uint8_t)(argb >> ARGB_GREEN_SHIFT); /* the low byte is the channel */
}
static inline uint8_t video_argb_blue(uint32_t argb)
{
    return (uint8_t)argb; /* the low byte is the channel */
}

/* The Amiga screen: chunky pixels of palette indices and the two palette halves of the copper list. Rows
   split_top to split_bottom - 1 use the top palette (palette[0..15], the dungeon view), the other rows the bottom
   palette (palette[16..31], the interface); a pixel with COLOR_INTERFACE_FLAG uses the bottom palette on any row.
   The default puts every row in the top palette. */
typedef struct {
    uint8_t px[SCREEN_H][SCREEN_W];
    uint16_t palette[2 * PALETTE_COLORS];
    int16_t split_top, split_bottom;
} Screen;
extern Screen g_screen;

/* The screen as a bitmap. */
static inline Bitmap video_get_screen(void)
{
    return (Bitmap){SCREEN_W, SCREEN_H, &g_screen.px[0][0]};
}
/* An Amiga 12-bit color from 4-bit red, green, blue. */
static inline uint16_t video_amiga_color(uint16_t red, uint16_t green, uint16_t blue)
{
    return (uint16_t)(red << AMIGA_RED_SHIFT | green << AMIGA_GREEN_SHIFT | blue);
}

/* Amiga color of index color on screen row y; COLOR_INTERFACE_FLAG selects the interface palette half. */
uint16_t video_get_color(int16_t y, uint8_t color);
/* Converts an Amiga 12-bit color (4 bits each of red, green, blue) to ARGB8888. */
uint32_t video_convert_argb(uint16_t amiga);

#endif
