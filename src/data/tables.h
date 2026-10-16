/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_TABLES_H
#define DM_DATA_TABLES_H
/* Prefixes: tables_ */

/* Static rule tables of KAOS.FTL, decoded once from the loaded image. Field bits: docs/rules/tables.md. */

#include "data/graphics.h"
#include <stdbool.h>
#include <stdint.h>

#define CREATURE_TYPE_COUNT           27
#define CREATURE_SOUND_COUNT          18
#define OBJECT_INFO_COUNT             180
#define WEAPON_TYPE_COUNT             46
#define ARMOUR_TYPE_COUNT             58
#define JUNK_TYPE_COUNT               53
#define FOOD_TYPE_COUNT               8
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
#define GRAPHIC_FLIP_NON_ATTACK         0x0004
#define GRAPHIC_HAS_SIDE                0x0008
#define GRAPHIC_HAS_BACK                0x0010
#define GRAPHIC_HAS_ATTACK              0x0020
#define GRAPHIC_SPECIAL_D2_FRONT        0x0080
#define GRAPHIC_D2_FRONT_FLIPPED        0x0100
#define GRAPHIC_FLIP_ATTACK             0x0200
#define GRAPHIC_FLIP_DURING_ATTACK      0x0400
#define GRAPHIC_HORIZONTAL_OFFSET_SHIFT 12
#define GRAPHIC_VERTICAL_OFFSET_SHIFT   14
#define GRAPHIC_OFFSET_RANGE_MASK       0x3

/* Range of the random horizontal aspect offset of a creature graphic. */
static inline uint16_t tables_get_horizontal_range(uint16_t graphic_info)
{
    return graphic_info >> GRAPHIC_HORIZONTAL_OFFSET_SHIFT & GRAPHIC_OFFSET_RANGE_MASK;
}
/* Range of the random vertical aspect offset of a creature graphic. */
static inline uint16_t tables_get_vertical_range(uint16_t graphic_info)
{
    return graphic_info >> GRAPHIC_VERTICAL_OFFSET_SHIFT;
}

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

/* Setup */

/* Loads the engine module at path (kaos_load), then decodes the tables. False as kaos_load. */
bool tables_load(const char *path);

/* True after a successful tables_load. */
bool tables_is_loaded(void);

/* Each struct accessor returns NULL for an out-of-range index. Call after tables_load. */

/* Attributes of a creature type. */
const CreatureInfo *tables_get_creature_info(int16_t creature_type);
/* Attack and movement sounds of a creature sound set. */
const CreatureSound *tables_get_creature_sound(int16_t index);
/* Type, aspect, action set and allowed slots of an object. */
const ObjectInfo *tables_get_object_info(int16_t index);
/* Weight, class, strength and kinetic energy of a weapon type. */
const WeaponInfo *tables_get_weapon_info(int16_t weapon_type);
/* Weight, defense and attributes of an armour type. */
const ArmourInfo *tables_get_armour_info(int16_t armour_type);
/* Symbols, skill, kind and type of a spell. */
const Spell *tables_get_spell(int16_t index);
/* The three actions and their minimum skill levels of an action set. */
const ActionSet *tables_get_action_set(int16_t index);
/* Ticks, damage, hit probability, stamina and experience of an action. */
const ActionInfo *tables_get_action_info(int16_t action);

/* The scalar tables return the value, or 0 for an out-of-range index. */

/* Weight of a junk type. */
uint8_t tables_get_junk_weight(int16_t junk_type);
/* Food that a food type adds. */
int16_t tables_get_food_value(int16_t index);
/* Mana cost of a symbol at a step, before the power multiplier. */
uint8_t tables_get_base_mana_cost(int16_t step, int16_t symbol);
/* Mana cost multiplier of a power symbol, in eighths. */
uint8_t tables_get_mana_multiplier(int16_t power);
/* Light amount of a light power. */
int16_t tables_get_light_amount(int16_t power);
/* Light threshold of a palette of the dungeon view. */
int16_t tables_get_light_threshold(int16_t index);
/* Bit of a slot in the allowed slots of an object. */
uint16_t tables_get_slot_mask(int16_t slot);
/* Share of a slot in the armour defense of a champion. */
uint8_t tables_get_wound_defense_factor(int16_t slot);
/* Entry of the fixed possessions lists of the creatures; 0 ends a list. */
uint16_t tables_get_fixed_possession(int16_t index);

