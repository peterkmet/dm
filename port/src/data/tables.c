#include "tables.h"
#include "spells.h"
#include "adventure.h"
#include "csbimage.h"
#include "ftl.h"

#define CREATURE_INFO 0x10192e
#define CREATURE_SOUNDS 0x101bec
#define OBJECT_INFO 0x101068
#define WEAPON_INFO 0x1014a0
#define ARMOUR_INFO 0x1015b4
#define JUNK_WEIGHTS 0x10169c
#define FOOD_VALUES 0x1016d2
#define SPELLS 0x103c84
#define SYMBOL_BASE_MANA_COST 0x103c66
#define SYMBOL_MANA_COST_MULTIPLIER 0x103c7e
#define ACTION_SETS 0x103d4c
#define ACTION_DISABLED_TICKS 0x103eac
#define ACTION_DAMAGE_FACTOR 0x103ed8
#define ACTION_HIT_PROBABILITY 0x103f04
#define ACTION_STAMINA 0x103f30
#define ACTION_DEFENSE 0x103f5c
#define ACTION_SKILL_INDEX 0x103f88
#define ACTION_EXPERIENCE_GAIN 0x103fb4
#define LIGHT_POWER_TO_LIGHT_AMOUNT 0x100cb4
#define PALETTE_LIGHT_THRESHOLDS 0x100cd4
#define SLOT_MASKS 0x100c68
#define WOUND_DEFENSE_FACTOR 0x100ce4
#define FIXED_POSSESSIONS 0x101c10

/* Record sizes in KAOS. */
#define CREATURE_INFO_BYTES 26
#define OBJECT_INFO_BYTES 6
#define WEAPON_INFO_BYTES 6
#define ARMOUR_INFO_BYTES 4
#define ACTION_SET_BYTES 8

/* Spell word 3: kind in bits 0-3, type in bits 4-9, disabled ticks in bits 10-15. */
#define SPELL_KIND(w) ((w) & 15)
#define SPELL_TYPE(w) ((w) >> 4 & 63)
#define SPELL_DISABLED_TICKS(w) ((w) >> 10)

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
        uint32_t a = CREATURE_INFO + i * CREATURE_INFO_BYTES;
        creature_info[i] = (CreatureInfo){
            kaos_u8(a), kaos_u8(a + 1), kaos_u16(a + 2), kaos_u16(a + 4),
            kaos_u8(a + 6), kaos_u8(a + 7), kaos_u8(a + 8), kaos_u8(a + 9),
            kaos_u8(a + 10), kaos_u8(a + 11), kaos_u8(a + 12),
            kaos_u16(a + 14), kaos_u16(a + 16), kaos_u16(a + 18), kaos_u16(a + 20),
            kaos_u16(a + 22), kaos_u8(a + 24),
        };
    }
    for (uint32_t i = 0; i < CREATURE_SOUND_COUNT; i++)
        creature_sounds[i] = (CreatureSound){(int8_t)kaos_u8(CREATURE_SOUNDS + i * 2), (int8_t)kaos_u8(CREATURE_SOUNDS + i * 2 + 1)};
}

/* Object, weapon, armour, junk, food, and door properties. */
static void read_objects(void)
{
    for (uint32_t i = 0; i < OBJECT_INFO_COUNT; i++) {
        uint32_t a = OBJECT_INFO + i * OBJECT_INFO_BYTES;
        object_info[i] = (ObjectInfo){kaos_u16(a), kaos_u8(a + 2), kaos_u8(a + 3), kaos_u16(a + 4)};
    }
    for (uint32_t i = 0; i < WEAPON_TYPE_COUNT; i++) {
        uint32_t a = WEAPON_INFO + i * WEAPON_INFO_BYTES;
        weapon_info[i] = (WeaponInfo){kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2), kaos_u8(a + 3), kaos_u16(a + 4)};
    }
    for (uint32_t i = 0; i < ARMOUR_TYPE_COUNT; i++) {
        uint32_t a = ARMOUR_INFO + i * ARMOUR_INFO_BYTES;
        armour_info[i] = (ArmourInfo){kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2)};
    }
    for (uint32_t i = 0; i < JUNK_TYPE_COUNT; i++)
        junk_weights[i] = kaos_u8(JUNK_WEIGHTS + i);
    for (uint32_t i = 0; i < FOOD_TYPE_COUNT; i++)
        food_values[i] = kaos_s16(FOOD_VALUES + i * 2);
    for (uint32_t i = 0; i < DOOR_TYPE_COUNT; i++)
        door_info[i] = (DoorInfo){kaos_u8(DOOR_INFO + i * 2), kaos_u8(DOOR_INFO + i * 2 + 1)};
}

