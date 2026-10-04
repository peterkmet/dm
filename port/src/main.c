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
#include "command.h"
#include "gameloop.h"
#include "random.h"
#include "text.h"

#define DUNGEON_PALETTE 0x100aa6 /* g_dungeon_palettes[0], full light */
#define STEP_EAST 0x101038       /* g_direction_to_step_east[4] */
#define STEP_NORTH 0x101040      /* g_direction_to_step_north[4] */
#define RESUME_TEXT_X 8          /* "no saved game" message on the entrance */
#define RESUME_TEXT_Y 192
#define RESUME_TEXT_COLOR 15     /* white in the entrance palette */

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

/* The Amiga key word of an SDL key: raw key code in the high byte, ASCII in the low byte. Keypad
   keys move as the cursor keys; 7 and 9 turn (Del and Help). Returns 0 for other keys. */
static uint16_t amiga_key(int key, int modifiers)
{
    enum { RAW_Q = 0x10, RAW_S = 0x21, CTRL_ASCII = 0x80, CTRL_MASK = 0x3f };
    if (modifiers & SDL_KMOD_CTRL) {
        if (key == SDLK_Q) return RAW_Q << 8 | CTRL_ASCII | ('Q' & CTRL_MASK);
        if (key == SDLK_S) return RAW_S << 8 | CTRL_ASCII | ('S' & CTRL_MASK);
        return 0;
    }
    switch (key) {
    case SDLK_UP: case SDLK_KP_8: return KEY_UP;
    case SDLK_DOWN: case SDLK_KP_2: case SDLK_KP_5: return KEY_DOWN;
    case SDLK_LEFT: case SDLK_KP_4: return KEY_LEFT;
    case SDLK_RIGHT: case SDLK_KP_6: return KEY_RIGHT;
    case SDLK_DELETE: case SDLK_KP_7: return KEY_DEL;
    case SDLK_HELP: case SDLK_INSERT: case SDLK_KP_9: return KEY_HELP;
    case SDLK_RETURN: case SDLK_KP_ENTER: return KEY_RETURN;
    case SDLK_ESCAPE: return KEY_ESCAPE;
    case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4: return (uint16_t)(KEY_F1 + ((key - SDLK_F1) << 8));
    }
    return 0;
}

/* Mouse buttons as click kinds: left and right, pressed or released. */
static int16_t click_kind(const PlatEvent *e)
{
    int left = e->button == SDL_BUTTON_LEFT;
    if (e->type == PLAT_MOUSE) return left ? CLICK_LEFT_PRESS : CLICK_RIGHT_PRESS;
    return left ? CLICK_LEFT_RELEASE : CLICK_RIGHT_RELEASE;
}

static void set_pointer(int in_game)
{
    uint8_t px[POINTER_SIZE * POINTER_SIZE];
    int x, y, w, h, hot_x, hot_y;
    plat_mouse_position(&x, &y);
    if (gameloop_pointer((int16_t)x, (int16_t)y, in_game, px, &w, &h, &hot_x, &hot_y))
        plat_set_pointer(px, w, h, hot_x, hot_y, gameloop_pointer_palette());
    else
        plat_set_pointer(NULL, 0, 0, 0, 0, NULL);
}

static int filter_factor(const char *name)
{
    if (!strcmp(name, "scale2x")) return 2;
    if (!strcmp(name, "scale3x")) return 3;
    return 1;
}

enum { MODE_SHOW, MODE_VIEW, MODE_ENTRANCE, MODE_GAME };

int main(int argc, char **argv)
{
    long frames = -1, graphic = -1;
    const char *data = "game", *filter = NULL;
    int view = 0, auto_enter = SDL_getenv("DM_AUTO_ENTER") != NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--view")) view = 1;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--data")) data = argv[i + 1];
        if (!strcmp(argv[i], "--show")) graphic = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--filter")) filter = argv[i + 1];
    }

    random_init();
    for (int i = 0; i < 16; i++) /* gray ramp, 0x0iii */
        palette[i] = i << 8 | i << 4 | i;
    int mode = graphic >= 0 ? MODE_SHOW : view ? MODE_VIEW : MODE_ENTRANCE;
    if (mode == MODE_SHOW && !show(data, graphic)) return 1;
    if (mode == MODE_VIEW && !view_start(data)) return 1;
    if (mode == MODE_ENTRANCE) {
        if (!gameloop_load_data(data)) return SDL_Log("cannot load the game files in %s", data), 1;
        entrance_draw();
        command_set_entrance_input();
    }
    if (!plat_init(3)) return 1;
    if (filter && !plat_set_filter(filter_factor(filter))) return 1;
    if (auto_enter) command_process_key(KEY_RETURN);

    for (long frame = 0; frames < 0 || frame < frames; frame++) {
        PlatEvent e;
        while (plat_poll(&e)) {
            if (e.type == PLAT_QUIT) goto done;
            if (mode < MODE_ENTRANCE) {
                if (e.type == PLAT_KEY && e.key == SDLK_ESCAPE) goto done;
                if (mode == MODE_VIEW && e.type == PLAT_KEY) view_key(e.key);
                continue;
            }
            uint16_t key = e.type == PLAT_KEY ? amiga_key(e.key, e.modifiers) : 0;
            if (key) command_process_key(key);
            if (e.type == PLAT_MOUSE || e.type == PLAT_MOUSE_UP) command_process_click((int16_t)e.x, (int16_t)e.y, click_kind(&e));
        }
        int tick = 0;
        if (mode == MODE_ENTRANCE) {
            while (command_process_queue())
                ;
            if (g_new_game == ENTRANCE_QUIT) goto done;
            if (g_new_game == ENTRANCE_RESUME) {
                text_draw_screen(RESUME_TEXT_X, RESUME_TEXT_Y, RESUME_TEXT_COLOR, 0, "NO SAVED GAME YET");
                g_new_game = ENTRANCE_WAITING;
            }
            if (g_new_game == ENTRANCE_NEW_GAME) {
                if (!gameloop_new_game(data)) return SDL_Log("cannot load %s/Dungeon.DAT", data), 1;
                mode = MODE_GAME;
            }
        } else if (mode == MODE_GAME) {
            tick = gameloop_frame();
        }
        if (mode >= MODE_ENTRANCE) set_pointer(mode == MODE_GAME);
        plat_present();
        if (tick) gameloop_tick();
        plat_wait_tick();
    }
done:
    plat_quit();
    return 0;
}
