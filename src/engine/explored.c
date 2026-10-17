/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "explored.h"
#include "dungeon.h"
#include "game.h"
#include "map.h"
#include "party.h"
#include "record.h"
#include <stdlib.h>
#include <string.h>

/* Version and header size of the explored block. */
#define EXPLORED_VERSION     1 /* magic, version, map count */
#define EXPLORED_HEADER_SIZE 6
static const char SAVE_MAGIC[] = EXPLORED_SAVE_MAGIC;
/* Size of the magic of the explored block. */
#define EXPLORED_MAGIC_SIZE (sizeof SAVE_MAGIC - 1) /* without the NUL */

/* One byte per square of every map, the maps one after another. */
static uint8_t *seen;
static size_t seen_size;

static size_t count_squares(int16_t map)
{
    return (size_t)dungeon_map_width(map) * (size_t)dungeon_map_height(map);
}

/* Index of the first square of a map in the seen storage. */
static size_t first_square(int16_t map)
{
    size_t offset = 0;
    for (int16_t m = 0; m < map; m++)
        offset += count_squares(m);
    return offset;
}

void explored_reset(void)
{
    free(seen);
    seen_size = first_square(dungeon_map_count());
    seen = calloc(seen_size ? seen_size : 1, 1);
}

static bool is_on_map(int16_t map, int16_t x, int16_t y)
{
    return map >= 0 && map < dungeon_map_count() && x >= 0 && x < dungeon_map_width(map) && y >= 0 &&
           y < dungeon_map_height(map);
}

/* The byte of square (x, y), or NULL outside the map. */
static uint8_t *get_seen_square(int16_t map, int16_t x, int16_t y)
{
    if (!seen)
        explored_reset();
    if (!seen || !is_on_map(map, x, y))
        return NULL;
    return seen + first_square(map) + (size_t)x * dungeon_map_height(map) + y;
}

bool explored_is_seen(int16_t map, int16_t x, int16_t y)
{
    uint8_t *square = get_seen_square(map, x, y);
    return square && *square;
}

/* Marks square (x, y) of map as seen; a square outside the map is ignored. */
static void mark_seen(int16_t map, int16_t x, int16_t y)
{
    uint8_t *square = get_seen_square(map, x, y);
    if (square)
        *square = 1;
}

/* A square the view cannot look through: a wall, a fake wall that looks like a wall, a closed door. */
static bool blocks_sight(int16_t map, int16_t x, int16_t y)
{
    if (!is_on_map(map, x, y))
        return true;
    uint8_t square = rec_square_byte(map, x, y);
    switch (rec_square_type(square)) {
    case SQUARE_WALL:
        return true;
    case SQUARE_FAKE_WALL:
        return !(square & FAKE_WALL_OPEN);
    case SQUARE_DOOR:
        return rec_door_state(square) == DOOR_CLOSED;
    default:
        return false;
    }
}

/* The darkest light level. */
#define DARKEST_PALETTE 5

/* Rows the party can make out in the current light: the darkest palettes hide the far rows. */
static int16_t light_depths(void)
{
    if (g_session.cheat_light)
        return EXPLORED_VIEW_DEPTHS;
    /* the darkest two light levels hide the far rows */
    if (g_party_light_level >= DARKEST_PALETTE)
        return EXPLORED_VIEW_DEPTHS - 2;
    if (g_party_light_level >= DARKEST_PALETTE - 1)
        return EXPLORED_VIEW_DEPTHS - 1;
    return EXPLORED_VIEW_DEPTHS;
}

int16_t explored_get_visible_depths(Position place, int16_t direction)
{
    int16_t limit = light_depths();
    for (int16_t depth = 1; depth < limit; depth++) {
        int16_t ahead_x = place.x;
        int16_t ahead_y = place.y;
        map_step(direction, depth, 0, &ahead_x, &ahead_y);
        if (blocks_sight(place.map, ahead_x, ahead_y))
            return depth + 1;
    }
    return limit;
}

bool explored_is_side_visible(Position place, int16_t direction, int16_t depth, int16_t right)
{
    if (depth == 0 || right == 0)
        return true;
    int16_t ahead_x = place.x;
    int16_t ahead_y = place.y;
    int16_t near_x = place.x;
    int16_t near_y = place.y;
    map_step(direction, depth, 0, &ahead_x, &ahead_y);
    map_step(direction, depth - 1, right, &near_x, &near_y);
    return !blocks_sight(place.map, ahead_x, ahead_y) || !blocks_sight(place.map, near_x, near_y);
}

void explored_mark_visible(Position place, int16_t direction)
{
    int16_t depths = explored_get_visible_depths(place, direction);
    for (int16_t depth = 0; depth < depths; depth++)
        for (int16_t side = -1; side <= 1; side++) {
            if (!explored_is_side_visible(place, direction, depth, side))
                continue;
            int16_t square_x = place.x;
            int16_t square_y = place.y;
            map_step(direction, depth, side, &square_x, &square_y);
            mark_seen(place.map, square_x, square_y);
        }
}

bool explored_save(uint8_t **out_data, size_t *out_length)
{
    if (!seen)
        explored_reset();
    int16_t map_count = dungeon_map_count();
    size_t size = EXPLORED_HEADER_SIZE + 2 * (size_t)map_count + seen_size;
    uint8_t *out = malloc(size);
    uint8_t *p = out;
    if (!out)
        return false;
    memcpy(p, SAVE_MAGIC, EXPLORED_MAGIC_SIZE);
    p += EXPLORED_MAGIC_SIZE;
    *p++ = EXPLORED_VERSION;
    *p++ = map_count;
    for (int16_t map = 0; map < map_count; map++) {
        *p++ = dungeon_map_width(map);
        *p++ = dungeon_map_height(map);
    }
    memcpy(p, seen, seen_size);
    *out_data = out;
    *out_length = size;
    return true;
}

bool explored_load(const uint8_t *buf, size_t len)
{
    int16_t map_count = dungeon_map_count();
    size_t expected = EXPLORED_HEADER_SIZE + 2 * (size_t)map_count + first_square(map_count);
    if (!buf || len != expected || memcmp(buf, SAVE_MAGIC, EXPLORED_MAGIC_SIZE) ||
        buf[EXPLORED_MAGIC_SIZE] != EXPLORED_VERSION || buf[EXPLORED_MAGIC_SIZE + 1] != map_count)
        return false;
    const uint8_t *p = buf + EXPLORED_HEADER_SIZE;
    for (int16_t map = 0; map < map_count; map++, p += 2)
        if (p[0] != dungeon_map_width(map) || p[1] != dungeon_map_height(map))
            return false;
    for (size_t i = 0; i < len - (size_t)(p - buf); i++)
        if (p[i] > 1)
            return false;
    explored_reset();
    if (!seen)
        return false;
    memcpy(seen, p, seen_size);
    return true;
}
