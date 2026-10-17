/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_STATE_JSON_H
#define DM_PLATFORM_STATE_JSON_H
/* Prefixes: state_ */
#include "control/json.h"
#include <stdint.h>

/* The game as JSON for the turn-based mode: what a player sees, plus the explored map
   (docs/llm/llm-mode.md). No SDL. */

#define DECIMAL_BASE 10 /* request numbers, tenths, and the map grid digits are decimal */

/* Each appends one JSON object. */
void state_json_write_state(Json *j); /* party, champions, messages, actions, runes, panels */
void state_json_write_view(Json *j);  /* the squares in front of the party, 4 deep and the sides */
void state_json_map(Json *j);         /* the explored squares of the party map as text rows */

#define STATE_JSON_VISIBLE_GROUPS_MAX 12 /* four depths of three squares; a square holds one group */
/* The creature groups (their things) on the squares the party sees now, written to groups. Returns the count. */
int state_json_write_visible_groups(uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX]);

/* A thing on a square the party sees now: the view row (depth), the side name, and the map square. */
typedef struct {
    int16_t depth, x, y;
    const char *side;
    uint16_t thing;
} StateJsonSeenThing;
/* The things of a thing type on the squares the party sees now (no walls), in view order. Returns the count. */
int state_json_write_visible_things(uint16_t type, StateJsonSeenThing *out, int max);
/* Appends the name of an explosion type, or its number when it has no name. */
void state_json_explosion_name(Json *j, uint16_t type);
/* The name of a creature type, or "unknown". */
const char *state_json_creature_name(uint16_t type);

#define STATE_JSON_STATE 0
#define STATE_JSON_VIEW  1
#define STATE_JSON_MAP   2
/* One part as a new string (tests); release it with free. */
char *state_json_get_string(int part);

#endif
