#include "tables.h"
#include "spells.h"
#include "adventure.h"
#include "bytes.h"
#include "csbimage.h"
#include "ftl.h"
#include "kaos_addresses.h"

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

enum { SOUND_ENTRY_RATE = 3, SOUND_ENTRY_PRIORITY = 4 };                        /* byte offsets in a sound entry */
enum { ATTACK_CELL_COUNT = 4, SOUND_VOLUME_ROW = 2 * SOUND_HEARING_RANGE + 1 }; /* row lengths */

/* Byte offsets in a replacement color set: a color word per light level, then the D2 and the D3 change. */
enum { REPLACEMENT_AT_CHANGE_D2 = 12, REPLACEMENT_FIRST_CHANGE_DEPTH = 2 };
/* Byte offsets of the words of a pointer zone and of a pointer hotspot. */
enum {
    POINTER_AT_X,
    POINTER_AT_Y = 2,
    POINTER_AT_W = 4,
    POINTER_AT_H = 6,
    POINTER_AT_SHAPE = 8,
    POINTER_AT_CHAMPION = 10
};
/* Byte offsets of the words of a mouse input entry and of a key entry. */
enum { MOUSE_AT_COMMAND, MOUSE_AT_X1 = 2, MOUSE_AT_X2 = 4, MOUSE_AT_Y1 = 6, MOUSE_AT_Y2 = 8, MOUSE_AT_BUTTONS = 10 };
enum { KEY_AT_COMMAND, KEY_AT_KEY = 2 };
enum { SKILL_LEVEL_TEXT_FIRST = 2 };                                         /* the level names start at level 2 */
enum { LAYOUT_AT_X = 2, LAYOUT_AT_Y = 4, LAYOUT_AT_W = 6, LAYOUT_AT_H = 8 }; /* byte offsets in a layout bitmap */

/* Byte offsets of the creature info fields (docs/tables.md); bytes 13 and 25 are pad bytes. */
enum {
    CREATURE_AT_ASPECT_INDEX,
    CREATURE_AT_ATTACK_SOUND_ORDINAL,
    CREATURE_AT_ATTRIBUTES,
    CREATURE_AT_GRAPHIC_INFO = 4,
    CREATURE_AT_MOVEMENT_TICKS = 6,
    CREATURE_AT_ATTACK_TICKS,
    CREATURE_AT_DEFENSE,
    CREATURE_AT_BASE_HEALTH,
    CREATURE_AT_ATTACK,
    CREATURE_AT_POISON_ATTACK,
    CREATURE_AT_DEXTERITY,
    CREATURE_AT_RANGES = 14,
    CREATURE_AT_PROPERTIES = 16,
    CREATURE_AT_RESISTANCES = 18,
    CREATURE_AT_ANIMATION_TICKS = 20,
    CREATURE_AT_WOUND_PROBABILITIES = 22,
    CREATURE_AT_ATTACK_TYPE = 24,
};

