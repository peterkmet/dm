/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_SCALE_H
#define DM_PLATFORM_SCALE_H
/* Prefixes: scale2x scale3x output_ */
#include <stdint.h>

/* Placement of the game screen in the output: scale and the offset of its top left corner. */
typedef struct {
    float scale, x, y;
} OutputLayout;

/* AdvMAME Scale2x/Scale3x on palette indices; dst is (w*n) x (h*n), edges replicate. */
void scale2x(const uint8_t *src, int w, int h, uint8_t *dst);
void scale3x(const uint8_t *src, int w, int h, uint8_t *dst);
/* Largest scale that fits the output with the screen's aspect ratio, centered (letterbox). */
OutputLayout output_layout(int out_w, int out_h);
/* Maps output pixel coordinates to game coordinates, clamped to the screen. */
void output_map_mouse(const OutputLayout *layout, int x, int y, int *gx, int *gy);

#endif
