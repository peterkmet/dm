#include "automap.h"
#include "advanced.h"
#include <stdio.h>
#include "dungeon.h"
#include "explored.h"
#include "game.h"
#include "group.h"
#include "map.h"
#include "text.h"
#include "ui.h"
#include "view.h"

enum { LABEL_WIDTH = 11, LABEL_BASELINE = 4 }; /* two digits; the font cell starts 4 rows above the baseline */
enum { LABEL_TEXT_SIZE = 7 };                  /* the longest int16_t, "-32768", and the end byte */
static int16_t scroll_x, scroll_y;             /* squares between the party and the center of the drawn map */

static bool is_on_map(int16_t map, int16_t x, int16_t y)
{
    return map >= 0 && map < dungeon_map_count() && x >= 0 && x < dungeon_map_width(map) && y >= 0 &&
           y < dungeon_map_height(map);
}

/* A square the view cannot look through: a wall, a fake wall that looks like a wall, a closed door. */
static bool blocks_sight(int16_t map, int16_t x, int16_t y)
{
    if (!is_on_map(map, x, y))
        return true;
    uint8_t square = dungeon_get_square(map, x, y);
    switch (SQUARE_TYPE(square)) {
    case SQUARE_WALL:
        return true;
    case SQUARE_FAKE_WALL:
        return !(square & FAKE_WALL_OPEN);
    case SQUARE_DOOR:
        return door_state(square) == DOOR_CLOSED;
    default:
        return false;
    }
}

#define DARKEST_PALETTE 5 /* g_dungeon_view_palette_index of the darkest light level */

/* Rows the party can make out in the current light: the darkest palettes hide the far rows. */
static int16_t light_depths(void)
{
    /* ponytail: fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it */
    if (g_cheat_light)
        return VIEW_DEPTH_COUNT;
    if (g_dungeon_view_palette_index >= DARKEST_PALETTE)
        return VIEW_DEPTH_COUNT - 2;
    if (g_dungeon_view_palette_index >= DARKEST_PALETTE - 1)
        return VIEW_DEPTH_COUNT - 1;
    return VIEW_DEPTH_COUNT;
}

int16_t automap_get_visible_depths(int16_t map, int16_t x, int16_t y, int16_t dir)
{
    int16_t limit = light_depths();
    for (int16_t depth = 1; depth < limit; depth++) {
        int16_t ahead_x = x;
        int16_t ahead_y = y;
        game_update_map_coordinates_after_relative_movement(dir, depth, 0, &ahead_x, &ahead_y);
        if (blocks_sight(map, ahead_x, ahead_y))
            return depth + 1;
    }
    return limit;
}

/* A side square is seen when the square ahead at its depth or the side square one row nearer is open. */
bool automap_side_is_visible(int16_t map, int16_t x, int16_t y, int16_t dir, int16_t depth, int16_t side)
{
    if (depth == 0 || side == 0)
        return true;
    int16_t ahead_x = x;
    int16_t ahead_y = y;
    int16_t near_x = x;
    int16_t near_y = y;
    game_update_map_coordinates_after_relative_movement(dir, depth, 0, &ahead_x, &ahead_y);
    game_update_map_coordinates_after_relative_movement(dir, depth - 1, side, &near_x, &near_y);
    return !blocks_sight(map, ahead_x, ahead_y) || !blocks_sight(map, near_x, near_y);
}

void automap_mark_seen(int16_t map, int16_t x, int16_t y, int16_t dir)
{
    int16_t depths = automap_get_visible_depths(map, x, y, dir);
    for (int16_t depth = 0; depth < depths; depth++)
        for (int16_t side = -1; side <= 1; side++) {
            if (!automap_side_is_visible(map, x, y, dir, depth, side))
                continue;
            int16_t square_x = x;
            int16_t square_y = y;
            game_update_map_coordinates_after_relative_movement(dir, depth, side, &square_x, &square_y);
            explored_mark(map, square_x, square_y);
        }
}

