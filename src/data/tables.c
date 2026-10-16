/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/tables.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/csbimage.h"
#include "data/kaos.h"
#include "data/kaos_addresses.h"
#include "data/spells.h"

/* Record sizes in KAOS. */
#define CREATURE_INFO_BYTES 26
#define OBJECT_INFO_BYTES   6
#define WEAPON_INFO_BYTES   6
#define ARMOUR_INFO_BYTES   4
#define ACTION_SET_BYTES    8
#define SOUND_ENTRY_BYTES   12
#define ESCAPE_STRING_BYTES 8
#define REPLACEMENT_BYTES   2

#define REPLACEMENT_SET_BYTES      14
#define CREATURE_ASPECT_BYTES      6
#define OBJECT_ASPECT_BYTES        4
#define PROJECTILE_ASPECT_BYTES    4
#define FIELD_ASPECT_BYTES         8
#define CREATURE_SHIFT_SET_BYTES   8
#define PILE_SHIFT_BYTES           2
#define LAYOUT_BITMAP_BYTES        14
#define PALETTE_BYTES              (TABLES_PALETTE_COLOR_COUNT * 2)
#define SLOT_BOX_BYTES             4
#define POINTER_ZONE_BYTES         12
#define MOUSE_INPUT_BYTES          12
#define KEY_ENTRY_BYTES            4
#define DIALOG_TABLE_POINTER_BYTES 4
#define POINTER_HOTSPOT_BYTES      8

#define SOUND_ENTRY_RATE     3 /* byte offsets in a sound entry */
#define SOUND_ENTRY_PRIORITY 4
#define ATTACK_CELL_COUNT    4 /* row lengths */
#define SOUND_VOLUME_ROW     (2 * SOUND_HEARING_RANGE + 1)

/* Byte offsets in a replacement color set: a color word per light level, then the D2 and the D3 change. */
#define REPLACEMENT_AT_CHANGE_D2       12
#define REPLACEMENT_FIRST_CHANGE_DEPTH 2
/* Byte offsets of the words of a pointer zone and of a pointer hotspot. */
#define POINTER_AT_X                   0
#define POINTER_AT_Y                   2
#define POINTER_AT_W                   4
#define POINTER_AT_H                   6
#define POINTER_AT_SHAPE               8
#define POINTER_AT_CHAMPION            10
/* Byte offsets of the words of a mouse input entry and of a key entry. */
#define MOUSE_AT_COMMAND               0
#define MOUSE_AT_X1                    2
#define MOUSE_AT_X2                    4
#define MOUSE_AT_Y1                    6
#define MOUSE_AT_Y2                    8
#define MOUSE_AT_BUTTONS               10
#define KEY_AT_COMMAND                 0
#define KEY_AT_KEY                     2
#define SKILL_LEVEL_TEXT_FIRST         2 /* the level names start at level 2 */
#define LAYOUT_AT_X                    2 /* byte offsets in a layout bitmap */
#define LAYOUT_AT_Y                    4
#define LAYOUT_AT_W                    6
#define LAYOUT_AT_H                    8

/* Byte offsets of the creature info fields (docs/rules/tables.md); bytes 13 and 25 are pad bytes. */
#define CREATURE_AT_ASPECT_INDEX         0
#define CREATURE_AT_ATTACK_SOUND_ORDINAL 1
#define CREATURE_AT_ATTRIBUTES           2
#define CREATURE_AT_GRAPHIC_INFO         4
#define CREATURE_AT_MOVEMENT_TICKS       6
#define CREATURE_AT_ATTACK_TICKS         7
#define CREATURE_AT_DEFENSE              8
#define CREATURE_AT_BASE_HEALTH          9
#define CREATURE_AT_ATTACK               10
#define CREATURE_AT_POISON_ATTACK        11
#define CREATURE_AT_DEXTERITY            12
#define CREATURE_AT_RANGES               14
#define CREATURE_AT_PROPERTIES           16
#define CREATURE_AT_RESISTANCES          18
#define CREATURE_AT_ANIMATION_TICKS      20
#define CREATURE_AT_WOUND_PROBABILITIES  22
#define CREATURE_AT_ATTACK_TYPE          24

#define SPELL_AT_LEVEL      4
#define SPELL_AT_SKILL      5
#define SPELL_AT_ATTRIBUTES 6
#define SPELL_BYTES         8

#define SPELL_KIND_MASK 0x0f
#define SPELL_TYPE_MASK 0x3f

#define SPELL_TYPE_SHIFT           4
#define SPELL_DISABLED_TICKS_SHIFT 10

/* The fields of the attributes word of a spell entry. */
static uint16_t attributes_kind(uint16_t attributes)
{
    return attributes & SPELL_KIND_MASK;
}
static uint16_t attributes_type(uint16_t attributes)
{
    return attributes >> SPELL_TYPE_SHIFT & SPELL_TYPE_MASK;
}
static uint16_t attributes_disabled_ticks(uint16_t attributes)
{
    return attributes >> SPELL_DISABLED_TICKS_SHIFT;
}

