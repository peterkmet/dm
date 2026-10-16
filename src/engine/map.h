/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_MAP_H
#define DM_ENGINE_MAP_H
/* Prefixes: door_ square_ map_ direction_ */
#include "engine/record.h"
#include <stdbool.h>
#include <stdint.h>

/* Current map, square bytes, and thing lists.
   Coordinates are on the current map. All records keep the big-endian layout of Dungeon.DAT. */

#define NORMALIZE(direction) ((direction) & 3)
#define CELL_COUNT           4 /* cells of a square, one for each direction */
#define DIRECTION_NORTH      0
#define DIRECTION_EAST       1
#define DIRECTION_SOUTH      2
#define DIRECTION_WEST       3
#define DIRECTION_BITS       2
#define DIRECTION_COUNT      4
#define LEVEL_UP             (-1)
#define LEVEL_DOWN           1
#define MAP_COORD_BITS       5 /* maps are at most 32 squares wide and high */
#define MAP_COORD_MASK       0x1f

/* The direction or cell a quarter turn right, a half turn, or a quarter turn left of this one. */
static inline int16_t direction_right(int16_t direction)
{
    return NORMALIZE(direction + 1);
}
static inline int16_t direction_back(int16_t direction)
{
    return NORMALIZE(direction + 2);
}
static inline int16_t direction_left(int16_t direction)
{
    return NORMALIZE(direction + 3);
}

/* Moves x and y by steps forward and to the right of a direction. */
void map_step(int16_t direction, int16_t forward, int16_t right, int16_t *out_x, int16_t *out_y);

/* Square byte: type in bits 5-7, a thing list in bit 4, type flags in bits 0-3. */
#define SQUARE_WALL            0
#define SQUARE_CORRIDOR        1
#define SQUARE_PIT             2
#define SQUARE_STAIRS          3
#define SQUARE_DOOR            4
#define SQUARE_TELEPORTER      5
#define SQUARE_FAKE_WALL       6
#define SQUARE_HAS_THINGS      0x10 /* random ornament: corridor and open fake wall */
#define SQUARE_RANDOM_ORNAMENT 8
#define WALL_ORNAMENT_RIGHT    1
#define WALL_ORNAMENT_ABOVE    2
#define WALL_ORNAMENT_LEFT     4
#define WALL_ORNAMENT_BELOW    8
#define PIT_IMAGINARY          1
#define PIT_INVISIBLE          4
#define PIT_OPEN               8
#define STAIRS_UP              4
#define STAIRS_NORTH_SOUTH     8
#define DOOR_STATE_MASK        7
#define DOOR_OPEN              0
#define DOOR_ALMOST_OPEN       1
#define DOOR_CLOSED            4
#define DOOR_DESTROYED         5
#define TELEPORTER_VISIBLE     4
#define TELEPORTER_OPEN        8
#define FAKE_WALL_IMAGINARY    1
#define FAKE_WALL_OPEN         4

/* Thing word: index in bits 0-9, type in bits 10-13, cell in bits 14-15. */
#define THING_DOOR                   0
#define THING_TELEPORTER             1
#define THING_TEXT                   2
#define THING_SENSOR                 3
#define THING_GROUP                  4
#define THING_WEAPON                 5
#define THING_ARMOUR                 6
#define THING_SCROLL                 7
#define THING_POTION                 8
#define THING_CONTAINER              9
#define THING_JUNK                   10
#define THING_PROJECTILE             14
#define THING_EXPLOSION              15
#define THING_ENDOFLIST              0xfffe
#define THING_NONE                   0xffff
#define THING_TYPE_SHIFT             10
#define THING_TYPE(thing)            ((thing) >> THING_TYPE_SHIFT & 0x0f)
#define THING_MAKE(type, index)      ((uint16_t)((type) << THING_TYPE_SHIFT | (index)))
#define THING_CELL(thing)            ((uint16_t)(thing) >> 14)
#define THING_INDEX(thing)           ((thing) & 0x3ff) /* record index without type and cell */
#define THING_STRIP_CELL(thing)      ((thing) & 0x3fff)
#define THING_WITH_CELL(thing, cell) ((uint16_t)(((cell) & 3) << 14 | THING_STRIP_CELL(thing)))

