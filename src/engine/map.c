/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/map.h"
#include "data/os.h"
#include "data/tables.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/party.h"
#include <inttypes.h>
#include <string.h>

#define FATAL_SQUARE_FIRST_THINGS_FULL 72
#define NO_DUNGEON_MAP                 0xff

int16_t g_current_map_index = -1, g_current_map_width, g_current_map_height;
uint8_t **g_current_map_data;
uint8_t *g_current_map;
uint16_t g_current_map_door_info[2];
static uint8_t *column_first_thing_index;

void map_reset(void)
{
    g_current_map_index = -1;
}

void map_set_current_map(int16_t map)
{
    if (map == g_current_map_index)
        return;
    g_current_map_index = map;
    g_current_map_data = g_dungeon_map_data[map];
    g_current_map = MAP_DESCRIPTOR(map);
    g_current_map_width = dungeon_map_width(map);
    g_current_map_height = dungeon_map_height(map);
    for (int16_t door = 0; door < 2; door++)
        g_current_map_door_info[door] = tables_get_door_attributes(map_door_set(map, door));
    column_first_thing_index = g_dungeon_column_first_thing_index + g_dungeon_map_first_column[map] * 2;
}

void map_set_current_map_and_party_map(int16_t map)
{
    g_party_map_index = map;
    map_set_current_map(map);
}

static bool is_corridor_or_pit(uint8_t square)
{
    return SQUARE_TYPE(square) == SQUARE_CORRIDOR || SQUARE_TYPE(square) == SQUARE_PIT;
}

uint8_t map_get_square(int16_t x, int16_t y)
{
    bool in_x = x >= 0 && x < g_current_map_width;
    bool in_y = y >= 0 && y < g_current_map_height;
    if (in_x && in_y)
        return g_current_map_data[x][y];
    if (in_x) {
        if (y == -1 && is_corridor_or_pit(g_current_map_data[x][0]))
            return WALL_ORNAMENT_ABOVE;
        if (y == g_current_map_height && is_corridor_or_pit(g_current_map_data[x][g_current_map_height - 1]))
            return WALL_ORNAMENT_BELOW;
    } else if (in_y) {
        if (x == -1 && is_corridor_or_pit(g_current_map_data[0][y]))
            return WALL_ORNAMENT_LEFT;
        if (x == g_current_map_width && is_corridor_or_pit(g_current_map_data[g_current_map_width - 1][y]))
            return WALL_ORNAMENT_RIGHT;
    }
    return SQUARE_WALL;
}

int16_t map_get_square_type(int16_t x, int16_t y)
{
    return SQUARE_TYPE(map_get_square(x, y));
}

bool map_is_square_open(int16_t x, int16_t y)
{
    int16_t type = map_get_square_type(x, y);
    return type == SQUARE_CORRIDOR || type == SQUARE_TELEPORTER || type == SQUARE_PIT || type == SQUARE_DOOR;
}

int16_t map_get_location_after_level_change(int16_t map, int16_t level_delta, int16_t *x, int16_t *y)
{
    if (g_party_map_index == NO_DUNGEON_MAP)
        return -1;
    int16_t dungeon_x = *x + map_offset_x(map);
    int16_t dungeon_y = *y + map_offset_y(map);
    uint16_t level = (uint16_t)(map_level(map) + level_delta); /* level 0 up wraps to no level */
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        if (level == map_level(m) && map_offset_x(m) <= dungeon_x && dungeon_x <= map_offset_x(m) + map_last_x(m) &&
            map_offset_y(m) <= dungeon_y && dungeon_y <= map_offset_y(m) + map_last_y(m)) {
            *y = dungeon_y - map_offset_y(m);
            *x = dungeon_x - map_offset_x(m);
            return m;
        }
    }
    return -1;
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

uint16_t map_get_next_thing(uint16_t thing)
{
    return be16(dungeon_get_thing_data(thing));
}