#define OBJECT_AT_TYPE             0
#define OBJECT_AT_ASPECT_INDEX     2
#define OBJECT_AT_ACTION_SET_INDEX 3
#define OBJECT_AT_ALLOWED_SLOTS    4

#define WEAPON_AT_WEIGHT         0
#define WEAPON_AT_CLASS          1
#define WEAPON_AT_STRENGTH       2
#define WEAPON_AT_KINETIC_ENERGY 3
#define WEAPON_AT_ATTRIBUTES     4

#define ARMOUR_AT_WEIGHT     0
#define ARMOUR_AT_DEFENSE    1
#define ARMOUR_AT_ATTRIBUTES 2

#define ACTION_SET_AT_ACTIONS 0
#define ACTION_SET_AT_LEVELS  4

static CreatureInfo creature_info[CREATURE_TYPE_COUNT];
static CreatureSound creature_sounds[CREATURE_SOUND_COUNT];
static ObjectInfo object_info[OBJECT_INFO_COUNT];
static WeaponInfo weapon_info[WEAPON_TYPE_COUNT];
static ArmourInfo armour_info[ARMOUR_TYPE_COUNT];
static uint8_t junk_weights[JUNK_TYPE_COUNT];
static int16_t food_values[FOOD_TYPE_COUNT];
static Spell spells[CSB_SPELL_COUNT];
static int16_t loaded_spell_count;
static uint8_t symbol_base_mana_cost[SYMBOL_STEP_COUNT][SYMBOL_COUNT];
static uint8_t symbol_mana_cost_multiplier[SYMBOL_COUNT];
static ActionSet action_sets[ACTION_SET_COUNT];
static ActionInfo action_info[ACTION_COUNT];
static int16_t light_power_to_light_amount[LIGHT_POWER_COUNT];
static int16_t palette_light_thresholds[PALETTE_LIGHT_THRESHOLD_COUNT];
static uint16_t slot_masks[SLOT_MASK_COUNT];
static uint8_t wound_defense_factor[WOUND_DEFENSE_FACTOR_COUNT];
static uint16_t fixed_possessions[FIXED_POSSESSION_COUNT];

/* Creature attributes and the creature attack and movement sounds. */
static void read_creatures(void)
{
    for (uint32_t i = 0; i < CREATURE_TYPE_COUNT; i++) {
        uint32_t a = KAOS_CREATURE_INFO + i * CREATURE_INFO_BYTES;
        creature_info[i] = (CreatureInfo){
            .aspect_index = kaos_u8(a + CREATURE_AT_ASPECT_INDEX),
            .attack_sound_ordinal = kaos_u8(a + CREATURE_AT_ATTACK_SOUND_ORDINAL),
            .attributes = kaos_u16(a + CREATURE_AT_ATTRIBUTES),
            .graphic_info = kaos_u16(a + CREATURE_AT_GRAPHIC_INFO),
            .movement_ticks = kaos_u8(a + CREATURE_AT_MOVEMENT_TICKS),
            .attack_ticks = kaos_u8(a + CREATURE_AT_ATTACK_TICKS),
            .defense = kaos_u8(a + CREATURE_AT_DEFENSE),
            .base_health = kaos_u8(a + CREATURE_AT_BASE_HEALTH),
            .attack = kaos_u8(a + CREATURE_AT_ATTACK),
            .poison_attack = kaos_u8(a + CREATURE_AT_POISON_ATTACK),
            .dexterity = kaos_u8(a + CREATURE_AT_DEXTERITY),
            .ranges = kaos_u16(a + CREATURE_AT_RANGES),
            .properties = kaos_u16(a + CREATURE_AT_PROPERTIES),
            .resistances = kaos_u16(a + CREATURE_AT_RESISTANCES),
            .animation_ticks = kaos_u16(a + CREATURE_AT_ANIMATION_TICKS),
            .wound_probabilities = kaos_u16(a + CREATURE_AT_WOUND_PROBABILITIES),
            .attack_type = kaos_u8(a + CREATURE_AT_ATTACK_TYPE),
        };
    }
    for (uint32_t i = 0; i < CREATURE_SOUND_COUNT; i++)
        creature_sounds[i] = (CreatureSound){(int8_t)kaos_u8(KAOS_CREATURE_SOUNDS + i * 2),
                                             (int8_t)kaos_u8(KAOS_CREATURE_SOUNDS + i * 2 + 1)};
}

