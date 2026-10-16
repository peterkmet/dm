/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_LAYOUT_H
#define DM_UI_LAYOUT_H
/* Prefixes: layout_ */
#include "data/graphics.h"
#include <stdbool.h>
#include <stdint.h>

#define ZONE_SCALE_FULL 10000 /* layout_get_zone_box: the full zone size, in 1/10000 */

/* Keeps the raw layout graphic. Returns 0 on success. */
int layout_init(void);
/* Places an item of *width x *height pixels (0 = bitmap size) in zone and clips it to the
   parent rectangles. ZONE_SHIFTED adds *width, *height to the position and takes the bitmap size.
   On return *width, *height hold the source offset of the clipped box. Returns false when nothing is visible. */
bool layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *width, int16_t *height);
/* The box of zone with the size record of its parent, scaled by
   wp / ZONE_SCALE_FULL and hp / ZONE_SCALE_FULL. Returns 0 when nothing is visible. */
int layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box);

#endif