int16_t map_get_square_first_thing_index(int16_t x, int16_t y)
{
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height ||
        !(g_current_map_data[x][y] & SQUARE_HAS_THINGS))
        return -1;
    int16_t index = (int16_t)be16(column_first_thing_index + x * 2); /* the word read as signed */
    for (int16_t i = 0; i < y; i++)
        index += (g_current_map_data[x][i] & SQUARE_HAS_THINGS) != 0;
    return index;
}

uint16_t map_get_square_first_thing(int16_t x, int16_t y)
{
    int16_t index = map_get_square_first_thing_index(x, y);
    return index == -1 ? THING_ENDOFLIST : be16(g_square_first_things + (uint16_t)index * 2);
}

uint16_t map_get_square_first_object(int16_t x, int16_t y)
{
    uint16_t thing = map_get_square_first_thing(x, y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) < THING_GROUP)
        thing = map_get_next_thing(thing);
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
    thing_set_next(thing, THING_ENDOFLIST);
    if (x >= 0) {
        uint8_t *square = &g_current_map_data[x][y];
        if (!(*square & SQUARE_HAS_THINGS)) {
            uint16_t count = rec_dungeon_square_first_thing_count();
            if (be16(g_square_first_things + (count - 1) * 2) != THING_NONE)
                game_fatal(FATAL_SQUARE_FIRST_THINGS_FULL);
            *square |= SQUARE_HAS_THINGS;
            add_to_later_columns(x, 1);
            uint16_t index = map_get_square_first_thing_index(x, y);
            uint8_t *p = g_square_first_things + index * 2;
            memmove(p + 2, p, (count - index - 1) * 2);
            wr16(p, thing);
            return;
        }
        list = map_get_square_first_thing(x, y);
    }
    for (uint16_t next = map_get_next_thing(list); next != THING_ENDOFLIST; next = map_get_next_thing(list))
        list = next;
    thing_set_next(list, thing);
}

