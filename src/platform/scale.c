/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scale.h"
#include "video.h"
#include <math.h>
#include <stdbool.h>

/* The pixels of an output row of Scale3x. */
#define COLUMN_LEFT   0
#define COLUMN_MIDDLE 1
#define COLUMN_RIGHT  2

/* The source pixel at (x, y); positions outside the image repeat the edge pixel. */
static uint8_t pixel(const uint8_t *src, int w, int h, int x, int y)
{
    x = x < 0 ? 0 : x >= w ? w - 1 : x;
    y = y < 0 ? 0 : y >= h ? h - 1 : y;
    return src[y * w + x];
}

void scale2x(const uint8_t *src, int w, int h, uint8_t *dst)
{
    /* Neighbors use the AdvMAME names: A B C above, D E F on the row (E the pixel), G H I below. */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t B = pixel(src, w, h, x, y - 1);
            uint8_t D = pixel(src, w, h, x - 1, y);
            uint8_t E = pixel(src, w, h, x, y);
            uint8_t F = pixel(src, w, h, x + 1, y);
            uint8_t H = pixel(src, w, h, x, y + 1);
            uint8_t *o = dst + 2 * y * 2 * w + 2 * x;
            bool on = B != H && D != F;
            o[0] = on && D == B ? D : E;
            o[1] = on && B == F ? F : E;
            o[2 * w] = on && D == H ? D : E;
            o[2 * w + 1] = on && H == F ? F : E;
        }
}

void scale3x(const uint8_t *src, int w, int h, uint8_t *dst)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t A = pixel(src, w, h, x - 1, y - 1);
            uint8_t B = pixel(src, w, h, x, y - 1);
            uint8_t C = pixel(src, w, h, x + 1, y - 1);
            uint8_t D = pixel(src, w, h, x - 1, y);
            uint8_t E = pixel(src, w, h, x, y);
            uint8_t F = pixel(src, w, h, x + 1, y);
            uint8_t G = pixel(src, w, h, x - 1, y + 1);
            uint8_t H = pixel(src, w, h, x, y + 1);
            uint8_t I = pixel(src, w, h, x + 1, y + 1);
            uint8_t *out_top = dst + 3 * y * 3 * w + 3 * x;
            uint8_t *out_middle = out_top + 3 * w;
            uint8_t *out_bottom = out_middle + 3 * w;
            out_top[COLUMN_LEFT] = E;
            out_top[COLUMN_MIDDLE] = E;
            out_top[COLUMN_RIGHT] = E;
            out_middle[COLUMN_LEFT] = E;
            out_middle[COLUMN_MIDDLE] = E;
            out_middle[COLUMN_RIGHT] = E;
            out_bottom[COLUMN_LEFT] = E;
            out_bottom[COLUMN_MIDDLE] = E;
            out_bottom[COLUMN_RIGHT] = E;
            if (B == H || D == F)
                continue;
            out_top[COLUMN_LEFT] = D == B ? D : E;
            out_top[COLUMN_MIDDLE] = (D == B && E != C) || (B == F && E != A) ? B : E;
            out_top[COLUMN_RIGHT] = B == F ? F : E;
            out_middle[COLUMN_LEFT] = (D == B && E != G) || (D == H && E != A) ? D : E;
            out_middle[COLUMN_RIGHT] = (B == F && E != I) || (H == F && E != C) ? F : E;
            out_bottom[COLUMN_LEFT] = D == H ? D : E;
            out_bottom[COLUMN_MIDDLE] = (D == H && E != I) || (H == F && E != G) ? H : E;
            out_bottom[COLUMN_RIGHT] = H == F ? F : E;
        }
}

OutputLayout output_layout(int out_w, int out_h)
{
    float sx = (float)out_w / SCREEN_W;
    float sy = (float)out_h / SCREEN_H;
    float s = sx < sy ? sx : sy;
    if (s <= 0)
        s = 1; /* minimized window: avoid a zero scale */
    return (OutputLayout){s, (out_w - SCREEN_W * s) / 2, (out_h - SCREEN_H * s) / 2};
}

static int clamp(int v, int max)
{
    return v < 0 ? 0 : v > max ? max : v;
}

void output_map_mouse(const OutputLayout *layout, int out_x, int out_y, int *gx, int *gy)
{
    *gx = clamp((int)floorf((out_x - layout->x) / layout->scale), SCREEN_W - 1);
    *gy = clamp((int)floorf((out_y - layout->y) / layout->scale), SCREEN_H - 1);
}
