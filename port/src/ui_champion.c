#include <stdio.h>
#include <string.h>
#include "advanced.h"
#include "adventure.h"
#include "champion.h"
#include "command.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "message.h"
#include "magic.h"
#include "map.h"
#include "recruit.h"
#include "tables.h"
#include "text.h"
#include "ui.h"
#include "view.h"

/* KAOS data addresses. */
#define CHAMPION_COLORS 0x100ce0         /* g_champion_colors[4] */
#define INVISIBILITY_PALETTE 0x10247a    /* g_invisibility_palette_changes[16] */
#define SKILL_LEVEL_STRINGS 0x101e74     /* g_skill_level_string_indices[15], level 2-16 */
#define BASE_SKILL_STRINGS 0x102472      /* g_base_skill_string_indices[4] */
#define STATISTIC_STRINGS 0x101e9a       /* g_statistic_string_indices[7] */
#define PORTRAIT_SIZE 0x102958           /* g_portrait_width, g_portrait_height */
#define CHAMPION_ICON_SIZE 0x10295c      /* g_champion_icon_width, g_champion_icon_height */
#define BAR_SHADOW 0x102980              /* offset of the panel bar shadow */
#define DIRECTION_STRINGS 0x101e92       /* g_direction_string_indices[4] */
#define LIST_SEPARATOR 0x101ebe          /* ", " */
#define BONES_SEPARATOR 0x101ec6         /* " " between champion name and "BONES" */
#define ATTRIBUTES_SUFFIX 0x101ec8       /* ")" */
#define ATTRIBUTES_PREFIX 0x101eca       /* "(" */

#define BAR_COUNT 3                      /* health, stamina, mana */
#define BAR_ZONE_STEP 4
#define SKILL_LEVEL_MAXIMUM 16
#define BASE_SKILL_COUNT 4
#define FOOD_WATER_MINIMUM (-1024)
#define FOOD_WATER_RANGE 3072            /* -1024 to 2048 */
#define FOOD_WATER_LOW (-512)
#define LOAD_WARNING_NUMERATOR 5         /* yellow above 5/8 of the maximum load */
#define LOAD_WARNING_DENOMINATOR 8
#define VALUE_WIDTH 3
#define PANEL_OBJECT_TEXT_Y 87           /* baseline of the first description line */
#define PANEL_LINE_WRAP 18               /* longer lines break at a space */
#define SCROLL_TEXT_SIZE 200
#define SCROLL_FONT_SHIFT 0x60           /* '{' to '~' print as scroll font codes 27-30 */
#define POTION_POWER_SYMBOL_FIRST '_'    /* + power / 40: the power rune before the name */
#define POTION_POWER_SYMBOL_STEP 40
#define TEXT_MODE_SCROLL 2
#define TEXT_MODE_ANY_VISIBILITY ((int16_t)0x8000)
#define SCROLL_LINE_CHARS 18             /* characters that fit on the parchment */
#define RUNE_FIRST 0x60                  /* font character of the first rune */
#define CREATURE_NAME_TEXT "CREANAME"    /* CSB: scroll text replaced by the creature name ahead */
#define MAGIC_MAP_RUNE_FIRST 0x72        /* font character of the KU rune; ROS, DAIN, NETA follow */
#define MAGIC_MAP_RUNE_COUNT 4
#define METER_LABEL_CHARS 8              /* the bar starts after the longest label */
#define METER_BAR_WIDTH 36
#define METER_BAR_HEIGHT 5
#define METER_VALUE_GAP 4                /* pixels between the bar and the value */
#define METER_BAR_ABOVE_BASELINE 4       /* the bar top is level with the top of the letters */
#define SLOT_BOX_ACTION_HAND (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_HAND_ACTION)
#define SLOT_BOX_CHEST_FIRST (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_CHEST_FIRST)

/* Object attribute bits of the description; the names are strings TEXT_CONSUMABLE + bit. */
enum { ATTRIBUTE_CONSUMABLE = 1, ATTRIBUTE_POISONED = 2, ATTRIBUTE_BROKEN = 4, ATTRIBUTE_CURSED = 8, ATTRIBUTE_COUNT = 4 };

const char *const g_rune_names[UI_RUNE_COUNT] = {
    "LO", "UM", "ON", "EE", "PAL", "MON", "YA", "VI", "OH", "FUL", "DES", "ZO",
    "VEN", "EW", "KATH", "IR", "BRO", "GOR", "KU", "ROS", "DAIN", "NETA", "RA", "SAR",
};

uint8_t g_champion_portraits[4][CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT];
int16_t g_panel_content;
static int16_t scroll_redraw;     /* CSB: the panel scroll shows a creature name or the magic map, so it draws again each tick */
static int16_t scroll_redraw_eye; /* the scroll is the leader hand object while the eye is pressed */

/* A number as text: right aligned in width characters, or not padded. */
static const char *number(char *buf, uint16_t value, int pad, int width)
{
    if (pad) snprintf(buf, 8, "%*u", width, value);
    else snprintf(buf, 8, "%u", value);
    return buf;
}

static uint8_t champion_color(int16_t champion_index) { return kaos_u8(CHAMPION_COLORS + champion_index); }

/* Draws the health, stamina, and mana bars of the status box: the empty top part dark gray, the
   rest in the champion color. */
void champion_draw_bar_graphs(int16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    const int16_t v[BAR_COUNT][2] = { { c->current_health, c->maximum_health }, { c->current_stamina, c->maximum_stamina }, { c->current_mana, c->maximum_mana } };
    Bitmap screen = ui_screen();
    for (int k = 0; k < BAR_COUNT; k++) {
        Box bar, empty;
        if (!ui_zone_box((int16_t)(ZONE_BAR_FIRST + BAR_ZONE_STEP * k + champion_index), &bar)) continue;
        if (v[k][0] < v[k][1]) {
            empty = bar;
            if (v[k][0]) {
                int16_t filled = (int16_t)((int32_t)v[k][0] * empty.h / v[k][1]);
                empty.h -= filled > 1 ? filled : 1;
            }
            if (empty.h > 0) {
                draw_fill(&screen, &empty, COLOR_DARKEST_GRAY);
                bar.y = (int16_t)(empty.h + empty.y);
                bar.h -= empty.h;
            }
        }
        if (v[k][0] && bar.h > 0) draw_fill(&screen, &bar, champion_color(champion_index));
    }
}