void map_unlink_thing_from_list(uint16_t thing, uint16_t list, int16_t x, int16_t y)
{
    if (thing == THING_ENDOFLIST)
        return;
    thing = THING_STRIP_CELL(thing);
    if (x >= 0) {
        uint16_t index = map_get_square_first_thing_index(x, y);
        uint8_t *first = g_square_first_things + index * 2;
        uint16_t after = map_get_next_thing(thing);
        if (after == THING_NONE && thing == THING_STRIP_CELL(be16(first)))
            report_free_record("unlink of a free record", x, y); /* its next is gone: remove the whole entry */
        if ((after == THING_ENDOFLIST || after == THING_NONE) && thing == THING_STRIP_CELL(be16(first))) {
            uint16_t last = rec_dungeon_square_first_thing_count() - 1;
            g_current_map_data[x][y] &= ~SQUARE_HAS_THINGS;
            memmove(first, first + 2, (last - index) * 2);
            wr16(g_square_first_things + last * 2, THING_NONE);
            add_to_later_columns(x, -1);
            thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        if (thing == THING_STRIP_CELL(be16(first))) {
            wr16(first, map_get_next_thing(thing));
            thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = be16(first);
    }
    uint16_t next = map_get_next_thing(list);
    while (thing != THING_STRIP_CELL(next)) {
        if (next == THING_ENDOFLIST || next == THING_NONE) {
            if (x >= 0)
                thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = next;
        next = map_get_next_thing(next);
    }
    thing_set_next(list, map_get_next_thing(next));
    thing_set_next(thing, THING_ENDOFLIST);
}

uint16_t map_get_square_group(int16_t x, int16_t y)
{
    uint16_t thing = map_get_square_first_thing(x, y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) != THING_GROUP)
        thing = map_get_next_thing(thing);
    return thing;
}

uint16_t map_get_creature_attributes(uint16_t group_thing)
{
    return tables_get_creature_info(rec_group_creature_type(group_thing))->attributes;
}

#define TYPE_MASK           0x7f
#define IMPORTANT           0x80
#define JUNK_RESERVED_COUNT 3

uint16_t map_get_unused_thing(uint16_t thing_type)
{
    uint16_t type = thing_type & ~NO_JUNK_RESERVE;
    uint16_t size = g_thing_data_byte_count[type];
    int16_t count = be16(g_dungeon_header + DUNGEON_HEADER_THING_COUNTS + type * 2);
    if (thing_type == THING_JUNK)
        count -= JUNK_RESERVED_COUNT;
    for (int16_t i = 0; i < count; i++) {
        uint8_t *record = g_thing_data[type] + i * size;
        if (be16(record) == THING_NONE) {
            memset(record, 0, size);
            wr16(record, THING_END);
            return THING_MAKE(type, i);
        }
    }
    return THING_NONE;
}

void thing_set_free(uint16_t thing)
{
    wr16(dungeon_get_thing_data(thing), THING_NONE);
}
uint16_t weapon_type(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[3] & TYPE_MASK;
}
uint16_t armour_type(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[3] & TYPE_MASK;
}
uint16_t junk_type(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[3] & TYPE_MASK;
}
uint16_t potion_type(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[2] & TYPE_MASK;
}
uint16_t potion_power(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[3];
}
uint16_t container_type(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[5] & 6) >> 1;
}
uint16_t container_first_thing(uint16_t thing)
{
    return be16(dungeon_get_thing_data(thing) + 2);
}
bool rec_object_is_cursed(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[2] & 1;
}
void rec_object_set_important(uint16_t thing)
{
    dungeon_get_thing_data(thing)[3] |= IMPORTANT;
}
void rec_object_clear_important(uint16_t thing)
{
    dungeon_get_thing_data(thing)[3] &= (uint8_t)~IMPORTANT;
}
bool weapon_is_lit(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[2] >> 7;
}
uint16_t weapon_charges(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0x3c) >> 2;
}
uint16_t junk_charges(uint16_t thing)
{
    return dungeon_get_thing_data(thing)[2] >> 6;
}
uint16_t armour_charges(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0x1e) >> 1;
}
bool scroll_is_closed(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0xfc) != 0;
}
bool weapon_is_poisoned(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0x02) != 0;
}
bool weapon_is_broken(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0x40) != 0;
}
bool armour_is_broken(uint16_t thing)
{
    return (dungeon_get_thing_data(thing)[2] & 0x20) != 0;
}
uint16_t scroll_text_index(uint16_t thing)
{
    return be16(dungeon_get_thing_data(thing) + 2) & 0x3ff;
}
void container_set_first_thing(uint16_t thing, uint16_t first)
{
    wr16(dungeon_get_thing_data(thing) + 2, first);
}

void weapon_set_lit(uint16_t thing, bool lit)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = lit ? d[2] | 0x80 : d[2] & 0x7f;
}

void weapon_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & ~0x3c) | (charges << 2 & 0x3c);
}

void armour_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & ~0x1e) | (charges << 1 & 0x1e);
}

void junk_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & 0x3f) | charges << 6;
}

void junk_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[3] = (d[3] & IMPORTANT) | type;
}

void potion_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & IMPORTANT) | type;
}

void potion_set_power(uint16_t thing, uint16_t power)
{
    dungeon_get_thing_data(thing)[3] = power;
}

void rec_object_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[3] = (d[3] & IMPORTANT) | (type & TYPE_MASK);
}

void rec_object_set_cursed(uint16_t thing, bool cursed)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & ~1) | (cursed != 0);
}

void scroll_set_closed(uint16_t thing, bool closed)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (d[2] & 3) | (closed ? 4 : 0);
}

void rec_sensor_disable(uint16_t thing)
{
    dungeon_get_thing_data(thing)[3] &= 0x80;
}

void map_step(int16_t direction, int16_t forward, int16_t right, int16_t *x, int16_t *y)
{
    int16_t side = direction_right(direction);
    *x = *x + forward * tables_get_step_east(direction) + right * tables_get_step_east(side);
    *y = *y + forward * tables_get_step_north(direction) + right * tables_get_step_north(side);
}
