/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/advanced.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/party.h"
#include "engine/record.h"
#include "engine/savegame.h"
#include "ui/automap.h"
#include "ui/command.h"
#include "ui/frame.h"
#include "ui/gameloop.h"
#include "ui/music.h"
#include "ui/startend.h"
#include "ui/text.h"
#include "ui/ui.h"
#include <stdio.h>

/* The automap read-out and the quick save and load messages. */
#define READOUT_TEXT_SIZE   40
#define READOUT_MARGIN      2 /* pixels between the rulers and the read-out */
#define QUICK_MESSAGE_COLOR COLOR_WHITE

bool g_advanced_features, g_automap_open;

static struct {
    bool active;
    int16_t x, y; /* the mouse position that matches the current scroll */
} drag;

static const char *const direction_names[DIRECTION_COUNT] = {"NORTH", "EAST", "SOUTH", "WEST"};
static const char *const soundtrack_names[] = {"ORIGINAL", "SUPER NES", "FM-TOWNS"};

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
    text_draw(viewport, x, y, COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "%s LIGHT %d", direction_names[NORMALIZE(g_party_direction)], party_get_light());
    text_draw(viewport, x, y + FONT_LINE_HEIGHT + FONT_BASELINE_GAP, COLOR_WHITE, COLOR_BLACK, line);
    if (g_session.mode != GAME_MODE_ADVANCED)
        return;
    snprintf(line, sizeof line, "L LIGHT %s I IMMORTAL %s", g_session.cheat_light ? "ON" : "OFF",
             g_session.cheat_immortal ? "ON" : "OFF");
    text_draw(viewport, x, y + 2 * (FONT_LINE_HEIGHT + FONT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "F FOOD AND WATER %s", g_session.cheat_food ? "ON" : "OFF");
    text_draw(viewport, x, y + 3 * (FONT_LINE_HEIGHT + FONT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "1 2 3 MUSIC %s", soundtrack_names[g_music_soundtrack]);
    text_draw(viewport, x, y + 4 * (FONT_LINE_HEIGHT + FONT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
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

/* Closes the automap and draws the dungeon view again. */
static void close_automap(void)
{
    g_automap_open = false;
    drag.active = false;
    automap_scroll_reset();
    gameloop_draw_view();
}

void advanced_drag(int16_t x, int16_t y)
{
    if (!drag.active || !g_automap_open)
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
    if (!g_automap_open)
        return false;
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
    if (g_session.mode == GAME_MODE_VANILLA)
        return false;
    if (g_automap_open) {
        if (key == KEY_M || key == KEY_TAB || key == KEY_ESCAPE)
            close_automap();
        if (g_session.mode == GAME_MODE_ADVANCED && (key == KEY_L || key == KEY_I || key == KEY_F)) {
            game_toggle_cheat(key == KEY_L ? CHEAT_LIGHT : key == KEY_I ? CHEAT_IMMORTAL : CHEAT_FOOD);
            show_automap();
        }
        if (g_session.mode == GAME_MODE_ADVANCED && (key == KEY_1 || key == KEY_2 || key == KEY_3)) {
            music_select(key == KEY_1 ? MUSIC_ORIGINAL : key == KEY_2 ? MUSIC_SUPER_NES : MUSIC_FM_TOWNS);
            show_automap();
        }
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
        g_automap_open = true;
        show_automap();
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
