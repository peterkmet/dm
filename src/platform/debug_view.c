/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "debug_view.h"
#include "adventure.h"
#include "file.h"
#include "graphics.h"
#include "tables.h"
#include "dungeon.h"
#include "map.h"
#include "party.h"
#include "layout.h"
#include "video.h"
#include "view.h"
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
    switch (keycode) {
    case SDLK_UP:
    case SDLK_KP_8:
    case SDLK_W:
        forward = 1;
        break;
    case SDLK_DOWN:
    case SDLK_KP_2:
    case SDLK_KP_5:
    case SDLK_S:
        forward = -1;
        break;
    case SDLK_LEFT:
    case SDLK_KP_4:
    case SDLK_A:
        right = -1;
        break;
    case SDLK_RIGHT:
    case SDLK_KP_6:
    case SDLK_D:
        right = 1;
        break;
    case SDLK_DELETE:
    case SDLK_KP_7:
    case SDLK_Q:
        direction = direction_left(direction);
        break;
    case SDLK_INSERT:
    case SDLK_HELP:
    case SDLK_KP_9:
    case SDLK_E:
        direction = direction_right(direction);
        break;
    default:
        return;
    }
    map_step(direction, forward, right, &x, &y);
    if (x < 0 || x >= dungeon_map_width(map) || y < 0 || y >= dungeon_map_height(map))
        return;
    g_party_map_x = x;
    g_party_map_y = y;
    g_party_direction = direction;
    view_render(map, x, y, direction);
}
