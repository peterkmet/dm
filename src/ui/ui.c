/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/ui.h"
#include "data/adventure.h"
#include "data/csb_text.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/message.h"
#include "ui/audio.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "ui/video.h"
#include "ui/view.h"
#include <stdlib.h>
#include <string.h>

#define SLOT_BOX_COUNT          46 /* 8 hands on the screen, 30 inventory and 8 chest slots */
#define SLOT_BOX_SCREEN_COUNT   8
#define ICONS_PER_ROW           16
#define LEADER_HAND_NAME_LENGTH 14
#define PADDED_TEXT_SIZE        80
#define BONES_NAME_SIZE         32 /* champion name and the bones name */

/* MouseInput.x1 values that make x2 a zone: on the screen, relative to the viewport origin,
   relative to the centered viewport. */
#define INPUT_ZONE_SCREEN            (-1)
#define INPUT_ZONE_VIEWPORT          (-2)
#define INPUT_ZONE_CENTERED_VIEWPORT (-3)

static uint8_t viewport_px[VIEWPORT_MAX_W * VIEWPORT_MAX_H];
static Bitmap viewport;
static int16_t slot_box_icon[SLOT_BOX_COUNT];

/* Loads the strings, action names, and object names, and sets up the viewport bitmap. */
int ui_init(void)
{
    audio_init();
    viewport = (Bitmap){tables_get_viewport_width(), tables_get_viewport_height(), viewport_px};
    if ((size_t)viewport.w * viewport.h > sizeof viewport_px)
        return -1;
    for (int i = 0; i < SLOT_BOX_COUNT; i++)
        slot_box_icon[i] = -1;
    return gfx_load_strings();
}

Bitmap ui_get_screen(void)
{
    return (Bitmap){SCREEN_W, SCREEN_H, &fb[0][0]};
}
Bitmap *ui_get_viewport(void)
{
    return &viewport;
}

int ui_zone_box(int16_t zone, Box *box)
{
    return layout_get_zone_box(zone, ZONE_SCALE_FULL, ZONE_SCALE_FULL, box);
}

void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color)
{
    Box b;
    if (ui_zone_box(zone, &b))
        draw_fill(dst, &b, color);
}

void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color)
{
    Box b;
    if (!ui_zone_box(zone, &b))
        return;
    for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++)
            if (!((x + y) & 1) && x >= 0 && x < dst->w && y >= 0 && y < dst->h)
                dst->px[y * dst->w + x] = color;
}

void ui_xor_screen_zone(int16_t zone)
{
    Box b;
    if (!ui_zone_box(zone, &b))
        return;
    for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++)
            if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
                fb[y][x] ^= COLOR_CYAN;
}

void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent)
{
    ui_blit_zone(dst, gfx_get_bitmap(graphic), zone, transparent);
}

void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent)
{
    const Bitmap *bm = gfx_get_bitmap(graphic);
    if (bm)
        draw_blit(bm, 0, 0, dst, box, transparent, 0);
}

void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent)
{
    int16_t w = 0;
    int16_t h = 0;
    Box b;
    if (src && layout_get_zone(zone, src->w, src->h, &b, &w, &h))
        draw_blit(src, w, h, dst, &b, transparent, 0);
}

void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text)
{
    int tw, th;
    Box b;
    if (!text_get_size(text, &tw, &th))
        return;
    int16_t w = tw;
    int16_t h = th;
    if (layout_get_zone(zone, 0, 0, &b, &w, &h))
        text_draw(dst, b.x, b.y + b.h - 1, fg, bg, text);
}

void ui_print_zone_padded(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t length)
{
    char buf[PADDED_TEXT_SIZE];
    int n = 0;
    for (; text[n] && n < (int)sizeof buf - 1; n++)
        buf[n] = text[n];
    for (; n < length && n < (int)sizeof buf - 1; n++)
        buf[n] = ' ';
    buf[n] = 0;
    ui_print_zone(dst, zone, fg, bg, buf);
}

