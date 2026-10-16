/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/map.h"
#include "data/bytes.h"
#include "data/os.h"
#include "data/tables.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/party.h"
#include "engine/record.h"
#include <inttypes.h>
#include <string.h>

#define FATAL_SQUARE_FIRST_THINGS_FULL 72
#define NO_DUNGEON_MAP                 0xff

int16_t g_current_map_index = -1, g_current_map_width, g_current_map_height;
const uint8_t *g_current_map;
uint16_t g_current_map_door_info[2];
static uint8_t *column_first_thing_index;

void map_step(int16_t direction, int16_t forward, int16_t right, int16_t *x, int16_t *y)
{
    int16_t side = direction_right(direction);
    *x = *x + forward * tables_get_step_east(direction) + right * tables_get_step_east(side);
    *y = *y + forward * tables_get_step_north(direction) + right * tables_get_step_north(side);
}

void map_reset(void)
{
    g_current_map_index = -1;
}

void map_set_current_map(int16_t map)
{
    if (map == g_current_map_index)
        return;
    g_current_map_index = map;
    g_current_map = rec_map_descriptor(map);
    g_current_map_width = dungeon_map_width(map);
    g_current_map_height = dungeon_map_height(map);
    for (int16_t door = 0; door < 2; door++)
        g_current_map_door_info[door] = tables_get_door_attributes(rec_map_door_set(map, door));
    column_first_thing_index = g_dungeon_column_first_thing_index + g_dungeon_map_first_column[map] * 2;
}

void map_set_current_map_and_party_map(int16_t map)
{
    g_party_map_index = map;
    map_set_current_map(map);
}

static bool is_corridor_or_pit(uint8_t square)
{
    return rec_square_type(square) == SQUARE_CORRIDOR || rec_square_type(square) == SQUARE_PIT;
}

uint8_t map_get_square(int16_t x, int16_t y)
{
    bool in_x = x >= 0 && x < g_current_map_width;
    bool in_y = y >= 0 && y < g_current_map_height;
    int16_t map = g_current_map_index;
    if (in_x && in_y)
        return rec_square_byte(map, x, y);
    if (in_x) {
        if (y == -1 && is_corridor_or_pit(rec_square_byte(map, x, 0)))
            return WALL_ORNAMENT_ABOVE;
        if (y == g_current_map_height && is_corridor_or_pit(rec_square_byte(map, x, g_current_map_height - 1)))
            return WALL_ORNAMENT_BELOW;
    } else if (in_y) {
        if (x == -1 && is_corridor_or_pit(rec_square_byte(map, 0, y)))
            return WALL_ORNAMENT_LEFT;
        if (x == g_current_map_width && is_corridor_or_pit(rec_square_byte(map, g_current_map_width - 1, y)))
            return WALL_ORNAMENT_RIGHT;
    }
    return SQUARE_WALL;
}

int16_t map_get_square_type(int16_t x, int16_t y)
{
    return rec_square_type(map_get_square(x, y));
}

bool map_is_square_open(int16_t x, int16_t y)
{
    int16_t type = map_get_square_type(x, y);
    return type == SQUARE_CORRIDOR || type == SQUARE_TELEPORTER || type == SQUARE_PIT || type == SQUARE_DOOR;
}

int16_t map_get_stairs_exit_direction(int16_t x, int16_t y)
{
    int16_t direction = map_get_square(x, y) & STAIRS_NORTH_SOUTH ? DIRECTION_NORTH : DIRECTION_EAST;
    int16_t ahead_x = x;
    int16_t ahead_y = y;
    map_step(direction, 1, 0, &ahead_x, &ahead_y);
    int16_t type = map_get_square_type(ahead_x, ahead_y);
    return type == SQUARE_WALL || type == SQUARE_STAIRS ? direction_back(direction) : direction;
}

int16_t map_get_square_first_thing_index(int16_t x, int16_t y)
{
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height ||
        !(rec_square_byte(g_current_map_index, x, y) & SQUARE_HAS_THINGS))
        return -1;
    int16_t index = (int16_t)be16(column_first_thing_index + x * 2); /* the word read as signed */
    for (int16_t i = 0; i < y; i++)
        index += (rec_square_byte(g_current_map_index, x, i) & SQUARE_HAS_THINGS) != 0;
    return index;
}

uint16_t map_get_square_first_thing(int16_t x, int16_t y)
{
    return map_get_first_thing_at((Position){g_current_map_index, x, y});
}

uint16_t map_get_square_first_object(int16_t x, int16_t y)
{
    uint16_t thing = map_get_square_first_thing(x, y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) < THING_GROUP)
        thing = rec_thing_next(thing);
    return thing;
}

/* Adds delta to the first-thing index of each map column after column x: a square list was added or removed. */
static void add_to_later_columns(int16_t x, int16_t delta)
{
    int16_t global_x = x + g_dungeon_map_first_column[g_current_map_index];
    for (int16_t c = global_x + 1; c < g_dungeon_column_count; c++) {
        uint8_t *p = g_dungeon_column_first_thing_index + c * 2;
        wr16(p, be16(p) + delta);
    }
}

/* A free record (THING_NONE) must never enter a square list: a save with it is damaged. Logs where it comes from. */
static void report_free_record(const char *action, int16_t x, int16_t y)
{
    os_log("square list guard: %s, map %d (%d,%d), game time %" PRIu32, action, g_current_map_index, x, y, g_game_time);
}

