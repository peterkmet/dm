/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/adventure.h"
#include "data/ftl.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/magic.h"
#include "engine/map.h"
#include "engine/message.h"
#include "ui/advanced.h"
#include "ui/command.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <stdio.h>
#include <string.h>

#define BAR_COUNT                 3 /* health, stamina, mana */
#define BAR_ZONE_STEP             4
#define FOOD_WATER_RANGE          3072 /* -1024 to 2048 */
#define VALUE_WIDTH               3
#define VALUE_PAIR_SIZE           16         /* "current/maximum" */
#define CHAMPION_ICON_MAX         64         /* the icon buffer holds up to 64 x 64 pixels */
#define PANEL_OBJECT_TEXT_Y       87         /* baseline of the first description line */
#define PANEL_LINE_WRAP           18         /* longer lines break at a space */
#define SCROLL_FONT_SHIFT         0x60       /* '{' to '~' print as scroll font codes 27-30 */
#define POTION_POWER_SYMBOL_FIRST '_'        /* + power / 40: the power rune before the name */
#define SCROLL_LINE_CHARS         18         /* characters that fit on the parchment */
#define RUNE_FIRST                0x60       /* font character of the first rune */
#define CREATURE_NAME_TEXT        "CREANAME" /* CSB: scroll text replaced by the creature name ahead */
#define MAGIC_MAP_RUNE_FIRST      0x72       /* font character of the KU rune; ROS, DAIN, NETA follow */
#define MAGIC_MAP_RUNE_COUNT      4
#define METER_LABEL_CHARS         8 /* the bar starts after the longest label */
#define METER_BAR_WIDTH           36
#define METER_BAR_HEIGHT          5
#define METER_VALUE_GAP           4 /* pixels between the bar and the value */
#define METER_BAR_ABOVE_BASELINE  4 /* the bar top is level with the top of the letters */
#define SLOT_BOX_ACTION_HAND      (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_HAND_ACTION)
#define SLOT_BOX_CHEST_FIRST      (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_CHEST_FIRST)

/* Object attribute bits of the description; the names are strings TEXT_CONSUMABLE + bit. */
#define ATTRIBUTE_CONSUMABLE 1
#define ATTRIBUTE_POISONED   2
#define ATTRIBUTE_BROKEN     4
#define ATTRIBUTE_CURSED     8
#define ATTRIBUTE_COUNT      4

const char *const g_rune_names[UI_RUNE_COUNT] = {
    "LO",  "UM", "ON",   "EE", "PAL", "MON", "YA", "VI",  "OH",   "FUL",  "DES", "ZO",
    "VEN", "EW", "KATH", "IR", "BRO", "GOR", "KU", "ROS", "DAIN", "NETA", "RA",  "SAR",
};

int16_t g_panel_content;
static int16_t
    scroll_redraw; /* CSB: the panel scroll shows a creature name or the magic map, so it draws again each tick */
static bool scroll_redraw_eye; /* the scroll is the leader hand object while the eye is pressed */

/* A number as text: right aligned in width characters, or not padded. */
static const char *format_number(char *buf, uint16_t value, int pad, int width)
{
    if (pad)
        snprintf(buf, 8, "%*u", width, value);
    else
        snprintf(buf, 8, "%u", value);
    return buf;
}

static uint8_t champion_color(int16_t champion_index)
{
    return tables_get_champion_color(champion_index);
}

/* Draws the health, stamina, and mana bars of the status box: the empty top part dark gray, the
   rest in the champion color. */
