/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_LAYOUT_H
#define DM_UI_LAYOUT_H
/* Prefixes: layout_ */

/* The screen layout from the layout graphic of Graphics.DAT: it finds the box of a zone on the screen. It owns the
   layout data only; it draws nothing. */

#include "data/graphics.h"
#include <stdbool.h>
#include <stdint.h>

/* layout_get_zone_box: the full zone size, in 1/10000. */
#define ZONE_SCALE_FULL 10000

/* Keeps the raw layout graphic. Returns true on success. */
bool layout_init(void);
/* An item to place in a zone: the bitmap size, and the item size w x h (0 = bitmap size). In a ZONE_SHIFTED zone,
   w and h move the position and the item has the bitmap size. */
typedef struct {
    int16_t bitmap_w, bitmap_h;
    int16_t w, h;
} ZoneItem;

/* A placed item: the box on the screen, clipped to the parent rectangles, and the source offset of the box. */
typedef struct {
    Box box;
    int16_t source_x, source_y;
} ZoneClip;

/* Places the item of sizes in zone and clips it to the parent rectangles. Returns false when nothing is visible. */
bool layout_get_zone(int16_t zone, const ZoneItem *sizes, ZoneClip *placed);
/* The box of zone with the size record of its parent, scaled by
   wp / ZONE_SCALE_FULL and hp / ZONE_SCALE_FULL. Returns false when nothing is visible. */
bool layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box);

#endif