/* Squares ahead and to each side that a sound reaches; the volume table covers -range..range. */
#define SOUND_HEARING_RANGE 12

/* Each accessor below reads the loaded image at call time; it needs tables_load. */

/* Map x step of one square in a direction. */
int16_t tables_get_step_east(int16_t direction);
/* Map y step of one square in a direction. */
int16_t tables_get_step_north(int16_t direction);
/* Squares forward of a movement arrow. */
int16_t tables_get_arrow_steps_forward(int16_t arrow);
/* Squares to the right of a movement arrow. */
int16_t tables_get_arrow_steps_right(int16_t arrow);
/* Cell i in the order that an attacker from row (side and direction) reaches the cells of the target square. */
uint8_t tables_get_attack_cell_order(int16_t row, int16_t i);
/* Wound mask of a wound probability index. */
uint8_t tables_get_wound_mask(int16_t index);
/* Inventory slot that a thief tries for a random index. */
uint8_t tables_get_steal_slot(int16_t index);
/* Icon offset of a lit torch by its charges. */
uint8_t tables_get_torch_icon_offset(int16_t charges);
/* Slot i in the order that a dead champion drops the objects. */
uint16_t tables_get_slot_drop_order(int16_t i);
/* Message color of a champion. */
uint8_t tables_get_champion_color(int16_t champion_index);
/* Event type that a sensor sends to a square of this type. */
uint8_t tables_get_square_type_event(int16_t square_type);
/* Graphic of a sound; negative when the sound has no sample. */
int16_t tables_get_sound_graphic(int16_t sound);
/* Sample rate of a sound. */
uint8_t tables_get_sound_rate(int16_t sound);
/* Priority of a sound: a higher value replaces a pending sound of the same loudness. */
uint8_t tables_get_sound_priority(int16_t sound);
/* Volume of one channel for a sound at squares ahead and squares toward the side of that channel. */
uint8_t tables_get_sound_volume(int16_t ahead, int16_t side);
/* Free records of a thing type that a new game adds to the dungeon. */
uint8_t tables_get_extra_things(int16_t thing_type);
/* Door info word of a door set: attributes in the high byte, defense in the low byte. */
uint16_t tables_get_door_attributes(int16_t door_set);
/* String of a message escape code. */
const char *tables_get_message_escape(int16_t code);
/* String of a wall inscription escape code. */
const char *tables_get_inscription_escape(int16_t code);
/* Replacement string of an escape code. */
const char *tables_get_escape_replacement(int16_t code);
/* Width of a champion portrait. */
int16_t tables_get_portrait_width(void);
/* Height of a champion portrait. */
int16_t tables_get_portrait_height(void);

/* Dungeon view tables; each accessor reads the loaded image at call time. */

/* Color word of a creature replacement color set at a light level (0-5). */
uint16_t tables_get_replacement_color(int16_t set, int16_t light);
/* Palette change of a creature replacement color set at depth 2 or 3. */
uint8_t tables_get_replacement_change(int16_t set, int16_t depth);
/* Aspect record of a creature: graphic, coordinate set, replacement color sets. */
const uint8_t *tables_get_creature_aspect(int16_t creature_type);
/* Aspect record of an object: graphic, flags, alcove coordinate set. */
const uint8_t *tables_get_object_aspect(int16_t index);
/* Aspect record of a projectile (index 1-14): graphic, pose and scale flags. */
const uint8_t *tables_get_projectile_aspect(int16_t index);
/* Aspect record of a field: the teleporter or fluxcage pattern of a view square. */
const uint8_t *tables_get_field_aspect(int16_t index);
/* Pixel shift index of a creature or a stacked object at a distance (0-2). */
int16_t tables_get_creature_shift(int16_t distance, int16_t index);
#define TABLES_AXIS_X 0
#define TABLES_AXIS_Y 1
/* Shift index of a stacked object on one axis (0 x, 1 y). */
uint8_t tables_get_pile_shift(int16_t pair, int16_t axis);
/* Scale of an object at a view row. */
uint8_t tables_get_object_scale(int16_t row);
/* Scale of a projectile at a view row. */
uint8_t tables_get_projectile_scale(int16_t row);
/* Scale of an explosion at a depth. */
uint8_t tables_get_explosion_scale(int16_t depth);
#define TABLES_REBIRTH_STEP_1 1 /* the two steps of a rebirth explosion */
#define TABLES_REBIRTH_STEP_2 2
/* Scale of rebirth step 1 or 2 at an object coordinate set. */
uint8_t tables_get_rebirth_scale(int16_t step, int16_t coord);
/* The 4 creature directions of a group that faces one direction, 2 bits per creature. */
uint8_t tables_get_group_directions(int16_t index);