void ui_draw_champion_bar_graphs(int16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    const int16_t bar_values[BAR_COUNT][2] = {
        {champion->current_health,  champion->maximum_health },
        {champion->current_stamina, champion->maximum_stamina},
        {champion->current_mana,    champion->maximum_mana   }
    };
    Bitmap screen = ui_get_screen();
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

/* Prints "current/maximum" in the inventory. */
static void draw_value_pair(int16_t zone, int16_t value, int16_t maximum)
{
    char a[8], b[8], s[VALUE_PAIR_SIZE];
    snprintf(s, sizeof s, "%s/%s", format_number(a, value, 1, VALUE_WIDTH), format_number(b, maximum, 1, VALUE_WIDTH));
    ui_print_zone(ui_get_viewport(), zone, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
}

void ui_draw_champion_slot(uint16_t champion_index, uint16_t slot)
{
    const Champion *champion = &g_champions[champion_index];
    bool inv = champion_index + 1 == g_inventory_champion_ordinal;
    int16_t slot_box, border = -1, icon;
    Bitmap screen = ui_get_screen();
    Box border_box;
    if (inv) {
        slot_box = slot + UI_SLOT_BOX_INVENTORY_FIRST;
    } else {
        if (slot > SLOT_HAND_ACTION || champion_index + 1 == g_candidate_champion_ordinal)
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
    if (slot == SLOT_HAND_ACTION && champion_index + 1 == g_acting_champion_ordinal)
        border = GFX_SLOT_BOX_ACTING_HAND;
    if (border != -1 && ui_get_slot_box_zone(slot_box, 1, &border_box))
        ui_blit_graphic_box(inv ? ui_get_viewport() : &screen, border, &border_box, COLOR_DARKEST_GRAY);
    ui_draw_icon_in_slot_box(slot_box, icon);
}

void ui_draw_champion_damage(int16_t champion_index, int16_t damage)
{
    Bitmap screen = ui_get_screen();
    char buf[8];
    bool inv = champion_index + 1 == g_inventory_champion_ordinal;
    int16_t zone = champion_index + (inv ? ZONE_PORTRAIT_DAMAGE_FIRST : ZONE_STATUS_BOX_DAMAGE_FIRST);
    ui_blit_graphic(&screen, inv ? GFX_DAMAGE_PORTRAIT : GFX_DAMAGE_STATUS_BOX, zone, COLOR_FLESH);
    ui_print_zone(&screen, zone, COLOR_WHITE, COLOR_RED, format_number(buf, damage, 0, VALUE_WIDTH));
    ui_draw_champion_state(champion_index);
}

/* Draws the party icon of a champion (top right, at its cell, facing its direction relative to
   the party); light gray and recolored while the party is invisible. */
static void draw_icon(uint16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    int16_t icon_width = tables_get_champion_icon_width();
    int16_t icon_height = tables_get_champion_icon_height();
    uint8_t px[CHAMPION_ICON_MAX * CHAMPION_ICON_MAX];
    Bitmap icon = {icon_width, icon_height, px};
    Bitmap screen = ui_get_screen();
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

Bitmap ui_draw_champion_portrait(int16_t champion_index)
{
    return (Bitmap){CHAMPION_PORTRAIT_WIDTH, CHAMPION_PORTRAIT_HEIGHT, g_champion_portraits[champion_index]};
}

void ui_draw_status_box_portrait(int16_t champion_index)
{
    Bitmap screen = ui_get_screen();
    Bitmap portrait = ui_draw_champion_portrait(champion_index);
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

/* Draws the inventory statistics: health, stamina, mana values, and the mouth and eye borders
   (red when hungry, thirsty, or poisoned, and when a statistic is below its maximum). */
static void draw_inventory_statistics(uint16_t champion_index)
{
    const Champion *champion = &g_champions[champion_index];
    Bitmap *viewport = ui_get_viewport();
    Box icon_box;
    draw_value_pair(ZONE_HEALTH_VALUE, champion->current_health, champion->maximum_health);
    draw_value_pair(ZONE_HEALTH_VALUE + 1, champion->current_stamina / STAMINA_DISPLAY_UNIT,
                    champion->maximum_stamina / STAMINA_DISPLAY_UNIT); /* stamina shows in tenths */
    draw_value_pair(ZONE_HEALTH_VALUE + 2, champion->current_mana, champion->maximum_mana);
    int16_t mouth = champion->food < 0 || champion->water < 0 || champion->poison_event_count ? GFX_SLOT_BOX_WOUNDED
                                                                                              : GFX_SLOT_BOX_NORMAL;
    int16_t eye = GFX_SLOT_BOX_NORMAL;
    if (ui_get_icon_zone_box(ZONE_MOUTH, 1, &icon_box))
        ui_blit_graphic_box(viewport, mouth, &icon_box, COLOR_DARKEST_GRAY);
    for (int s = STAT_STRENGTH; s <= STAT_ANTIFIRE; s++)
        if (champion->statistics[s][STAT_CURRENT] < champion->statistics[s][STAT_MAXIMUM]) {
            eye = GFX_SLOT_BOX_WOUNDED;
            break;
        }
    if (ui_get_icon_zone_box(ZONE_EYE, 1, &icon_box))
        ui_blit_graphic_box(viewport, eye, &icon_box, COLOR_DARKEST_GRAY);
}

/* Draws the load line "LOAD nn.n/ mm KG": red above the maximum load, yellow above 5/8 of it. */
static void draw_load(uint16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t max = champion_get_maximum_load(c);
    uint8_t color = max < c->load ? COLOR_RED
                    : (int32_t)max * HEAVY_LOAD_NUMERATOR < (int32_t)c->load * HEAVY_LOAD_DENOMINATOR
                        ? COLOR_YELLOW
                        : COLOR_LIGHT_GRAY;
    char text[PANEL_LINE_SIZE], a[8], b[8], d[8]; /* the load is in tenths of a kilogram */
    ui_print_zone(ui_get_viewport(), ZONE_LOAD_LABEL, color, COLOR_DARKEST_GRAY, gfx_string(TEXT_LOAD));
    snprintf(text, sizeof text, "%s%s%s/%s%s", format_number(a, c->load / WEIGHT_TENTHS, 1, VALUE_WIDTH),
             gfx_string(TEXT_DECIMAL_POINT), format_number(d, c->load % WEIGHT_TENTHS, 0, 1),
             format_number(b, (uint16_t)(max + WEIGHT_TENTHS / 2) / WEIGHT_TENTHS, 1, VALUE_WIDTH),
             gfx_string(TEXT_KG)); /* 16-bit sum as in the original */
    ui_print_zone(ui_get_viewport(), ZONE_LOAD_VALUE, color, COLOR_DARKEST_GRAY, text);
}

/* Draws the status box of a living champion: background, shields, and the parts that follow. */
static uint16_t draw_status_box(uint16_t champion_index, uint16_t attr, int inv)
{
    const Champion *champion = &g_champions[champion_index];
    Bitmap screen = ui_get_screen();
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
        ui_draw_status_box_portrait(champion_index);
        return attr | CHAMPION_ATTR_STATISTICS;
    }
    return attr | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS | CHAMPION_ATTR_STATISTICS |
           CHAMPION_ATTR_NAME_TITLE;
}

/* Draws the name in the status box (gold for the leader), or name and title in the inventory. */
static uint16_t draw_name(uint16_t champion_index, uint16_t attr, int inv)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap screen = ui_get_screen();
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

void ui_draw_champion_state(uint16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    uint16_t attr = champion->attributes;
    Bitmap screen = ui_get_screen();
    bool inv = champion_index + 1 == g_inventory_champion_ordinal;
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
            ui_draw_champion_bar_graphs(champion_index);
            if (inv) {
                draw_inventory_statistics(champion_index);
                attr |= CHAMPION_ATTR_VIEWPORT;
            }
        }
        if (attr & CHAMPION_ATTR_WOUNDS) {
            if (inv)
                attr |= CHAMPION_ATTR_VIEWPORT;
            for (int s = inv ? UI_BODY_SLOT_COUNT - 1 : SLOT_HAND_ACTION; s >= 0; s--)
                ui_draw_champion_slot(champion_index, s);
        }
        if (attr & CHAMPION_ATTR_LOAD && inv) {
            draw_load(champion_index);
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ICON)
            draw_icon(champion_index);
        if (attr & CHAMPION_ATTR_PANEL && inv) {
            ui_draw_panel();
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ACTION_HAND) {
            ui_draw_champion_slot(champion_index, SLOT_HAND_ACTION);
            menus_draw_action_icon(champion_index);
            if (inv)
                attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_VIEWPORT)
            ui_draw_viewport();
    }
    champion->attributes &= CHAMPION_ATTR_KEEP_MASK;
}

void ui_draw_all_champion_states(uint16_t attributes)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        champion_add_redraw_flags(i, attributes);
        ui_draw_champion_state(i);
    }
}