static int ready;
static CreatureInfo creature_info[CREATURE_TYPE_COUNT];
static CreatureSound creature_sounds[CREATURE_SOUND_COUNT];
static ObjectInfo object_info[OBJECT_INFO_COUNT];
static WeaponInfo weapon_info[WEAPON_TYPE_COUNT];
static ArmourInfo armour_info[ARMOUR_TYPE_COUNT];
static uint8_t junk_weights[JUNK_TYPE_COUNT];
static int16_t food_values[FOOD_TYPE_COUNT];
static DoorInfo door_info[DOOR_TYPE_COUNT];
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
            kaos_u8(a + CREATURE_AT_ASPECT_INDEX),     kaos_u8(a + CREATURE_AT_ATTACK_SOUND_ORDINAL),
            kaos_u16(a + CREATURE_AT_ATTRIBUTES),      kaos_u16(a + CREATURE_AT_GRAPHIC_INFO),
            kaos_u8(a + CREATURE_AT_MOVEMENT_TICKS),   kaos_u8(a + CREATURE_AT_ATTACK_TICKS),
            kaos_u8(a + CREATURE_AT_DEFENSE),          kaos_u8(a + CREATURE_AT_BASE_HEALTH),
            kaos_u8(a + CREATURE_AT_ATTACK),           kaos_u8(a + CREATURE_AT_POISON_ATTACK),
            kaos_u8(a + CREATURE_AT_DEXTERITY),        kaos_u16(a + CREATURE_AT_RANGES),
            kaos_u16(a + CREATURE_AT_PROPERTIES),      kaos_u16(a + CREATURE_AT_RESISTANCES),
            kaos_u16(a + CREATURE_AT_ANIMATION_TICKS), kaos_u16(a + CREATURE_AT_WOUND_PROBABILITIES),
            kaos_u8(a + CREATURE_AT_ATTACK_TYPE),
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
        object_info[i] = (ObjectInfo){kaos_u16(a), kaos_u8(a + 2), kaos_u8(a + 3), kaos_u16(a + 4)};
    }
    for (uint32_t i = 0; i < WEAPON_TYPE_COUNT; i++) {
        uint32_t a = KAOS_WEAPON_INFO + i * WEAPON_INFO_BYTES;
        weapon_info[i] = (WeaponInfo){kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2), kaos_u8(a + 3), kaos_u16(a + 4)};
    }
    for (uint32_t i = 0; i < ARMOUR_TYPE_COUNT; i++) {
        uint32_t a = KAOS_ARMOUR_INFO + i * ARMOUR_INFO_BYTES;
        armour_info[i] = (ArmourInfo){kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2)};
    }
    for (uint32_t i = 0; i < JUNK_TYPE_COUNT; i++)
        junk_weights[i] = kaos_u8(KAOS_JUNK_WEIGHTS + i);
    for (uint32_t i = 0; i < FOOD_TYPE_COUNT; i++)
        food_values[i] = kaos_s16(KAOS_FOOD_VALUES + i * 2);
    for (uint32_t i = 0; i < DOOR_TYPE_COUNT; i++)
        door_info[i] = (DoorInfo){kaos_u8(KAOS_DOOR_INFO + i * 2), kaos_u8(KAOS_DOOR_INFO + i * 2 + 1)};
}

