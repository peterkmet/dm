/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_TABLES_H
#define DM_DATA_TABLES_H
/* Prefixes: tables_ */
#include <stdbool.h>
#include <stdint.h>

/* Static rule tables of KAOS.FTL, decoded once from the loaded image. Field bits: docs/rules/tables.md. */

#define CREATURE_TYPE_COUNT           27
#define CREATURE_SOUND_COUNT          18
#define OBJECT_INFO_COUNT             180
#define WEAPON_TYPE_COUNT             46
#define ARMOUR_TYPE_COUNT             58
#define JUNK_TYPE_COUNT               53
#define FOOD_TYPE_COUNT               8
#define DOOR_TYPE_COUNT               4
#define SYMBOL_STEP_COUNT             4
#define SYMBOL_COUNT                  6
#define ACTION_SET_COUNT              44
#define ACTION_COUNT                  44
#define LIGHT_POWER_COUNT             16
#define PALETTE_LIGHT_THRESHOLD_COUNT 6
#define SLOT_MASK_COUNT               38
#define WOUND_DEFENSE_FACTOR_COUNT    6
#define FIXED_POSSESSION_COUNT        40

/* Skill indices of the champion records: the 4 base skills, then the hidden skills. */
#define SKILL_FIGHTER          0
#define SKILL_NINJA            1
#define SKILL_PRIEST           2
#define SKILL_WIZARD           3
#define SKILL_BASE_COUNT       4
#define SKILL_SWING            4
#define SKILL_PARRY            7
#define SKILL_THROW            10
#define SKILL_SHOOT            11
#define SKILL_HEAL             13
#define SKILL_INFLUENCE        14
#define SKILL_DEFEND           15
#define SKILL_COUNT            20
#define HIDDEN_SKILLS_PER_BASE 4
#define SKILL_LEVEL_MAXIMUM    16 /* the level names stop at this level */

/* Object info index of the first record of each object type. */
#define OBJECT_INFO_FIRST_SCROLL    0
#define OBJECT_INFO_FIRST_CONTAINER 1
#define OBJECT_INFO_FIRST_POTION    2
#define OBJECT_INFO_FIRST_WEAPON    23
#define OBJECT_INFO_FIRST_ARMOUR    69
#define OBJECT_INFO_FIRST_JUNK      127
/* Object info allowed slots bit: the object passes through a door grate. */
#define OBJECT_PASSES_DOOR_GRATE    0x0100

/* Creature graphic info bits (CreatureInfo.graphic_info). */
#define GRAPHIC_FLIP_NON_ATTACK               0x0004
#define GRAPHIC_HAS_SIDE                      0x0008
#define GRAPHIC_HAS_BACK                      0x0010
#define GRAPHIC_HAS_ATTACK                    0x0020
#define GRAPHIC_SPECIAL_D2_FRONT              0x0080
#define GRAPHIC_D2_FRONT_FLIPPED              0x0100
#define GRAPHIC_FLIP_ATTACK                   0x0200
#define GRAPHIC_FLIP_DURING_ATTACK            0x0400
#define GRAPHIC_HORIZONTAL_OFFSET_RANGE(info) ((info) >> 12 & 3)
#define GRAPHIC_VERTICAL_OFFSET_RANGE(info)   ((info) >> 14)

typedef struct {
    uint8_t aspect_index;
    uint8_t attack_sound_ordinal; /* 0 = none, else index + 1 into creature sounds */
    uint16_t attributes;
    uint16_t graphic_info;
    uint8_t movement_ticks;
    uint8_t attack_ticks;
    uint8_t defense; /* 255 = invulnerable */
    uint8_t base_health;
    uint8_t attack;
    uint8_t poison_attack;
    uint8_t dexterity;
    uint16_t ranges;
    uint16_t properties;
    uint16_t resistances;
    uint16_t animation_ticks;
    uint16_t wound_probabilities;
    uint8_t attack_type;
} CreatureInfo;

typedef struct {
    int8_t attack; /* sound index, -1 = none */
    int8_t movement;
} CreatureSound;

typedef struct {
    uint16_t type; /* object name index and base icon index */
    uint8_t aspect_index;
    uint8_t action_set_index;
    uint16_t allowed_slots;
} ObjectInfo;

typedef struct {
    uint8_t weight;
    uint8_t weapon_class;
    uint8_t strength;
    uint8_t kinetic_energy;
    uint16_t attributes;
} WeaponInfo;

typedef struct {
    uint8_t weight;
    uint8_t defense;
    uint8_t attributes;
} ArmourInfo;