/* Draws a food or water bar with a black shadow: length by value (-1024 to 2048), yellow below
   0, red below -512. */
static void draw_food_water_bar(int16_t value, int16_t zone, uint8_t color)
{
    Box b;
    if (value < FOOD_WATER_STARVING)
        color = COLOR_RED;
    else if (value < 0)
        color = COLOR_YELLOW;
    if (!layout_get_zone_box(zone, (int32_t)(value - FOOD_WATER_MINIMUM) * ZONE_SCALE_FULL / FOOD_WATER_RANGE,
                             ZONE_SCALE_FULL, &b))
        return;
    Box shadow = b;
    shadow.x += tables_get_bar_shadow();
    shadow.y += tables_get_bar_shadow();
    draw_fill(ui_get_viewport(), &shadow, COLOR_BLACK);
    draw_fill(ui_get_viewport(), &b, color);
}

void ui_draw_panel_food_water_poisoned(void)
{
    scroll_redraw = 0;
    const Champion *c = &g_champions[g_inventory_champion_ordinal - 1];
    Bitmap *viewport = ui_get_viewport();
    inventory_close_chest();
    ui_blit_graphic(viewport, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    ui_blit_graphic(viewport, GFX_PANEL_FOOD_LABEL, ZONE_PANEL_FOOD_LABEL, COLOR_DARKEST_GRAY);
    ui_blit_graphic(viewport, GFX_PANEL_WATER_LABEL, ZONE_PANEL_WATER_LABEL, COLOR_DARKEST_GRAY);
    if (c->poison_event_count)
        ui_blit_graphic(viewport, GFX_PANEL_POISONED_LABEL, ZONE_PANEL_POISONED_LABEL, COLOR_DARKEST_GRAY);
    draw_food_water_bar(c->food, ZONE_PANEL_FOOD_BAR, COLOR_LIGHT_BROWN);
    draw_food_water_bar(c->water, ZONE_PANEL_WATER_BAR, COLOR_BLUE);
}

void ui_draw_panel(void)
{
    scroll_redraw = 0;
    inventory_close_chest();
    if (g_candidate_champion_ordinal) {
        g_panel_content = PANEL_CONTENT_RESURRECT;
        ui_blit_graphic(ui_get_viewport(), GFX_PANEL_RESURRECT_REINCARNATE, ZONE_PANEL, COLOR_DARK_GREEN);
        return;
    }
    uint16_t thing = g_champions[g_inventory_champion_ordinal - 1].slots[SLOT_HAND_ACTION];
    g_panel_content = thing == THING_NONE                    ? PANEL_CONTENT_FOOD_WATER
                      : THING_TYPE(thing) == THING_SCROLL    ? PANEL_CONTENT_SCROLL
                      : THING_TYPE(thing) == THING_CONTAINER ? PANEL_CONTENT_CHEST
                                                             : PANEL_CONTENT_FOOD_WATER;
    if (g_panel_content == PANEL_CONTENT_FOOD_WATER)
        ui_draw_panel_food_water_poisoned();
    else
        ui_draw_panel_object(thing, 0);
}

/* The position of zone (a 1 x 1 box). */
static void get_zone_point(int16_t zone, int16_t *x, int16_t *y)
{
    int16_t w = 1;
    int16_t h = 1;
    Box b = {0};
    layout_get_zone(zone, 0, 0, &b, &w, &h);
    *x = b.x;
    *y = b.y;
}

const char *ui_get_skill_rank_name(int16_t level)
{
    if (level <= 1)
        return NULL;
    if (level > SKILL_LEVEL_MAXIMUM)
        level = SKILL_LEVEL_MAXIMUM;
    return gfx_string(tables_get_skill_level_text(level));
}

void ui_draw_champion_skills_and_statistics(void)
{
    scroll_redraw = 0;
    int16_t champion_index = g_inventory_champion_ordinal - 1, x, y, vx;
    const Champion *champion = &g_champions[champion_index];
    Bitmap *viewport = ui_get_viewport();
    char line[PANEL_LINE_SIZE], a[8], b[8];
    inventory_close_chest();
    ui_blit_graphic(viewport, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    get_zone_point(ZONE_PANEL_SKILLS, &x, &y);
    for (uint16_t k = 0; k < SKILL_BASE_COUNT; k++) {
        int16_t level = champion_get_skill_level(champion_index, k | SKILL_IGNORE_TEMPORARY_EXPERIENCE);
        if (level == 1)
            continue;
        snprintf(line, sizeof line, "%s %s", ui_get_skill_rank_name(level), gfx_string(tables_get_base_skill_text(k)));
        text_draw(viewport, x, y, g_champion_skill_level_up[champion_index][k] ? COLOR_GREEN : COLOR_LIGHT_GRAY,
                  COLOR_DARKEST_GRAY, line);
        y += MESSAGE_ROW_HEIGHT;
    }
    get_zone_point(ZONE_PANEL_STATISTICS, &vx, &y);
    for (int k = STAT_STRENGTH; k <= STAT_ANTIFIRE; k++) {
        uint8_t cur = champion->statistics[k][STAT_CURRENT];
        uint8_t max = champion->statistics[k][STAT_MAXIMUM];
        text_draw(viewport, x, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, gfx_string(tables_get_statistic_text(k)));
        text_draw(viewport, vx, y,
                  cur < max   ? COLOR_RED
                  : cur > max ? COLOR_GREEN
                              : COLOR_LIGHT_GRAY,
                  COLOR_DARKEST_GRAY, format_number(a, cur, 1, VALUE_WIDTH));
        snprintf(line, sizeof line, "/%s", format_number(b, max, 1, VALUE_WIDTH));
        text_draw(viewport, vx + TEXT_CHAR_WIDTH * VALUE_WIDTH, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, line);
        y += MESSAGE_ROW_HEIGHT;
    }
}

void ui_draw_inventory_open(int16_t champion_index)
{
    Bitmap screen = ui_get_screen();
    Bitmap *viewport = ui_get_viewport();
    const Bitmap *bg = gfx_get_bitmap(GFX_INVENTORY);
    if (!g_inventory_champion_ordinal)
        ui_fill_zone_pattern(&screen, ZONE_MOVEMENT_ARROWS, COLOR_BLACK);
    inventory_set_open_champion(champion_index + 1);
    if (bg)
        draw_blit(bg, 0, 0, viewport, &(Box){0, 0, viewport->w, viewport->h}, COLOR_NONE, 0);
    if (g_candidate_champion_ordinal) { /* a candidate cannot save, sleep, or close */
        ui_fill_zone(viewport, ZONE_INVENTORY_SAVE, COLOR_DARKEST_GRAY);
        ui_fill_zone(viewport, ZONE_INVENTORY_SLEEP, COLOR_DARKEST_GRAY);
        ui_fill_zone(viewport, ZONE_INVENTORY_CLOSE, COLOR_DARKEST_GRAY);
    }
    ui_print_zone(viewport, ZONE_HEALTH_LABEL, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, gfx_string(TEXT_HEALTH));
    ui_print_zone(viewport, ZONE_HEALTH_LABEL + 1, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, gfx_string(TEXT_STAMINA));
    ui_print_zone(viewport, ZONE_HEALTH_LABEL + 2, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, gfx_string(TEXT_MANA));
    for (uint16_t s = 0; s < SLOT_COUNT; s++)
        ui_draw_champion_slot(champion_index, s);
    g_champions[champion_index].attributes |= CHAMPION_ATTR_INVENTORY_ALL;
    ui_draw_champion_state(champion_index);
}

void ui_draw_inventory_close(int16_t champion_index)
{
    inventory_set_open_champion(0);
    inventory_close_chest();
    if (g_champions[champion_index].current_health && !g_candidate_champion_ordinal) {
        g_champions[champion_index].attributes |= CHAMPION_ATTR_STATUS_BOX;
        ui_draw_champion_state(champion_index);
    }
    if (!g_party_is_sleeping)
        menus_draw_movement_arrows();
}

static void append(char *dst, size_t size, const char *s)
{
    size_t n = strlen(dst);
    snprintf(dst + n, size - n, "%s", s);
}

/* The attribute list "(POISONED, BROKEN AND CURSED)" of the bits in both masks, or "" for none. */
static void format_attributes_text(uint16_t potential, uint16_t actual, char *dst, size_t size)
{
    uint16_t bits = potential & actual;
    int16_t left = 0;
    for (int16_t i = 0; i < ATTRIBUTE_COUNT; i++)
        left += bits >> i & 1;
    dst[0] = 0;
    if (!left)
        return;
    append(dst, size, tables_get_text(TABLES_TEXT_ATTRIBUTES_PREFIX));
    for (int16_t i = 0; i < ATTRIBUTE_COUNT; i++) {
        if (!(bits >> i & 1))
            continue;
        append(dst, size, gfx_string(TEXT_CONSUMABLE + i));
        left--;
        if (left > 1)
            append(dst, size, tables_get_text(TABLES_TEXT_LIST_SEPARATOR));
        else if (left == 1)
            append(dst, size, gfx_string(TEXT_AND));
    }
    append(dst, size, tables_get_text(TABLES_TEXT_ATTRIBUTES_SUFFIX));
}

int16_t panel_draw_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE])
{
    int16_t icon = object_get_icon_index(thing);
    int16_t n = 0;
    uint16_t type = THING_TYPE(thing);
    uint16_t potential = 0;
    uint16_t actual = 0;
    uint16_t weight = inventory_get_object_weight(thing);
    if (icon == ICON_BONES)
        snprintf(name, PANEL_LINE_SIZE, "%.8s%s%s", g_champions[junk_charges(thing)].name,
                 tables_get_text(TABLES_TEXT_BONES_SEPARATOR), gfx_object_name(icon));
    else if (type == THING_POTION && icon != ICON_WATER_FLASK && g_inventory_champion_ordinal &&
             champion_get_skill_level(g_inventory_champion_ordinal - 1, SKILL_PRIEST) > 1)
        snprintf(name, PANEL_LINE_SIZE, "%c %s",
                 POTION_POWER_SYMBOL_FIRST + potion_power(thing) / POTION_POWER_PER_LEVEL, gfx_object_name(icon));
    else
        snprintf(name, PANEL_LINE_SIZE, "%s", gfx_object_name(icon));
    switch (type) {
    case THING_WEAPON:
        potential = ATTRIBUTE_POISONED | ATTRIBUTE_BROKEN | ATTRIBUTE_CURSED;
        actual = (weapon_is_poisoned(thing) ? ATTRIBUTE_POISONED : 0) |
                 (weapon_is_broken(thing) ? ATTRIBUTE_BROKEN : 0) |
                 (rec_object_is_cursed(thing) ? ATTRIBUTE_CURSED : 0);
        if (icon >= ICON_TORCH_UNLIT && icon <= ICON_TORCH_LAST && !weapon_charges(thing))
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s", gfx_string(TEXT_BURNT_OUT));
        break;
    case THING_ARMOUR:
        potential = ATTRIBUTE_BROKEN | ATTRIBUTE_CURSED;
        actual =
            (armour_is_broken(thing) ? ATTRIBUTE_BROKEN : 0) | (rec_object_is_cursed(thing) ? ATTRIBUTE_CURSED : 0);
        break;
    case THING_POTION:
        potential = ATTRIBUTE_CONSUMABLE;
        actual = tables_get_object_info(inventory_get_object_info_index(thing))->allowed_slots;
        break;
    case THING_JUNK:
        if (icon <= ICON_COMPASS_NORTH + DIRECTION_WEST)
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s%s", gfx_string(TEXT_PARTY_FACING),
                     gfx_string(tables_get_direction_text(icon)));
        else if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL)
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s", gfx_string(TEXT_WATERSKIN_EMPTY + junk_charges(thing)));
        else {
            potential = ATTRIBUTE_CONSUMABLE;
            actual = tables_get_object_info(inventory_get_object_info_index(thing))->allowed_slots;
        }
        break;
    }
    format_attributes_text(potential, actual, lines[n], PANEL_LINE_SIZE);
    if (lines[n][0])
        n++;
    char a[8], b[8];
    snprintf(lines[n++], PANEL_LINE_SIZE, "%s%s%s%s%s", gfx_string(TEXT_WEIGHS),
             format_number(a, weight / WEIGHT_TENTHS, 0, 0), gfx_string(TEXT_DECIMAL_POINT),
             format_number(b, weight % WEIGHT_TENTHS, 0, 0),
             gfx_string(TEXT_KG_PERIOD)); /* the weight is in tenths of a kilogram */
    return n;
}

