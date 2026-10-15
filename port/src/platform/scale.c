#include <stdbool.h>
#include "scale.h"

/* The source pixel at (x, y); positions outside the image repeat the edge pixel. */
static uint8_t pixel(const uint8_t *src, int w, int h, int x, int y)
{
    x = x < 0 ? 0 : x >= w ? w - 1 : x;
    y = y < 0 ? 0 : y >= h ? h - 1 : y;
    return src[y * w + x];
}

/* Neighbors use the AdvMAME names: A B C above, D E F on the row (E the pixel), G H I below. */
void scale2x(const uint8_t *src, int w, int h, uint8_t *dst)
{
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
            uint8_t *o = dst + 3 * y * 3 * w + 3 * x;
            uint8_t *o1 = o + 3 * w;
            uint8_t *o2 = o1 + 3 * w;
            o[0] = E;
            o[1] = E;
            o[2] = E;
            o1[0] = E;
            o1[1] = E;
            o1[2] = E;
            o2[0] = E;
            o2[1] = E;
            o2[2] = E;
            if (B == H || D == F)
                continue;
            o[0] = D == B ? D : E;
            o[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
            o[2] = B == F ? F : E;
            o1[0] = (D == B && E != G) || (D == H && E != A) ? D : E;
            o1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
            o2[0] = D == H ? D : E;
            o2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
            o2[2] = H == F ? F : E;
        }
}