typedef struct {
    uint8_t attributes;
    uint8_t defense;
} DoorInfo;

typedef struct {
    uint32_t symbols; /* bits 24-31 power symbol, 0 = none */
    uint8_t base_required_skill_level;
    uint8_t skill_index;
    uint8_t kind; /* 1 potion, 2 projectile, 3 other */
    uint8_t type;
    uint8_t disabled_ticks;
} Spell;

typedef struct {
    uint8_t action_indices[3];       /* 255 = none */
    uint8_t minimum_skill_levels[2]; /* actions 1 and 2; bit 7 = needs charges */
    uint8_t flags;                   /* copied with the set; no reader is known */
} ActionSet;
#define ACTION_LIST_COUNT          3
#define ACTION_SET_EMPTY_HAND      2
#define ACTION_LEVEL_NEEDS_CHARGES 0x80 /* bit of a minimum skill level */

typedef struct {
    uint8_t disabled_ticks;
    uint8_t damage_factor;
    uint8_t hit_probability;
    uint8_t stamina;
    int8_t defense;
    uint8_t skill_index;
    uint8_t experience_gain;
} ActionInfo;

/* Call after kaos_load. Returns 0, or -1 when no KAOS image is loaded. */
int tables_init(void);

/* Each accessor returns NULL for an out-of-range index or before tables_init. */
const CreatureInfo *tables_get_creature_info(int creature_type);
const CreatureSound *tables_get_creature_sound(int index);
const ObjectInfo *tables_get_object_info(int index);
const WeaponInfo *tables_get_weapon_info(int weapon_type);
const ArmourInfo *tables_get_armour_info(int armour_type);
const uint8_t *tables_get_junk_weight(int junk_type);
const int16_t *tables_get_food_value(int index);
const DoorInfo *tables_get_door_info(int door_type);
const Spell *tables_get_spell(int index);
const uint8_t *tables_get_symbol_base_mana_cost(int step, int symbol);
const uint8_t *tables_get_symbol_mana_cost_multiplier(int power);
const ActionSet *tables_get_action_set(int index);
const ActionInfo *tables_get_action_info(int action);
const int16_t *tables_get_light_power_to_light_amount(int power);
const int16_t *tables_get_palette_light_threshold(int index);
const uint16_t *tables_get_slot_mask(int slot);
const uint8_t *tables_get_wound_defense_factor(int slot);
const uint16_t *tables_get_fixed_possession(int index);

/* Loads the engine module at path (kaos_load), then decodes the tables. Returns 0, or -1 as kaos_load. */
int tables_load(const char *path);

/* Squares ahead and to each side that a sound reaches; the volume table covers -range..range. */
#define SOUND_HEARING_RANGE 12

/* Each accessor below reads the loaded image at call time; it needs kaos_load, not tables_init. */
int16_t tables_get_step_east(int16_t direction);
int16_t tables_get_step_north(int16_t direction);
int16_t tables_get_arrow_steps_forward(int16_t arrow);
int16_t tables_get_arrow_steps_right(int16_t arrow);
uint8_t tables_get_ordered_cell_to_attack(int16_t row, int16_t i);
uint8_t tables_get_wound_probability_mask(int16_t index);
uint8_t tables_get_slot_to_steal_from(int16_t index);
uint8_t tables_get_torch_icon_offset(int16_t charges);
uint16_t tables_get_slot_drop_order(int16_t i);
uint8_t tables_get_champion_color(int16_t champion_index);
uint8_t tables_get_square_type_event(int16_t square_type);
int16_t tables_get_sound_graphic(int16_t sound);
uint8_t tables_get_sound_rate(int16_t sound);
uint8_t tables_get_sound_priority(int16_t sound);
uint8_t tables_get_sound_volume(int16_t ahead, int16_t side);
uint8_t tables_get_new_game_extra_things(int16_t thing_type);
uint16_t tables_get_door_attributes(int16_t door_set);
const char *tables_get_message_escape(int16_t code);
const char *tables_get_inscription_escape(int16_t code);
const char *tables_get_escape_replacement(int16_t code);
int16_t tables_get_portrait_width(void);
int16_t tables_get_portrait_height(void);