/* Sensor record (thing type 3). */
#define SENSOR_DISABLED                           0 /* sensor type 0 does nothing; the sensor data stays */
#define SENSOR_FLOOR_ANY                          1
#define SENSOR_FLOOR_PARTY_OR_GROUP               2
#define SENSOR_FLOOR_PARTY                        3
#define SENSOR_FLOOR_OBJECT                       4
#define SENSOR_FLOOR_PARTY_ON_STAIRS              5
#define SENSOR_FLOOR_GROUP_GENERATOR              6
#define SENSOR_FLOOR_GROUP                        7
#define SENSOR_FLOOR_PARTY_POSSESSION             8
#define SENSOR_FLOOR_VERSION_CHECKER              9
#define SENSOR_WALL_ORNAMENT_CLICK                1
#define SENSOR_WALL_ANY_OBJECT                    2
#define SENSOR_WALL_OBJECT                        3
#define SENSOR_WALL_OBJECT_REMOVED                4
#define SENSOR_WALL_AND_OR_GATE                   5
#define SENSOR_WALL_COUNTER                       6
#define SENSOR_WALL_SINGLE_OBJECT_STORAGE         11
#define SENSOR_WALL_OBJECT_GENERATOR              12
#define SENSOR_WALL_SINGLE_OBJECT_EXCHANGER       13
#define SENSOR_WALL_OBJECT_EXCHANGER              16
#define SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED 17
#define SENSOR_WALL_ENDGAME                       18
#define SENSOR_WALL_CHAMPION_PORTRAIT             127

/* Teleporter record (thing type 1). Scope: 1 creatures, 2 party, 3 everything. */
#define TELEPORTER_SCOPE_CREATURES 1
#define TELEPORTER_SCOPE_PARTY     2
#define TELEPORTER_SCOPE_ALL       3

/* Door defense: the low byte of the door info. */
#define DOOR_DEFENSE_MASK 0xff
static inline int16_t door_info_defense(uint16_t door_info)
{
    return door_info & DOOR_DEFENSE_MASK;
}

extern int16_t g_current_map_index, g_current_map_width, g_current_map_height;
extern const uint8_t *g_current_map;        /* map descriptor, 16 bytes */
extern uint16_t g_current_map_door_info[2]; /* high byte attributes, low byte defense */

/* Door attributes (high byte of the door info of the current map): the door is see-through, projectiles pass. */
#define DOOR_ATTRIBUTES_SHIFT 8
#define DOOR_SEE_THROUGH      1
#define DOOR_PROJECTILES_PASS 2
static inline uint16_t door_attributes(uint16_t door)
{
    return g_current_map_door_info[rec_door_type(door)] >> DOOR_ATTRIBUTES_SHIFT;
}
static inline bool door_is_see_through(uint16_t door)
{
    return door_attributes(door) & DOOR_SEE_THROUGH;
}

/* Forget the current map, for example after dungeon_load replaced the buffers. */
void map_reset(void);
/* Selects the map that the square and thing functions work on. */
void map_set_current_map(int16_t map);
/* The party map becomes the current map. The ornament lists are view data (view_load_map). */
void map_set_current_map_and_party_map(int16_t map);

/* Outside the map: a wall, with the "ornament allowed" bit when the adjacent square is a corridor or a pit. */
uint8_t map_get_square(int16_t x, int16_t y);
/* Corridor, pit, door, or teleporter. */
bool map_is_square_open(int16_t x, int16_t y);
/* The stairs axis is north-south or east-west. The exit side is the one that is not a wall or stairs. */
int16_t map_get_stairs_exit_direction(int16_t x, int16_t y);

uint16_t map_get_square_first_thing(int16_t x, int16_t y);
/* The first group, object, projectile, or explosion of the square. */
uint16_t map_get_square_first_object(int16_t x, int16_t y);
/* x >= 0: append the thing to the list of square (x, y). x < 0: append it to the list that starts at list. */
void map_link_thing_to_list(uint16_t thing, uint16_t list, int16_t x, int16_t y);
/* Removes the thing from the square list (x >= 0) or from the list after list. Things compare without the cell. */
void map_unlink_thing_from_list(uint16_t thing, uint16_t list, int16_t x, int16_t y);

/* A square of the current map. */
typedef struct {
    int16_t x, y;
} Location;

/* A square with its map. */
typedef struct {
    int16_t map, x, y;
} Position;

/* The square one level below (LEVEL_DOWN) or above (LEVEL_UP) at the same dungeon place; map -1 when none. */
Position map_get_level_change_location(Position place, int16_t level_delta);

/* The party stands on the square of the current map. */
bool map_is_party_on(Location at);
/* The first thing of a type on a square of the current map, or THING_ENDOFLIST. */
uint16_t map_find_thing(Location at, int16_t type_wanted);
/* The first thing on a square of any map, or THING_ENDOFLIST. */
uint16_t map_get_first_thing_at(Position place);
/* The map one level up or down that covers the dungeon position of (x, y) on map, or -1. */
int16_t map_find_level_map(int16_t map, int16_t level_delta, int16_t x, int16_t y);
/* Of the two cells on the side of a direction, the one beside cell. */
static inline int16_t direction_front_cell(int16_t direction, int16_t cell)
{
    return NORMALIZE(direction + (((cell - direction + 1) & 2) >> 1));
}

uint16_t map_get_creature_attributes(uint16_t group_thing);

#endif
