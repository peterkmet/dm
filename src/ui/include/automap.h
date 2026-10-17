/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_AUTOMAP_H
#define DM_UI_AUTOMAP_H
/* Prefixes: automap_ */

/* Automap (not in the original game): draws the squares the party has seen (engine/explored owns them) as a
   top-down map. */

#include "graphics.h"
#include <stdbool.h>
#include <stdint.h>

/* Automap geometry, in pixels. */
#define AUTOMAP_CELL_SIZE    7  /* pixels per square: 32 squares fill the 224 pixel viewport */
#define AUTOMAP_RULER_WIDTH  12 /* the left ruler with the row numbers: two digits and a gap */
#define AUTOMAP_RULER_HEIGHT 7  /* the top ruler with the column numbers: one text row and a gap */

/* Moves the center of the drawn map by (dx, dy) squares; the center stays on the map. */
void automap_scroll(int16_t map, int16_t dx, int16_t dy);
/* Centers the drawn map on the party again. */
void automap_scroll_reset(void);
/* Draws the seen squares of map (all squares with the reveal map cheat) into area of target, centered on the
   party when it is on map, moved by the scroll. The map has a white frame; the top ruler numbers each second
   column, the left ruler each row. */
void automap_draw(Bitmap *target, const Box *area, int16_t map);

#endif
