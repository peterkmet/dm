#include "automap.h"
#include <stdlib.h>
#include <string.h>
#include "dungeon.h"
#include "game.h"
#include "group.h"
#include "map.h"
#include "ui.h"

/* Square flags that map.h does not name yet. */
enum { TELEPORTER_VISIBLE = 4, DOOR_PASSAGE_NORTH_SOUTH = 8 };

enum { VIEW_DEPTH_COUNT = 4 };                    /* the view draws the party row and 3 rows ahead */
enum { SAVE_VERSION = 1, SAVE_HEADER_SIZE = 6 };  /* magic, version, map count */
static const char SAVE_MAGIC[] = AUTOMAP_SAVE_MAGIC;
enum { SAVE_MAGIC_SIZE = sizeof SAVE_MAGIC - 1 };  /* without the NUL */

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
    for (int16_t m = 0; m < map; m++) offset += map_square_count(m);
    return offset;
}

void automap_reset(void)
{
    free(seen);
    seen_size = map_first_square(dungeon_map_count());
    seen = calloc(seen_size ? seen_size : 1, 1);
}

static int is_on_map(int16_t map, int16_t x, int16_t y)
{
    return map >= 0 && map < dungeon_map_count() && x >= 0 && x < dungeon_map_width(map) && y >= 0 && y < dungeon_map_height(map);
}

/* The byte of square (x, y), or NULL outside the map. */
static uint8_t *seen_square(int16_t map, int16_t x, int16_t y)
{
    if (!seen) automap_reset();
    if (!seen || !is_on_map(map, x, y)) return NULL;
    return seen + map_first_square(map) + (size_t)x * dungeon_map_height(map) + y;
}

int automap_is_seen(int16_t map, int16_t x, int16_t y)
{
    uint8_t *square = seen_square(map, x, y);
    return square && *square;
}

/* A square the view cannot look through: a wall, a fake wall that looks like a wall, a closed door. */
static int blocks_sight(int16_t map, int16_t x, int16_t y)
{
    if (!is_on_map(map, x, y)) return 1;
    uint8_t square = dungeon_square(map, x, y);
    switch (SQUARE_TYPE(square)) {
    case SQUARE_WALL: return 1;
    case SQUARE_FAKE_WALL: return !(square & FAKE_WALL_OPEN);
    case SQUARE_DOOR: return door_state(square) == DOOR_CLOSED;
    default: return 0;
    }
}

#define DARKEST_PALETTE 5  /* g_dungeon_view_palette_index of the darkest light level */

/* Rows the party can make out in the current light: the darkest palettes hide the far rows. */
static int16_t light_depths(void)
{
    /* ponytail: fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it */
    if (g_dungeon_view_palette_index >= DARKEST_PALETTE) return VIEW_DEPTH_COUNT - 2;
    if (g_dungeon_view_palette_index >= DARKEST_PALETTE - 1) return VIEW_DEPTH_COUNT - 1;
    return VIEW_DEPTH_COUNT;
}

int16_t automap_visible_depths(int16_t map, int16_t x, int16_t y, int16_t dir)
{
    int16_t limit = light_depths();
    for (int16_t depth = 1; depth < limit; depth++) {
        int16_t ahead_x = x, ahead_y = y;
        update_map_coordinates_after_relative_movement(dir, depth, 0, &ahead_x, &ahead_y);
        if (blocks_sight(map, ahead_x, ahead_y)) return depth + 1;
    }
    return limit;
}

/* A side square is seen when the square ahead at its depth or the side square one row nearer is open. */
static int side_is_visible(int16_t map, int16_t x, int16_t y, int16_t dir, int16_t depth, int16_t side)
{
    if (depth == 0 || side == 0) return 1;
    int16_t ahead_x = x, ahead_y = y, near_x = x, near_y = y;
    update_map_coordinates_after_relative_movement(dir, depth, 0, &ahead_x, &ahead_y);
    update_map_coordinates_after_relative_movement(dir, depth - 1, side, &near_x, &near_y);
    return !blocks_sight(map, ahead_x, ahead_y) || !blocks_sight(map, near_x, near_y);
}

void automap_mark_seen(int16_t map, int16_t x, int16_t y, int16_t dir)
{
    int16_t depths = automap_visible_depths(map, x, y, dir);
    for (int16_t depth = 0; depth < depths; depth++)
        for (int16_t side = -1; side <= 1; side++) {
            if (!side_is_visible(map, x, y, dir, depth, side)) continue;
            int16_t square_x = x, square_y = y;
            update_map_coordinates_after_relative_movement(dir, depth, side, &square_x, &square_y);
            uint8_t *square = seen_square(map, square_x, square_y);
            if (square) *square = 1;
        }
}

/* Block: "AMAP", version, map count, then per map its width, height and one byte per square. */
int automap_save(uint8_t **buf, size_t *len)
{
    if (!seen) automap_reset();
    int16_t map_count = dungeon_map_count();
    size_t size = SAVE_HEADER_SIZE + 2 * (size_t)map_count + seen_size;
    uint8_t *out = malloc(size), *p = out;
    if (!out) return -1;
    memcpy(p, SAVE_MAGIC, SAVE_MAGIC_SIZE);
    p += SAVE_MAGIC_SIZE;
    *p++ = SAVE_VERSION;
    *p++ = (uint8_t)map_count;
    for (int16_t map = 0; map < map_count; map++) {
        *p++ = (uint8_t)dungeon_map_width(map);
        *p++ = (uint8_t)dungeon_map_height(map);
    }
    memcpy(p, seen, seen_size);
    *buf = out;
    *len = size;
    return 0;
}

