/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_TIMELINE_PROCESS_H
#define DM_ENGINE_TIMELINE_PROCESS_H
/* Prefixes: event_ */
#include "engine/game.h"
#include <stdbool.h>
#include <stdint.h>

/* Timeline event types and the handlers of the square events. */

enum {
    EVENT_DOOR_ANIMATION = 1,
    EVENT_DOOR_DESTRUCTION = 2,
    EVENT_CORRIDOR = 5,
    EVENT_WALL = 6,
    EVENT_FAKE_WALL = 7,
    EVENT_TELEPORTER = 8,
    EVENT_PIT = 9,
    EVENT_DOOR = 10,
    EVENT_ENABLE_CHAMPION_ACTION = 11,
    EVENT_HIDE_DAMAGE_RECEIVED = 12,
    EVENT_VI_ALTAR_REBIRTH = 13,
    EVENT_PLAY_SOUND = 20,
    EVENT_REMOVE_FLUXCAGE = 24,
    EVENT_EXPLOSION = 25,
    EVENT_WATCHDOG = 53,
    EVENT_ENABLE_GROUP_GENERATOR = 65,
    EVENT_LIGHT = 70,
    EVENT_INVISIBILITY_END = 71,
    EVENT_CHAMPION_SHIELD_END = 72,
    EVENT_THIEVES_EYE_END = 73,
    EVENT_PARTY_SHIELD_END = 74,
    EVENT_POISON = 75,
    EVENT_SPELL_SHIELD_END = 77,
    EVENT_FIRE_SHIELD_END = 78,
    EVENT_FOOTPRINTS_END = 79,
    EVENT_MAGIC_MAP_DAIN_END = 80,
    EVENT_MAGIC_MAP_ROS_END = 81,
    EVENT_MAGIC_MAP_KU_END = 82,
    EVENT_MAGIC_MAP_NETA_END = 83 /* CSB */
};

/* Group timeline events. A behavior event is an aspect event + 5. */
enum {
    EVENT_GROUP_REACTION_DANGER_ON_SQUARE = 29,
    EVENT_GROUP_REACTION_HIT_BY_PROJECTILE = 30,
    EVENT_GROUP_REACTION_PARTY_IS_ADJACENT = 31,
    EVENT_UPDATE_ASPECT_GROUP = 32,
    EVENT_UPDATE_ASPECT_CREATURE_0 = 33,
    EVENT_UPDATE_BEHAVIOR_GROUP = 37,
    EVENT_UPDATE_BEHAVIOR_CREATURE_0 = 38,
    EVENT_UPDATE_BEHAVIOR_CREATURE_3 = 41,
    EVENT_MOVE_GROUP_SILENT = 60,
    EVENT_MOVE_GROUP_AUDIBLE = 61
};

/* A projectile moves one square per event 48 or 49. Event 48 ignores impacts on the square where it starts. */
enum { EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS = 48, EVENT_MOVE_PROJECTILE = 49 };

/* Event time: map index in bits 24-31, game time in bits 0-23. */
#define EVENT_MAP_TIME(map, time) ((uint32_t)(map) << 24 | (uint32_t)(time))
#define EVENT_TIME_MASK           0xffffff
#define EVENT_MAP(event)          ((int16_t)((event)->map_time >> 24))

/* Square events: b = x << 8 | y, c = cell << 8 | effect. Group events: c = ticks << 8. Event 11: b = quiver
   slot + 1 << 8, 0: none. */
enum {
    EVENT_X_SHIFT = 8,
    EVENT_CELL_SHIFT = 8,
    EVENT_GROUP_TICKS_SHIFT = 8,
    EVENT_QUIVER_SLOT_SHIFT = 8,
    EVENT_BYTE_MASK = 0xff,
    EVENT_HIGH_BYTE_MASK = 0xff00
};
static inline uint16_t event_square(int16_t x, int16_t y)
{
    return (x & EVENT_BYTE_MASK) << EVENT_X_SHIFT | (y & EVENT_BYTE_MASK);
}
static inline uint16_t event_cell_effect(int16_t cell, int16_t effect)
{
    return (cell & EVENT_BYTE_MASK) << EVENT_CELL_SHIFT | (effect & EVENT_BYTE_MASK);
}
static inline int16_t event_square_x(const TimelineEvent *event)
{
    return event->b >> EVENT_X_SHIFT;
}
static inline int16_t event_square_y(const TimelineEvent *event)
{
    return event->b & EVENT_BYTE_MASK;
}
static inline int16_t event_cell(const TimelineEvent *event)
{
    return event->c >> EVENT_CELL_SHIFT;
}
static inline uint8_t event_effect(const TimelineEvent *event)
{
    return event->c;
}
static inline void event_set_effect(TimelineEvent *event, uint8_t effect)
{
    event->c = (event->c & EVENT_HIGH_BYTE_MASK) | effect;
}
/* The c word of a group event that carries the ticks of the next update. */
static inline uint16_t event_group_c(uint8_t ticks)
{
    return ticks << EVENT_GROUP_TICKS_SHIFT;
}
static inline uint16_t event_group_ticks(const TimelineEvent *event)
{
    return event->c >> EVENT_GROUP_TICKS_SHIFT;
}
/* Event 11 after a throw: the slot that gets the next weapon of the quiver, -1: none. */
static inline int16_t event_quiver_slot(const TimelineEvent *event)
{
    return (int8_t)(event->b >> EVENT_QUIVER_SLOT_SHIFT) - 1;
}
static inline void event_set_quiver_slot(TimelineEvent *event, int16_t slot)
{
    event->b = (slot + 1) << EVENT_QUIVER_SLOT_SHIFT | (event->b & EVENT_BYTE_MASK);
}

extern bool g_game_won;
/* The endgame of an endgame sensor, set by the user interface: it runs inside the sensor event. */
extern void (*g_endgame_sensor_hook)(uint16_t delay);

void event_process_expired(void);
void event_process_door_animation(TimelineEvent *event);
void event_process_door_destruction(TimelineEvent *event);
void event_process_corridor(TimelineEvent *event);
void event_process_wall(TimelineEvent *event);
void event_process_fake_wall(TimelineEvent *event);
void event_process_teleporter(TimelineEvent *event);
void event_process_pit(TimelineEvent *event);
void event_process_door(TimelineEvent *event);
void event_enable_champion_action(uint16_t champion_index);
void event_hide_damage_received(uint16_t champion_index);
void event_process_vi_altar_rebirth(TimelineEvent *event);
void event_enable_group_generator(TimelineEvent *event);
void event_process_light(TimelineEvent *event);
void event_move_teleporter_or_pit_square_things(int16_t x, int16_t y);
bool event_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, uint16_t ticks);

#endif
