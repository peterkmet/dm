#include <stdlib.h>
#include <string.h>
#include "adventure.h"
#include "csbtext.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "message.h"
#include "text.h"
#include "ui.h"
#include "video.h"
#include "view.h"

/* KAOS data addresses. */
#define BOTTOM_PALETTE 0x100170   /* g_initial_colormap[0..15]: interface palette */
#define ICON_FIRST 0x100b92       /* g_icon_graphic_first_index[7] */
#define SLOT_BOXES 0x100bb0       /* g_slot_boxes[46]: zone, current icon */
#define SLOT_BOX_BORDER 0x102956  /* g_slot_box_border */
static const uint32_t mouse_tables[UI_INPUT_DIALOG_FIRST] = {
    0x1031f8, 0x1032e8, 0x103354, 0x10351c, 0x103540, 0x103564,
    0x1035a0, 0x1035dc, 0x103654, 0x1036f0, 0x10375c, 0x10378c, 0x10318c, 0x1031d4,
};
#define DIALOG_MOUSE_INPUTS 0x103bd4 /* g_dialog_mouse_inputs[8]: pointers to the dialog tables */

#define SLOT_BOX_COUNT 46         /* 8 hands on the screen, 30 inventory and 8 chest slots */
#define SLOT_BOX_SCREEN_COUNT 8
#define ICON_SHEET_COUNT 7
#define ICONS_PER_ROW 16
#define LEADER_HAND_NAME_LENGTH 14

/* INPUT_X1 values that make INPUT_X2 a zone: on the screen, relative to the viewport origin,
   relative to the centered viewport. */
enum { INPUT_ZONE_SCREEN = -1, INPUT_ZONE_VIEWPORT = -2, INPUT_ZONE_CENTERED_VIEWPORT = -3 };

static uint8_t viewport_px[224 * 136];
static Bitmap viewport;
static int16_t slot_box_icon[SLOT_BOX_COUNT];

/* Loads the strings, action names, and object names, and sets up the viewport bitmap. */
int ui_init(void)
{
    viewport = (Bitmap){ kaos_u16(VIEWPORT_SIZE), kaos_u16(VIEWPORT_SIZE + 2), viewport_px };
    if ((size_t)viewport.w * viewport.h > sizeof viewport_px) return -1;
    for (int i = 0; i < SLOT_BOX_COUNT; i++) slot_box_icon[i] = -1;
    return gfx_load_strings();
}

Bitmap ui_screen(void) { return (Bitmap){ SCREEN_W, SCREEN_H, &fb[0][0] }; }
Bitmap *ui_viewport(void) { return &viewport; }

int ui_zone_box(int16_t zone, Box *box) { return layout_get_zone_box(zone, 10000, 10000, box); }

/* Fills the box of zone with one color. */
void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color)
{
    Box b;
    if (ui_zone_box(zone, &b)) draw_fill(dst, &b, color);
}

/* Dims the box of zone with a checkerboard of color (every second pixel, aligned to the screen). */
void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color)
{
    Box b;
    if (!ui_zone_box(zone, &b)) return;
    for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++)
            if (!((x + y) & 1) && x >= 0 && x < dst->w && y >= 0 && y < dst->h) dst->px[y * dst->w + x] = color;
}

/* Inverts bit plane 2 in the box of zone: black becomes cyan. */
void ui_xor_screen_zone(int16_t zone)
{
    Box b;
    if (!ui_zone_box(zone, &b)) return;
    for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++)
            if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y][x] ^= COLOR_CYAN;
}

/* Draws a graphic placed in zone. */
void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent)
{
    ui_blit_zone(dst, gfx_bitmap(graphic), zone, transparent);
}

/* Draws a graphic from its top left corner into box. */
void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent)
{
    const Bitmap *bm = gfx_bitmap(graphic);
    if (bm) draw_blit(bm, 0, 0, dst, box, transparent, 0);
}

/* Draws a bitmap placed in zone. */
void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent)
{
    int16_t w = 0, h = 0;
    Box b;
    if (src && layout_get_zone(zone, src->w, src->h, &b, &w, &h)) draw_blit(src, w, h, dst, &b, transparent, 0);
}