/* Prints "current/maximum" in the inventory. */
static void value_pair(int16_t zone, int16_t value, int16_t maximum)
{
    char a[8], b[8], s[16];
    snprintf(s, sizeof s, "%s/%s", number(a, (uint16_t)value, 1, VALUE_WIDTH), number(b, (uint16_t)maximum, 1, VALUE_WIDTH));
    ui_print_zone(ui_viewport(), zone, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
}

/* Draws one slot: a hand in the status box, or any slot (and the chest slots) in the inventory of
   the champion. Empty body slots show their empty icon; wounded body slots get a red border, the
   action hand of the acting champion the acting border. */
void champion_draw_slot(uint16_t champion_index, uint16_t slot)
{
    const Champion *c = &g_champions[champion_index];
    int inv = champion_index + 1 == g_inventory_champion_ordinal;
    int16_t slot_box, border = -1, icon;
    Bitmap screen = ui_screen();
    Box b;
    if (inv) {
        slot_box = (int16_t)(slot + UI_SLOT_BOX_INVENTORY_FIRST);
    } else {
        if (slot > SLOT_HAND_ACTION || champion_index + 1 == g_candidate_champion_ordinal) return;
        slot_box = (int16_t)(slot + champion_index * 2);
    }
    uint16_t thing = slot < SLOT_COUNT ? c->slots[slot] : g_open_chest_slots[slot - SLOT_CHEST_FIRST];
    int wounded = slot < UI_BODY_SLOT_COUNT && (c->wounds & 1 << slot);
    if (thing == THING_NONE) {
        if (slot < UI_BODY_SLOT_COUNT) {
            icon = (int16_t)(ICON_EMPTY_BODY_SLOT_FIRST + slot * 2 + wounded);
            border = wounded ? GFX_SLOT_BOX_WOUNDED : GFX_SLOT_BOX_NORMAL;
        } else if (slot >= SLOT_NECK && slot <= SLOT_BACKPACK_FIRST) {
            icon = (int16_t)(ICON_EMPTY_NECK_POUCH_BASE + slot);
        } else {
            icon = ICON_EMPTY_SLOT;
        }
    } else {
        icon = object_get_icon_index(thing);
        if (inv && slot == SLOT_HAND_ACTION && icon == ICON_CHEST_CLOSED) icon = ICON_CHEST_OPEN;
        if (slot < UI_BODY_SLOT_COUNT) border = wounded ? GFX_SLOT_BOX_WOUNDED : GFX_SLOT_BOX_NORMAL;
    }
    if (slot == SLOT_HAND_ACTION && champion_index + 1 == g_acting_champion_ordinal) border = GFX_SLOT_BOX_ACTING_HAND;
    if (border != -1 && ui_slot_box_zone(slot_box, 1, &b)) ui_blit_graphic_box(inv ? ui_viewport() : &screen, border, &b, COLOR_DARKEST_GRAY);
    ui_draw_icon_in_slot_box(slot_box, icon);
}

/* Shows the damage a champion took: a splat with the number on the status box, or on the
   portrait while the inventory of the champion is open. */
void champion_draw_damage(int16_t champion_index, int16_t damage)
{
    Bitmap screen = ui_screen();
    char buf[8];
    int inv = champion_index + 1 == g_inventory_champion_ordinal;
    int16_t zone = (int16_t)(champion_index + (inv ? ZONE_PORTRAIT_DAMAGE_FIRST : ZONE_STATUS_BOX_DAMAGE_FIRST));
    ui_blit_graphic(&screen, inv ? GFX_DAMAGE_PORTRAIT : GFX_DAMAGE_STATUS_BOX, zone, COLOR_FLESH);
    ui_print_zone(&screen, zone, COLOR_WHITE, COLOR_RED, number(buf, (uint16_t)damage, 0, VALUE_WIDTH));
    champion_draw_state((uint16_t)champion_index);
}

/* Draws the party icon of a champion (top right, at its cell, facing its direction relative to
   the party); light gray and recolored while the party is invisible. */
static void draw_icon(uint16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    int16_t w = kaos_s16(CHAMPION_ICON_SIZE), h = kaos_s16(CHAMPION_ICON_SIZE + 2);
    uint8_t px[64 * 64];
    Bitmap bm = { (uint16_t)w, (uint16_t)h, px }, screen = ui_screen();
    const Bitmap *icons = gfx_bitmap(GFX_CHAMPION_ICONS);
    int invisible = g_party.event71_count_invisibility != 0;
    if (w * h > (int)sizeof px) return;
    int16_t cell = (int16_t)NORMALIZE(c->cell + 4 - g_party_direction);
    memset(px, invisible ? COLOR_DARK_GRAY : champion_color((int16_t)champion_index), sizeof px);
    if (icons) draw_blit(icons, (int16_t)(NORMALIZE(c->direction + 4 - g_party_direction) * w), 0, &bm, &(Box){ 0, 0, w, h }, COLOR_DARKEST_GRAY, 0);
    if (invisible)
        for (int i = 0; i < w * h; i++) px[i] = kaos_u8(INVISIBILITY_PALETTE + (px[i] & 15));
    ui_fill_zone(&screen, (int16_t)(ZONE_CHAMPION_ICON_FIRST + cell), COLOR_BLACK);
    ui_blit_zone(&screen, &bm, (int16_t)(ZONE_CHAMPION_ICON_FIRST + cell), COLOR_NONE);
}

Bitmap champion_portrait(int16_t champion_index)
{
    return (Bitmap){ CHAMPION_PORTRAIT_WIDTH, CHAMPION_PORTRAIT_HEIGHT, g_champion_portraits[champion_index] };
}

void champion_set_portrait(int16_t champion_index, int16_t portrait_index)
{
    const Bitmap *portraits = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    Bitmap portrait = champion_portrait(champion_index);
    Box b = { 0, 0, CHAMPION_PORTRAIT_WIDTH, CHAMPION_PORTRAIT_HEIGHT };
    int16_t x, y;
    memset(portrait.px, 0, CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT);
    recruit_portrait_origin(portrait_index, &x, &y);
    if (portraits) draw_blit(portraits, x, y, &portrait, &b, COLOR_NONE, 0);
}

/* Draws the portrait in the status box while the inventory is open. */
void draw_status_box_portrait(int16_t champion_index)
{
    Bitmap screen = ui_screen(), portrait = champion_portrait(champion_index);
    int16_t zone = (int16_t)(ZONE_STATUS_BOX_PORTRAIT_FIRST + champion_index);
    Box b;
    if (!ui_zone_box(zone, &b)) return;
    b.w = kaos_s16(PORTRAIT_SIZE);
    b.h = kaos_s16(PORTRAIT_SIZE + 2);
    draw_blit(&portrait, 0, 0, &screen, &b, COLOR_NONE, 0);
    if (g_party.event71_count_invisibility) ui_fill_zone_pattern(&screen, zone, COLOR_DARKEST_GRAY);
}

/* Draws the inventory statistics: health, stamina, mana values, and the mouth and eye borders
   (red when hungry, thirsty, or poisoned, and when a statistic is below its maximum). */
static void draw_inventory_statistics(uint16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap *vp = ui_viewport();
    Box b;
    value_pair(ZONE_HEALTH_VALUE, c->current_health, c->maximum_health);
    value_pair(ZONE_HEALTH_VALUE + 1, (int16_t)(c->current_stamina / 10), (int16_t)(c->maximum_stamina / 10)); /* stamina shows in tenths */
    value_pair(ZONE_HEALTH_VALUE + 2, c->current_mana, c->maximum_mana);
    int16_t mouth = c->food < 0 || c->water < 0 || c->poison_event_count ? GFX_SLOT_BOX_WOUNDED : GFX_SLOT_BOX_NORMAL, eye = GFX_SLOT_BOX_NORMAL;
    if (ui_icon_zone_box(ZONE_MOUTH, 1, &b)) ui_blit_graphic_box(vp, mouth, &b, COLOR_DARKEST_GRAY);
    for (int s = STAT_STRENGTH; s <= STAT_ANTIFIRE; s++)
        if (c->statistics[s][STAT_CURRENT] < c->statistics[s][STAT_MAXIMUM]) {
            eye = GFX_SLOT_BOX_WOUNDED;
            break;
        }
    if (ui_icon_zone_box(ZONE_EYE, 1, &b)) ui_blit_graphic_box(vp, eye, &b, COLOR_DARKEST_GRAY);
}

/* Draws the load line "LOAD nn.n/ mm KG": red above the maximum load, yellow above 5/8 of it. */
static void draw_load(uint16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t max = champion_get_maximum_load(c);
    uint8_t color = max < c->load ? COLOR_RED : (int32_t)max * LOAD_WARNING_NUMERATOR < (int32_t)c->load * LOAD_WARNING_DENOMINATOR ? COLOR_YELLOW : COLOR_LIGHT_GRAY;
    char text[40], a[8], b[8], d[8]; /* the load is in tenths of a kilogram */
    ui_print_zone(ui_viewport(), ZONE_LOAD_LABEL, color, COLOR_DARKEST_GRAY, ui_string(TEXT_LOAD));
    snprintf(text, sizeof text, "%s%s%s/%s%s", number(a, c->load / 10, 1, VALUE_WIDTH), ui_string(TEXT_DECIMAL_POINT), number(d, c->load % 10, 0, 1),
             number(b, (uint16_t)(max + 5) / 10, 1, VALUE_WIDTH), ui_string(TEXT_KG));
    ui_print_zone(ui_viewport(), ZONE_LOAD_VALUE, color, COLOR_DARKEST_GRAY, text);
}

/* Draws the status box of a living champion: background, shields, and the parts that follow. */
static uint16_t draw_status_box(uint16_t champion_index, uint16_t attr, int inv)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap screen = ui_screen();
    int16_t shields[3], n = 0;
    Box box;
    ui_zone_box((int16_t)(ZONE_STATUS_BOX_FIRST + champion_index), &box);
    draw_fill(&screen, &box, COLOR_DARKEST_GRAY);
    if (g_party.fire_shield_defense > 0) shields[n++] = GFX_SHIELD_FIRE;
    if (g_party.spell_shield_defense > 0) shields[n++] = GFX_SHIELD_SPELL;
    if (g_party.shield_defense > 0 || c->shield_defense) shields[n++] = GFX_SHIELD_PARTY;
    while (n--) ui_blit_graphic_box(&screen, shields[n], &box, COLOR_FLESH);
    if (inv) {
        draw_status_box_portrait((int16_t)champion_index);
        return attr | CHAMPION_ATTR_STATISTICS;
    }
    return attr | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS | CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_NAME_TITLE;
}