void ui_draw_viewport(void)
{
    Bitmap screen = ui_get_screen();
    tables_get_palette(TABLES_PALETTE_INTERFACE, palette);
    ui_blit_zone(&screen, &viewport, ZONE_VIEWPORT, COLOR_NONE);
}

/* Sets the palette split: the viewport rows use top (when not NULL, it replaces palette[0..15]),
   the other rows the interface palette. */
void ui_set_screen_palette(const uint16_t *colors)
{
    for (int i = 0; i < PALETTE_COLORS; i++)
        palette[i] = colors ? colors[i] : tables_get_palette_color(TABLES_PALETTE_INTERFACE, i);
    palette_split_top = 0;
    palette_split_bottom = SCREEN_H;
}

void ui_set_palette(const uint16_t *top)
{
    Box b;
    for (int i = 0; i < PALETTE_COLORS; i++) {
        if (top)
            palette[i] = top[i];
        palette[PALETTE_COLORS + i] = tables_get_palette_color(TABLES_PALETTE_INTERFACE, i);
    }
    if (ui_zone_box(ZONE_VIEWPORT, &b)) {
        palette_split_top = b.y;
        palette_split_bottom = b.y + b.h;
    }
}

/* Draws the 4 rows into the zone at (x, y) in their scrolled state: the zone is cleared
   to color 0, then each cell is drawn with its color on color 0. */
static void draw_messages(Bitmap *bm, int x, int y)
{
    for (int py = y; py < y + MESSAGE_ROWS * MESSAGE_ROW_HEIGHT; py++)
        for (int px = x; px < x + MESSAGE_ROW_WIDTH; px++)
            if (py >= 0 && py < bm->h && px >= 0 && px < bm->w)
                bm->px[py * bm->w + px] = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        for (int c = 0; c < MESSAGE_COLS; c++)
            if (g_message.text[r][c]) {
                char ch[2] = {g_message.text[r][c], 0};
                text_draw(bm, x + c * FONT_CHAR_WIDTH,
                          y + r * MESSAGE_ROW_HEIGHT + FONT_LINE_HEIGHT - FONT_BASELINE_GAP - 1, g_message.color[r][c],
                          0, ch);
            }
}

void ui_draw_message_area(void)
{
    Bitmap screen = ui_get_screen();
    Box b;
    if (ui_zone_box(ZONE_MESSAGE_AREA, &b))
        draw_messages(&screen, b.x, b.y);
}

void ui_draw_interface(void)
{
    Bitmap screen = ui_get_screen();
    ui_fill_zone(&screen, ZONE_SCREEN_TOP, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_RIGHT, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_BOTTOM, COLOR_BLACK);
    if (g_inventory_champion_ordinal) {
        int16_t i = g_inventory_champion_ordinal - 1;
        inventory_set_open_champion(0);
        ui_draw_inventory_open(i);
    } else {
        menus_draw_movement_arrows();
    }
    ui_draw_all_champion_states(CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ICON);
    menus_draw_spell_area();
    g_redraw |= REDRAW_ACTION_AREA;
    menus_refresh_action_area();
    ui_draw_message_area();
}

/* Copies object icon into out (16 x 16). Each icon sheet holds 16 icons per row. */
void ui_extract_icon(int16_t icon, Bitmap *out)
{
    int sheet = 0;
    while (sheet < ICON_SHEET_COUNT && tables_get_icon_sheet_first(sheet) <= icon)
        sheet++;
    int16_t i = icon - tables_get_icon_sheet_first(sheet - 1);
    const Bitmap *bm = gfx_get_bitmap(GFX_OBJECT_ICONS_BEFORE_FIRST + sheet);
    out->w = UI_ICON_SIZE;
    out->h = UI_ICON_SIZE;
    memset(out->px, 0, UI_ICON_SIZE * UI_ICON_SIZE);
    if (bm)
        draw_blit(bm, i % ICONS_PER_ROW * UI_ICON_SIZE, i / ICONS_PER_ROW * UI_ICON_SIZE, out,
                  &(Box){0, 0, UI_ICON_SIZE, UI_ICON_SIZE}, COLOR_NONE, 0);
}

