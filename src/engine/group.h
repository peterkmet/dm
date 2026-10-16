/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_GROUP_H
#define DM_ENGINE_GROUP_H
/* Prefixes: group_ creature_ */
#include "data/tables.h"
#include "engine/event.h"
#include "engine/map.h"
#include "engine/move.h"
#include "engine/record.h"
#include "engine/timeline.h"
#include <stdbool.h>
#include <stdint.h>

/* Creature groups (KAOS range 4, GROUP.C, and the group helpers of range 7): docs/rules/creatures.md. */

#define GROUP_MAX_CREATURES 4

/* Creature attributes (CreatureInfo.attributes). */
#define CREATURE_SIZE_MASK                  0x0003
#define CREATURE_SIDE_ATTACK                0x0004
#define CREATURE_PREFER_BACK_ROW            0x0008
#define CREATURE_ATTACK_ANY_CHAMPION        0x0010
#define CREATURE_LEVITATES                  0x0020
#define CREATURE_NON_MATERIAL               0x0040
#define CREATURE_DROPS_FIXED_POSSESSIONS    0x0200
#define CREATURE_KEEPS_THROWN_SHARP_WEAPONS 0x0400
#define CREATURE_SEES_INVISIBLE             0x0800
#define CREATURE_NIGHT_VISION               0x1000
#define CREATURE_ARCHENEMY                  0x2000
#define CREATURE_NOT_ON_MAP                 0x4000 /* KU does not show the creature */
#define CREATURE_SIZE_QUARTER               0
#define CREATURE_SIZE_HALF                  1
#define CREATURE_SIZE_FULL                  2
#define CREATURE_IMMOBILE                   255
#define CREATURE_NO_ATTACK                  255
#define CREATURE_INVULNERABLE               255
#define CREATURE_NEVER_HITS                 255
#define FEAR_RESISTANCE_NONE_FEARLESS       15
#define POISON_IMMUNE                       15
#define FIRE_IMMUNE                         15

/* Creature types with special rules. */
#define CREATURE_SWAMP_SLIME     1
#define CREATURE_GIGGLER         2
#define CREATURE_WIZARD_EYE      3
#define CREATURE_PAIN_RAT        4
#define CREATURE_SCREAMER        6
#define CREATURE_ROCK_PILE       7
#define CREATURE_STONE_GOLEM     9
#define CREATURE_BLACK_FLAME     11
#define CREATURE_SKELETON        12
#define CREATURE_COUATL          13
#define CREATURE_VEXIRK          14
#define CREATURE_MAGENTA_WORM    15
#define CREATURE_TROLIN          16
#define CREATURE_ANIMATED_ARMOUR 18
#define CREATURE_MATERIALIZER    19
#define CREATURE_DEMON           22
#define CREATURE_LORD_CHAOS      23
#define CREATURE_RED_DRAGON      24
#define CREATURE_LORD_ORDER      25
#define CREATURE_GREY_LORD       26

/* Group behavior (group record word 14 bits 0-3). */
#define BEHAVIOR_WANDER    0
#define BEHAVIOR_USELESS_2 2
#define BEHAVIOR_USELESS_3 3
#define BEHAVIOR_FLEE      5
#define BEHAVIOR_ATTACK    6
#define BEHAVIOR_APPROACH  7

/* A behavior event is an aspect event + 5. */
#define EVENT_BEHAVIOR_TO_ASPECT 5

/* Reactions that other rules send to group_process_reaction_event. Reaction r becomes event 32 + r. */
#define REACTION_DANGER_ON_SQUARE  (-3)
#define REACTION_HIT_BY_PROJECTILE (-2)
#define REACTION_PARTY_IS_ADJACENT (-1)

/* Results of the damage functions (g_creature_damage_outcome). */
#define OUTCOME_NO_CREATURE_KILLED    0
#define OUTCOME_SOME_CREATURES_KILLED 1
#define OUTCOME_ALL_CREATURES_KILLED  2

/* Cells byte of one creature that stands in the center of the square; the cell of a centered explosion. */
#define CELL_CENTERED 0xff

/* Active group aspect byte: bit 7 attacking, bit 6 flipped, bits 3-5 vertical offset, bits 0-2 horizontal offset. */
#define ASPECT_ATTACKING             0x80
#define ASPECT_FLIPPED               0x40
#define ASPECT_OFFSET_MASK           0x7
#define ASPECT_VERTICAL_OFFSET_SHIFT 3

