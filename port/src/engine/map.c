#include "map.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* KAOS tables. */
#define KAOS_DOOR_INFO 0x1016e2
#define KAOS_CREATURE_INFO 0x10192e
#define CREATURE_INFO_SIZE 26
#define CREATURE_INFO_ATTRIBUTES 2

#define FATAL_SQUARE_FIRST_THINGS_FULL 72
#define NO_DUNGEON_MAP 0xff

int16_t g_current_map_index = -1, g_current_map_width, g_current_map_height;
uint8_t **g_current_map_data;
uint8_t *g_current_map;
uint16_t g_current_map_door_info[2];
static uint8_t *column_first_thing_index;

/* Forget the current map, for example after load_dungeon replaced the buffers. */
void map_reset(void)
{
    g_current_map_index = -1;
}

/* Selects the map that the square and thing functions work on. */
void set_current_map(int16_t map)
{
    if (map == g_current_map_index)
        return;
    g_current_map_index = map;
    g_current_map_data = g_dungeon_map_data[map];
    g_current_map = MAP_DESCRIPTOR(map);
    g_current_map_width = dungeon_map_width(map);
    g_current_map_height = dungeon_map_height(map);
    for (int16_t door = 0; door < 2; door++)
        g_current_map_door_info[door] = kaos_u16(KAOS_DOOR_INFO + map_door_set(map, door) * 2);
    column_first_thing_index = g_dungeon_column_first_thing_index + g_dungeon_map_first_column[map] * 2;
}

/* The party map becomes the current map. The ornament lists are view data (view_load_map). */
void set_current_map_and_party_map(int16_t map)
{
    g_party_map_index = map;
    set_current_map(map);
}

static int16_t is_corridor_or_pit(uint8_t square)
{
    return SQUARE_TYPE(square) == SQUARE_CORRIDOR || SQUARE_TYPE(square) == SQUARE_PIT;
}

/* Outside the map: a wall, with the "ornament allowed" bit when the adjacent square is a corridor or a pit. */
uint8_t get_square(int16_t x, int16_t y)
{
    int16_t in_x = x >= 0 && x < g_current_map_width, in_y = y >= 0 && y < g_current_map_height;
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

int16_t get_square_type(int16_t x, int16_t y)
{
    return SQUARE_TYPE(get_square(x, y));
}

/* Corridor, pit, door, or teleporter. */
int16_t is_square_open(int16_t x, int16_t y)
{
    int16_t type = get_square_type(x, y);
    return type == SQUARE_CORRIDOR || type == SQUARE_TELEPORTER || type == SQUARE_PIT || type == SQUARE_DOOR;
}

/* The map one level below (LEVEL_DOWN) or above (LEVEL_UP) that covers the dungeon position of (x, y). -1 when none. */
int16_t get_location_after_level_change(int16_t map, int16_t level_delta, int16_t *x, int16_t *y)
{
    if (g_party_map_index == NO_DUNGEON_MAP)
        return -1;
    int16_t dungeon_x = *x + map_offset_x(map), dungeon_y = *y + map_offset_y(map);
    uint16_t level = (uint16_t)(map_level(map) + level_delta);
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

/* The stairs axis is north-south or east-west. The exit side is the one that is not a wall or stairs. */
int16_t get_stairs_exit_direction(int16_t x, int16_t y)
{
    int16_t direction = get_square(x, y) & STAIRS_NORTH_SOUTH ? DIRECTION_NORTH : DIRECTION_EAST;
    int16_t ahead_x = x, ahead_y = y;
    update_map_coordinates_after_relative_movement(direction, 1, 0, &ahead_x, &ahead_y);
    int16_t type = get_square_type(ahead_x, ahead_y);
    return type == SQUARE_WALL || type == SQUARE_STAIRS ? NORMALIZE(direction + 2) : direction;
}

uint16_t get_next_thing(uint16_t thing)
{
    return BE16(dungeon_get_thing_data(thing));
}

/* Index of the first thing of the square in the square first thing table, or -1. */
int16_t get_square_first_thing_index(int16_t x, int16_t y)
{
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height || !(g_current_map_data[x][y] & SQUARE_HAS_THINGS))
        return -1;
    int16_t index = (int16_t)BE16(column_first_thing_index + x * 2);
    for (int16_t i = 0; i < y; i++)
        index += (g_current_map_data[x][i] & SQUARE_HAS_THINGS) != 0;
    return index;
}

uint16_t get_square_first_thing(int16_t x, int16_t y)
{
    int16_t index = get_square_first_thing_index(x, y);
    return index == -1 ? THING_ENDOFLIST : BE16(g_square_first_things + (uint16_t)index * 2);
}

/* The first group, object, projectile, or explosion of the square. */
uint16_t get_square_first_object(int16_t x, int16_t y)
{
    uint16_t thing = get_square_first_thing(x, y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) < THING_GROUP)
        thing = get_next_thing(thing);
    return thing;
}