/* Draws the name in the status box (gold for the leader), or name and title in the inventory. */
static uint16_t draw_name(uint16_t champion_index, uint16_t attr, int inv)
{
    const Champion *c = &g_champions[champion_index];
    Bitmap screen = ui_screen();
    uint8_t color = (int16_t)champion_index == g_leader_index ? COLOR_GOLD : COLOR_LIGHT_GRAY;
    if (!inv) {
        ui_fill_zone(&screen, (int16_t)(ZONE_STATUS_BOX_NAME_FIRST + champion_index), COLOR_DARK_GRAY);
        ui_print_zone(&screen, (int16_t)(ZONE_STATUS_BOX_NAME_TEXT_FIRST + champion_index), color, COLOR_DARK_GRAY, c->name);
        return attr;
    }
    char s[40];
    int joined = c->title[0] && strchr(",;-", c->title[0]); /* "NAME, TITLE" without a space */
    snprintf(s, sizeof s, "%.8s%s%.20s", c->name, joined ? "" : " ", c->title);
    ui_print_zone(ui_viewport(), ZONE_NAME_TITLE, color, COLOR_DARKEST_GRAY, s);
    return attr | CHAMPION_ATTR_VIEWPORT;
}

/* Draws the parts of a champion that have a redraw flag in its attributes, then clears the flags. */
void champion_draw_state(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    uint16_t attr = c->attributes;
    Bitmap screen = ui_screen();
    int inv = champion_index + 1 == g_inventory_champion_ordinal;
    if (!(attr & CHAMPION_ATTR_REDRAW_MASK)) return;
    if (attr & CHAMPION_ATTR_STATUS_BOX) {
        if (c->current_health) {
            attr = draw_status_box(champion_index, attr, inv);
        } else {
            Box box;
            if (ui_zone_box((int16_t)(ZONE_STATUS_BOX_FIRST + champion_index), &box)) ui_blit_graphic_box(&screen, GFX_STATUS_BOX_DEAD, &box, COLOR_NONE);
            ui_print_zone(&screen, (int16_t)(ZONE_STATUS_BOX_NAME_TEXT_FIRST + champion_index), COLOR_LIGHT_GRAY, COLOR_DARK_GRAY, c->name);
            menus_draw_action_icon(champion_index);
        }
    }
    if (c->current_health) {
        if (attr & CHAMPION_ATTR_NAME_TITLE) attr = draw_name(champion_index, attr, inv);
        if (attr & CHAMPION_ATTR_STATISTICS) {
            champion_draw_bar_graphs((int16_t)champion_index);
            if (inv) {
                draw_inventory_statistics(champion_index);
                attr |= CHAMPION_ATTR_VIEWPORT;
            }
        }
        if (attr & CHAMPION_ATTR_WOUNDS) {
            if (inv) attr |= CHAMPION_ATTR_VIEWPORT;
            for (int s = inv ? UI_BODY_SLOT_COUNT - 1 : SLOT_HAND_ACTION; s >= 0; s--) champion_draw_slot(champion_index, (uint16_t)s);
        }
        if (attr & CHAMPION_ATTR_LOAD && inv) {
            draw_load(champion_index);
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ICON) draw_icon(champion_index);
        if (attr & CHAMPION_ATTR_PANEL && inv) {
            draw_panel();
            attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_ACTION_HAND) {
            champion_draw_slot(champion_index, SLOT_HAND_ACTION);
            menus_draw_action_icon(champion_index);
            if (inv) attr |= CHAMPION_ATTR_VIEWPORT;
        }
        if (attr & CHAMPION_ATTR_VIEWPORT) ui_draw_viewport();
    }
    c->attributes &= CHAMPION_ATTR_KEEP_MASK;
}

