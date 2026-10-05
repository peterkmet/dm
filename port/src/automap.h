#ifndef AUTOMAP_H
#define AUTOMAP_H
#include <stddef.h>
#include <stdint.h>
#include "draw.h"

/* Automap (not in the original game): remembers the squares the party has seen and draws them
   as a top-down map. */

#define AUTOMAP_CELL_SIZE 7 /* pixels per square: 32 squares fill the 224 pixel viewport */

/* Forgets every seen square and sizes the storage from the loaded dungeon. */
void automap_reset(void);
/* The rows of the view that the party sees from (x, y) looking in direction dir: the party row and the
   rows ahead up to the first center square that blocks sight (a wall, a closed fake wall, a closed door). */
int16_t automap_visible_depths(int16_t map, int16_t x, int16_t y, int16_t dir);
/* Marks the squares the dungeon view shows from (x, y) looking in direction dir on map. */
void automap_mark_seen(int16_t map, int16_t x, int16_t y, int16_t dir);
int automap_is_seen(int16_t map, int16_t x, int16_t y);
/* A side square of the view of a party at (x, y) facing dir is seen when the square ahead at its depth or the side
   square one row nearer is open; squares of the center lane and of the party row are always seen. */
int automap_side_is_visible(int16_t map, int16_t x, int16_t y, int16_t dir, int16_t depth, int16_t side);

#define AUTOMAP_SAVE_MAGIC "AMAP" /* the first 4 bytes of the save block */

/* Save block: a malloc'd buffer the caller frees. Both return 0 on success, -1 on failure. */
int automap_save(uint8_t **buf, size_t *len);
int automap_load(const uint8_t *buf, size_t len);

/* Draws the seen squares of map into area of target, centered on the party when it is on map, moved by
   the scroll. */
void automap_draw(Bitmap *target, Box area, int16_t map);
/* Moves the center of the drawn map by (dx, dy) squares; the center stays on the map. */
void automap_scroll(int16_t map, int16_t dx, int16_t dy);
/* Centers the drawn map on the party again. */
void automap_scroll_reset(void);

#endif