/* Hit probability argument of the melee damage: bit 15 hits non-material creatures too. */
#define HIT_PROBABILITY_ALWAYS_HITS 0x8000
#define HIT_PROBABILITY_MASK        0x7fff

typedef struct {
    int16_t group_index; /* -1: free entry */
    uint8_t directions, cells;
    uint8_t last_move_time, delay_fleeing;
    uint8_t target_x, target_y, prior_x, prior_y, home_x, home_y;
    uint8_t aspect[4];
} ActiveGroup;

#define ACTIVE_GROUP_MAX_COUNT     110 /* creature groups that can act at the same time */
#define CSB_ACTIVE_GROUP_MAX_COUNT 60  /* active group slots of a new CSB game */

extern ActiveGroup *g_active_groups;
extern uint16_t g_active_group_count, g_max_active_group_count;
extern int32_t g_last_creature_attack_time;

/* New game only: all active group entries are free. */
void group_init_active_groups(void);
/* Allocates count active group entries, all free. A failed allocation is a fatal error. */
void group_allocate_active(uint16_t count);

/* On the party map the cells byte of the record holds the index of the active group. */
static inline ActiveGroup *group_active(uint16_t group)
{
    return &g_active_groups[rec_group_cells(group)];
}
static inline const CreatureInfo *group_info(uint16_t group)
{
    return tables_get_creature_info(rec_group_creature_type(group));
}

/* Creature info fields that pack several values, 4 bits each. */
#define CREATURE_INFO_NIBBLE            15
#define CREATURE_INFO_HIGH_NIBBLE_SHIFT 12
/* Height of the creature, attribute bits 7-8. */
static inline int16_t creature_height(uint16_t attributes)
{
    return attributes >> 7 & 3;
}
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

uint8_t group_get_cells(uint16_t group, int16_t map);
void group_set_cells(uint16_t group, uint8_t cells, int16_t map);
/* Off the party map all creatures of a group share the direction of the record. */
uint8_t group_get_directions(uint16_t group, int16_t map);
void group_set_directions(uint16_t group, uint8_t directions, int16_t map);
/* The creature type list of a map follows its last square. */
bool group_is_creature_allowed_on_map(uint16_t group, int16_t map);

/* Ordinal (index + 1) of the creature in a cell, or 0. Half-square creatures fill two cells side by side. */
int16_t group_get_creature_ordinal_in_cell(uint16_t group, uint16_t cell);
/* The creature that a champion in a cell hits first. */
int16_t group_get_melee_target_creature_ordinal(int16_t group_x, int16_t group_y, int16_t party_x, int16_t party_y,
                                                int16_t champion_cell);
/* The cells and directions of a group: two bits for each creature. */
#define GROUP_CELL_BITS 2
#define GROUP_CELL_MASK 3
static inline uint16_t group_cell(uint16_t packed, int16_t creature)
{
    return packed >> (GROUP_CELL_BITS * creature) & GROUP_CELL_MASK;
}
uint16_t group_set_cell_value(uint16_t packed, uint16_t creature_index, uint16_t value);
/* New graphic offsets and flip state of one creature (or all with creature_index -1). Returns the next aspect time. */
uint32_t group_get_creature_aspect_update_time(ActiveGroup *active, int16_t creature_index, bool attacking);
/* A new group behavior event (37) after one tick. Faster creatures get a higher event priority. */
void group_start_wandering(int16_t x, int16_t y);
void group_delete_events(int16_t x, int16_t y);
void group_stop_attacking(ActiveGroup *active, int16_t x, int16_t y);
/* A group on the party map gets an active group entry. The record keeps the entry index in its cells byte. */
void group_add_active_group(uint16_t group, int16_t x, int16_t y);
/* The record gets back its cells and one direction. A fleeing or attacking group wanders again. */
void group_remove_active_group(uint16_t active_index);
void group_remove_all_active_groups(void);
/* Each group of the new party map becomes active and wanders. */
void group_add_all_active_groups(void);
/* The fluxcage and direction memories of the last group step do not carry into a new game. */
void group_reset(void);
/* Ordinal of the latest party scent on a square of the current map, or 0. */
uint16_t group_get_party_scent_ordinal(int16_t x, int16_t y);

/* A new group of creatures on a square. Health is base health * multiplier plus a random part. */
uint16_t group_get_generated(uint16_t creature_type, int16_t health_multiplier, uint16_t last_creature_index,
                             int16_t direction, Location at);
