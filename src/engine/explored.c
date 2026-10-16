/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/explored.h"
#include "engine/dungeon.h"
#include <stdlib.h>
#include <string.h>

enum { SAVE_VERSION = 1, SAVE_HEADER_SIZE = 6 }; /* magic, version, map count */
static const char SAVE_MAGIC[] = EXPLORED_SAVE_MAGIC;
enum { SAVE_MAGIC_SIZE = sizeof SAVE_MAGIC - 1 }; /* without the NUL */

/* One byte per square of every map, the maps one after another. */
static uint8_t *seen;
static size_t seen_size;

static size_t map_square_count(int16_t map)
{
    return (size_t)dungeon_map_width(map) * (size_t)dungeon_map_height(map);
}

static size_t map_first_square(int16_t map)
{
    size_t offset = 0;
    for (int16_t m = 0; m < map; m++)
        offset += map_square_count(m);
    return offset;
}

void explored_reset(void)
{
    free(seen);
    seen_size = map_first_square(dungeon_map_count());
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
    return seen + map_first_square(map) + (size_t)x * dungeon_map_height(map) + y;
}

bool explored_is_seen(int16_t map, int16_t x, int16_t y)
{
    uint8_t *square = get_seen_square(map, x, y);
    return square && *square;
}

void explored_mark(int16_t map, int16_t x, int16_t y)
{
    uint8_t *square = get_seen_square(map, x, y);
    if (square)
        *square = 1;
}

/* Block: "AMAP", version, map count, then per map its width, height and one byte per square. */
int explored_save(uint8_t **buf, size_t *len)
{
    if (!seen)
        explored_reset();
    int16_t map_count = dungeon_map_count();
    size_t size = SAVE_HEADER_SIZE + 2 * (size_t)map_count + seen_size;
    uint8_t *out = malloc(size);
    uint8_t *p = out;
    if (!out)
        return -1;
    memcpy(p, SAVE_MAGIC, SAVE_MAGIC_SIZE);
    p += SAVE_MAGIC_SIZE;
    *p++ = SAVE_VERSION;
    *p++ = map_count;
    for (int16_t map = 0; map < map_count; map++) {
        *p++ = dungeon_map_width(map);
        *p++ = dungeon_map_height(map);
    }
    memcpy(p, seen, seen_size);
    *buf = out;
    *len = size;
    return 0;
}

int explored_load(const uint8_t *buf, size_t len)
{
    int16_t map_count = dungeon_map_count();
    size_t expected = SAVE_HEADER_SIZE + 2 * (size_t)map_count + map_first_square(map_count);
    if (!buf || len != expected || memcmp(buf, SAVE_MAGIC, SAVE_MAGIC_SIZE) || buf[SAVE_MAGIC_SIZE] != SAVE_VERSION ||
        buf[SAVE_MAGIC_SIZE + 1] != map_count)
        return -1;
    const uint8_t *p = buf + SAVE_HEADER_SIZE;
    for (int16_t map = 0; map < map_count; map++, p += 2)
        if (p[0] != dungeon_map_width(map) || p[1] != dungeon_map_height(map))
            return -1;
    for (size_t i = 0; i < len - (size_t)(p - buf); i++)
        if (p[i] > 1)
            return -1;
    explored_reset();
    if (!seen)
        return -1;
    memcpy(seen, p, seen_size);
    return 0;
}