/* Sets redraw flags on all champions and draws them. */
void champion_draw_all_states(uint16_t attributes)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        g_champions[i].attributes |= attributes;
        champion_draw_state(i);
    }
}

/* Draws a food or water bar with a black shadow: length by value (-1024 to 2048), yellow below
   0, red below -512. */
static void food_water_bar(int16_t value, int16_t zone, uint8_t color)
{
    Box b;
    if (value < FOOD_WATER_LOW) color = COLOR_RED;
    else if (value < 0) color = COLOR_YELLOW;
    if (!layout_get_zone_box(zone, (int16_t)((int32_t)(value - FOOD_WATER_MINIMUM) * 10000 / FOOD_WATER_RANGE), 10000, &b)) return;
    Box shadow = b;
    shadow.x += kaos_s16(BAR_SHADOW);
    shadow.y += kaos_s16(BAR_SHADOW);
    draw_fill(ui_viewport(), &shadow, COLOR_BLACK);
    draw_fill(ui_viewport(), &b, color);
}

/* Draws the food and water panel of the inventory, with "POISONED" when poisoned. */
void draw_panel_food_water_poisoned(void)
{
    scroll_redraw = 0;
    const Champion *c = &g_champions[g_inventory_champion_ordinal - 1];
    Bitmap *vp = ui_viewport();
    close_chest();
    ui_blit_graphic(vp, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    ui_blit_graphic(vp, GFX_PANEL_FOOD_LABEL, ZONE_PANEL_FOOD_LABEL, COLOR_DARKEST_GRAY);
    ui_blit_graphic(vp, GFX_PANEL_WATER_LABEL, ZONE_PANEL_WATER_LABEL, COLOR_DARKEST_GRAY);
    if (c->poison_event_count) ui_blit_graphic(vp, GFX_PANEL_POISONED_LABEL, ZONE_PANEL_POISONED_LABEL, COLOR_DARKEST_GRAY);
    food_water_bar(c->food, ZONE_PANEL_FOOD_BAR, COLOR_LIGHT_BROWN);
    food_water_bar(c->water, ZONE_PANEL_WATER_BAR, COLOR_BLUE);
}

/* Draws the inventory panel: resurrect/reincarnate for a candidate, the open scroll or chest in
   the action hand, else food and water. */
void draw_panel(void)
{
    scroll_redraw = 0;
    close_chest();
    if (g_candidate_champion_ordinal) {
        g_panel_content = PANEL_CONTENT_RESURRECT;
        ui_blit_graphic(ui_viewport(), GFX_PANEL_RESURRECT_REINCARNATE, ZONE_PANEL, COLOR_DARK_GREEN);
        return;
    }
    uint16_t thing = g_champions[g_inventory_champion_ordinal - 1].slots[SLOT_HAND_ACTION];
    g_panel_content = thing == THING_NONE ? PANEL_CONTENT_FOOD_WATER
                    : THING_TYPE(thing) == THING_SCROLL ? PANEL_CONTENT_SCROLL
                    : THING_TYPE(thing) == THING_CONTAINER ? PANEL_CONTENT_CHEST : PANEL_CONTENT_FOOD_WATER;
    if (g_panel_content == PANEL_CONTENT_FOOD_WATER) draw_panel_food_water_poisoned();
    else draw_panel_object(thing, 0);
}

/* The position of zone (a 1 x 1 box). */
static void zone_point(int16_t zone, int16_t *x, int16_t *y)
{
    int16_t w = 1, h = 1;
    Box b = { 0 };
    layout_get_zone(zone, 0, 0, &b, &w, &h);
    *x = b.x;
    *y = b.y;
}

const char *ui_skill_rank_name(int16_t level)
{
    if (level <= 1) return NULL;
    if (level > SKILL_LEVEL_MAXIMUM) level = SKILL_LEVEL_MAXIMUM;
    return ui_string(kaos_s16(SKILL_LEVEL_STRINGS + 2 * (level - 2)));
}

/* Draws the eye panel: base skill levels above 1 (green after a level up), then the six
   statistics with current value (red below maximum, green above) and maximum value. */
void draw_champion_skills_and_statistics(void)
{
    scroll_redraw = 0;
    int16_t ci = g_inventory_champion_ordinal - 1, x, y, vx;
    const Champion *c = &g_champions[ci];
    Bitmap *vp = ui_viewport();
    char s[40], a[8], b[8];
    close_chest();
    ui_blit_graphic(vp, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    zone_point(ZONE_PANEL_SKILLS, &x, &y);
    for (uint16_t k = 0; k < BASE_SKILL_COUNT; k++) {
        int16_t level = champion_get_skill_level(ci, k | SKILL_IGNORE_TEMPORARY_EXPERIENCE);
        if (level == 1) continue;
        snprintf(s, sizeof s, "%s %s", ui_skill_rank_name(level), ui_string(kaos_s16(BASE_SKILL_STRINGS + 2 * k)));
        text_draw(vp, x, y, g_champion_skill_level_up[ci][k] ? COLOR_GREEN : COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
        y += MESSAGE_ROW_HEIGHT;
    }
    zone_point(ZONE_PANEL_STATISTICS, &vx, &y);
    for (int k = STAT_STRENGTH; k <= STAT_ANTIFIRE; k++) {
        uint8_t cur = c->statistics[k][STAT_CURRENT], max = c->statistics[k][STAT_MAXIMUM];
        text_draw(vp, x, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, ui_string(kaos_s16(STATISTIC_STRINGS + 2 * k)));
        text_draw(vp, vx, y, cur < max ? COLOR_RED : cur > max ? COLOR_GREEN : COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, number(a, cur, 1, VALUE_WIDTH));
        snprintf(s, sizeof s, "/%s", number(b, max, 1, VALUE_WIDTH));
        text_draw(vp, vx + TEXT_CHAR_WIDTH * VALUE_WIDTH, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
        y += MESSAGE_ROW_HEIGHT;
    }
}

/* Opens the inventory of a champion on the screen: dims the movement arrows and draws the
   inventory background, labels, all slots, and the champion state. Sets g_inventory_champion_ordinal. */
void inventory_draw_open(int16_t champion_index)
{
    Bitmap screen = ui_screen(), *vp = ui_viewport();
    const Bitmap *bg = gfx_bitmap(GFX_INVENTORY);
    if (!g_inventory_champion_ordinal) ui_fill_zone_pattern(&screen, ZONE_MOVEMENT_ARROWS, COLOR_BLACK);
    g_inventory_champion_ordinal = (int16_t)(champion_index + 1);
    if (bg) draw_blit(bg, 0, 0, vp, &(Box){ 0, 0, (int16_t)vp->w, (int16_t)vp->h }, COLOR_NONE, 0);
    if (g_candidate_champion_ordinal) { /* a candidate cannot save, sleep, or close */
        ui_fill_zone(vp, ZONE_INVENTORY_SAVE, COLOR_DARKEST_GRAY);
        ui_fill_zone(vp, ZONE_INVENTORY_SLEEP, COLOR_DARKEST_GRAY);
        ui_fill_zone(vp, ZONE_INVENTORY_CLOSE, COLOR_DARKEST_GRAY);
    }
    ui_print_zone(vp, ZONE_HEALTH_LABEL, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, ui_string(TEXT_HEALTH));
    ui_print_zone(vp, ZONE_HEALTH_LABEL + 1, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, ui_string(TEXT_STAMINA));
    ui_print_zone(vp, ZONE_HEALTH_LABEL + 2, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, ui_string(TEXT_MANA));
    for (uint16_t s = 0; s < SLOT_COUNT; s++) champion_draw_slot((uint16_t)champion_index, s);
    g_champions[champion_index].attributes |= CHAMPION_ATTR_INVENTORY_ALL;
    champion_draw_state((uint16_t)champion_index);
}

/* Closes the inventory on the screen: the status box returns and the movement arrows come back.
   The caller redraws the dungeon view. */
void inventory_draw_close(int16_t champion_index)
{
    g_inventory_champion_ordinal = 0;
    close_chest();
    if (g_champions[champion_index].current_health && !g_candidate_champion_ordinal) {
        g_champions[champion_index].attributes |= CHAMPION_ATTR_STATUS_BOX;
        champion_draw_state((uint16_t)champion_index);
    }
    if (!g_party_is_sleeping) menus_draw_movement_arrows();
}

static void append(char *dst, size_t size, const char *s)
{
    size_t n = strlen(dst);
    snprintf(dst + n, size - n, "%s", s);
}

/* The attribute list "(POISONED, BROKEN AND CURSED)" of the bits in both masks, or "" for none. */
static void attributes_text(uint16_t potential, uint16_t actual, char *dst, size_t size)
{
    uint16_t bits = potential & actual;
    int16_t left = 0;
    for (int16_t i = 0; i < ATTRIBUTE_COUNT; i++) left += bits >> i & 1;
    dst[0] = 0;
    if (!left) return;
    append(dst, size, (const char *)kaos_ptr(ATTRIBUTES_PREFIX));
    for (int16_t i = 0; i < ATTRIBUTE_COUNT; i++) {
        if (!(bits >> i & 1)) continue;
        append(dst, size, ui_string((int16_t)(TEXT_CONSUMABLE + i)));
        left--;
        if (left > 1) append(dst, size, (const char *)kaos_ptr(LIST_SEPARATOR));
        else if (left == 1) append(dst, size, ui_string(TEXT_AND));
    }
    append(dst, size, (const char *)kaos_ptr(ATTRIBUTES_SUFFIX));
}

/* The object panel text: the name (bones with the champion name, a potion with its power rune
   for a priest above level 1) and the description lines (burnt out, compass direction, waterskin
   level, attributes, weight). Returns the line count. */
int16_t panel_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE])
{
    int16_t icon = object_get_icon_index(thing), n = 0;
    uint16_t type = THING_TYPE(thing), potential = 0, actual = 0, weight = get_object_weight(thing);
    if (icon == ICON_BONES)
        snprintf(name, PANEL_LINE_SIZE, "%.8s%s%s", g_champions[junk_charges(thing)].name, (const char *)kaos_ptr(BONES_SEPARATOR), ui_object_name(icon));
    else if (type == THING_POTION && icon != ICON_WATER_FLASK && g_inventory_champion_ordinal &&
             champion_get_skill_level((int16_t)(g_inventory_champion_ordinal - 1), SKILL_PRIEST) > 1)
        snprintf(name, PANEL_LINE_SIZE, "%c %s", POTION_POWER_SYMBOL_FIRST + potion_power(thing) / POTION_POWER_SYMBOL_STEP, ui_object_name(icon));
    else
        snprintf(name, PANEL_LINE_SIZE, "%s", ui_object_name(icon));
    switch (type) {
    case THING_WEAPON:
        potential = ATTRIBUTE_POISONED | ATTRIBUTE_BROKEN | ATTRIBUTE_CURSED;
        actual = (weapon_is_poisoned(thing) ? ATTRIBUTE_POISONED : 0) | (weapon_is_broken(thing) ? ATTRIBUTE_BROKEN : 0) |
                 (object_is_cursed(thing) ? ATTRIBUTE_CURSED : 0);
        if (icon >= ICON_TORCH_UNLIT && icon <= ICON_TORCH_LAST && !weapon_charges(thing))
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s", ui_string(TEXT_BURNT_OUT));
        break;
    case THING_ARMOUR:
        potential = ATTRIBUTE_BROKEN | ATTRIBUTE_CURSED;
        actual = (armour_is_broken(thing) ? ATTRIBUTE_BROKEN : 0) | (object_is_cursed(thing) ? ATTRIBUTE_CURSED : 0);
        break;
    case THING_POTION:
        potential = ATTRIBUTE_CONSUMABLE;
        actual = tables_object_info(get_object_info_index(thing))->allowed_slots;
        break;
    case THING_JUNK:
        if (icon <= ICON_COMPASS_NORTH + DIRECTION_WEST)
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s%s", ui_string(TEXT_PARTY_FACING), ui_string(kaos_s16(DIRECTION_STRINGS + 2 * icon)));
        else if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL)
            snprintf(lines[n++], PANEL_LINE_SIZE, "%s", ui_string((int16_t)(TEXT_WATERSKIN_EMPTY + junk_charges(thing))));
        else {
            potential = ATTRIBUTE_CONSUMABLE;
            actual = tables_object_info(get_object_info_index(thing))->allowed_slots;
        }
        break;
    }
    attributes_text(potential, actual, lines[n], PANEL_LINE_SIZE);
    if (lines[n][0]) n++;
    char a[8], b[8];
    snprintf(lines[n++], PANEL_LINE_SIZE, "%s%s%s%s%s", ui_string(TEXT_WEIGHS), number(a, weight / 10, 0, 0), ui_string(TEXT_DECIMAL_POINT),
             number(b, weight % 10, 0, 0), ui_string(TEXT_KG_PERIOD)); /* the weight is in tenths of a kilogram */
    return n;
}

/* Prints a description line at (x, *y) and moves *y down; a line longer than 18 characters
   breaks at the last space within the first 18. */
static void print_panel_line(int16_t x, int16_t *y, const char *text)
{
    char buf[PANEL_LINE_SIZE];
    snprintf(buf, sizeof buf, "%s", text);
    for (char *s = buf; *s;) {
        char *rest = s + strlen(s);
        if (strlen(s) > PANEL_LINE_WRAP) {
            char *space = s + PANEL_LINE_WRAP - 1;
            while (space > s && *space != ' ') space--;
            *space = 0;
            rest = space + 1;
        }
        text_draw(ui_viewport(), x, *y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, s);
        *y += MESSAGE_ROW_HEIGHT;
        s = rest;
    }
}

/* Draws an object icon in an icon zone of the viewport. */
static void draw_icon_to_viewport(int16_t icon, int16_t zone)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = { UI_ICON_SIZE, UI_ICON_SIZE, px };
    Box b;
    if (!ui_icon_zone_box(zone, 0, &b)) return;
    ui_extract_icon(icon, &bm);
    draw_blit(&bm, 0, 0, ui_viewport(), &b, COLOR_NONE, 0);
}