/* The creature generator of a corridor event: count, health multiplier (0: map difficulty), random direction. */
void group_generate_from_sensor(uint16_t sensor, int16_t x, int16_t y);

/* A dead creature leaves the fixed possessions of its type. Possessions with bit 15 drop half of the time. */
void group_drop_creature_fixed_possessions(uint16_t creature_type, int16_t x, int16_t y, uint16_t cell,
                                           int16_t sound_mode);
/* Creatures that died during a move drop their fixed possessions on the destination square. */
void group_drop_moving_creature_fixed_possessions(uint16_t group, int16_t x, int16_t y);
/* All creatures drop their fixed possessions (sound_mode >= 0), then the carried objects fall on random cells. */
void group_drop_group_possessions(int16_t x, int16_t y, uint16_t group, int16_t sound_mode);
/* The group record becomes free. Health is cleared first, so that no projectile impact runs on the removal. */
void group_delete(int16_t x, int16_t y);
/* Damage to one creature. A dead creature leaves smoke; the last one also deletes the group (with notify).
   Returns OUTCOME_NO_CREATURE_KILLED, OUTCOME_SOME_CREATURES_KILLED, or OUTCOME_ALL_CREATURES_KILLED. */
int16_t group_get_damage_creature_outcome(uint16_t group, uint16_t creature_index, Location at, int16_t damage,
                                          bool notify);
/* The same attack, with a random spread of 1/8, on every creature of the group. */
int16_t group_get_damage_all_creatures_outcome(uint16_t group, int16_t x, int16_t y, int16_t attack, bool notify);
uint16_t group_get_resistance_adjusted_poison_attack(uint16_t creature_type, int16_t poison_attack);
/* A closing door hits the creatures on its square; the survivors react to the danger. */
void group_door_damages_creatures(int16_t x, int16_t y);

int16_t group_get_square_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* True when the destination is in the 90 degree cone in front of the source. */
bool group_is_destination_visible_from_source(int16_t direction, int16_t src_x, int16_t src_y, int16_t dst_x,
                                              int16_t dst_y);
/* The two directions from a square toward a destination: the main one and the second best. */
typedef struct {
    int16_t primary, secondary;
} DirectionPair;
/* The directions from a source square to a destination. */
DirectionPair group_get_directions_to_destination(Location from, Location to);
/* The 4 cells of the target square in the order the attacker reaches them. */
void group_set_ordered_cells_to_attack(uint8_t *ordered_cells, Location target, Location group_at, int16_t group_cell);
/* The champion that a creature on an adjacent square attacks, or -1. */
int16_t group_get_target_champion_index(int16_t x, int16_t y, int16_t cell);

/* A creature hits a champion: dodge by dexterity and luck, damage from attack, map difficulty, and parry skill,
   then poison. Returns the damage. */
int16_t group_get_champion_damage(uint16_t group, uint16_t champion_index);
/* A melee attack of a champion on a creature. */
typedef struct {
    int16_t champion_index;
    uint16_t group;
    int16_t creature_index;
    Location at;
    uint16_t hit_probability;
    uint16_t damage_factor;
    int16_t skill_index;
} MeleeAttack;
/* A champion hits a creature with a melee action: hit chance by dexterity, damage by strength, the action damage
   factor, and the creature defense. Returns the damage (0 for a miss). */
int16_t group_get_melee_action_damage(const MeleeAttack *attack);

/* A rule tells the group on a square about a danger, a projectile hit or the adjacent party (REACTION_*). */
void group_process_reaction_event(Location at, int16_t reaction);
/* The group event of the timeline (EVENT_GROUP_REACTION_* to EVENT_UPDATE_BEHAVIOR_CREATURE_3). Off the party map a
   group only wanders. Lord Chaos ignores Freeze Life. */
void group_process_event(const TimelineEvent *event);

/* move_get_move_result for a group. Teleporters, pits (20 damage), and stairs move it on. A group that dies or
   reaches a map that does not allow its type is deleted (stopped). An occupied destination makes the group wait
   for event 60/61 (stopped). Returns done, stopped, or turned (see the teleporter rotation). */
MoveResult group_get_move_result(const Move *move, MoveDestination *destination);
/* The group waits off the map and tries to enter the square 5 ticks later (event 61 plays the teleporter sound). */
void group_create_event_move_group(uint16_t group, int16_t x, int16_t y, int16_t map, bool audible);
/* A waiting group enters its square when the square is free. Lord Chaos may take an adjacent open square. */
void group_process_move_event(TimelineEvent *event);

#endif
