#ifndef EXPLORED_H
#define EXPLORED_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Prefixes: explored_ */

/* Explored squares (not in the original game): the squares the party has seen, for the automap. */

#define EXPLORED_SAVE_MAGIC "AMAP" /* the first 4 bytes of the save block */

/* Forgets every seen square and sizes the storage from the loaded dungeon. */
void explored_reset(void);
/* Marks square (x, y) of map as seen; a square outside the map is ignored. */
void explored_mark(int16_t map, int16_t x, int16_t y);
bool explored_is_seen(int16_t map, int16_t x, int16_t y);

/* Save block: a malloc'd buffer the caller frees. Both return 0 on success, -1 on failure. */
int explored_save(uint8_t **buf, size_t *len);
int explored_load(const uint8_t *buf, size_t len);

#endif
