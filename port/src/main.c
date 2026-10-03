#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "dungeon.h"
#include "ftl.h"
#include "graphics.h"
#include "layout.h"
#include "platform.h"
#include "view.h"

#define DUNGEON_PALETTE 0x100aa6 /* g_dungeon_palettes[0], full light */
#define STEP_EAST 0x101038       /* g_direction_to_step_east[4] */
#define STEP_NORTH 0x101040      /* g_direction_to_step_north[4] */

/* Draws graphic n at the top-left with the dungeon palette. */
static int show(const char *dir, int n)
{
    char path[1024];
    Bitmap bm;
    snprintf(path, sizeof path, "%s/KAOS.FTL", dir);
    if (kaos_load(path)) return SDL_Log("cannot load %s", path), 0;
    snprintf(path, sizeof path, "%s/Graphics.DAT", dir);
    if (gfx_open(path) < 0) return SDL_Log("cannot load %s", path), 0;
    if (gfx_load_bitmap(n, &bm)) return SDL_Log("cannot decode graphic %d", n), 0;
    for (int i = 0; i < 16; i++) palette[i] = kaos_u16(DUNGEON_PALETTE + 2 * i);
    memset(fb, 0, sizeof fb);
    for (int y = 0; y < bm.h && y < SCREEN_H; y++)
        for (int x = 0; x < bm.w && x < SCREEN_W; x++)
            fb[y][x] = bm.px[y * bm.w + x];
    gfx_free_bitmap(&bm);
    return 1;
}

/* --view: loads the game files and shows the dungeon view at the party start of a new game. */
static int view_start(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/KAOS.FTL", dir);
    if (kaos_load(path)) return SDL_Log("cannot load %s", path), 0;
    snprintf(path, sizeof path, "%s/Graphics.DAT", dir);
    if (gfx_open(path) < 0 || layout_init()) return SDL_Log("cannot load %s", path), 0;
    snprintf(path, sizeof path, "%s/Dungeon.DAT", dir);
    if (!load_dungeon(path)) return SDL_Log("cannot load %s", path), 0;
    memset(fb, 0, sizeof fb);
    return view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
}

/* Free movement for visual checks: up/down move, left/right turn, A/D strafe. Walls block. */
static void view_key(int key)
{
    int16_t d = g_party_direction, forward = 0, right = 0, x = g_party_map_x, y = g_party_map_y, m = g_party_map_index;
    if (key == SDLK_UP) forward = 1;
    else if (key == SDLK_DOWN) forward = -1;
    else if (key == SDLK_LEFT) d = (d + 3) & 3;
    else if (key == SDLK_RIGHT) d = (d + 1) & 3;
    else if (key == SDLK_A) right = -1;
    else if (key == SDLK_D) right = 1;
    else return;
    x += forward * kaos_s16(STEP_EAST + 2 * d) + right * kaos_s16(STEP_EAST + 2 * ((d + 1) & 3));
    y += forward * kaos_s16(STEP_NORTH + 2 * d) + right * kaos_s16(STEP_NORTH + 2 * ((d + 1) & 3));
    if (x < 0 || x >= dungeon_map_width(m) || y < 0 || y >= dungeon_map_height(m)) return;
    uint8_t s = dungeon_square(m, x, y);
    if (s >> 5 == 0 || (s >> 5 == 6 && !(s & 4))) return;
    g_party_map_x = x;
    g_party_map_y = y;
    g_party_direction = d;
    view_render(m, x, y, d);
}

static void draw(int frame)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            fb[y][x] = x / 20;
    for (int x = 0; x < SCREEN_W; x++)
        fb[frame % SCREEN_H][x] = 15 - x / 20;
}

int main(int argc, char **argv)
{
    long frames = -1, graphic = -1;
    const char *data = "game";
    int view = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--view")) view = 1;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--data")) data = argv[i + 1];
        if (!strcmp(argv[i], "--show")) graphic = strtol(argv[i + 1], NULL, 10);
    }

    for (int i = 0; i < 16; i++) /* gray ramp, 0x0iii */
        palette[i] = i << 8 | i << 4 | i;
    if (graphic >= 0 && !show(data, graphic)) return 1;
    if (view && !view_start(data)) return 1;
    if (!plat_init(3)) return 1;

    for (int frame = 0; frames < 0 || frame < frames; frame++) {
        PlatEvent e;
        while (plat_poll(&e)) {
            if (e.type == PLAT_QUIT || (e.type == PLAT_KEY && e.key == SDLK_ESCAPE)) goto done;
            if (e.type == PLAT_MOUSE) SDL_Log("mouse %d at %d,%d", e.button, e.x, e.y);
            if (view && e.type == PLAT_KEY) view_key(e.key);
        }
        if (graphic < 0 && !view) draw(frame);
        plat_present();
        plat_wait_tick();
    }
done:
    plat_quit();
    return 0;
}
