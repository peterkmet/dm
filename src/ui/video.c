/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/video.h"
#include <stdbool.h>

#define ARGB_OPAQUE         0xff000000u
#define AMIGA_CHANNEL_BITS  4
#define AMIGA_CHANNEL_MASK  0xf
#define CHANNEL_4_TO_8_BITS 17 /* 15 * 17 = 255 */

uint8_t fb[SCREEN_H][SCREEN_W] = {0};
uint16_t palette[2 * PALETTE_COLORS];
int16_t palette_split_top = 0;
int16_t palette_split_bottom = SCREEN_H;

uint16_t video_get_color(int y, uint8_t c)
{
    bool bottom = y < palette_split_top || y >= palette_split_bottom;
    return palette[(c & COLOR_INDEX_MASK) | (bottom ? PALETTE_COLORS : 0)];
}

uint32_t video_convert_argb(uint16_t amiga)
{
    uint32_t r = amiga >> 2 * AMIGA_CHANNEL_BITS & AMIGA_CHANNEL_MASK;
    uint32_t g = amiga >> AMIGA_CHANNEL_BITS & AMIGA_CHANNEL_MASK;
    uint32_t b = amiga & AMIGA_CHANNEL_MASK;
    return ARGB_OPAQUE | (r * CHANNEL_4_TO_8_BITS) << ARGB_RED_SHIFT | (g * CHANNEL_4_TO_8_BITS) << ARGB_GREEN_SHIFT |
           b * CHANNEL_4_TO_8_BITS;
}
