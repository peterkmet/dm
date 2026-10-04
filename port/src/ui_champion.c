#include <stdio.h>
#include <string.h>
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "message.h"
#include "recruit.h"
#include "text.h"
#include "ui.h"

/* KAOS data addresses. */
#define CHAMPION_COLORS 0x100ce0         /* g_champion_colors[4] */
#define INVISIBILITY_PALETTE 0x10247a    /* g_invisibility_palette_changes[16] */
#define SKILL_LEVEL_STRINGS 0x101e74     /* g_skill_level_string_indices[15], level 2-16 */
#define BASE_SKILL_STRINGS 0x102472      /* g_base_skill_string_indices[4] */
#define STATISTIC_STRINGS 0x101e9a       /* g_statistic_string_indices[7] */
#define PORTRAIT_SIZE 0x102958           /* g_portrait_width, g_portrait_height */
#define CHAMPION_ICON_SIZE 0x10295c      /* g_champion_icon_width, g_champion_icon_height */
#define BAR_SHADOW 0x102980              /* offset of the panel bar shadow */

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

int16_t g_champion_portrait_indices[4];

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

/* Draws the portrait in the status box while the inventory is open. */
void draw_status_box_portrait(int16_t champion_index)
{
    Bitmap screen = ui_screen();
    const Bitmap *portraits = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t x, y, zone = (int16_t)(ZONE_STATUS_BOX_PORTRAIT_FIRST + champion_index);
    Box b;
    if (!ui_zone_box(zone, &b)) return;
    recruit_portrait_origin(g_champion_portrait_indices[champion_index], &x, &y);
    b.w = kaos_s16(PORTRAIT_SIZE);
    b.h = kaos_s16(PORTRAIT_SIZE + 2);
    if (portraits) draw_blit(portraits, x, y, &screen, &b, COLOR_NONE, 0);
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
    const Champion *c = &g_champions[g_inventory_champion_ordinal - 1];
    Bitmap *vp = ui_viewport();
    ui_blit_graphic(vp, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    ui_blit_graphic(vp, GFX_PANEL_FOOD_LABEL, ZONE_PANEL_FOOD_LABEL, COLOR_DARKEST_GRAY);
    ui_blit_graphic(vp, GFX_PANEL_WATER_LABEL, ZONE_PANEL_WATER_LABEL, COLOR_DARKEST_GRAY);
    if (c->poison_event_count) ui_blit_graphic(vp, GFX_PANEL_POISONED_LABEL, ZONE_PANEL_POISONED_LABEL, COLOR_DARKEST_GRAY);
    food_water_bar(c->food, ZONE_PANEL_FOOD_BAR, COLOR_LIGHT_BROWN);
    food_water_bar(c->water, ZONE_PANEL_WATER_BAR, COLOR_BLUE);
}

/* Draws the inventory panel: resurrect/reincarnate for a candidate, else food and water.
   ponytail: scroll and chest panels (object in the action hand) show food and water; add them
   with the panel object drawing. */
void draw_panel(void)
{
    if (g_candidate_champion_ordinal) {
        ui_blit_graphic(ui_viewport(), GFX_PANEL_RESURRECT_REINCARNATE, ZONE_PANEL, COLOR_DARK_GREEN);
        return;
    }
    draw_panel_food_water_poisoned();
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

/* Draws the eye panel: base skill levels above 1 (green after a level up), then the six
   statistics with current value (red below maximum, green above) and maximum value. */
void draw_champion_skills_and_statistics(void)
{
    int16_t ci = g_inventory_champion_ordinal - 1, x, y, vx;
    const Champion *c = &g_champions[ci];
    Bitmap *vp = ui_viewport();
    char s[40], a[8], b[8];
    ui_blit_graphic(vp, GFX_PANEL_EMPTY, ZONE_PANEL, COLOR_RED);
    zone_point(ZONE_PANEL_SKILLS, &x, &y);
    for (uint16_t k = 0; k < BASE_SKILL_COUNT; k++) {
        int16_t level = champion_get_skill_level(ci, k | SKILL_IGNORE_TEMPORARY_EXPERIENCE);
        if (level > SKILL_LEVEL_MAXIMUM) level = SKILL_LEVEL_MAXIMUM;
        if (level == 1) continue;
        snprintf(s, sizeof s, "%s %s", ui_string(kaos_s16(SKILL_LEVEL_STRINGS + 2 * (level - 2))), ui_string(kaos_s16(BASE_SKILL_STRINGS + 2 * k)));
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
    if (g_champions[champion_index].current_health && !g_candidate_champion_ordinal) {
        g_champions[champion_index].attributes |= CHAMPION_ATTR_STATUS_BOX;
        champion_draw_state((uint16_t)champion_index);
    }
    if (!g_party_is_sleeping) menus_draw_movement_arrows();
}
