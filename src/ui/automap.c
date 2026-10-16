/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/automap.h"
#include "engine/arith.h"
#include "engine/dungeon.h"
#include "engine/explored.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/record.h"
#include "ui/advanced.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/view.h"
#include <stdio.h>

#define LABEL_WIDTH     11 /* two digits; the font cell starts 4 rows above the baseline */
#define LABEL_BASELINE  4
#define LABEL_TEXT_SIZE 7          /* the longest int16_t, "-32768", and the end byte */
static int16_t scroll_x, scroll_y; /* squares between the party and the center of the drawn map */

/* Fills the part of the box (x, y, w, h) that lies inside area. */
static void fill_clipped(Bitmap *target, const Box *area, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    int16_t left = max16(x, area->x);
    int16_t top = max16(y, area->y);
    int16_t right = min16(x + w, area->x + area->w);
    int16_t bottom = min16(y + h, area->y + area->h);
    if (left >= right || top >= bottom)
        return;
    Box box = {left, top, right - left, bottom - top};
    draw_fill(target, &box, color);
}

/* One seen square at screen position (left, top): a light floor with a 1 pixel grid gap, or a full dark wall.
   pad: a pressure pad that the view draws, as a gray square in the middle of the floor. */
static void show_square(Bitmap *target, const Box *area, uint8_t square, bool pad, int16_t left, int16_t top)
{
    const int16_t size = AUTOMAP_CELL_SIZE;
    const int16_t floor = AUTOMAP_CELL_SIZE - 1;
    const int16_t bar = AUTOMAP_CELL_SIZE / 3;
    const int16_t middle = (AUTOMAP_CELL_SIZE - AUTOMAP_CELL_SIZE / 3) / 2;
    int type = rec_square_type(square);
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
    int state = rec_door_state(square);
    uint8_t door_color = state == DOOR_OPEN || state == DOOR_DESTROYED ? COLOR_BROWN : COLOR_LIGHT_BROWN;
    if (square & DOOR_NORTH_SOUTH)
        fill_clipped(target, area, left, top + middle, floor, bar, door_color);
    else
        fill_clipped(target, area, left + middle, top, bar, floor, door_color);
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

/* The number n at (x, y), only when the whole number is inside area. */
static void show_label(Bitmap *target, const Box *area, int16_t x, int16_t y, int16_t n)
{
    char text[LABEL_TEXT_SIZE];
    if (x < area->x || x + LABEL_WIDTH > area->x + area->w || y < area->y || y + FONT_LINE_HEIGHT > area->y + area->h)
        return;
    snprintf(text, sizeof text, "%02d", n);
    text_draw(target, x, y + LABEL_BASELINE, COLOR_LIGHT_GRAY, COLOR_BLACK, text);
}

void automap_draw(Bitmap *target, const Box *area, int16_t map)
{
    fill_clipped(target, area, area->x, area->y, area->w, area->h, COLOR_BLACK);
    if (map < 0 || map >= dungeon_map_count())
        return;
    Box top = {area->x + AUTOMAP_RULER_WIDTH, area->y, area->w - AUTOMAP_RULER_WIDTH, AUTOMAP_RULER_HEIGHT};
    Box left = {area->x, area->y + AUTOMAP_RULER_HEIGHT, AUTOMAP_RULER_WIDTH, area->h - AUTOMAP_RULER_HEIGHT};
    Box maps = {top.x, left.y, top.w, left.h};
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
            if (explored_is_seen(map, x, y))
                show_square(target, &maps, rec_square_byte(map, x, y), party_here && view_is_pressure_pad(x, y),
                            origin_x + x * AUTOMAP_CELL_SIZE, origin_y + y * AUTOMAP_CELL_SIZE);
    /* The frame lies in the grid gap around the outer squares. */
    int16_t right = origin_x + map_width * AUTOMAP_CELL_SIZE - 1;
    int16_t bottom = origin_y + map_height * AUTOMAP_CELL_SIZE - 1;
    fill_clipped(target, &maps, origin_x - 1, origin_y - 1, right - origin_x + 2, 1, COLOR_WHITE);
    fill_clipped(target, &maps, origin_x - 1, bottom, right - origin_x + 2, 1, COLOR_WHITE);
    fill_clipped(target, &maps, origin_x - 1, origin_y - 1, 1, bottom - origin_y + 2, COLOR_WHITE);
    fill_clipped(target, &maps, right, origin_y - 1, 1, bottom - origin_y + 2, COLOR_WHITE);
    for (int16_t x = 0; x < map_width; x += 2)
        show_label(target, &top, origin_x + x * AUTOMAP_CELL_SIZE + (AUTOMAP_CELL_SIZE - LABEL_WIDTH) / 2, top.y, x);
    for (int16_t y = 0; y < map_height; y++)
        show_label(target, &left, left.x, origin_y + y * AUTOMAP_CELL_SIZE, y);
    if (party_here)
        show_party_arrow(target, &maps, origin_x + g_party_map_x * AUTOMAP_CELL_SIZE,
                         origin_y + g_party_map_y * AUTOMAP_CELL_SIZE, g_party_direction);
}
