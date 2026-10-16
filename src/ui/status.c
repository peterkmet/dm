/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/status.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/champion.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/party.h"
#include "ui/draw.h"
#include "ui/menus.h"
#include "ui/panel.h"
#include "ui/ui.h"
#include "ui/video.h"
#include <stdio.h>
#include <string.h>

/* Bars and party icons of the status boxes. */
#define BAR_COUNT         3 /* health, stamina, mana */
#define BAR_ZONE_STEP     4
#define CHAMPION_ICON_MAX 64 /* the icon buffer holds up to 64 x 64 pixels */

static uint8_t champion_color(int16_t champion_index)
{
    return tables_get_champion_color(champion_index);
}

/* Draws the health, stamina, and mana bars of the status box: the empty top part dark gray, the
   rest in the champion color. */
static void draw_bar_graphs(int16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    const int16_t bar_values[BAR_COUNT][2] = {
        {champion->current_health,  champion->maximum_health },
        {champion->current_stamina, champion->maximum_stamina},
        {champion->current_mana,    champion->maximum_mana   }
    };
    Bitmap screen = video_get_screen();
    for (int k = 0; k < BAR_COUNT; k++) {
        Box bar, empty;
        if (!ui_zone_box(ZONE_BAR_FIRST + BAR_ZONE_STEP * k + champion_index, &bar))
            continue;
        if (bar_values[k][0] < bar_values[k][1]) {
            empty = bar;
            if (bar_values[k][0]) {
                int16_t filled = (int32_t)bar_values[k][0] * empty.h / bar_values[k][1];
                empty.h -= filled > 1 ? filled : 1;
            }
            if (empty.h > 0) {
                draw_fill(&screen, &empty, COLOR_DARKEST_GRAY);
                bar.y = empty.h + empty.y;
                bar.h -= empty.h;
            }
        }
        if (bar_values[k][0] && bar.h > 0)
            draw_fill(&screen, &bar, champion_color(champion_index));
    }
}

void status_draw_slot(int16_t champion_index, int16_t slot)
{
    const Champion *champion = &g_champions[champion_index];
    bool inv = champion_index == g_session.inventory_champion_index;
    int16_t slot_box, border = -1, icon;
    Bitmap screen = video_get_screen();
    Box border_box;
    if (inv) {
        slot_box = slot + UI_SLOT_BOX_INVENTORY_FIRST;
    } else {
        if (slot > SLOT_HAND_ACTION || champion_index == g_session.candidate_champion_index)
            return;
        slot_box = slot + champion_index * 2;
    }
    uint16_t thing = slot < SLOT_COUNT ? champion->slots[slot] : g_open_chest_slots[slot - SLOT_CHEST_FIRST];
    bool wounded = slot < UI_BODY_SLOT_COUNT && (champion->wounds & 1 << slot);
    if (thing == THING_NONE) {
        if (slot < UI_BODY_SLOT_COUNT) {
            icon = ICON_EMPTY_BODY_SLOT_FIRST + slot * 2 + wounded;
            border = wounded ? GFX_SLOT_BOX_WOUNDED : GFX_SLOT_BOX_NORMAL;
        } else if (slot >= SLOT_NECK && slot <= SLOT_BACKPACK_FIRST) {
            icon = ICON_EMPTY_NECK_POUCH_BASE + slot;
        } else {
            icon = ICON_EMPTY_SLOT;
        }
    } else {
        icon = object_get_icon_index(thing);
        if (inv && slot == SLOT_HAND_ACTION && icon == ICON_CHEST_CLOSED)
            icon = ICON_CHEST_OPEN;
        if (slot < UI_BODY_SLOT_COUNT)
            border = wounded ? GFX_SLOT_BOX_WOUNDED : GFX_SLOT_BOX_NORMAL;
    }
    if (slot == SLOT_HAND_ACTION && champion_index == g_session.acting_champion_index)
        border = GFX_SLOT_BOX_ACTING_HAND;
    if (border != -1 && ui_get_slot_box_zone(slot_box, true, &border_box))
        ui_blit_graphic_box(inv ? ui_get_viewport() : &screen, border, &border_box, COLOR_DARKEST_GRAY);
    ui_draw_icon_in_slot_box(slot_box, icon);
}