/* Spells and the mana costs of the spell symbols. */
static void read_magic(void)
{
    int csb = g_adventure == ADVENTURE_CSB;
    loaded_spell_count = csb ? CSB_SPELL_COUNT : DM_SPELL_COUNT;
    for (uint32_t i = 0; i < (uint32_t)loaded_spell_count; i++) {
        uint32_t a = (csb ? CSB_SPELLS : KAOS_SPELLS) + i * SPELL_BYTES;
        uint16_t w = kaos_u16(a + SPELL_ENTRY_ATTRIBUTES);
        spells[i] =
            (Spell){kaos_u32(a),         kaos_u8(a + SPELL_ENTRY_LEVEL),          kaos_u8(a + SPELL_ENTRY_SKILL),
                    w & SPELL_KIND_MASK, w >> SPELL_TYPE_SHIFT & SPELL_TYPE_MASK, w >> SPELL_DISABLED_TICKS_SHIFT};
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
            {kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2)},
            {kaos_u8(a + 4), kaos_u8(a + 5)},
            kaos_u8(a + 6)
        };
    }
    for (uint32_t i = 0; i < ACTION_COUNT; i++)
        action_info[i] = (ActionInfo){
            kaos_u8(KAOS_ACTIONS_DISABLED + i),       kaos_u8(KAOS_ACTION_DAMAGE_FACTOR + i),
            kaos_u8(KAOS_ACTION_HIT_PROBABILITY + i), kaos_u8(KAOS_ACTION_STAMINA + i),
            (int8_t)kaos_u8(KAOS_ACTION_DEFENSE + i), kaos_u8(KAOS_ACTION_SKILL_INDEX + i),
            kaos_u8(KAOS_ACTION_EXPERIENCE_GAIN + i),
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

int tables_init(void)
{
    if (!kaos.data)
        return -1;
    read_creatures();
    read_objects();
    read_magic();
    read_actions();
    read_champion_tables();
    ready = 1;
    return 0;
}

#define GET(arr, i) (ready && (unsigned)(i) < sizeof(arr) / sizeof((arr)[0]) ? &(arr)[i] : NULL)

const CreatureInfo *tables_creature_info(int i)
{
    return GET(creature_info, i);
}
const CreatureSound *tables_creature_sound(int i)
{
    return GET(creature_sounds, i);
}
const ObjectInfo *tables_object_info(int i)
{
    return GET(object_info, i);
}
const WeaponInfo *tables_weapon_info(int i)
{
    return GET(weapon_info, i);
}
const ArmourInfo *tables_armour_info(int i)
{
    return GET(armour_info, i);
}
const uint8_t *tables_junk_weight(int i)
{
    return GET(junk_weights, i);
}
const int16_t *tables_food_value(int i)
{
    return GET(food_values, i);
}
const DoorInfo *tables_door_info(int i)
{
    return GET(door_info, i);
}
const Spell *tables_spell(int i)
{
    return i < loaded_spell_count ? GET(spells, i) : NULL;
}
const uint8_t *tables_symbol_mana_cost_multiplier(int i)
{
    return GET(symbol_mana_cost_multiplier, i);
}
const ActionSet *tables_action_set(int i)
{
    return GET(action_sets, i);
}
const ActionInfo *tables_action_info(int i)
{
    return GET(action_info, i);
}
const int16_t *tables_light_power_to_light_amount(int i)
{
    return GET(light_power_to_light_amount, i);
}
const int16_t *tables_palette_light_threshold(int i)
{
    return GET(palette_light_thresholds, i);
}
const uint16_t *tables_slot_mask(int i)
{
    return GET(slot_masks, i);
}
const uint8_t *tables_wound_defense_factor(int i)
{
    return GET(wound_defense_factor, i);
}
const uint16_t *tables_fixed_possession(int i)
{
    return GET(fixed_possessions, i);
}

const uint8_t *tables_symbol_base_mana_cost(int step, int symbol)
{
    const uint8_t (*row)[SYMBOL_COUNT] = GET(symbol_base_mana_cost, step);
    return row ? GET(*row, symbol) : NULL;
}

int tables_load(const char *path)
{
    int r = kaos_load(path);
    return r ? r : tables_init();
}

/* Map x step of one square in a direction. */
int16_t tables_step_east(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_TO_STEP_EAST + direction * 2);
}
/* Map y step of one square in a direction. */
int16_t tables_step_north(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_TO_STEP_NORTH + direction * 2);
}
/* Squares forward of a movement arrow. */
int16_t tables_arrow_steps_forward(int16_t arrow)
{
    return kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD + arrow * 2);
}
/* Squares to the right of a movement arrow. */
int16_t tables_arrow_steps_right(int16_t arrow)
{
    return kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT + arrow * 2);
}
/* Cell i in the order that an attacker from row (side and direction) reaches the cells of the target square. */
uint8_t tables_ordered_cell_to_attack(int16_t row, int16_t i)
{
    return kaos_u8(KAOS_ORDERED_CELLS_TO_ATTACK + (uint32_t)(row * ATTACK_CELL_COUNT) + i);
}
/* Wound mask of a wound probability index. */
uint8_t tables_wound_probability_mask(int16_t index)
{
    return kaos_u8(KAOS_WOUND_PROBABILITY_INDEX_TO_MASK + index);
}
/* Inventory slot that a thief tries for a random index. */
uint8_t tables_slot_to_steal_from(int16_t index)
{
    return kaos_u8(KAOS_SLOTS_TO_STEAL_FROM + index);
}
/* Icon offset of a lit torch by its charges. */
uint8_t tables_torch_icon_offset(int16_t charges)
{
    return kaos_u8(KAOS_TORCH_CHARGE_TO_ICON_OFFSET + charges);
}
/* Slot i in the order that a dead champion drops the objects. */
uint16_t tables_slot_drop_order(int16_t i)
{
    return kaos_u16(KAOS_SLOT_DROP_ORDER + i * 2);
}
/* Message color of a champion. */
uint8_t tables_champion_color(int16_t champion_index)
{
    return kaos_u8(KAOS_CHAMPION_COLORS + champion_index);
}
/* Event type that a sensor sends to a square of this type. */
uint8_t tables_square_type_event(int16_t square_type)
{
    return kaos_u8(KAOS_SQUARE_TYPE_TO_EVENT_TYPE + square_type);
}
/* Graphic of a sound; negative when the sound has no sample. */
int16_t tables_sound_graphic(int16_t sound)
{
    return kaos_s16(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound);
}
/* Sample rate of a sound. */
uint8_t tables_sound_rate(int16_t sound)
{
    return kaos_u8(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound + SOUND_ENTRY_RATE);
}
/* Priority of a sound: a higher value replaces a pending sound of the same loudness. */
uint8_t tables_sound_priority(int16_t sound)
{
    return kaos_u8(KAOS_SOUNDS + SOUND_ENTRY_BYTES * sound + SOUND_ENTRY_PRIORITY);
}
/* Volume of one channel for a sound at squares ahead and squares toward the side of that channel. */
uint8_t tables_sound_volume(int16_t ahead, int16_t side)
{
    uint32_t row = KAOS_SOUND_VOLUMES + (ahead + SOUND_HEARING_RANGE) * SOUND_VOLUME_ROW;
    return kaos_u8(row + SOUND_HEARING_RANGE + side);
}
/* Free records of a thing type that a new game adds to the dungeon. */
uint8_t tables_new_game_extra_things(int16_t thing_type)
{
    return kaos_u8(KAOS_NEW_GAME_EXTRA_THINGS + thing_type);
}
/* Door info word of a door set: attributes in the high byte, defense in the low byte. */
uint16_t tables_door_attributes(int16_t door_set)
{
    return kaos_u16(KAOS_DOOR_INFO + door_set * 2);
}
/* String of a message escape code. */
const char *tables_message_escape(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_MESSAGE_ESCAPE_STRINGS + code * ESCAPE_STRING_BYTES);
}
/* String of a wall inscription escape code. */
const char *tables_inscription_escape(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_INSCRIPTION_ESCAPE_STRINGS + code * ESCAPE_STRING_BYTES);
}
/* Replacement string of an escape code. */
const char *tables_escape_replacement(int16_t code)
{
    return (const char *)kaos_ptr(KAOS_ESCAPE_REPLACEMENT_CHARS + code * REPLACEMENT_BYTES);
}
/* Width of a champion portrait. */
int16_t tables_portrait_width(void)
{
    return kaos_s16(KAOS_PORTRAIT_WIDTH);
}
/* Height of a champion portrait. */
int16_t tables_portrait_height(void)
{
    return kaos_s16(KAOS_PORTRAIT_HEIGHT);
}