/* Prints a description line at (x, *y) and moves *y down; a line longer than 18 characters
   breaks at the last space within the first 18. */
static void print_panel_line(int16_t x, int16_t *y, const char *text)
{
    char buf[PANEL_LINE_SIZE];
    snprintf(buf, sizeof buf, "%.*s", (int)sizeof buf - 1, text);
    for (char *s = buf; *s;) {
        char *rest = s + strlen(s);
        if (strlen(s) > PANEL_LINE_WRAP) {
            char *space = s + PANEL_LINE_WRAP - 1;
            while (space > s && *space != ' ')
                space--;
            *space = 0;
            rest = space + 1;
        }
        text_draw(ui_get_viewport(), x, *y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
        *y += MESSAGE_ROW_HEIGHT;
        s = rest;
    }
}

/* Draws an object icon in an icon zone of the viewport. */
void ui_draw_icon_to_viewport(int16_t icon, int16_t zone)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    Box b;
    if (!ui_get_icon_zone_box(zone, 0, &b))
        return;
    ui_extract_icon(icon, &bm);
    draw_blit(&bm, 0, 0, ui_get_viewport(), &b, COLOR_NONE, 0);
}

/* The rune of a word on a scroll ("FUL", "IR." ...), or -1. *core_length gets the length without the punctuation. */
static int get_rune_of_word(const char *word, size_t length, size_t *core_length)
{
    while (length && (word[length - 1] == '.' || word[length - 1] == ','))
        length--;
    *core_length = length;
    for (int k = 0; k < UI_RUNE_COUNT; k++)
        if (strlen(g_rune_names[k]) == length && !strncmp(word, g_rune_names[k], length))
            return k;
    return -1;
}