int ui_get_icon_zone_box(int16_t zone, int border, Box *box)
{
    int16_t w = UI_ICON_SIZE;
    int16_t h = UI_ICON_SIZE;
    int ok = layout_get_zone(zone, 0, 0, box, &w, &h);
    if (border) {
        int16_t d = tables_get_slot_box_border();
        box->x -= d;
        box->y -= d;
        box->w += 2 * d;
        box->h += 2 * d;
    }
    return ok;
}

bool ui_get_slot_box_zone(int16_t slot_box, int border, Box *box)
{
    return slot_box >= 0 && slot_box < SLOT_BOX_COUNT &&
           ui_get_icon_zone_box(tables_get_slot_box_zone(slot_box), border, box);
}

void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    Bitmap screen = ui_get_screen();
    Box b;
    if (slot_box < 0 || slot_box >= SLOT_BOX_COUNT)
        return;
    slot_box_icon[slot_box] = icon;
    if (icon == -1 || !ui_get_slot_box_zone(slot_box, 0, &b))
        return;
    ui_extract_icon(icon, &bm);
    draw_blit(&bm, 0, 0, slot_box < SLOT_BOX_SCREEN_COUNT ? &screen : &viewport, &b, COLOR_NONE, 0);
}

int16_t ui_get_icon_in_slot_box(int16_t slot_box)
{
    return slot_box >= 0 && slot_box < SLOT_BOX_COUNT ? slot_box_icon[slot_box] : -1;
}

/* The champion of a bones object: the charge bits of the junk record. */
static int bones_champion(uint16_t thing)
{
    return junk_charges(thing);
}

void ui_draw_leader_hand_object_name(uint16_t thing)
{
    Bitmap screen = ui_get_screen();
    char buf[BONES_NAME_SIZE];
    ui_fill_zone(&screen, ZONE_LEADER_HAND_OBJECT_NAME, COLOR_BLACK);
    int16_t icon = object_get_icon_index(thing);
    const char *name = gfx_object_name(icon);
    if (icon == ICON_BONES) {
        strncpy(buf, g_champions[bones_champion(thing)].name, sizeof buf - 1);
        buf[sizeof buf - 1] = 0;
        strncat(buf, gfx_object_name(ICON_BONES), sizeof buf - strlen(buf) - 1);
        name = buf;
    }
    ui_print_zone_padded(&screen, ZONE_LEADER_HAND_OBJECT_NAME, COLOR_CYAN, COLOR_BLACK, name, LEADER_HAND_NAME_LENGTH);
}

/* Tells whether (x, y) is in the box of zone. */
bool ui_is_point_in_zone(int16_t zone, int16_t x, int16_t y)
{
    Box b;
    return ui_zone_box(zone, &b) && x >= b.x && x <= b.x + b.w - 1 && y >= b.y && y <= b.y + b.h - 1;
}

int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask)
{
    if (table < 0 || table >= TABLES_MOUSE_COUNT)
        return 0;
    MouseInput input;
    for (int16_t entry = 0; tables_get_mouse_input(table, entry, &input); entry++) {
        bool buttons = (input.buttons & button_mask) != 0;
        if (input.x1 >= 0) {
            if (x >= input.x1 && x <= input.x2 && y >= input.y1 && y <= input.y2 && buttons)
                return input.command;
            continue;
        }
        int16_t px = x;
        int16_t py = y;
        if (input.x1 == INPUT_ZONE_VIEWPORT) {
            px -= tables_get_viewport_x();
            py -= tables_get_viewport_y();
        } else if (input.x1 == INPUT_ZONE_CENTERED_VIEWPORT) {
            px -= (SCREEN_W - viewport.w) / 2;
            py -= (SCREEN_H - viewport.h) / 2;
        }
        if (buttons && ui_is_point_in_zone(input.x2, px, py))
            return input.command;
    }
    return 0;
}