/* Color word of a creature replacement color set at a light level (0-5). */
uint16_t tables_replacement_color(int16_t set, int16_t light)
{
    return kaos_u16(KAOS_CREATURE_REPLACEMENT_COLOR_SETS + REPLACEMENT_SET_BYTES * set + 2 * light);
}
/* Palette change of a creature replacement color set at depth 2 or 3. */
uint8_t tables_replacement_change(int16_t set, int16_t depth)
{
    return kaos_u8(KAOS_CREATURE_REPLACEMENT_COLOR_SETS + REPLACEMENT_SET_BYTES * set + REPLACEMENT_AT_CHANGE_D2 +
                   depth - REPLACEMENT_FIRST_CHANGE_DEPTH);
}
/* Aspect record of a creature: graphic, coordinate set, replacement color sets. */
const uint8_t *tables_creature_aspect(int16_t creature_type)
{
    return kaos_ptr(KAOS_CREATURE_ASPECTS + CREATURE_ASPECT_BYTES * creature_type);
}
/* Aspect record of an object: graphic, flags, alcove coordinate set. */
const uint8_t *tables_object_aspect(int16_t index)
{
    return kaos_ptr(KAOS_OBJECT_ASPECTS + OBJECT_ASPECT_BYTES * index);
}
/* Aspect record of a projectile (index 1-14): graphic, pose and scale flags. */
const uint8_t *tables_projectile_aspect(int16_t index)
{
    return kaos_ptr(KAOS_PROJECTILE_ASPECTS + PROJECTILE_ASPECT_BYTES * index);
}
/* Aspect record of a field: the teleporter or fluxcage pattern of a view square. */
const uint8_t *tables_field_aspect(int16_t index)
{
    return kaos_ptr(KAOS_FIELD_ASPECTS + FIELD_ASPECT_BYTES * index);
}
/* Pixel shift index of a creature or a stacked object at a distance (0-2). */
int16_t tables_creature_shift(int16_t distance, int16_t index)
{
    return (int8_t)kaos_u8(KAOS_CREATURE_SHIFT_SETS + CREATURE_SHIFT_SET_BYTES * distance + index);
}
/* Shift index of a stacked object on one axis (0 x, 1 y). */
uint8_t tables_pile_shift(int16_t pair, int16_t axis)
{
    return kaos_u8(KAOS_PILE_SHIFTS + PILE_SHIFT_BYTES * pair + axis);
}
/* Scale of an object at a view row. */
uint8_t tables_object_scale(int16_t row)
{
    return kaos_u8(KAOS_OBJECT_SCALES + row);
}
/* Scale of a projectile at a view row. */
uint8_t tables_projectile_scale(int16_t row)
{
    return kaos_u8(KAOS_PROJECTILE_SCALES + row);
}
/* Scale of an explosion at a depth. */
uint8_t tables_explosion_scale(int16_t depth)
{
    return kaos_u8(KAOS_EXPLOSION_SCALES + depth);
}
/* Scale of rebirth step 1 or 2 at an object coordinate set. */
uint8_t tables_rebirth_scale(int16_t step, int16_t coord)
{
    return kaos_u8((step == TABLES_REBIRTH_STEP_1 ? KAOS_REBIRTH_STEP_1_SCALES : KAOS_REBIRTH_SCALES) + coord);
}
/* The 4 creature directions of a group that faces one direction, 2 bits per creature. */
uint8_t tables_group_directions(int16_t index)
{
    return kaos_u8(KAOS_GROUP_DIRECTIONS_PACKED + index);
}

