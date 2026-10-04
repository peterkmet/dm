#include "video.h"

uint8_t fb[SCREEN_H][SCREEN_W] = { 0 };
uint16_t palette[32];
int16_t palette_split_top = 0, palette_split_bottom = SCREEN_H;

/* Amiga color of index c on screen row y. */
uint16_t video_color(int y, uint8_t c)
{
    return palette[(c & 15) | (y >= palette_split_top && y < palette_split_bottom ? 0 : 16)];
}

/* Converts Amiga 0x0RGB to ARGB8888. */
uint32_t video_argb(uint16_t amiga)
{
    uint32_t r = (amiga >> 8) & 15, g = (amiga >> 4) & 15, b = amiga & 15;
    return 0xFF000000u | (r * 17) << 16 | (g * 17) << 8 | b * 17;
}

/* Largest integer scale that fits the output, centered (letterbox). */
void video_layout(int out_w, int out_h, int *scale, int *ox, int *oy)
{
    int sx = out_w / SCREEN_W, sy = out_h / SCREEN_H;
    int s = sx < sy ? sx : sy;
    if (s < 1) s = 1;
    *scale = s;
    *ox = (out_w - SCREEN_W * s) / 2;
    *oy = (out_h - SCREEN_H * s) / 2;
}

static int clamp(int v, int max) { return v < 0 ? 0 : v > max ? max : v; }

/* Maps output pixel coordinates to game coordinates, clamped to the screen. */
void video_map_mouse(int out_w, int out_h, int x, int y, int *gx, int *gy)
{
    int s, ox, oy;
    video_layout(out_w, out_h, &s, &ox, &oy);
    *gx = clamp((x - ox) / s, SCREEN_W - 1);
    *gy = clamp((y - oy) / s, SCREEN_H - 1);
}
