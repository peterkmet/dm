#include <stdio.h>
#include "ui/advanced.h"
#include "ui/automap.h"
#include "engine/champion.h"
#include "ui/command.h"
#include "ui/dialog.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "ui/gameloop.h"
#include "data/ids.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "ui/music.h"
#include "engine/savegame.h"
#include "ui/startend.h"
#include "ui/text.h"
#include "engine/timeline_process.h"
#include "ui/ui.h"

#define READOUT_TEXT_SIZE   40
#define READOUT_MARGIN      2 /* pixels between the rulers and the read-out */
#define QUICK_MESSAGE_COLOR COLOR_WHITE

bool g_advanced_features, g_automap_open;

static struct {
    bool active;
    int16_t x, y; /* the mouse position that matches the current scroll */
} drag;

static const char *const direction_names[4] = {"NORTH", "EAST", "SOUTH", "WEST"};
static const char *const soundtrack_names[3] = {"ORIGINAL", "SUPER NES", "FM-TOWNS"};

/* The party walks the dungeon: no inventory, dialog, freeze, sleep, candidate, death, sequence, or won game. */
static bool in_dungeon_view(void)
{
    return g_dungeon_maps && !g_game_screen_inactive && g_game_time_ticking && !g_game_frozen && !g_party_dead &&
           !g_inventory_champion_ordinal && !g_party_is_sleeping && !g_candidate_champion_ordinal && !g_waiting_frame &&
           !g_game_won;
}

/* Level, position, facing, and light in the top-left corner of the map, inside the rulers. */
static void draw_readout(Bitmap *viewport)
{
    char line[READOUT_TEXT_SIZE];
    int16_t x = AUTOMAP_RULER_WIDTH + READOUT_MARGIN;
    int16_t y = AUTOMAP_RULER_HEIGHT + READOUT_MARGIN + TEXT_LINE_HEIGHT - TEXT_BASELINE_GAP;
    snprintf(line, sizeof line, "LEVEL %u X %d Y %d", map_level(g_party_map_index), g_party_map_x, g_party_map_y);
    text_draw(viewport, x, y, COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "%s LIGHT %d", direction_names[NORMALIZE(g_party_direction)],
             inventory_get_party_light());
    text_draw(viewport, x, y + TEXT_LINE_HEIGHT + TEXT_BASELINE_GAP, COLOR_WHITE, COLOR_BLACK, line);
    if (g_game_mode != GAME_MODE_ADVANCED)
        return;
    snprintf(line, sizeof line, "L LIGHT %s I IMMORTAL %s", g_cheat_light ? "ON" : "OFF",
             g_cheat_immortal ? "ON" : "OFF");
    text_draw(viewport, x, y + 2 * (TEXT_LINE_HEIGHT + TEXT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "F FOOD AND WATER %s", g_cheat_food ? "ON" : "OFF");
    text_draw(viewport, x, y + 3 * (TEXT_LINE_HEIGHT + TEXT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "1 2 3 MUSIC %s", soundtrack_names[g_music_soundtrack]);
    text_draw(viewport, x, y + 4 * (TEXT_LINE_HEIGHT + TEXT_BASELINE_GAP), COLOR_WHITE, COLOR_BLACK, line);
}

void advanced_draw_automap(void)
{
    Bitmap *viewport = ui_get_viewport();
    Box area = {0, 0, viewport->w, viewport->h};
    automap_draw(viewport, area, g_party_map_index);
    draw_readout(viewport);
    ui_draw_viewport();
}

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
    advanced_draw_automap();
}

bool advanced_process_click(int16_t x, int16_t y, int16_t kind)
{
    if (!g_automap_open)
        return false;
    Box b;
    if (kind == CLICK_LEFT_RELEASE) {
        advanced_drag(x, y);
        drag.active = false;
    } else if (kind == CLICK_LEFT_PRESS && ui_zone_box(ZONE_VIEWPORT, &b) && x >= b.x && x < b.x + b.w && y >= b.y &&
               y < b.y + b.h) {
        drag.active = true;
        drag.x = x;
        drag.y = y;
    }
    return true;
}

/* A white line in the message area. */
void advanced_print_message(const char *text)
{
    message_new_line(g_game_time);
    message_print(QUICK_MESSAGE_COLOR, text, g_game_time);
}

/* A quick save is not a saved game: RESTART and the QUIT question look only at the save file. */
static void quick_save(void)
{
    advanced_print_message(savegame_quick_save(startend_save_dir()) ? "GAME SAVED" : "UNABLE TO SAVE");
}

/* After a damaged file, savegame_load_from puts the running game back: the game goes on. */
static void quick_load(void)
{
    int16_t result = savegame_quick_load(startend_save_dir());
    if (result == SAVEGAME_LOADED)
        gameloop_continue_loaded_game();
    advanced_print_message(result == SAVEGAME_LOADED    ? "GAME LOADED"
                           : result == SAVEGAME_MISSING ? "NO QUICK SAVE"
                                                        : "QUICK SAVE DAMAGED");
}

bool advanced_process_key(uint16_t key)
{
    if (g_game_mode == GAME_MODE_VANILLA)
        return false;
    if (g_automap_open) {
        if (key == KEY_M || key == KEY_TAB || key == KEY_ESCAPE)
            close_automap();
        if (g_game_mode == GAME_MODE_ADVANCED && (key == KEY_L || key == KEY_I || key == KEY_F)) {
            game_toggle_cheat(key == KEY_L ? CHEAT_LIGHT : key == KEY_I ? CHEAT_IMMORTAL : CHEAT_FOOD);
            advanced_draw_automap();
        }
        if (g_game_mode == GAME_MODE_ADVANCED && (key == KEY_1 || key == KEY_2 || key == KEY_3)) {
            music_select(key == KEY_1 ? MUSIC_ORIGINAL : key == KEY_2 ? MUSIC_SUPER_NES : MUSIC_FM_TOWNS);
            advanced_draw_automap();
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
            return !(key == KEY_TAB && g_inventory_champion_ordinal);
        g_automap_open = true;
        advanced_draw_automap();
        return true;
    }
    return false;
}

uint16_t advanced_letter_key(int letter)
{
    if (g_game_mode == GAME_MODE_VANILLA)
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