/* Lane, depth, and coordinate sets of a view square. */
ViewSquare tables_view_square(int16_t view_square)
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
/* Layout bitmap id of wall set entry i. */
int16_t tables_wall_set_layout_id(int16_t i)
{
    return kaos_s16(KAOS_WALL_SET_LAYOUT_IDS + 2 * i);
}
/* Depth (0-4) of a wall ornament view. */
uint8_t tables_wall_ornament_depth(int16_t view)
{
    return kaos_u8(KAOS_WALL_ORNAMENT_DEPTHS + view);
}
/* Visible plaque height of an inscription of 1-3 lines at a depth. */
uint8_t tables_plaque_height(int16_t lines, int16_t depth)
{
    return kaos_u8(KAOS_PLAQUE_HEIGHTS + lines + TABLES_PLAQUE_LINES * depth);
}
/* Graphic offset of a floor ornament view. */
uint8_t tables_floor_ornament_view_offset(int16_t view)
{
    return kaos_u8(KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS + view);
}
/* Wall ornament k of the alcove ornaments. */
uint8_t tables_alcove_ornament(int16_t k)
{
    return kaos_u8(KAOS_ALCOVE_ORNAMENT_INDICES + k);
}
/* The wall ornament that is a fountain. */
uint8_t tables_fountain_ornament(void)
{
    return kaos_u8(KAOS_FOUNTAIN_ORNAMENT_INDICES);
}
/* Coordinate set of a wall ornament graphic. */
uint8_t tables_wall_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_WALL_ORNAMENT_COORD_SETS + ornament);
}
/* Coordinate set of a floor ornament graphic. */
uint8_t tables_floor_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_FLOOR_ORNAMENT_COORD_SETS + ornament);
}
/* Coordinate set of a door ornament graphic. */
uint8_t tables_door_ornament_coord_set(int16_t ornament)
{
    return kaos_u8(KAOS_DOOR_ORNAMENT_COORD_SETS + ornament);
}
/* Coordinate set of the door button. */
uint8_t tables_door_button_coord_set(void)
{
    return kaos_u8(KAOS_DOOR_BUTTON_COORD_SETS);
}
/* Width of a wall text glyph. */
int16_t tables_glyph_width(void)
{
    return kaos_s16(KAOS_WALL_TEXT_GLYPH_WIDTH);
}
/* Height of a wall text glyph. */
int16_t tables_glyph_height(void)
{
    return kaos_s16(KAOS_WALL_TEXT_GLYPH_HEIGHT);
}
/* Screen x of the viewport. */
int16_t tables_viewport_x(void)
{
    return kaos_s16(KAOS_VIEWPORT_X);
}
/* Screen y of the viewport. */
int16_t tables_viewport_y(void)
{
    return kaos_s16(KAOS_VIEWPORT_Y);
}
/* Width of the viewport. */
int16_t tables_viewport_width(void)
{
    return kaos_s16(KAOS_VIEWPORT_WIDTH);
}
/* Height of the viewport. */
int16_t tables_viewport_height(void)
{
    return kaos_s16(KAOS_VIEWPORT_HEIGHT);
}

