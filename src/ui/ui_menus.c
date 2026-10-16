/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/graphics.h"
#include "data/ids.h"
#include "data/spells.h"
#include "data/tables.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/magic.h"
#include "ui/advanced.h"
#include "ui/draw.h"
#include "ui/ui.h"
#include "ui/video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACTING_NAME_LENGTH 7
#define ACTION_NAME_LENGTH 12
#define DAMAGE_SMALL_BELOW 16
#define DAMAGE_LARGE_BELOW 41

bool g_action_area_contains_icons = true;

/* Draws the movement arrows. */
void menus_draw_movement_arrows(void)
{
    Bitmap screen = ui_get_screen();
    ui_blit_graphic(&screen, GFX_MOVEMENT_ARROWS, ZONE_MOVEMENT_ARROWS, COLOR_NONE);
}

void menus_clear_acting_champion(void)
{
    int16_t ordinal = g_acting_champion_ordinal;
    action_clear_acting_champion();
    if (ordinal)
        ui_draw_champion_state(ordinal - 1);
}

void menus_set_acting_champion(uint16_t champion_index)
{
    if (!action_set_acting_champion(champion_index))
        return;
    g_action_area_contains_icons = false;
    ui_draw_champion_state(champion_index);
    menus_draw_action_area();
}

void menus_draw_action_icon(uint16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    Bitmap screen = ui_get_screen();
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    int16_t zone = ZONE_ACTION_ICON_BOX_FIRST + champion_index;
    uint16_t thing = champion->slots[SLOT_HAND_ACTION];
    if (!g_action_area_contains_icons)
        return;
    if (!champion->current_health) {
        ui_fill_zone(&screen, zone, COLOR_BLACK);
        return;
    }
    const ObjectInfo *info =
        thing == THING_NONE ? NULL : tables_get_object_info(inventory_get_object_info_index(thing));
    if (info && !info->action_set_index) { /* an object without actions: an empty box */
        memset(px, COLOR_CYAN, sizeof px);
    } else {
        ui_extract_icon(thing == THING_NONE ? ICON_EMPTY_ACTION_HAND : object_get_icon_index(thing), &icon);
        for (size_t i = 0; i < sizeof px; i++) {
            uint8_t stamp = tables_get_palette_changes(TABLES_CHANGES_ACTION_ICON)[px[i] & COLOR_INDEX_MASK];
            if (!g_advanced_features || stamp == COLOR_CYAN)
                px[i] = stamp;
        }
    }
    ui_fill_zone(&screen, zone, COLOR_CYAN);
    ui_blit_zone(&screen, &icon, ZONE_ACTION_ICON_FIRST + champion_index, COLOR_NONE);
    if (champion->attributes & CHAMPION_ATTR_ACTION_DISABLED || g_candidate_champion_ordinal || g_party_is_sleeping)
        ui_fill_zone_pattern(&screen, zone, COLOR_BLACK);
}

