/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MOVE_H
#define DM_ENGINE_MOVE_H
/* Prefixes: move_ */
#include "engine/map.h"
#include <stdbool.h>
#include <stdint.h>

/* Party and object movement (KAOS range 8, MOVESENS.C and COMMAND.C). */

#define SQUARE_CHAIN_MAXIMUM 99 /* teleporters, pits, and stairs that one move follows */

#define COMMAND_TURN_LEFT     1
#define COMMAND_TURN_RIGHT    2
#define COMMAND_MOVE_FORWARD  3
#define COMMAND_MOVE_RIGHT    4
#define COMMAND_MOVE_BACKWARD 5
#define COMMAND_MOVE_LEFT     6

extern int g_party_teleport_count; /* port only: teleporters that moved the party */

/* Levitating groups, projectiles, and explosions do not fall into pits and do not trigger sensors when they leave a square. */
bool move_is_levitating(uint16_t thing);
/* A teleporter turns a projectile: its flight direction and its cell. */
uint16_t move_get_teleporter_rotated_projectile_thing(uint16_t teleporter, uint16_t thing, int16_t *direction);

/* Results of move_get_move_result. */
typedef enum {
    MOVE_RESULT_DONE,    /* the thing is on its destination (or stays where it is) */
    MOVE_RESULT_STOPPED, /* the mover died, was deleted, waits off the map, or became active or inactive */
    MOVE_RESULT_TURNED,  /* a teleporter turned an attacking group, which stopped its attack */
} MoveResult;

/* One move of a thing; THING_NONE is the party. */
typedef struct {
    uint16_t thing;
    Location from;   /* not read when enters */
    Location to;     /* not read when leaves */
    bool enters;     /* the thing has no source square: it appears on the map */
    bool leaves;     /* the thing has no destination: it leaves the map */
    bool keeps_cell; /* a teleporter does not turn the thing (an object that a launcher creates) */
    bool uses_rope;  /* the party climbs down with a rope: no fall damage */
} Move;

/* Where a move ended: the square after teleporters, pits and stairs, the cell and the direction. */
typedef struct {
    int16_t map, x, y, cell, direction;
} MoveDestination;

/* Moves a thing (the party, a group, an object, a projectile) and follows teleporters, pits and stairs.
   Fills destination (when not NULL) for a move with a destination. */
MoveResult move_get_move_result(const Move *move, MoveDestination *destination);
/* The map change of the game loop: the new map becomes the party map, then the party arrives with its sensors. */
void move_process_new_party_map(void);
/* New game: the party map becomes current and the party enters the start square. */
void move_party_enter_dungeon(void);
/* No map change waits at the start of a game. */
void move_reset(void);

/* Turns the party. Each champion keeps its place relative to the party. The loop also runs one time without champions. */
void move_set_party_direction(int16_t direction);

/* The party leaves the stairs square and arrives on the other level facing out of the stairs. */
void move_take_stairs(bool stairs_go_up);
/* On stairs, a turn takes the stairs. The party leaves the square sensors before the turn and enters them after it. */
void move_turn_party(int16_t command);
/* One step of the party. With champions, a bump hurts two champions and a group in the way blocks the step and
   reacts to the party. */
void move_party(int16_t command);

#endif
