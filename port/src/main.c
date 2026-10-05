#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "advanced.h"
#include "anim.h"
#include "logo.h"
#include "adventure.h"
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
#include "select.h"
#include "text.h"

#define DUNGEON_PALETTE 0x100aa6 /* g_dungeon_palettes[0], full light */
#define PATH_SIZE 1024
#define WINDOW_SCALE 3
#define GRAY_STEP 0x111          /* one step of the gray ramp: equal red, green, and blue */
#define ICON_GRAPHIC 616          /* the front view of the mummy */
#define ICON_TRANSPARENT_COLOR 4  /* the see-through color of the creature graphics */
#define LOGO_FILE "SWSH.FTL"       /* the FTL logo, before the DM title */
#define TITLE_FILE "TITL.DAT"      /* the DM title animation, before the entrance */
#define DEFAULT_CONTROL_SOCKET ".dm/control.sock" /* in the home directory */

/* Loads the engine module and Graphics.DAT from dir. Returns 0 with a log line when a file is missing. */
static int load_kaos_and_graphics(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/%s", dir, adventure_engine_name());
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
   keys move as the cursor keys; 7 and 9 turn (Del and Help). Outside vanilla, M, Tab, F5, F9, and
   W, A, S, D, Q, E are the advanced keys. Returns 0 for other keys. */
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
    case SDLK_F9: return KEY_F9;
    case SDLK_L: return KEY_L;
    case SDLK_F: return KEY_F;
    case SDLK_I: return KEY_I;
    case SDLK_1: return KEY_1;
    case SDLK_2: return KEY_2;
    case SDLK_3: return KEY_3;
    }
    return advanced_letter_key(key);
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

enum { MODE_SHOW, MODE_VIEW, MODE_SELECT, MODE_ENTRANCE, MODE_GAME };

static int mode, turnbased, auto_enter, title_pending;
static int loaded_adventure = -1; /* the adventure whose game files are loaded */ /* title_pending: the DM title plays before the entrance */
static const char *data, *data_override, *control, *csb_dir;

/* Keys and clicks of the start screens. Ctrl+Q quits. */
static void select_input(const PlatEvent *e)
{
    if (e->type == PLAT_MOUSE && e->button == SDL_BUTTON_LEFT) select_click(e->x, e->y);
    if (e->type != PLAT_KEY) return;
    if (e->key == SDLK_Q && e->modifiers & SDL_KMOD_CTRL) g_quit_requested = 1;
    switch (e->key) {
    case SDLK_UP: case SDLK_KP_8: select_key(SELECT_KEY_UP); break;
    case SDLK_DOWN: case SDLK_KP_2: select_key(SELECT_KEY_DOWN); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: select_key(SELECT_KEY_ENTER); break;
    case SDLK_ESCAPE: select_key(SELECT_KEY_ESCAPE); break;
    }
}

/* Keys and mouse buttons become commands; closing the window quits. In turn-based mode the turns
   are the only game input. */
