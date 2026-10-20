/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app.h"
#include "adventure.h"
#include "bytes.h"
#include "file.h"
#include "graphics.h"
#include "ids.h"
#include "os.h"
#include "tables.h"
#include "champion.h"
#include "game.h"
#include "random.h"
#include "advanced.h"
#include "anim.h"
#include "command.h"
#include "entrance.h"
#include "frame.h"
#include "gameloop.h"
#include "logo.h"
#include "music.h"
#include "optmenu.h"
#include "select.h"
#include "startend.h"
#include "text.h"
#include "ui.h"
#include "video.h"
#include "control.h"
#include "debug_view.h"
#include "options.h"
#include "platform.h"
#include "transport.h"
#include <SDL3/SDL.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The window. */
#define WINDOW_SCALE           3
#define ICON_TRANSPARENT_COLOR 4 /* the see-through color of the creature graphics */

/* The palette before the game files. */
#define GRAY_STEP 0x111 /* one step of the gray ramp: equal red, green, and blue */

/* Files and paths. */
#define LOGO_FILE              "SWSH.FTL"         /* the FTL logo, before the DM title */
#define TITLE_FILE             "TITL.DAT"         /* the DM title animation, before the entrance */
#define DISK_IMAGE_EXTENSION   ".adf"             /* the original game disks, extracted at start */
#define DEFAULT_CONTROL_SOCKET ".dm/control.sock" /* in the home directory */

/* The Ctrl keys: raw key codes and the ASCII control range. */
#define RAW_Q      0x10
#define RAW_S      0x21
#define CTRL_ASCII 0x80
#define CTRL_MASK  0x3f

/* The Amiga key word of a key of the advanced modes: C, M, Tab, F5, F9, and the movement letters. */
static uint16_t get_advanced_key(int key)
{
    switch (key) {
    case SDLK_C:
        return KEY_C;
    case SDLK_M:
        return KEY_M;
    case SDLK_TAB:
        return KEY_TAB;
    case SDLK_F5:
        return KEY_F5;
    case SDLK_F9:
        return KEY_F9;
    }
    return advanced_letter_key(key);
}

/* The Amiga key word of a key of the name entry keyboard: Backspace, or the letter, the space, ',', '.', ';' or ':'
   that the key types. The name entry key table is the only table with these keys and gives their raw key codes. */
static uint16_t get_name_entry_key(int key, int character)
{
    if (key == SDLK_BACKSPACE)
        return KEY_BACKSPACE;
    if (character <= 0 || character > CHAR_MAX)
        return 0;
    return tables_get_char_key(TABLES_KEYS_NAME_ENTRY, (char)character);
}

/* The Amiga key word of an SDL key: raw key code in the high byte, ASCII in the low byte. Keypad
   keys move as the cursor keys; 7 and 9 turn (Del and Help). In vanilla and during the name entry, the letters,
   the space, Backspace and the punctuation are the name entry keys; else C, M, Tab, F5, F9, and W, A, S, D, Q, E
   are the advanced keys. Returns 0 for other keys. */
static uint16_t amiga_key(int key, int modifiers, int character)
{
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
    if (g_session.mode == GAME_MODE_VANILLA || champion_is_renaming())
        return get_name_entry_key(key, character);
    uint16_t advanced = get_advanced_key(key);
    return advanced ? advanced : get_name_entry_key(key, character); /* any key ends the credits */
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
    int16_t x, y;
    plat_mouse_position(&x, &y);
    if (gameloop_get_pointer(x, y, in_game, &image))
        plat_set_pointer(&image, gameloop_get_pointer_palette());
    else
        plat_set_pointer(NULL, NULL);
}

/* What the frame runs. */
#define SCREEN_DEBUG    0 /* --show or --view */
#define SCREEN_SELECT   1 /* the start screens */
#define SCREEN_ENTRANCE 2 /* the DM entrance */
#define SCREEN_GAME     3