/* Fills the part of the box (x, y, w, h) that lies inside area. */
static void fill_clipped(Bitmap *target, Box area, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    int16_t left = max16(x, area.x);
    int16_t top = max16(y, area.y);
    int16_t right = min16(x + w, area.x + area.w);
    int16_t bottom = min16(y + h, area.y + area.h);
    if (left >= right || top >= bottom)
        return;
    Box box = {left, top, right - left, bottom - top};
    draw_fill(target, &box, color);
}

/* One seen square at screen position (left, top): a light floor with a 1 pixel grid gap, or a full dark wall.
   pad: a pressure pad that the view draws, as a gray square in the middle of the floor. */
static void draw_square(Bitmap *target, Box area, uint8_t square, bool pad, int16_t left, int16_t top)
{
    const int16_t size = AUTOMAP_CELL_SIZE;
    const int16_t floor = AUTOMAP_CELL_SIZE - 1;
    const int16_t bar = AUTOMAP_CELL_SIZE / 3;
    const int16_t middle = (AUTOMAP_CELL_SIZE - AUTOMAP_CELL_SIZE / 3) / 2;
    int type = SQUARE_TYPE(square);
    if (type == SQUARE_WALL || (type == SQUARE_FAKE_WALL && !(square & FAKE_WALL_OPEN))) {
        fill_clipped(target, area, left, top, size, size, COLOR_DARKEST_GRAY);
        return;
    }
    uint8_t color = COLOR_LIGHT_GRAY;
    if (type == SQUARE_STAIRS)
        color = square & STAIRS_UP ? COLOR_WHITE : COLOR_GOLD;
    else if (type == SQUARE_PIT && (square & PIT_OPEN))
        color = COLOR_RED;
    else if (type == SQUARE_TELEPORTER && (square & TELEPORTER_OPEN) && (square & TELEPORTER_VISIBLE))
        color = COLOR_CYAN;
    fill_clipped(target, area, left, top, floor, floor, color);
    if (pad)
        fill_clipped(target, area, left + bar, top + bar, floor - 2 * bar, floor - 2 * bar, COLOR_GRAY);
    if (type != SQUARE_DOOR)
        return;
    int state = door_state(square);
    uint8_t door_color = state == DOOR_OPEN || state == DOOR_DESTROYED ? COLOR_BROWN : COLOR_LIGHT_BROWN;
    if (square & DOOR_NORTH_SOUTH)
        fill_clipped(target, area, left, top + middle, floor, bar, door_color);
    else
        fill_clipped(target, area, left + middle, top, bar, floor, door_color);
}

/* The party as a triangle in its square, the tip toward its direction. */
static void draw_party_arrow(Bitmap *target, Box area, int16_t left, int16_t top, int16_t dir)
{
    const int16_t center = AUTOMAP_CELL_SIZE / 2;
    for (int16_t step = 0; step <= center; step++) {
        /* row of the triangle, its ends */
        int16_t tip = 1 + step;
        int16_t near = center - step;
        int16_t far = center + step;
        switch (dir) {
        case DIRECTION_NORTH:
            fill_clipped(target, area, left + near, top + tip, far - near + 1, 1, COLOR_YELLOW);
            break;
        case DIRECTION_SOUTH:
            fill_clipped(target, area, left + near, top + AUTOMAP_CELL_SIZE - 1 - tip, far - near + 1, 1, COLOR_YELLOW);
            break;
        case DIRECTION_WEST:
            fill_clipped(target, area, left + tip, top + near, 1, far - near + 1, COLOR_YELLOW);
            break;
        case DIRECTION_EAST:
            fill_clipped(target, area, left + AUTOMAP_CELL_SIZE - 1 - tip, top + near, 1, far - near + 1, COLOR_YELLOW);
            break;
        }
    }
}

/* The square at the center of the drawn map. */
static void draw_center(int16_t map, int16_t *center_x, int16_t *center_y)
{
    bool party_here = map == g_party_map_index;
    *center_x = (party_here ? g_party_map_x : dungeon_map_width(map) / 2) + scroll_x;
    *center_y = (party_here ? g_party_map_y : dungeon_map_height(map) / 2) + scroll_y;
}