/* The rune of a word on a scroll ("FUL", "IR." ...), or -1. *core_length gets the length without the punctuation. */
static int rune_of_word(const char *word, size_t length, size_t *core_length)
{
    while (length && (word[length - 1] == '.' || word[length - 1] == ',')) length--;
    *core_length = length;
    for (int k = 0; k < UI_RUNE_COUNT; k++)
        if (strlen(g_rune_names[k]) == length && !strncmp(word, g_rune_names[k], length)) return k;
    return -1;
}

/* Copies text to out with the rune symbol after each rune name, after gap. */
static void add_rune_symbols(const char *text, const char *gap, char *out, size_t size)
{
    size_t n = 0;
    out[0] = 0;
    for (const char *word = text; *word;) {
        size_t length = strcspn(word, " "), core;
        int rune = rune_of_word(word, length, &core);
        if (rune < 0) n += (size_t)snprintf(out + n, size - n, "%.*s", (int)length, word);
        else n += (size_t)snprintf(out + n, size - n, "%.*s%s%c%s", (int)core, word, gap, RUNE_FIRST + rune, word + core);
        if (n >= size) return;
        word += length;
        while (*word == ' ' && n + 1 < size) out[n++] = *word++, out[n] = 0;
    }
}

/* Prints one scroll line centered on x in the scroll font: letters and '{' to '~' have their own codes.
   With the advanced features each rune name gets its symbol in red. */
