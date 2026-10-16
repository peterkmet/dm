/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_UI_H
#define DM_UI_UI_H
/* Prefixes: ui_ panel_ menus_ */
#include "data/graphics.h"
#include "data/tables.h"
#include "engine/champion.h"
#include <stdbool.h>
#include <stdint.h>

/* Interface drawing on fb: champion status boxes, action and spell areas, inventory panel,
   object icons, message area (KAOS CHAMPION.C, MENU.C, panel parts of INVENTORY.C). Graphics,
   zones, strings, and input tables come from Graphics.DAT and the KAOS image at run time. */

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

#define UI_SLOT_BOX_INVENTORY_FIRST 8 /* slot box of champion slot 0 in the inventory */

/* Mouse button masks of the input tables (the click kinds of input_event_handler). */
#define UI_BUTTON_RIGHT 1
#define UI_BUTTON_LEFT  2

/* MENU.C state. */
extern bool g_action_area_contains_icons;
/* The portrait of a champion as a bitmap. */
Bitmap ui_draw_champion_portrait(int16_t champion_index);

/* Setup: call after tables_load, gfx_open, layout_init, and text_init. */
int ui_init(void);

/* Shared helpers (ui.c). */
Bitmap ui_get_screen(void);
Bitmap *ui_get_viewport(void); /* the dungeon view bitmap, 224 x 136 */
/* The rank name that the eye panel shows for a skill level (NEOPHYTE ...), or NULL for level 1. */
const char *ui_get_skill_rank_name(int16_t level);
int ui_zone_box(int16_t zone, Box *box); /* the box of a layout zone on the screen */
/* Fills the box of zone with one color. */
void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color);
/* Dims the box of zone with a checkerboard of color (every second pixel, aligned to the screen). */
void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color);
/* Inverts bit plane 2 in the box of zone: black becomes cyan. */
void ui_xor_screen_zone(int16_t zone);
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
void ui_draw_viewport(void); /* draw_viewport(0): the viewport bitmap to zone 7 */
void ui_set_palette(const uint16_t *top);
/* One palette for all rows: the 16 colors, or the interface palette for NULL (dialogs, endgame). */
void ui_set_screen_palette(const uint16_t *colors);
/* Draws the whole screen outside the viewport, as at the start of a game. */
void ui_draw_interface(void);
/* Draws the message area at the bottom of the screen. */
void ui_draw_message_area(void);

/* Object icons. */
void ui_extract_icon(int16_t icon, Bitmap *out); /* out->px holds 16 x 16 bytes */
/* Draws an icon in a slot box: the hands are on the screen, the other slots in the viewport. */
void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon);
/* The icon last drawn in a slot box, or -1. */
int16_t ui_get_icon_in_slot_box(int16_t slot_box);
/* The icon box of zone; with border, grown by the slot box border. */
int ui_get_icon_zone_box(int16_t zone, int border, Box *box); /* the box of a slot zone, with border the frame */
/* The icon box of a slot box. */
bool ui_get_slot_box_zone(int16_t slot_box, int border, Box *box);
/* Shows the name of the object in the leader hand above the action area. Bones show the name of
   their champion first (English order). */
void ui_draw_leader_hand_object_name(uint16_t thing);

/* Hit tests. */
bool ui_is_point_in_zone(int16_t zone, int16_t x, int16_t y);
/* The command of a click at (x, y) with button_mask in an original mouse input table, or 0.
   An entry is a fixed box, or a zone on the screen or relative to the viewport. */
int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask);

/* Rune names by font character - 0x60: power, element, form, alignment. The original shows only
   the glyphs; scrolls name the runes. */
#define UI_RUNE_COUNT 24
extern const char *const g_rune_names[UI_RUNE_COUNT];

/* Champions and the inventory panel (ui_champion.c). */
void ui_draw_champion_bar_graphs(int16_t champion_index);
/* Draws one slot: a hand in the status box, or any slot (and the chest slots) in the inventory of
   the champion. Empty body slots show their empty icon; wounded body slots get a red border, the
   action hand of the acting champion the acting border. */
void ui_draw_champion_slot(uint16_t champion_index, uint16_t slot);
/* Shows the damage a champion took: a splat with the number on the status box, or on the
   portrait while the inventory of the champion is open. */
void ui_draw_champion_damage(int16_t champion_index, int16_t damage);
/* Draws the parts of a champion that have a redraw flag in its attributes, then clears the flags. */
void ui_draw_champion_state(uint16_t champion_index);
/* Sets redraw flags on all champions and draws them. */
void ui_draw_all_champion_states(uint16_t attributes);
/* Draws the portrait in the status box while the inventory is open. */
void ui_draw_status_box_portrait(int16_t champion_index);
/* Opens the inventory of a champion on the screen: dims the movement arrows and draws the
   inventory background, labels, all slots, and the champion state. Sets g_inventory_champion_ordinal. */
