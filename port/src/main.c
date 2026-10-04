#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "advanced.h"
#include "control.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "layout.h"
#include "map.h"
#include "platform.h"
#include "view.h"
#include "command.h"
#include "gameloop.h"
#include "startend.h"
#include "random.h"
#include "text.h"

#define DUNGEON_PALETTE 0x100aa6 /* g_dungeon_palettes[0], full light */
#define PATH_SIZE 1024
#define WINDOW_SCALE 3
#define GRAY_STEP 0x111          /* one step of the gray ramp: equal red, green, and blue */
#define DEFAULT_CONTROL_SOCKET ".dm/control.sock" /* in the home directory */

/* Loads KAOS.FTL and Graphics.DAT from dir. Returns 0 with a log line when a file is missing. */
static int load_kaos_and_graphics(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/KAOS.FTL", dir);
    if (kaos_load(path)) return SDL_Log("cannot load %s", path), 0;
    snprintf(path, sizeof path, "%s/Graphics.DAT", dir);
    if (gfx_open(path) < 0) return SDL_Log("cannot load %s", path), 0;
    return 1;
}

/* Draws graphic n at the top-left with the dungeon palette. */
static int show(const char *dir, int n)
{
    Bitmap bm;
    if (!load_kaos_and_graphics(dir)) return 0;
    if (gfx_load_bitmap(n, &bm)) return SDL_Log("cannot decode graphic %d", n), 0;
    for (int i = 0; i < PALETTE_COLORS; i++) palette[i] = kaos_u16(DUNGEON_PALETTE + 2 * i);
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
    char path[PATH_SIZE];
    if (!load_kaos_and_graphics(dir)) return 0;
    if (layout_init()) return SDL_Log("cannot load %s/Graphics.DAT", dir), 0;
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
    else if (key == SDLK_LEFT) d = NORMALIZE(d - 1);
    else if (key == SDLK_RIGHT) d = NORMALIZE(d + 1);
    else if (key == SDLK_A) right = -1;
    else if (key == SDLK_D) right = 1;
    else return;
    update_map_coordinates_after_relative_movement(d, forward, right, &x, &y);
    if (x < 0 || x >= dungeon_map_width(m) || y < 0 || y >= dungeon_map_height(m)) return;
    uint8_t square = dungeon_square(m, x, y);
    if (SQUARE_TYPE(square) == SQUARE_WALL || (SQUARE_TYPE(square) == SQUARE_FAKE_WALL && !(square & FAKE_WALL_OPEN))) return;
    g_party_map_x = x;
    g_party_map_y = y;
    g_party_direction = d;
    view_render(m, x, y, d);
}

/* The Amiga key word of an SDL key: raw key code in the high byte, ASCII in the low byte. Keypad
   keys move as the cursor keys; 7 and 9 turn (Del and Help). Outside vanilla, M, Tab, F5, and F6
   are the advanced keys. Returns 0 for other keys. */
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
    if (g_game_mode == GAME_MODE_VANILLA) return 0;
    switch (key) {
    case SDLK_M: return KEY_M;
    case SDLK_TAB: return KEY_TAB;
    case SDLK_F5: return KEY_F5;
    case SDLK_F6: return KEY_F6;
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

/* The pointer image of the mouse position: in the game it depends on the zone and the hand. */
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

enum { MODE_SHOW, MODE_VIEW, MODE_ENTRANCE, MODE_GAME };

static int mode;

/* Keys and mouse buttons become commands; closing the window quits. In turn-based mode the turns
   are the only game input. */
static void read_input(void)
{
    PlatEvent e;
    while (plat_poll(&e)) {
        if (e.type == PLAT_QUIT) g_quit_requested = 1;
        if (g_game_mode == GAME_MODE_TURNBASED && mode >= MODE_ENTRANCE) continue;
        if (mode < MODE_ENTRANCE) {
            if (e.type == PLAT_KEY && e.key == SDLK_ESCAPE) g_quit_requested = 1;
            if (mode == MODE_VIEW && e.type == PLAT_KEY) view_key(e.key);
            continue;
        }
        uint16_t key = e.type == PLAT_KEY ? amiga_key(e.key, e.modifiers) : 0;
        if (key) command_process_key(key);
        if (e.type == PLAT_MOUSE || e.type == PLAT_MOUSE_UP) command_process_click((int16_t)e.x, (int16_t)e.y, click_kind(&e));
    }
}

/* One frame of a dialog, the endgame, or the fuse sequence. */
static void frame_hook(void)
{
    read_input();
    set_pointer(0);
    plat_present();
    plat_wait_tick();
}

/* The entrance again, after a cancelled or failed resume. */
static void show_entrance(void)
{
    entrance_draw();
    command_set_entrance_input();
    command_discard_all_input();
    g_new_game = ENTRANCE_WAITING;
}

/* Runs the entrance commands; RESUME loads the saved game, ENTER starts a new game. Returns 0
   when the dungeon cannot be loaded. */
static int entrance_frame(const char *data)
{
    while (command_process_queue())
        ;
    if (g_quit_requested) return 1;
    if (g_new_game == ENTRANCE_RESUME) {
        if (load_saved_game()) {
            gameloop_start_loaded_game();
            mode = MODE_GAME;
        } else {
            show_entrance();
        }
    }
    if (g_new_game == ENTRANCE_NEW_GAME) {
        if (!gameloop_new_game(data)) return SDL_Log("cannot load %s/Dungeon.DAT", data), 0;
        mode = MODE_GAME;
    }
    return 1;
}

/* The game mode of a --mode value, or -1. */
static int game_mode_of(const char *name)
{
    static const char *const names[] = { "vanilla", "advanced", "turnbased" };
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(name, names[i])) return i;
    return -1;
}

