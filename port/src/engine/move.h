#ifndef MOVE_H
#define MOVE_H
/* Prefixes: move_ */
#include <stdbool.h>
#include <stdint.h>

/* Party and object movement (KAOS range 8, MOVESENS.C and COMMAND.C). */

#define SQUARE_CHAIN_MAXIMUM 99 /* teleporters, pits, and stairs that one move follows */

enum {
    COMMAND_TURN_LEFT = 1,
    COMMAND_TURN_RIGHT,
    COMMAND_MOVE_FORWARD,
    COMMAND_MOVE_RIGHT,
    COMMAND_MOVE_BACKWARD,
    COMMAND_MOVE_LEFT
};

extern int16_t g_new_party_map_index; /* -1: no map change pending */
extern int16_t g_move_result_map_x, g_move_result_map_y;
extern uint16_t g_move_result_map_index, g_move_result_cell, g_move_result_direction;
extern int16_t g_use_rope_to_climb_down_pit;
extern int g_party_teleport_count; /* port only: teleporters that moved the party */

bool move_is_levitating(uint16_t thing);
uint16_t move_get_teleporter_rotated_projectile_thing(uint16_t teleporter, uint16_t thing);
int16_t move_get_move_result(uint16_t thing, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y);
void move_process_new_party_map(void);
void move_party_enter_dungeon(void);
void move_reset(void);

void move_set_party_direction(int16_t direction);
/* A scent word: x in bits 11-15, y in bits 6-10, map in bits 0-5. */
void move_add_scent_strength(int16_t x, int16_t y, int16_t strength_and_flags);

void move_take_stairs(int16_t stairs_go_up);
void move_turn_party(int16_t command);
void move_party(int16_t command);

#endif
