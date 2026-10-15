#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "action.h"
#include "advanced.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "spells.h"
#include "tables.h"
#include "ui.h"
#include "video.h"

#define ACTING_NAME_LENGTH 7
#define ACTION_NAME_LENGTH 12
#define DAMAGE_SMALL_BELOW 16
#define DAMAGE_LARGE_BELOW 41

bool g_action_area_contains_icons = true;
uint8_t g_action_list_minimum_skill_levels[ACTION_LIST_COUNT];
uint16_t g_action_count;

/* Draws the movement arrows. */
void menus_draw_movement_arrows(void)
{
    Bitmap screen = ui_screen();
    ui_blit_graphic(&screen, GFX_MOVEMENT_ARROWS, ZONE_MOVEMENT_ARROWS, COLOR_NONE);
}

/* The charges of the object in the action hand of the acting champion (1 when it has none). */
static uint16_t action_hand_charge_count(void)
{
    uint16_t thing = g_champions[g_acting_champion_ordinal - 1].slots[SLOT_HAND_ACTION];
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        return weapon_charges(thing);
    case THING_ARMOUR:
        return armour_charges(thing);
    case THING_JUNK:
        return junk_charges(thing);
    }
    return 1;
}

/* Builds the action menu of the acting champion: the first action always; the other two when the
   champion has the skill level and, when the action needs them, charges. */
void menus_set_action_list(int16_t action_set_index)
{
    const ActionSet *set = tables_action_set(action_set_index);
    uint16_t n = 1;
    if (!set)
        return;
    g_action_list_indices[0] = set->action_indices[0];
    g_action_list_minimum_skill_levels[0] = 1;
    for (int k = 1; k < ACTION_LIST_COUNT; k++) {
        uint8_t action = set->action_indices[k];
        uint8_t level = set->minimum_skill_levels[k - 1];
        const ActionInfo *info = tables_action_info(action);
        if (action == ACTION_NONE || !info || ((level & ACTION_LEVEL_NEEDS_CHARGES) && !action_hand_charge_count()))
            continue;
        level &= ~ACTION_LEVEL_NEEDS_CHARGES;
        if (level <= champion_get_skill_level(g_acting_champion_ordinal - 1, info->skill_index)) {
            g_action_list_indices[n] = action;
            g_action_list_minimum_skill_levels[n++] = level;
        }
    }
    g_action_count = n;
    for (; n < ACTION_LIST_COUNT; n++)
        g_action_list_indices[n] = ACTION_NONE;
}

/* Closes the action menu and redraws the action hand of the champion. */
void menus_clear_acting_champion(void)
{
    int16_t ordinal = g_acting_champion_ordinal;
    action_clear_acting_champion();
    if (ordinal)
        ui_draw_champion_state(ordinal - 1);
}

/* Opens the action menu of a champion: the actions of the object in the action hand. */
void menus_set_acting_champion(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    int16_t set = ACTION_SET_EMPTY_HAND;
    if (c->attributes & CHAMPION_ATTR_ACTION_DISABLED || !c->current_health)
        return;
    if (c->slots[SLOT_HAND_ACTION] != THING_NONE) {
        const ObjectInfo *info = tables_object_info(inventory_get_object_info_index(c->slots[SLOT_HAND_ACTION]));
        if (!info || !(set = info->action_set_index))
            return;
    }
    g_acting_champion_ordinal = champion_index + 1;
    menus_set_action_list(set);
    g_action_area_contains_icons = false;
    c->attributes |= CHAMPION_ATTR_ACTION_HAND;
    ui_draw_champion_state(champion_index);
    menus_draw_action_area();
}

/* Draws the action hand object of a champion as a black icon on a cyan box (with the advanced
   features, in its own colors on the cyan box); dimmed while the action is disabled, a candidate is
   shown, or the party sleeps. */