int main(int argc, char **argv)
{
    long frames = -1, graphic = -1;
    const char *data = "game", *filter = NULL, *control = NULL;
    char default_control[PATH_SIZE];
    int view = 0, auto_enter = SDL_getenv("DM_AUTO_ENTER") != NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--view")) view = 1;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--data")) data = argv[i + 1];
        if (!strcmp(argv[i], "--show")) graphic = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--filter")) filter = argv[i + 1];
        if (!strcmp(argv[i], "--control")) control = argv[i + 1];
        if (!strcmp(argv[i], "--mode") && (g_game_mode = (int16_t)game_mode_of(argv[i + 1])) < 0)
            return SDL_Log("--mode must be vanilla, advanced, or turnbased"), 1;
    }
    g_advanced_features = g_game_mode != GAME_MODE_VANILLA;
    int turnbased = g_game_mode == GAME_MODE_TURNBASED;
    if (turnbased) auto_enter = 1;

    random_init();
    for (int i = 0; i < PALETTE_COLORS; i++) /* gray ramp */
        palette[i] = (uint16_t)(i * GRAY_STEP);
    mode = graphic >= 0 ? MODE_SHOW : view ? MODE_VIEW : MODE_ENTRANCE;
    if (mode == MODE_SHOW && !show(data, graphic)) return 1;
    if (mode == MODE_VIEW && !view_start(data)) return 1;
    if (mode == MODE_ENTRANCE) {
        if (!gameloop_load_data(data)) return SDL_Log("cannot load the game files in %s", data), 1;
        entrance_draw();
        command_set_entrance_input();
    }
    if (!plat_init(WINDOW_SCALE)) return 1;
    if (filter && !plat_set_filter(plat_filter_factor(filter))) return 1;
    if (auto_enter) command_process_key(KEY_RETURN);

    if (turnbased) {
        if (!control) {
            const char *home = SDL_getenv("HOME");
            snprintf(default_control, sizeof default_control, "%s/%s", home ? home : ".", DEFAULT_CONTROL_SOCKET);
            control = default_control;
        }
        if (!control_open(control)) return SDL_Log("cannot open the control socket %s", control), 1;
    }
    /* turn-based: no frames inside the endgame and dialogs, so a dead party ends the turn at once */
    g_frame_hook = turnbased ? NULL : frame_hook;
    for (long frame = 0; frames < 0 || frame < frames; frame++) {
        read_input();
        if (g_quit_requested) break;
        control_poll();
        int tick = 0;
        if (mode == MODE_ENTRANCE) {
            if (!entrance_frame(data)) return 1;
        } else if (mode == MODE_GAME && (!turnbased || control_is_turn_pending())) {
            tick = gameloop_frame();
            if (g_return_to_entrance) tick = 0;
        }
        if (g_quit_requested) break;
        if (mode >= MODE_ENTRANCE) set_pointer(mode == MODE_GAME);
        plat_present();
        if (tick) gameloop_tick();
        if (g_quit_requested) break;
        if (g_return_to_entrance) {
            g_return_to_entrance = 0;
            mode = MODE_ENTRANCE;
            show_entrance();
        }
        plat_wait_tick();
    }

    control_close();
    plat_quit();
    return 0;
}
