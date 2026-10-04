#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "champion.h"
#include "draw.h"

/* Interface drawing on fb: champion status boxes, action and spell areas, inventory panel,
   object icons, message area (KAOS CHAMPION.C, MENU.C, panel parts of INVENTORY.C). Graphics,
   zones, strings, and input tables come from Graphics.DAT and the KAOS image at run time. */

#define UI_ICON_SIZE 16

/* Colors of the interface palette. */
enum {
    COLOR_BLACK, COLOR_DARK_GRAY, COLOR_GRAY, COLOR_BROWN, COLOR_CYAN, COLOR_LIGHT_BROWN, COLOR_DARK_GREEN,
    COLOR_GREEN, COLOR_RED, COLOR_GOLD, COLOR_FLESH, COLOR_YELLOW, COLOR_DARKEST_GRAY, COLOR_LIGHT_GRAY,
    COLOR_BLUE, COLOR_WHITE,
};
#define COLOR_NONE (-1) /* opaque blit: no transparent color */

#define UI_BODY_SLOT_COUNT 6 /* hands, head, torso, legs, feet */

/* Champion attribute redraw flags (Champion.attributes). */
enum {
    CHAMPION_ATTR_ACTION_DISABLED = 0x0008,
    CHAMPION_ATTR_NAME_TITLE = 0x0080,
    CHAMPION_ATTR_STATISTICS = 0x0100,
    CHAMPION_ATTR_LOAD = 0x0200,
    CHAMPION_ATTR_ICON = 0x0400,
    CHAMPION_ATTR_PANEL = 0x0800,
    CHAMPION_ATTR_STATUS_BOX = 0x1000,
    CHAMPION_ATTR_WOUNDS = 0x2000,
    CHAMPION_ATTR_VIEWPORT = 0x4000,
    CHAMPION_ATTR_ACTION_HAND = 0x8000,
    CHAMPION_ATTR_REDRAW_MASK = 0xff80,      /* all redraw flags */
    CHAMPION_ATTR_KEEP_MASK = 0x007f,        /* flags that stay after a redraw */
    CHAMPION_ATTR_INVENTORY_ALL = 0x5f80,    /* everything the inventory shows */
};

#ifndef THING_NONE
#define THING_NONE 0xffff
#endif
#define UI_SLOT_BOX_INVENTORY_FIRST 8        /* slot box of champion slot 0 in the inventory */

/* Mouse input tables of COMMAND.C, in the order of their KAOS addresses. */
enum {
    UI_INPUT_INTERFACE,       /* g_mouse_input_interface */
    UI_INPUT_MOVEMENT,        /* g_mouse_input_movement */
    UI_INPUT_INVENTORY,       /* g_mouse_input_champion_inventory */
    UI_INPUT_SLEEPING,        /* g_mouse_input_party_sleeping */
    UI_INPUT_FROZEN,          /* g_mouse_input_frozen_game */
    UI_INPUT_ACTION_NAMES,    /* g_mouse_input_action_area_names */
    UI_INPUT_ACTION_ICONS,    /* g_mouse_input_action_area_icons */
    UI_INPUT_SPELL_AREA,      /* g_mouse_input_spell_area */
    UI_INPUT_STATUS_BOX,      /* g_mouse_input_champion_status_box */
    UI_INPUT_CHEST,           /* g_mouse_input_panel_chest */
    UI_INPUT_RESURRECT,       /* g_mouse_input_panel_resurrect_reincarnate */
    UI_INPUT_NAME_ENTRY,      /* g_mouse_input_champion_name_entry */
    UI_INPUT_ENTRANCE,        /* the entrance buttons */
    UI_INPUT_ENDGAME,         /* RESTART THIS GAME and QUIT of the endgame screen */
    UI_INPUT_DIALOG_FIRST,    /* g_dialog_mouse_inputs: + full screen * 4 + choice count - 1 */
    UI_INPUT_COUNT = UI_INPUT_DIALOG_FIRST + 8
};

/* Mouse button masks of the input tables (the click kinds of input_event_handler). */
enum { UI_BUTTON_RIGHT = 1, UI_BUTTON_LEFT = 2 };