/* Object, weapon, armour, junk, food, and door properties. */
static void read_objects(void)
{
    for (uint32_t i = 0; i < OBJECT_INFO_COUNT; i++) {
        uint32_t a = KAOS_OBJECT_INFO + i * OBJECT_INFO_BYTES;
        object_info[i] = (ObjectInfo){
            .type = kaos_u16(a + OBJECT_AT_TYPE),
            .aspect_index = kaos_u8(a + OBJECT_AT_ASPECT_INDEX),
            .action_set_index = kaos_u8(a + OBJECT_AT_ACTION_SET_INDEX),
            .allowed_slots = kaos_u16(a + OBJECT_AT_ALLOWED_SLOTS),
        };
    }
    for (uint32_t i = 0; i < WEAPON_TYPE_COUNT; i++) {
        uint32_t a = KAOS_WEAPON_INFO + i * WEAPON_INFO_BYTES;
        weapon_info[i] = (WeaponInfo){
            .weight = kaos_u8(a + WEAPON_AT_WEIGHT),
            .weapon_class = kaos_u8(a + WEAPON_AT_CLASS),
            .strength = kaos_u8(a + WEAPON_AT_STRENGTH),
            .kinetic_energy = kaos_u8(a + WEAPON_AT_KINETIC_ENERGY),
            .attributes = kaos_u16(a + WEAPON_AT_ATTRIBUTES),
        };
    }
    for (uint32_t i = 0; i < ARMOUR_TYPE_COUNT; i++) {
        uint32_t a = KAOS_ARMOUR_INFO + i * ARMOUR_INFO_BYTES;
        armour_info[i] = (ArmourInfo){
            .weight = kaos_u8(a + ARMOUR_AT_WEIGHT),
            .defense = kaos_u8(a + ARMOUR_AT_DEFENSE),
            .attributes = kaos_u8(a + ARMOUR_AT_ATTRIBUTES),
        };
    }
    for (uint32_t i = 0; i < JUNK_TYPE_COUNT; i++)
        junk_weights[i] = kaos_u8(KAOS_JUNK_WEIGHTS + i);
    for (uint32_t i = 0; i < FOOD_TYPE_COUNT; i++)
        food_values[i] = kaos_s16(KAOS_FOOD_VALUES + i * 2);
}

/* Spells and the mana costs of the spell symbols. */
static void read_magic(void)
{
    bool csb = g_adventure == ADVENTURE_CSB;
    loaded_spell_count = csb ? CSB_SPELL_COUNT : DM_SPELL_COUNT;
    for (uint32_t i = 0; i < (uint32_t)loaded_spell_count; i++) {
        uint32_t a = (csb ? CSB_SPELLS : KAOS_SPELLS) + i * SPELL_BYTES;
        uint16_t w = kaos_u16(a + SPELL_AT_ATTRIBUTES);
        spells[i] = (Spell){kaos_u32(a),        kaos_u8(a + SPELL_AT_LEVEL), kaos_u8(a + SPELL_AT_SKILL),
                            attributes_kind(w), attributes_type(w),          attributes_disabled_ticks(w)};
    }
    for (uint32_t s = 0; s < SYMBOL_STEP_COUNT; s++)
        for (uint32_t n = 0; n < SYMBOL_COUNT; n++)
            symbol_base_mana_cost[s][n] = kaos_u8(KAOS_SYMBOL_BASE_MANA_COST + s * SYMBOL_COUNT + n);
    for (uint32_t i = 0; i < SYMBOL_COUNT; i++)
        symbol_mana_cost_multiplier[i] = kaos_u8(KAOS_SYMBOL_MANA_COST_MULTIPLIER + i);
}

/* Action sets of the objects and the properties of each action. */
static void read_actions(void)
{
    for (uint32_t i = 0; i < ACTION_SET_COUNT; i++) {
        uint32_t a = KAOS_ACTION_SETS + i * ACTION_SET_BYTES;
        action_sets[i] = (ActionSet){
            .action_indices = {kaos_u8(a + ACTION_SET_AT_ACTIONS), kaos_u8(a + ACTION_SET_AT_ACTIONS + 1),
                               kaos_u8(a + ACTION_SET_AT_ACTIONS + 2)},
            .minimum_skill_levels = {kaos_u8(a + ACTION_SET_AT_LEVELS), kaos_u8(a + ACTION_SET_AT_LEVELS + 1)},
        };
    }
    for (uint32_t i = 0; i < ACTION_COUNT; i++)
        action_info[i] = (ActionInfo){
            .disabled_ticks = kaos_u8(KAOS_ACTIONS_DISABLED + i),
            .damage_factor = kaos_u8(KAOS_ACTION_DAMAGE_FACTOR + i),
            .hit_probability = kaos_u8(KAOS_ACTION_HIT_PROBABILITY + i),
            .stamina = kaos_u8(KAOS_ACTION_STAMINA + i),
            .defense = (int8_t)kaos_u8(KAOS_ACTION_DEFENSE + i),
            .skill_index = kaos_u8(KAOS_ACTION_SKILL_INDEX + i),
            .experience_gain = kaos_u8(KAOS_ACTION_EXPERIENCE_GAIN + i),
        };
}