/* Source rectangle of layout bitmap slot in its graphic. */
TablesRect tables_layout_bitmap(int16_t slot)
{
    uint32_t record = KAOS_LAYOUT_BITMAPS + LAYOUT_BITMAP_BYTES * slot;
    return (TablesRect){kaos_s16(record + LAYOUT_AT_X), kaos_s16(record + LAYOUT_AT_Y), kaos_s16(record + LAYOUT_AT_W),
                        kaos_s16(record + LAYOUT_AT_H)};
}

/* One color of a palette. */
uint16_t tables_palette_color(int16_t palette, int16_t color)
{
    static const uint32_t first[TABLES_PALETTE_DUNGEON] = {KAOS_INITIAL_COLORMAP, KAOS_POINTER_PALETTE,
                                                           KAOS_CREDITS_PALETTE, KAOS_ENTRANCE_PALETTE};
    uint32_t at = palette < TABLES_PALETTE_DUNGEON
                      ? first[palette]
                      : KAOS_DUNGEON_PALETTES + PALETTE_BYTES * (palette - TABLES_PALETTE_DUNGEON);
    return kaos_u16(at + 2 * color);
}
/* The colors of a palette. */
void tables_palette(int16_t palette, uint16_t colors[TABLES_PALETTE_COLOR_COUNT])
{
    for (int16_t i = 0; i < TABLES_PALETTE_COLOR_COUNT; i++)
        colors[i] = tables_palette_color(palette, i);
}

