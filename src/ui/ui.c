/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui.h"
#include "graphics.h"
#include "ids.h"
#include "champion.h"
#include "game.h"
#include "inventory.h"
#include "message.h"
#include "record.h"
#include "audio.h"
#include "draw.h"
#include "layout.h"
#include "menus.h"
#include "panel.h"
#include "status.h"
#include "text.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Icon sheets and text buffers. */
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

const char *const g_rune_names[UI_RUNE_COUNT] = {
    "LO",  "UM", "ON",   "EE", "PAL", "MON", "YA", "VI",  "OH",   "FUL",  "DES", "ZO",
    "VEN", "EW", "KATH", "IR", "BRO", "GOR", "KU", "ROS", "DAIN", "NETA", "RA",  "SAR",
};

bool ui_init(void)
{
    audio_init();
    viewport = (Bitmap){tables_get_viewport_width(), tables_get_viewport_height(), viewport_px};
    if ((size_t)viewport.w * viewport.h > sizeof viewport_px)
        return false;
    return gfx_load_strings();
}

Bitmap *ui_get_viewport(void)
{
    return &viewport;
}

bool ui_zone_box(int16_t zone, Box *box)
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

void ui_xor_screen_box(const Box *box)
{
    for (int y = box->y; y < box->y + box->h; y++)
        for (int x = box->x; x < box->x + box->w; x++)
            if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
                g_screen.px[y][x] ^= COLOR_CYAN;
}

void ui_xor_screen_zone(int16_t zone)
{
    Box b;
    if (ui_zone_box(zone, &b))
        ui_xor_screen_box(&b);
}

void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent)
{
    ui_blit_zone(dst, gfx_get_bitmap(graphic), zone, transparent);
}

void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent)
{
    const Bitmap *bm = gfx_get_bitmap(graphic);
    if (bm)
        draw_blit(&(BlitSource){bm, 0, 0, transparent, 0}, dst, box);
}

void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent)
{
    ZoneClip placed;
    if (src && layout_get_zone(zone, &(ZoneItem){src->w, src->h, 0, 0}, &placed))
        draw_blit(&(BlitSource){src, placed.source_x, placed.source_y, transparent, 0}, dst, &placed.box);
}

void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text)
{
    int16_t w, h;
    ZoneClip placed;
    if (!text_get_size(text, &w, &h))
        return;
    if (layout_get_zone(zone, &(ZoneItem){0, 0, w, h}, &placed))
        text_draw(dst, (Point){placed.box.x, placed.box.y + placed.box.h - 1}, fg, bg, text);
}

void ui_print_zone_padded(int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t pad_length)
{
    Bitmap screen = video_get_screen();
    char buf[PADDED_TEXT_SIZE];
    int n = 0;
    for (; text[n] && n < (int)sizeof buf - 1; n++)
        buf[n] = text[n];
    for (; n < pad_length && n < (int)sizeof buf - 1; n++)
        buf[n] = ' ';
    buf[n] = 0;
    ui_print_zone(&screen, zone, fg, bg, buf);
}

void ui_draw_viewport(void)
{
    Bitmap screen = video_get_screen();
    tables_get_palette(TABLES_PALETTE_INTERFACE, g_screen.palette);
    ui_blit_zone(&screen, &viewport, ZONE_VIEWPORT, COLOR_NONE);
}

void ui_set_screen_palette(const uint16_t *colors)
{
    for (int i = 0; i < PALETTE_COLORS; i++)
        g_screen.palette[i] = colors ? colors[i] : tables_get_palette_color(TABLES_PALETTE_INTERFACE, i);
    g_screen.split_top = 0;
    g_screen.split_bottom = SCREEN_H;
}

void ui_set_palette(const uint16_t *top)
{
    Box b;
    for (int i = 0; i < PALETTE_COLORS; i++) {
        if (top)
            g_screen.palette[i] = top[i];
        g_screen.palette[PALETTE_COLORS + i] = tables_get_palette_color(TABLES_PALETTE_INTERFACE, i);
    }
    if (ui_zone_box(ZONE_VIEWPORT, &b)) {
        g_screen.split_top = b.y;
        g_screen.split_bottom = b.y + b.h;
    }
}

/* Draws the 4 rows into the zone at (x, y) in their scrolled state: the zone is cleared
   to color 0, then each cell is drawn with its color on color 0. */
static void show_messages(Bitmap *bm, int x, int y)
{
    for (int py = y; py < y + MESSAGE_ROWS * MESSAGE_ROW_HEIGHT; py++)
        for (int px = x; px < x + MESSAGE_ROW_WIDTH; px++)
            if (py >= 0 && py < bm->h && px >= 0 && px < bm->w)
                bm->px[py * bm->w + px] = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        for (int c = 0; c < MESSAGE_COLS; c++)
            if (g_message.text[r][c]) {
                char ch[CHAR_TEXT_SIZE] = {g_message.text[r][c], 0};
                text_draw(bm,
                          (Point){x + c * FONT_CHAR_WIDTH,
                                  y + r * MESSAGE_ROW_HEIGHT + FONT_LINE_HEIGHT - FONT_BASELINE_GAP - 1},
                          g_message.color[r][c], 0, ch);
            }
}