/* Light amounts, palette thresholds, slot masks, wound defense, and fixed possessions. */
static void read_champion_tables(void)
{
    for (uint32_t i = 0; i < LIGHT_POWER_COUNT; i++)
        light_power_to_light_amount[i] = kaos_s16(KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT + i * 2);
    for (uint32_t i = 0; i < PALETTE_LIGHT_THRESHOLD_COUNT; i++)
        palette_light_thresholds[i] = kaos_s16(KAOS_PALETTE_LIGHT_THRESHOLDS + i * 2);
    for (uint32_t i = 0; i < SLOT_MASK_COUNT; i++)
        slot_masks[i] = kaos_u16(KAOS_SLOT_MASKS + i * 2);
    for (uint32_t i = 0; i < WOUND_DEFENSE_FACTOR_COUNT; i++)
        wound_defense_factor[i] = kaos_u8(KAOS_WOUND_DEFENSE_FACTOR + i);
    for (uint32_t i = 0; i < FIXED_POSSESSION_COUNT; i++)
        fixed_possessions[i] = kaos_u16(KAOS_FIXED_POSSESSIONS + i * 2);
}

#define GET(arr, i)       ((i) >= 0 && (i) < ARRAY_COUNT(arr) ? &(arr)[i] : NULL)
/* The table entry at i, or 0 outside the table: a defined value for a corrupt index. */
#define GET_VALUE(arr, i) ((i) >= 0 && (i) < ARRAY_COUNT(arr) ? (arr)[i] : 0)

const CreatureInfo *tables_get_creature_info(int16_t i)
{
    return GET(creature_info, i);
}
const CreatureSound *tables_get_creature_sound(int16_t i)
{
    return GET(creature_sounds, i);
}
const ObjectInfo *tables_get_object_info(int16_t i)
{
    return GET(object_info, i);
}
const WeaponInfo *tables_get_weapon_info(int16_t i)
{
    return GET(weapon_info, i);
}
const ArmourInfo *tables_get_armour_info(int16_t i)
{
    return GET(armour_info, i);
}
uint8_t tables_get_junk_weight(int16_t i)
{
    return GET_VALUE(junk_weights, i);
}
int16_t tables_get_food_value(int16_t i)
{
    return GET_VALUE(food_values, i);
}
const Spell *tables_get_spell(int16_t i)
{
    return i < loaded_spell_count ? GET(spells, i) : NULL;
}
uint8_t tables_get_symbol_mana_cost_multiplier(int16_t i)
{
    return GET_VALUE(symbol_mana_cost_multiplier, i);
}
const ActionSet *tables_get_action_set(int16_t i)
{
    return GET(action_sets, i);
}
const ActionInfo *tables_get_action_info(int16_t i)
{
    return GET(action_info, i);
}
int16_t tables_get_light_power_to_light_amount(int16_t i)
{
    return GET_VALUE(light_power_to_light_amount, i);
}
int16_t tables_get_palette_light_threshold(int16_t i)
{
    return GET_VALUE(palette_light_thresholds, i);
}
uint16_t tables_get_slot_mask(int16_t i)
{
    return GET_VALUE(slot_masks, i);
}
uint8_t tables_get_wound_defense_factor(int16_t i)
{
    return GET_VALUE(wound_defense_factor, i);
}
uint16_t tables_get_fixed_possession(int16_t i)
{
    return GET_VALUE(fixed_possessions, i);
}

uint8_t tables_get_symbol_base_mana_cost(int16_t step, int16_t symbol)
{
    if (step < 0 || step >= SYMBOL_STEP_COUNT)
        return 0;
    return GET_VALUE(symbol_base_mana_cost[step], symbol);
}

/* Fills the cached tables from the loaded image. */
static void decode_tables(void)
{
    read_creatures();
    read_objects();
    read_magic();
    read_actions();
    read_champion_tables();
}

int tables_load(const char *path)
{
    int r = kaos_load(path);
    if (!r)
        decode_tables();
    return r;
}

