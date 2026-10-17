/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_DISCARD_H
#define DM_ENGINE_DISCARD_H
/* Prefixes: discard_ */

/* The record of a new thing: a free record, or, when no record of the type is free, the record of a thing that
   disappears from a square far from the party. It never discards a thing near the party or on a sensor square. */

#include <stdint.h>

/* Setup */
/* The search of each thing type starts again on map 0. */
void discard_reset(void);

/* Queries */
/* The map where the last search for a thing of the type ended. */
int16_t discard_get_last_map(uint16_t thing_type); /* exported for the tests */

/* Commands */
/* A cleared record of the type, as rec_find_unused_thing gives it. When no record is free, a thing of the type on a
   far square disappears and its record is cleared. THING_NONE when no thing can disappear. */
uint16_t discard_get_unused_thing(uint16_t thing_type);

#endif
