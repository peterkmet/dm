/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/file.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/os.h"
#include "engine/game.h"
#include "engine/random.h"
#include "ui/advanced.h"
#include "ui/anim.h"
#include "ui/command.h"
#include "ui/entrance.h"
#include "ui/frame.h"
#include "ui/gameloop.h"
#include "ui/logo.h"
#include "ui/music.h"
#include "ui/select.h"
#include "ui/startend.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "control/control.h"
#include "platform/debug_view.h"
#include "platform/options.h"
#include "platform/platform.h"
#include "platform/transport.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_SCALE           3
#define GRAY_STEP              0x111              /* one step of the gray ramp: equal red, green, and blue */
#define ICON_TRANSPARENT_COLOR 4                  /* the see-through color of the creature graphics */
#define LOGO_FILE              "SWSH.FTL"         /* the FTL logo, before the DM title */
#define TITLE_FILE             "TITL.DAT"         /* the DM title animation, before the entrance */
#define DISK_IMAGE_EXTENSION   ".adf"             /* the original game disks, extracted at start */
#define DEFAULT_CONTROL_SOCKET ".dm/control.sock" /* in the home directory */

/* The Amiga key word of a key of the advanced modes: M, Tab, F5, F9, L, F, I, 1-3, and the movement letters. */
static uint16_t advanced_key(int key)
{
    switch (key) {
    case SDLK_M:
        return KEY_M;
    case SDLK_TAB:
        return KEY_TAB;
    case SDLK_F5:
        return KEY_F5;
    case SDLK_F9:
        return KEY_F9;
    case SDLK_L:
        return KEY_L;
    case SDLK_F:
        return KEY_F;
    case SDLK_I:
        return KEY_I;
    case SDLK_1:
        return KEY_1;
    case SDLK_2:
        return KEY_2;
    case SDLK_3:
        return KEY_3;
    }
    return advanced_letter_key(key);
}

/* The Amiga key word of an SDL key: raw key code in the high byte, ASCII in the low byte. Keypad
   keys move as the cursor keys; 7 and 9 turn (Del and Help). Outside vanilla, M, Tab, F5, F9, and
   W, A, S, D, Q, E are the advanced keys. Returns 0 for other keys. */
static uint16_t amiga_key(int key, int modifiers)
{
    enum { RAW_Q = 0x10, RAW_S = 0x21, CTRL_ASCII = 0x80, CTRL_MASK = 0x3f };
    if (modifiers & SDL_KMOD_CTRL) {
        if (key == SDLK_Q)
            return command_key(RAW_Q, CTRL_ASCII | ('Q' & CTRL_MASK));
        if (key == SDLK_S)
            return command_key(RAW_S, CTRL_ASCII | ('S' & CTRL_MASK));
        return 0;
    }
    switch (key) {
    case SDLK_UP:
    case SDLK_KP_8:
        return KEY_UP;
    case SDLK_DOWN:
    case SDLK_KP_2:
    case SDLK_KP_5:
        return KEY_DOWN;
    case SDLK_LEFT:
    case SDLK_KP_4:
        return KEY_LEFT;
    case SDLK_RIGHT:
    case SDLK_KP_6:
        return KEY_RIGHT;
    case SDLK_DELETE:
    case SDLK_KP_7:
        return KEY_DEL;
    case SDLK_HELP:
    case SDLK_INSERT:
    case SDLK_KP_9:
        return KEY_HELP;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return KEY_RETURN;
    case SDLK_ESCAPE:
        return KEY_ESCAPE;
    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
        return command_champion_key(key - SDLK_F1);
    }
    if (g_session.mode == GAME_MODE_VANILLA)
        return 0;
    return advanced_key(key);
}

/* Mouse buttons as click kinds: left and right, pressed or released. */
static int16_t click_kind(const PlatEvent *e)
{
    bool left = e->button == SDL_BUTTON_LEFT;
    if (e->type == PLAT_MOUSE)
        return left ? CLICK_LEFT_PRESS : CLICK_RIGHT_PRESS;
    return left ? CLICK_LEFT_RELEASE : CLICK_RIGHT_RELEASE;
}

/* The pointer image of the mouse position: in the game it depends on the zone and the hand. */
static void set_pointer(bool in_game)
{
    PointerImage image;
    int x, y;
    plat_mouse_position(&x, &y);
    if (gameloop_get_pointer(x, y, in_game, &image))
        plat_set_pointer(image.px, image.w, image.h, image.hot_x, image.hot_y, gameloop_get_pointer_palette());
    else
        plat_set_pointer(NULL, 0, 0, 0, 0, NULL);
}