void ui_draw_inventory_open(int16_t champion_index);
/* Closes the inventory on the screen: the status box returns and the movement arrows come back.
   The caller redraws the dungeon view. */
void ui_draw_inventory_close(int16_t champion_index);
/* Draws the inventory panel: resurrect/reincarnate for a candidate, the open scroll or chest in
   the action hand, else food and water. */
void ui_draw_panel(void);
/* Draws an object icon in an icon zone of the viewport. */
void ui_draw_icon_to_viewport(int16_t icon, int16_t zone);
/* Draws the food and water panel of the inventory, with "POISONED" when poisoned. */
void ui_draw_panel_food_water_poisoned(void);
/* Draws the eye panel: base skill levels above 1 (green after a level up), then the six
   statistics with current value (red below maximum, green above) and maximum value. */
void ui_draw_champion_skills_and_statistics(void);

/* Inventory panel content (g_panel_content). */
#define PANEL_CONTENT_FOOD_WATER 1
#define PANEL_CONTENT_SCROLL     5
#define PANEL_CONTENT_CHEST      6
#define PANEL_CONTENT_RESURRECT  7
#define PANEL_LINE_COUNT         3
#define PANEL_LINE_SIZE          40
#define STAMINA_DISPLAY_UNIT     10 /* the inventory shows stamina in tenths of the stored value */
#define WEIGHT_TENTHS            10 /* loads and object weights are in tenths of a kilogram */
extern int16_t g_panel_content;
/* The object panel text: the name (bones with the champion name, a potion with its power rune
   for a priest above level 1) and the description lines (burnt out, compass direction, waterskin
   level, attributes, weight). Returns the line count. */
int16_t panel_draw_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE]);
/* The ATTACK (weapon) or DEFENSE (armour) meter of the advanced object panel: 1 with its label and value, else 0. */
bool panel_draw_object_meter(uint16_t thing, const char **label, uint16_t *value);
/* Draws an object in the panel: the open scroll, the open chest, or the name, icon, and
   description of any other object; then the arrow, or the eye while it is pressed. */
void ui_draw_panel_object(uint16_t thing, int16_t pressing_eye);
/* The panel while the eye is pressed: the leader hand object, or the skills and statistics.
   ponytail: the leader hand object name stays; the original clears it until the eye is released. */
void ui_draw_panel_eye(void);
/* CSB, each tick with the inventory open: a scroll with a creature name or the magic map draws again. */
void panel_redraw_scroll(void);
/* A click in the open chest panel swaps the leader hand object with the clicked chest slot. */
void panel_click_in_chest(int16_t x, int16_t y);

/* Action area, spell area, movement arrows (ui_menus.c). */
void menus_draw_movement_arrows(void);
/* Opens the action menu of a champion: the actions of the object in the action hand. */
void menus_set_acting_champion(uint16_t champion_index);
/* Closes the action menu and redraws the action hand of the champion. */
void menus_clear_acting_champion(void);
/* Draws the action hand object of a champion as a black icon on a cyan box (with the advanced
   features, in its own colors on the cyan box); dimmed while the action is disabled, a candidate is
   shown, or the party sleeps. */
void menus_draw_action_icon(uint16_t champion_index);
/* Draws the action area: the action icons of all champions, or the action menu of the acting
   champion (its name and up to three action names). */
void menus_draw_action_area(void);
/* Shows the result of an action: "CAN'T REACH", "NEED AMMO", or the damage in a splat (smaller
   for small damage). */
void menus_draw_action_damage(int16_t damage);
/* Runs each tick: turns idle champions toward their last attacker, then redraws the action area
   when requested (menu, damage, or icons). */
void menus_refresh_action_area(void);
/* Draws the caster tabs of the spell area for a champion as the caster. */
void menus_draw_spell_area_controls(uint16_t champion_index);
/* Draws the spell area of the caster again. A dead caster (the whole party died) is cleared and nothing is
   drawn. */
void menus_draw_spell_area(void);
/* Draws the six runes that the caster can choose at its symbol step. */
void menus_draw_available_symbols(uint16_t symbol_step);
/* Draws the runes that the champion has chosen so far. */
void menus_draw_champion_symbols(uint16_t champion_index);
/* Selects the spell caster and draws the spell area; -1 clears it. */
void menus_set_magic_caster(int16_t champion_index);
/* Draws the spell area parts that the rules changed, with caster as the record of the magic caster (NULL: its own). */
void menus_refresh_spell_area(const Champion *caster);
/* No acting champion, no action list, no action damage, and the action icons at the start of a game. */
void menus_reset(void);

#endif
