#ifndef UI_H
#define UI_H
/* Prefixes: ui_ panel_ menus_ */
#include <stdint.h>
#include "champion.h"
#include "draw.h"

/* Interface drawing on fb: champion status boxes, action and spell areas, inventory panel,
   object icons, message area (KAOS CHAMPION.C, MENU.C, panel parts of INVENTORY.C). Graphics,
   zones, strings, and input tables come from Graphics.DAT and the KAOS image at run time. */

#define UI_ICON_SIZE 16

#define SPELL_TAB_STEP 5 /* caster tab zones per caster */

/* Colors of the interface palette. */
enum {
    COLOR_BLACK,
    COLOR_DARK_GRAY,
    COLOR_GRAY,
    COLOR_BROWN,
    COLOR_CYAN,
    COLOR_LIGHT_BROWN,
    COLOR_DARK_GREEN,
    COLOR_GREEN,
    COLOR_RED,
    COLOR_GOLD,
    COLOR_FLESH,
    COLOR_YELLOW,
    COLOR_DARKEST_GRAY,
    COLOR_LIGHT_GRAY,
    COLOR_BLUE,
    COLOR_WHITE,
};
#define COLOR_NONE       (-1) /* opaque blit: no transparent color */
/* Amiga colors of the interface palette (g_initial_colormap), for the screens before the game files load. */
#define RGB_BLACK        0x000
#define RGB_DARK_GRAY    0x666
#define RGB_GRAY         0x888
#define RGB_BROWN        0x620
#define RGB_CYAN         0x0cc
#define RGB_LIGHT_BROWN  0x840
#define RGB_DARK_GREEN   0x080
#define RGB_GREEN        0x0c0
#define RGB_RED          0xf00
#define RGB_GOLD         0xfa0
#define RGB_FLESH        0xc86
#define RGB_YELLOW       0xff0
#define RGB_DARKEST_GRAY 0x444
#define RGB_LIGHT_GRAY   0xaaa
#define RGB_BLUE         0x00f
#define RGB_WHITE        0xfff

#define UI_BODY_SLOT_COUNT 6 /* hands, head, torso, legs, feet */

#define UI_SLOT_BOX_INVENTORY_FIRST 8 /* slot box of champion slot 0 in the inventory */

/* Mouse input entry: 6 words: command, x1, x2, y1, y2, buttons. A negative x1 marks a zone entry;
   x2 is then the zone. */
enum { INPUT_COMMAND, INPUT_X1, INPUT_X2, INPUT_Y1, INPUT_Y2, INPUT_BUTTONS, INPUT_WORDS };
#define MOUSE_ENTRY_BYTES (2 * INPUT_WORDS)

/* Mouse input tables of COMMAND.C, in the order of their KAOS addresses. */
enum {
    UI_INPUT_INTERFACE,    /* g_mouse_input_interface */
    UI_INPUT_MOVEMENT,     /* g_mouse_input_movement */
    UI_INPUT_INVENTORY,    /* g_mouse_input_champion_inventory */
    UI_INPUT_SLEEPING,     /* g_mouse_input_party_sleeping */
    UI_INPUT_FROZEN,       /* g_mouse_input_frozen_game */
    UI_INPUT_ACTION_NAMES, /* g_mouse_input_action_area_names */
    UI_INPUT_ACTION_ICONS, /* g_mouse_input_action_area_icons */
    UI_INPUT_SPELL_AREA,   /* g_mouse_input_spell_area */
    UI_INPUT_STATUS_BOX,   /* g_mouse_input_champion_status_box */
    UI_INPUT_CHEST,        /* g_mouse_input_panel_chest */
    UI_INPUT_RESURRECT,    /* g_mouse_input_panel_resurrect_reincarnate */
    UI_INPUT_NAME_ENTRY,   /* g_mouse_input_champion_name_entry */
    UI_INPUT_ENTRANCE,     /* the entrance buttons */
    UI_INPUT_ENDGAME,      /* RESTART THIS GAME and QUIT of the endgame screen */
    UI_INPUT_DIALOG_FIRST, /* g_dialog_mouse_inputs: + full screen * 4 + choice count - 1 */
    UI_INPUT_COUNT = UI_INPUT_DIALOG_FIRST + 8
};

/* Mouse button masks of the input tables (the click kinds of input_event_handler). */
enum { UI_BUTTON_RIGHT = 1, UI_BUTTON_LEFT = 2 };

/* MENU.C state. */
extern int16_t g_action_area_contains_icons;
extern uint8_t g_action_list_minimum_skill_levels[3];
extern uint16_t g_action_count;
/* The portrait of a champion as a bitmap. */
Bitmap ui_champion_portrait(int16_t champion_index);