#define MODE_SHOW     0
#define MODE_VIEW     1
#define MODE_SELECT   2
#define MODE_ENTRANCE 3
#define MODE_GAME     4

static int mode;
static bool turnbased, auto_enter, title_pending; /* title_pending: the DM title plays before the entrance */
static int loaded_adventure = -1;                 /* the adventure whose game files are loaded */
static const char *data_dir, *data_override, *saves_dir, *control_socket, *csb_dir;

/* Keys and clicks of the start screens. Ctrl+Q quits. */
static void select_input(const PlatEvent *event)
{
    if (event->type == PLAT_MOUSE && event->button == SDL_BUTTON_LEFT)
        select_click(event->x, event->y);
    if (event->type != PLAT_KEY)
        return;
    if (event->key == SDLK_Q && event->modifiers & SDL_KMOD_CTRL)
        g_quit_requested = true;
    switch (event->key) {
    case SDLK_UP:
    case SDLK_KP_8:
        select_key(SELECT_KEY_UP);
        break;
    case SDLK_DOWN:
    case SDLK_KP_2:
        select_key(SELECT_KEY_DOWN);
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        select_key(SELECT_KEY_ENTER);
        break;
    case SDLK_ESCAPE:
        select_key(SELECT_KEY_ESCAPE);
        break;
    }
}

/* Keys and mouse buttons become commands; closing the window quits. In turn-based mode the turns
   are the only game input. */
static void read_input(void)
{
    PlatEvent event;
    while (plat_poll(&event)) {
        if (event.type == PLAT_QUIT)
            g_quit_requested = true;
        if (mode == MODE_SELECT) {
            select_input(&event);
            continue;
        }
        if (g_session.mode == GAME_MODE_TURNBASED && mode >= MODE_ENTRANCE)
            continue;
        if (mode < MODE_ENTRANCE) {
            if (event.type == PLAT_KEY && event.key == SDLK_ESCAPE)
                g_quit_requested = true;
            if (mode == MODE_VIEW && event.type == PLAT_KEY)
                debug_view_key(event.key);
            continue;
        }
        uint16_t key = event.type == PLAT_KEY ? amiga_key(event.key, event.modifiers) : 0;
        if (key)
            command_process_key(key);
        if (event.type == PLAT_MOUSE || event.type == PLAT_MOUSE_UP)
            command_process_click(event.x, event.y, click_kind(&event));
    }
}

/* One frame of a dialog, the endgame, or the fuse sequence. */
static void run_frame_hook(void)
{
    read_input();
    set_pointer(false);
    plat_present();
    plat_wait_tick();
}

/* The entrance again, after a cancelled or failed resume. */
static void show_entrance(void)
{
    entrance_draw();
    command_set_entrance_input();
    command_discard_all_input();
    g_entrance_choice = ENTRANCE_WAITING;
}

/* The FTL logo, the title animation, then the entrance. In MODE_ENTRANCE the input goes to the commands: a key or a click stops it. */
static void play_title(void)
{
    char path[PATH_SIZE];
    title_pending = false;
    snprintf(path, sizeof path, "%s/%s", data_dir, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", data_dir, TITLE_FILE);
    anim_play(path);
    show_entrance();
}

/* NEW ADVENTURE of CSB: the FTL logo and the intro of disk B. A key or a click stops the intro. */
static void play_csb_intro(void)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/%s", data_dir, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", data_dir, CSB_INTRO_FILE);
    anim_play(path);
    command_discard_all_input();
}

/* QUIT on the DM entrance or the CSB screen: back to the first start screen. In turn-based mode, or
   when --adventure and --mode skip every start screen, the program ends. */
static void quit_to_start(void)
{
    if (turnbased || !select_restart()) {
        g_quit_requested = true;
        return;
    }
    mode = MODE_SELECT;
}

/* Runs the entrance commands; RESUME loads the saved game, ENTER starts a new game. Returns 0
   when the dungeon cannot be loaded. */
static int entrance_frame(const char *folder)
{
    if (g_quit_requested)
        return 1;
    while (command_process_queue())
        ;
    if (g_quit_requested) { /* the QUIT button: closing the window and Ctrl+Q end the program before */
        g_quit_requested = false;
        quit_to_start();
        return 1;
    }
    if (g_entrance_choice == ENTRANCE_RESUME || g_entrance_choice == ENTRANCE_NEW_GAME)
        entrance_press_button(g_entrance_choice == ENTRANCE_RESUME);
    if (g_entrance_choice == ENTRANCE_RESUME) {
        if (startend_load_saved_game()) {
            gameloop_start_loaded_game();
            mode = MODE_GAME;
        } else {
            show_entrance();
        }
    }
    if (g_entrance_choice == ENTRANCE_NEW_GAME) {
        entrance_open_doors();
        if (!gameloop_start_new_game(folder)) {
            SDL_Log("cannot load %s/Dungeon.DAT", folder);
            return 0;
        }
        mode = MODE_GAME;
    }
    return 1;
}