void automap_scroll(int16_t map, int16_t dx, int16_t dy)
{
    if (map < 0 || map >= dungeon_map_count())
        return;
    int16_t center_x, center_y;
    draw_center(map, &center_x, &center_y);
    scroll_x += max16(0, min16(dungeon_map_width(map) - 1, center_x + dx)) - center_x;
    scroll_y += max16(0, min16(dungeon_map_height(map) - 1, center_y + dy)) - center_y;
}

void automap_scroll_reset(void)
{
    scroll_x = 0;
    scroll_y = 0;
}

/* The number n at (x, y), only when the whole number is inside area. */
static void draw_label(Bitmap *target, Box area, int16_t x, int16_t y, int16_t n)
{
    char text[LABEL_TEXT_SIZE];
    if (x < area.x || x + LABEL_WIDTH > area.x + area.w || y < area.y || y + TEXT_LINE_HEIGHT > area.y + area.h)
        return;
    snprintf(text, sizeof text, "%02d", n);
    text_draw(target, x, y + LABEL_BASELINE, COLOR_LIGHT_GRAY, COLOR_BLACK, text);
}

void automap_draw(Bitmap *target, Box area, int16_t map)
{
    fill_clipped(target, area, area.x, area.y, area.w, area.h, COLOR_BLACK);
    if (map < 0 || map >= dungeon_map_count())
        return;
    Box top = {area.x + AUTOMAP_RULER_WIDTH, area.y, area.w - AUTOMAP_RULER_WIDTH, AUTOMAP_RULER_HEIGHT};
    Box left = {area.x, area.y + AUTOMAP_RULER_HEIGHT, AUTOMAP_RULER_WIDTH, area.h - AUTOMAP_RULER_HEIGHT};
    Box maps = {top.x, left.y, top.w, left.h};
    bool party_here = map == g_party_map_index;
    int16_t center_x;
    int16_t center_y;
    int16_t map_width = dungeon_map_width(map);
    int16_t map_height = dungeon_map_height(map);
    draw_center(map, &center_x, &center_y);
    int16_t origin_x = maps.x + (maps.w - AUTOMAP_CELL_SIZE) / 2 - center_x * AUTOMAP_CELL_SIZE;
    int16_t origin_y = maps.y + (maps.h - AUTOMAP_CELL_SIZE) / 2 - center_y * AUTOMAP_CELL_SIZE;
    for (int16_t x = 0; x < map_width; x++)
        for (int16_t y = 0; y < map_height; y++)
            if (explored_is_seen(map, x, y))
                draw_square(target, maps, dungeon_get_square(map, x, y), party_here && view_is_pressure_pad(x, y),
                            origin_x + x * AUTOMAP_CELL_SIZE, origin_y + y * AUTOMAP_CELL_SIZE);
    /* The frame lies in the grid gap around the outer squares. */
    int16_t right = origin_x + map_width * AUTOMAP_CELL_SIZE - 1;
    int16_t bottom = origin_y + map_height * AUTOMAP_CELL_SIZE - 1;
    fill_clipped(target, maps, origin_x - 1, origin_y - 1, right - origin_x + 2, 1, COLOR_WHITE);
    fill_clipped(target, maps, origin_x - 1, bottom, right - origin_x + 2, 1, COLOR_WHITE);
    fill_clipped(target, maps, origin_x - 1, origin_y - 1, 1, bottom - origin_y + 2, COLOR_WHITE);
    fill_clipped(target, maps, right, origin_y - 1, 1, bottom - origin_y + 2, COLOR_WHITE);
    for (int16_t x = 0; x < map_width; x += 2)
        draw_label(target, top, origin_x + x * AUTOMAP_CELL_SIZE + (AUTOMAP_CELL_SIZE - LABEL_WIDTH) / 2, top.y, x);
    for (int16_t y = 0; y < map_height; y++)
        draw_label(target, left, left.x, origin_y + y * AUTOMAP_CELL_SIZE, y);
    if (party_here)
        draw_party_arrow(target, maps, origin_x + g_party_map_x * AUTOMAP_CELL_SIZE,
                         origin_y + g_party_map_y * AUTOMAP_CELL_SIZE, g_party_direction);
}