/* What the view sees of one view square; a negative coordinate set: the view draws no such thing there. */
typedef struct {
    int8_t lane, depth, object_coord, alcove_coord, creature_coord, explosion_coord, field_aspect;
} ViewSquare;
/* Lane, depth, and coordinate sets of a view square. */
ViewSquare tables_get_view_square(int16_t view_square);
/* Layout bitmap id of wall set entry i. */
int16_t tables_get_wall_layout_id(int16_t i);
/* Depth (0-4) of a wall ornament view. */
uint8_t tables_get_wall_ornament_depth(int16_t view);
#define TABLES_PLAQUE_LINES 3 /* the most lines that an inscription plaque shows */
/* Visible plaque height of an inscription of 1-3 lines at a depth. */
uint8_t tables_get_plaque_height(int16_t lines, int16_t depth);
/* Graphic offset of a floor ornament view. */
uint8_t tables_get_floor_ornament_offset(int16_t view);
/* Wall ornament k of the alcove ornaments. */
uint8_t tables_get_alcove_ornament(int16_t k);
/* The wall ornament that is a fountain. */
uint8_t tables_get_fountain_ornament(void);
/* Coordinate set of a wall ornament graphic. */
uint8_t tables_get_wall_ornament_coords(int16_t ornament);
/* Coordinate set of a floor ornament graphic. */
uint8_t tables_get_floor_ornament_coords(int16_t ornament);
/* Coordinate set of a door ornament graphic. */
uint8_t tables_get_door_ornament_coords(int16_t ornament);
/* Coordinate set of the door button. */
uint8_t tables_get_door_button_coords(void);
/* Width of a wall text glyph. */
int16_t tables_get_glyph_width(void);
/* Height of a wall text glyph. */
int16_t tables_get_glyph_height(void);
/* Screen x of the viewport. */
int16_t tables_get_viewport_x(void);
/* Screen y of the viewport. */
int16_t tables_get_viewport_y(void);
/* Width of the viewport. */
int16_t tables_get_viewport_width(void);
/* Height of the viewport. */
int16_t tables_get_viewport_height(void);

/* Source rectangle of layout bitmap slot in its graphic. */
Box tables_get_layout_bitmap(int16_t slot);

/* Palettes; TABLES_PALETTE_DUNGEON + light level (0-5) gives the dungeon palette of that light. */
#define TABLES_PALETTE_COLOR_COUNT 16
#define TABLES_PALETTE_INTERFACE   0
#define TABLES_PALETTE_POINTER     1
#define TABLES_PALETTE_CREDITS     2
#define TABLES_PALETTE_ENTRANCE    3
#define TABLES_PALETTE_DUNGEON     4
/* One color of a palette. */
uint16_t tables_get_palette_color(int16_t palette, int16_t color);
/* The colors of a palette. */
void tables_get_palette(int16_t palette, uint16_t colors[TABLES_PALETTE_COLOR_COUNT]);

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
/* The color map of a palette change. */
const uint8_t *tables_get_palette_changes(int16_t changes);

/* Interface and champion panel tables; each accessor reads the loaded image at call time. */