/* The folder hook of data/: makes the folder and its parents. */
static bool make_folder(const char *path)
{
    return SDL_CreateDirectory(path);
}

typedef struct {
    void (*found)(const char *name, void *data);
    void *data;
} FolderVisit;

static SDL_EnumerationResult SDLCALL visit_name(void *userdata, const char *dirname, const char *fname)
{
    (void)dirname;
    const FolderVisit *visit = userdata;
    visit->found(fname, visit->data);
    return SDL_ENUM_CONTINUE;
}

/* The folder hook of data/: calls found for each name in the folder. */
static void list_folder(const char *path, void (*found)(const char *name, void *context), void *context)
{
    FolderVisit visit = {found, context};
    SDL_EnumerateDirectory(path, visit_name, &visit);
}

/* Loads the font of the start screens from Graphics.DAT: the --data folder, else DM, else CSB. */
static int load_font(void)
{
    const char *dirs[] = {data_override, adventure_get(ADVENTURE_DM)->dir, adventure_get(ADVENTURE_CSB)->dir};
    char path[PATH_SIZE];
    for (int i = 0; i < ARRAY_COUNT(dirs); i++) {
        if (!dirs[i])
            continue;
        snprintf(path, sizeof path, "%s/Graphics.DAT", dirs[i]);
        if (gfx_open(path) && text_init())
            return 1;
    }
    SDL_Log("cannot load the font from Graphics.DAT");
    return 0;
}

/* The window icon from the open Graphics.DAT in the colors of the interface. Without the graphic, SDL keeps its icon. */
static void set_icon(void)
{
    static const uint16_t colors[PALETTE_COLORS] = {
        RGB_BLACK, RGB_DARK_GRAY, RGB_GRAY,  RGB_BROWN,  RGB_CYAN,         RGB_LIGHT_BROWN, RGB_DARK_GREEN, RGB_GREEN,
        RGB_RED,   RGB_GOLD,      RGB_FLESH, RGB_YELLOW, RGB_DARKEST_GRAY, RGB_LIGHT_GRAY,  RGB_BLUE,       RGB_WHITE,
    };
    const Bitmap *bm = gfx_get_bitmap(GFX_APP_ICON);
    if (bm)
        plat_set_icon(bm->px, bm->w, bm->h, ICON_TRANSPARENT_COLOR, colors);
}

/* Applies the chosen game mode: auto enter, frame hook, and in turn-based mode the control socket. */
static int apply_mode(void)
{
    static char default_control[PATH_SIZE];
    turnbased = g_session.mode == GAME_MODE_TURNBASED;
    if (turnbased)
        auto_enter = true;
    /* turn-based: no frames inside the endgame and dialogs, so a dead party ends the turn at once */
    g_frame_hook = turnbased ? NULL : run_frame_hook;
    if (!turnbased)
        return 1;
    if (!control_socket) {
        const char *home = SDL_getenv("HOME");
        snprintf(default_control, sizeof default_control, "%s/%s", home ? home : ".", DEFAULT_CONTROL_SOCKET);
        control_socket = default_control;
    }
    if (!transport_open(control_socket)) {
        SDL_Log("cannot open the control socket %s", control_socket);
        return 0;
    }
    return 1;
}

/* The disk A folder of g_adventure: CSB uses csb_dir, DM the --data folder or game/. */
static const char *get_adventure_dir(void)
{
    if (g_adventure == ADVENTURE_CSB)
        return csb_dir;
    return data_override ? data_override : adventure_get(ADVENTURE_DM)->dir;
}

/* Loads the game files of g_adventure from data_dir after the check that they are the Amiga files. Returns 0 with a log line. */
static int load_game_files(void)
{
    const char *bad = adventure_find_bad_file(g_adventure, data_dir);
    if (bad) {
        SDL_Log("%s/%s is missing or is not the file of %s for the Amiga", data_dir, bad,
                adventure_get(g_adventure)->title);
        return 0;
    }
    if (!gameloop_load_data(data_dir)) {
        SDL_Log("cannot load the game files in %s", data_dir);
        return 0;
    }
    return 1;
}