/* Prints text placed in zone; the baseline is the bottom of the text box. */
void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text)
{
    int tw, th;
    Box b;
    if (!text_size(text, &tw, &th)) return;
    int16_t w = (int16_t)tw, h = (int16_t)th;
    if (layout_get_zone(zone, 0, 0, &b, &w, &h)) text_draw(dst, b.x, b.y + b.h - 1, fg, bg, text);
}

/* Prints text padded with spaces to length characters, so it covers older text. */
void ui_print_zone_padded(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t length)
{
    char buf[80];
    int n = 0;
    for (; text[n] && n < (int)sizeof buf - 1; n++) buf[n] = text[n];
    for (; n < length && n < (int)sizeof buf - 1; n++) buf[n] = ' ';
    buf[n] = 0;
    ui_print_zone(dst, zone, fg, bg, buf);
}

/* Shows the viewport bitmap (inventory) on the screen with the interface palette. */
void ui_draw_viewport(void)
{
    Bitmap screen = ui_screen();
    for (int i = 0; i < 16; i++) palette[i] = kaos_u16(BOTTOM_PALETTE + 2 * i);
    ui_blit_zone(&screen, &viewport, ZONE_VIEWPORT, COLOR_NONE);
}

/* Sets the palette split: the viewport rows use top (when not NULL, it replaces palette[0..15]),
   the other rows the interface palette. */
/* One palette for all rows: the 16 colors, or the interface palette for NULL (dialogs, endgame). */
void ui_set_screen_palette(const uint16_t *colors)
{
    for (int i = 0; i < 16; i++) palette[i] = colors ? colors[i] : kaos_u16(BOTTOM_PALETTE + 2 * i);
    palette_split_top = 0;
    palette_split_bottom = SCREEN_H;
}

void ui_set_palette(const uint16_t *top)
{
    Box b;
    for (int i = 0; i < 16; i++) {
        if (top) palette[i] = top[i];
        palette[16 + i] = kaos_u16(BOTTOM_PALETTE + 2 * i);
    }
    if (ui_zone_box(ZONE_VIEWPORT, &b)) {
        palette_split_top = b.y;
        palette_split_bottom = (int16_t)(b.y + b.h);
    }
}

/* Draws the message area at the bottom of the screen. */
void ui_draw_message_area(void)
{
    Bitmap screen = ui_screen();
    Box b;
    if (ui_zone_box(ZONE_MESSAGE_AREA, &b)) message_draw(&screen, b.x, b.y);
}

/* Draws the whole screen outside the viewport, as at the start of a game. */
void ui_draw_interface(void)
{
    Bitmap screen = ui_screen();
    ui_fill_zone(&screen, ZONE_SCREEN_TOP, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_RIGHT, COLOR_BLACK);
    ui_fill_zone(&screen, ZONE_SCREEN_BOTTOM, COLOR_BLACK);
    if (g_inventory_champion_ordinal) {
        int16_t i = g_inventory_champion_ordinal - 1;
        g_inventory_champion_ordinal = 0;
        inventory_draw_open(i);
    } else {
        menus_draw_movement_arrows();
    }
    champion_draw_all_states(CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ICON);
    int16_t caster = g_magic_caster_champion_index;
    g_magic_caster_champion_index = -1;
    menus_set_magic_caster(caster);
    g_redraw |= REDRAW_ACTION_AREA;
    menus_refresh_action_area();
    ui_draw_message_area();
}

/* Copies object icon into out (16 x 16). Each icon sheet holds 16 icons per row. */
void ui_extract_icon(int16_t icon, Bitmap *out)
{
    int sheet = 0;
    while (sheet < ICON_SHEET_COUNT && kaos_s16(ICON_FIRST + 2 * sheet) <= icon) sheet++;
    int16_t i = (int16_t)(icon - kaos_s16(ICON_FIRST + 2 * (sheet - 1)));
    const Bitmap *bm = gfx_bitmap(GFX_OBJECT_ICONS_BEFORE_FIRST + sheet);
    out->w = out->h = UI_ICON_SIZE;
    memset(out->px, 0, UI_ICON_SIZE * UI_ICON_SIZE);
    if (bm) draw_blit(bm, (int16_t)(i % ICONS_PER_ROW * UI_ICON_SIZE), (int16_t)(i / ICONS_PER_ROW * UI_ICON_SIZE), out, &(Box){ 0, 0, UI_ICON_SIZE, UI_ICON_SIZE }, COLOR_NONE, 0);
}