void menus_draw_action_icon(uint16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap screen = ui_screen();
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    int16_t zone = ZONE_ACTION_ICON_BOX_FIRST + champion_index;
    uint16_t thing = c->slots[SLOT_HAND_ACTION];
    if (!g_action_area_contains_icons)
        return;
    if (!c->current_health) {
        ui_fill_zone(&screen, zone, COLOR_BLACK);
        return;
    }
    const ObjectInfo *info = thing == THING_NONE ? NULL : tables_object_info(inventory_get_object_info_index(thing));
    if (info && !info->action_set_index) { /* an object without actions: an empty box */
        memset(px, COLOR_CYAN, sizeof px);
    } else {
        ui_extract_icon(thing == THING_NONE ? ICON_EMPTY_ACTION_HAND : object_get_icon_index(thing), &icon);
        for (size_t i = 0; i < sizeof px; i++) {
            uint8_t stamp = tables_palette_changes(TABLES_CHANGES_ACTION_ICON)[px[i] & COLOR_INDEX_MASK];
            if (!g_advanced_features || stamp == COLOR_CYAN)
                px[i] = stamp;
        }
    }
    ui_fill_zone(&screen, zone, COLOR_CYAN);
    ui_blit_zone(&screen, &icon, ZONE_ACTION_ICON_FIRST + champion_index, COLOR_NONE);
    if (c->attributes & CHAMPION_ATTR_ACTION_DISABLED || g_candidate_champion_ordinal || g_party_is_sleeping)
        ui_fill_zone_pattern(&screen, zone, COLOR_BLACK);
}

/* Draws the action area: the action icons of all champions, or the action menu of the acting
   champion (its name and up to three action names). */