/* Map x step of one square in a direction. */
int16_t tables_get_step_east(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_TO_STEP_EAST + direction * 2);
}
int16_t tables_get_step_north(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_TO_STEP_NORTH + direction * 2);
}
int16_t tables_get_arrow_steps_forward(int16_t arrow)
{
    return kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD + arrow * 2);
}
int16_t tables_get_arrow_steps_right(int16_t arrow)
{
    return kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT + arrow * 2);
}
uint8_t tables_get_ordered_cell_to_attack(int16_t row, int16_t i)
{
    return kaos_u8(KAOS_ORDERED_CELLS_TO_ATTACK + (uint32_t)(row * ATTACK_CELL_COUNT) + i);
}
uint8_t tables_get_wound_probability_mask(int16_t index)
{
    return kaos_u8(KAOS_WOUND_PROBABILITY_INDEX_TO_MASK + index);
}
uint8_t tables_get_slot_to_steal_from(int16_t index)
{
    return kaos_u8(KAOS_SLOTS_TO_STEAL_FROM + index);
}
uint8_t tables_get_torch_icon_offset(int16_t charges)
{
    return kaos_u8(KAOS_TORCH_CHARGE_TO_ICON_OFFSET + charges);
}
uint16_t tables_get_slot_drop_order(int16_t i)
{
    return kaos_u16(KAOS_SLOT_DROP_ORDER + i * 2);
}
uint8_t tables_get_champion_color(int16_t champion_index)
{
    return kaos_u8(KAOS_CHAMPION_COLORS + champion_index);
}
uint8_t tables_get_square_type_event(int16_t square_type)
{
    return kaos_u8(KAOS_SQUARE_TYPE_TO_EVENT_TYPE + square_type);
}
int16_t tables_get_sound_graphic(int16_t sound)
{
    return kaos_s16(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound);
}
uint8_t tables_get_sound_rate(int16_t sound)
{
    return kaos_u8(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound + SOUND_ENTRY_RATE);
}
uint8_t tables_get_sound_priority(int16_t sound)
{
    return kaos_u8(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound + SOUND_ENTRY_PRIORITY);
}
uint8_t tables_get_sound_volume(int16_t ahead, int16_t side)
{
    uint32_t row = KAOS_SOUND_VOLUMES + (ahead + SOUND_HEARING_RANGE) * SOUND_VOLUME_ROW;
    return kaos_u8(row + SOUND_HEARING_RANGE + side);
}
uint8_t tables_get_new_game_extra_things(int16_t thing_type)
{
    return kaos_u8(KAOS_NEW_GAME_EXTRA_THINGS + thing_type);
}
uint16_t tables_get_door_attributes(int16_t door_set)
{
    return kaos_u16(KAOS_DOOR_INFO + door_set * 2);
}
const char *tables_get_message_escape(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_MESSAGE_ESCAPE_STRINGS + code * ESCAPE_STRING_BYTES);
}
const char *tables_get_inscription_escape(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_INSCRIPTION_ESCAPE_STRINGS + code * ESCAPE_STRING_BYTES);
}
const char *tables_get_escape_replacement(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_ESCAPE_REPLACEMENT_CHARS + code * REPLACEMENT_BYTES);
}
int16_t tables_get_portrait_width(void)
{
    return kaos_s16(KAOS_PORTRAIT_WIDTH);
}
int16_t tables_get_portrait_height(void)
{
    return kaos_s16(KAOS_PORTRAIT_HEIGHT);
}

/* Color word of a creature replacement color set at a light level (0-5). */
uint16_t tables_get_replacement_color(int16_t set, int16_t light)
{
    return kaos_u16(KAOS_CREATURE_REPLACEMENT_COLOR_SETS + REPLACEMENT_SET_BYTES * set + 2 * light);
}
uint8_t tables_get_replacement_change(int16_t set, int16_t depth)
{
    return kaos_u8(KAOS_CREATURE_REPLACEMENT_COLOR_SETS + REPLACEMENT_SET_BYTES * set + REPLACEMENT_AT_CHANGE_D2 +
                   depth - REPLACEMENT_FIRST_CHANGE_DEPTH);
}
const uint8_t *tables_get_creature_aspect(int16_t creature_type)
{
    return kaos_ptr(KAOS_CREATURE_ASPECTS + CREATURE_ASPECT_BYTES * creature_type);
}
const uint8_t *tables_get_object_aspect(int16_t index)
{
    return kaos_ptr(KAOS_OBJECT_ASPECTS + OBJECT_ASPECT_BYTES * index);
}
const uint8_t *tables_get_projectile_aspect(int16_t index)
{
    return kaos_ptr(KAOS_PROJECTILE_ASPECTS + PROJECTILE_ASPECT_BYTES * index);
}
const uint8_t *tables_get_field_aspect(int16_t index)
{
    return kaos_ptr(KAOS_FIELD_ASPECTS + FIELD_ASPECT_BYTES * index);
}
int16_t tables_get_creature_shift(int16_t distance, int16_t index)
{
    return (int8_t)kaos_u8(KAOS_CREATURE_SHIFT_SETS + CREATURE_SHIFT_SET_BYTES * distance + index); /* a signed byte */
}
uint8_t tables_get_pile_shift(int16_t pair, int16_t axis)
{
    return kaos_u8(KAOS_PILE_SHIFTS + PILE_SHIFT_BYTES * pair + axis);
}
uint8_t tables_get_object_scale(int16_t row)
{
    return kaos_u8(KAOS_OBJECT_SCALES + row);
}
uint8_t tables_get_projectile_scale(int16_t row)
{
    return kaos_u8(KAOS_PROJECTILE_SCALES + row);
}
uint8_t tables_get_explosion_scale(int16_t depth)
{
    return kaos_u8(KAOS_EXPLOSION_SCALES + depth);
}
uint8_t tables_get_rebirth_scale(int16_t step, int16_t coord)
{
    return kaos_u8((step == TABLES_REBIRTH_STEP_1 ? KAOS_REBIRTH_STEP_1_SCALES : KAOS_REBIRTH_SCALES) + coord);
}
uint8_t tables_get_group_directions(int16_t index)
{
    return kaos_u8(KAOS_GROUP_DIRECTIONS_PACKED + index);
}