/* The color map of a palette change. */
const uint8_t *tables_palette_changes(int16_t changes)
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
int16_t tables_icon_sheet_first(int16_t sheet)
{
    return kaos_s16(KAOS_ICON_GRAPHIC_FIRST_INDEX + 2 * sheet);
}
/* Layout zone of a slot box. */
int16_t tables_slot_box_zone(int16_t slot_box)
{
    return kaos_s16(KAOS_SLOT_BOXES + SLOT_BOX_BYTES * slot_box);
}
/* Pixels that a bordered slot box grows on each side. */
int16_t tables_slot_box_border(void)
{
    return kaos_s16(KAOS_SLOT_BOX_BORDER);
}
/* Width of a champion party icon. */
int16_t tables_champion_icon_width(void)
{
    return kaos_s16(KAOS_CHAMPION_ICON_WIDTH);
}
/* Height of a champion party icon. */
int16_t tables_champion_icon_height(void)
{
    return kaos_s16(KAOS_CHAMPION_ICON_HEIGHT);
}
/* Offset of the shadow of a panel bar. */
int16_t tables_bar_shadow(void)
{
    return kaos_s16(KAOS_BAR_SHADOW_OFFSET);
}
/* Width of the small or the large damage splat. */
int16_t tables_damage_splat_width(bool large)
{
    return kaos_s16(large ? KAOS_DAMAGE_SPLAT_LARGE_WIDTH : KAOS_DAMAGE_SPLAT_SMALL_WIDTH);
}
/* Height of the small or the large damage splat. */
int16_t tables_damage_splat_height(bool large)
{
    return kaos_s16(large ? KAOS_DAMAGE_SPLAT_LARGE_HEIGHT : KAOS_DAMAGE_SPLAT_SMALL_HEIGHT);
}
/* Text of a skill level name (level 2-16). */
int16_t tables_skill_level_text(int16_t level)
{
    return kaos_s16(KAOS_SKILL_LEVEL_STRING_INDICES + 2 * (level - SKILL_LEVEL_TEXT_FIRST));
}
/* Text of a base skill name. */
int16_t tables_base_skill_text(int16_t skill)
{
    return kaos_s16(KAOS_BASE_SKILL_STRING_INDICES + 2 * skill);
}
/* Text of a statistic name. */
int16_t tables_statistic_text(int16_t statistic)
{
    return kaos_s16(KAOS_STATISTIC_STRING_INDICES + 2 * statistic);
}
/* Text of a direction name. */
int16_t tables_direction_text(int16_t direction)
{
    return kaos_s16(KAOS_DIRECTION_STRING_INDICES + 2 * direction);
}
/* A short text of the image. */
const char *tables_text(int16_t text)
{
    static const uint32_t at[TABLES_TEXT_COUNT] = {KAOS_LIST_SEPARATOR, KAOS_BONES_SEPARATOR, KAOS_ATTRIBUTES_PREFIX,
                                                   KAOS_ATTRIBUTES_SUFFIX, KAOS_DIALOG_VERSION_TEXT};
    return (const char *)kaos_ptr(at[text]);
}
/* Pixels above and below a dialog message line. */
int16_t tables_message_line_pad(void)
{
    return kaos_s16(KAOS_DIALOG_MESSAGE_LINE_PAD);
}
/* Color of dialog message i. */
uint8_t tables_message_color(int16_t i)
{
    return (uint8_t)kaos_s16(KAOS_DIALOG_MESSAGE_COLORS + 2 * i);
}
/* Outer border of an endgame button on one axis. */
int16_t tables_button_border(int16_t axis)
{
    return kaos_s16(KAOS_ENDGAME_BUTTON_BORDER + 2 * axis);
}
/* Inner border of an endgame button on one axis. */
int16_t tables_button_inner_border(int16_t axis)
{
    return kaos_s16(KAOS_ENDGAME_BUTTON_INNER_BORDER + 2 * axis);
}
/* CSB: the outer or the inner box of the RESTART THIS GAME button. */
TablesRect tables_csb_restart_box(bool inner)
{
    uint32_t at = CSB_RESTART_BOXES + (inner ? CSB_BOX_BYTES : 0);
    return (TablesRect){kaos_s16(at), kaos_s16(at + 2), kaos_s16(at + 4), kaos_s16(at + 6)};
}