static void add_to_later_columns(int16_t x, int16_t delta)
{
    int16_t global_x = x + g_dungeon_map_first_column[g_current_map_index];
    for (int16_t c = global_x + 1; c < g_dungeon_column_count; c++) {
        uint8_t *p = g_dungeon_column_first_thing_index + c * 2;
        WR16(p, BE16(p) + delta);
    }
}

static void fatal(int16_t code)
{
    fprintf(stderr, "System Error %d\n", code);
    exit(1);
}

/* A free record (THING_NONE) must never enter a square list: a save with it is damaged. Logs where it comes from. */
static void report_free_record(const char *action, int16_t x, int16_t y)
{
    enum { BACKTRACE_FRAMES = 16 };
    void *frames[BACKTRACE_FRAMES];
    fprintf(stderr, "square list guard: %s, map %d (%d,%d), game time %lu\n", action, g_current_map_index, x, y,
            (unsigned long)g_game_time);
    backtrace_symbols_fd(frames, backtrace(frames, BACKTRACE_FRAMES), 2);
}

/* x >= 0: append the thing to the list of square (x, y). x < 0: append it to the list that starts at list. */
void link_thing_to_list(uint16_t thing, uint16_t list, int16_t x, int16_t y)
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
            if (BE16(g_square_first_things + (count - 1) * 2) != THING_NONE)
                fatal(FATAL_SQUARE_FIRST_THINGS_FULL);
            *square |= SQUARE_HAS_THINGS;
            add_to_later_columns(x, 1);
            uint16_t index = (uint16_t)get_square_first_thing_index(x, y);
            uint8_t *p = g_square_first_things + index * 2;
            memmove(p + 2, p, (count - index - 1) * 2);
            WR16(p, thing);
            return;
        }
        list = get_square_first_thing(x, y);
    }
    for (uint16_t next = get_next_thing(list); next != THING_ENDOFLIST; next = get_next_thing(list))
        list = next;
    thing_set_next(list, thing);
}

