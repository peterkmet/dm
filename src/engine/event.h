/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_EVENT_H
#define DM_ENGINE_EVENT_H
/* Prefixes: event_ */

/* Timeline event types and the handlers of the square events. engine/timeline.h stores the events; this
   module runs them. */

#include "engine/map.h"
#include "engine/timeline.h"
#include <stdbool.h>
#include <stdint.h>

/* Event types of the timeline. */
#define EVENT_DOOR_ANIMATION         1
#define EVENT_DOOR_DESTRUCTION       2
#define EVENT_CORRIDOR               5
#define EVENT_WALL                   6
#define EVENT_FAKE_WALL              7
#define EVENT_TELEPORTER             8
#define EVENT_PIT                    9
#define EVENT_DOOR                   10
#define EVENT_ENABLE_CHAMPION_ACTION 11
#define EVENT_HIDE_DAMAGE_RECEIVED   12
#define EVENT_VI_ALTAR_REBIRTH       13
#define EVENT_PLAY_SOUND             20
#define EVENT_REMOVE_FLUXCAGE        24
#define EVENT_EXPLOSION              25
#define EVENT_WATCHDOG               53
#define EVENT_ENABLE_GROUP_GENERATOR 65
#define EVENT_LIGHT                  70
#define EVENT_INVISIBILITY_END       71
#define EVENT_CHAMPION_SHIELD_END    72
#define EVENT_THIEVES_EYE_END        73
#define EVENT_PARTY_SHIELD_END       74
#define EVENT_POISON                 75
#define EVENT_SPELL_SHIELD_END       77
#define EVENT_FIRE_SHIELD_END        78
#define EVENT_FOOTPRINTS_END         79
#define EVENT_MAGIC_MAP_DAIN_END     80
#define EVENT_MAGIC_MAP_ROS_END      81
#define EVENT_MAGIC_MAP_KU_END       82
#define EVENT_MAGIC_MAP_NETA_END     83 /* CSB */

/* Group timeline events. A behavior event is an aspect event + 5. */
#define EVENT_GROUP_REACTION_DANGER_ON_SQUARE  29
#define EVENT_GROUP_REACTION_HIT_BY_PROJECTILE 30
#define EVENT_GROUP_REACTION_PARTY_IS_ADJACENT 31
#define EVENT_UPDATE_ASPECT_GROUP              32
#define EVENT_UPDATE_ASPECT_CREATURE_0         33
#define EVENT_UPDATE_BEHAVIOR_GROUP            37
#define EVENT_UPDATE_BEHAVIOR_CREATURE_0       38
#define EVENT_UPDATE_BEHAVIOR_CREATURE_3       41
#define EVENT_MOVE_GROUP_SILENT                60
#define EVENT_MOVE_GROUP_AUDIBLE               61

/* A projectile moves one square per EVENT_MOVE_PROJECTILE or EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS. The latter
   ignores impacts on the square where it starts. */
#define EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS 48
#define EVENT_MOVE_PROJECTILE                49

/* Event time: map index in bits 24-31, game time in bits 0-23. */
#define EVENT_MAP_TIME(map, time) ((uint32_t)(map) << 24 | (uint32_t)(time))
/* Bit mask of the time of an event. */
#define EVENT_TIME_MASK           0xffffff
#define EVENT_MAP(event)          ((int16_t)((event)->map_time >> 24))

/* Square events: b = x << 8 | y, c = cell << 8 | effect. Group events: c = ticks << 8.
   EVENT_ENABLE_CHAMPION_ACTION: b = quiver slot + 1 << 8, 0: none. */
#define EVENT_X_SHIFT           8
#define EVENT_CELL_SHIFT        8
#define EVENT_GROUP_TICKS_SHIFT 8
#define EVENT_QUIVER_SLOT_SHIFT 8

/* Bit masks of the bytes of an event word. */
#define EVENT_BYTE_MASK      0x00ff
#define EVENT_HIGH_BYTE_MASK 0xff00
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
static inline void event_set_effect(TimelineEvent *event, int16_t effect)
{
    event->c = (event->c & EVENT_HIGH_BYTE_MASK) | (effect & EVENT_BYTE_MASK);
}
/* The c word of a group event that carries the ticks of the next update. */
static inline uint16_t event_group_c(uint16_t ticks)
{
    return ticks << EVENT_GROUP_TICKS_SHIFT;
}
static inline uint16_t event_group_ticks(const TimelineEvent *event)
{
    return event->c >> EVENT_GROUP_TICKS_SHIFT;
}
/* EVENT_ENABLE_CHAMPION_ACTION after a throw: the slot that gets the next weapon of the quiver, -1: none. */
static inline int16_t event_quiver_slot(const TimelineEvent *event)
{
    return (int8_t)(event->b >> EVENT_QUIVER_SLOT_SHIFT) - 1;
}
static inline void event_set_quiver_slot(TimelineEvent *event, int16_t slot)
{
    event->b = (slot + 1) << EVENT_QUIVER_SLOT_SHIFT | (event->b & EVENT_BYTE_MASK);
}

/* The endgame of an endgame sensor, set by the user interface: it runs inside the sensor event. */
extern void (*g_endgame_sensor_hook)(uint16_t delay);

/* Texts of the event cell change visibility. Counters, AND/OR gates, and projectile launchers of the cell run.
   An endgame sensor ends the game. */
void event_process_wall(TimelineEvent *event); /* exported for the tests */
/* A closed door breaks when the attack reaches its defense and the door accepts the attack kind (force or magic).
   ticks 0 breaks it at once, else a door destruction event breaks it later. Returns true when the door breaks. */
bool event_is_door_destroyed(Location at, int16_t attack, bool magic, uint16_t ticks);
/* Runs every expired event on its map. Event types without a handler here belong to later steps. */
void event_process_expired(void);

#endif