void menus_draw_action_area(void)
{
    Bitmap screen = ui_screen();
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

/* Shows the result of an action: "CAN'T REACH", "NEED AMMO", or the damage in a splat (smaller
   for small damage). */
void menus_draw_action_damage(int16_t damage)
{
    Bitmap screen = ui_screen();
    char buf[8];
    ui_fill_zone(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
    if (damage < 0) {
        ui_print_zone(&screen, ZONE_ACTION_DAMAGE, COLOR_CYAN, COLOR_BLACK,
                      gfx_string(damage == -1 ? TEXT_CANT_REACH : TEXT_NEED_AMMO));
        return;
    }
    const Bitmap *splat = gfx_bitmap(GFX_DAMAGE_ACTION);
    Bitmap scaled = {0};
    if (splat && damage < DAMAGE_LARGE_BELOW) {
        bool large = damage >= DAMAGE_SMALL_BELOW;
        draw_shrink(splat, tables_damage_splat_width(large), tables_damage_splat_height(large), NULL, &scaled);
    }
    ui_blit_zone(&screen, scaled.px ? &scaled : splat, ZONE_ACTION_DAMAGE, COLOR_NONE);
    free(scaled.px);
    snprintf(buf, sizeof buf, "%d", damage);
    ui_print_zone(&screen, ZONE_ACTION_DAMAGE, COLOR_CYAN, COLOR_BLACK, buf);
}

/* Runs each tick: turns idle champions toward their last attacker, then redraws the action area
   when requested (menu, damage, or icons). */
void menus_refresh_action_area(void)
{
    if (!g_party_champion_count)
        return;
    if (!g_party_is_sleeping && !g_candidate_champion_ordinal) {
        for (uint16_t i = 0; i < g_party_champion_count; i++) {
            Champion *c = &g_champions[i];
            if (i != g_leader_index && i + 1 != g_acting_champion_ordinal && c->maximum_damage_received &&
                c->direction != c->direction_maximum_damage_received) {
                c->direction = c->direction_maximum_damage_received;
                c->attributes |= CHAMPION_ATTR_ICON;
                ui_draw_champion_state(i);
            }
            c->maximum_damage_received = 0;
        }
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
        menus_draw_action_damage(g_action_damage);
        g_action_damage = 0;
    }
}

/* Draws the caster tabs of the spell area: a cyan tab for each other living champion, the name of
   the caster on its tab. */
void menus_draw_spell_area_controls(uint16_t champion_index)
{
    Bitmap screen = ui_screen();
    ui_fill_zone(&screen, ZONE_SPELL_AREA_TABS, COLOR_BLACK);
    for (uint16_t i = 0; i < 4; i++)
        if (i < g_party_champion_count && g_champions[i].current_health)
            ui_xor_screen_zone(ZONE_SPELL_CASTER_TAB_FIRST + champion_index * SPELL_TAB_STEP + i);
    ui_print_zone(&screen, ZONE_SPELL_CASTER_NAME_FIRST + champion_index * SPELL_TAB_STEP, COLOR_BLACK, COLOR_CYAN,
                  g_champions[champion_index].name);
}

/* Draws the six runes that the caster can choose at its symbol step. */
void menus_draw_available_symbols(uint16_t symbol_step)
{
    Bitmap screen = ui_screen();
    char s[2] = {0, 0};
    for (int k = 0; k < SYMBOL_COUNT; k++) {
        s[0] = (char)(SPELL_SYMBOL_FIRST + symbol_step * SYMBOL_COUNT + k);
        ui_print_zone(&screen, ZONE_SPELL_SYMBOL_FIRST + k, COLOR_CYAN, COLOR_BLACK, s);
    }
}

/* Draws the runes that the champion has chosen so far. */
void menus_draw_champion_symbols(uint16_t champion_index)
{
    Bitmap screen = ui_screen();
    const char *symbols = g_champions[champion_index].symbols;
    size_t n = 0;
    char s[2] = {0, 0};
    while (n < CHAMPION_SYMBOL_COUNT && symbols[n])
        n++;
    for (size_t k = 0; k < CHAMPION_SYMBOL_COUNT; k++) {
        s[0] = k < n ? symbols[k] : ' ';
        ui_print_zone(&screen, ZONE_SPELL_CHAMPION_SYMBOL_FIRST + k, COLOR_CYAN, COLOR_BLACK, s);
    }
}

/* Draws the spell area of the magic caster; without a caster it is black. */
static void draw_spell_area(void)
{
    Bitmap screen = ui_screen();
    int16_t caster = g_magic_caster_champion_index;
    if (caster == -1) {
        ui_fill_zone(&screen, ZONE_SPELL_AREA, COLOR_BLACK);
        return;
    }
    ui_blit_graphic(&screen, GFX_SPELL_AREA, ZONE_SPELL_AREA, COLOR_NONE);
    menus_draw_spell_area_controls(caster);
    menus_draw_available_symbols(g_champions[caster].symbol_step);
    menus_draw_champion_symbols(caster);
}

/* Selects the spell caster and draws the spell area; -1 clears it. */
void menus_set_magic_caster(int16_t champion_index)
{
    if (champion_index == g_magic_caster_champion_index ||
        (champion_index != -1 && !g_champions[champion_index].current_health))
        return;
    g_magic_caster_champion_index = champion_index;
    draw_spell_area();
}

/* Draws the parts of the spell area that the game rules changed: all of it, the caster tabs, or the runes. */
void menus_refresh_spell_area(void)
{
    int16_t caster = g_magic_caster_champion_index;
    if (g_redraw & REDRAW_SPELL_AREA) {
        draw_spell_area();
    } else if (caster != -1) {
        if (g_redraw & REDRAW_SPELL_TABS)
            menus_draw_spell_area_controls(caster);
        if (g_redraw & REDRAW_SPELL_SYMBOLS) {
            menus_draw_available_symbols(g_champions[caster].symbol_step);
            menus_draw_champion_symbols(caster);
        }
    }
    g_redraw &= ~(REDRAW_SPELL_AREA | REDRAW_SPELL_TABS | REDRAW_SPELL_SYMBOLS);
}

/* No acting champion, no action list, and no action damage at the start of a game. */
void menus_reset(void)
{
    g_acting_champion_ordinal = 0;
    g_action_area_contains_icons = true;
    memset(g_action_list_indices, ACTION_NONE, sizeof g_action_list_indices);
    memset(g_action_list_minimum_skill_levels, 0, sizeof g_action_list_minimum_skill_levels);
    g_action_count = 0;
    g_action_damage = 0;
    g_redraw = 0;
}