static void draw_scroll_line(int16_t x, int16_t y, char *text)
{
    char runes[SCROLL_TEXT_SIZE];
    int w, h;
    if (g_advanced_features) {
        add_rune_symbols(text, " ", runes, sizeof runes);
        if (strlen(runes) > SCROLL_LINE_CHARS) add_rune_symbols(text, "", runes, sizeof runes);
        text = runes;
    }
    for (char *c = text; *c; c++) {
        if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 1);
        else if (*c >= '{') *c = (char)(*c - SCROLL_FONT_SHIFT);
    }
    if (!text_size(text, &w, &h)) return;
    int16_t left = (int16_t)(x - (w >> 1));
    text_draw(ui_viewport(), left, y, COLOR_BLACK, COLOR_WHITE, text);
    for (int i = 0; g_advanced_features && text[i]; i++) {
        char glyph[2] = { text[i], 0 };
        if ((uint8_t)text[i] >= RUNE_FIRST && (uint8_t)text[i] < RUNE_FIRST + UI_RUNE_COUNT)
            text_draw(ui_viewport(), left + i * TEXT_CHAR_WIDTH, y, COLOR_RED, COLOR_WHITE, glyph);
    }
}

/* CSB: each CREANAME in the text becomes the name of the creatures ahead of the party, or NO CREATURE. */
static void insert_creature_name(char *text, size_t size)
{
    const char *name = NULL;
    size_t length = strlen(CREATURE_NAME_TEXT);
    for (char *at = strstr(text, CREATURE_NAME_TEXT); at; at = strstr(at, CREATURE_NAME_TEXT)) {
        if (!name) {
            int16_t x = g_party_map_x, y = g_party_map_y;
            update_map_coordinates_after_relative_movement(g_party_direction, 1, 0, &x, &y);
            uint16_t group = group_get_thing(x, y);
            name = ui_string(group == THING_ENDOFLIST ? TEXT_NO_CREATURE : (int16_t)(TEXT_CREATURE_NAME_FIRST + group_creature_type(group)));
            scroll_redraw = 1;
        }
        size_t rest = strlen(at + length), n = strlen(name);
        if ((size_t)(at - text) + n + rest >= size) n = size - 1 - (size_t)(at - text) - rest;
        memmove(at + n, at + length, rest + 1);
        memcpy(at, name, n);
        at += n;
    }
}

