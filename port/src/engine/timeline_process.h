#ifndef TIMELINE_PROCESS_H
#define TIMELINE_PROCESS_H
#include <stdint.h>
#include "game.h"

/* Timeline event types and the handlers of the square events. */

enum {
    EVENT_DOOR_ANIMATION = 1, EVENT_DOOR_DESTRUCTION = 2, EVENT_CORRIDOR = 5, EVENT_WALL = 6, EVENT_FAKE_WALL = 7,
    EVENT_TELEPORTER = 8, EVENT_PIT = 9, EVENT_DOOR = 10, EVENT_ENABLE_CHAMPION_ACTION = 11,
    EVENT_HIDE_DAMAGE_RECEIVED = 12, EVENT_VI_ALTAR_REBIRTH = 13, EVENT_PLAY_SOUND = 20, EVENT_REMOVE_FLUXCAGE = 24,
    EVENT_EXPLOSION = 25, EVENT_WATCHDOG = 53, EVENT_ENABLE_GROUP_GENERATOR = 65, EVENT_LIGHT = 70,
    EVENT_INVISIBILITY_END = 71, EVENT_CHAMPION_SHIELD_END = 72, EVENT_THIEVES_EYE_END = 73, EVENT_PARTY_SHIELD_END = 74,
    EVENT_POISON = 75, EVENT_SPELL_SHIELD_END = 77, EVENT_FIRE_SHIELD_END = 78, EVENT_FOOTPRINTS_END = 79,
    EVENT_MAGIC_MAP_DAIN_END = 80, EVENT_MAGIC_MAP_ROS_END = 81, EVENT_MAGIC_MAP_KU_END = 82, EVENT_MAGIC_MAP_NETA_END = 83 /* CSB */
};

/* Group timeline events. A behavior event is an aspect event + 5. */
enum {
    EVENT_GROUP_REACTION_DANGER_ON_SQUARE = 29, EVENT_GROUP_REACTION_HIT_BY_PROJECTILE = 30,
    EVENT_GROUP_REACTION_PARTY_IS_ADJACENT = 31, EVENT_UPDATE_ASPECT_GROUP = 32, EVENT_UPDATE_ASPECT_CREATURE_0 = 33,
    EVENT_UPDATE_BEHAVIOR_GROUP = 37, EVENT_UPDATE_BEHAVIOR_CREATURE_0 = 38, EVENT_UPDATE_BEHAVIOR_CREATURE_3 = 41,
    EVENT_MOVE_GROUP_SILENT = 60, EVENT_MOVE_GROUP_AUDIBLE = 61
};

/* A projectile moves one square per event 48 or 49. Event 48 ignores impacts on the square where it starts. */
enum { EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS = 48, EVENT_MOVE_PROJECTILE = 49 };

/* Event time: map index in bits 24-31, game time in bits 0-23. */
#define EVENT_MAP_TIME(map, time) ((uint32_t)(map) << 24 | (uint32_t)(time))
#define EVENT_TIME_MASK 0xffffff
#define EVENT_MAP(event) ((int16_t)((event)->map_time >> 24))

/* Square events: b = x << 8 | y, c = cell << 8 | effect. */
#define EVENT_SQUARE(x, y) ((uint16_t)(((x) & 0xff) << 8 | ((y) & 0xff)))
#define EVENT_CELL_EFFECT(cell, effect) ((uint16_t)(((cell) & 0xff) << 8 | ((effect) & 0xff)))
#define EVENT_X(event) ((int16_t)((event)->b >> 8))
#define EVENT_Y(event) ((int16_t)((event)->b & 0xff))
#define EVENT_CELL(event) ((int16_t)((event)->c >> 8))
#define EVENT_EFFECT(event) ((uint8_t)(event)->c)
#define EVENT_SET_EFFECT(event, effect) ((event)->c = (uint16_t)(((event)->c & 0xff00) | (uint8_t)(effect)))

/* Projectile move events: b = the projectile thing, c = x in bits 11-15, y in bits 6-10, direction in bits 4-5. */
#define PROJECTILE_EVENT_DIRECTION(event) ((event)->c >> 4 & 3)

extern int16_t g_game_won;
/* The endgame of an endgame sensor, set by the user interface: it runs inside the sensor event. */
extern void (*g_endgame_sensor_hook)(uint16_t delay);

void timeline_process(void);
void timeline_process_event1_door_animation(TimelineEvent *event);
void timeline_process_event2_door_destruction(TimelineEvent *event);
void timeline_process_event5_corridor(TimelineEvent *event);
void timeline_process_event6_wall(TimelineEvent *event);
void timeline_process_event7_fake_wall(TimelineEvent *event);
void timeline_process_event8_teleporter(TimelineEvent *event);
void timeline_process_event9_pit(TimelineEvent *event);
void timeline_process_event10_door(TimelineEvent *event);
void timeline_process_event11_enable_champion_action(uint16_t champion_index);
void timeline_process_event12_hide_damage_received(uint16_t champion_index);
void timeline_process_event13_vi_altar_rebirth(TimelineEvent *event);
void timeline_process_event65_enable_group_generator(TimelineEvent *event);
void timeline_process_event70_light(TimelineEvent *event);
void timeline_move_teleporter_or_pit_square_things(int16_t x, int16_t y);
int16_t group_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, int16_t ticks);

#endif