/* Copies text to out with the rune symbol after each rune name, after gap. */
static void add_rune_symbols(const char *text, const char *gap, char *out, size_t size)
{
    size_t n = 0;
    out[0] = 0;
    for (const char *word = text; *word;) {
        size_t length = strcspn(word, " "), core;
        int rune = get_rune_of_word(word, length, &core);
        if (rune < 0)
            n += (size_t)snprintf(out + n, size - n, "%.*s", (int)length, word);
        else
            n +=
                (size_t)snprintf(out + n, size - n, "%.*s%s%c%s", (int)core, word, gap, RUNE_FIRST + rune, word + core);
        if (n >= size)
            return;
        word += length;
        while (*word == ' ' && n + 1 < size) {
            out[n++] = *word++;
            out[n] = 0;
        }
    }
}

/* Prints one scroll line centered on x in the scroll font: letters and '{' to '~' have their own codes.
   With the advanced features each rune name gets its symbol in red. */
static void draw_scroll_line(int16_t x, int16_t y, char *text)
{
    char runes[SCROLL_TEXT_SIZE];
    int width, height;
    if (g_advanced_features) {
        add_rune_symbols(text, " ", runes, sizeof runes);
        if (strlen(runes) > SCROLL_LINE_CHARS)
            add_rune_symbols(text, "", runes, sizeof runes);
        text = runes;
    }
    for (char *c = text; *c; c++) {
        if (*c >= 'A' && *c <= 'Z')
            *c = (char)(*c - 'A' + 1);
        else if (*c >= '{')
            *c = (char)(*c - SCROLL_FONT_SHIFT);
    }
    if (!text_get_size(text, &width, &height))
        return;
    int16_t left = x - (width >> 1);
    text_draw(ui_get_viewport(), left, y, COLOR_BLACK, COLOR_WHITE, text);
    for (int i = 0; g_advanced_features && text[i]; i++) {
        char glyph[2] = {text[i], 0};
        if ((uint8_t)text[i] >= RUNE_FIRST && (uint8_t)text[i] < RUNE_FIRST + UI_RUNE_COUNT)
            text_draw(ui_get_viewport(), left + i * TEXT_CHAR_WIDTH, y, COLOR_RED, COLOR_WHITE, glyph);
    }
}