ViewSquare tables_get_view_square(int16_t view_square)
{
    return (ViewSquare){
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_LANE + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_DEPTH + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_OBJECT_COORD + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_ALCOVE_COORD + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_CREATURE_COORD + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_EXPLOSION_COORD + view_square),
        (int8_t)kaos_u8(KAOS_VIEW_SQUARE_FIELD_ASPECT + view_square),
    };
}
int16_t tables_get_wall_set_layout_id(int16_t i)
{
    return kaos_s16(KAOS_WALL_SET_LAYOUT_IDS + 2 * i);
}
uint8_t tables_get_wall_ornament_depth(int16_t view)
{
    return kaos_u8(KAOS_WALL_ORNAMENT_DEPTHS + view);
}
uint8_t tables_get_plaque_height(int16_t lines, int16_t depth)
{
    return kaos_u8(KAOS_PLAQUE_HEIGHTS + lines + TABLES_PLAQUE_LINES * depth);
}
uint8_t tables_get_floor_ornament_view_offset(int16_t view)
{
    return kaos_u8(KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS + view);
}
uint8_t tables_get_alcove_ornament(int16_t k)
{
    return kaos_u8(KAOS_ALCOVE_ORNAMENT_INDICES + k);
}
uint8_t tables_get_fountain_ornament(void)
{
    return kaos_u8(KAOS_FOUNTAIN_ORNAMENT_INDICES);
}
uint8_t tables_get_wall_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_WALL_ORNAMENT_COORD_SETS + ornament);
}
uint8_t tables_get_floor_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_FLOOR_ORNAMENT_COORD_SETS + ornament);
}
uint8_t tables_get_door_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_DOOR_ORNAMENT_COORD_SETS + ornament);
}
uint8_t tables_get_door_button_coord_set(void)
{
    return kaos_u8(KAOS_DOOR_BUTTON_COORD_SETS);
}
int16_t tables_get_glyph_width(void)
{
    return kaos_s16(KAOS_WALL_TEXT_GLYPH_WIDTH);
}
int16_t tables_get_glyph_height(void)
{
    return kaos_s16(KAOS_WALL_TEXT_GLYPH_HEIGHT);
}
int16_t tables_get_viewport_x(void)
{
    return kaos_s16(KAOS_VIEWPORT_X);
}
int16_t tables_get_viewport_y(void)
{
    return kaos_s16(KAOS_VIEWPORT_Y);
}
int16_t tables_get_viewport_width(void)
{
    return kaos_s16(KAOS_VIEWPORT_WIDTH);
}
int16_t tables_get_viewport_height(void)
{
    return kaos_s16(KAOS_VIEWPORT_HEIGHT);
}

TablesRect tables_get_layout_bitmap(int16_t slot)
{
    uint32_t record = KAOS_LAYOUT_BITMAPS + LAYOUT_BITMAP_BYTES * slot;
    return (TablesRect){kaos_s16(record + LAYOUT_AT_X), kaos_s16(record + LAYOUT_AT_Y), kaos_s16(record + LAYOUT_AT_W),
                        kaos_s16(record + LAYOUT_AT_H)};
}

uint16_t tables_get_palette_color(int16_t palette, int16_t color)
{
    static const uint32_t first[TABLES_PALETTE_DUNGEON] = {KAOS_INITIAL_COLORMAP, KAOS_POINTER_PALETTE,
                                                           KAOS_CREDITS_PALETTE, KAOS_ENTRANCE_PALETTE};
    if (palette < TABLES_PALETTE_DUNGEON)
        return kaos_u16(first[palette] + 2 * color);
    return kaos_u16(KAOS_DUNGEON_PALETTES + PALETTE_BYTES * (palette - TABLES_PALETTE_DUNGEON) + 2 * color);
}
void tables_get_palette(int16_t palette, uint16_t colors[TABLES_PALETTE_COLOR_COUNT])
{
    for (int16_t i = 0; i < TABLES_PALETTE_COLOR_COUNT; i++)
        colors[i] = tables_get_palette_color(palette, i);
}

