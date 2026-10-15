#include <math.h>
#include <stdbool.h>
#include "video.h"

#define ARGB_OPAQUE         0xff000000u
#define AMIGA_CHANNEL_BITS  4
#define AMIGA_CHANNEL_MASK  0xf
#define CHANNEL_4_TO_8_BITS 17 /* 15 * 17 = 255 */

uint8_t fb[SCREEN_H][SCREEN_W] = {0};
uint16_t palette[2 * PALETTE_COLORS];
int16_t palette_split_top = 0;
int16_t palette_split_bottom = SCREEN_H;

uint16_t video_color(int y, uint8_t c)
{
    bool bottom = y < palette_split_top || y >= palette_split_bottom;
    return palette[(c & COLOR_INDEX_MASK) | (bottom ? PALETTE_COLORS : 0)];
}

uint32_t video_argb(uint16_t amiga)
{
    uint32_t r = amiga >> 2 * AMIGA_CHANNEL_BITS & AMIGA_CHANNEL_MASK;
    uint32_t g = amiga >> AMIGA_CHANNEL_BITS & AMIGA_CHANNEL_MASK;
    uint32_t b = amiga & AMIGA_CHANNEL_MASK;
    return ARGB_OPAQUE | (r * CHANNEL_4_TO_8_BITS) << ARGB_RED_SHIFT | (g * CHANNEL_4_TO_8_BITS) << ARGB_GREEN_SHIFT |
           b * CHANNEL_4_TO_8_BITS;
}

void video_layout(int out_w, int out_h, float *scale, float *ox, float *oy)
{
    float sx = (float)out_w / SCREEN_W;
    float sy = (float)out_h / SCREEN_H;
    float s = sx < sy ? sx : sy;
    if (s <= 0)
        s = 1; /* minimized window: avoid a zero scale */
    *scale = s;
    *ox = (out_w - SCREEN_W * s) / 2;
    *oy = (out_h - SCREEN_H * s) / 2;
}

static int clamp(int v, int max)
{
    return v < 0 ? 0 : v > max ? max : v;
}

void video_map_mouse(int out_w, int out_h, int x, int y, int *gx, int *gy)
{
    float s, ox, oy;
    video_layout(out_w, out_h, &s, &ox, &oy);
    *gx = clamp((int)floorf((x - ox) / s), SCREEN_W - 1);
    *gy = clamp((int)floorf((y - oy) / s), SCREEN_H - 1);
}