/* CSB: each CREANAME in the text becomes the name of the creatures ahead of the party, or NO CREATURE. */
static void insert_creature_name(char *text, size_t size)
{
    const char *name = NULL;
    size_t length = strlen(CREATURE_NAME_TEXT);
    for (char *match = strstr(text, CREATURE_NAME_TEXT); match; match = strstr(match, CREATURE_NAME_TEXT)) {
        if (!name) {
            int16_t x = g_party_map_x;
            int16_t y = g_party_map_y;
            game_update_map_coordinates_after_relative_movement(g_party_direction, 1, 0, &x, &y);
            uint16_t group = map_get_square_group(x, y);
            name = gfx_string(group == THING_ENDOFLIST ? TEXT_NO_CREATURE
                                                       : TEXT_CREATURE_NAME_FIRST + rec_group_creature_type(group));
            scroll_redraw = 1;
        }
        size_t rest = strlen(match + length);
        size_t n = strlen(name);
        if ((size_t)(match - text) + n + rest >= size)
            n = size - 1 - (size_t)(match - text) - rest;
        memmove(match + n, match + length, rest + 1);
        memcpy(match, name, n);
        match += n;
    }
}

/* CSB: the magic map panel when the first scroll line is MAGICMAP: the runes of the active map spells lit,
   and the map grid. Returns false for another scroll. */