const uint8_t *tables_get_palette_changes(int16_t changes)
{
    static const uint32_t at[TABLES_CHANGES_COUNT] = {
        KAOS_PALETTE_CHANGES_D3,
        KAOS_PALETTE_CHANGES_D2,
        KAOS_PALETTE_CHANGES_DOOR_ORNAMENT_D3,
        KAOS_PALETTE_CHANGES_DOOR_ORNAMENT_D2,
        KAOS_PALETTE_CHANGES_SMOKE,
        KAOS_PALETTE_CHANGES_OBJECT_D3,
        KAOS_PALETTE_CHANGES_OBJECT_D2,
        KAOS_PALETTE_CHANGES_CREATURE_D3,
        KAOS_PALETTE_CHANGES_CREATURE_D2,
        KAOS_INVISIBILITY_PALETTE_CHANGES,
        KAOS_PALETTE_CHANGES_ACTION_ICON,
        KAOS_POINTER_SHADOW_COLORS,
    };
    return kaos_ptr(at[changes]);
}

/* First icon of an icon sheet. */
int16_t tables_get_icon_sheet_first(int16_t sheet)
{
    return kaos_s16(KAOS_ICON_GRAPHIC_FIRST_INDEX + 2 * sheet);
}
int16_t tables_get_slot_box_zone(int16_t slot_box)
{
    return kaos_s16(KAOS_SLOT_BOXES + SLOT_BOX_BYTES * slot_box);
}
int16_t tables_get_slot_box_border(void)
{
    return kaos_s16(KAOS_SLOT_BOX_BORDER);
}
int16_t tables_get_champion_icon_width(void)
{
    return kaos_s16(KAOS_CHAMPION_ICON_WIDTH);
}
int16_t tables_get_champion_icon_height(void)
{
    return kaos_s16(KAOS_CHAMPION_ICON_HEIGHT);
}
int16_t tables_get_bar_shadow(void)
{
    return kaos_s16(KAOS_BAR_SHADOW_OFFSET);
}
int16_t tables_get_damage_splat_width(bool large)
{
    return kaos_s16(large ? KAOS_DAMAGE_SPLAT_LARGE_WIDTH : KAOS_DAMAGE_SPLAT_SMALL_WIDTH);
}
int16_t tables_get_damage_splat_height(bool large)
{
    return kaos_s16(large ? KAOS_DAMAGE_SPLAT_LARGE_HEIGHT : KAOS_DAMAGE_SPLAT_SMALL_HEIGHT);
}
int16_t tables_get_skill_level_text(int16_t level)
{
    return kaos_s16(KAOS_SKILL_LEVEL_STRING_INDICES + 2 * (level - SKILL_LEVEL_TEXT_FIRST));
}
int16_t tables_get_base_skill_text(int16_t skill)
{
    return kaos_s16(KAOS_BASE_SKILL_STRING_INDICES + 2 * skill);
}
int16_t tables_get_statistic_text(int16_t statistic)
{
    return kaos_s16(KAOS_STATISTIC_STRING_INDICES + 2 * statistic);
}
int16_t tables_get_direction_text(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_STRING_INDICES + 2 * direction);
}
const char *tables_get_text(int16_t text)
{
    static const uint32_t at[TABLES_TEXT_COUNT] = {KAOS_LIST_SEPARATOR, KAOS_BONES_SEPARATOR, KAOS_ATTRIBUTES_PREFIX,
                                                   KAOS_ATTRIBUTES_SUFFIX, KAOS_DIALOG_VERSION_TEXT};
    return (const char *)kaos_ptr(at[text]);
}
int16_t tables_get_message_line_pad(void)
{
    return kaos_s16(KAOS_DIALOG_MESSAGE_LINE_PAD);
}
uint8_t tables_get_message_color(int16_t i)
{
    return kaos_s16(KAOS_DIALOG_MESSAGE_COLORS + 2 * i);
}
int16_t tables_get_button_border(int16_t axis)
{
    return kaos_s16(KAOS_ENDGAME_BUTTON_BORDER + 2 * axis);
}
int16_t tables_get_button_inner_border(int16_t axis)
{
    return kaos_s16(KAOS_ENDGAME_BUTTON_INNER_BORDER + 2 * axis);
}
TablesRect tables_get_csb_restart_box(bool inner)
{
    uint32_t at = CSB_RESTART_BOXES + (inner ? CSB_BOX_BYTES : 0);
    return (TablesRect){kaos_s16(at), kaos_s16(at + 2), kaos_s16(at + 4), kaos_s16(at + 6)};
}

