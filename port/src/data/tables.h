#ifndef TABLES_H
#define TABLES_H
/* Prefixes: tables_ */
#include <stdint.h>

/* Static rule tables of KAOS.FTL, decoded once from the loaded image. Field bits: docs/tables.md. */

/* KAOS data addresses. */
#define DOOR_INFO 0x1016e2 /* g_door_info[], high byte attributes */

enum {
    CREATURE_TYPE_COUNT = 27,
    CREATURE_SOUND_COUNT = 18,
    OBJECT_INFO_COUNT = 180,
    WEAPON_TYPE_COUNT = 46,
    ARMOUR_TYPE_COUNT = 58,
    JUNK_TYPE_COUNT = 53,
    FOOD_TYPE_COUNT = 8,
    DOOR_TYPE_COUNT = 4,
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

/* Skill indices of the champion records: the 4 base skills, then the hidden skills. */
enum {
    SKILL_FIGHTER,
    SKILL_NINJA,
    SKILL_PRIEST,
    SKILL_WIZARD,
    SKILL_BASE_COUNT,
    SKILL_SWING = 4,
    SKILL_PARRY = 7,
    SKILL_THROW = 10,
    SKILL_SHOOT,
    SKILL_HEAL = 13,
    SKILL_INFLUENCE,
    SKILL_DEFEND,
    SKILL_COUNT = 20
};
#define HIDDEN_SKILLS_PER_BASE 4
#define SKILL_LEVEL_MAXIMUM    16 /* the level names stop at this level */

/* Object info index of the first record of each object type. */
enum {
    OBJECT_INFO_FIRST_SCROLL = 0,
    OBJECT_INFO_FIRST_CONTAINER = 1,
    OBJECT_INFO_FIRST_POTION = 2,
    OBJECT_INFO_FIRST_WEAPON = 23,
    OBJECT_INFO_FIRST_ARMOUR = 69,
    OBJECT_INFO_FIRST_JUNK = 127
};
/* Object info allowed slots bit: the object passes through a door grate. */
enum { OBJECT_PASSES_DOOR_GRATE = 0x0100 };

/* Creature graphic info bits (CreatureInfo.graphic_info). */
enum {
    GRAPHIC_FLIP_NON_ATTACK = 0x0004,
    GRAPHIC_HAS_SIDE = 0x0008,
    GRAPHIC_HAS_BACK = 0x0010,
    GRAPHIC_HAS_ATTACK = 0x0020,
    GRAPHIC_SPECIAL_D2_FRONT = 0x0080,
    GRAPHIC_D2_FRONT_FLIPPED = 0x0100,
    GRAPHIC_FLIP_ATTACK = 0x0200,
    GRAPHIC_FLIP_DURING_ATTACK = 0x0400
};
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
    uint8_t byte_6;
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

/* Loads the engine module at path (kaos_load), then decodes the tables. Returns 0, or -1 as kaos_load. */
int tables_load(const char *path);

/* Squares ahead and to each side that a sound reaches; the volume table covers -range..range. */
#define SOUND_HEARING_RANGE 12

/* Each accessor below reads the loaded image at call time; it needs kaos_load, not tables_init. */
int16_t tables_step_east(int16_t direction);
int16_t tables_step_north(int16_t direction);
int16_t tables_arrow_steps_forward(int16_t arrow);
int16_t tables_arrow_steps_right(int16_t arrow);
uint8_t tables_ordered_cell_to_attack(int16_t row, int16_t i);
uint8_t tables_wound_probability_mask(int16_t index);
uint8_t tables_slot_to_steal_from(int16_t index);
uint8_t tables_torch_icon_offset(int16_t charges);
uint16_t tables_slot_drop_order(int16_t i);
uint8_t tables_champion_color(int16_t champion_index);
uint8_t tables_square_type_event(int16_t square_type);
int16_t tables_sound_graphic(int16_t sound);
uint8_t tables_sound_rate(int16_t sound);
uint8_t tables_sound_priority(int16_t sound);
uint8_t tables_sound_volume(int16_t ahead, int16_t side);
uint8_t tables_new_game_extra_things(int16_t thing_type);
uint16_t tables_door_attributes(int16_t door_set);
const char *tables_message_escape(int16_t code);
const char *tables_inscription_escape(int16_t code);
const char *tables_escape_replacement(int16_t code);
int16_t tables_portrait_width(void);
int16_t tables_portrait_height(void);

#endif