static bool draw_panel_magic_map(const char *text)
{
    char line[SCROLL_TEXT_SIZE];
    snprintf(line, sizeof line, "%.*s", (int)strcspn(text, "\n"), text);
    if (!magic_map_is_text(line))
        return false;
    const uint8_t *spells = g_champions[g_inventory_champion_ordinal - 1].magic_map;
    static const int8_t rune_spell[MAGIC_MAP_RUNE_COUNT] = {MAGIC_MAP_KU, MAGIC_MAP_ROS, MAGIC_MAP_DAIN,
                                                            MAGIC_MAP_NETA};
    Bitmap *viewport = ui_get_viewport();
    int16_t x, y;
    scroll_redraw = 1;
    ui_blit_graphic(viewport, GFX_PANEL_MAGIC_MAP, ZONE_PANEL, COLOR_RED);
    for (int16_t i = 0; i < MAGIC_MAP_RUNE_COUNT; i++) {
        char rune[2] = {(char)(MAGIC_MAP_RUNE_FIRST + i), 0};
        ui_print_zone(viewport, ZONE_MAGIC_MAP_RUNE_FIRST + i, spells[rune_spell[i]] ? COLOR_RED : COLOR_BLACK,
                      COLOR_WHITE, rune);
    }
    get_zone_point(ZONE_MAGIC_MAP, &x, &y);
    view_draw_magic_map(viewport, spells, x, y);
    return true;
}

/* Draws the open scroll with its text, vertically centered. */
static void draw_panel_scroll(uint16_t scroll)
{
    char text[SCROLL_TEXT_SIZE];
    int16_t x, y, count = 1;
    dungeon_decode_text(text, sizeof text, THING_MAKE(THING_TEXT, scroll_text_index(scroll)),
                        TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY);
    if (g_adventure == ADVENTURE_CSB) {
        insert_creature_name(text, sizeof text);
        if (draw_panel_magic_map(text))
            return;
    }
    char *rest = strchr(text, '\n');
    if (rest) {
        *rest++ = 0;
        char *end = rest + strlen(rest);
        for (const char *c = rest; *c; c++)
            count += *c == '\n';
        /* the count reads the NUL of the first line when the rest is empty, as the original does */
        if (end[-1] != '\n')
            count++;
        else if (end[-2] == '\n')
            count--;
    } else {
        rest = text + strlen(text);
    }
    ui_blit_graphic(ui_get_viewport(), GFX_PANEL_SCROLL, ZONE_PANEL, COLOR_RED);
    get_zone_point(ZONE_PANEL_SCROLL_CENTER, &x, &y);
    y -= ((MESSAGE_ROW_HEIGHT * count - TEXT_BASELINE_GAP - 1) >> 1) - TEXT_LINE_HEIGHT;
    draw_scroll_line(x, y, text);
    while (*rest) {
        char *nl = strchr(rest, '\n');
        y += MESSAGE_ROW_HEIGHT;
        if (nl)
            *nl = 0;
        draw_scroll_line(x, y, rest);
        rest = nl ? nl + 1 : rest + strlen(rest);
    }
}

/* The largest value of a field over the entries of a rule table, at least 1. */
static uint16_t largest_attack(void)
{
    uint16_t most = 1;
    for (int i = 0; i < WEAPON_TYPE_COUNT; i++)
        most = tables_get_weapon_info(i)->strength > most ? tables_get_weapon_info(i)->strength : most;
    return most;
}

static uint16_t get_largest_defense(void)
{
    uint16_t most = 1;
    for (int i = 0; i < ARMOUR_TYPE_COUNT; i++)
        most = tables_get_armour_info(i)->defense > most ? tables_get_armour_info(i)->defense : most;
    return most;
}

static uint16_t get_largest_weight(void)
{
    uint16_t most = 1;
    for (int i = 0; i < WEAPON_TYPE_COUNT; i++)
        most = tables_get_weapon_info(i)->weight > most ? tables_get_weapon_info(i)->weight : most;
    for (int i = 0; i < ARMOUR_TYPE_COUNT; i++)
        most = tables_get_armour_info(i)->weight > most ? tables_get_armour_info(i)->weight : most;
    for (int i = 0; i < JUNK_TYPE_COUNT; i++)
        most = tables_get_junk_weight(i) > most ? tables_get_junk_weight(i) : most;
    return most;
}

/* One stat meter at baseline y: the label, a bar with a black shadow, and the value when show_value is set. */
static void draw_stat_meter(int16_t x, int16_t y, const char *label, uint16_t value, uint16_t maximum, uint8_t color,
                            bool show_value)
{
    Bitmap *viewport = ui_get_viewport();
    char text[8];
    int16_t bar_x = x + METER_LABEL_CHARS * TEXT_CHAR_WIDTH;
    int16_t top = y - METER_BAR_ABOVE_BASELINE;
    int16_t filled = value >= maximum ? METER_BAR_WIDTH : value * METER_BAR_WIDTH / maximum;
    if (value && !filled)
        filled = 1; /* a small value still shows */
    text_draw(viewport, x, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, label);
    Box shadow = {bar_x + 1, top + 1, filled, METER_BAR_HEIGHT};
    Box bar = {bar_x, top, filled, METER_BAR_HEIGHT};
    if (filled) {
        draw_fill(viewport, &shadow, COLOR_BLACK);
        draw_fill(viewport, &bar, color);
    }
    if (!show_value)
        return;
    snprintf(text, sizeof text, "%3u", value);
    text_draw(viewport, bar_x + METER_BAR_WIDTH + METER_VALUE_GAP, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, text);
}

