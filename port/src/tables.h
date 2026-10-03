#ifndef TABLES_H
#define TABLES_H
#include <stdint.h>

/* Static rule tables of KAOS.FTL, decoded once from the loaded image. Field bits: docs/tables.md. */

enum {
    CREATURE_TYPE_COUNT = 27,
    CREATURE_SOUND_COUNT = 18,
    OBJECT_INFO_COUNT = 180,
    WEAPON_TYPE_COUNT = 46,
    ARMOUR_TYPE_COUNT = 58,
    JUNK_TYPE_COUNT = 53,
    FOOD_TYPE_COUNT = 8,
    DOOR_TYPE_COUNT = 4,
    SPELL_COUNT = 25,
    SYMBOL_STEP_COUNT = 4,
    SYMBOL_COUNT = 6,
    ACTION_SET_COUNT = 44,
    ACTION_COUNT = 44,
    LIGHT_POWER_COUNT = 16,
    PALETTE_LIGHT_THRESHOLD_COUNT = 6,
    SLOT_MASK_COUNT = 38,
    WOUND_DEFENSE_FACTOR_COUNT = 6,
    FIXED_POSSESSION_COUNT = 40,
};

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
    uint8_t action_indices[3]; /* 0xff = none */
    uint8_t minimum_skill_levels[2]; /* actions 1 and 2; bit 7 = needs charges */
    uint8_t byte_6;
} ActionSet;

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
const CreatureInfo *tables_creature_info(int creature_type);
const CreatureSound *tables_creature_sound(int index);
const ObjectInfo *tables_object_info(int index);
const WeaponInfo *tables_weapon_info(int weapon_type);
const ArmourInfo *tables_armour_info(int armour_type);
const uint8_t *tables_junk_weight(int junk_type);
const int16_t *tables_food_value(int index);
const DoorInfo *tables_door_info(int door_type);
const Spell *tables_spell(int index);
const uint8_t *tables_symbol_base_mana_cost(int step, int symbol);
const uint8_t *tables_symbol_mana_cost_multiplier(int power);
const ActionSet *tables_action_set(int index);
const ActionInfo *tables_action_info(int action);
const int16_t *tables_light_power_to_light_amount(int power);
const int16_t *tables_palette_light_threshold(int index);
const uint16_t *tables_slot_mask(int slot);
const uint8_t *tables_wound_defense_factor(int slot);
const uint16_t *tables_fixed_possession(int index);

#endif
