/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_GROUP_H
#define DM_ENGINE_GROUP_H
/* Prefixes: group_ creature_ */
#include "data/tables.h"
#include "engine/game.h"
#include "engine/map.h"
#include "engine/timeline_process.h"
#include <stdbool.h>
#include <stdint.h>

/* Creature groups (KAOS range 4, GROUP.C, and the group helpers of range 7): docs/rules/creatures.md. */

#define GROUP_MAX_CREATURES 4

/* Creature attributes (CreatureInfo.attributes). CREATURE_LEVITATES and CREATURE_NON_MATERIAL are in map.h. */
enum {
    CREATURE_SIZE_MASK = 0x0003,
    CREATURE_SIDE_ATTACK = 0x0004,
    CREATURE_PREFER_BACK_ROW = 0x0008,
    CREATURE_ATTACK_ANY_CHAMPION = 0x0010,
    CREATURE_DROPS_FIXED_POSSESSIONS = 0x0200,
    CREATURE_KEEPS_THROWN_SHARP_WEAPONS = 0x0400,
    CREATURE_SEES_INVISIBLE = 0x0800,
    CREATURE_NIGHT_VISION = 0x1000,
    CREATURE_ARCHENEMY = 0x2000,
    CREATURE_NOT_ON_MAP = 0x4000 /* KU does not show the creature */
};
enum { CREATURE_SIZE_QUARTER, CREATURE_SIZE_HALF, CREATURE_SIZE_FULL };
enum { CREATURE_IMMOBILE = 255, CREATURE_NO_ATTACK = 255, CREATURE_INVULNERABLE = 255, CREATURE_NEVER_HITS = 255 };
enum { FEAR_RESISTANCE_NONE_FEARLESS = 15, POISON_IMMUNE = 15, FIRE_IMMUNE = 15 };

/* Creature types with special rules. */
enum {
    CREATURE_SWAMP_SLIME = 1,
    CREATURE_GIGGLER = 2,
    CREATURE_WIZARD_EYE = 3,
    CREATURE_PAIN_RAT = 4,
    CREATURE_SCREAMER = 6,
    CREATURE_ROCK_PILE = 7,
    CREATURE_STONE_GOLEM = 9,
    CREATURE_BLACK_FLAME = 11,
    CREATURE_SKELETON = 12,
    CREATURE_COUATL = 13,
    CREATURE_VEXIRK = 14,
    CREATURE_MAGENTA_WORM = 15,
    CREATURE_TROLIN = 16,
    CREATURE_ANIMATED_ARMOUR = 18,
    CREATURE_MATERIALIZER = 19,
    CREATURE_DEMON = 22,
    CREATURE_LORD_CHAOS = 23,
    CREATURE_RED_DRAGON = 24,
    CREATURE_LORD_ORDER = 25,
    CREATURE_GREY_LORD = 26
};

/* Group behavior (group record word 14 bits 0-3). */
enum {
    BEHAVIOR_WANDER = 0,
    BEHAVIOR_USELESS_2 = 2,
    BEHAVIOR_USELESS_3 = 3,
    BEHAVIOR_FLEE = 5,
    BEHAVIOR_ATTACK = 6,
    BEHAVIOR_APPROACH = 7
};

/* A behavior event is an aspect event + 5. */
enum { EVENT_BEHAVIOR_TO_ASPECT = 5 };

/* Reactions that other rules send to group_process_reaction_event. Reaction r becomes event 32 + r. */
enum { REACTION_DANGER_ON_SQUARE = -3, REACTION_HIT_BY_PROJECTILE = -2, REACTION_PARTY_IS_ADJACENT = -1 };

/* Results of the damage functions (g_creature_damage_outcome). */
enum { OUTCOME_NO_CREATURE_KILLED, OUTCOME_SOME_CREATURES_KILLED, OUTCOME_ALL_CREATURES_KILLED };

/* Cells byte of one creature that stands in the center of the square; the cell of a centered explosion. */
enum { CELL_CENTERED = 0xff };

/* Active group aspect byte: bit 7 attacking, bit 6 flipped, bits 3-5 vertical offset, bits 0-2 horizontal offset. */
enum { ASPECT_ATTACKING = 0x80, ASPECT_FLIPPED = 0x40, ASPECT_OFFSET_MASK = 0x7, ASPECT_VERTICAL_OFFSET_SHIFT = 3 };

/* Hit probability argument of the melee damage: bit 15 hits non-material creatures too. */
enum { HIT_PROBABILITY_ALWAYS_HITS = 0x8000, HIT_PROBABILITY_MASK = 0x7fff };