bool panel_draw_object_meter(uint16_t thing, const char **label, uint16_t *value)
{
    if (THING_TYPE(thing) == THING_WEAPON) {
        *label = "ATTACK";
        *value = tables_get_weapon_info(weapon_type(thing))->strength;
        return true;
    }
    if (THING_TYPE(thing) == THING_ARMOUR) {
        *label = "DEFENSE";
        *value = tables_get_armour_info(armour_type(thing))->defense;
        return true;
    }
    return false;
}

/* Advanced feature: attack (weapons), defense (armour), and weight meters below the description. */
static void draw_stat_meters(uint16_t thing, int16_t x, int16_t y)
{
    const char *label;
    uint16_t value;
    if (panel_draw_object_meter(thing, &label, &value)) {
        bool attack = THING_TYPE(thing) == THING_WEAPON;
        draw_stat_meter(x, y, label, value, attack ? largest_attack() : get_largest_defense(),
                        attack ? COLOR_GOLD : COLOR_BLUE, true);
        y += MESSAGE_ROW_HEIGHT;
    }
    draw_stat_meter(x, y, "WEIGHT", inventory_get_object_weight(thing), get_largest_weight(), COLOR_LIGHT_BROWN, false);
}

/* Opens a chest in the panel and draws its slots. While the eye is pressed, the action hand keeps
   its icon. */
static void open_and_draw_chest(uint16_t chest, int16_t pressing_eye)
{
    if (g_open_chest == chest)
        return;
    inventory_close_chest();
    inventory_open_chest(chest);
    if (!pressing_eye)
        ui_draw_icon_in_slot_box(SLOT_BOX_ACTION_HAND, ICON_CHEST_OPEN);
    ui_blit_graphic(ui_get_viewport(), GFX_PANEL_CHEST, ZONE_PANEL, COLOR_RED);
    for (int16_t i = 0; i < SLOT_CHEST_COUNT; i++) {
        uint16_t thing = g_open_chest_slots[i];
        ui_draw_icon_in_slot_box(SLOT_BOX_CHEST_FIRST + i, thing == THING_NONE ? -1 : object_get_icon_index(thing));
    }
}

void ui_draw_panel_object(uint16_t thing, int16_t pressing_eye)
{
    Bitmap *viewport = ui_get_viewport();
    scroll_redraw = 0;
    scroll_redraw_eye = pressing_eye;
    inventory_close_chest();
    if (THING_TYPE(thing) == THING_SCROLL) {
        draw_panel_scroll(thing);
    } else if (THING_TYPE(thing) == THING_CONTAINER) {
        open_and_draw_chest(thing, pressing_eye);
    } else {
        char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT][PANEL_LINE_SIZE];
        int16_t count = panel_draw_object_text(thing, name, lines), x, y;
        ui_blit_graphic(viewport, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
        ui_blit_graphic(viewport, GFX_PANEL_OBJECT_CIRCLE, ZONE_PANEL_OBJECT_CIRCLE, COLOR_DARKEST_GRAY);
        ui_print_zone(viewport, ZONE_PANEL_OBJECT_NAME, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, name);
        ui_draw_icon_to_viewport(object_get_icon_index(thing), ZONE_PANEL_OBJECT_ICON);
        get_zone_point(ZONE_PANEL_OBJECT_TEXT, &x, &y);
        y = PANEL_OBJECT_TEXT_Y;
        for (int16_t i = 0; i < count; i++)
            print_panel_line(x, &y, lines[i]);
        if (g_advanced_features)
            draw_stat_meters(thing, x, y);
    }
    ui_blit_graphic(viewport, pressing_eye ? GFX_PANEL_EYE : GFX_PANEL_ARROW, ZONE_PANEL_ARROW_OR_EYE, COLOR_RED);
}

void ui_draw_panel_eye(void)
{
    if (!g_leader_empty_handed)
        ui_draw_panel_object(g_leader_hand_object, 1);
    else
        ui_draw_champion_skills_and_statistics();
}

void panel_redraw_scroll(void)
{
    if (!scroll_redraw)
        return;
    if (scroll_redraw_eye) {
        ui_draw_panel_eye();
        ui_draw_viewport();
        return;
    }
    g_champions[g_inventory_champion_ordinal - 1].attributes |= CHAMPION_ATTR_PANEL;
    ui_draw_champion_state(g_inventory_champion_ordinal - 1);
}

void panel_click_in_chest(int16_t x, int16_t y)
{
    if (g_panel_content != PANEL_CONTENT_CHEST || g_leader_index == -1)
        return;
    int16_t command = ui_command_from_mouse(TABLES_MOUSE_CHEST, x, y, UI_BUTTON_LEFT);
    if (!command)
        return;
    uint16_t slot_box = command - COMMAND_CLICK_SLOT_BOX_FIRST;
    inventory_click_on_slot_box(slot_box);
    ui_draw_champion_slot(g_inventory_champion_ordinal - 1, slot_box - UI_SLOT_BOX_INVENTORY_FIRST);
    ui_draw_viewport();
}