/* MENU.C state. */
extern int16_t g_acting_champion_ordinal;      /* 0: none */
extern int16_t g_action_area_contains_icons;
extern uint8_t g_action_list_indices[3];       /* 0xff: none */
extern uint8_t g_action_list_minimum_skill_levels[3];
extern uint16_t g_action_count;
extern int16_t g_action_damage;                /* 0: none, -1 can't reach, -2 need ammo */
extern int16_t g_refresh_action_area;
/* Portrait of each champion in graphic 26 (the portrait bitmap pointer at champion +0x114). */
extern int16_t g_champion_portrait_indices[4];

/* Setup: call after kaos_load, gfx_open, layout_init, text_init, and tables_init. */
int ui_init(void);

/* Shared helpers (ui.c). */
Bitmap ui_screen(void);
Bitmap *ui_viewport(void);                     /* g_viewport_bitmap, 224 x 136 */
const char *ui_string(int16_t index);          /* get_string: graphic 700 */
const char *ui_object_name(int16_t icon);      /* g_object_names: graphic 0x2b6 */
const char *ui_action_name(int16_t action);    /* menus_get_action_name: graphic 699 */
/* The rank name that the eye panel shows for a skill level (NEOPHYTE ...), or NULL for level 1. */
const char *ui_skill_rank_name(int16_t level);
int ui_zone_box(int16_t zone, Box *box);       /* get_zone_box */
void ui_fill_zone(Bitmap *dst, int16_t zone, uint8_t color);
void ui_fill_zone_pattern(Bitmap *dst, int16_t zone, uint8_t color);
void ui_xor_screen_zone(int16_t zone);
void ui_blit_graphic(Bitmap *dst, int16_t graphic, int16_t zone, int16_t transparent);
void ui_blit_graphic_box(Bitmap *dst, int16_t graphic, const Box *box, int16_t transparent);
void ui_blit_zone(Bitmap *dst, const Bitmap *src, int16_t zone, int16_t transparent);
void ui_print_zone(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text);
void ui_print_zone_padded(Bitmap *dst, int16_t zone, uint8_t fg, uint8_t bg, const char *text, int16_t length);
void ui_draw_viewport(void);                   /* draw_viewport(0): the viewport bitmap to zone 7 */
void ui_set_palette(const uint16_t *top);
void ui_set_screen_palette(const uint16_t *colors);
void ui_draw_interface(void);
void ui_draw_message_area(void);

/* Object icons. */
void ui_extract_icon(int16_t icon, Bitmap *out);  /* out->px holds 16 x 16 bytes */
void ui_draw_icon_in_slot_box(int16_t slot_box, int16_t icon);
int16_t ui_get_icon_in_slot_box(int16_t slot_box);
int ui_icon_zone_box(int16_t zone, int border, Box *box);   /* get_slot_box_zone, _border_zone */
int ui_slot_box_zone(int16_t slot_box, int border, Box *box);
void draw_leader_hand_object_name(uint16_t thing);

/* Hit tests. */
int16_t ui_point_in_zone(int16_t zone, int16_t x, int16_t y);
int16_t ui_command_from_mouse(int16_t table, int16_t x, int16_t y, uint16_t button_mask);
uint32_t ui_mouse_table_address(int16_t table); /* KAOS address of a mouse input table */

/* Rune names by font character - 0x60: power, element, form, alignment. The original shows only
   the glyphs; scrolls name the runes. */
#define UI_RUNE_COUNT 24
extern const char *const g_rune_names[UI_RUNE_COUNT];

/* Champions and the inventory panel (ui_champion.c). */
void champion_draw_bar_graphs(int16_t champion_index);
void champion_draw_slot(uint16_t champion_index, uint16_t slot);
void champion_draw_damage(int16_t champion_index, int16_t damage);
void champion_draw_state(uint16_t champion_index);
void champion_draw_all_states(uint16_t attributes);
void draw_status_box_portrait(int16_t champion_index);
void inventory_draw_open(int16_t champion_index);
void inventory_draw_close(int16_t champion_index);
void draw_panel(void);
void draw_panel_food_water_poisoned(void);
void draw_champion_skills_and_statistics(void);

/* Inventory panel content (g_panel_content). */
enum { PANEL_CONTENT_FOOD_WATER = 1, PANEL_CONTENT_SCROLL = 5, PANEL_CONTENT_CHEST = 6, PANEL_CONTENT_RESURRECT = 7 };
enum { PANEL_LINE_COUNT = 3, PANEL_LINE_SIZE = 40 };
extern int16_t g_panel_content;
int16_t panel_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE]);
void draw_panel_object(uint16_t thing, int16_t pressing_eye);
void draw_panel_eye(void);
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
void menus_reset(void);

#endif