typedef struct {
    int16_t screen;                  /* SCREEN_* */
    bool view;                       /* the debug screen is --view (free movement keys) */
    bool turnbased;                  /* the game mode is turn-based: the socket gives the turns */
    bool auto_enter;                 /* DM_AUTO_ENTER or turn-based: NEW ADVENTURE without the title */
    bool title_pending;              /* the DM title plays before the entrance */
    bool pointer_hidden;             /* the entrance fades and credits hide the pointer */
    int16_t loaded_adventure;        /* the adventure whose game files are loaded; -1 none */
    Options options;                 /* the command line */
    const char *data_dir;            /* the disk A folder of the running adventure */
    const char *csb_dir;             /* the CSB folder: --data with --adventure csb, else game-csb/ */
    char default_control[PATH_SIZE]; /* ~/.dm/control.sock when --control is not given */
    FILE *log;                       /* dm.log of the Windows build, else NULL */
} App;

static App app;

/* The startup failures, and the log file of the Windows build. */
#define FAIL_TEXT_SIZE 512
#define LOG_FILE       "dm.log"

/* A startup failure: the message goes to the log and to a message box (the Windows build has no console). Returns
   false for the caller to return. */
static bool fail(const char *format, ...)
{
    char text[FAIL_TEXT_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    SDL_Log("%s", text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dungeon Master", text, NULL);
    return false;
}

/* Keys and clicks of the start screens. Ctrl+Q quits. */
static void read_start_screen_event(const PlatEvent *event)
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
        if (app.screen == SCREEN_SELECT) {
            read_start_screen_event(&event);
            continue;
        }
        if (g_session.mode == GAME_MODE_TURNBASED && app.screen >= SCREEN_ENTRANCE)
            continue;
        if (app.screen == SCREEN_DEBUG) {
            if (event.type == PLAT_KEY && event.key == SDLK_ESCAPE)
                g_quit_requested = true;
            if (app.view && event.type == PLAT_KEY)
                debug_view_key(event.key);
            continue;
        }
        uint16_t key = event.type == PLAT_KEY ? amiga_key(event.key, event.modifiers, event.character) : 0;
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
    if (app.pointer_hidden)
        plat_set_pointer(NULL, NULL);
    else
        set_pointer(false);
    plat_present();
    plat_wait_tick();
}

/* The entrance with its fades; the pointer is hidden until it is drawn, as the original. */
static void redraw_entrance_hidden(void)
{
    app.pointer_hidden = true;
    entrance_draw();
    app.pointer_hidden = false;
}

/* The entrance again, after a cancelled or failed resume. */
static void show_entrance(void)
{
    redraw_entrance_hidden();
    command_set_entrance_input();
    command_discard_all_input();
    g_entrance_choice = ENTRANCE_WAITING;
}

/* The FTL logo, the title animation, then the entrance. In SCREEN_ENTRANCE the input goes to the commands: a key or a click stops it. */
static void play_title(void)
{
    char path[PATH_SIZE];
    app.title_pending = false;
    snprintf(path, sizeof path, "%s/%s", app.data_dir, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", app.data_dir, TITLE_FILE);
    anim_play(path);
    show_entrance();
}

/* NEW ADVENTURE of CSB: the FTL logo and the intro of disk B. A key or a click stops the intro. */
static void play_csb_intro(void)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/%s", app.data_dir, LOGO_FILE);
    logo_play(path);
    snprintf(path, sizeof path, "%s/%s", app.data_dir, CSB_INTRO_FILE);
    anim_set_stop_at_once(g_session.mode == GAME_MODE_ADVANCED);
    anim_play(path);
    anim_set_stop_at_once(false);
    command_discard_all_input();
}

/* QUIT on the DM entrance or the CSB screen: back to the first start screen. In turn-based mode, or
   when --adventure and --mode skip every start screen, the program ends. */
static void quit_to_start(void)
{
    if (app.turnbased || !select_restart()) {
        g_quit_requested = true;
        return;
    }
    app.screen = SCREEN_SELECT;
}

/* Runs the entrance commands; RESUME loads the saved game, ENTER starts a new game. False when
   the dungeon cannot be loaded. */
static bool run_entrance(const char *folder)
{
    if (g_quit_requested)
        return true;
    while (command_process_queue())
        ;
    if (g_quit_requested) { /* the QUIT button: closing the window and Ctrl+Q end the program before */
        g_quit_requested = false;
        quit_to_start();
        return true;
    }
    if (g_entrance_choice == ENTRANCE_CREDITS) {
        app.pointer_hidden = true; /* until the entrance is drawn again, as the original */
        entrance_show_credits();
        show_entrance();
    }
    if (g_entrance_choice == ENTRANCE_RESUME || g_entrance_choice == ENTRANCE_NEW_GAME)
        entrance_press_button(g_entrance_choice == ENTRANCE_RESUME);
    if (g_entrance_choice == ENTRANCE_RESUME) {
        if (startend_load_saved_game()) {
            gameloop_start_loaded_game();
            app.screen = SCREEN_GAME;
        } else {
            show_entrance();
        }
    }
    if (g_entrance_choice == ENTRANCE_NEW_GAME) {
        entrance_open_doors();
        if (!gameloop_start_new_game(folder))
            return fail("cannot load %s/Dungeon.DAT", folder);
        app.screen = SCREEN_GAME;
    }
    return true;
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

/* The SDL folder callback of list_folder: passes one name on. */
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

/* The file time hook of data/: the modification time of the file in local time. */
static bool get_file_time(const char *path, OsDateTime *time)
{
    SDL_PathInfo info;
    SDL_DateTime local;
    if (!SDL_GetPathInfo(path, &info) || !SDL_TimeToDateTime(info.modify_time, &local, true))
        return false;
    *time = (OsDateTime){local.year, local.month, local.day, local.hour, local.minute};
    return true;
}

/* Loads the font of the start screens from Graphics.DAT: the --data folder, else DM, else CSB. */
static bool load_font(void)
{
    const char *dirs[] = {app.options.data_dir, adventure_get(ADVENTURE_DM)->dir, adventure_get(ADVENTURE_CSB)->dir};
    char path[PATH_SIZE];
    for (int i = 0; i < ARRAY_COUNT(dirs); i++) {
        if (!dirs[i])
            continue;
        snprintf(path, sizeof path, "%s/Graphics.DAT", dirs[i]);
        if (gfx_open(path) && text_init())
            return true;
    }
    return fail("cannot load the font from Graphics.DAT");
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
static bool apply_mode(void)
{
    app.turnbased = g_session.mode == GAME_MODE_TURNBASED;
    if (app.turnbased)
        app.auto_enter = true;
    /* turn-based: no frames inside the endgame and dialogs, so a dead party ends the turn at once */
    g_frame_hook = app.turnbased ? NULL : run_frame_hook;
    if (!app.turnbased)
        return true;
    const char *socket = app.options.control_socket;
    if (!socket) {
        const char *home = SDL_getenv("HOME");
        snprintf(app.default_control, sizeof app.default_control, "%s/%s", home ? home : ".", DEFAULT_CONTROL_SOCKET);
        socket = app.default_control;
    }
    if (!transport_open(socket))
        return fail("cannot open the control socket %s", socket);
    return true;
}

/* The disk A folder of g_adventure: CSB uses csb_dir, DM the --data folder or game/. */
static const char *get_adventure_dir(void)
{
    if (g_adventure == ADVENTURE_CSB)
        return app.csb_dir;
    return app.options.data_dir ? app.options.data_dir : adventure_get(ADVENTURE_DM)->dir;
}

/* Loads the game files of g_adventure from data_dir after the check that they are the Amiga files. False with a log line. */
static bool load_game_files(void)
{
    const char *bad = adventure_find_bad_file(g_adventure, app.data_dir);
    if (bad)
        return fail("%s/%s is missing or is not the file of %s for the Amiga", app.data_dir, bad,
                    adventure_get(g_adventure)->title);
    if (!gameloop_load_data(app.data_dir))
        return fail("cannot load the game files in %s", app.data_dir);
    return true;
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
    int16_t choice = select_csb_choice();
    if (choice == SELECT_CSB_QUIT) {
        quit_to_start();
        return true;
    }
    if (app.loaded_adventure != ADVENTURE_CSB) {
        SDL_Log("adventure csb, mode %s", options_get_mode_name(g_session.mode));
        app.data_dir = get_adventure_dir();
        if (!load_game_files())
            return false;
        if (app.options.saves_dir)
            startend_set_save_dir(app.options.saves_dir);
        app.loaded_adventure = ADVENTURE_CSB;
    }
    if (!apply_mode())
        return false;
    app.screen = SCREEN_GAME; /* the dialogs of RESUME read the game input */
    if (choice == SELECT_CSB_NEW_ADVENTURE && !app.auto_enter)
        play_csb_intro();
    if (choice == SELECT_CSB_NEW_ADVENTURE) {
        if (!gameloop_start_new_game(app.data_dir))
            return fail("cannot load %s/Dungeon.DAT", app.data_dir);
    } else if (startend_load_saved_game()) {
        gameloop_start_loaded_game();
    } else if (app.turnbased) {
        redraw_entrance_hidden();
    } else {
        app.screen = SCREEN_SELECT;
        select_start_csb(NULL);
    }
    return true;
}

/* Starts the DM entrance after the start screens. Returns false when the game files or the control socket cannot
   be opened. */
static bool start_dm(void)
{
    SDL_Log("adventure dm, mode %s", options_get_mode_name(g_session.mode));
    app.data_dir = get_adventure_dir();
    if (!load_game_files())
        return false;
    if (app.options.saves_dir)
        startend_set_save_dir(app.options.saves_dir);
    app.loaded_adventure = ADVENTURE_DM;
    if (!apply_mode())
        return false;
    app.screen = SCREEN_ENTRANCE;
    app.title_pending = !app.auto_enter;
    if (!app.title_pending) /* else the title draws the entrance after it */
        redraw_entrance_hidden();
    command_set_entrance_input();
    if (app.auto_enter)
        command_process_key(KEY_RETURN);
    /* turn-based: the game starts before the first socket request, so an early load cannot keep the entrance */
    if (app.turnbased && !run_entrance(app.data_dir))
        return false;
    return true;
}

/* Starts the selected adventure. DM: the entrance. CSB: the choice of the CSB screen. Returns false when
   the game files or the control socket cannot be opened. */
static bool start_adventure(void)
{
    if (g_adventure == ADVENTURE_CSB)
        return start_csb();
    return start_dm();
}

/* After the game: the entrance or the CSB screen again; in turn-based mode the entrance waits for the socket. */
static void return_to_start(void)
{
    g_return_to_entrance = false;
    music_stop(); /* the start screens have no music */
    if (app.turnbased) {
        redraw_entrance_hidden(); /* the game mode stays: load and new_game over the socket start the next game */
    } else if (g_adventure == ADVENTURE_CSB) {
        app.screen = SCREEN_SELECT;
        select_start_csb(NULL);
    } else {
        app.screen = SCREEN_ENTRANCE;
        show_entrance();
    }
}

/* The log hook of data/: one SDL log line. */
static void log_message(const char *message)
{
    SDL_Log("%s", message);
}

/* The fatal error hook of data/: one critical SDL log line; os_fatal then ends the program. */
static void log_fatal(const char *message)
{
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

#ifdef _WIN32
/* The SDL log output of the Windows build: a line of dm.log in the game folder, as the build has no console. */
static void log_to_file(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    (void)category;
    (void)priority;
    fprintf(userdata, "%s\n", message);
    fflush(userdata);
}
#endif

/* Extracts the game disk images (.adf) of the current folder, then finds the CSB folder and starts the start
   screens. False when --adventure csb has no CSB data. */
static bool install_disks(void)
{
    SDL_EnumerateDirectory(".", install_disk, NULL);
    /* --data is the CSB folder only with --adventure csb */
    const char *data = app.options.data_dir;
    app.csb_dir = data && app.options.adventure == ADVENTURE_CSB ? data : adventure_get(ADVENTURE_CSB)->dir;
    bool csb_present = adventure_is_csb_present(app.csb_dir);
    if (app.options.adventure == ADVENTURE_CSB && !csb_present)
        return fail("CSB data is missing in %s", app.csb_dir);
    select_start(app.options.adventure, app.options.mode, csb_present, transport_is_available());
    app.data_dir = get_adventure_dir();
    return true;
}

/* The first screen: --show or --view, else the start screens, or the adventure when the options choose it. */
static bool choose_first_screen(void)
{
    app.view = app.options.graphic < 0 && app.options.view; /* --show wins over --view */
    app.screen = app.options.graphic >= 0 || app.view ? SCREEN_DEBUG : SCREEN_SELECT;
    if (app.screen == SCREEN_DEBUG)
        return debug_view_open(app.data_dir, app.view ? -1 : app.options.graphic) && apply_mode();
    if (select_screen() != SELECT_SCREEN_DONE)
        return load_font();
    return start_adventure();
}

/* Shows the image with the upscale filter of the options menu. */
static bool set_filter(int16_t filter)
{
    return plat_set_filter(filter);
}

/* Opens the window with the icon and the filter. */
static bool open_window(void)
{
    if (!plat_init(WINDOW_SCALE))
        return false;
    set_icon();
    if (app.options.filter) {
        g_filter = (int16_t)plat_filter_factor(app.options.filter);
        g_filter_fixed = true;
        if (!plat_set_filter(g_filter))
            return false;
    }
    g_filter_hook = set_filter;
    /* turn-based and DM_AUTO_ENTER choose NEW ADVENTURE, as ENTER at the DM entrance */
    if (app.screen == SCREEN_SELECT && select_screen() == SELECT_SCREEN_CSB &&
        (app.auto_enter || g_session.mode == GAME_MODE_TURNBASED))
        select_key(SELECT_KEY_ENTER);
    return true;
}

bool app_open(const Options *options)
{
    app.options = *options;
    app.loaded_adventure = -1;
    app.auto_enter = SDL_getenv("DM_AUTO_ENTER") != NULL;
    /* DM_AUTO_ENTER, --show, and --view show no start screen: the mode is vanilla unless --mode says otherwise */
    if ((app.auto_enter || app.options.view || app.options.graphic >= 0) && app.options.mode < 0)
        app.options.mode = GAME_MODE_VANILLA;
#ifdef _WIN32
    if ((app.log = fopen(LOG_FILE, "w")))
        SDL_SetLogOutputFunction(log_to_file, app.log);
#endif
    g_os_log_hook = log_message;
    g_os_fatal_hook = log_fatal;
    g_os_make_folder_hook = make_folder;
    g_os_list_folder_hook = list_folder;
    g_os_file_time_hook = get_file_time;
    if (!install_disks())
        return false;
    random_init();
    for (int i = 0; i < PALETTE_COLORS; i++) /* gray ramp */
        g_screen.palette[i] = i * GRAY_STEP;
    return choose_first_screen() && open_window();
}

void app_close(void)
{
    transport_close();
    gfx_close();
    tables_free();
    if (app.log)
        fclose(app.log);
    plat_quit();
}

AppFrame app_run_frame(void)
{
    read_input();
    if (g_quit_requested)
        return APP_FRAME_QUIT;
    transport_poll();
    bool tick = false;
    if (app.screen == SCREEN_SELECT) {
        if (select_screen() == SELECT_SCREEN_DONE && !start_adventure())
            return APP_FRAME_FAILED;
        if (app.screen == SCREEN_SELECT)
            select_draw();
    }
    if (app.screen == SCREEN_ENTRANCE) {
        if (app.title_pending)
            play_title();
        if (!run_entrance(app.data_dir))
            return APP_FRAME_FAILED;
    } else if (app.screen == SCREEN_GAME && (!app.turnbased || control_is_turn_pending())) {
        tick = gameloop_run_frame();
        if (g_return_to_entrance)
            tick = false;
    }
    if (g_quit_requested)
        return APP_FRAME_QUIT;
    if (app.screen >= SCREEN_ENTRANCE || app.loaded_adventure >= 0)
        set_pointer(app.screen == SCREEN_GAME); /* the arrow on the start screens after QUIT */
    if (app.screen == SCREEN_GAME) {
        int16_t mouse_x, mouse_y;
        plat_mouse_position(&mouse_x, &mouse_y);
        advanced_drag(mouse_x, mouse_y);
    }
    plat_present();
    if (tick)
        gameloop_tick();
    if (g_quit_requested)
        return APP_FRAME_QUIT;
    if (g_return_to_entrance)
        return_to_start();
    plat_wait_tick();
    return APP_FRAME_NEXT;
}
