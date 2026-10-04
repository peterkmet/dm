#include <stdio.h>
#include "advanced.h"
#include "automap.h"
#include "champion.h"
#include "command.h"
#include "dialog.h"
#include "dungeon.h"
#include "game.h"
#include "gameloop.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "text.h"
#include "ui.h"

#define DEBUG_TEXT_SIZE 40
#define DEBUG_MARGIN 2                 /* pixels between the viewport edge and the read-out */

int16_t g_game_mode = GAME_MODE_VANILLA;
int16_t g_advanced_features, g_automap_open, g_debug_readout;

static const char *const direction_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };

/* The party walks the dungeon: no inventory, dialog, freeze, sleep, candidate, or death. */
static int in_dungeon_view(void)
{
    return g_dungeon_maps && !g_game_screen_inactive && g_game_time_ticking && !g_game_frozen && !g_party_dead &&
           !g_inventory_champion_ordinal && !g_party_is_sleeping && !g_candidate_champion_ordinal;
}

void advanced_draw_automap(void)
{
    Bitmap *vp = ui_viewport();
    Box area = { 0, 0, (int16_t)vp->w, (int16_t)vp->h };
    automap_draw(vp, area, g_party_map_index);
    ui_draw_viewport();
}

static void close_automap(void)
{
    g_automap_open = 0;
    gameloop_draw_view();
}

/* The panels that look different with the advanced features: action icons, inventory panel, view. */
static void redraw_after_toggle(void)
{
    if (!g_dungeon_maps || g_game_screen_inactive || g_game_frozen) return;
    menus_draw_action_area();
    if (g_inventory_champion_ordinal) {
        draw_panel();
        ui_draw_viewport();
    } else if (!g_party_is_sleeping) {
        gameloop_draw_view();
    }
}

int advanced_process_key(uint16_t key)
{
    if (g_game_mode == GAME_MODE_VANILLA) return 0;
    if (g_automap_open) {
        if (key == KEY_M || key == KEY_TAB || key == KEY_ESCAPE) close_automap();
        return 1;
    }
    switch (key) {
    case KEY_F6:
        g_advanced_features = !g_advanced_features;
        redraw_after_toggle();
        return 1;
    case KEY_F5:
        if (!g_advanced_features) return 1;
        g_debug_readout = !g_debug_readout;
        if (in_dungeon_view()) gameloop_draw_view();
        return 1;
    case KEY_M:
    case KEY_TAB:
        if (!g_advanced_features || !in_dungeon_view()) return 1;
        g_automap_open = 1;
        advanced_draw_automap();
        return 1;
    }
    return 0;
}

void advanced_draw_debug(int16_t light)
{
    Bitmap screen = ui_screen();
    Box vp;
    char line[DEBUG_TEXT_SIZE];
    if (!g_advanced_features || !g_debug_readout || !ui_zone_box(ZONE_VIEWPORT, &vp)) return;
    int16_t x = (int16_t)(vp.x + DEBUG_MARGIN), y = (int16_t)(vp.y + DEBUG_MARGIN + TEXT_LINE_HEIGHT - TEXT_BASELINE_GAP);
    snprintf(line, sizeof line, "LEVEL %u X %d Y %d", map_level(g_party_map_index), g_party_map_x, g_party_map_y);
    text_draw(&screen, x, y, COLOR_WHITE, COLOR_BLACK, line);
    snprintf(line, sizeof line, "%s LIGHT %d", direction_names[NORMALIZE(g_party_direction)], light);
    text_draw(&screen, x, y + TEXT_LINE_HEIGHT + TEXT_BASELINE_GAP, COLOR_WHITE, COLOR_BLACK, line);
}