void status_draw_damage(int16_t champion_index, int16_t damage)
{
    Bitmap screen = video_get_screen();
    char buf[8];
    bool inv = champion_index == g_session.inventory_champion_index;
    int16_t zone = champion_index + (inv ? ZONE_PORTRAIT_DAMAGE_FIRST : ZONE_STATUS_BOX_DAMAGE_FIRST);
    ui_blit_graphic(&screen, inv ? GFX_DAMAGE_PORTRAIT : GFX_DAMAGE_STATUS_BOX, zone, COLOR_FLESH);
    ui_print_zone(&screen, zone, COLOR_WHITE, COLOR_RED, ui_format_number(buf, sizeof buf, damage, false, VALUE_WIDTH));
    status_draw_champion(champion_index);
}

/* Draws the party icon of a champion (top right, at its cell, facing its direction relative to
   the party); light gray and recolored while the party is invisible. */
static void draw_icon(int16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    int16_t icon_width = tables_get_champion_icon_width();
    int16_t icon_height = tables_get_champion_icon_height();
    uint8_t px[CHAMPION_ICON_MAX * CHAMPION_ICON_MAX];
    Bitmap icon = {icon_width, icon_height, px};
    Bitmap screen = video_get_screen();
    const Bitmap *icons = gfx_get_bitmap(GFX_CHAMPION_ICONS);
    bool invisible = g_party.invisibility_count != 0;
    if (icon_width * icon_height > (int)sizeof px)
        return;
    int16_t cell = NORMALIZE(champion->cell - g_party_direction);
    memset(px, invisible ? COLOR_DARK_GRAY : champion_color(champion_index), sizeof px);
    if (icons)
        draw_blit(icons, NORMALIZE(champion->direction - g_party_direction) * icon_width, 0, &icon,
                  &(Box){0, 0, icon_width, icon_height}, COLOR_DARKEST_GRAY, 0);
    if (invisible) {
        const uint8_t *changes = tables_get_palette_changes(TABLES_CHANGES_INVISIBILITY);
        for (int i = 0; i < icon_width * icon_height; i++)
            px[i] = changes[px[i] & COLOR_INDEX_MASK];
    }
    ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + cell, COLOR_BLACK);
    ui_blit_zone(&screen, &icon, ZONE_CHAMPION_ICON_FIRST + cell, COLOR_NONE);
}

/* Draws the portrait in the status box while the inventory is open. */
static void draw_portrait(int16_t champion_index)
{
    Bitmap screen = video_get_screen();
    Bitmap portrait = ui_get_champion_portrait(champion_index);
    int16_t zone = ZONE_STATUS_BOX_PORTRAIT_FIRST + champion_index;
    Box b;
    if (!ui_zone_box(zone, &b))
        return;
    b.w = tables_get_portrait_width();
    b.h = tables_get_portrait_height();
    draw_blit(&portrait, 0, 0, &screen, &b, COLOR_NONE, 0);
    if (g_party.invisibility_count)
        ui_fill_zone_pattern(&screen, zone, COLOR_DARKEST_GRAY);
}

/* Draws the status box of a living champion: background, shields, and the parts that follow. */
static uint16_t draw_status_box(int16_t champion_index, uint16_t attr, bool inv)
{
    const Champion *champion = &g_champions[champion_index];
    Bitmap screen = video_get_screen();
    int16_t shields[3], n = 0;
    Box box;
    ui_zone_box(ZONE_STATUS_BOX_FIRST + champion_index, &box);
    draw_fill(&screen, &box, COLOR_DARKEST_GRAY);
    if (g_party.fire_shield_defense > 0)
        shields[n++] = GFX_SHIELD_FIRE;
    if (g_party.spell_shield_defense > 0)
        shields[n++] = GFX_SHIELD_SPELL;
    if (g_party.shield_defense > 0 || champion->shield_defense)
        shields[n++] = GFX_SHIELD_PARTY;
    while (n--)
        ui_blit_graphic_box(&screen, shields[n], &box, COLOR_FLESH);
    if (inv) {
        draw_portrait(champion_index);
        return attr | CHAMPION_ATTR_STATISTICS;
    }
    return attr | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS | CHAMPION_ATTR_STATISTICS |
           CHAMPION_ATTR_NAME_TITLE;
}