void map_link_thing_to_list(uint16_t thing, uint16_t list, int16_t x, int16_t y)
{
    if (thing == THING_ENDOFLIST)
        return;
    if (thing == THING_NONE) {
        report_free_record("link of a free record", x, y);
        return;
    }
    rec_thing_set_next(thing, THING_ENDOFLIST);
    if (x >= 0) {
        uint8_t square = rec_square_byte(g_current_map_index, x, y);
        if (!(square & SQUARE_HAS_THINGS)) {
            uint16_t count = rec_dungeon_square_things();
            if (be16(g_square_first_things + (count - 1) * 2) != THING_NONE)
                game_fatal(FATAL_SQUARE_FIRST_THINGS_FULL);
            rec_square_set_byte(g_current_map_index, x, y, square | SQUARE_HAS_THINGS);
            add_to_later_columns(x, 1);
            uint16_t index = map_get_square_first_thing_index(x, y);
            uint8_t *p = g_square_first_things + index * 2;
            memmove(p + 2, p, (count - index - 1) * 2);
            wr16(p, thing);
            return;
        }
        list = map_get_square_first_thing(x, y);
    }
    for (uint16_t next = rec_thing_next(list); next != THING_ENDOFLIST; next = rec_thing_next(list))
        list = next;
    rec_thing_set_next(list, thing);
}

void map_unlink_thing_from_list(uint16_t thing, uint16_t list, int16_t x, int16_t y)
{
    if (thing == THING_ENDOFLIST)
        return;
    thing = THING_STRIP_CELL(thing);
    if (x >= 0) {
        uint16_t index = map_get_square_first_thing_index(x, y);
        uint8_t *first = g_square_first_things + index * 2;
        uint16_t after = rec_thing_next(thing);
        if (after == THING_NONE && thing == THING_STRIP_CELL(be16(first)))
            report_free_record("unlink of a free record", x, y); /* its next is gone: remove the whole entry */
        if ((after == THING_ENDOFLIST || after == THING_NONE) && thing == THING_STRIP_CELL(be16(first))) {
            uint16_t last = rec_dungeon_square_things() - 1;
            rec_square_set_byte(g_current_map_index, x, y,
                                rec_square_byte(g_current_map_index, x, y) & ~SQUARE_HAS_THINGS);
            memmove(first, first + 2, (last - index) * 2);
            wr16(g_square_first_things + last * 2, THING_NONE);
            add_to_later_columns(x, -1);
            rec_thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        if (thing == THING_STRIP_CELL(be16(first))) {
            wr16(first, rec_thing_next(thing));
            rec_thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = be16(first);
    }
    uint16_t next = rec_thing_next(list);
    while (thing != THING_STRIP_CELL(next)) {
        if (next == THING_ENDOFLIST || next == THING_NONE) {
            if (x >= 0)
                rec_thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = next;
        next = rec_thing_next(next);
    }
    rec_thing_set_next(list, rec_thing_next(next));
    rec_thing_set_next(thing, THING_ENDOFLIST);
}

Position map_get_level_change_location(Position place, int16_t level_delta)
{
    Position to = {-1, place.x, place.y};
    if (g_party_map_index == NO_DUNGEON_MAP)
        return to;
    to.map = map_find_level_map(place.map, level_delta, place.x, place.y);
    if (to.map >= 0) {
        to.x += rec_map_offset_x(place.map) - rec_map_offset_x(to.map);
        to.y += rec_map_offset_y(place.map) - rec_map_offset_y(to.map);
    }
    return to;
}

bool map_is_party_on(Location at)
{
    return g_current_map_index == g_party_map_index && at.x == g_party_map_x && at.y == g_party_map_y;
}

uint16_t map_find_thing(Location at, int16_t type_wanted)
{
    uint16_t thing = map_get_square_first_thing(at.x, at.y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) != type_wanted)
        thing = rec_thing_next(thing);
    return thing;
}

uint16_t map_get_first_thing_at(Position place)
{
    if (place.x < 0 || place.x >= dungeon_map_width(place.map) || place.y < 0 ||
        place.y >= dungeon_map_height(place.map))
        return THING_ENDOFLIST;
    if (!(rec_square_byte(place.map, place.x, place.y) & SQUARE_HAS_THINGS))
        return THING_ENDOFLIST;
    uint16_t index = be16(g_dungeon_column_first_thing_index + (g_dungeon_map_first_column[place.map] + place.x) * 2);
    for (int16_t i = 0; i < place.y; i++)
        index += (rec_square_byte(place.map, place.x, i) & SQUARE_HAS_THINGS) != 0;
    if (index >= rec_dungeon_square_things())
        return THING_ENDOFLIST;
    return be16(g_square_first_things + index * 2);
}

int16_t map_find_level_map(int16_t map, int16_t level_delta, int16_t x, int16_t y)
{
    int16_t dungeon_x = x + rec_map_offset_x(map);
    int16_t dungeon_y = y + rec_map_offset_y(map);
    uint16_t level = (uint16_t)(rec_map_level(map) + level_delta); /* level 0 up wraps to no level */
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        if (level == rec_map_level(m) && rec_map_offset_x(m) <= dungeon_x &&
            dungeon_x <= rec_map_offset_x(m) + rec_map_last_x(m) && rec_map_offset_y(m) <= dungeon_y &&
            dungeon_y <= rec_map_offset_y(m) + rec_map_last_y(m))
            return m;
    }
    return -1;
}

uint16_t map_get_creature_attributes(uint16_t group_thing)
{
    uint16_t creature_type = rec_group_creature_type(group_thing);
    /* Deviation: a creature type beyond the table (a corrupt dungeon) has no attributes; the original reads
       beyond it. */
    if (creature_type >= CREATURE_TYPE_COUNT)
        return 0;
    return tables_get_creature_info(creature_type)->attributes;
}
