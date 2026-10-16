/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MOVE_H
#define DM_ENGINE_MOVE_H
/* Prefixes: move_ */
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

extern int16_t g_new_party_map_index; /* -1: no map change pending */
extern int16_t g_move_result_map_x, g_move_result_map_y;
extern uint16_t g_move_result_map_index, g_move_result_cell, g_move_result_direction;
extern bool g_use_rope_to_climb_down_pit;
extern int g_party_teleport_count; /* port only: teleporters that moved the party */

/* Levitating groups, projectiles, and explosions do not fall into pits and do not trigger sensors when they leave a square. */
bool move_is_levitating(uint16_t thing);
/* A teleporter turns a projectile: its flight direction and its cell. */
uint16_t move_get_teleporter_rotated_projectile_thing(uint16_t teleporter, uint16_t thing);
/* thing THING_NONE is the party. src_x < 0: add only. dst_x < 0: remove only. The destination follows teleporters,
   pits, and stairs. Returns 0. A group moves with group_get_move_result (group.c). */
int16_t move_get_move_result(uint16_t thing, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y);
/* The map change of the game loop: the new map becomes the party map, then the party arrives with its sensors. */
void move_process_new_party_map(void);
/* New game: the party map becomes current and the party enters the start square. */
void move_party_enter_dungeon(void);
/* No map change and no rope climb wait at the start of a game. */
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