/* CSB: the magic map panel when the first scroll line is MAGICMAP: the runes of the active map spells lit,
   and the map grid. Returns 0 for another scroll. */
static int draw_panel_magic_map(const char *text)
{
    char line[SCROLL_TEXT_SIZE];
    snprintf(line, sizeof line, "%.*s", (int)strcspn(text, "\n"), text);
    if (!magic_map_is_text(line)) return 0;
    const uint8_t *spells = g_champions[g_inventory_champion_ordinal - 1].magic_map;
    static const int8_t rune_spell[MAGIC_MAP_RUNE_COUNT] = { MAGIC_MAP_KU, MAGIC_MAP_ROS, MAGIC_MAP_DAIN, MAGIC_MAP_NETA };
    Bitmap *vp = ui_viewport();
    int16_t x, y;
    scroll_redraw = 1;
    ui_blit_graphic(vp, GFX_PANEL_MAGIC_MAP, ZONE_PANEL, COLOR_RED);
    for (int16_t i = 0; i < MAGIC_MAP_RUNE_COUNT; i++) {
        char rune[2] = { (char)(MAGIC_MAP_RUNE_FIRST + i), 0 };
        ui_print_zone(vp, (int16_t)(ZONE_MAGIC_MAP_RUNE_FIRST + i), spells[rune_spell[i]] ? COLOR_RED : COLOR_BLACK, COLOR_WHITE, rune);
    }
    zone_point(ZONE_MAGIC_MAP, &x, &y);
    view_draw_magic_map(vp, spells, x, y);
    return 1;
}

/* Draws the open scroll with its text, vertically centered. */
static void draw_panel_scroll(uint16_t scroll)
{
    char text[SCROLL_TEXT_SIZE];
    int16_t x, y, count = 1;
    decode_text(text, sizeof text, (uint16_t)(THING_TEXT << 10 | scroll_text_index(scroll)), TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY);
    if (g_adventure == ADVENTURE_CSB) {
        insert_creature_name(text, sizeof text);
        if (draw_panel_magic_map(text)) return;
    }
    char *rest = strchr(text, '\n');
    if (rest) {
        *rest++ = 0;
        char *end = rest + strlen(rest);
        for (const char *c = rest; *c; c++) count += *c == '\n';
        /* the count reads the NUL of the first line when the rest is empty, as the original does */
        if (end[-1] != '\n') count++;
        else if (end[-2] == '\n') count--;
    } else {
        rest = text + strlen(text);
    }
    ui_blit_graphic(ui_viewport(), GFX_PANEL_SCROLL, ZONE_PANEL, COLOR_RED);
    zone_point(ZONE_PANEL_SCROLL_CENTER, &x, &y);
    y = (int16_t)(y - (((MESSAGE_ROW_HEIGHT * count - TEXT_BASELINE_GAP - 1) >> 1) - TEXT_LINE_HEIGHT));
    draw_scroll_line(x, y, text);
    while (*rest) {
        char *nl = strchr(rest, '\n');
        y += MESSAGE_ROW_HEIGHT;
        if (nl) *nl = 0;
        draw_scroll_line(x, y, rest);
        rest = nl ? nl + 1 : rest + strlen(rest);
    }
}

/* The largest value of a field over the entries of a rule table, at least 1. */
static uint16_t largest_attack(void)
{
    uint16_t most = 1;
    for (int i = 0; i < WEAPON_TYPE_COUNT; i++) most = tables_weapon_info(i)->strength > most ? tables_weapon_info(i)->strength : most;
    return most;
}

static uint16_t largest_defense(void)
{
    uint16_t most = 1;
    for (int i = 0; i < ARMOUR_TYPE_COUNT; i++) most = tables_armour_info(i)->defense > most ? tables_armour_info(i)->defense : most;
    return most;
}

static uint16_t largest_weight(void)
{
    uint16_t most = 1;
    for (int i = 0; i < WEAPON_TYPE_COUNT; i++) most = tables_weapon_info(i)->weight > most ? tables_weapon_info(i)->weight : most;
    for (int i = 0; i < ARMOUR_TYPE_COUNT; i++) most = tables_armour_info(i)->weight > most ? tables_armour_info(i)->weight : most;
    for (int i = 0; i < JUNK_TYPE_COUNT; i++) most = *tables_junk_weight(i) > most ? *tables_junk_weight(i) : most;
    return most;
}