/* The icon box of zone; with border, grown by the slot box border. */
int ui_icon_zone_box(int16_t zone, int border, Box *box)
{
    int16_t w = UI_ICON_SIZE, h = UI_ICON_SIZE;
    int ok = layout_get_zone(zone, 0, 0, box, &w, &h);
    if (border) {
        int16_t d = kaos_s16(SLOT_BOX_BORDER);
        box->x -= d, box->y -= d, box->w += 2 * d, box->h += 2 * d;
    }
    return ok;
}

/* The icon box of a slot box. */
int ui_slot_box_zone(int16_t slot_box, int border, Box *box)
{
    return slot_box >= 0 && slot_box < SLOT_BOX_COUNT && ui_icon_zone_box(kaos_s16(SLOT_BOXES + 4 * slot_box), border, box);
}

/* Draws an icon in a slot box: the hands are on the screen, the other slots in the viewport. */
void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = { UI_ICON_SIZE, UI_ICON_SIZE, px }, screen = ui_screen();
    Box b;
    if (slot_box < 0 || slot_box >= SLOT_BOX_COUNT) return;
    slot_box_icon[slot_box] = icon;
    if (icon == -1 || !ui_slot_box_zone(slot_box, 0, &b)) return;
    ui_extract_icon(icon, &bm);
    draw_blit(&bm, 0, 0, slot_box < SLOT_BOX_SCREEN_COUNT ? &screen : &viewport, &b, COLOR_NONE, 0);
}

/* The icon last drawn in a slot box, or -1. */
int16_t ui_get_icon_in_slot_box(int16_t slot_box)
{
    return slot_box >= 0 && slot_box < SLOT_BOX_COUNT ? slot_box_icon[slot_box] : -1;
}

/* The champion of a bones object: the charge bits of the junk record. */
static int bones_champion(uint16_t thing) { return junk_charges(thing); }

/* Shows the name of the object in the leader hand above the action area. Bones show the name of
   their champion first (English order). */
void draw_leader_hand_object_name(uint16_t thing)
{
    Bitmap screen = ui_screen();
    char buf[32];
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
int16_t ui_point_in_zone(int16_t zone, int16_t x, int16_t y)
{
    Box b;
    return ui_zone_box(zone, &b) && x >= b.x && x <= b.x + b.w - 1 && y >= b.y && y <= b.y + b.h - 1;
}

uint32_t ui_mouse_table_address(int16_t table)
{
    if (table >= UI_INPUT_DIALOG_FIRST) return kaos_u32(DIALOG_MOUSE_INPUTS + 4 * (uint32_t)(table - UI_INPUT_DIALOG_FIRST));
    return mouse_tables[table];
}

/* The command of a click at (x, y) with button_mask in an original mouse input table, or 0.
   An entry is a fixed box, or a zone on the screen or relative to the viewport. */
int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask)
{
    if (table < 0 || table >= UI_INPUT_COUNT) return 0;
    for (uint32_t a = ui_mouse_table_address(table);; a += 2 * INPUT_WORDS) {
        int16_t command = kaos_s16(a + 2 * INPUT_COMMAND), x1 = kaos_s16(a + 2 * INPUT_X1), x2 = kaos_s16(a + 2 * INPUT_X2);
        int buttons = (kaos_u16(a + 2 * INPUT_BUTTONS) & button_mask) != 0;
        if (!command) return 0;
        if (x1 >= 0) {
            if (x >= x1 && x <= x2 && y >= kaos_s16(a + 2 * INPUT_Y1) && y <= kaos_s16(a + 2 * INPUT_Y2) && buttons) return command;
            continue;
        }
        int16_t px = x, py = y;
        if (x1 == INPUT_ZONE_VIEWPORT) {
            px -= kaos_s16(VIEWPORT_ORIGIN);
            py -= kaos_s16(VIEWPORT_ORIGIN + 2);
        } else if (x1 == INPUT_ZONE_CENTERED_VIEWPORT) {
            px -= (int16_t)((SCREEN_W - viewport.w) / 2);
            py -= (int16_t)((SCREEN_H - viewport.h) / 2);
        }
        if (buttons && ui_point_in_zone(x2, px, py)) return command;
    }
}