/* First icon of an icon sheet. */
int16_t tables_get_icon_sheet_first(int16_t sheet);
/* Layout zone of a slot box. */
int16_t tables_get_slot_box_zone(int16_t slot_box);
/* Pixels that a bordered slot box grows on each side. */
int16_t tables_get_slot_box_border(void);
/* Width of a champion party icon. */
int16_t tables_get_champion_icon_width(void);
/* Height of a champion party icon. */
int16_t tables_get_champion_icon_height(void);
/* Offset of the shadow of a panel bar. */
int16_t tables_get_bar_shadow(void);
/* Width of the small or the large damage splat. */
int16_t tables_get_damage_splat_width(bool large);
/* Height of the small or the large damage splat. */
int16_t tables_get_damage_splat_height(bool large);
/* Text of a skill level name (level 2-16). */
int16_t tables_get_skill_level_text(int16_t level);
/* Text of a base skill name. */
int16_t tables_get_base_skill_text(int16_t skill);
/* Text of a statistic name. */
int16_t tables_get_statistic_text(int16_t statistic);
/* Text of a direction name. */
int16_t tables_get_direction_text(int16_t direction);
/* Short texts of the image, as C strings. */
#define TABLES_TEXT_LIST_SEPARATOR    0
#define TABLES_TEXT_BONES_SEPARATOR   1
#define TABLES_TEXT_ATTRIBUTES_PREFIX 2
#define TABLES_TEXT_ATTRIBUTES_SUFFIX 3
#define TABLES_TEXT_VERSION           4
#define TABLES_TEXT_COUNT             5
/* A short text of the image. */
const char *tables_get_text(int16_t text);
/* Pixels above and below a dialog message line. */
int16_t tables_get_message_line_pad(void);
/* Color of dialog message i. */
uint8_t tables_get_message_color(int16_t i);
/* Outer border of an endgame button on one axis. */
int16_t tables_get_button_border(int16_t axis);
/* Inner border of an endgame button on one axis. */
int16_t tables_get_button_inner_border(int16_t axis);
/* CSB: the outer or the inner box of the RESTART THIS GAME button. */
Box tables_get_csb_restart_box(bool inner);

/* Mouse pointer: the zones that select the pointer shape, the hotspots, the shadow, and the bitmaps. */
typedef struct {
    int16_t x, y, w, h, shape, champion;
} PointerZone;
/* Pointer zone i: a screen box, the pointer shape, and the champion + 1 of a status box. */
PointerZone tables_get_pointer_zone(int16_t i);
/* Hotspot x of a pointer shape. */
int16_t tables_get_pointer_hotspot_x(int16_t shape);
/* Hotspot y of a pointer shape. */
int16_t tables_get_pointer_hotspot_y(int16_t shape);
/* Shadow offset x of the object pointer. */
int16_t tables_get_pointer_shadow_dx(void);
/* Shadow offset y of the object pointer. */
int16_t tables_get_pointer_shadow_dy(void);
/* The 4 bit planes and the size of the arrow or the hand pointer. */
typedef struct {
    const uint8_t *planes;
    int16_t width, height;
} PointerBitmap;
/* The bit planes of the arrow or the hand pointer and its size. */
PointerBitmap tables_get_pointer_bitmap(bool hand);

/* Mouse input tables: which screen boxes give which command, in the order of their image addresses. */
#define TABLES_MOUSE_INTERFACE    0  /* the champion bars, the action and spell areas */
#define TABLES_MOUSE_MOVEMENT     1  /* the movement arrows and the dungeon view */
#define TABLES_MOUSE_INVENTORY    2  /* the open inventory */
#define TABLES_MOUSE_SLEEPING     3  /* the sleeping party: a click wakes it */
#define TABLES_MOUSE_FROZEN       4  /* the frozen game: a click goes on */
#define TABLES_MOUSE_ACTION_NAMES 5  /* the action names of the action menu */
#define TABLES_MOUSE_ACTION_ICONS 6  /* the action icons of the champions */
#define TABLES_MOUSE_SPELL_AREA   7  /* the runes and the cast button */
#define TABLES_MOUSE_STATUS_BOX   8  /* the status boxes of the champions */
#define TABLES_MOUSE_CHEST        9  /* the slots of an open chest */
#define TABLES_MOUSE_RESURRECT    10 /* the resurrect and reincarnate buttons */
#define TABLES_MOUSE_NAME_ENTRY   11 /* the keyboard of the champion name entry */
#define TABLES_MOUSE_ENTRANCE     12 /* the entrance buttons */
#define TABLES_MOUSE_ENDGAME      13 /* RESTART THIS GAME and QUIT of the endgame screen */
#define TABLES_MOUSE_DIALOG_FIRST 14 /* the dialog tables: + full screen * 4 + choice count - 1 */
#define TABLES_MOUSE_COUNT        (TABLES_MOUSE_DIALOG_FIRST + 8)
/* Mouse input entry: a command (0 ends a table), a box x1-x2, y1-y2, and the buttons. A negative x1 marks a
   zone entry; x2 is then the zone. */
typedef struct {
    int16_t command, x1, x2, y1, y2;
    uint16_t buttons;
} MouseInput;
/* Entry of a mouse input table; false at the end of the table. */
bool tables_get_mouse_input(int16_t table, int16_t entry, MouseInput *out);
/* Writes the box of an entry into the mouse input table of the image, as the endgame screen does. */
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