/* Group record, 16 bytes (thing type 4): next, possessions, type, cells, 4 health words, word 14 flags.
   Byte 14: direction in bits 0-1, do not discard in bit 2. Byte 15: behavior in bits 0-3, last creature index in bits 5-6. */
enum {
    GROUP_RECORD_DIRECTION_MASK = 3,
    GROUP_RECORD_DO_NOT_DISCARD = 0x04,
    GROUP_RECORD_BEHAVIOR_MASK = 0x0f,
    GROUP_RECORD_LAST_CREATURE_MASK = 0x60,
    GROUP_RECORD_LAST_CREATURE_SHIFT = 5
};
static inline uint16_t group_possessions(uint16_t group)
{
    return BE16(REC(group) + 2);
}
static inline void group_set_possessions(uint16_t group, uint16_t thing)
{
    WR16(REC(group) + 2, thing);
}
static inline void group_set_creature_type(uint16_t group, uint16_t type)
{
    REC(group)[4] = type;
}
static inline void group_set_cells_byte(uint16_t group, uint8_t cells)
{
    REC(group)[5] = cells;
}
static inline uint16_t group_health(uint16_t group, int16_t creature)
{
    return BE16(REC(group) + 6 + 2 * creature);
}
static inline void group_set_health(uint16_t group, int16_t creature, uint16_t health)
{
    WR16(REC(group) + 6 + 2 * creature, health);
}
static inline uint16_t group_behavior(uint16_t group)
{
    return REC(group)[GROUP_BEHAVIOR_BYTE] & GROUP_RECORD_BEHAVIOR_MASK;
}
static inline void group_set_behavior(uint16_t group, uint16_t behavior)
{
    REC(group)
    [GROUP_BEHAVIOR_BYTE] = (REC(group)[GROUP_BEHAVIOR_BYTE] & ~GROUP_RECORD_BEHAVIOR_MASK) | behavior;
}
static inline void group_set_last_creature_index(uint16_t group, uint16_t index)
{
    REC(group)
    [GROUP_BEHAVIOR_BYTE] = (REC(group)[GROUP_BEHAVIOR_BYTE] & ~GROUP_RECORD_LAST_CREATURE_MASK) |
                            (index & 3) << GROUP_RECORD_LAST_CREATURE_SHIFT;
}
static inline void group_set_direction_field(uint16_t group, uint16_t direction)
{
    REC(group)
    [GROUP_DIRECTION_BYTE] =
        (REC(group)[GROUP_DIRECTION_BYTE] & ~GROUP_RECORD_DIRECTION_MASK) | (direction & GROUP_RECORD_DIRECTION_MASK);
}
static inline void group_clear_do_not_discard(uint16_t group)
{
    REC(group)[GROUP_DIRECTION_BYTE] &= ~GROUP_RECORD_DO_NOT_DISCARD;
}

/* On the party map the cells byte of the record holds the index of the active group. */
static inline ActiveGroup *group_active(uint16_t group)
{
    return &g_active_groups[rec_group_cells_byte(group)];
}
static inline const CreatureInfo *group_info(uint16_t group)
{
    return tables_get_creature_info(rec_group_creature_type(group));
}

