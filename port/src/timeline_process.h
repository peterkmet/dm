#ifndef TIMELINE_PROCESS_H
#define TIMELINE_PROCESS_H
#include <stdint.h>
#include "game.h"

/* Timeline event types and the handlers of the square events. */

enum {
    EVENT_DOOR_ANIMATION = 1, EVENT_DOOR_DESTRUCTION = 2, EVENT_CORRIDOR = 5, EVENT_WALL = 6, EVENT_FAKE_WALL = 7,
    EVENT_TELEPORTER = 8, EVENT_PIT = 9, EVENT_DOOR = 10, EVENT_PLAY_SOUND = 20, EVENT_REMOVE_FLUXCAGE = 24,
    EVENT_EXPLOSION = 25, EVENT_WATCHDOG = 53, EVENT_ENABLE_GROUP_GENERATOR = 65, EVENT_LIGHT = 70,
    EVENT_INVISIBILITY_END = 71, EVENT_CHAMPION_SHIELD_END = 72, EVENT_THIEVES_EYE_END = 73, EVENT_PARTY_SHIELD_END = 74,
    EVENT_SPELL_SHIELD_END = 77, EVENT_FIRE_SHIELD_END = 78, EVENT_FOOTPRINTS_END = 79
};

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

void timeline_process(void);
void timeline_process_event1_door_animation(TimelineEvent *event);
void timeline_process_event2_door_destruction(TimelineEvent *event);
void timeline_process_event5_corridor(TimelineEvent *event);
void timeline_process_event6_wall(TimelineEvent *event);
void timeline_process_event7_fake_wall(TimelineEvent *event);
void timeline_process_event8_teleporter(TimelineEvent *event);
void timeline_process_event9_pit(TimelineEvent *event);
void timeline_process_event10_door(TimelineEvent *event);
void timeline_process_event65_enable_group_generator(TimelineEvent *event);
void timeline_process_event70_light(TimelineEvent *event);
void timeline_move_teleporter_or_pit_square_things(int16_t x, int16_t y);
int16_t group_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, int16_t ticks);

#endif
