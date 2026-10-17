/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_GAME_H
#define DM_ENGINE_GAME_H
/* Prefixes: game_ */

/* The game session: the clock, the run flags, the modes and cheats, and the redraw flags that the rules raise. */

#include "map.h"
#include <stdbool.h>
#include <stdint.h>

/* Watchdog interval and the system error code of a failed allocation. */
#define WATCHDOG_INTERVAL   300 /* game ticks between two watchdog events */
#define SYSTEM_ERROR_MEMORY 70  /* allocation failed, or an event is missing from the timeline */

extern uint32_t g_game_time, g_game_id;

/* Interface parts that the game rules changed: the user interface draws them again (g_redraw bits). */
extern uint16_t g_redraw;
/* Redraw flags of the action area and the spell area. */
#define REDRAW_ACTION_AREA   1
#define REDRAW_SPELL_AREA    2
#define REDRAW_SPELL_TABS    4
#define REDRAW_SPELL_SYMBOLS 8

/* Game modes (not in the original game): the user interface differs, the game rules are the same. */
#define GAME_MODE_VANILLA   0
#define GAME_MODE_ADVANCED  1
#define GAME_MODE_TURNBASED 2

/* Cheat bits of the advanced mode, as the cheat block of the save file holds them (game_get_cheats). */
#define CHEAT_LIGHT    0x01
#define CHEAT_IMMORTAL 0x02
#define CHEAT_FOOD     0x04
#define CHEAT_KILL     0x08
#define CHEAT_WALLS    0x10
#define CHEAT_MAP      0x20
#define CHEAT_ALL      0x3f

/* The state of one run of the game: a new game or a load resets it. The save file of the advanced mode holds only
   the cheats. */
typedef struct {
    int16_t mode; /* GAME_MODE_*; a load keeps it */
    bool party_sleeping;
    bool party_dead;
    bool won;
    bool cheat_light;       /* the view draws in full light; the creature rules keep the real light */
    bool cheat_immortal;    /* the health of a champion stops at 1 */
    bool cheat_food;        /* food and water do not go down */
    bool cheat_kill;        /* a damage of the party above 0 kills a creature that is not invulnerable */
    bool cheat_walls;       /* the party walks into walls and solid fake walls inside the map */
    bool cheat_map;         /* the automap shows every square of the level */
    bool saved_game_exists; /* set after a save or a load: the endgame offers a restart */
    uint32_t last_save_time, last_load_time;
    int16_t candidate_champion_index; /* -1: no candidate */
    int16_t inventory_champion_index; /* -1: no inventory open */
    int16_t acting_champion_index;    /* -1: none */
    int16_t action_damage;            /* 0: none, -1 can't reach, -2 need ammo */
    int16_t new_party_map;            /* -1: no map change pending */
} Session;
extern Session g_session;

/* Stops the game with the "System Error" message of the original. */
_Noreturn void game_fatal(int16_t code);

/* load_game (new game) and the end of load_dungeon. dungeon_load must run first. */
void game_start_new(void);
/* The start of a game, new or loaded: no candidate, no sleep, no won game, no skill level-ups. */
void game_reset_session(void);
/* Sets the game mode (GAME_MODE_*) of the session. */
void game_set_mode(int16_t mode);
/* The cheats that are on, as CHEAT_* bits. */
uint8_t game_get_cheats(void);
/* Turns on the cheats of the CHEAT_* bits and turns off the others. */
void game_set_cheats(uint8_t cheats);
/* The fuse wins the game: bright magical light and strong shields for the end sequence. */
void game_start_fuse_sequence(void);
/* Lord Chaos on square at before the fuse: full fuse health, centered, facing the party; the fluxcages on
   the party square and on at disappear. */
void game_prepare_fuse_lord(uint16_t lord, Location at);
/* One tick of the game clock alone (the fuse sequence). */
void game_increment_time(void);
/* The game time advances one tick: torches burn and freeze life counts down. */
void game_advance_time(void);
/* The rest of the tick: the champions get their time effects and the movement delays count down. */
void game_apply_tick_effects(void);

#endif