int automap_load(const uint8_t *buf, size_t len)
{
    int16_t map_count = dungeon_map_count();
    size_t expected = SAVE_HEADER_SIZE + 2 * (size_t)map_count + map_first_square(map_count);
    if (!buf || len != expected || memcmp(buf, SAVE_MAGIC, SAVE_MAGIC_SIZE) || buf[4] != SAVE_VERSION || buf[5] != map_count)
        return -1;
    const uint8_t *p = buf + SAVE_HEADER_SIZE;
    for (int16_t map = 0; map < map_count; map++, p += 2)
        if (p[0] != dungeon_map_width(map) || p[1] != dungeon_map_height(map)) return -1;
    for (size_t i = 0; i < len - (size_t)(p - buf); i++)
        if (p[i] > 1) return -1;
    automap_reset();
    if (!seen) return -1;
    memcpy(seen, p, seen_size);
    return 0;
}

/* Fills the part of the box (x, y, w, h) that lies inside area. */
static void fill_clipped(Bitmap *target, Box area, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    int16_t left = max16(x, area.x), top = max16(y, area.y);
    int16_t right = min16(x + w, area.x + area.w), bottom = min16(y + h, area.y + area.h);
    if (left >= right || top >= bottom) return;
    Box box = { left, top, right - left, bottom - top };
    draw_fill(target, &box, color);
}

/* One seen square at screen position (left, top): a light floor with a 1 pixel grid gap, or a full dark wall. */
static void draw_square(Bitmap *target, Box area, uint8_t square, int16_t left, int16_t top)
{
    const int16_t size = AUTOMAP_CELL_SIZE, floor = AUTOMAP_CELL_SIZE - 1;
    const int16_t bar = AUTOMAP_CELL_SIZE / 3, middle = (AUTOMAP_CELL_SIZE - AUTOMAP_CELL_SIZE / 3) / 2;
    int type = SQUARE_TYPE(square);
    if (type == SQUARE_WALL || (type == SQUARE_FAKE_WALL && !(square & FAKE_WALL_OPEN))) {
        fill_clipped(target, area, left, top, size, size, COLOR_DARKEST_GRAY);
        return;
    }
    uint8_t color = COLOR_LIGHT_GRAY;
    if (type == SQUARE_STAIRS) color = square & STAIRS_UP ? COLOR_WHITE : COLOR_GOLD;
    else if (type == SQUARE_PIT && (square & PIT_OPEN)) color = COLOR_RED;
    else if (type == SQUARE_TELEPORTER && (square & TELEPORTER_OPEN) && (square & TELEPORTER_VISIBLE)) color = COLOR_CYAN;
    fill_clipped(target, area, left, top, floor, floor, color);
    if (type != SQUARE_DOOR) return;
    int state = door_state(square);
    uint8_t door_color = state == DOOR_OPEN || state == DOOR_DESTROYED ? COLOR_BROWN : COLOR_LIGHT_BROWN;
    if (square & DOOR_PASSAGE_NORTH_SOUTH) fill_clipped(target, area, left, top + middle, floor, bar, door_color);
    else fill_clipped(target, area, left + middle, top, bar, floor, door_color);
}

/* The party as a triangle in its square, the tip toward its direction. */
static void draw_party_arrow(Bitmap *target, Box area, int16_t left, int16_t top, int16_t dir)
{
    const int16_t center = AUTOMAP_CELL_SIZE / 2;
    for (int16_t step = 0; step <= center; step++) {
        int16_t tip = 1 + step, near = center - step, far = center + step; /* row of the triangle, its ends */
        switch (dir) {
        case DIRECTION_NORTH: fill_clipped(target, area, left + near, top + tip, far - near + 1, 1, COLOR_YELLOW); break;
        case DIRECTION_SOUTH: fill_clipped(target, area, left + near, top + AUTOMAP_CELL_SIZE - 1 - tip, far - near + 1, 1, COLOR_YELLOW); break;
        case DIRECTION_WEST: fill_clipped(target, area, left + tip, top + near, 1, far - near + 1, COLOR_YELLOW); break;
        case DIRECTION_EAST: fill_clipped(target, area, left + AUTOMAP_CELL_SIZE - 1 - tip, top + near, 1, far - near + 1, COLOR_YELLOW); break;
        }
    }
}

void automap_draw(Bitmap *target, Box area, int16_t map)
{
    fill_clipped(target, area, area.x, area.y, area.w, area.h, COLOR_BLACK);
    if (map < 0 || map >= dungeon_map_count()) return;
    int party_here = map == g_party_map_index;
    int16_t center_x = party_here ? g_party_map_x : dungeon_map_width(map) / 2;
    int16_t center_y = party_here ? g_party_map_y : dungeon_map_height(map) / 2;
    int16_t origin_x = area.x + (area.w - AUTOMAP_CELL_SIZE) / 2 - center_x * AUTOMAP_CELL_SIZE;
    int16_t origin_y = area.y + (area.h - AUTOMAP_CELL_SIZE) / 2 - center_y * AUTOMAP_CELL_SIZE;
    for (int16_t x = 0; x < dungeon_map_width(map); x++)
        for (int16_t y = 0; y < dungeon_map_height(map); y++)
            if (automap_is_seen(map, x, y))
                draw_square(target, area, dungeon_square(map, x, y), origin_x + x * AUTOMAP_CELL_SIZE, origin_y + y * AUTOMAP_CELL_SIZE);
    if (party_here)
        draw_party_arrow(target, area, origin_x + center_x * AUTOMAP_CELL_SIZE, origin_y + center_y * AUTOMAP_CELL_SIZE, g_party_direction);
}