/* One stat meter at baseline y: the label, a bar with a black shadow, and the value when show_value is 1. */
static void draw_stat_meter(int16_t x, int16_t y, const char *label, uint16_t value, uint16_t maximum, uint8_t color, int show_value)
{
    Bitmap *vp = ui_viewport();
    char text[8];
    int16_t bar_x = (int16_t)(x + METER_LABEL_CHARS * TEXT_CHAR_WIDTH), top = (int16_t)(y - METER_BAR_ABOVE_BASELINE);
    int16_t filled = (int16_t)(value >= maximum ? METER_BAR_WIDTH : value * METER_BAR_WIDTH / maximum);
    if (value && !filled) filled = 1; /* a small value still shows */
    text_draw(vp, x, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, label);
    Box shadow = { (int16_t)(bar_x + 1), (int16_t)(top + 1), filled, METER_BAR_HEIGHT };
    Box bar = { bar_x, top, filled, METER_BAR_HEIGHT };
    if (filled) {
        draw_fill(vp, &shadow, COLOR_BLACK);
        draw_fill(vp, &bar, color);
    }
    if (!show_value) return;
    snprintf(text, sizeof text, "%3u", value);
    text_draw(vp, bar_x + METER_BAR_WIDTH + METER_VALUE_GAP, y, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, text);
}

int panel_object_meter(uint16_t thing, const char **label, uint16_t *value)
{
    if (THING_TYPE(thing) == THING_WEAPON) {
        *label = "ATTACK";
        *value = tables_weapon_info(weapon_type(thing))->strength;
        return 1;
    }
    if (THING_TYPE(thing) == THING_ARMOUR) {
        *label = "DEFENSE";
        *value = tables_armour_info(armour_type(thing))->defense;
        return 1;
    }
    return 0;
}

/* Advanced feature: attack (weapons), defense (armour), and weight meters below the description. */
static void draw_stat_meters(uint16_t thing, int16_t x, int16_t y)
{
    const char *label;
    uint16_t value;
    if (panel_object_meter(thing, &label, &value)) {
        int attack = THING_TYPE(thing) == THING_WEAPON;
        draw_stat_meter(x, y, label, value, attack ? largest_attack() : largest_defense(), attack ? COLOR_GOLD : COLOR_BLUE, 1);
        y += MESSAGE_ROW_HEIGHT;
    }
    draw_stat_meter(x, y, "WEIGHT", get_object_weight(thing), largest_weight(), COLOR_LIGHT_BROWN, 0);
}

/* Opens a chest in the panel and draws its slots. While the eye is pressed, the action hand keeps
   its icon. */
static void open_and_draw_chest(uint16_t chest, int16_t pressing_eye)
{
    if (g_open_chest == chest) return;
    close_chest();
    chest_open(chest);
    if (!pressing_eye) ui_draw_icon_in_slot_box(SLOT_BOX_ACTION_HAND, ICON_CHEST_OPEN);
    ui_blit_graphic(ui_viewport(), GFX_PANEL_CHEST, ZONE_PANEL, COLOR_RED);
    for (int16_t i = 0; i < SLOT_CHEST_COUNT; i++) {
        uint16_t thing = g_open_chest_slots[i];
        ui_draw_icon_in_slot_box((int16_t)(SLOT_BOX_CHEST_FIRST + i), thing == THING_NONE ? -1 : object_get_icon_index(thing));
    }
}

/* Draws an object in the panel: the open scroll, the open chest, or the name, icon, and
   description of any other object; then the arrow, or the eye while it is pressed. */
void draw_panel_object(uint16_t thing, int16_t pressing_eye)
{
    Bitmap *vp = ui_viewport();
    scroll_redraw = 0;
    scroll_redraw_eye = pressing_eye;
    close_chest();
    if (THING_TYPE(thing) == THING_SCROLL) {
        draw_panel_scroll(thing);
    } else if (THING_TYPE(thing) == THING_CONTAINER) {
        open_and_draw_chest(thing, pressing_eye);
    } else {
        char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT][PANEL_LINE_SIZE];
        int16_t count = panel_object_text(thing, name, lines), x, y;
        ui_blit_graphic(vp, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
        ui_blit_graphic(vp, GFX_PANEL_OBJECT_CIRCLE, ZONE_PANEL_OBJECT_CIRCLE, COLOR_DARKEST_GRAY);
        ui_print_zone(vp, ZONE_PANEL_OBJECT_NAME, COLOR_LIGHT_GRAY, COLOR_DARKEST_GRAY, name);
        draw_icon_to_viewport(object_get_icon_index(thing), ZONE_PANEL_OBJECT_ICON);
        zone_point(ZONE_PANEL_OBJECT_TEXT, &x, &y);
        y = PANEL_OBJECT_TEXT_Y;
        for (int16_t i = 0; i < count; i++) print_panel_line(x, &y, lines[i]);
        if (g_advanced_features) draw_stat_meters(thing, x, y);
    }
    ui_blit_graphic(vp, pressing_eye ? GFX_PANEL_EYE : GFX_PANEL_ARROW, ZONE_PANEL_ARROW_OR_EYE, COLOR_RED);
}

/* The panel while the eye is pressed: the leader hand object, or the skills and statistics.
   ponytail: the leader hand object name stays; the original clears it until the eye is released. */
void draw_panel_eye(void)
{
    if (!g_leader_empty_handed) draw_panel_object(g_leader_hand_object, 1);
    else draw_champion_skills_and_statistics();
}

/* CSB, each tick with the inventory open: a scroll with a creature name or the magic map draws again. */
void panel_redraw_scroll(void)
{
    if (!scroll_redraw) return;
    if (scroll_redraw_eye) {
        draw_panel_eye();
        ui_draw_viewport();
        return;
    }
    g_champions[g_inventory_champion_ordinal - 1].attributes |= CHAMPION_ATTR_PANEL;
    champion_draw_state((uint16_t)(g_inventory_champion_ordinal - 1));
}

/* A click in the open chest panel swaps the leader hand object with the clicked chest slot. */
void panel_click_in_chest(int16_t x, int16_t y)
{
    if (g_panel_content != PANEL_CONTENT_CHEST || g_leader_index == -1) return;
    int16_t command = ui_command_from_mouse(UI_INPUT_CHEST, x, y, UI_BUTTON_LEFT);
    if (!command) return;
    uint16_t slot_box = (uint16_t)(command - COMMAND_CLICK_SLOT_BOX_FIRST);
    champion_click_on_slot_box(slot_box);
    champion_draw_slot((uint16_t)(g_inventory_champion_ordinal - 1), (uint16_t)(slot_box - UI_SLOT_BOX_INVENTORY_FIRST));
    ui_draw_viewport();
}