/* Extracts a game disk image (.adf) of the current folder into its game folder, once. */
static SDL_EnumerationResult SDLCALL install_disk(void *userdata, const char *dirname, const char *name)
{
    (void)userdata;
    (void)dirname;
    size_t len = strlen(name);
    size_t ext = strlen(DISK_IMAGE_EXTENSION);
    if (len <= ext || SDL_strcasecmp(name + len - ext, DISK_IMAGE_EXTENSION))
        return SDL_ENUM_CONTINUE;
    const char *dir = NULL;
    AdventureDisk r = adventure_install_disk(name, &dir);
    if (r == ADVENTURE_DISK_INSTALLED)
        SDL_Log("extracted %s into %s", name, dir);
    if (r == ADVENTURE_DISK_FAILED)
        SDL_Log("cannot extract %s into %s", name, dir);
    return SDL_ENUM_CONTINUE;
}

/* The choice of the CSB screen: NEW ADVENTURE starts the prison, RESUME loads CSBGAME.DAT. The game
   files and the game mode are loaded at the first start only. A failed resume shows the CSB screen
   again; in turn-based mode, load and new_game over the socket start the game. Returns false when the
   game files or the control socket cannot be opened. */
static bool start_csb(void)
{
    int choice = select_csb_choice();
    if (choice == SELECT_CSB_QUIT) {
        quit_to_start();
        return true;
    }
    if (loaded_adventure != ADVENTURE_CSB) {
        SDL_Log("adventure csb, mode %s", options_get_mode_name(g_session.mode));
        data_dir = get_adventure_dir();
        if (!load_game_files())
            return false;
        if (saves_dir)
            startend_set_save_dir(saves_dir);
        loaded_adventure = ADVENTURE_CSB;
    }
    if (!apply_mode())
        return false;
    mode = MODE_GAME; /* the dialogs of RESUME read the game input */
    if (choice == SELECT_CSB_NEW_ADVENTURE && !auto_enter)
        play_csb_intro();
    if (choice == SELECT_CSB_NEW_ADVENTURE) {
        if (!gameloop_start_new_game(data_dir)) {
            SDL_Log("cannot load %s/Dungeon.DAT", data_dir);
            return 0;
        }
    } else if (startend_load_saved_game()) {
        gameloop_start_loaded_game();
    } else if (turnbased) {
        entrance_draw();
    } else {
        mode = MODE_SELECT;
        select_start_csb(NULL);
    }
    return true;
}

/* Starts the selected adventure. DM: the entrance. CSB: the choice of the CSB screen. Returns false when
   the game files or the control socket cannot be opened. */
static bool start_adventure(void)
{
    if (g_adventure == ADVENTURE_CSB)
        return start_csb();
    SDL_Log("adventure dm, mode %s", options_get_mode_name(g_session.mode));
    data_dir = get_adventure_dir();
    if (!load_game_files())
        return false;
    if (saves_dir)
        startend_set_save_dir(saves_dir);
    loaded_adventure = ADVENTURE_DM;
    if (!apply_mode())
        return false;
    entrance_draw();
    command_set_entrance_input();
    mode = MODE_ENTRANCE;
    title_pending = !auto_enter;
    if (auto_enter)
        command_process_key(KEY_RETURN);
    /* turn-based: the game starts before the first socket request, so an early load cannot keep the entrance */
    if (turnbased && !entrance_frame(data_dir))
        return false;
    return true;
}

#define RUN_NEXT_FRAME 0
#define RUN_QUIT       1
#define RUN_FAILED     2