void ui_draw_message_area(void)
{
    Bitmap screen = video_get_screen();
    Box b;
    if (ui_zone_box(ZONE_MESSAGE_AREA, &b))
        show_messages(&screen, b.x, b.y);
}

void ui_draw_interface(void)
{
    Bitmap screen = video_get_screen();
    ui_fill_zone(&screen, ZONE_SCREEN_TOP, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_RIGHT, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_BOTTOM, COLOR_BLACK);
    if (g_session.inventory_champion_index >= 0) {
        int16_t i = g_session.inventory_champion_index;
        inventory_set_open_champion(-1);
        panel_open(i);
    } else {
        menus_draw_movement_arrows();
    }
    status_draw_all(CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ICON);
    menus_draw_spell_area();
    menus_end_damage_hold();
    g_redraw |= REDRAW_ACTION_AREA;
    menus_refresh_action_area();
    ui_draw_message_area();
}

void ui_extract_icon(int16_t icon, Bitmap *out)
{
    int sheet = 0;
    while (sheet < ICON_SHEET_COUNT && tables_get_icon_sheet_first(sheet) <= icon)
        sheet++;
    out->w = UI_ICON_SIZE;
    out->h = UI_ICON_SIZE;
    memset(out->px, 0, UI_ICON_SIZE * UI_ICON_SIZE);
    /* an icon below the first sheet draws empty (the original reads before the table) */
    if (sheet == 0)
        return;
    int16_t i = icon - tables_get_icon_sheet_first(sheet - 1);
    const Bitmap *bm = gfx_get_bitmap(GFX_OBJECT_ICONS_BEFORE_FIRST + sheet);
    if (bm)
        draw_blit(&(BlitSource){bm, i % ICONS_PER_ROW * UI_ICON_SIZE, i / ICONS_PER_ROW * UI_ICON_SIZE, COLOR_NONE, 0},
                  out, &(Box){0, 0, UI_ICON_SIZE, UI_ICON_SIZE});
}

bool ui_get_icon_zone_box(int16_t zone, bool border, Box *box)
{
    ZoneClip placed;
    if (!layout_get_zone(zone, &(ZoneItem){0, 0, UI_ICON_SIZE, UI_ICON_SIZE}, &placed))
        return false;
    *box = placed.box;
    if (border) {
        int16_t d = tables_get_slot_box_border();
        box->x -= d;
        box->y -= d;
        box->w += 2 * d;
        box->h += 2 * d;
    }
    return true;
}

bool ui_get_slot_box_zone(int16_t slot_box, bool border, Box *box)
{
    return slot_box >= 0 && slot_box < UI_SLOT_BOX_COUNT &&
           ui_get_icon_zone_box(tables_get_slot_box_zone(slot_box), border, box);
}

void ui_draw_slot_icon(int16_t slot_box, int16_t icon)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    Bitmap screen = video_get_screen();
    Box b;
    if (slot_box < 0 || slot_box >= UI_SLOT_BOX_COUNT)
        return;
    if (icon == -1 || !ui_get_slot_box_zone(slot_box, false, &b))
        return;
    ui_extract_icon(icon, &bm);
    draw_blit(&(BlitSource){&bm, 0, 0, COLOR_NONE, 0}, slot_box < UI_SLOT_BOX_INVENTORY_FIRST ? &screen : &viewport,
              &b);
}

/* The champion of a bones object: the charge bits of the junk record. */
static int bones_champion(uint16_t thing)
{
    return rec_junk_charges(thing);
}

void ui_draw_hand_object_name(uint16_t thing)
{
    Bitmap screen = video_get_screen();
    char buf[BONES_NAME_SIZE];
    ui_fill_zone(&screen, ZONE_LEADER_HAND_OBJECT_NAME, COLOR_BLACK);
    int16_t icon = object_get_icon_index(thing);
    const char *name = gfx_get_object_name(icon);
    if (icon == ICON_BONES) {
        strncpy(buf, g_champions[bones_champion(thing)].name, sizeof buf - 1);
        buf[sizeof buf - 1] = 0;
        strncat(buf, gfx_get_object_name(ICON_BONES), sizeof buf - strlen(buf) - 1);
        name = buf;
    }
    ui_print_zone_padded(ZONE_LEADER_HAND_OBJECT_NAME, COLOR_CYAN, COLOR_BLACK, name, LEADER_HAND_NAME_LENGTH);
}

bool ui_is_point_in_zone(int16_t zone, int16_t x, int16_t y)
{
    Box b;
    return ui_zone_box(zone, &b) && box_contains(&b, x, y);
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

const char *ui_format_number(char *buf, size_t size, uint16_t number, bool pad, int16_t width)
{
    if (pad)
        snprintf(buf, size, "%*u", width, number);
    else
        snprintf(buf, size, "%u", number);
    return buf;
}

Bitmap ui_get_champion_portrait(int16_t champion_index)
{
    return (Bitmap){CHAMPION_PORTRAIT_WIDTH, CHAMPION_PORTRAIT_HEIGHT, g_champions[champion_index].runtime.portrait};
}

void ui_draw_icon_to_viewport(int16_t icon, int16_t zone)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    Box b;
    if (!ui_get_icon_zone_box(zone, false, &b))
        return;
    ui_extract_icon(icon, &bm);
    draw_blit(&(BlitSource){&bm, 0, 0, COLOR_NONE, 0}, ui_get_viewport(), &b);
}
