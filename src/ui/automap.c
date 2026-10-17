/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "automap.h"
#include "arith.h"
#include "dungeon.h"
#include "explored.h"
#include "game.h"
#include "map.h"
#include "party.h"
#include "record.h"
#include "draw.h"
#include "ui.h"
#include "view.h"

static int16_t scroll_x, scroll_y; /* squares between the party and the center of the drawn map */

/* Fills the part of box that lies inside area. */
static void fill_clipped(Bitmap *target, const Box *area, const Box *box, uint8_t color)
{
    int16_t left = max16(box->x, area->x);
    int16_t top = max16(box->y, area->y);
    int16_t right = min16(box->x + box->w, area->x + area->w);
    int16_t bottom = min16(box->y + box->h, area->y + area->h);
    if (left >= right || top >= bottom)
        return;
    Box clipped = {left, top, right - left, bottom - top};
    draw_fill(target, &clipped, color);
}

/* One seen square at screen position at: a light floor with a 1 pixel grid gap, or a full dark wall.
   pad: a pressure pad that the view draws, as a gray square in the middle of the floor. */
static void show_square(Bitmap *target, const Box *area, uint8_t square, bool pad, const Box *at)
{
    int16_t left = at->x;
    int16_t top = at->y;
    const int16_t size = AUTOMAP_CELL_SIZE;
    const int16_t floor = AUTOMAP_CELL_SIZE - 1;
    const int16_t bar = AUTOMAP_CELL_SIZE / 3;
    const int16_t middle = (AUTOMAP_CELL_SIZE - AUTOMAP_CELL_SIZE / 3) / 2;
    int type = rec_square_type(square);
    if (type == SQUARE_WALL || (type == SQUARE_FAKE_WALL && !(square & FAKE_WALL_OPEN))) {
        fill_clipped(target, area, &(Box){left, top, size, size}, COLOR_DARKEST_GRAY);
        return;
    }
    uint8_t color = COLOR_LIGHT_GRAY;
    if (type == SQUARE_STAIRS)
        color = square & STAIRS_UP ? COLOR_WHITE : COLOR_GOLD;
    else if (type == SQUARE_PIT && (square & PIT_OPEN))
        color = COLOR_RED;
    else if (type == SQUARE_TELEPORTER && (square & TELEPORTER_OPEN) && (square & TELEPORTER_VISIBLE))
        color = COLOR_CYAN;
    fill_clipped(target, area, &(Box){left, top, floor, floor}, color);
    if (pad)
        fill_clipped(target, area, &(Box){left + bar, top + bar, floor - 2 * bar, floor - 2 * bar}, COLOR_GRAY);
    if (type != SQUARE_DOOR)
        return;
    int state = rec_door_state(square);
    uint8_t door_color = state == DOOR_OPEN || state == DOOR_DESTROYED ? COLOR_BROWN : COLOR_LIGHT_BROWN;
    if (square & DOOR_NORTH_SOUTH)
        fill_clipped(target, area, &(Box){left, top + middle, floor, bar}, door_color);
    else
        fill_clipped(target, area, &(Box){left + middle, top, bar, floor}, door_color);
}

/* The party as a triangle in its square, the tip toward its direction. */
static void show_party_arrow(Bitmap *target, const Box *area, int16_t left, int16_t top, int16_t dir)
{
    const int16_t center = AUTOMAP_CELL_SIZE / 2;
    for (int16_t step = 0; step <= center; step++) {
        /* row of the triangle, its ends */
        int16_t tip = 1 + step;
        int16_t near = center - step;
        int16_t far = center + step;
        switch (dir) {
        case DIRECTION_NORTH:
            fill_clipped(target, area, &(Box){left + near, top + tip, far - near + 1, 1}, COLOR_YELLOW);
            break;
        case DIRECTION_SOUTH:
            fill_clipped(target, area, &(Box){left + near, top + AUTOMAP_CELL_SIZE - 1 - tip, far - near + 1, 1},
                         COLOR_YELLOW);
            break;
        case DIRECTION_WEST:
            fill_clipped(target, area, &(Box){left + tip, top + near, 1, far - near + 1}, COLOR_YELLOW);
            break;
        case DIRECTION_EAST:
            fill_clipped(target, area, &(Box){left + AUTOMAP_CELL_SIZE - 1 - tip, top + near, 1, far - near + 1},
                         COLOR_YELLOW);
            break;
        }
    }
}

/* The square at the center of the drawn map. */
static void get_map_center(int16_t map, int16_t *center_x, int16_t *center_y)
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
    get_map_center(map, &center_x, &center_y);
    scroll_x += max16(0, min16(dungeon_map_width(map) - 1, center_x + dx)) - center_x;
    scroll_y += max16(0, min16(dungeon_map_height(map) - 1, center_y + dy)) - center_y;
}

void automap_scroll_reset(void)
{
    scroll_x = 0;
    scroll_y = 0;
}

void automap_draw(Bitmap *target, const Box *area, int16_t map)
{
    fill_clipped(target, area, area, COLOR_BLACK);
    if (map < 0 || map >= dungeon_map_count())
        return;
    Box maps = {area->x, area->y + AUTOMAP_HEADER_HEIGHT, area->w, area->h - AUTOMAP_HEADER_HEIGHT};
    bool party_here = map == g_party_map_index;
    int16_t center_x;
    int16_t center_y;
    int16_t map_width = dungeon_map_width(map);
    int16_t map_height = dungeon_map_height(map);
    get_map_center(map, &center_x, &center_y);
    int16_t origin_x = maps.x + (maps.w - AUTOMAP_CELL_SIZE) / 2 - center_x * AUTOMAP_CELL_SIZE;
    int16_t origin_y = maps.y + (maps.h - AUTOMAP_CELL_SIZE) / 2 - center_y * AUTOMAP_CELL_SIZE;
    for (int16_t x = 0; x < map_width; x++)
        for (int16_t y = 0; y < map_height; y++)
            if (g_session.cheat_map || explored_is_seen(map, x, y))
                show_square(target, &maps, rec_square_byte(map, x, y), party_here && view_is_pressure_pad(x, y),
                            &(Box){origin_x + x * AUTOMAP_CELL_SIZE, origin_y + y * AUTOMAP_CELL_SIZE,
                                   AUTOMAP_CELL_SIZE, AUTOMAP_CELL_SIZE});
    /* The frame lies in the grid gap around the outer squares. */
    int16_t right = origin_x + map_width * AUTOMAP_CELL_SIZE - 1;
    int16_t bottom = origin_y + map_height * AUTOMAP_CELL_SIZE - 1;
    fill_clipped(target, &maps, &(Box){origin_x - 1, origin_y - 1, right - origin_x + 2, 1}, COLOR_WHITE);
    fill_clipped(target, &maps, &(Box){origin_x - 1, bottom, right - origin_x + 2, 1}, COLOR_WHITE);
    fill_clipped(target, &maps, &(Box){origin_x - 1, origin_y - 1, 1, bottom - origin_y + 2}, COLOR_WHITE);
    fill_clipped(target, &maps, &(Box){right, origin_y - 1, 1, bottom - origin_y + 2}, COLOR_WHITE);
    if (party_here)
        show_party_arrow(target, &maps, origin_x + g_party_map_x * AUTOMAP_CELL_SIZE,
                         origin_y + g_party_map_y * AUTOMAP_CELL_SIZE, g_party_direction);
}