/* Spells and the mana costs of the spell symbols. */
static void read_magic(void)
{
    int csb = g_adventure == ADVENTURE_CSB;
    loaded_spell_count = csb ? CSB_SPELL_COUNT : DM_SPELL_COUNT;
    for (uint32_t i = 0; i < (uint32_t)loaded_spell_count; i++) {
        uint32_t a = (csb ? CSB_SPELLS : SPELLS) + i * SPELL_BYTES;
        uint16_t w = kaos_u16(a + 6);
        spells[i] = (Spell){kaos_u32(a), kaos_u8(a + 4), kaos_u8(a + 5), SPELL_KIND(w), SPELL_TYPE(w), SPELL_DISABLED_TICKS(w)};
    }
    for (uint32_t s = 0; s < SYMBOL_STEP_COUNT; s++)
        for (uint32_t n = 0; n < SYMBOL_COUNT; n++)
            symbol_base_mana_cost[s][n] = kaos_u8(SYMBOL_BASE_MANA_COST + s * SYMBOL_COUNT + n);
    for (uint32_t i = 0; i < SYMBOL_COUNT; i++)
        symbol_mana_cost_multiplier[i] = kaos_u8(SYMBOL_MANA_COST_MULTIPLIER + i);
}

/* Action sets of the objects and the properties of each action. */
static void read_actions(void)
{
    for (uint32_t i = 0; i < ACTION_SET_COUNT; i++) {
        uint32_t a = ACTION_SETS + i * ACTION_SET_BYTES;
        action_sets[i] = (ActionSet){{kaos_u8(a), kaos_u8(a + 1), kaos_u8(a + 2)}, {kaos_u8(a + 4), kaos_u8(a + 5)}, kaos_u8(a + 6)};
    }
    for (uint32_t i = 0; i < ACTION_COUNT; i++)
        action_info[i] = (ActionInfo){
            kaos_u8(ACTION_DISABLED_TICKS + i), kaos_u8(ACTION_DAMAGE_FACTOR + i),
            kaos_u8(ACTION_HIT_PROBABILITY + i), kaos_u8(ACTION_STAMINA + i),
            (int8_t)kaos_u8(ACTION_DEFENSE + i), kaos_u8(ACTION_SKILL_INDEX + i),
            kaos_u8(ACTION_EXPERIENCE_GAIN + i),
        };
}

/* Light amounts, palette thresholds, slot masks, wound defense, and fixed possessions. */
static void read_champion_tables(void)
{
    for (uint32_t i = 0; i < LIGHT_POWER_COUNT; i++)
        light_power_to_light_amount[i] = kaos_s16(LIGHT_POWER_TO_LIGHT_AMOUNT + i * 2);
    for (uint32_t i = 0; i < PALETTE_LIGHT_THRESHOLD_COUNT; i++)
        palette_light_thresholds[i] = kaos_s16(PALETTE_LIGHT_THRESHOLDS + i * 2);
    for (uint32_t i = 0; i < SLOT_MASK_COUNT; i++)
        slot_masks[i] = kaos_u16(SLOT_MASKS + i * 2);
    for (uint32_t i = 0; i < WOUND_DEFENSE_FACTOR_COUNT; i++)
        wound_defense_factor[i] = kaos_u8(WOUND_DEFENSE_FACTOR + i);
    for (uint32_t i = 0; i < FIXED_POSSESSION_COUNT; i++)
        fixed_possessions[i] = kaos_u16(FIXED_POSSESSIONS + i * 2);
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

const CreatureInfo *tables_creature_info(int i) { return GET(creature_info, i); }
const CreatureSound *tables_creature_sound(int i) { return GET(creature_sounds, i); }
const ObjectInfo *tables_object_info(int i) { return GET(object_info, i); }
const WeaponInfo *tables_weapon_info(int i) { return GET(weapon_info, i); }
const ArmourInfo *tables_armour_info(int i) { return GET(armour_info, i); }
const uint8_t *tables_junk_weight(int i) { return GET(junk_weights, i); }
const int16_t *tables_food_value(int i) { return GET(food_values, i); }
const DoorInfo *tables_door_info(int i) { return GET(door_info, i); }
const Spell *tables_spell(int i) { return i < loaded_spell_count ? GET(spells, i) : NULL; }
const uint8_t *tables_symbol_mana_cost_multiplier(int i) { return GET(symbol_mana_cost_multiplier, i); }
const ActionSet *tables_action_set(int i) { return GET(action_sets, i); }
const ActionInfo *tables_action_info(int i) { return GET(action_info, i); }
const int16_t *tables_light_power_to_light_amount(int i) { return GET(light_power_to_light_amount, i); }
const int16_t *tables_palette_light_threshold(int i) { return GET(palette_light_thresholds, i); }
const uint16_t *tables_slot_mask(int i) { return GET(slot_masks, i); }
const uint8_t *tables_wound_defense_factor(int i) { return GET(wound_defense_factor, i); }
const uint16_t *tables_fixed_possession(int i) { return GET(fixed_possessions, i); }

const uint8_t *tables_symbol_base_mana_cost(int step, int symbol)
{
    const uint8_t (*row)[SYMBOL_COUNT] = GET(symbol_base_mana_cost, step);
    return row ? GET(*row, symbol) : NULL;
}
