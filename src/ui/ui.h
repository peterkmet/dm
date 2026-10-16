/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_UI_H
#define DM_UI_UI_H
/* Prefixes: ui_ */

/* Interface drawing on g_screen: zones, blits, palette, object icons, hit tests, message area, action and
   spell areas. Graphics, zones, strings, and input tables come from Graphics.DAT and the KAOS image at run time. */

#include "data/graphics.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_ICON_SIZE 16

#define SPELL_TAB_STEP 5 /* caster tab zones per caster */

/* Colors of the interface palette. */
#define COLOR_BLACK        0
#define COLOR_DARK_GRAY    1
#define COLOR_GRAY         2
#define COLOR_BROWN        3
#define COLOR_CYAN         4
#define COLOR_LIGHT_BROWN  5
#define COLOR_DARK_GREEN   6
#define COLOR_GREEN        7
#define COLOR_RED          8
#define COLOR_GOLD         9
#define COLOR_FLESH        10
#define COLOR_YELLOW       11
#define COLOR_DARKEST_GRAY 12
#define COLOR_LIGHT_GRAY   13
#define COLOR_BLUE         14
#define COLOR_WHITE        15
#define COLOR_NONE         (-1) /* opaque blit: no transparent color */
/* Amiga colors of the interface palette (g_initial_colormap), for the screens before the game files load. */
#define RGB_BLACK          0x000
#define RGB_DARK_GRAY      0x666
#define RGB_GRAY           0x888
#define RGB_BROWN          0x620
#define RGB_CYAN           0x0cc
#define RGB_LIGHT_BROWN    0x840
#define RGB_DARK_GREEN     0x080
#define RGB_GREEN          0x0c0
#define RGB_RED            0xf00
#define RGB_GOLD           0xfa0
#define RGB_FLESH          0xc86
#define RGB_YELLOW         0xff0
#define RGB_DARKEST_GRAY   0x444
#define RGB_LIGHT_GRAY     0xaaa
#define RGB_BLUE           0x00f
#define RGB_WHITE          0xfff

#define UI_BODY_SLOT_COUNT 6 /* hands, head, torso, legs, feet */

/* Slot boxes: the two hands of each status box, then the inventory slots, then the chest slots. */
#define UI_SLOT_BOX_STATUS_FIRST    0
#define UI_SLOT_BOX_INVENTORY_FIRST 8
#define UI_SLOT_BOX_ACTION_HAND     (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_HAND_ACTION)
#define UI_SLOT_BOX_CHEST_FIRST     (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_CHEST_FIRST)
#define UI_SLOT_BOX_COUNT           46 /* 8 hands on the screen, 30 inventory and 8 chest slots */

/* Mouse button masks of the input tables (the click kinds of the input tables). */
#define UI_BUTTON_RIGHT 1
#define UI_BUTTON_LEFT  2

/* Loads the strings, action names, and object names, and sets up the viewport bitmap. Returns true on success;
   call after tables_load, gfx_open, layout_init, and text_init. */
bool ui_init(void);

/* Shared helpers (ui.c). */
Bitmap *ui_get_viewport(void); /* the dungeon view bitmap, 224 x 136 */
/* The box of a layout zone on the screen. */
bool ui_zone_box(int16_t zone, Box *box);
/* Fills the box of zone with one color. */
void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color);
/* Dims the box of zone with a checkerboard of color (every second pixel, aligned to the screen). */
void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color);
/* Inverts bit plane 2 in the box of zone: black becomes cyan. */
void ui_xor_screen_zone(int16_t zone);
/* XORs the box on the screen with the highlight color. */
void ui_xor_screen_box(const Box *box);
/* Draws a graphic placed in zone. */
void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent);
/* Draws a graphic from its top left corner into box. */
void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent);
/* Draws a bitmap placed in zone. */
void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent);
/* Prints text placed in zone; the baseline is the bottom of the text box. */
void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text);
/* Prints text padded with spaces to length characters, so it covers older text. */
void ui_print_zone_padded(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t length);
/* Shows the viewport bitmap (inventory) on the screen with the interface palette. */
void ui_draw_viewport(void); /* draws the dungeon view bitmap into its zone */
/* Sets the palette split: the viewport rows use top (when not NULL, it replaces palette[0..15]),
   the other rows the interface palette. */
void ui_set_palette(const uint16_t *top);
/* One palette for all rows: the 16 colors, or the interface palette for NULL (dialogs, endgame). */
void ui_set_screen_palette(const uint16_t *colors);
/* Draws the whole screen outside the viewport, as at the start of a game. */
void ui_draw_interface(void);
/* Draws the message area at the bottom of the screen. */
void ui_draw_message_area(void);

/* Copies object icon into out (16 x 16 bytes). Each icon sheet holds 16 icons per row. */
void ui_extract_icon(int16_t icon, Bitmap *out);
/* Draws an icon in a slot box: the hands are on the screen, the other slots in the viewport. */
void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon);
/* The icon box of zone; with border, grown by the slot box border. */
bool ui_get_icon_zone_box(int16_t zone, bool border, Box *box); /* the box of a slot zone, with border the frame */
/* The icon box of a slot box. */
bool ui_get_slot_box_zone(int16_t slot_box, bool border, Box *box);
/* Shows the name of the object in the leader hand above the action area. Bones show the name of
   their champion first (English order). */
void ui_draw_leader_hand_object_name(uint16_t thing);

/* Tells whether (x, y) is in the box of zone. */
bool ui_is_point_in_zone(int16_t zone, int16_t x, int16_t y);
/* The command of a click at (x, y) with button_mask in an original mouse input table, or 0.
   An entry is a fixed box, or a zone on the screen or relative to the viewport. */
int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask);

/* Rune names by font character - 0x60: power, element, form, alignment. The original shows only
   the glyphs; scrolls name the runes. */
#define UI_RUNE_COUNT 24
extern const char *const g_rune_names[UI_RUNE_COUNT];

/* Width of a padded number in the status boxes and the panel. */
#define VALUE_WIDTH 3
/* A number as text: right aligned in width characters, or not padded. */
const char *ui_format_number(char *buf, size_t size, uint16_t number, bool pad, int16_t width);
/* The portrait of a champion as a bitmap. */
Bitmap ui_get_champion_portrait(int16_t champion_index);
/* Draws an object icon in an icon zone of the viewport. */
void ui_draw_icon_to_viewport(int16_t icon, int16_t zone);

#endif