/* Draws the name in the status box (gold for the leader), or name and title in the inventory. */
static uint16_t draw_name(int16_t champion_index, uint16_t attr, bool inv)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap screen = video_get_screen();
    uint8_t color = champion_index == g_leader_index ? COLOR_GOLD : COLOR_LIGHT_GRAY;
    if (!inv) {
        ui_fill_zone(&screen, ZONE_STATUS_BOX_NAME_FIRST + champion_index, COLOR_DARK_GRAY);
        ui_print_zone(&screen, ZONE_STATUS_BOX_NAME_TEXT_FIRST + champion_index, color, COLOR_DARK_GRAY, c->name);
        return attr;
    }
    char s[PANEL_LINE_SIZE];
    bool joined = c->title[0] && strchr(",;-", c->title[0]); /* "NAME, TITLE" without a space */
    snprintf(s, sizeof s, "%.8s%s%.20s", c->name, joined ? "" : " ", c->title);
    ui_print_zone(ui_get_viewport(), ZONE_NAME_TITLE, color, COLOR_DARKEST_GRAY, s);
    return attr | CHAMPION_ATTR_VIEWPORT;
}

void status_draw_champion(int16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    uint16_t attr = champion->attributes;
    Bitmap screen = video_get_screen();
    bool inv = champion_index == g_session.inventory_champion_index;
    if (!(attr & CHAMPION_ATTR_REDRAW_MASK))
        return;
    if (attr & CHAMPION_ATTR_STATUS_BOX) {
        if (champion->current_health) {
            attr = draw_status_box(champion_index, attr, inv);
        } else {
            Box box;
            if (ui_zone_box(ZONE_STATUS_BOX_FIRST + champion_index, &box))
                ui_blit_graphic_box(&screen, GFX_STATUS_BOX_DEAD, &box, COLOR_NONE);
            ui_print_zone(&screen, ZONE_STATUS_BOX_NAME_TEXT_FIRST + champion_index, COLOR_LIGHT_GRAY, COLOR_DARK_GRAY,
                          champion->name);
            menus_draw_action_icon(champion_index);
        }
    }
    if (champion->current_health) {
        if (attr & CHAMPION_ATTR_NAME_TITLE)
            attr = draw_name(champion_index, attr, inv);
        if (attr & CHAMPION_ATTR_STATISTICS) {
            draw_bar_graphs(champion_index);
            if (inv) {
                panel_draw_statistics(champion_index);
                attr |= CHAMPION_ATTR_VIEWPORT;
            }
        }
        if (attr & CHAMPION_ATTR_WOUNDS) {
            if (inv)
                attr |= CHAMPION_ATTR_VIEWPORT;
            for (int s = inv ? UI_BODY_SLOT_COUNT - 1 : SLOT_HAND_ACTION; s >= 0; s--)
                status_draw_slot(champion_index, s);
        }
        if (attr & CHAMPION_ATTR_LOAD && inv) {
            panel_draw_load(champion_index);
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ICON)
            draw_icon(champion_index);
        if (attr & CHAMPION_ATTR_PANEL && inv) {
            panel_draw();
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ACTION_HAND) {
            status_draw_slot(champion_index, SLOT_HAND_ACTION);
            menus_draw_action_icon(champion_index);
            if (inv)
                attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_VIEWPORT)
            ui_draw_viewport();
    }
    champion->attributes &= CHAMPION_ATTR_KEEP_MASK;
}

void status_draw_all(uint16_t attributes)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        champion_add_redraw_flags(i, attributes);
        status_draw_champion(i);
    }
}