/* Creature info fields that pack several values, 4 bits each. */
enum { CREATURE_INFO_NIBBLE = 15, CREATURE_INFO_HIGH_NIBBLE_SHIFT = 12 };
static inline uint16_t creature_sight_range(const CreatureInfo *info)
{
    return info->ranges & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_hearing_range(const CreatureInfo *info)
{
    return info->ranges >> 4 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_smell_range(const CreatureInfo *info)
{
    return info->ranges >> 8 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_attack_range(const CreatureInfo *info)
{
    return info->ranges >> CREATURE_INFO_HIGH_NIBBLE_SHIFT;
}
static inline uint16_t creature_fear_resistance(const CreatureInfo *info)
{
    return info->properties >> 4 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_experience(const CreatureInfo *info)
{
    return info->properties >> 8 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_wariness(const CreatureInfo *info)
{
    return info->properties >> CREATURE_INFO_HIGH_NIBBLE_SHIFT;
}
static inline uint16_t creature_fire_resistance(const CreatureInfo *info)
{
    return info->resistances >> 4 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_poison_resistance(const CreatureInfo *info)
{
    return info->resistances >> 8 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_ticks_after_attack(const CreatureInfo *info)
{
    return info->animation_ticks & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_ticks_non_attack_aspect(const CreatureInfo *info)
{
    return info->animation_ticks >> 4 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_ticks_attack_aspect(const CreatureInfo *info)
{
    return info->animation_ticks >> 8 & CREATURE_INFO_NIBBLE;
}
static inline uint16_t creature_size(const CreatureInfo *info)
{
    return info->attributes & CREATURE_SIZE_MASK;
}

extern int16_t g_creature_damage_outcome;
extern int16_t g_group_secondary_direction;
extern int16_t g_current_group_map_x, g_current_group_map_y;
extern uint16_t g_current_group_thing, g_current_group_distance_to_party;
extern int16_t g_current_group_primary_direction_to_party, g_current_group_secondary_direction_to_party;
extern int16_t g_dungeon_view_palette_index; /* 0 (bright) to 5; the view step sets it */

uint8_t group_get_cells(uint16_t group, int16_t map);
void group_set_cells(uint16_t group, uint8_t cells, int16_t map);
uint8_t group_get_directions(uint16_t group, int16_t map);
void group_set_directions(uint16_t group, uint8_t directions, int16_t map);
bool group_is_creature_allowed_on_map(uint16_t group, int16_t map);

int16_t group_get_creature_ordinal_in_cell(uint16_t group, uint16_t cell);
int16_t group_get_melee_target_creature_ordinal(int16_t group_x, int16_t group_y, int16_t party_x, int16_t party_y,
                                                int16_t champion_cell);
/* The cells and directions of a group: two bits for each creature. */
enum { GROUP_CELL_BITS = 2, GROUP_CELL_MASK = 3 };
static inline uint16_t group_cell(uint16_t packed, int16_t creature)
{
    return packed >> (GROUP_CELL_BITS * creature) & GROUP_CELL_MASK;
}
uint16_t group_set_cell_value(uint16_t packed, uint16_t creature_index, uint16_t value);
uint32_t group_get_creature_aspect_update_time(ActiveGroup *active, int16_t creature_index, bool attacking);
void group_start_wandering(int16_t x, int16_t y);
void group_delete_events(int16_t x, int16_t y);
void group_stop_attacking(ActiveGroup *active, int16_t x, int16_t y);
void group_add_active_group(uint16_t group, int16_t x, int16_t y);
void group_remove_active_group(uint16_t active_index);
void group_remove_all_active_groups(void);
void group_add_all_active_groups(void);
void group_reset(void);
/* Ordinal of the latest party scent on a square of the current map, or 0. */
uint16_t group_get_party_scent_ordinal(int16_t x, int16_t y);

uint16_t group_get_generated(uint16_t creature_type, int16_t health_multiplier, uint16_t last_creature_index,
                             int16_t direction, int16_t x, int16_t y);
void group_generate_from_sensor(uint16_t sensor, int16_t x, int16_t y);

void group_drop_creature_fixed_possessions(uint16_t creature_type, int16_t x, int16_t y, uint16_t cell,
                                           int16_t sound_mode);
void group_drop_moving_creature_fixed_possessions(uint16_t group, int16_t x, int16_t y);
void group_drop_group_possessions(int16_t x, int16_t y, uint16_t group, int16_t sound_mode);
void group_delete(int16_t x, int16_t y);
int16_t group_get_damage_creature_outcome(uint16_t group, uint16_t creature_index, int16_t x, int16_t y, int16_t damage,
                                          bool notify);
int16_t group_get_damage_all_creatures_outcome(uint16_t group, int16_t x, int16_t y, int16_t attack, bool notify);
uint16_t group_get_resistance_adjusted_poison_attack(uint16_t creature_type, int16_t poison_attack);
void group_door_damages_creatures(int16_t x, int16_t y);

int16_t group_get_square_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
bool group_is_destination_visible_from_source(int16_t direction, int16_t src_x, int16_t src_y, int16_t dst_x,
                                              int16_t dst_y);
int16_t group_get_directions_to_destination(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y);
void group_set_ordered_cells_to_attack(uint8_t *ordered_cells, int16_t target_x, int16_t target_y, int16_t group_x,
                                       int16_t group_y, int16_t group_cell);
int16_t group_get_target_champion_index(int16_t x, int16_t y, int16_t cell);

int16_t group_get_champion_damage(uint16_t group, uint16_t champion_index);
int16_t group_get_melee_action_damage(int16_t champion_index, uint16_t group, int16_t creature_index, int16_t x,
                                      int16_t y, uint16_t hit_probability, uint16_t damage_factor, int16_t skill_index);

void group_process_reaction_event(int16_t x, int16_t y, int16_t event_type, uint16_t ticks);

int16_t group_get_move_result(uint16_t group, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y);
void group_create_event_move_group(uint16_t group, int16_t x, int16_t y, int16_t map, bool audible);
void group_process_move_event(TimelineEvent *event);

#endif