/* Setup: call after kaos_load, gfx_open, layout_init, text_init, and tables_init. */
int ui_init(void);

/* Shared helpers (ui.c). */
Bitmap ui_screen(void);
Bitmap *ui_viewport(void); /* g_viewport_bitmap, 224 x 136 */
/* The rank name that the eye panel shows for a skill level (NEOPHYTE ...), or NULL for level 1. */
const char *ui_skill_rank_name(int16_t level);
int ui_zone_box(int16_t zone, Box *box); /* get_zone_box */
void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color);
void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color);
void ui_xor_screen_zone(int16_t zone);
void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent);
void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent);
void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent);
void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text);
void ui_print_zone_padded(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t length);
void ui_draw_viewport(void); /* draw_viewport(0): the viewport bitmap to zone 7 */
void ui_set_palette(const uint16_t *top);
void ui_set_screen_palette(const uint16_t *colors);
void ui_draw_interface(void);
void ui_draw_message_area(void);

/* Object icons. */
void ui_extract_icon(int16_t icon, Bitmap *out); /* out->px holds 16 x 16 bytes */
void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon);
int16_t ui_get_icon_in_slot_box(int16_t slot_box);
int ui_icon_zone_box(int16_t zone, int border, Box *box); /* get_slot_box_zone, _border_zone */
int ui_slot_box_zone(int16_t slot_box, int border, Box *box);
void ui_draw_leader_hand_object_name(uint16_t thing);

/* Hit tests. */
int16_t ui_point_in_zone(int16_t zone, int16_t x, int16_t y);
int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask);
uint32_t ui_mouse_table_address(int16_t table); /* KAOS address of a mouse input table */

/* Rune names by font character - 0x60: power, element, form, alignment. The original shows only
   the glyphs; scrolls name the runes. */
#define UI_RUNE_COUNT 24
extern const char *const g_rune_names[UI_RUNE_COUNT];

/* Champions and the inventory panel (ui_champion.c). */
void ui_draw_champion_bar_graphs(int16_t champion_index);
void ui_draw_champion_slot(uint16_t champion_index, uint16_t slot);
void ui_draw_champion_damage(int16_t champion_index, int16_t damage);
void ui_draw_champion_state(uint16_t champion_index);
void ui_draw_all_champion_states(uint16_t attributes);
void ui_draw_status_box_portrait(int16_t champion_index);
void ui_draw_inventory_open(int16_t champion_index);
void ui_draw_inventory_close(int16_t champion_index);
void ui_draw_panel(void);
/* Draws an object icon in an icon zone of the viewport. */
void ui_draw_icon_to_viewport(int16_t icon, int16_t zone);
void ui_draw_panel_food_water_poisoned(void);
void ui_draw_champion_skills_and_statistics(void);

/* Inventory panel content (g_panel_content). */
enum { PANEL_CONTENT_FOOD_WATER = 1, PANEL_CONTENT_SCROLL = 5, PANEL_CONTENT_CHEST = 6, PANEL_CONTENT_RESURRECT = 7 };
enum { PANEL_LINE_COUNT = 3, PANEL_LINE_SIZE = 40 };
#define STAMINA_DISPLAY_UNIT 10 /* the inventory shows stamina in tenths of the stored value */
#define WEIGHT_TENTHS        10 /* loads and object weights are in tenths of a kilogram */
extern int16_t g_panel_content;
int16_t panel_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE]);
/* The ATTACK (weapon) or DEFENSE (armour) meter of the advanced object panel: 1 with its label and value, else 0. */
int panel_object_meter(uint16_t thing, const char **label, uint16_t *value);
void ui_draw_panel_object(uint16_t thing, int16_t pressing_eye);
void ui_draw_panel_eye(void);
void panel_redraw_scroll(void);
void panel_click_in_chest(int16_t x, int16_t y);

/* Action area, spell area, movement arrows (ui_menus.c). */
void menus_draw_movement_arrows(void);
void menus_set_action_list(int16_t action_set_index);
void menus_set_acting_champion(uint16_t champion_index);
void menus_clear_acting_champion(void);
void menus_draw_action_icon(uint16_t champion_index);
void menus_draw_action_area(void);
void menus_draw_action_damage(int16_t damage);
void menus_refresh_action_area(void);
void menus_draw_spell_area_controls(uint16_t champion_index);
void menus_draw_available_symbols(uint16_t symbol_step);
void menus_draw_champion_symbols(uint16_t champion_index);
void menus_set_magic_caster(int16_t champion_index);
void menus_refresh_spell_area(void);
void menus_reset(void);

#endif
