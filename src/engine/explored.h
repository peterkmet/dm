/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_EXPLORED_H
#define DM_ENGINE_EXPLORED_H
#include "engine/map.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Prefixes: explored_ */

/* Explored squares (not in the original game): the squares the party has seen, for the automap. */

/* Magic of the explored block. */
#define EXPLORED_SAVE_MAGIC "AMAP" /* the first 4 bytes of the save block */

/* Forgets every seen square and sizes the storage from the loaded dungeon. */
void explored_reset(void);
bool explored_is_seen(int16_t map, int16_t x, int16_t y);

/* The rows of the view: the party row and 3 rows ahead. */
#define EXPLORED_VIEW_DEPTHS 4
/* The rows that the party sees from place facing direction: up to the first center square that blocks sight
   (a wall, a closed fake wall, a closed door), and fewer in the two darkest light levels. */
int16_t explored_get_visible_depths(Position place, int16_t direction);
/* True when the party at place facing direction sees the square depth rows ahead and right squares to the right
   (-1 left, 1 right); squares of the center lane and of the party row are always seen. */
bool explored_is_side_visible(Position place, int16_t direction, int16_t depth, int16_t right);
/* Marks the squares that the party sees from place facing direction as seen. */
void explored_mark_visible(Position place, int16_t direction);

/* Save block: a malloc'd buffer the caller frees. Both return true on success. */
bool explored_save(uint8_t **out_data, size_t *out_length);
bool explored_load(const uint8_t *buf, size_t len);

#endif
