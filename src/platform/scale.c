#include "platform/scale.h"
#include <stdbool.h>

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

void scale3x(const uint8_t *src, int width, int height, uint8_t *dst)
{
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            uint8_t A = pixel(src, width, height, x - 1, y - 1);
            uint8_t B = pixel(src, width, height, x, y - 1);
            uint8_t C = pixel(src, width, height, x + 1, y - 1);
            uint8_t D = pixel(src, width, height, x - 1, y);
            uint8_t E = pixel(src, width, height, x, y);
            uint8_t F = pixel(src, width, height, x + 1, y);
            uint8_t G = pixel(src, width, height, x - 1, y + 1);
            uint8_t H = pixel(src, width, height, x, y + 1);
            uint8_t I = pixel(src, width, height, x + 1, y + 1);
            uint8_t *out_top = dst + 3 * y * 3 * width + 3 * x;
            uint8_t *out_middle = out_top + 3 * width;
            uint8_t *out_bottom = out_middle + 3 * width;
            out_top[0] = E;
            out_top[1] = E;
            out_top[2] = E;
            out_middle[0] = E;
            out_middle[1] = E;
            out_middle[2] = E;
            out_bottom[0] = E;
            out_bottom[1] = E;
            out_bottom[2] = E;
            if (B == H || D == F)
                continue;
            out_top[0] = D == B ? D : E;
            out_top[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
            out_top[2] = B == F ? F : E;
            out_middle[0] = (D == B && E != G) || (D == H && E != A) ? D : E;
            out_middle[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
            out_bottom[0] = D == H ? D : E;
            out_bottom[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
            out_bottom[2] = H == F ? F : E;
        }
}
