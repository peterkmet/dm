#include "scale.h"

#define P(xx, yy) src[((yy) < 0 ? 0 : (yy) >= h ? h - 1 : (yy)) * w + \
                      ((xx) < 0 ? 0 : (xx) >= w ? w - 1 : (xx))]

void scale2x(const uint8_t *src, int w, int h, uint8_t *dst)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t B = P(x, y - 1), D = P(x - 1, y), E = P(x, y), F = P(x + 1, y), H = P(x, y + 1);
            uint8_t *o = dst + 2 * y * 2 * w + 2 * x;
            int on = B != H && D != F;
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
            uint8_t A = P(x - 1, y - 1), B = P(x, y - 1), C = P(x + 1, y - 1);
            uint8_t D = P(x - 1, y), E = P(x, y), F = P(x + 1, y);
            uint8_t G = P(x - 1, y + 1), H = P(x, y + 1), I = P(x + 1, y + 1);
            uint8_t *o = dst + 3 * y * 3 * w + 3 * x, *o1 = o + 3 * w, *o2 = o1 + 3 * w;
            o[0] = o[1] = o[2] = o1[0] = o1[1] = o1[2] = o2[0] = o2[1] = o2[2] = E;
            if (B == H || D == F) continue;
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