static void read_input(void)
{
    PlatEvent e;
    while (plat_poll(&e)) {
        if (e.type == PLAT_QUIT) g_quit_requested = 1;
        if (mode == MODE_SELECT) {
            select_input(&e);
            continue;
        }
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

/* The FTL logo, the title animation, then the entrance. In MODE_ENTRANCE the input goes to the commands: a key or a click stops it. */
static void play_title(void)
{
    char path[PATH_SIZE];
    title_pending = 0;
    snprintf(path, sizeof path, "%s/%s", data, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", data, TITLE_FILE);
    anim_play(path);
    show_entrance();
}

/* NEW ADVENTURE of CSB: the FTL logo and the intro of disk B. A key or a click stops the intro. */
static void play_csb_intro(void)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/%s", data, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", data, CSB_INTRO_FILE);
    anim_play(path);
    command_discard_all_input();
}

/* QUIT on the DM entrance or the CSB screen: back to the first start screen. In turn-based mode, or
   when --adventure and --mode skip every start screen, the program ends. */
static void quit_to_start(void)
{
    if (turnbased || !select_restart()) {
        g_quit_requested = 1;
        return;
    }
    mode = MODE_SELECT;
}

/* Runs the entrance commands; RESUME loads the saved game, ENTER starts a new game. Returns 0
   when the dungeon cannot be loaded. */
static int entrance_frame(const char *data)
{
    if (g_quit_requested) return 1;
    while (command_process_queue())
        ;
    if (g_quit_requested) { /* the QUIT button: closing the window and Ctrl+Q end the program before */
        g_quit_requested = 0;
        quit_to_start();
        return 1;
    }
    if (g_new_game == ENTRANCE_RESUME || g_new_game == ENTRANCE_NEW_GAME) entrance_press_button(g_new_game == ENTRANCE_RESUME);
    if (g_new_game == ENTRANCE_RESUME) {
        if (load_saved_game()) {
            gameloop_start_loaded_game();
            mode = MODE_GAME;
        } else {
            show_entrance();
        }
    }
    if (g_new_game == ENTRANCE_NEW_GAME) {
        entrance_open_doors();
        if (!gameloop_new_game(data)) return SDL_Log("cannot load %s/Dungeon.DAT", data), 0;
        mode = MODE_GAME;
    }
    return 1;
}

static const char *const game_mode_names[] = { "vanilla", "advanced", "turnbased" };

/* The game mode of a --mode value, or -1. */
static int game_mode_of(const char *name)
{
    for (int i = 0; i < (int)(sizeof game_mode_names / sizeof game_mode_names[0]); i++)
        if (!strcmp(name, game_mode_names[i])) return i;
    return -1;
}

/* Loads the font of the start screens from Graphics.DAT: the --data folder, else DM, else CSB. */
static int load_font(void)
{
    const char *dirs[] = { data_override, adventure_default_dir(ADVENTURE_DM), adventure_default_dir(ADVENTURE_CSB) };
    char path[PATH_SIZE];
    for (int i = 0; i < (int)(sizeof dirs / sizeof dirs[0]); i++) {
        if (!dirs[i]) continue;
        snprintf(path, sizeof path, "%s/Graphics.DAT", dirs[i]);
        if (gfx_open(path) >= 0 && !text_init()) return 1;
    }
    return SDL_Log("cannot load the font from Graphics.DAT"), 0;
}

/* The window icon from the open Graphics.DAT in the colors of the interface. Without the graphic, SDL keeps its icon. */
static void set_icon(void)
{
    static const uint16_t colors[PALETTE_COLORS] = { 0x000, 0x666, 0x888, 0x620, 0x0cc, 0x840, 0x080, 0x0c0,
                                                     0xf00, 0xfa0, 0xc86, 0xff0, 0x444, 0xaaa, 0x00f, 0xfff };
    const Bitmap *bm = gfx_bitmap(ICON_GRAPHIC);
    if (bm) plat_set_icon(bm->px, bm->w, bm->h, ICON_TRANSPARENT_COLOR, colors);
}

/* Applies the chosen game mode: auto enter, frame hook, and in turn-based mode the control socket. */
static int apply_mode(void)
{
    static char default_control[PATH_SIZE];
    turnbased = g_game_mode == GAME_MODE_TURNBASED;
    if (turnbased) auto_enter = 1;
    /* turn-based: no frames inside the endgame and dialogs, so a dead party ends the turn at once */
    g_frame_hook = turnbased ? NULL : frame_hook;
    if (!turnbased) return 1;
    if (!control) {
        const char *home = SDL_getenv("HOME");
        snprintf(default_control, sizeof default_control, "%s/%s", home ? home : ".", DEFAULT_CONTROL_SOCKET);
        control = default_control;
    }
    if (!control_open(control)) return SDL_Log("cannot open the control socket %s", control), 0;
    return 1;
}

/* The disk A folder of g_adventure: CSB uses csb_dir, DM the --data folder or game/. */
static const char *adventure_dir(void)
{
    if (g_adventure == ADVENTURE_CSB) return csb_dir;
    return data_override ? data_override : adventure_default_dir(ADVENTURE_DM);
}

/* The choice of the CSB screen: NEW ADVENTURE starts the prison, RESUME loads CSBGAME.DAT. The game
   files and the game mode are loaded at the first start only. A failed resume shows the CSB screen
   again; in turn-based mode, load and new_game over the socket start the game. Returns 0 when the
   game files or the control socket cannot be opened. */
static int start_csb(void)
{
    int choice = select_csb_choice();
    if (choice == CSB_QUIT) {
        quit_to_start();
        return 1;
    }
    if (loaded_adventure != ADVENTURE_CSB) {
        SDL_Log("adventure csb, mode %s", game_mode_names[g_game_mode]);
        data = adventure_dir();
        if (!gameloop_load_data(data)) return SDL_Log("cannot load the game files in %s", data), 0;
        loaded_adventure = ADVENTURE_CSB;
    }
    if (!apply_mode()) return 0;
    mode = MODE_GAME; /* the dialogs of RESUME read the game input */
    if (choice == CSB_NEW_ADVENTURE && !auto_enter) play_csb_intro();
    if (choice == CSB_NEW_ADVENTURE) {
        if (!gameloop_new_game(data)) return SDL_Log("cannot load %s/Dungeon.DAT", data), 0;
    } else if (load_saved_game()) {
        gameloop_start_loaded_game();
    } else if (turnbased) {
        entrance_draw();
    } else {
        mode = MODE_SELECT;
        select_start_csb(NULL);
    }
    return 1;
}

/* Starts the selected adventure. DM: the entrance. CSB: the choice of the CSB screen. Returns 0 when
   the game files or the control socket cannot be opened. */
static int start_adventure(void)
{
    if (g_adventure == ADVENTURE_CSB) return start_csb();
    SDL_Log("adventure dm, mode %s", game_mode_names[g_game_mode]);
    data = adventure_dir();
    if (!gameloop_load_data(data)) return SDL_Log("cannot load the game files in %s", data), 0;
    loaded_adventure = ADVENTURE_DM;
    if (!apply_mode()) return 0;
    entrance_draw();
    command_set_entrance_input();
    mode = MODE_ENTRANCE;
    title_pending = !auto_enter;
    if (auto_enter) command_process_key(KEY_RETURN);
    /* turn-based: the game starts before the first socket request, so an early load cannot keep the entrance */
    if (turnbased && !entrance_frame(data)) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    long frames = -1, graphic = -1;
    const char *filter = NULL;
    int view = 0, adventure = -1, mode_arg = -1;
    auto_enter = SDL_getenv("DM_AUTO_ENTER") != NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--view")) view = 1;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--data")) data_override = argv[i + 1];
        if (!strcmp(argv[i], "--show")) graphic = strtol(argv[i + 1], NULL, 10);
        if (!strcmp(argv[i], "--filter")) filter = argv[i + 1];
        if (!strcmp(argv[i], "--control")) control = argv[i + 1];
        if (!strcmp(argv[i], "--mode") && (mode_arg = game_mode_of(argv[i + 1])) < 0)
            return SDL_Log("--mode must be vanilla, advanced, or turnbased"), 1;
        if (!strcmp(argv[i], "--adventure") && (adventure = adventure_of(argv[i + 1])) < 0)
            return SDL_Log("--adventure must be dm or csb"), 1;
    }
    /* DM_AUTO_ENTER, --show, and --view show no start screen: the mode is vanilla unless --mode says otherwise */
    if ((auto_enter || view || graphic >= 0) && mode_arg < 0) mode_arg = GAME_MODE_VANILLA;
    /* --data is the CSB folder only with --adventure csb */
    csb_dir = data_override && adventure == ADVENTURE_CSB ? data_override : adventure_default_dir(ADVENTURE_CSB);
    int csb_present = adventure_csb_present(csb_dir);
    if (adventure == ADVENTURE_CSB && !csb_present) return SDL_Log("CSB data is missing in %s", csb_dir), 1;
    select_start((int16_t)adventure, (int16_t)mode_arg, csb_present);
    data = adventure_dir();

    random_init();
    for (int i = 0; i < PALETTE_COLORS; i++) /* gray ramp */
        palette[i] = (uint16_t)(i * GRAY_STEP);
    mode = graphic >= 0 ? MODE_SHOW : view ? MODE_VIEW : MODE_SELECT;
    if (mode == MODE_SHOW && !show(data, graphic)) return 1;
    if (mode == MODE_VIEW && !view_start(data)) return 1;
    if (mode == MODE_SELECT && select_screen() != SCREEN_DONE && !load_font()) return 1;
    if (mode == MODE_SELECT && select_screen() == SCREEN_DONE && !start_adventure()) return 1;
    if (mode < MODE_SELECT && !apply_mode()) return 1;
    if (!plat_init(WINDOW_SCALE)) return 1;
    set_icon();
    if (filter && !plat_set_filter(plat_filter_factor(filter))) return 1;
    /* turn-based and DM_AUTO_ENTER choose NEW ADVENTURE, as ENTER at the DM entrance */
    if (mode == MODE_SELECT && select_screen() == SCREEN_CSB && (auto_enter || g_game_mode == GAME_MODE_TURNBASED))
        select_key(SELECT_KEY_ENTER);

    for (long frame = 0; frames < 0 || frame < frames; frame++) {
        read_input();
        if (g_quit_requested) break;
        control_poll();
        int tick = 0;
        if (mode == MODE_SELECT) {
            if (select_screen() == SCREEN_DONE && !start_adventure()) return 1;
            if (mode == MODE_SELECT) select_draw();
        }
        if (mode == MODE_ENTRANCE) {
            if (title_pending) play_title();
            if (!entrance_frame(data)) return 1;
        } else if (mode == MODE_GAME && (!turnbased || control_is_turn_pending())) {
            tick = gameloop_frame();
            if (g_return_to_entrance) tick = 0;
        }
        if (g_quit_requested) break;
        if (mode >= MODE_ENTRANCE || loaded_adventure >= 0) set_pointer(mode == MODE_GAME); /* the arrow on the start screens after QUIT */
        plat_present();
        if (tick) gameloop_tick();
        if (g_quit_requested) break;
        if (g_return_to_entrance) {
            g_return_to_entrance = 0;
            if (turnbased) {
                entrance_draw(); /* the game mode stays: load and new_game over the socket start the next game */
            } else if (g_adventure == ADVENTURE_CSB) {
                mode = MODE_SELECT;
                select_start_csb(NULL);
            } else {
                mode = MODE_ENTRANCE;
                show_entrance();
            }
        }
        plat_wait_tick();
    }

    control_close();
    plat_quit();
    return 0;
}