/* Removes the thing from the square list (x >= 0) or from the list after list. Things compare without the cell. */
void unlink_thing_from_list(uint16_t thing, uint16_t list, int16_t x, int16_t y)
{
    if (thing == THING_ENDOFLIST)
        return;
    thing = THING_STRIP_CELL(thing);
    if (x >= 0) {
        uint16_t index = (uint16_t)get_square_first_thing_index(x, y);
        uint8_t *first = g_square_first_things + index * 2;
        uint16_t after = get_next_thing(thing);
        if (after == THING_NONE && thing == THING_STRIP_CELL(BE16(first)))
            report_free_record("unlink of a free record", x, y); /* its next is gone: remove the whole entry */
        if ((after == THING_ENDOFLIST || after == THING_NONE) && thing == THING_STRIP_CELL(BE16(first))) {
            uint16_t last = rec_dungeon_square_first_thing_count() - 1;
            g_current_map_data[x][y] &= ~SQUARE_HAS_THINGS;
            memmove(first, first + 2, (last - index) * 2);
            WR16(g_square_first_things + last * 2, THING_NONE);
            add_to_later_columns(x, -1);
            thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        if (thing == THING_STRIP_CELL(BE16(first))) {
            WR16(first, get_next_thing(thing));
            thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = BE16(first);
    }
    uint16_t next = get_next_thing(list);
    while (thing != THING_STRIP_CELL(next)) {
        if (next == THING_ENDOFLIST || next == THING_NONE) {
            if (x >= 0)
                thing_set_next(thing, THING_ENDOFLIST);
            return;
        }
        list = next;
        next = get_next_thing(next);
    }
    thing_set_next(list, get_next_thing(next));
    thing_set_next(thing, THING_ENDOFLIST);
}

uint16_t get_square_group(int16_t x, int16_t y)
{
    uint16_t thing = get_square_first_thing(x, y);
    while (thing != THING_ENDOFLIST && THING_TYPE(thing) != THING_GROUP)
        thing = get_next_thing(thing);
    return thing;
}

uint16_t get_creature_attributes(uint16_t group_thing)
{
    return kaos_u16(KAOS_CREATURE_INFO + rec_group_creature_type(group_thing) * CREATURE_INFO_SIZE + CREATURE_INFO_ATTRIBUTES);
}

#define TYPE_MASK 0x7f
#define IMPORTANT 0x80
#define JUNK_RESERVED_COUNT 3

/* A free record of the type. Junk keeps 3 records in reserve unless NO_JUNK_RESERVE is set.
   ponytail: no get_discarded_thing yet; returns THING_NONE when no record is free. */
uint16_t get_unused_thing(uint16_t thing_type)
{
    uint16_t type = thing_type & ~NO_JUNK_RESERVE, size = g_thing_data_byte_count[type];
    int16_t count = (int16_t)BE16(g_dungeon_header + 12 + type * 2);
    if (thing_type == THING_JUNK)
        count -= JUNK_RESERVED_COUNT;
    for (int16_t i = 0; i < count; i++) {
        uint8_t *record = g_thing_data[type] + i * size;
        if (BE16(record) == THING_NONE) {
            memset(record, 0, size);
            WR16(record, THING_END);
            return (uint16_t)(type << 10 | i);
        }
    }
    return THING_NONE;
}

void thing_set_free(uint16_t thing) { WR16(dungeon_get_thing_data(thing), THING_NONE); }
uint16_t weapon_type(uint16_t thing) { return dungeon_get_thing_data(thing)[3] & TYPE_MASK; }
uint16_t armour_type(uint16_t thing) { return dungeon_get_thing_data(thing)[3] & TYPE_MASK; }
uint16_t junk_type(uint16_t thing) { return dungeon_get_thing_data(thing)[3] & TYPE_MASK; }
uint16_t potion_type(uint16_t thing) { return dungeon_get_thing_data(thing)[2] & TYPE_MASK; }
uint16_t potion_power(uint16_t thing) { return dungeon_get_thing_data(thing)[3]; }
uint16_t container_type(uint16_t thing) { return (dungeon_get_thing_data(thing)[5] & 6) >> 1; }
uint16_t container_first_thing(uint16_t thing) { return BE16(dungeon_get_thing_data(thing) + 2); }
int16_t rec_object_is_cursed(uint16_t thing) { return dungeon_get_thing_data(thing)[2] & 1; }
void rec_object_set_important(uint16_t thing) { dungeon_get_thing_data(thing)[3] |= IMPORTANT; }
void rec_object_clear_important(uint16_t thing) { dungeon_get_thing_data(thing)[3] &= (uint8_t)~IMPORTANT; }
int16_t weapon_is_lit(uint16_t thing) { return dungeon_get_thing_data(thing)[2] >> 7; }
uint16_t weapon_charges(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0x3c) >> 2; }
uint16_t junk_charges(uint16_t thing) { return dungeon_get_thing_data(thing)[2] >> 6; }
uint16_t armour_charges(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0x1e) >> 1; }
int16_t scroll_is_closed(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0xfc) != 0; }
int16_t weapon_is_poisoned(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0x02) != 0; }
int16_t weapon_is_broken(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0x40) != 0; }
int16_t armour_is_broken(uint16_t thing) { return (dungeon_get_thing_data(thing)[2] & 0x20) != 0; }
uint16_t scroll_text_index(uint16_t thing) { return BE16(dungeon_get_thing_data(thing) + 2) & 0x3ff; }
void container_set_first_thing(uint16_t thing, uint16_t first) { WR16(dungeon_get_thing_data(thing) + 2, first); }

void weapon_set_lit(uint16_t thing, int16_t lit)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = lit ? d[2] | 0x80 : d[2] & 0x7f;
}

void weapon_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & ~0x3c) | (charges & 15) << 2);
}

void armour_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & ~0x1e) | (charges & 15) << 1);
}

void junk_set_charges(uint16_t thing, uint16_t charges)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & 0x3f) | charges << 6);
}

void junk_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[3] = (uint8_t)((d[3] & IMPORTANT) | type);
}

void potion_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & IMPORTANT) | type);
}

void potion_set_power(uint16_t thing, uint16_t power) { dungeon_get_thing_data(thing)[3] = (uint8_t)power; }

/* Weapon, armour, and junk: the type in word 2 bits 0-6; the important bit stays. */
void rec_object_set_type(uint16_t thing, uint16_t type)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[3] = (uint8_t)((d[3] & IMPORTANT) | (type & TYPE_MASK));
}

void rec_object_set_cursed(uint16_t thing, int16_t cursed)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & ~1) | (cursed != 0));
}

/* The closed bits are 10-15; bits 8-9 stay. */
void scroll_set_closed(uint16_t thing, int16_t closed)
{
    uint8_t *d = dungeon_get_thing_data(thing);
    d[2] = (uint8_t)((d[2] & 3) | (closed ? 4 : 0));
}

/* Sensor type 0 does nothing. The sensor data stays. */
void rec_sensor_disable(uint16_t thing) { dungeon_get_thing_data(thing)[3] &= 0x80; }