/* Dungeon view tables; each accessor reads the loaded image at call time. */
uint16_t tables_get_replacement_color(int16_t set, int16_t light);
uint8_t tables_get_replacement_change(int16_t set, int16_t depth);
const uint8_t *tables_get_creature_aspect(int16_t creature_type);
const uint8_t *tables_get_object_aspect(int16_t index);
const uint8_t *tables_get_projectile_aspect(int16_t index);
const uint8_t *tables_get_field_aspect(int16_t index);
int16_t tables_get_creature_shift(int16_t distance, int16_t index);
#define TABLES_AXIS_X 0
#define TABLES_AXIS_Y 1
uint8_t tables_get_pile_shift(int16_t pair, int16_t axis);
uint8_t tables_get_object_scale(int16_t row);
uint8_t tables_get_projectile_scale(int16_t row);
uint8_t tables_get_explosion_scale(int16_t depth);
#define TABLES_REBIRTH_STEP_1 1 /* the two steps of a rebirth explosion */
#define TABLES_REBIRTH_STEP_2 2
uint8_t tables_get_rebirth_scale(int16_t step, int16_t coord);
uint8_t tables_get_group_directions(int16_t index);

/* What the view sees of one view square; a negative coordinate set: the view draws no such thing there. */
typedef struct {
    int8_t lane, depth, object_coord, alcove_coord, creature_coord, explosion_coord, field_aspect;
} ViewSquare;
ViewSquare tables_get_view_square(int16_t view_square);
int16_t tables_get_wall_set_layout_id(int16_t i);
uint8_t tables_get_wall_ornament_depth(int16_t view);
#define TABLES_PLAQUE_LINES 3 /* the most lines that an inscription plaque shows */
uint8_t tables_get_plaque_height(int16_t lines, int16_t depth);
uint8_t tables_get_floor_ornament_view_offset(int16_t view);
uint8_t tables_get_alcove_ornament(int16_t k);
uint8_t tables_get_fountain_ornament(void);
uint8_t tables_get_wall_ornament_coord_set(int16_t ornament);
uint8_t tables_get_floor_ornament_coord_set(int16_t ornament);
uint8_t tables_get_door_ornament_coord_set(int16_t ornament);
uint8_t tables_get_door_button_coord_set(void);
int16_t tables_get_glyph_width(void);
int16_t tables_get_glyph_height(void);
int16_t tables_get_viewport_x(void);
int16_t tables_get_viewport_y(void);
int16_t tables_get_viewport_width(void);
int16_t tables_get_viewport_height(void);

/* Source rectangle of a layout bitmap slot in its graphic (0: the bitmap size). */
typedef struct {
    int16_t x, y, w, h;
} TablesRect;
TablesRect tables_get_layout_bitmap(int16_t slot);

/* Palettes; TABLES_PALETTE_DUNGEON + light level (0-5) gives the dungeon palette of that light. */
#define TABLES_PALETTE_COLOR_COUNT 16
#define TABLES_PALETTE_INTERFACE   0
#define TABLES_PALETTE_POINTER     1
#define TABLES_PALETTE_CREDITS     2
#define TABLES_PALETTE_ENTRANCE    3
#define TABLES_PALETTE_DUNGEON     4
void tables_get_palette(int16_t palette, uint16_t colors[TABLES_PALETTE_COLOR_COUNT]);
uint16_t tables_get_palette_color(int16_t palette, int16_t color);

/* Palette changes: a color map of TABLES_PALETTE_COLOR_COUNT bytes for draw_shrink. */
#define TABLES_CHANGES_D3               0
#define TABLES_CHANGES_D2               1
#define TABLES_CHANGES_DOOR_ORNAMENT_D3 2
#define TABLES_CHANGES_DOOR_ORNAMENT_D2 3
#define TABLES_CHANGES_SMOKE            4
#define TABLES_CHANGES_OBJECT_D3        5
#define TABLES_CHANGES_OBJECT_D2        6
#define TABLES_CHANGES_CREATURE_D3      7
#define TABLES_CHANGES_CREATURE_D2      8
#define TABLES_CHANGES_INVISIBILITY     9
#define TABLES_CHANGES_ACTION_ICON      10
#define TABLES_CHANGES_POINTER_SHADOW   11
#define TABLES_CHANGES_COUNT            12
const uint8_t *tables_get_palette_changes(int16_t changes);