/* One frame of the program: the input, the control socket, the start screens or the game, and the picture. */
static int run_frame(void)
{
    read_input();
    if (g_quit_requested)
        return RUN_QUIT;
    transport_poll();
    int tick = 0;
    if (mode == MODE_SELECT) {
        if (select_screen() == SELECT_SCREEN_DONE && !start_adventure())
            return RUN_FAILED;
        if (mode == MODE_SELECT)
            select_draw();
    }
    if (mode == MODE_ENTRANCE) {
        if (title_pending)
            play_title();
        if (!entrance_frame(data_dir))
            return RUN_FAILED;
    } else if (mode == MODE_GAME && (!turnbased || control_is_turn_pending())) {
        tick = gameloop_run_frame();
        if (g_return_to_entrance)
            tick = 0;
    }
    if (g_quit_requested)
        return RUN_QUIT;
    if (mode >= MODE_ENTRANCE || loaded_adventure >= 0)
        set_pointer(mode == MODE_GAME); /* the arrow on the start screens after QUIT */
    if (mode == MODE_GAME) {
        int mouse_x, mouse_y;
        plat_mouse_position(&mouse_x, &mouse_y);
        advanced_drag(mouse_x, mouse_y);
    }
    plat_present();
    if (tick)
        gameloop_tick();
    if (g_quit_requested)
        return RUN_QUIT;
    if (g_return_to_entrance) {
        g_return_to_entrance = false;
        music_stop(); /* the start screens have no music */
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
    return RUN_NEXT_FRAME;
}

/* Reads the command line; --data, --saves, --control go to file statics. False for a wrong --mode or --adventure. */
static bool parse_options(int argc, char **argv, Options *options)
{
    *options = (Options){.frames = -1, .graphic = -1, .filter = NULL, .view = false, .adventure = -1, .mode = -1};
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--view"))
            options->view = true;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--frames"))
            options->frames = atoll(argv[i + 1]);
        if (!strcmp(argv[i], "--data"))
            data_override = argv[i + 1];
        if (!strcmp(argv[i], "--saves"))
            saves_dir = argv[i + 1];
        if (!strcmp(argv[i], "--show"))
            options->graphic = atoll(argv[i + 1]);
        if (!strcmp(argv[i], "--filter"))
            options->filter = argv[i + 1];
        if (!strcmp(argv[i], "--control"))
            control_socket = argv[i + 1];
        if (!strcmp(argv[i], "--mode") && (options->mode = options_find_mode(argv[i + 1])) < 0) {
            SDL_Log("--mode must be vanilla, advanced, or turnbased");
            return false;
        }
        if (options->mode == GAME_MODE_TURNBASED && !transport_is_available()) {
            SDL_Log("turn-based mode needs macOS");
            return false;
        }
        if (!strcmp(argv[i], "--adventure") && (options->adventure = adventure_find(argv[i + 1])) < 0) {
            SDL_Log("--adventure must be dm or csb");
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    Options options;
    auto_enter = SDL_getenv("DM_AUTO_ENTER") != NULL;
    if (!parse_options(argc, argv, &options))
        return 1;
    /* DM_AUTO_ENTER, --show, and --view show no start screen: the mode is vanilla unless --mode says otherwise */
    if ((auto_enter || options.view || options.graphic >= 0) && options.mode < 0)
        options.mode = GAME_MODE_VANILLA;
    g_os_make_folder_hook = make_folder;
    g_os_list_folder_hook = list_folder;
    SDL_EnumerateDirectory(".", install_disk, NULL);
    /* --data is the CSB folder only with --adventure csb */
    csb_dir = data_override && options.adventure == ADVENTURE_CSB ? data_override : adventure_get(ADVENTURE_CSB)->dir;
    bool csb_present = adventure_is_csb_present(csb_dir);
    if (options.adventure == ADVENTURE_CSB && !csb_present) {
        SDL_Log("CSB data is missing in %s", csb_dir);
        return 1;
    }
    select_start(options.adventure, options.mode, csb_present, transport_is_available());
    data_dir = get_adventure_dir();

    random_init();
    for (int i = 0; i < PALETTE_COLORS; i++) /* gray ramp */
        g_screen.palette[i] = i * GRAY_STEP;
    mode = options.graphic >= 0 ? MODE_SHOW : options.view ? MODE_VIEW : MODE_SELECT;
    if (mode == MODE_SHOW && !debug_view_open(data_dir, options.graphic))
        return 1;
    if (mode == MODE_VIEW && !debug_view_open(data_dir, -1))
        return 1;
    if (mode == MODE_SELECT && select_screen() != SELECT_SCREEN_DONE && !load_font())
        return 1;
    if (mode == MODE_SELECT && select_screen() == SELECT_SCREEN_DONE && !start_adventure())
        return 1;
    if (mode < MODE_SELECT && !apply_mode())
        return 1;
    if (!plat_init(WINDOW_SCALE))
        return 1;
    set_icon();
    if (options.filter && !plat_set_filter(plat_filter_factor(options.filter)))
        return 1;
    /* turn-based and DM_AUTO_ENTER choose NEW ADVENTURE, as ENTER at the DM entrance */
    if (mode == MODE_SELECT && select_screen() == SELECT_SCREEN_CSB &&
        (auto_enter || g_session.mode == GAME_MODE_TURNBASED))
        select_key(SELECT_KEY_ENTER);

    for (int64_t frame = 0; options.frames < 0 || frame < options.frames; frame++) {
        int result = run_frame();
        if (result == RUN_FAILED)
            return 1;
        if (result == RUN_QUIT)
            break;
    }

    transport_close();
    plat_quit();
    return 0;
}