void menus_draw_action_area(void)
{
    Bitmap screen = ui_get_screen();
    ui_fill_zone(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
    if (g_action_area_contains_icons) {
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            menus_draw_action_icon(i);
    } else if (g_acting_champion_ordinal) {
        int16_t zone = ZONE_ACTION_AREA;
        if (g_action_list_indices[2] == ACTION_NONE)
            zone = ZONE_ACTION_AREA_TWO_ACTIONS;
        if (g_action_list_indices[1] == ACTION_NONE)
            zone = ZONE_ACTION_AREA_ONE_ACTION;
        ui_blit_graphic(&screen, GFX_ACTION_AREA_NAMES, zone, COLOR_NONE);
        ui_print_zone_padded(&screen, ZONE_ACTING_CHAMPION_NAME, COLOR_BLACK, COLOR_CYAN,
                             g_champions[g_acting_champion_ordinal - 1].name, ACTING_NAME_LENGTH);
        for (int k = 0; k < ACTION_LIST_COUNT; k++) {
            uint8_t action = g_action_list_indices[k];
            ui_print_zone_padded(&screen, ZONE_ACTION_NAME_FIRST + k, COLOR_CYAN, COLOR_BLACK,
                                 action == ACTION_NONE ? "" : gfx_action_name(action), ACTION_NAME_LENGTH);
        }
    }
    g_redraw &= ~REDRAW_ACTION_AREA;
}

void menus_draw_action_damage(int16_t damage)
{
    Bitmap screen = ui_get_screen();
    char buf[8];
    ui_fill_zone(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
    if (damage < 0) {
        ui_print_zone(&screen, ZONE_ACTION_DAMAGE, COLOR_CYAN, COLOR_BLACK,
                      gfx_string(damage == -1 ? TEXT_CANT_REACH : TEXT_NEED_AMMO));
        return;
    }
    const Bitmap *splat = gfx_get_bitmap(GFX_DAMAGE_ACTION);
    Bitmap scaled = {0};
    if (splat && damage < DAMAGE_LARGE_BELOW) {
        bool large = damage >= DAMAGE_SMALL_BELOW;
        draw_shrink(splat, tables_get_damage_splat_width(large), tables_get_damage_splat_height(large), NULL, &scaled);
    }
    ui_blit_zone(&screen, scaled.px ? &scaled : splat, ZONE_ACTION_DAMAGE, COLOR_NONE);
    free(scaled.px);
    snprintf(buf, sizeof buf, "%d", damage);
    ui_print_zone(&screen, ZONE_ACTION_DAMAGE, COLOR_CYAN, COLOR_BLACK, buf);
}

void menus_refresh_action_area(void)
{
    if (!g_party_champion_count)
        return;
    if (!g_party_is_sleeping && !g_candidate_champion_ordinal) {
        uint16_t turned = champion_turn_to_last_attackers();
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            if (turned & 1 << i)
                ui_draw_champion_state(i);
    } else {
        if (g_acting_champion_ordinal) {
            menus_clear_acting_champion();
            return;
        }
        if (!g_candidate_champion_ordinal)
            return;
    }
    if (!(g_redraw & REDRAW_ACTION_AREA))
        return;
    if (g_acting_champion_ordinal) {
        g_action_area_contains_icons = false;
        g_champions[g_acting_champion_ordinal - 1].attributes |= CHAMPION_ATTR_ACTION_HAND;
        ui_draw_champion_state(g_acting_champion_ordinal - 1);
        menus_draw_action_area();
    } else if (!g_action_damage) {
        g_action_area_contains_icons = true;
        menus_draw_action_area();
    } else {
        menus_draw_action_damage(action_take_damage());
    }
}

/* Draws the caster tabs of the spell area: a cyan tab for each other living champion, the name of
   the caster on its tab. */
static void draw_caster_tabs(uint16_t champion_index, const Champion *caster)
{
    Bitmap screen = ui_get_screen();
    ui_fill_zone(&screen, ZONE_SPELL_AREA_TABS, COLOR_BLACK);
    for (uint16_t i = 0; i < 4; i++) {
        const Champion *champion = i == champion_index ? caster : &g_champions[i];
        if (i < g_party_champion_count && champion->current_health)
            ui_xor_screen_zone(ZONE_SPELL_CASTER_TAB_FIRST + champion_index * SPELL_TAB_STEP + i);
    }
    ui_print_zone(&screen, ZONE_SPELL_CASTER_NAME_FIRST + champion_index * SPELL_TAB_STEP, COLOR_BLACK, COLOR_CYAN,
                  caster->name);
}

void menus_draw_spell_area_controls(uint16_t champion_index)
{
    draw_caster_tabs(champion_index, &g_champions[champion_index]);
}

void menus_draw_available_symbols(uint16_t symbol_step)
{
    Bitmap screen = ui_get_screen();
    char s[2] = {0, 0};
    for (int k = 0; k < SYMBOL_COUNT; k++) {
        s[0] = (char)spell_get_symbol(symbol_step, k);
        ui_print_zone(&screen, ZONE_SPELL_SYMBOL_FIRST + k, COLOR_CYAN, COLOR_BLACK, s);
    }
}

/* Draws the runes that a champion has chosen so far. */
static void draw_chosen_symbols(const char *symbols)
{
    Bitmap screen = ui_get_screen();
    size_t n = 0;
    char s[2] = {0, 0};
    while (n < CHAMPION_SYMBOL_COUNT && symbols[n])
        n++;
    for (size_t k = 0; k < CHAMPION_SYMBOL_COUNT; k++) {
        s[0] = k < n ? symbols[k] : ' ';
        ui_print_zone(&screen, ZONE_SPELL_CHAMPION_SYMBOL_FIRST + k, COLOR_CYAN, COLOR_BLACK, s);
    }
}

void menus_draw_champion_symbols(uint16_t champion_index)
{
    draw_chosen_symbols(g_champions[champion_index].symbols);
}

/* Draws the spell area of the magic caster from the caster record; without a caster it is black. */
static void draw_spell_area_of(const Champion *caster)
{
    Bitmap screen = ui_get_screen();
    int16_t caster_index = g_magic_caster_champion_index;
    if (caster_index == -1) {
        ui_fill_zone(&screen, ZONE_SPELL_AREA, COLOR_BLACK);
        return;
    }
    ui_blit_graphic(&screen, GFX_SPELL_AREA, ZONE_SPELL_AREA, COLOR_NONE);
    draw_caster_tabs(caster_index, caster);
    menus_draw_available_symbols(caster->symbol_step);
    draw_chosen_symbols(caster->symbols);
}

/* Draws the spell area of the magic caster; without a caster it is black. */
static void draw_spell_area(void)
{
    int16_t caster = g_magic_caster_champion_index;
    draw_spell_area_of(caster == -1 ? NULL : &g_champions[caster]);
}

void menus_draw_spell_area(void)
{
    int16_t caster = g_magic_caster_champion_index;
    if (caster == -1)
        return;
    if (!g_champions[caster].current_health) {
        magic_set_caster(-1);
        return;
    }
    draw_spell_area();
}

void menus_set_magic_caster(int16_t champion_index)
{
    if (magic_set_caster(champion_index))
        draw_spell_area();
}

/* Draws the parts of the spell area that the game rules changed: all of it, the caster tabs, or the runes. */
void menus_refresh_spell_area(const Champion *caster)
{
    int16_t caster_index = g_magic_caster_champion_index;
    if (!caster && caster_index != -1)
        caster = &g_champions[caster_index];
    if (g_redraw & REDRAW_SPELL_AREA) {
        draw_spell_area_of(caster);
    } else if (caster_index != -1) {
        if (g_redraw & REDRAW_SPELL_TABS)
            draw_caster_tabs(caster_index, caster);
        if (g_redraw & REDRAW_SPELL_SYMBOLS) {
            menus_draw_available_symbols(caster->symbol_step);
            draw_chosen_symbols(caster->symbols);
        }
    }
    g_redraw &= ~(REDRAW_SPELL_AREA | REDRAW_SPELL_TABS | REDRAW_SPELL_SYMBOLS);
}

void menus_reset(void)
{
    action_reset();
    g_action_area_contains_icons = true;
}