PointerZone tables_get_pointer_zone(int16_t i)
{
    uint32_t z = KAOS_MOUSE_POINTER_ZONES + POINTER_ZONE_BYTES * i;
    return (PointerZone){kaos_s16(z + POINTER_AT_X), kaos_s16(z + POINTER_AT_Y),     kaos_s16(z + POINTER_AT_W),
                         kaos_s16(z + POINTER_AT_H), kaos_s16(z + POINTER_AT_SHAPE), kaos_s16(z + POINTER_AT_CHAMPION)};
}
int16_t tables_get_pointer_hotspot_x(int16_t shape)
{
    return kaos_s16(KAOS_MOUSE_POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + POINTER_AT_X);
}
int16_t tables_get_pointer_hotspot_y(int16_t shape)
{
    return kaos_s16(KAOS_MOUSE_POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + POINTER_AT_Y);
}
int16_t tables_get_pointer_shadow_dx(void)
{
    return kaos_s16(KAOS_POINTER_SHADOW_DX);
}
int16_t tables_get_pointer_shadow_dy(void)
{
    return kaos_s16(KAOS_POINTER_SHADOW_DY);
}
/* The bit planes of the arrow or the hand pointer and its size. */
const uint8_t *tables_get_pointer_planes(bool hand, int16_t *w, int16_t *h)
{
    *w = kaos_s16(hand ? KAOS_HAND_POINTER_WIDTH : KAOS_ARROW_POINTER_WIDTH);
    *h = kaos_s16(hand ? KAOS_HAND_POINTER_HEIGHT : KAOS_ARROW_POINTER_HEIGHT);
    return kaos_ptr(hand ? KAOS_HAND_POINTER_BITMAP : KAOS_ARROW_POINTER_BITMAP);
}

/* Image address of a mouse input table; the dialog tables come from a pointer table. */
static uint32_t mouse_table(int16_t table)
{
    static const uint32_t at[TABLES_MOUSE_DIALOG_FIRST] = {
        KAOS_MOUSE_INPUT_INTERFACE,    KAOS_MOUSE_INPUT_MOVEMENT,   KAOS_MOUSE_INPUT_INVENTORY,
        KAOS_MOUSE_INPUT_SLEEPING,     KAOS_MOUSE_INPUT_FROZEN,     KAOS_MOUSE_INPUT_ACTION_NAMES,
        KAOS_MOUSE_INPUT_ACTION_ICONS, KAOS_MOUSE_INPUT_SPELL_AREA, KAOS_MOUSE_INPUT_STATUS_BOX,
        KAOS_MOUSE_INPUT_CHEST,        KAOS_MOUSE_INPUT_RESURRECT,  KAOS_MOUSE_INPUT_NAME_ENTRY,
        KAOS_MOUSE_INPUT_ENTRANCE,     KAOS_MOUSE_INPUT_ENDGAME,
    };
    if (table >= TABLES_MOUSE_DIALOG_FIRST)
        return kaos_u32(KAOS_DIALOG_MOUSE_INPUTS + DIALOG_TABLE_POINTER_BYTES * (table - TABLES_MOUSE_DIALOG_FIRST));
    return at[table];
}
/* Entry of a mouse input table; false at the terminator. */
bool tables_get_mouse_input(int16_t table, int16_t entry, MouseInput *out)
{
    uint32_t a = mouse_table(table) + MOUSE_INPUT_BYTES * entry;
    int16_t command = kaos_s16(a + MOUSE_AT_COMMAND);
    if (!command)
        return false;
    *out = (MouseInput){command,
                        kaos_s16(a + MOUSE_AT_X1),
                        kaos_s16(a + MOUSE_AT_X2),
                        kaos_s16(a + MOUSE_AT_Y1),
                        kaos_s16(a + MOUSE_AT_Y2),
                        kaos_u16(a + MOUSE_AT_BUTTONS)};
    return true;
}
/* Writes x1, x2, y1, y2 of an entry as big-endian words, as the endgame screen does. */
void tables_set_mouse_input_box(int16_t table, int16_t entry, int16_t x1, int16_t x2, int16_t y1, int16_t y2)
{
    uint32_t a = mouse_table(table) + MOUSE_INPUT_BYTES * entry;
    kaos_write_u16(a + MOUSE_AT_X1, (uint16_t)x1);
    kaos_write_u16(a + MOUSE_AT_X2, (uint16_t)x2);
    kaos_write_u16(a + MOUSE_AT_Y1, (uint16_t)y1);
    kaos_write_u16(a + MOUSE_AT_Y2, (uint16_t)y2);
}
/* The command of a key in a keyboard table, or 0. */
int16_t tables_get_key_command(int16_t key_table, uint16_t key)
{
    static const uint32_t at[TABLES_KEYS_COUNT] = {KAOS_KEYS_ENTRANCE, KAOS_KEYS_INTERFACE, KAOS_KEYS_MOVEMENT,
                                                   KAOS_KEYS_SLEEPING, KAOS_KEYS_FROZEN};
    if (key_table == TABLES_KEYS_NONE)
        return 0;
    for (uint32_t entry = at[key_table]; kaos_s16(entry + KEY_AT_COMMAND); entry += KEY_ENTRY_BYTES)
        if (kaos_u16(entry + KEY_AT_KEY) == key)
            return kaos_s16(entry + KEY_AT_COMMAND);
    return 0;
}
