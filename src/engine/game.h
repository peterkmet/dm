/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_GAME_H
#define DM_ENGINE_GAME_H
/* Prefixes: game_ turn_ */
#include <stdbool.h>
#include <stdint.h>

#define NORMALIZE(direction) ((direction) & 3)
#define CELL_COUNT           4   /* cells of a square, one for each direction */
#define WATCHDOG_INTERVAL    300 /* game ticks between two watchdog events */
#define SYSTEM_ERROR_MEMORY  70  /* allocation failed, or an event is missing from the timeline */

/* The direction or cell a quarter turn right, a half turn, or a quarter turn left of this one. */
static inline int16_t turn_right(int16_t direction)
{
    return NORMALIZE(direction + 1);
}
static inline int16_t turn_back(int16_t direction)
{
    return NORMALIZE(direction + 2);
}
static inline int16_t turn_left(int16_t direction)
{
    return NORMALIZE(direction + 3);
}

extern bool g_party_is_sleeping, g_party_dead;
extern bool g_game_won;
extern uint32_t g_game_time, g_game_id;

/* Interface parts that the game rules changed: the user interface draws them again (g_redraw bits). */
extern uint16_t g_redraw;
#define REDRAW_ACTION_AREA   1
#define REDRAW_SPELL_AREA    2
#define REDRAW_SPELL_TABS    4
#define REDRAW_SPELL_SYMBOLS 8

/* Game modes (not in the original game): the user interface differs, the game rules are the same. */
#define GAME_MODE_VANILLA   0
#define GAME_MODE_ADVANCED  1
#define GAME_MODE_TURNBASED 2
extern int16_t g_game_mode; /* GAME_MODE_VANILLA by default */
/* Cheats of the advanced mode, L, I, and F on the automap. Not saved; a new or loaded game turns them off. */
extern bool g_cheat_light;    /* the view draws in full light; the creature rules keep the real light */
extern bool g_cheat_immortal; /* the health of a champion stops at 1 */
extern bool g_cheat_food;     /* food and water do not go down */
#define CHEAT_LIGHT    0
#define CHEAT_IMMORTAL 1
#define CHEAT_FOOD     2

/* Stops the game with the "System Error" message of the original. */
_Noreturn void game_fatal(int16_t code);

/* load_game (new game) and the end of load_dungeon. dungeon_load must run first. */
void game_start_new(void);
/* The start of a game, new or loaded: no candidate, no sleep, no won game, no cheats, no skill level-ups. */
void game_reset_session(void);
void game_set_mode(int16_t mode);
/* Turns a cheat of the advanced mode on or off. */
void game_toggle_cheat(int16_t cheat);
/* The fuse wins the game: bright magical light and strong shields for the end sequence. */
void game_start_fuse_sequence(void);
/* One tick of the game clock alone (the fuse sequence). */
void game_increment_time(void);
/* The game time advances one tick: torches burn and freeze life counts down. */
void game_advance_time(void);
/* The rest of the tick: the champions get their time effects and the movement delays count down. */
void game_apply_tick_effects(void);

/* Moves map coordinates by steps forward and to the right of a direction. */
void game_update_map_coordinates_after_relative_movement(int16_t direction, int16_t steps_forward, int16_t steps_right,
                                                         int16_t *map_x, int16_t *map_y);

#endif
