#ifndef SCALE_H
#define SCALE_H
#include <stdint.h>

/* AdvMAME Scale2x/Scale3x on palette indices; dst is (w*n) x (h*n), edges replicate. */
void scale2x(const uint8_t *src, int w, int h, uint8_t *dst);
void scale3x(const uint8_t *src, int w, int h, uint8_t *dst);

#endif
