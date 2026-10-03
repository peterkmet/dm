#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "ftl.h"
#include "graphics.h"
#include "platform.h"

#define DUNGEON_PALETTE 0x100aa6 /* g_dungeon_palettes[0], full light */

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
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--data")) data = argv[i + 1];
        if (!strcmp(argv[i], "--show")) graphic = strtol(argv[i + 1], NULL, 10);
    }

    for (int i = 0; i < 16; i++) /* gray ramp, 0x0iii */
        palette[i] = i << 8 | i << 4 | i;
    if (graphic >= 0 && !show(data, graphic)) return 1;
    if (!plat_init(3)) return 1;

    for (int frame = 0; frames < 0 || frame < frames; frame++) {
        PlatEvent e;
        while (plat_poll(&e)) {
            if (e.type == PLAT_QUIT || (e.type == PLAT_KEY && e.key == SDLK_ESCAPE)) goto done;
            if (e.type == PLAT_MOUSE) SDL_Log("mouse %d at %d,%d", e.button, e.x, e.y);
        }
        if (graphic < 0) draw(frame);
        plat_present();
        plat_wait_tick();
    }
done:
    plat_quit();
    return 0;
}