/* Interface and champion panel tables; each accessor reads the loaded image at call time. */
int16_t tables_get_icon_sheet_first(int16_t sheet);
int16_t tables_get_slot_box_zone(int16_t slot_box);
int16_t tables_get_slot_box_border(void);
int16_t tables_get_champion_icon_width(void);
int16_t tables_get_champion_icon_height(void);
int16_t tables_get_bar_shadow(void);
int16_t tables_get_damage_splat_width(bool large);
int16_t tables_get_damage_splat_height(bool large);
int16_t tables_get_skill_level_text(int16_t level);
int16_t tables_get_base_skill_text(int16_t skill);
int16_t tables_get_statistic_text(int16_t statistic);
int16_t tables_get_direction_text(int16_t direction);
/* Short texts of the image, as C strings. */
#define TABLES_TEXT_LIST_SEPARATOR    0
#define TABLES_TEXT_BONES_SEPARATOR   1
#define TABLES_TEXT_ATTRIBUTES_PREFIX 2
#define TABLES_TEXT_ATTRIBUTES_SUFFIX 3
#define TABLES_TEXT_VERSION           4
#define TABLES_TEXT_COUNT             5
const char *tables_get_text(int16_t text);
int16_t tables_get_message_line_pad(void);
uint8_t tables_get_message_color(int16_t i);
int16_t tables_get_button_border(int16_t axis);
int16_t tables_get_button_inner_border(int16_t axis);
TablesRect tables_get_csb_restart_box(bool inner);

/* Mouse pointer: the zones that select the pointer shape, the hotspots, the shadow, and the bitmaps. */
typedef struct {
    int16_t x, y, w, h, shape, champion;
} PointerZone;
PointerZone tables_get_pointer_zone(int16_t i);
int16_t tables_get_pointer_hotspot_x(int16_t shape);
int16_t tables_get_pointer_hotspot_y(int16_t shape);
int16_t tables_get_pointer_shadow_dx(void);
int16_t tables_get_pointer_shadow_dy(void);
/* The 4 bit planes of the arrow or the hand pointer; *w and *h get its size. */
const uint8_t *tables_get_pointer_planes(bool hand, int16_t *w, int16_t *h);

/* Mouse input tables of COMMAND.C, in the order of their KAOS addresses. */
#define UI_INPUT_INTERFACE    0  /* the champion bars, the action and spell areas */
#define UI_INPUT_MOVEMENT     1  /* the movement arrows and the dungeon view */
#define UI_INPUT_INVENTORY    2  /* the open inventory */
#define UI_INPUT_SLEEPING     3  /* the sleeping party: a click wakes it */
#define UI_INPUT_FROZEN       4  /* the frozen game: a click goes on */
#define UI_INPUT_ACTION_NAMES 5  /* the action names of the action menu */
#define UI_INPUT_ACTION_ICONS 6  /* the action icons of the champions */
#define UI_INPUT_SPELL_AREA   7  /* the runes and the cast button */
#define UI_INPUT_STATUS_BOX   8  /* the status boxes of the champions */
#define UI_INPUT_CHEST        9  /* the slots of an open chest */
#define UI_INPUT_RESURRECT    10 /* the resurrect and reincarnate buttons */
#define UI_INPUT_NAME_ENTRY   11 /* the keyboard of the champion name entry */
#define UI_INPUT_ENTRANCE     12 /* the entrance buttons */
#define UI_INPUT_ENDGAME      13 /* RESTART THIS GAME and QUIT of the endgame screen */
#define UI_INPUT_DIALOG_FIRST 14 /* the dialog tables: + full screen * 4 + choice count - 1 */
#define UI_INPUT_COUNT        (UI_INPUT_DIALOG_FIRST + 8)
/* Mouse input entry: a command (0 ends a table), a box x1-x2, y1-y2, and the buttons. A negative x1 marks a
   zone entry; x2 is then the zone. */
typedef struct {
    int16_t command, x1, x2, y1, y2;
    uint16_t buttons;
} MouseInput;
/* Entry of a mouse input table; false at the end of the table. */
bool tables_get_mouse_input(int16_t table, int16_t entry, MouseInput *out);
/* Writes the box of an entry into the mouse input table of the image. */
void tables_set_mouse_input_box(int16_t table, int16_t entry, int16_t x1, int16_t x2, int16_t y1, int16_t y2);

/* Keyboard input tables. */
#define TABLES_KEYS_NONE      (-1)
#define TABLES_KEYS_ENTRANCE  0
#define TABLES_KEYS_INTERFACE 1
#define TABLES_KEYS_MOVEMENT  2
#define TABLES_KEYS_SLEEPING  3
#define TABLES_KEYS_FROZEN    4
#define TABLES_KEYS_COUNT     5
/* The command of a key in a keyboard table, or 0. */
int16_t tables_get_key_command(int16_t key_table, uint16_t key);

#endif
