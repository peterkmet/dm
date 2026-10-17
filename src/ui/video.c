/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/video.h"
#include <stdbool.h>

/* Alpha of an opaque ARGB pixel. */
#define ARGB_OPAQUE 0xff000000u

/* One 4-bit component of an Amiga color. */
#define AMIGA_CHANNEL_MASK 0xf

/* Widens a 4-bit component to 8 bits: 15 * 17 = 255. */
#define CHANNEL_4_TO_8_FACTOR 17

Screen g_screen = {.split_bottom = SCREEN_H};

uint16_t video_get_color(int16_t y, uint8_t color)
{
    bool bottom = y < g_screen.split_top || y >= g_screen.split_bottom || color & COLOR_INTERFACE_FLAG;
    return g_screen.palette[(color & COLOR_INDEX_MASK) | (bottom ? PALETTE_COLORS : 0)];
}

uint32_t video_convert_argb(uint16_t amiga)
{
    uint32_t r = amiga >> AMIGA_RED_SHIFT & AMIGA_CHANNEL_MASK;
    uint32_t g = amiga >> AMIGA_GREEN_SHIFT & AMIGA_CHANNEL_MASK;
    uint32_t b = amiga & AMIGA_CHANNEL_MASK;
    return ARGB_OPAQUE | (r * CHANNEL_4_TO_8_FACTOR) << ARGB_RED_SHIFT |
           (g * CHANNEL_4_TO_8_FACTOR) << ARGB_GREEN_SHIFT | b * CHANNEL_4_TO_8_FACTOR;
}
