/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/debug_view.h"
#include "data/adventure.h"
#include "data/file.h"
#include "data/graphics.h"
#include "data/tables.h"
#include "engine/dungeon.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/record.h"
#include "ui/layout.h"
#include "ui/video.h"
#include "ui/view.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

/* Draws graphic n at the top-left with the dungeon palette. */
static bool show_graphic(int16_t n)
{
    Bitmap bm;
    if (!gfx_load_bitmap(n, &bm)) {
        SDL_Log("cannot decode graphic %d", n);
        return false;
    }
    tables_get_palette(TABLES_PALETTE_DUNGEON, g_screen.palette);
    memset(g_screen.px, 0, sizeof g_screen.px);
    for (int y = 0; y < bm.h && y < SCREEN_H; y++)
        for (int x = 0; x < bm.w && x < SCREEN_W; x++)
            g_screen.px[y][x] = bm.px[y * bm.w + x];
    gfx_free_bitmap(&bm);
    return true;
}

/* Loads the dungeon and shows the view at the party start of a new game. */
static bool show_view(const char *dir)
{
    char path[PATH_SIZE];
    if (!layout_init()) {
        SDL_Log("cannot load %s/Graphics.DAT", dir);
        return false;
    }
    snprintf(path, sizeof path, "%s/Dungeon.DAT", dir);
    if (!dungeon_load(path)) {
        SDL_Log("cannot load %s", path);
        return false;
    }
    memset(g_screen.px, 0, sizeof g_screen.px);
    return view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
}

bool debug_view_open(const char *dir, int16_t graphic)
{
    const char *failed;
    if (!adventure_load_files(dir, &failed)) {
        SDL_Log("cannot load %s/%s", dir, failed);
        return false;
    }
    return graphic >= 0 ? show_graphic(graphic) : show_view(dir);
}

void debug_view_key(int keycode)
{
    int16_t direction = g_party_direction;
    int16_t forward = 0;
    int16_t right = 0;
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    int16_t map = g_party_map_index;
    if (keycode == SDLK_UP)
        forward = 1;
    else if (keycode == SDLK_DOWN)
        forward = -1;
    else if (keycode == SDLK_LEFT)
        direction = direction_left(direction);
    else if (keycode == SDLK_RIGHT)
        direction = direction_right(direction);
    else if (keycode == SDLK_A)
        right = -1;
    else if (keycode == SDLK_D)
        right = 1;
    else
        return;
    map_step(direction, forward, right, &x, &y);
    if (x < 0 || x >= dungeon_map_width(map) || y < 0 || y >= dungeon_map_height(map))
        return;
    uint8_t square = rec_square_byte(map, x, y);
    if (rec_square_type(square) == SQUARE_WALL ||
        (rec_square_type(square) == SQUARE_FAKE_WALL && !(square & FAKE_WALL_OPEN)))
        return;
    g_party_map_x = x;
    g_party_map_y = y;
    g_party_direction = direction;
    view_render(map, x, y, direction);
}
