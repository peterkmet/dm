#include "tables.h"
#include "spells.h"
#include "adventure.h"
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

enum { SOUND_ENTRY_RATE = 3, SOUND_ENTRY_PRIORITY = 4 };                        /* byte offsets in a sound entry */
enum { ATTACK_CELL_COUNT = 4, SOUND_VOLUME_ROW = 2 * SOUND_HEARING_RANGE + 1 }; /* row lengths */

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
