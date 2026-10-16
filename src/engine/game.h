/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_GAME_H
#define DM_ENGINE_GAME_H
/* Prefixes: game_ position_ turn_ */
#include "data/bytes.h"
#include "data/tables.h"
#include "engine/party.h"
#include <stdbool.h>
#include <stdint.h>

#define NORMALIZE(direction) ((direction) & 3)
#define CELL_COUNT           4 /* cells of a square, one for each direction */
#define CHAMPION_MAX_COUNT   4
#define STAT_LIMIT           170 /* no statistic grows above this value */
#define STAT_MINIMUM_VALUE   30  /* the minimum of each statistic of a new champion */
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

#define STAT_LUCK           0
#define STAT_STRENGTH       1
#define STAT_DEXTERITY      2
#define STAT_WISDOM         3
#define STAT_VITALITY       4
#define STAT_ANTIMAGIC      5
#define STAT_ANTIFIRE       6
#define STAT_COUNT          7
#define STAT_MANA           STAT_COUNT /* object modifiers: the change goes to the maximum mana */
#define STAT_MAXIMUM        0
#define STAT_CURRENT        1
#define STAT_MINIMUM        2
#define STAT_VALUE_COUNT    3
/* Champion slots (Champion.slots), then the slots of the open chest. */
#define SLOT_HAND_READY     0
#define SLOT_HAND_ACTION    1
#define SLOT_HEAD           2
#define SLOT_TORSO          3
#define SLOT_LEGS           4
#define SLOT_FEET           5
#define SLOT_POUCH_2        6
#define SLOT_QUIVER_LINE2_1 7
#define SLOT_QUIVER_LINE1_2 8
#define SLOT_QUIVER_LINE2_2 9
#define SLOT_NECK           10
#define SLOT_POUCH_1        11
#define SLOT_QUIVER_LINE1_1 12
#define SLOT_BACKPACK_FIRST 13
#define SLOT_COUNT          30
#define SLOT_CHEST_FIRST    30
#define SLOT_CHEST_COUNT    8
#define CHAMPION_NAME_SIZE  8
#define CHAMPION_TITLE_SIZE 20

typedef struct {
    int16_t temporary_experience;
    int32_t experience;
} Skill;

typedef struct {
    char name[CHAMPION_NAME_SIZE];
    char title[CHAMPION_TITLE_SIZE];
    uint8_t direction, cell;
    uint8_t action_index; /* 255: none */
    uint8_t symbol_step;
    char symbols[5];
    uint8_t direction_maximum_damage_received, maximum_damage_received;
    uint8_t poison_event_count;
    int16_t enable_action_event_index;        /* event 11, or -1 */
    int16_t hide_damage_received_event_index; /* event 12, or -1 */
    uint16_t attributes;
    uint16_t wounds;
    int16_t current_health, maximum_health;
    int16_t current_stamina, maximum_stamina;
    int16_t current_mana, maximum_mana;
    int16_t action_defense;
    int16_t food, water;
    uint8_t statistics[STAT_COUNT][STAT_VALUE_COUNT];
    Skill skills[SKILL_COUNT];
    uint16_t slots[SLOT_COUNT];
    uint16_t load;
    int16_t shield_defense;
    uint8_t magic_map[6]; /* CSB record bytes 0x118-0x11d, kept as they are */
} Champion;

/* Champion.magic_map: the DAIN, ROS, and KU spell counts; NETA holds direction + 1 of the kept map; the
   kept position is a big-endian word with x in bits 11-15, y in bits 6-10, and the map in bits 0-5. */
#define MAGIC_MAP_DAIN     0
#define MAGIC_MAP_ROS      1
#define MAGIC_MAP_KU       2
#define MAGIC_MAP_NETA     3
#define MAGIC_MAP_POSITION 4
/* Stores the kept map position of NETA in the magic map bytes. */
static inline void game_set_magic_map_position(uint8_t *spells, int16_t map, int16_t x, int16_t y)
{
    wr16(spells + MAGIC_MAP_POSITION, party_pack_position(map, x, y));
}

/* Reads the kept map position of NETA from the magic map bytes. */
static inline void game_get_magic_map_position(const uint8_t *spells, int16_t *map, int16_t *x, int16_t *y)
{
    uint16_t word = be16(spells + MAGIC_MAP_POSITION);
    *map = word & PARTY_POSITION_MAP_MASK;
    *x = party_position_x(word);
    *y = party_position_y(word);
}

extern Champion g_champions[CHAMPION_MAX_COUNT];
extern uint16_t g_party_champion_count;
extern int16_t g_leader_index, g_magic_caster_champion_index;
extern uint16_t g_leader_hand_object;
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

/* Below half stamina, a value drops toward half of it. */
int16_t game_get_stamina_adjusted_value(const Champion *champion, int16_t value);
/* An attack value scaled down by how far the statistic is below the limit. */
uint16_t game_get_statistic_adjusted_attack(const Champion *champion, uint16_t statistic, uint16_t attack);
/* Changes a current statistic within its minimum and the limit; high values grow slower. */
void game_adjust_statistic_current_value(Champion *champion, uint16_t statistic, int16_t delta);
/* Luck test against a percentage. A lucky test lowers luck, a failed one raises it.
   The original reads the luck bytes as signed. A lucky first roll does not change luck. */
bool game_is_lucky(Champion *champion, uint16_t percentage);

#endif
