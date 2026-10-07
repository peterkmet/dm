#ifndef AUTOMAP_H
#define AUTOMAP_H
/* Prefixes: automap_ */
#include <stdbool.h>
#include <stdint.h>
#include "draw.h"

/* Automap (not in the original game): remembers the squares the party has seen and draws them
   as a top-down map. */

#define AUTOMAP_CELL_SIZE 7 /* pixels per square: 32 squares fill the 224 pixel viewport */
#define AUTOMAP_RULER_WIDTH 12 /* the left ruler with the row numbers: two digits and a gap */
#define AUTOMAP_RULER_HEIGHT 7 /* the top ruler with the column numbers: one text row and a gap */

/* The rows of the view that the party sees from (x, y) looking in direction dir: the party row and the
   rows ahead up to the first center square that blocks sight (a wall, a closed fake wall, a closed door). */
int16_t automap_visible_depths(int16_t map, int16_t x, int16_t y, int16_t dir);
/* Marks the squares the dungeon view shows from (x, y) looking in direction dir on map. */
void automap_mark_seen(int16_t map, int16_t x, int16_t y, int16_t dir);
/* A side square of the view of a party at (x, y) facing dir is seen when the square ahead at its depth or the side
   square one row nearer is open; squares of the center lane and of the party row are always seen. */
bool automap_side_is_visible(int16_t map, int16_t x, int16_t y, int16_t dir, int16_t depth, int16_t side);

/* Draws the seen squares of map into area of target, centered on the party when it is on map, moved by
   the scroll. The map has a white frame; the top ruler numbers each second column, the left ruler each row. */
void automap_draw(Bitmap *target, Box area, int16_t map);
/* Moves the center of the drawn map by (dx, dy) squares; the center stays on the map. */
void automap_scroll(int16_t map, int16_t dx, int16_t dy);
/* Centers the drawn map on the party again. */
void automap_scroll_reset(void);

#endif