/* Pointer zone i: a screen box, the pointer shape, and the champion + 1 of a status box. */
PointerZone tables_pointer_zone(int16_t i)
{
    uint32_t z = KAOS_MOUSE_POINTER_ZONES + POINTER_ZONE_BYTES * i;
    return (PointerZone){kaos_s16(z + POINTER_AT_X), kaos_s16(z + POINTER_AT_Y),     kaos_s16(z + POINTER_AT_W),
                         kaos_s16(z + POINTER_AT_H), kaos_s16(z + POINTER_AT_SHAPE), kaos_s16(z + POINTER_AT_CHAMPION)};
}
/* Hotspot x of a pointer shape. */
int16_t tables_pointer_hotspot_x(int16_t shape)
{
    return kaos_s16(KAOS_MOUSE_POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + POINTER_AT_X);
}
/* Hotspot y of a pointer shape. */
int16_t tables_pointer_hotspot_y(int16_t shape)
{
    return kaos_s16(KAOS_MOUSE_POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + POINTER_AT_Y);
}
/* Shadow offset x of the object pointer. */
int16_t tables_pointer_shadow_dx(void)
{
    return kaos_s16(KAOS_POINTER_SHADOW_DX);
}
/* Shadow offset y of the object pointer. */
int16_t tables_pointer_shadow_dy(void)
{
    return kaos_s16(KAOS_POINTER_SHADOW_DY);
}
/* The bit planes of the arrow or the hand pointer and its size. */
const uint8_t *tables_pointer_planes(bool hand, int16_t *w, int16_t *h)
{
    *w = (int16_t)kaos_u16(hand ? KAOS_HAND_POINTER_WIDTH : KAOS_ARROW_POINTER_WIDTH);
    *h = (int16_t)kaos_u16(hand ? KAOS_HAND_POINTER_HEIGHT : KAOS_ARROW_POINTER_HEIGHT);
    return kaos_ptr(hand ? KAOS_HAND_POINTER_BITMAP : KAOS_ARROW_POINTER_BITMAP);
}

/* Image address of a mouse input table; the dialog tables come from a pointer table. */
static uint32_t mouse_table(int16_t table)
{
    static const uint32_t at[UI_INPUT_DIALOG_FIRST] = {
        KAOS_MOUSE_INPUT_INTERFACE,    KAOS_MOUSE_INPUT_MOVEMENT,   KAOS_MOUSE_INPUT_INVENTORY,
        KAOS_MOUSE_INPUT_SLEEPING,     KAOS_MOUSE_INPUT_FROZEN,     KAOS_MOUSE_INPUT_ACTION_NAMES,
        KAOS_MOUSE_INPUT_ACTION_ICONS, KAOS_MOUSE_INPUT_SPELL_AREA, KAOS_MOUSE_INPUT_STATUS_BOX,
        KAOS_MOUSE_INPUT_CHEST,        KAOS_MOUSE_INPUT_RESURRECT,  KAOS_MOUSE_INPUT_NAME_ENTRY,
        KAOS_MOUSE_INPUT_ENTRANCE,     KAOS_MOUSE_INPUT_ENDGAME,
    };
    if (table >= UI_INPUT_DIALOG_FIRST)
        return kaos_u32(KAOS_DIALOG_MOUSE_INPUTS + DIALOG_TABLE_POINTER_BYTES * (table - UI_INPUT_DIALOG_FIRST));
    return at[table];
}
/* Entry of a mouse input table; false at the terminator. */
bool tables_mouse_input(int16_t table, int16_t entry, MouseInput *out)
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
    uint8_t *a = kaos_ptr(mouse_table(table) + MOUSE_INPUT_BYTES * entry);
    WR16(a + MOUSE_AT_X1, x1);
    WR16(a + MOUSE_AT_X2, x2);
    WR16(a + MOUSE_AT_Y1, y1);
    WR16(a + MOUSE_AT_Y2, y2);
}
/* The command of a key in a keyboard table, or 0. */
int16_t tables_key_command(int16_t key_table, uint16_t key)
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
