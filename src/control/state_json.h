/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_STATE_JSON_H
#define DM_CONTROL_STATE_JSON_H
/* Prefixes: state_ */

/* The game as JSON for the program player: the party, the champions, what the party sees, and the explored
   map (docs/llm/llm-mode.md). It reads the game and changes nothing. */

#include "control/json.h"
#include <stdint.h>

/* Parts of state_get_string. */
#define STATE_PART_STATE 0
#define STATE_PART_VIEW  1
#define STATE_PART_MAP   2

/* Counts of the visible things. */
#define STATE_VISIBLE_GROUPS_MAX 12 /* four depths of three squares; a square holds one group */

/* A thing on a square that the party sees now: the view row (depth), the side name, and the map square. */
typedef struct {
    int16_t depth;
    int16_t x;
    int16_t y;
    const char *side;
    uint16_t thing;
} StateSeenThing;

/* Writers */
/* Appends the party, champions, messages, actions, runes and panels as one object. */
void state_write_state(Json *j);
/* Appends the squares in front of the party, 4 deep and the sides, as one object. */
void state_write_view(Json *j);
/* Appends the explored squares of the party map as text rows. */
void state_write_map(Json *j);
/* Appends the name of an explosion type, or its number when it has no name. */
void state_write_explosion_name(Json *j, uint16_t type);

/* Queries */
/* The creature groups (their things) on the squares the party sees now. Returns the count. */
int state_get_visible_groups(uint16_t groups[STATE_VISIBLE_GROUPS_MAX]);
/* The things of a thing type on the squares the party sees now (no walls), in view order. Returns the count. */
int state_get_visible_things(uint16_t type, StateSeenThing *things, int max);
/* The name of a creature type, or "unknown". */
const char *state_get_creature_name(uint16_t type);
/* One part (STATE_PART_*) as a new string; release it with free. */
char *state_get_string(int16_t part); /* exported for the tests */

#endif
