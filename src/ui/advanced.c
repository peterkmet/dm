/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced.h"
#include "graphics.h"
#include "ids.h"
#include "champion.h"
#include "dungeon.h"
#include "game.h"
#include "map.h"
#include "message.h"
#include "party.h"
#include "record.h"
#include "savegame.h"
#include "automap.h"
#include "command.h"
#include "frame.h"
#include "gameloop.h"
#include "optmenu.h"
#include "startend.h"
#include "text.h"
#include "ui.h"
#include <stdio.h>

/* The automap read-out and the quick save and load messages. */
#define READOUT_TEXT_SIZE   40
#define READOUT_MARGIN      2 /* pixels between the rulers and the read-out */
#define QUICK_MESSAGE_COLOR COLOR_WHITE

bool g_advanced_features;
int16_t g_advanced_screen = ADVANCED_SCREEN_NONE;

static struct {
    bool active;
    int16_t x, y; /* the mouse position that matches the current scroll */
} drag;

static const char *const direction_names[DIRECTION_COUNT] = {"NORTH", "EAST", "SOUTH", "WEST"};

/* The party walks the dungeon: no inventory, dialog, freeze, sleep, candidate, death, sequence, or won game. */
static bool in_dungeon_view(void)
{
    return g_dungeon_maps && g_game_screen_shown && g_game_time_ticking && !g_game_frozen && !g_session.party_dead &&
           g_session.inventory_champion_index < 0 && !g_session.party_sleeping &&
           g_session.candidate_champion_index < 0 && !g_frame_wait_depth && !g_session.won;
}

/* Level, position, facing, and light in the top-left corner of the map, inside the rulers. */
static void show_readout(Bitmap *viewport)
{
    char line[READOUT_TEXT_SIZE];
    int16_t x = AUTOMAP_RULER_WIDTH + READOUT_MARGIN;
    int16_t y = AUTOMAP_RULER_HEIGHT + READOUT_MARGIN + FONT_LINE_HEIGHT - FONT_BASELINE_GAP;
    snprintf(line, sizeof line, "LEVEL %u X %d Y %d", rec_map_level(g_party_map_index), g_party_map_x, g_party_map_y);
    text_draw(viewport, (Point){x, y}, COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "%s LIGHT %d", direction_names[NORMALIZE(g_party_direction)], party_get_light());
    text_draw(viewport, (Point){x, y + FONT_LINE_HEIGHT + FONT_BASELINE_GAP}, COLOR_WHITE, COLOR_BLACK, line);
}

/* Draws the automap and the position read-out (level, position, facing, light) in place of the viewport with the
   interface palette. */
static void show_automap(void)
{
    Bitmap *viewport = ui_get_viewport();
    Box area = {0, 0, viewport->w, viewport->h};
    automap_draw(viewport, &area, g_party_map_index);
    show_readout(viewport);
    ui_draw_viewport();
}

/* Opens a viewport screen: the game time stops until it closes. */
static void open_screen(int16_t screen)
{
    g_advanced_screen = screen;
}

/* Closes the open viewport screen and draws the dungeon view again. */
static void close_screen(void)
{
    g_advanced_screen = ADVANCED_SCREEN_NONE;
    drag.active = false;
    automap_scroll_reset();
    gameloop_draw_view();
}

void advanced_drag(int16_t x, int16_t y)
{
    if (!drag.active || g_advanced_screen != ADVANCED_SCREEN_AUTOMAP)
        return;
    int16_t dx = (x - drag.x) / AUTOMAP_CELL_SIZE;
    int16_t dy = (y - drag.y) / AUTOMAP_CELL_SIZE;
    if (!dx && !dy)
        return;
    drag.x += dx * AUTOMAP_CELL_SIZE;
    drag.y += dy * AUTOMAP_CELL_SIZE;
    automap_scroll(g_party_map_index, -dx, -dy);
    show_automap();
}

bool advanced_process_click(int16_t x, int16_t y, int16_t kind)
{
    if (g_advanced_screen == ADVANCED_SCREEN_NONE)
        return false;
    if (g_advanced_screen == ADVANCED_SCREEN_OPTIONS) {
        if (kind == CLICK_LEFT_PRESS && optmenu_process_click(x, y))
            close_screen();
        return true;
    }
    Box b;
    if (kind == CLICK_LEFT_RELEASE) {
        advanced_drag(x, y);
        drag.active = false;
    } else if (kind == CLICK_LEFT_PRESS && ui_zone_box(ZONE_VIEWPORT, &b) && box_contains(&b, x, y)) {
        drag.active = true;
        drag.x = x;
        drag.y = y;
    }
    return true;
}

void advanced_print_message(const char *text)
{
    message_new_line();
    message_print(QUICK_MESSAGE_COLOR, text);
}

/* A quick save is not a saved game: RESTART and the QUIT question look only at the save file. */
static void quick_save(void)
{
    advanced_print_message(savegame_quick_save(startend_save_dir()) ? "GAME SAVED" : "UNABLE TO SAVE");
}

/* After a damaged file, savegame_load_from puts the running game back: the game goes on. */
static void quick_load(void)
{
    SavegameResult result = savegame_quick_load(startend_save_dir());
    if (result == SAVEGAME_LOADED)
        gameloop_continue_loaded_game();
    advanced_print_message(result == SAVEGAME_LOADED    ? "GAME LOADED"
                           : result == SAVEGAME_MISSING ? "NO QUICK SAVE"
                                                        : "QUICK SAVE DAMAGED");
}

bool advanced_process_key(uint16_t key)
{
    if (g_session.mode == GAME_MODE_VANILLA || champion_is_renaming())
        return false;
    switch (g_advanced_screen) {
    case ADVANCED_SCREEN_AUTOMAP:
        if (key == KEY_M || key == KEY_TAB || key == KEY_ESCAPE)
            close_screen();
        return true;
    case ADVANCED_SCREEN_OPTIONS:
        if (optmenu_process_key(key))
            close_screen();
        return true;
    }
    switch (key) {
    case KEY_F5:
    case KEY_F9:
        if (!in_dungeon_view())
            return true;
        if (key == KEY_F5)
            quick_save();
        else
            quick_load();
        return true;
    case KEY_M:
    case KEY_TAB:
        if (!in_dungeon_view())
            return !(key == KEY_TAB && g_session.inventory_champion_index >= 0);
        open_screen(ADVANCED_SCREEN_AUTOMAP);
        show_automap();
        return true;
    case KEY_ESCAPE:
        if (g_session.mode != GAME_MODE_ADVANCED || !in_dungeon_view())
            return false; /* the inventory closes, or the game freezes or unfreezes */
        open_screen(ADVANCED_SCREEN_OPTIONS);
        optmenu_open();
        return true;
    }
    return false;
}

uint16_t advanced_letter_key(int letter)
{
    if (g_session.mode == GAME_MODE_VANILLA)
        return 0;
    switch (letter) {
    case 'w':
        return KEY_UP;
    case 's':
        return KEY_DOWN;
    case 'a':
        return KEY_LEFT;
    case 'd':
        return KEY_RIGHT;
    case 'q':
        return KEY_DEL;
    case 'e':
        return KEY_HELP;
    }
    return 0;
}
