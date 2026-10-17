/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/group.h"
#include "data/adventure.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/move.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/random.h"
#include "engine/record.h"
#include "engine/sensor.h"
#include "engine/sound.h"
#include "engine/timeline.h"
#include <stdlib.h>

/* Aspect flags that a group keeps when its aspect changes. */
#define ASPECT_KEEP_FLAGS (ASPECT_ATTACKING | ASPECT_FLIPPED)

/* Flag and mask of a fixed possession entry. */
#define FIXED_POSSESSION_RANDOM_DROP 0x8000
#define FIXED_POSSESSION_INDEX_MASK  0x7fff

/* System error, tick counts, chances and damages of the group rules. */
#define SYSTEM_ERROR_NO_FREE_ACTIVE_GROUP    71
#define ACTIVE_GROUP_RESERVE                 5
#define MOVEMENT_TICKS_OF_IMMOBILE_CREATURE  100
#define FREEZE_LIFE_RETRY_TICKS              4
#define MOVE_GROUP_RETRY_TICKS               5
#define WARINESS_AVOIDS_FORBIDDEN_TELEPORTER 10
#define FALL_DAMAGE                          20
#define DOOR_DAMAGE                          5
#define INVISIBLE_PARTY_DODGE_BONUS          16
#define LUCK_PERCENT_AGAINST_CREATURE        60
#define LUCK_PERCENT_AGAINST_MISS            75
#define STEAL_CHANCE_STEP                    20
#define SMOKE_ATTACK_QUARTER                 110
#define SMOKE_ATTACK_HALF                    190
#define SMOKE_ATTACK_FULL                    255

/* Bit mask of the cells of three creatures. */
#define CELLS_OF_THREE_CREATURES    0x3f
#define DIRECTION_ALL_CELLS_FACTOR  0x55 /* repeats a 2-bit direction in the four cell fields */
/* Faster creatures get a higher event priority: this minus the movement ticks. */
#define GROUP_EVENT_PRIORITY_MAX    255
/* A new active group can move at once: its last move is this many ticks ago. Move times are kept in one byte. */
#define NEW_GROUP_LAST_MOVE_AGE     127
#define MOVE_TIME_WRAP              256
/* First word of the fixed possession list of each creature type in the possession table. */
#define POSSESSIONS_SKELETON        0
#define POSSESSIONS_STONE_GOLEM     3
#define POSSESSIONS_TROLIN          5
#define POSSESSIONS_ANIMATED_ARMOUR 7
#define POSSESSIONS_ROCK_PILE       14
#define POSSESSIONS_PAIN_RAT        19
#define POSSESSIONS_SCREAMER        22
#define POSSESSIONS_MAGENTA_WORM    25
#define POSSESSIONS_RED_DRAGON      29
/* A frightened group flees for random(100 - 4 * fear) + 20 events; a thief for random(64) + 20. */
#define FEAR_RANDOM_MASK            0x0f
#define THEFT_FLEE_RANDOM_MASK      0x3f

/* Flee delay and steal chance values. */
#define FLEE_DELAY_RANGE                  100
#define FLEE_DELAY_FEAR_STEP              4
#define FLEE_DELAY_MINIMUM                20
#define STEAL_CHANCE_START                100
/* Line slopes in fixed point: 64 is one square across per square along. */
#define LINE_SLOPE_ONE                    64
#define LINE_SLOPE_STRAIGHT               128
/* Sight: an invisible party gives the range -10 (compared unsigned); next to the party a roll adds hearing and smell. */
#define INVISIBLE_PARTY_SIGHT_RANGE       (-10)
#define ADJACENT_SIGHT_BONUS              5
#define SIGHT_ROLL_CENTER                 3
/* A scent trail is followed when 30 - 2 * smell is below the scent strength plus a random part. */
#define SCENT_TRAIL_THRESHOLD             30
/* The spell projectile of a creature. */
#define CREATURE_SPELL_MIN_KINETIC_ENERGY 20
#define CREATURE_SPELL_STEP_ENERGY        8
#define POISON_ATTACK_FACTOR              8
/* Wound probabilities: four 4-bit thresholds, compared with a random nibble. */
#define WOUND_PROBABILITY_MASK            0xf

/* Bits of one wound probability in the creature wound word. */
#define WOUND_PROBABILITY_SHIFT     4
/* Hit rolls: random(32) - 16 around the aim of the attacker. */
#define AIM_RANDOM_MASK             0x1f
#define CREATURE_DAMAGE_RANDOM_MASK 0x0f
#define WEAK_HIT_RANDOM_MASK        0x0f
#define ARMOUR_RANDOM_MASK          0x1f
#define STRENGTH_DAMAGE_RANDOM_MASK 0x1f

/* Center of the random aim value. */
#define AIM_RANDOM_CENTER         16
/* A skill level above a random(64) doubles the melee damage and adds 10. */
#define DOUBLE_DAMAGE_RANDOM_MASK 0x3f

/* Damage and experience bonuses and the stamina cost of a melee hit. */
#define DOUBLE_DAMAGE_BONUS    10
#define MELEE_EXPERIENCE_BONUS 3
#define MELEE_HIT_STAMINA      4
/* Creatures farther than this on an axis update their aspect at a slow fixed pace. */
#define ASPECT_NEAR_DISTANCE   3
/* A creature with more attack ticks waits a random -2 to 5 ticks more. */
#define SLOW_ATTACK_TICKS      15

/* Bit mask of the random term of the attack range. */
#define ATTACK_RANGE_RANDOM_MASK 0xf
/* Random bits that must all be 0: a hit without weighted wounds (1 in 8), a single small creature that
   shifts (1 in 4), and its move to a new cell (1 in 8). */
#define PLAIN_HIT_RANDOM_BITS    0x70
#define MOVE_CELL_RANDOM_BITS    0xc0
#define NEW_CELL_RANDOM_BITS     0x38

ActiveGroup *g_active_groups;
uint16_t g_active_group_count, g_max_active_group_count;
int32_t g_last_creature_attack_time;

/* The last pair turn of a half-size pair: two creatures turn together once per game tick. */
static struct {
    uint32_t time;
    const ActiveGroup *group;
} set_direction;
/* The group of the last group turn. Kept quirk: a group that becomes active plays its flip sound at this
   square, and a group off the party map tests teleporters with this thing, as in the original. */
static struct {
    uint16_t thing;
    Location at;
} last_turn;
/* Kept quirk: the party-blocked result of the last step test. wander_step reads it also when it skips the test, as
   in the original; it survives between turns and group_reset does not clear it. */
static bool last_blocked_by_party;
/* The entry for a cells byte beyond the table (a corrupt save): reads give a free entry, writes are lost. */
static ActiveGroup corrupt_entry;

/* The result of one step test of a group. */
typedef struct {
    bool possible;
    bool blocked_by_party;
} MovementTest;

/* State of one group turn (a reaction or a group event). */
typedef struct {
    int16_t x, y;
    int16_t event_type; /* reactions are REACTION_* here */
    uint16_t ticks;
    uint16_t group;
    const CreatureInfo *info;
    ActiveGroup *active;
    TimelineEvent next;             /* the next event of the group */
    uint32_t aspect_update_time;    /* 0: the aspect of the whole group is updated */
    int16_t ticks_delay_next_event; /* 0: ticks delay the aspect update instead (behavior events) */
    int16_t ticks_since_last_move;  /* -1: the group waits to move */
    int16_t movement_ticks;
    int16_t archenemy;
    uint16_t last_creature_index, size;
    int16_t distance_x, distance_y;
    int16_t moved;
    int16_t was_attacking;
    uint16_t visible_distance;
    int16_t direction;           /* the direction the group wants to move in */
    int16_t secondary_direction; /* the second best direction toward the same square */
    uint16_t distance_to_party;
    DirectionPair directions_to_party;
    uint8_t tested_directions[DIRECTION_COUNT];
    uint8_t flux_cages[DIRECTION_COUNT];
    int16_t flux_cage_count;
} GroupTurn;

static uint16_t get_active_group_thing(const ActiveGroup *active)
{
    return THING_MAKE(THING_GROUP, active->group_index);
}
static void set_target_to_party(ActiveGroup *active)
{
    active->target_x = g_party_map_x;
    active->target_y = g_party_map_y;
}

/* The creature sound index of a creature type, or -1 for none. */
static int16_t get_sound_index(const CreatureInfo *info)
{
    return info->attack_sound_byte - 1;
}

/* The movement sound of a creature type, or -1 while the party sleeps (move_get_creature_sound). */
static int16_t get_creature_movement_sound(uint16_t creature_type)
{
    int16_t sound_index = get_sound_index(tables_get_creature_info(creature_type));
    if (g_session.party_sleeping || sound_index < 0)
        return -1;
    return tables_get_creature_sound(sound_index)->movement;
}

/* The active group entry of a group on the party map. Deviation: the original reads beyond the table when the
   cells byte of the record is too large. */
static ActiveGroup *active_of(uint16_t group)
{
    uint16_t index = rec_group_cells(group);
    if (index >= g_max_active_group_count) {
        corrupt_entry = (ActiveGroup){.group_index = -1};
        return &corrupt_entry;
    }
    return &g_active_groups[index];
}

const ActiveGroup *group_get_active(uint16_t group)
{
    if (!g_active_groups)
        return NULL;
    return active_of(group);
}

void group_allocate_active(uint16_t count)
{
    free(g_active_groups);
    g_active_groups = calloc(count, sizeof *g_active_groups);
    if (!g_active_groups)
        game_fatal(SYSTEM_ERROR_MEMORY);
    for (uint16_t i = 0; i < count; i++)
        g_active_groups[i].group_index = -1;
}

void group_init_active_groups(void)
{
    g_max_active_group_count = g_adventure == ADVENTURE_CSB ? ACTIVE_GROUP_MAX_COUNT_CSB : ACTIVE_GROUP_MAX_COUNT;
    group_allocate_active(g_max_active_group_count);
}

void group_set_cells(uint16_t group, uint8_t cells, int16_t map)
{
    if (map == g_party_map_index)
        active_of(group)->cells = cells;
    else
        rec_group_set_cells(group, cells);
}

void group_set_directions(uint16_t group, uint8_t directions, int16_t map)
{
    if (map == g_party_map_index)
        active_of(group)->directions = directions;
    else
        rec_group_set_direction(group, directions);
}

bool group_is_allowed_on_map(uint16_t group, int16_t map)
{
    const uint8_t *types = rec_map_type_lists(map);
    for (int16_t n = rec_map_creature_types(map); n > 0; n--, types++)
        if (rec_group_creature_type(group) == *types)
            return true;
    return false;
}

static uint8_t get_cells(uint16_t group, int16_t map)
{
    return map == g_party_map_index ? active_of(group)->cells : rec_group_cells(group);
}

/* Off the party map all creatures of a group share the direction of the record. */
static uint8_t get_directions(uint16_t group, int16_t map)
{
    if (map == g_party_map_index)
        return active_of(group)->directions;
    uint16_t direction = rec_group_direction(group);
    return direction * DIRECTION_ALL_CELLS_FACTOR;
}

int16_t group_get_creature_in_cell(uint16_t group, int16_t cell)
{
    uint8_t cells = get_cells(group, g_current_map_index);
    if (cells == CELL_CENTERED)
        return 0;
    bool half_square = creature_size(group_info(group)) == CREATURE_SIZE_HALF;
    if (half_square && (cell & 1) == (get_directions(group, g_current_map_index) & 1))
        cell = direction_left(cell);
    for (int16_t i = rec_group_last_creature(group); i >= 0; i--) {
        uint16_t creature_cell = group_cell(cells, i);
        if (cell == creature_cell || (half_square && direction_right(cell) == creature_cell))
            return i;
    }
    return -1;
}

/* The 4 cells of the target square in the order the attacker reaches them. */
static void order_attack_cells(uint8_t *ordered_cells, Location target, Location group_at, int16_t group_cell)
{
    int16_t direction = group_get_directions_to(target, group_at).primary;
    if (!(direction & 1))
        group_cell++;
    int16_t row = ((group_cell >> 1) & 1) + direction * 2;
    for (int16_t i = 0; i < CELL_COUNT; i++)
        ordered_cells[i] = tables_get_attack_cell_order(row, i);
}

int16_t group_get_melee_target(int16_t group_x, int16_t group_y, int16_t party_x, int16_t party_y,
                               int16_t champion_cell)
{
    uint16_t group = map_find_thing((Location){group_x, group_y}, THING_GROUP);
    if (group == THING_ENDOFLIST)
        return -1;
    uint8_t cells[CELL_COUNT];
    order_attack_cells(cells, (Location){group_x, group_y}, (Location){party_x, party_y}, champion_cell);
    for (int16_t i = 0; i < CELL_COUNT; i++) {
        int16_t creature_index = group_get_creature_in_cell(group, cells[i]);
        if (creature_index >= 0)
            return creature_index;
    }
    return -1;
}

/* A random offset of the creature graphic in a range, mirrored half of the time. */
static uint8_t aspect_offset(uint16_t range)
{
    uint8_t offset = random16() % range;
    if (random_bit())
        offset = -offset & ASPECT_OFFSET_MASK;
    return offset;
}

void group_start_wandering(int16_t x, int16_t y)
{
    uint16_t group = map_find_thing((Location){x, y}, THING_GROUP);
    if (rec_group_behavior(group) > BEHAVIOR_WANDER_NO_SMELL)
        rec_group_set_behavior(group, BEHAVIOR_WANDER);
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + 1),
        .type = EVENT_UPDATE_BEHAVIOR_GROUP,
        .priority = GROUP_EVENT_PRIORITY_MAX - group_info(group)->movement_ticks,
        .b = event_square(x, y),
    };
    timeline_add_event(&event);
}

/* Deletes the group events of the square. */
static void delete_events(int16_t x, int16_t y)
{
    for (uint16_t i = 0; i < g_timeline.high_water; i++) {
        TimelineEvent *event = &g_timeline.events[i];
        if (EVENT_MAP(event) == g_current_map_index && event->type >= EVENT_GROUP_REACTION_DANGER_ON_SQUARE &&
            event->type <= EVENT_UPDATE_BEHAVIOR_CREATURE_3 && event_square_x(event) == x && event_square_y(event) == y)
            timeline_delete_event(i);
    }
}

/* The creatures of the group stop their attack, and its pending group events go. */
static void stop_attacking(ActiveGroup *active, int16_t x, int16_t y)
{
    for (int16_t i = 0; i < GROUP_MAX_CREATURES; i++)
        active->aspect[i] &= ~ASPECT_ATTACKING;
    delete_events(x, y);
}

void group_frighten(uint16_t group, int16_t x, int16_t y, uint8_t delay_fleeing)
{
    ActiveGroup *active = active_of(group);
    if (rec_group_behavior(group) == BEHAVIOR_ATTACK) {
        stop_attacking(active, x, y);
        group_start_wandering(x, y);
    }
    rec_group_set_behavior(group, BEHAVIOR_FLEE);
    active->delay_fleeing = delay_fleeing;
}

static uint16_t set_cell_value(uint16_t packed, int16_t creature_index, uint16_t value)
{
    uint16_t shift = GROUP_CELL_SHIFT * creature_index;
    return (packed & ~(GROUP_CELL_MASK << shift)) | (value & GROUP_CELL_MASK) << shift;
}

/* New graphic offsets and flip state of one creature (or all with creature_index -1). Returns the next aspect time. */
static uint32_t next_aspect_time(ActiveGroup *active, int16_t creature_index, bool attacking)
{
    uint16_t group = get_active_group_thing(active);
    uint16_t type = rec_group_creature_type(group);
    uint16_t graphic = tables_get_creature_info(type)->graphic_info;
    int16_t i = creature_index < 0 ? rec_group_last_creature(group) : creature_index;
    for (;;) {
        uint8_t aspect = active->aspect[i] & ASPECT_KEEP_FLAGS;
        if (tables_get_horizontal_range(graphic))
            aspect |= aspect_offset(tables_get_horizontal_range(graphic));
        if (tables_get_vertical_range(graphic))
            aspect |= aspect_offset(tables_get_vertical_range(graphic)) << ASPECT_VERTICAL_OFFSET_SHIFT;
        if (!attacking) {
            if (!(graphic & GRAPHIC_FLIP_NON_ATTACK)) {
                aspect &= ~ASPECT_FLIPPED;
            } else if (type == CREATURE_COUATL) {
                if (random_bit()) {
                    aspect ^= ASPECT_FLIPPED;
                    sound_request_play(get_creature_movement_sound(CREATURE_COUATL), last_turn.at,
                                       SOUND_MODE_AFTER_TICK, 0);
                }
            } else if (random_bit()) {
                aspect |= ASPECT_FLIPPED;
            } else {
                aspect &= ~ASPECT_FLIPPED;
            }
            aspect &= ~ASPECT_ATTACKING;
        } else {
            if (!(graphic & GRAPHIC_FLIP_ATTACK)) {
                aspect &= ~ASPECT_FLIPPED;
            } else if ((aspect & ASPECT_ATTACKING) && type == CREATURE_ANIMATED_ARMOUR) {
                if (random_bit()) {
                    aspect ^= ASPECT_FLIPPED;
                    sound_request_play(SOUND_SWING, last_turn.at, SOUND_MODE_AFTER_TICK, 0);
                }
            } else if (!(aspect & ASPECT_ATTACKING) || !(graphic & GRAPHIC_FLIP_DURING_ATTACK)) {
                if (random_bit())
                    aspect |= ASPECT_FLIPPED;
                else
                    aspect &= ~ASPECT_FLIPPED;
            }
            aspect |= ASPECT_ATTACKING;
        }
        active->aspect[i] = aspect;
        if (creature_index >= 0 || i == 0)
            break;
        i--;
    }
    const CreatureInfo *info = tables_get_creature_info(type);
    uint16_t ticks = attacking ? creature_ticks_attack_aspect(info) : creature_ticks_non_attack_aspect(info);
    return g_game_time + random_bit() + ticks;
}

void group_add_active_group(uint16_t group, int16_t x, int16_t y)
{
    uint16_t index = 0;
    while (g_active_groups[index].group_index >= 0) {
        if (++index >= g_max_active_group_count)
            game_fatal(SYSTEM_ERROR_NO_FREE_ACTIVE_GROUP);
    }
    ActiveGroup *active = &g_active_groups[index];
    g_active_group_count++;
    active->group_index = THING_INDEX(group);
    active->cells = rec_group_cells(group);
    rec_group_set_cells(group, index);
    active->prior_x = x;
    active->home_x = active->prior_x;
    active->prior_y = y;
    active->home_y = active->prior_y;
    active->last_move_time = g_game_time - NEW_GROUP_LAST_MOVE_AGE;
    for (int16_t i = rec_group_last_creature(group); i >= 0; i--) {
        active->directions = set_cell_value(active->directions, i, rec_group_direction(group));
        active->aspect[i] = 0;
    }
    next_aspect_time(active, -1, false);
}

/* The record gets back its cells and one direction. A fleeing or attacking group wanders again. */
static void remove_active_group(uint16_t active_index)
{
    /* The original also accepts active_index == g_max_active_group_count (one past the table). */
    if (active_index >= g_max_active_group_count || g_active_groups[active_index].group_index < 0)
        return;
    ActiveGroup *active = &g_active_groups[active_index];
    uint16_t group = get_active_group_thing(active);
    g_active_group_count--;
    rec_group_set_cells(group, active->cells);
    rec_group_set_direction(group, active->directions);
    if (rec_group_behavior(group) > BEHAVIOR_WANDER_NO_SMELL)
        rec_group_set_behavior(group, BEHAVIOR_WANDER);
    active->group_index = -1;
}

void group_remove_all_active_groups(void)
{
    for (uint16_t i = 0; g_active_group_count; i++)
        if (g_active_groups[i].group_index >= 0)
            remove_active_group(i);
}

void group_add_all_active_groups(void)
{
    for (int16_t x = 0; x < g_current_map_width; x++) {
        for (int16_t y = 0; y < g_current_map_height; y++) {
            if (!(rec_square_byte(g_current_map_index, x, y) & SQUARE_HAS_THINGS))
                continue;
            uint16_t group = map_find_thing((Location){x, y}, THING_GROUP);
            if (group == THING_ENDOFLIST)
                continue;
            delete_events(x, y);
            group_add_active_group(group, x, y);
            group_start_wandering(x, y);
        }
    }
}

void group_reset(void)
{
    set_direction.time = 0;
    set_direction.group = NULL;
    last_turn.thing = 0;
    last_turn.at = (Location){0, 0};
}

uint16_t group_get_generated(uint16_t creature_type, int16_t health_multiplier, uint16_t last_creature_index,
                             int16_t direction, Location at)
{
    /* 16-bit wrap as in the original */
    if ((uint16_t)(g_max_active_group_count - ACTIVE_GROUP_RESERVE) <= g_active_group_count &&
        g_current_map_index == g_party_map_index)
        return THING_NONE;
    uint16_t group = rec_find_unused_thing(THING_GROUP);
    if (group == THING_NONE)
        return THING_NONE;
    rec_group_set_possessions(group, THING_ENDOFLIST);
    rec_group_set_no_discard(group, false);
    rec_group_set_direction(group, direction);
    rec_group_set_last_creature(group, last_creature_index);
    /* Bug fix: the original starts a group of several creatures from a stale register; the port starts from 0. */
    uint16_t cells = 0;
    uint16_t cell = 0;
    if (last_creature_index == 0)
        cells = CELL_CENTERED;
    else
        cell = random_2bits();
    rec_group_set_creature_type(group, creature_type);
    const CreatureInfo *info = tables_get_creature_info(creature_type);
    for (int16_t i = last_creature_index; i >= 0; i--) {
        uint16_t random = random16();
        rec_group_set_health(group, i, info->base_health * health_multiplier + random % ((info->base_health >> 2) + 1));
        if (last_creature_index) {
            cells = set_cell_value(cells, i, cell);
            cell = NORMALIZE(cell + (creature_size(info) == CREATURE_SIZE_HALF ? 2 : 1));
        }
    }
    rec_group_set_cells(group, cells);
    if (move_get_move_result(&(Move){.thing = group, .to = at, .enters = true}, NULL) != MOVE_RESULT_DONE)
        return THING_NONE;
    sound_request_play(SOUND_BUZZ, at, SOUND_MODE_AFTER_TICK, 0);
    return group;
}

/* Flag and masks of the creature count field of a group generator. */
#define GENERATOR_RANDOM_FLAG 0x8
#define GENERATOR_RANDOM_MASK 0x7
#define GENERATOR_FIELD_MASK  0xf

void group_generate_from_sensor(uint16_t sensor, int16_t x, int16_t y)
{
    uint16_t field = rec_sensor_generator_count(sensor), last_creature_index;
    if (field & GENERATOR_RANDOM_FLAG)
        last_creature_index = random16() % (field & GENERATOR_RANDOM_MASK);
    else
        last_creature_index = (field & GENERATOR_FIELD_MASK) - 1;
    int16_t multiplier = rec_sensor_health_multiplier(sensor);
    if (!multiplier)
        multiplier = rec_current_map_difficulty();
    int16_t direction = random_2bits();
    group_get_generated(rec_sensor_creature_type(sensor), multiplier, last_creature_index, direction, (Location){x, y});
}

/* First word of the fixed possession list of each creature type, or -1 (the switch of the original). */
static int16_t fixed_possession_list_start(uint16_t creature_type)
{
    switch (creature_type) {
    case CREATURE_SKELETON:
        return POSSESSIONS_SKELETON;
    case CREATURE_STONE_GOLEM:
        return POSSESSIONS_STONE_GOLEM;
    case CREATURE_TROLIN:
        return POSSESSIONS_TROLIN;
    case CREATURE_ANIMATED_ARMOUR:
        return POSSESSIONS_ANIMATED_ARMOUR;
    case CREATURE_ROCK_PILE:
        return POSSESSIONS_ROCK_PILE;
    case CREATURE_PAIN_RAT:
        return POSSESSIONS_PAIN_RAT;
    case CREATURE_SCREAMER:
        return POSSESSIONS_SCREAMER;
    case CREATURE_MAGENTA_WORM:
        return POSSESSIONS_MAGENTA_WORM;
    case CREATURE_RED_DRAGON:
        return POSSESSIONS_RED_DRAGON;
    default:
        return -1;
    }
}

/* A dead creature leaves the fixed possessions of its type. Possessions with bit 15 drop half of the time. */
static void drop_fixed_possessions(uint16_t creature_type, int16_t x, int16_t y, int16_t cell, int16_t sound_mode)
{
    int16_t index = fixed_possession_list_start(creature_type);
    if (index < 0)
        return;
    bool cursed = creature_type == CREATURE_ANIMATED_ARMOUR;
    bool weapon_dropped = false;
    for (uint16_t entry; (entry = tables_get_fixed_possession(index)) != 0; index++) {
        if ((entry & FIXED_POSSESSION_RANDOM_DROP) && random_bit())
            continue;
        uint16_t info_index = entry & FIXED_POSSESSION_INDEX_MASK, thing_type, object_type;
        if (info_index >= OBJECT_INFO_FIRST_JUNK) {
            thing_type = THING_JUNK;
            object_type = info_index - OBJECT_INFO_FIRST_JUNK;
        } else if (info_index >= OBJECT_INFO_FIRST_ARMOUR) {
            thing_type = THING_ARMOUR;
            object_type = info_index - OBJECT_INFO_FIRST_ARMOUR;
        } else {
            weapon_dropped = true;
            thing_type = THING_WEAPON;
            object_type = info_index - OBJECT_INFO_FIRST_WEAPON;
        }
        uint16_t thing = rec_find_unused_thing(thing_type);
        if (thing == THING_NONE)
            continue;
        rec_object_set_type(thing, object_type);
        rec_object_set_cursed(thing, cursed);
        uint16_t drop_cell = cell;
        if (cell == CELL_CENTERED || random_2bits() == 0)
            drop_cell = random_2bits();
        move_get_move_result(&(Move){.thing = THING_WITH_CELL(thing, drop_cell), .to.x = x, .to.y = y, .enters = true},
                             NULL);
    }
    sound_request_play(weapon_dropped ? SOUND_METALLIC_THUD : SOUND_WOODEN_THUD, (Location){x, y}, sound_mode,
                       sound_mode == SOUND_MODE_EVENT ? 1 : 0);
}

void group_drop_group_possessions(int16_t x, int16_t y, uint16_t group, int16_t sound_mode)
{
    uint16_t type = rec_group_creature_type(group);
    if (sound_mode != SOUND_MODE_NONE &&
        (tables_get_creature_info(type)->attributes & CREATURE_DROPS_FIXED_POSSESSIONS)) {
        uint8_t cells = get_cells(group, g_current_map_index);
        for (int16_t i = rec_group_last_creature(group); i >= 0; i--)
            drop_fixed_possessions(type, x, y, cells == CELL_CENTERED ? CELL_CENTERED : group_cell(cells, i),
                                   sound_mode);
    }
    uint16_t thing = rec_group_possessions(group);
    if (thing == THING_ENDOFLIST)
        return;
    bool weapon_dropped = false;
    uint16_t next;
    do {
        next = rec_thing_next(thing);
        uint16_t cell = random_2bits();
        if (THING_TYPE(thing) == THING_WEAPON)
            weapon_dropped = true;
        move_get_move_result(&(Move){.thing = THING_WITH_CELL(thing, cell), .to.x = x, .to.y = y, .enters = true},
                             NULL);
        thing = next;
    } while (next != THING_ENDOFLIST);
    if (sound_mode != SOUND_MODE_NONE)
        sound_request_play(weapon_dropped ? SOUND_METALLIC_THUD : SOUND_WOODEN_THUD, (Location){x, y}, sound_mode,
                           sound_mode == SOUND_MODE_EVENT ? 1 : 0);
}

void group_delete(int16_t x, int16_t y)
{
    uint16_t group = map_find_thing((Location){x, y}, THING_GROUP);
    if (group == THING_ENDOFLIST)
        return;
    for (int16_t i = 0; i < GROUP_MAX_CREATURES; i++)
        rec_group_set_health(group, i, 0);
    move_get_move_result(&(Move){.thing = group, .from.x = x, .from.y = y, .leaves = true}, NULL);
    rec_thing_set_free(group);
    if (g_current_map_index == g_party_map_index) {
        g_active_groups[rec_group_cells(group)].group_index = -1;
        g_active_group_count--;
    }
    delete_events(x, y);
}

void group_delete_all_except(Location at)
{
    for (int16_t gx = 0; gx < g_current_map_width; gx++)
        for (int16_t gy = 0; gy < g_current_map_height; gy++)
            if (map_find_thing((Location){gx, gy}, THING_GROUP) != THING_ENDOFLIST && (gx != at.x || gy != at.y))
                group_delete(gx, gy);
}

/* Events of the creatures after a dead creature move down one index; the events of the dead creature go away. */
static void shift_creature_events(int16_t x, int16_t y, uint16_t dead_creature)
{
    for (uint16_t i = 0; i < g_timeline.high_water; i++) {
        TimelineEvent *event = &g_timeline.events[i];
        if (EVENT_MAP(event) != g_current_map_index || event_square_x(event) != x || event_square_y(event) != y ||
            event->type < EVENT_UPDATE_ASPECT_CREATURE_0 || event->type > EVENT_UPDATE_BEHAVIOR_CREATURE_3)
            continue;
        uint16_t first = event->type < EVENT_UPDATE_BEHAVIOR_GROUP ? EVENT_UPDATE_ASPECT_CREATURE_0
                                                                   : EVENT_UPDATE_BEHAVIOR_CREATURE_0;
        uint16_t creature = event->type - first;
        if (creature == dead_creature) {
            timeline_delete_event(i);
        } else if (dead_creature < creature) {
            event->type--;
            timeline_fix_chronology(timeline_get_index(i));
        }
    }
}

/* One creature of a group of more than one dies: it drops its fixed possessions (in a fall, its cell goes to drops),
   an attacking group may flee, and the creatures after it move down one place. */
static void remove_dead_creature(uint16_t group, int16_t creature_index, Location at, int16_t cell,
                                 GroupFallDrops *drops)
{
    uint16_t type = rec_group_creature_type(group);
    const CreatureInfo *info = tables_get_creature_info(type);
    uint8_t cells = get_cells(group, g_current_map_index);
    uint16_t last = rec_group_last_creature(group);
    uint16_t directions = get_directions(group, g_current_map_index);
    if (info->attributes & CREATURE_DROPS_FIXED_POSSESSIONS) {
        if (!drops)
            drop_fixed_possessions(type, at.x, at.y, cell, SOUND_MODE_EVENT);
        else
            drops->cells[drops->count++] = (uint8_t)cell;
    }
    bool on_party_map = g_current_map_index == g_party_map_index;
    ActiveGroup *active = on_party_map ? active_of(group) : NULL;
    if (rec_group_behavior(group) == BEHAVIOR_ATTACK) {
        shift_creature_events(at.x, at.y, creature_index);
        uint16_t fear = creature_fear_resistance(info);
        if (on_party_map && fear != FEAR_RESISTANCE_NONE_FEARLESS) {
            fear += last - 1;
            if (fear < (random16() & FEAR_RANDOM_MASK)) {
                active->delay_fleeing =
                    random16() % (FLEE_DELAY_RANGE - FLEE_DELAY_FEAR_STEP * fear) + FLEE_DELAY_MINIMUM;
                rec_group_set_behavior(group, BEHAVIOR_FLEE);
            }
        }
    }
    for (uint16_t i = creature_index; i < last; i++) {
        rec_group_set_health(group, i, rec_group_health(group, i + 1));
        directions = set_cell_value(directions, i, group_cell(directions, i + 1));
        cells = set_cell_value(cells, i, group_cell(cells, i + 1));
        if (on_party_map)
            active->aspect[i] = active->aspect[i + 1];
    }
    group_set_cells(group, cells & CELLS_OF_THREE_CREATURES, g_current_map_index);
    group_set_directions(group, directions, g_current_map_index);
    rec_group_set_last_creature(group, last - 1);
}

/* Damage to one creature. Without drops (not a fall) a dead creature drops its possessions now and the last one
   deletes the group. */
static int16_t damage_creature(uint16_t group, int16_t creature_index, Location at, int16_t damage,
                               GroupFallDrops *drops)
{
    uint16_t type = rec_group_creature_type(group);
    const CreatureInfo *info = tables_get_creature_info(type);
    if (info->defense == CREATURE_INVULNERABLE)
        return OUTCOME_NO_CREATURE_KILLED;
    if (rec_group_health(group, creature_index) >
        (uint16_t)damage) { /* unsigned as in the original: a negative damage kills */
        if (damage > 0)
            rec_group_set_health(group, creature_index, rec_group_health(group, creature_index) - damage);
        return OUTCOME_NO_CREATURE_KILLED;
    }
    uint8_t cells = get_cells(group, g_current_map_index);
    uint16_t cell = cells == CELL_CENTERED ? CELL_CENTERED : group_cell(cells, creature_index);
    uint16_t last = rec_group_last_creature(group);
    int16_t outcome;
    if (last == 0) {
        if (!drops) {
            group_drop_group_possessions(at.x, at.y, map_find_thing(at, THING_GROUP), SOUND_MODE_EVENT);
            group_delete(at.x, at.y);
        }
        outcome = OUTCOME_ALL_CREATURES_KILLED;
    } else {
        remove_dead_creature(group, creature_index, at, cell, drops);
        outcome = OUTCOME_SOME_CREATURES_KILLED;
    }
    static const uint16_t smoke_attack[] = {SMOKE_ATTACK_QUARTER, SMOKE_ATTACK_HALF, SMOKE_ATTACK_FULL,
                                            SMOKE_ATTACK_FULL};
    explosion_create(EXPLOSION_THING(EXPLOSION_SMOKE), smoke_attack[creature_size(info)], at, at, cell);
    return outcome;
}

DamageOutcome group_damage_creature(uint16_t group, int16_t creature_index, Location at, int16_t damage)
{
    return damage_creature(group, creature_index, at, damage, NULL);
}

/* The same attack, with a random spread of 1/8, on every creature of the group. */
static int16_t damage_all_creatures(uint16_t group, Location at, int16_t attack, GroupFallDrops *drops)
{
    if (attack < 1)
        return OUTCOME_NO_CREATURE_KILLED;
    int16_t spread = (attack >> 3) + 1;
    int16_t all_killed = 1;
    int16_t some_killed = 0;
    for (int16_t i = rec_group_last_creature(group); i >= 0; i--) {
        int16_t damage = attack - spread + random16() % (spread * 2);
        int16_t outcome = damage_creature(group, i, at, damage, drops);
        all_killed = all_killed && outcome;
        some_killed = some_killed || outcome;
    }
    return all_killed    ? OUTCOME_ALL_CREATURES_KILLED
           : some_killed ? OUTCOME_SOME_CREATURES_KILLED
                         : OUTCOME_NO_CREATURE_KILLED;
}

DamageOutcome group_damage_all_creatures(uint16_t group, int16_t x, int16_t y, int16_t attack)
{
    return damage_all_creatures(group, (Location){x, y}, attack, NULL);
}

uint16_t group_get_poison_attack(uint16_t creature_type, int16_t poison_attack)
{
    uint16_t resistance = creature_poison_resistance(tables_get_creature_info(creature_type));
    if (!poison_attack || resistance == POISON_IMMUNE)
        return 0;
    return (uint16_t)((poison_attack + random_2bits()) * POISON_ATTACK_FACTOR) /
           (resistance + 1); /* 16-bit product as in the original */
}

void group_door_damages_creatures(int16_t x, int16_t y)
{
    uint16_t group = map_find_thing((Location){x, y}, THING_GROUP);
    if (group_damage_all_creatures(group, x, y, DOOR_DAMAGE) != OUTCOME_ALL_CREATURES_KILLED)
        group_process_reaction_event((Location){x, y}, REACTION_DANGER_ON_SQUARE);
}

/* A Giggler steals objects from random slots while the champion fails the dexterity and luck checks, then flees. */
static void steal_from_champion(uint16_t group, int16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    bool stole = false;
    int16_t chance = STEAL_CHANCE_START - champion_get_dexterity(champion_index);
    uint16_t slot_index = random16();
    while (chance > 0 && !champion_is_lucky(champion_index, chance)) {
        uint16_t slot = tables_get_steal_slot(slot_index & 7);
        if (champion->slots[slot] != THING_NONE) {
            stole = true;
            uint16_t thing = inventory_take_slot_object(champion_index, slot);
            if (rec_group_possessions(group) == THING_ENDOFLIST)
                rec_group_set_possessions(group, thing);
            else
                map_link_thing_to_list(thing, rec_group_possessions(group), -1, 0);
        }
        slot_index = (slot_index & 7) + 1;
        chance -= STEAL_CHANCE_STEP;
    }
    if ((random16() & 7) && (!stole || !random_bit()))
        return;
    active_of(group)->delay_fleeing = (random16() & THEFT_FLEE_RANDOM_MASK) + FLEE_DELAY_MINIMUM;
    rec_group_set_behavior(group, BEHAVIOR_FLEE);
}

/* A wall, a solid fake wall, or a closed door that is not see-through hides the party. */
static bool is_view_party_blocked(int16_t x, int16_t y)
{
    uint8_t square = rec_square_byte(g_current_map_index, x, y);
    switch (rec_square_type(square)) {
    case SQUARE_DOOR: {
        int16_t state = rec_door_state(square);
        bool see_through = door_is_see_through(map_get_square_first_thing(x, y));
        return (state == 3 || state == DOOR_CLOSED) && !see_through;
    }
    case SQUARE_WALL:
        return true;
    case SQUARE_FAKE_WALL:
        return !(square & FAKE_WALL_OPEN);
    default:
        return false;
    }
}

/* Only walls and solid fake walls stop the smell of the party. */
static bool is_smell_party_blocked(int16_t x, int16_t y)
{
    uint8_t square = rec_square_byte(g_current_map_index, x, y);
    return rec_square_type(square) == SQUARE_WALL ||
           (rec_square_type(square) == SQUARE_FAKE_WALL && !(square & (FAKE_WALL_OPEN | FAKE_WALL_IMAGINARY)));
}

/* Slope of the line from the source in fixed point (* 64); 128 for a zero divisor. */
static int16_t get_line_slope(int16_t along, int16_t across)
{
    return along ? across * LINE_SLOPE_ONE / along : LINE_SLOPE_STRAIGHT;
}

/* Walks a line from the destination back to the source. Returns the square distance, or 0 when a square blocks. */
static int16_t unblocked_distance(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y,
                                  bool (*is_blocked)(int16_t, int16_t))
{
    if (abs16(src_x - dst_x) + abs16(src_y - dst_y) <= 1)
        return 1;
    int16_t dx = abs16(dst_x - src_x);
    int16_t dy = abs16(dst_y - src_y);
    bool y_major = dy > dx;
    bool diagonal = dy == dx;
    int16_t step_x = dst_x - src_x > 0 ? -1 : 1;
    int16_t step_y = dst_y - src_y > 0 ? -1 : 1;
    int16_t x = dst_x;
    int16_t y = dst_y;
    int16_t slope =
        y_major ? get_line_slope(dst_y - src_y, dst_x - src_x) : get_line_slope(dst_x - src_x, dst_y - src_y);
    do {
        if (diagonal) {
            if (is_blocked(x + step_x, y) && is_blocked(x, y + step_y))
                return 0;
            y += step_y;
            x += step_x;
            if (is_blocked(x, y))
                return 0;
        } else {
            int16_t error_x =
                y_major ? get_line_slope(y - src_y, x + step_x - src_x) : get_line_slope(x + step_x - src_x, y - src_y);
            int16_t error_y =
                y_major ? get_line_slope(y + step_y - src_y, x - src_x) : get_line_slope(x - src_x, y + step_y - src_y);
            error_x = abs16(error_x - slope);
            error_y = abs16(error_y - slope);
            if (error_y > error_x)
                x += step_x;
            else
                y += step_y;
            if (is_blocked(x, y)) {
                if (error_x != error_y)
                    return 0;
                y -= step_y;
                x += step_x;
                if (is_blocked(x, y))
                    return 0;
            }
        }
    } while (abs16(x - src_x) + abs16(y - src_y) > 1);
    return group_get_square_distance(src_x, src_y, dst_x, dst_y);
}

/* Distance to the party when a creature (or any creature with -1) sees it, else 0. Light, invisibility, and
   the sight range limit the view. Quirk kept: the invisibility range -10 is compared unsigned, so it does not hide
   the party from a creature that does not see invisible. */
static uint16_t visible_party_distance(const GroupTurn *t, uint16_t group, int16_t creature_index, int16_t x, int16_t y)
{
    const CreatureInfo *info = group_info(group);
    if (!(info->attributes & CREATURE_SIDE_ATTACK)) {
        uint8_t directions = active_of(group)->directions;
        uint16_t seen[GROUP_MAX_CREATURES];
        int16_t count = 0;
        if (creature_index < 0) {
            for (int16_t i = rec_group_last_creature(group); i >= 0; i--) {
                uint16_t direction = group_cell(directions, i);
                bool known = false;
                for (int16_t k = 0; k < count; k++)
                    known |= seen[k] == direction;
                if (!known)
                    seen[count++] = direction;
            }
        } else {
            seen[count++] = group_cell(directions, creature_index);
        }
        bool visible = false;
        while (count && !visible) {
            count--;
            visible = group_is_destination_visible(seen[count], x, y, g_party_map_x, g_party_map_y);
        }
        if (!visible)
            return 0;
    }
    uint16_t range = creature_sight_range(info);
    if (g_party.invisibility_count && !(info->attributes & CREATURE_SEES_INVISIBLE))
        range = INVISIBLE_PARTY_SIGHT_RANGE;
    else if (!(info->attributes & CREATURE_NIGHT_VISION))
        range -= g_party_light_level >> 1;
    if (range < t->distance_to_party) {
        if (t->distance_to_party == 1) {
            uint16_t random1 = random16();
            uint16_t random2 = random16();
            range += random1 % (creature_hearing_range(info) + 1) + random2 % (creature_smell_range(info) + 1);
            if (!(random16() & 7))
                range += random_bit() + ADJACENT_SIGHT_BONUS;
        }
        if ((uint16_t)(range + (random16() & 7) - SIGHT_ROLL_CENTER) <
            t->distance_to_party) /* 16-bit wrap as in the original */
            return 0;
    }
    return unblocked_distance(x, y, g_party_map_x, g_party_map_y, is_view_party_blocked);
}

int16_t group_get_party_scent(int16_t x, int16_t y)
{
    uint16_t scent = party_pack_position(g_current_map_index, x, y);
    for (int16_t i = g_party.scent_count - 1; i >= 0; i--)
        if (g_party.scents[i] == scent)
            return i;
    return -1;
}

/* The direction toward the party by smell: straight when close enough, else along the scent trail. -1: none. */
static int16_t get_smelled_party_direction(GroupTurn *t, const CreatureInfo *info, int16_t x, int16_t y)
{
    uint16_t smell = creature_smell_range(info);
    if (!smell)
        return -1;
    if (t->distance_to_party <= (smell + 1) >> 1 &&
        unblocked_distance(x, y, g_party_map_x, g_party_map_y, is_smell_party_blocked)) {
        t->secondary_direction = t->directions_to_party.secondary;
        return t->directions_to_party.primary;
    }
    int16_t scent = group_get_party_scent(x, y);
    if (scent >= 0 && SCENT_TRAIL_THRESHOLD - 2 * smell < random_2bits() + g_party.scent_strengths[scent]) {
        uint16_t next = g_party.scents[scent + 1];
        DirectionPair directions =
            group_get_directions_to((Location){x, y}, (Location){party_position_x(next), party_position_y(next)});
        t->secondary_direction = directions.secondary;
        return directions.primary;
    }
    return -1;
}

/* Can the creature step one square in a direction? Lord Chaos stops at fluxcages. A wary creature avoids a
   teleporter to a map that does not allow its type. */
static MovementTest test_movement(GroupTurn *t, int16_t direction, bool allow_imaginary)
{
    const CreatureInfo *info = t->info;
    int16_t x = t->x;
    int16_t y = t->y;
    MovementTest result = {false, false};
    t->tested_directions[direction] = 1;
    last_blocked_by_party = false;
    if (info->movement_ticks == CREATURE_IMMOBILE)
        return result;
    map_step(direction, 1, 0, &x, &y);
    uint8_t square = 0;
    int16_t type = SQUARE_WALL;
    bool open = false;
    if (x >= 0 && x < g_current_map_width && y >= 0 && y < g_current_map_height) {
        square = rec_square_byte(g_current_map_index, x, y);
        type = rec_square_type(square);
        bool imaginary_allowed = (square & PIT_IMAGINARY) && allow_imaginary;
        open = type != SQUARE_WALL && type != SQUARE_STAIRS &&
               (type != SQUARE_PIT || imaginary_allowed || !(square & PIT_OPEN) ||
                (info->attributes & CREATURE_LEVITATES)) &&
               (type != SQUARE_FAKE_WALL || (square & FAKE_WALL_OPEN) || imaginary_allowed);
    }
    if (!open)
        return result;
    if (info->attributes & CREATURE_ARCHENEMY) {
        for (uint16_t thing = map_get_square_first_thing(x, y); thing != THING_ENDOFLIST;
             thing = rec_thing_next(thing)) {
            if (THING_TYPE(thing) == THING_EXPLOSION && rec_explosion_type(thing) == EXPLOSION_FLUXCAGE) {
                t->flux_cages[direction] = 1;
                t->flux_cage_count++;
                return result;
            }
        }
    }
    if (type == SQUARE_TELEPORTER && (square & TELEPORTER_OPEN) &&
        creature_wariness(info) >= WARINESS_AVOIDS_FORBIDDEN_TELEPORTER) {
        uint16_t teleporter = map_get_square_first_thing(x, y);
        if ((rec_teleporter_scope(teleporter) & TELEPORTER_SCOPE_CREATURES) &&
            !group_is_allowed_on_map(last_turn.thing, rec_teleporter_target_map(teleporter)))
            return result;
    }
    result.blocked_by_party = map_is_party_on((Location){x, y});
    last_blocked_by_party = result.blocked_by_party;
    if (result.blocked_by_party)
        return result;
    if (type == SQUARE_DOOR) {
        int16_t height = rec_door_is_vertical(map_get_square_first_thing(x, y)) ? creature_height(info->attributes) : 1;
        int16_t state = rec_door_state(square);
        if (height < state && state != DOOR_DESTROYED && !(info->attributes & CREATURE_NON_MATERIAL))
            return result;
    }
    result.possible = map_find_thing((Location){x, y}, THING_GROUP) == THING_ENDOFLIST;
    return result;
}

/* The first untested direction where the creature can move, or -1. */
static int16_t get_first_free_direction(GroupTurn *t, bool allow_imaginary)
{
    for (int16_t direction = 0; direction < DIRECTION_COUNT; direction++)
        if (!t->tested_directions[direction] && test_movement(t, direction, allow_imaginary).possible)
            return direction;
    return -1;
}

/* Lord Chaos jumps two squares: the first square must have no fluxcage, the second must be free. */
static bool can_archenemy_double_move(GroupTurn *t, int16_t direction)
{
    if (t->flux_cages[direction])
        return false;
    int16_t x = t->x;
    int16_t y = t->y;
    map_step(direction, 1, 0, &t->x, &t->y);
    bool possible = test_movement(t, direction, false).possible;
    t->x = x;
    t->y = y;
    return possible;
}

/* Turns one creature. A creature never turns around at once: it turns left or right first. Two half-square
   creatures turn together, once per game tick. */
static void rotate_creature(ActiveGroup *active, int16_t direction, int16_t creature_index, bool turn_pair)
{
    if (turn_pair && g_game_time == set_direction.time && active == set_direction.group)
        return;
    uint16_t directions = active->directions;
    if (NORMALIZE(group_cell(directions, creature_index) - direction) == 2)
        direction = NORMALIZE((random16() & 2) + direction + 1);
    directions = set_cell_value(directions, creature_index, direction);
    if (turn_pair) {
        directions = set_cell_value(directions, creature_index ^ 1, direction);
        set_direction.time = g_game_time;
        set_direction.group = active;
    }
    active->directions = directions;
}

/* Turns the group: the first creature always, the others half of the time. */
static void rotate_group(ActiveGroup *active, int16_t direction, int16_t last_creature_index, uint16_t size)
{
    bool turn_pair = last_creature_index && size == CREATURE_SIZE_HALF;
    if (turn_pair)
        last_creature_index--;
    for (int16_t i = last_creature_index; i >= 0; i--)
        if (i == 0 || random_bit())
            rotate_creature(active, direction, i, turn_pair);
}

/* The spell that a creature type casts at the party: its explosion type and whether the cast has a sound. */
typedef struct {
    uint16_t explosion;
    bool sound;
} CreatureSpell;

/* Explosion type of the spell that a creature type casts at the party. */
static CreatureSpell get_creature_spell_explosion(uint16_t creature_type)
{
    switch (creature_type) {
    case CREATURE_SWAMP_SLIME:
        return (CreatureSpell){EXPLOSION_SLIME, false};
    case CREATURE_WIZARD_EYE:
        return (CreatureSpell){(random16() & 7) ? EXPLOSION_LIGHTNING_BOLT : EXPLOSION_OPEN_DOOR, true};
    case CREATURE_VEXIRK:
    case CREATURE_LORD_CHAOS:
        if (random_bit())
            return (CreatureSpell){EXPLOSION_FIREBALL, true};
        switch (random_2bits()) {
        case 0:
            return (CreatureSpell){EXPLOSION_HARM_NON_MATERIAL, true};
        case 1:
            return (CreatureSpell){EXPLOSION_LIGHTNING_BOLT, true};
        case 2:
            return (CreatureSpell){EXPLOSION_POISON_CLOUD, true};
        default:
            return (CreatureSpell){EXPLOSION_OPEN_DOOR, true};
        }
    case CREATURE_MATERIALIZER:
        return (CreatureSpell){random_bit() ? EXPLOSION_POISON_CLOUD : EXPLOSION_FIREBALL, true};
    default:
        return (CreatureSpell){EXPLOSION_FIREBALL, true};
    }
}

/* A creature casts its spell: an explosion projectile with a random kinetic energy from its attack. */
static void launch_creature_spell(const CreatureInfo *info, uint16_t explosion, Location at, int16_t cell,
                                  int16_t direction)
{
    uint16_t kinetic_energy = (info->attack >> 2) + 1;
    kinetic_energy += random16() % kinetic_energy;
    kinetic_energy += random16() % kinetic_energy;
    projectile_create(&(ProjectileLaunch){
        .thing = EXPLOSION_THING(explosion),
        .at = at,
        .cell = cell,
        .direction = direction,
        .kinetic_energy = bounded_value(CREATURE_SPELL_MIN_KINETIC_ENERGY, kinetic_energy, KINETIC_ENERGY_MAXIMUM),
        .attack = info->dexterity,
        .step_energy = CREATURE_SPELL_STEP_ENERGY,
    });
}

/* One creature attacks: a melee hit (or theft) on a champion in front, or a spell projectile toward the party.
   Returns true when the creature attacked. */
static bool is_creature_attacking(const GroupTurn *t, uint16_t group, int16_t x, int16_t y, int16_t creature_index)
{
    int16_t primary = t->directions_to_party.primary;
    int16_t spell = 0;
    int16_t spell_sound = 1;
    g_last_creature_attack_time = g_game_time;
    uint16_t type = rec_group_creature_type(group);
    const CreatureInfo *info = tables_get_creature_info(type);
    uint8_t cells = active_of(group)->cells;
    int16_t cell = cells == CELL_CENTERED ? NORMALIZE(primary + random_bit())
                                          : direction_front_cell(primary, group_cell(cells, creature_index));
    if (creature_attack_range(info) <= 1 || (t->distance_to_party <= 1 && !random_bit())) {
        int16_t champion_index;
        if (!(info->attributes & CREATURE_ATTACK_ANY_CHAMPION)) {
            champion_index = group_get_target_champion_index(x, y, cell);
            if (champion_index < 0)
                return false;
        } else {
            champion_index = random_2bits();
            int16_t tried = 0;
            while (tried < CHAMPION_MAX_COUNT && !g_champions[champion_index].current_health) {
                champion_index = (champion_index + 1) % CHAMPION_MAX_COUNT;
                tried++;
            }
            if (tried == CHAMPION_MAX_COUNT)
                return false;
        }
        if (type == CREATURE_GIGGLER) {
            steal_from_champion(group, champion_index);
        } else {
            int16_t damage = group_get_champion_damage(group, champion_index) + 1;
            Champion *champion = &g_champions[champion_index];
            if (champion->maximum_damage_received < damage) {
                champion->maximum_damage_received = damage;
                champion->direction_maximum_damage_received = direction_back(primary);
            }
        }
    } else {
        spell = 1;
        CreatureSpell cast = get_creature_spell_explosion(type);
        spell_sound = cast.sound;
        launch_creature_spell(info, cast.explosion, (Location){x, y}, cell, primary);
    }
    int16_t sound_index = get_sound_index(info);
    int16_t sound = sound_index >= 0 ? tables_get_creature_sound(sound_index)->attack : -1;
    if (sound != -1)
        sound_request_play(sound, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
    else if (spell && spell_sound)
        sound_request_play(SOUND_SPELL, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
    return true;
}

/* Adds the next event of a group. When the aspect update comes first, the event becomes an aspect event and keeps
   the delay to the behavior update in byte 8. */
static void add_event(TimelineEvent *event, uint32_t aspect_update_time)
{
    uint32_t time = event->map_time & EVENT_TIME_MASK;
    if (aspect_update_time < time) {
        event->type -= EVENT_BEHAVIOR_TO_ASPECT;
        event->c = event_group_c(time - aspect_update_time);
        event->map_time = (event->map_time & ~(uint32_t)EVENT_TIME_MASK) | aspect_update_time;
    } else {
        event->c = event_group_c(aspect_update_time - time);
    }
    timeline_add_event(event);
}

int16_t group_get_square_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    return abs16(x1 - x2) + abs16(y1 - y2);
}

bool group_is_destination_visible(int16_t direction, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y)
{
    int16_t ahead, side;
    switch (direction) {
    case DIRECTION_NORTH:
        ahead = src_y - dst_y;
        side = src_x - dst_x;
        break;
    case DIRECTION_EAST:
        ahead = dst_x - src_x;
        side = dst_y - src_y;
        break;
    case DIRECTION_SOUTH:
        ahead = dst_y - src_y;
        side = dst_x - src_x;
        break;
    default:
        ahead = src_x - dst_x;
        side = src_y - dst_y;
        break;
    }
    ahead++;
    return ahead > 0 && abs16(side) <= ahead;
}

DirectionPair group_get_directions_to(Location from, Location to)
{
    int16_t src_x = from.x, src_y = from.y, dst_x = to.x, dst_y = to.y;
    if (dst_x == src_x) {
        int16_t secondary = (random16() & 2) + 1;
        return (DirectionPair){src_y <= dst_y ? DIRECTION_SOUTH : DIRECTION_NORTH, secondary};
    }
    if (dst_y == src_y) {
        int16_t secondary = random16() & 2;
        return (DirectionPair){src_x <= dst_x ? DIRECTION_EAST : DIRECTION_WEST, secondary};
    }
    int16_t primary = 0;
    while (!group_is_destination_visible(primary, src_x, src_y, dst_x, dst_y))
        primary++;
    int16_t secondary = direction_right(primary);
    int16_t both_visible = group_is_destination_visible(secondary, src_x, src_y, dst_x, dst_y);
    if (!both_visible && primary == 0) {
        secondary = DIRECTION_WEST;
        both_visible = group_is_destination_visible(secondary, src_x, src_y, dst_x, dst_y);
    }
    if (!both_visible)
        return (DirectionPair){primary, NORMALIZE(primary + (random16() & 2) + 1)};
    if (random_bit())
        return (DirectionPair){secondary, primary};
    return (DirectionPair){primary, secondary};
}

int16_t group_get_target_champion_index(int16_t x, int16_t y, int16_t cell)
{
    if (!g_party_champion_count || abs16(x - g_party_map_x) + abs16(y - g_party_map_y) > 1)
        return -1;
    uint8_t cells[CELL_COUNT];
    order_attack_cells(cells, (Location){g_party_map_x, g_party_map_y}, (Location){x, y}, cell);
    for (int16_t i = 0; i < CELL_COUNT; i++) {
        int16_t champion_index = champion_get_index_in_cell(cells[i]);
        if (champion_index >= 0)
            return champion_index;
    }
    return -1;
}

/* Wounds of a creature hit: random wound masks weighted by the wound probability nibbles of the creature. */
static uint16_t hit_wounds(const CreatureInfo *info)
{
    uint16_t random = random16();
    if (!(random & PLAIN_HIT_RANDOM_BITS))
        return random & 1;
    uint16_t probabilities = info->wound_probabilities;
    uint16_t index = 0;
    while ((probabilities & WOUND_PROBABILITY_MASK) < (random & WOUND_PROBABILITY_MASK)) {
        index++;
        probabilities >>= WOUND_PROBABILITY_SHIFT;
    }
    return tables_get_wound_mask(index);
}

int16_t group_get_champion_damage(uint16_t group, int16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    if (champion_index < 0 || champion_index >= g_party_champion_count || !champion->current_health)
        return 0;
    if (g_session.party_sleeping)
        champion_wake_up();
    int16_t difficulty = rec_current_map_difficulty() * 2;
    const CreatureInfo *info = group_info(group);
    champion_add_skill_experience(champion_index, SKILL_PARRY, creature_experience(info));
    int16_t dodge_bonus;
    if (g_party.invisibility_count && !(info->attributes & CREATURE_SEES_INVISIBLE))
        dodge_bonus = INVISIBLE_PARTY_DODGE_BONUS;
    else if (info->attributes & CREATURE_NIGHT_VISION)
        dodge_bonus = 0;
    else
        dodge_bonus = g_party_light_level * 2;
    if (!g_session.party_sleeping && info->dexterity != CREATURE_NEVER_HITS) {
        uint16_t dexterity = champion_get_dexterity(champion_index);
        uint16_t aim = (random16() & AIM_RANDOM_MASK) + info->dexterity + difficulty + dodge_bonus - AIM_RANDOM_CENTER;
        if (aim <= dexterity && random_2bits())
            return 0;
        if (champion_is_lucky(champion_index, LUCK_PERCENT_AGAINST_CREATURE))
            return 0;
    }
    uint16_t wounds = hit_wounds(info);
    uint16_t random = random16();
    int16_t damage = (random & CREATURE_DAMAGE_RANDOM_MASK) + info->attack + difficulty -
                     2 * champion_get_skill_level(champion_index, SKILL_PARRY, false, false);
    if (damage <= 1) {
        if (random_bit())
            return 0;
        damage = random_2bits() + 2;
    }
    damage >>= 1;
    random = random16();
    damage += random % damage + random_2bits();
    damage += random16() % damage;
    damage >>= 2;
    damage += random_2bits() + 1;
    if (random_bit())
        damage -= random16() % ((damage >> 1) + 1) - 1;
    damage = champion_add_pending_damage(champion_index, damage, wounds, info->attack_type);
    if (!damage)
        return 0;
    sound_request_play(SOUND_CHAMPION_0_DAMAGED + champion_index, (Location){g_party_map_x, g_party_map_y},
                       SOUND_MODE_EVENT, 1);
    if (info->poison_attack && random_bit()) {
        uint16_t poison = champion_get_adjusted_attack(champion_index, STAT_VITALITY, info->poison_attack);
        champion_poison(champion_index, poison);
    }
    return damage;
}

/* Damage of a hit that misses the strength path: a small random amount, sometimes more. */
static int16_t weak_hit_damage(int16_t base)
{
    int16_t damage = random_2bits();
    if (!damage)
        return 0;
    damage++;
    base += random16() & WEAK_HIT_RANDOM_MASK;
    if (base > 0 || random_bit()) {
        damage += random_2bits();
        if (!random_2bits())
            damage += max16(0, base + (random16() & WEAK_HIT_RANDOM_MASK));
    }
    return damage;
}

/* Damage of a melee hit: the strength with a random part, scaled by the action damage factor, minus the creature
   armour (less with a Diamond Edge or Hardcleave), then a random spread. 0 when the hit does no damage. */
static int16_t melee_hit_damage(int16_t champion_index, const CreatureInfo *info, int16_t icon, int16_t difficulty,
                                uint16_t damage_factor)
{
    int16_t damage = 0;
    /* Bug fix: without strength the original uses a stale register as the base; the port uses 0. */
    int16_t strength = champion_get_strength(champion_index, SLOT_HAND_ACTION);
    int16_t base = 0;
    if (strength) {
        strength += random16() % ((strength >> 1) + 1);
        damage = ((int32_t)strength * damage_factor) >> 5;
        int16_t armour = (random16() & ARMOUR_RANDOM_MASK) + info->defense + difficulty;
        if (icon == ICON_DIAMOND_EDGE)
            armour -= armour >> 2;
        else if (icon == ICON_HARDCLEAVE)
            armour -= armour >> 3;
        damage += random16() & STRENGTH_DAMAGE_RANDOM_MASK;
        damage -= armour;
        base = damage;
    }
    if (damage <= 1)
        damage = weak_hit_damage(base);
    if (damage) {
        damage >>= 1;
        uint16_t random = random16();
        damage += random % damage + random_2bits();
        damage += random16() % damage;
        damage >>= 2;
        damage += random_2bits() + 1;
        if (icon == ICON_VORPAL_BLADE && !(info->attributes & CREATURE_NON_MATERIAL))
            damage >>= 1;
    }
    return damage;
}

int16_t group_get_melee_action_damage(const MeleeAttack *melee)
{
    int16_t champion_index = melee->champion_index;
    uint16_t group = melee->group;
    int16_t creature_index = melee->creature_index;
    Location at = melee->at;
    uint16_t hit_probability = melee->hit_probability;
    uint16_t damage_factor = melee->damage_factor;
    int16_t skill_index = melee->skill_index;
    if (champion_index >= g_party_champion_count)
        return 0;
    Champion *champion = &g_champions[champion_index];
    if (!champion->current_health)
        return 0;
    int16_t difficulty = rec_current_map_difficulty() * 2;
    int16_t damage = 0;
    int16_t outcome = OUTCOME_NO_CREATURE_KILLED;
    const CreatureInfo *info = group_info(group);
    bool hit = false;
    if (info->dexterity != CREATURE_NEVER_HITS) {
        int16_t icon = object_get_icon_index(champion->slots[SLOT_HAND_ACTION]);
        bool always_hits = (hit_probability & HIT_PROBABILITY_ALWAYS_HITS) != 0;
        hit_probability &= HIT_PROBABILITY_MASK;
        if (!(info->attributes & CREATURE_NON_MATERIAL) || always_hits) {
            uint16_t dexterity = champion_get_dexterity(champion_index);
            uint16_t defense = (random16() & AIM_RANDOM_MASK) + info->dexterity + difficulty + g_party_light_level * 2 -
                               AIM_RANDOM_CENTER;
            hit = defense < dexterity || !random_2bits() ||
                  champion_is_lucky(champion_index, LUCK_PERCENT_AGAINST_MISS - hit_probability);
        }
        if (hit) {
            damage = melee_hit_damage(champion_index, info, icon, difficulty, damage_factor);
            hit = damage != 0;
        }
    }
    if (hit) {
        uint16_t random = random16();
        if ((random & DOUBLE_DAMAGE_RANDOM_MASK) < champion_get_skill_level(champion_index, skill_index, false, false))
            damage = damage * 2 + DOUBLE_DAMAGE_BONUS;
        outcome = group_damage_creature(group, creature_index, at, damage);
        /* 16-bit product as in the original */
        champion_add_skill_experience(champion_index, skill_index,
                                      ((uint16_t)(creature_experience(info) * damage) >> 4) + MELEE_EXPERIENCE_BONUS);
        champion_decrement_stamina(champion_index, random_2bits() + MELEE_HIT_STAMINA);
    } else {
        damage = 0;
        champion_decrement_stamina(champion_index, random_bit() + 2);
    }
    if (outcome != OUTCOME_ALL_CREATURES_KILLED)
        group_process_reaction_event(at, REACTION_PARTY_IS_ADJACENT);
    return damage;
}

/* Adds the next event with the aspect update time, and the ticks that the event carried. */
static void add_event_with_aspect(GroupTurn *t)
{
    if (t->ticks_delay_next_event)
        t->next.map_time += t->ticks;
    else
        t->aspect_update_time += t->ticks;
    add_event(&t->next, t->aspect_update_time);
}

/* The next event is a group behavior event; the aspect of the whole group changes when no time is set. */
static void add_behavior_event(GroupTurn *t)
{
    t->next.type = EVENT_UPDATE_BEHAVIOR_GROUP;
    if (!t->aspect_update_time)
        t->aspect_update_time = next_aspect_time(t->active, -1, false);
    add_event_with_aspect(t);
}

/* The group acts again after about its movement ticks. */
static void add_wander_event(GroupTurn *t)
{
    t->next.map_time += max16(1, t->movement_ticks + random_2bits() - 1);
    add_behavior_event(t);
}

/* The group approaches the party it sees. */
static void set_behavior_approach(GroupTurn *t)
{
    rec_group_set_behavior(t->group, BEHAVIOR_APPROACH);
    set_target_to_party(t->active);
    t->next.map_time++;
    add_behavior_event(t);
}

/* The group attacks: each creature turns to the party and gets its own behavior event. */
static void set_behavior_attack(GroupTurn *t)
{
    if (t->event_type == REACTION_HIT_BY_PROJECTILE)
        delete_events(t->x, t->y);
    set_target_to_party(t->active);
    rec_group_set_behavior(t->group, BEHAVIOR_ATTACK);
    int16_t primary = t->directions_to_party.primary;
    bool turn_pair = t->last_creature_index && t->size == CREATURE_SIZE_HALF;
    for (int16_t i = t->last_creature_index; i >= 0; i--) {
        uint32_t map = t->next.map_time & ~(uint32_t)EVENT_TIME_MASK;
        if (group_cell(t->active->directions, i) == primary || (i && random_bit())) {
            t->next.map_time = map | (g_game_time + 1);
        } else {
            rotate_creature(t->active, primary, i, turn_pair);
            t->next.map_time = map | (g_game_time + random_2bits() + 2);
        }
        if (t->ticks_delay_next_event)
            t->next.map_time += min16(random_2bits() + (t->info->attack_ticks >> 1), t->ticks);
        t->next.type = EVENT_UPDATE_BEHAVIOR_CREATURE_0 + i;
        add_event(&t->next, next_aspect_time(t->active, i, false));
    }
}

/* The group moves one or two squares when its movement ticks have passed. Returns true when the move ended the turn
   (the group left the map, died, or changed its active entry). */
static bool try_move(GroupTurn *t, int16_t target_x, int16_t target_y)
{
    int16_t remaining = (t->movement_ticks >> 1) - t->ticks_since_last_move;
    t->moved = remaining < 1;
    if (!t->moved) {
        t->ticks_since_last_move = -1;
        t->movement_ticks = remaining;
        return false;
    }
    MoveDestination destination;
    if (move_get_move_result(
            &(Move){.thing = t->group, .from.x = t->x, .from.y = t->y, .to.x = target_x, .to.y = target_y},
            &destination) != MOVE_RESULT_DONE)
        return true;
    t->next.b = event_square(destination.x, destination.y);
    t->active->prior_x = t->x;
    t->active->prior_y = t->y;
    t->active->last_move_time = g_game_time;
    return false;
}

/* After a move or a failed move: the group turns, a reaction ends the attack, and the next event follows. */
static void rotate_and_finish(GroupTurn *t, int16_t direction)
{
    if (!t->moved && t->ticks_since_last_move >= 0)
        direction = random_2bits();
    rotate_group(t->active, direction, t->last_creature_index, t->size);
    if (t->event_type < REACTION_PARTY_IS_ADJACENT) {
        if (!t->moved)
            return;
        if (t->was_attacking)
            rec_group_set_behavior(t->group, BEHAVIOR_APPROACH);
        stop_attacking(t->active, t->x, t->y);
    }
    add_wander_event(t);
}

/* A group that did not move keeps its direction when it smells the party, except after danger. */
static void finish_movement(GroupTurn *t, int16_t direction)
{
    if (!t->moved) {
        bool keep_direction = random_2bits() != 0 && t->visible_distance > creature_smell_range(t->info);
        if (keep_direction || t->event_type == REACTION_DANGER_ON_SQUARE) {
            if (t->event_type < REACTION_PARTY_IS_ADJACENT && !t->moved)
                return;
            add_wander_event(t);
            return;
        }
    }
    rotate_and_finish(t, direction);
}

/* Mutual recursion: after_move and jump_archenemy call each other. */
static bool jump_archenemy(GroupTurn *t);

/* After a move attempt Lord Chaos may jump in a random direction; any other group finishes its movement. */
static bool after_move(GroupTurn *t, int16_t direction)
{
    if (t->moved || t->ticks_since_last_move == -1 || !t->archenemy ||
        (t->event_type != REACTION_DANGER_ON_SQUARE && random_2bits())) {
        finish_movement(t, direction);
        return false;
    }
    t->direction = random_2bits();
    t->secondary_direction = direction_right(t->direction);
    return jump_archenemy(t);
}

/* Lord Chaos jumps two squares over a free square, away from fluxcages. Returns true when the turn ended at once. */
static bool jump_archenemy(GroupTurn *t)
{
    for (;;) {
        get_first_free_direction(t, false);
        int16_t direction = t->direction;
        bool possible = can_archenemy_double_move(t, direction);
        if (!possible) {
            direction = t->secondary_direction;
            possible = can_archenemy_double_move(t, direction);
        }
        if (!possible && t->flux_cage_count) {
            direction = direction_back(t->secondary_direction);
            possible = can_archenemy_double_move(t, direction);
        }
        if (!possible && t->flux_cage_count >= 2) {
            direction = direction_back(t->direction);
            possible = can_archenemy_double_move(t, direction);
        }
        if (!possible) {
            rotate_group(t->active, t->direction, t->last_creature_index, t->size);
            add_wander_event(t);
            return false;
        }
        int16_t x = t->x;
        int16_t y = t->y;
        map_step(direction, 2, 0, &x, &y);
        sound_request_play(SOUND_BUZZ, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
        if (try_move(t, x, y))
            return true;
        if (t->moved || t->ticks_since_last_move == -1 ||
            (t->event_type != REACTION_DANGER_ON_SQUARE && random_2bits())) {
            finish_movement(t, direction);
            return false;
        }
        t->direction = random_2bits();
        t->secondary_direction = direction_right(t->direction);
    }
}

/* Moves one square in a direction that is possible. */
static bool step_in_direction(GroupTurn *t, int16_t direction)
{
    int16_t x = t->x;
    int16_t y = t->y;
    map_step(direction, 1, 0, &x, &y);
    if (try_move(t, x, y))
        return true;
    return after_move(t, direction);
}

/* Moves toward t->direction, else toward the second direction, then the other sides. */
static bool step_toward(GroupTurn *t, bool allow_imaginary)
{
    int16_t direction = t->direction;
    if (test_movement(t, direction, allow_imaginary).possible)
        return step_in_direction(t, direction);
    bool allow_second = allow_imaginary && random_bit();
    direction = t->secondary_direction;
    if (test_movement(t, direction, allow_second).possible)
        return step_in_direction(t, direction);
    direction = direction_back(direction);
    if (test_movement(t, direction, false).possible)
        return step_in_direction(t, direction);
    if (!random_2bits()) {
        direction = direction_back(t->direction);
        if (test_movement(t, direction, false).possible)
            return step_in_direction(t, direction);
    }
    if (t->archenemy)
        return jump_archenemy(t);
    rotate_group(t->active, t->direction, t->last_creature_index, t->size);
    add_wander_event(t);
    return false;
}

/* Wanders in a random direction, avoiding the square it came from. A party in the way starts an attack. */
static bool wander_step(GroupTurn *t)
{
    int16_t first = random_2bits();
    int16_t direction = first;
    do {
        int16_t x = t->x;
        int16_t y = t->y;
        map_step(direction, 1, 0, &x, &y);
        if ((x != t->active->prior_x || y != t->active->prior_y || !random_2bits()) &&
            test_movement(t, direction, false).possible) {
            if (try_move(t, x, y))
                return true;
            return after_move(t, direction);
        }
        if (last_blocked_by_party) {
            if (t->event_type != REACTION_DANGER_ON_SQUARE && t->info->attack_ticks != CREATURE_NO_ATTACK &&
                (rec_group_behavior(t->group) != BEHAVIOR_FLEE || get_first_free_direction(t, false) < 0 ||
                 random_bit())) {
                set_behavior_attack(t);
                return false;
            }
            set_target_to_party(t->active);
        }
        direction = direction_right(direction);
    } while (direction != first);
    return after_move(t, direction);
}

/* Moves toward the target square, or wanders when the group stands on it. */
static bool approach_target(GroupTurn *t)
{
    int16_t target_x = t->active->target_x;
    int16_t target_y = t->active->target_y;
    if (target_x == t->x && target_y == t->y) {
        t->moved = 0;
        rec_group_set_behavior(t->group, BEHAVIOR_WANDER);
        rotate_and_finish(t, 0);
        return false;
    }
    DirectionPair directions = group_get_directions_to((Location){t->x, t->y}, (Location){target_x, target_y});
    t->direction = directions.primary;
    t->secondary_direction = directions.secondary;
    return step_toward(t, true);
}

/* Moves toward the party at double speed. */
static bool approach_party(GroupTurn *t)
{
    t->movement_ticks = (t->movement_ticks + 1) >> 1;
    set_target_to_party(t->active);
    DirectionPair directions =
        group_get_directions_to((Location){t->x, t->y}, (Location){g_party_map_x, g_party_map_y});
    t->direction = directions.primary;
    t->secondary_direction = directions.secondary;
    return step_toward(t, true);
}

/* Runs away from the party (or the last target), sometimes home. Fleeing ends after a delay without sight. */
static bool flee(GroupTurn *t)
{
    t->visible_distance = visible_party_distance(t, t->group, -1, t->x, t->y);
    int16_t from_x, from_y;
    if (!t->visible_distance) {
        if (--t->active->delay_fleeing == 0) {
            t->moved = 0;
            rec_group_set_behavior(t->group, BEHAVIOR_WANDER);
            rotate_and_finish(t, 0);
            return false;
        }
        if (random_bit()) {
            if (get_first_free_direction(t, false) < 0 &&
                abs16(t->x - g_party_map_x) + abs16(t->y - g_party_map_y) < 2) {
                t->moved = 0;
                rec_group_set_behavior(t->group, BEHAVIOR_WANDER);
                rotate_and_finish(t, 0);
                return false;
            }
            DirectionPair directions =
                group_get_directions_to((Location){t->x, t->y}, (Location){t->active->home_x, t->active->home_y});
            t->direction = directions.primary;
            t->secondary_direction = directions.secondary;
            return step_toward(t, true);
        }
        from_x = t->active->target_x;
        from_y = t->active->target_y;
    } else {
        set_target_to_party(t->active);
        from_x = g_party_map_x;
        from_y = g_party_map_y;
    }
    DirectionPair directions = group_get_directions_to((Location){t->x, t->y}, (Location){from_x, from_y});
    t->direction = direction_back(directions.primary);
    t->secondary_direction = direction_back(directions.secondary);
    t->movement_ticks -= t->movement_ticks >> 2;
    return step_toward(t, true);
}

/* The party is in attack range, in line, and the creature can attack. */
static bool can_start_attack(const GroupTurn *t)
{
    return t->visible_distance <= creature_attack_range(t->info) && t->info->attack_ticks != CREATURE_NO_ATTACK &&
           (!t->distance_x || !t->distance_y);
}

/* EVENT_GROUP_REACTION_*: reactions to danger, a projectile hit, or the party next to the group. */
static void process_reaction(GroupTurn *t, uint16_t behavior)
{
    switch (t->event_type) {
    case EVENT_GROUP_REACTION_HIT_BY_PROJECTILE: {
        t->event_type = REACTION_HIT_BY_PROJECTILE;
        if (behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE)
            return;
        bool useless = behavior == BEHAVIOR_WANDER_NO_SMELL || behavior == BEHAVIOR_WANDER_NO_APPROACH;
        if (useless || random_2bits()) {
            if (!visible_party_distance(t, t->group, -1, t->x, t->y)) {
                rotate_and_finish(t, 0);
                return;
            }
            if (useless || random_2bits())
                return;
        }
        wander_step(t);
        return;
    }
    case EVENT_GROUP_REACTION_PARTY_IS_ADJACENT:
        t->event_type = REACTION_PARTY_IS_ADJACENT;
        if (t->info->attack_ticks == CREATURE_NO_ATTACK || behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE) {
            set_target_to_party(t->active);
            return;
        }
        delete_events(t->x, t->y);
        set_behavior_attack(t);
        return;
    default:
        t->event_type = REACTION_DANGER_ON_SQUARE;
        t->was_attacking = behavior == BEHAVIOR_ATTACK;
        wander_step(t);
        return;
    }
}

/* EVENT_UPDATE_ASPECT_*: the aspect of the group or of one creature changes; a group that sees the party may react. */
static void process_aspect_event(GroupTurn *t, uint16_t behavior)
{
    t->next.type = t->event_type + EVENT_BEHAVIOR_TO_ASPECT;
    if (visible_party_distance(t, t->group, -1, t->x, t->y)) {
        bool attacks_or_flees = behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE;
        if (!attacks_or_flees && t->info->attack_ticks != CREATURE_NO_ATTACK &&
            abs16(g_party_map_x - t->x) + abs16(g_party_map_y - t->y) <= 1) {
            set_behavior_attack(t);
            return;
        }
        if (!attacks_or_flees && (behavior == BEHAVIOR_WANDER || behavior == BEHAVIOR_WANDER_NO_SMELL)) {
            set_behavior_approach(t);
            return;
        }
        set_target_to_party(t->active);
    }
    if (behavior == BEHAVIOR_ATTACK) {
        /* For EVENT_UPDATE_ASPECT_GROUP (all creatures) the original reads the byte before aspect[0] (home y, always
           below 128). */
        int16_t creature = t->event_type - EVENT_UPDATE_ASPECT_CREATURE_0;
        t->aspect_update_time =
            next_aspect_time(t->active, creature, creature >= 0 && (t->active->aspect[creature] & ASPECT_ATTACKING));
        add_event_with_aspect(t);
        return;
    }
    if (t->distance_x > ASPECT_NEAR_DISTANCE || t->distance_y > ASPECT_NEAR_DISTANCE) {
        t->aspect_update_time = g_game_time + creature_ticks_non_attack_aspect(t->info);
        add_event_with_aspect(t);
        return;
    }
    if (!t->aspect_update_time)
        t->aspect_update_time = next_aspect_time(t->active, -1, false);
    add_event_with_aspect(t);
}

/* EVENT_UPDATE_BEHAVIOR_GROUP: the behavior of the whole group. */
static void process_group_behavior(GroupTurn *t, uint16_t behavior)
{
    switch (behavior) {
    case BEHAVIOR_WANDER:
    case BEHAVIOR_WANDER_NO_APPROACH:
    case BEHAVIOR_WANDER_NO_SMELL:
        t->visible_distance = visible_party_distance(t, t->group, -1, t->x, t->y);
        if (!t->visible_distance) {
            if (behavior != BEHAVIOR_WANDER) {
                add_wander_event(t);
                return;
            }
            int16_t direction = get_smelled_party_direction(t, t->info, t->x, t->y);
            if (direction >= 0) {
                t->direction = direction;
                step_toward(t, false);
            } else if (!random_bit()) {
                after_move(t, 0);
            } else {
                wander_step(t);
            }
            return;
        }
        if (can_start_attack(t))
            set_behavior_attack(t);
        else if (behavior != BEHAVIOR_WANDER_NO_APPROACH)
            set_behavior_approach(t);
        else
            add_wander_event(t);
        return;
    case BEHAVIOR_APPROACH:
        t->visible_distance = visible_party_distance(t, t->group, -1, t->x, t->y);
        if (!t->visible_distance)
            approach_target(t);
        else if (can_start_attack(t))
            set_behavior_attack(t);
        else
            approach_party(t);
        return;
    case BEHAVIOR_FLEE:
        flee(t);
        return;
    default:
        add_wander_event(t);
        return;
    }
}

/* A single small creature sometimes changes its cell; a projectile in the new cell may hit it.
   Returns true when all creatures died. */
static bool shift_single_creature(GroupTurn *t)
{
    uint16_t random = random16();
    if (t->last_creature_index || t->size == CREATURE_SIZE_FULL || (random & MOVE_CELL_RANDOM_BITS))
        return false;
    uint16_t cell = random;
    if (t->active->cells != CELL_CENTERED) {
        if (!(random & NEW_CELL_RANDOM_BITS))
            cell = NORMALIZE((random & 1 ? 1 : -1) + group_cell(t->active->cells, 0));
        else
            t->active->cells = CELL_CENTERED;
    }
    if ((cell & NEW_CELL_RANDOM_BITS) || t->visible_distance == 1 || t->size != CREATURE_SIZE_QUARTER)
        return false;
    ImpactCount impact = projectile_get_impact_count(IMPACT_CREATURE, t->x, t->y, t->active->cells);
    if (impact.count && impact.outcome == OUTCOME_ALL_CREATURES_KILLED)
        return true;
    t->active->cells = NORMALIZE(cell);
    return false;
}

/* A creature steps to another cell of the square, toward the front. Returns true when all creatures died. */
static bool bring_to_front(GroupTurn *t, int16_t creature)
{
    uint16_t position = t->active->cells >> (GROUP_CELL_SHIFT * creature);
    uint16_t cell = NORMALIZE(position);
    if (!t->last_creature_index && random_bit()) {
        t->active->cells = CELL_CENTERED;
        return false;
    }
    cell = (position & 1) == (t->direction & 1) ? direction_left(cell) : direction_right(cell);
    if (group_get_creature_in_cell(t->group, cell) >= 0) {
        if (!random_bit())
            return false;
        cell = direction_back(cell);
        if (group_get_creature_in_cell(t->group, cell) >= 0)
            return false;
    }
    ImpactCount impact = projectile_get_impact_count(IMPACT_CREATURE, t->x, t->y, t->active->cells);
    if (impact.count && impact.outcome == OUTCOME_ALL_CREATURES_KILLED)
        return true;
    if (impact.outcome != OUTCOME_SOME_CREATURES_KILLED)
        t->active->cells = set_cell_value(t->active->cells, creature, cell);
    return false;
}

/* A creature that attacked waits for its attack ticks before its next behavior event. */
static void delay_after_attack(GroupTurn *t, int16_t creature)
{
    t->aspect_update_time = next_aspect_time(t->active, creature, false);
    /* 16-bit values as in the original: -1 and -2 add 65535 and 65534 ticks */
    t->next.map_time += (uint16_t)(t->info->attack_ticks + random_2bits() - 1);
    if (t->info->attack_ticks > SLOW_ATTACK_TICKS)
        t->next.map_time += (uint16_t)((random16() & 7) - 2);
    t->next.type = t->event_type;
    add_event_with_aspect(t);
}

/* A creature attacks at once from range, from the back row (when it prefers it and attacks any champion, 3 in 4),
   when it is not quarter size, or when it stands centered. Else only from a cell on the side of the party. */
static bool can_attack_from_cell(const GroupTurn *t, int16_t creature, uint16_t range)
{
    uint16_t attributes = t->info->attributes;
    if (range != 1 ||
        ((attributes & CREATURE_PREFER_BACK_ROW) && random_2bits() && (attributes & CREATURE_ATTACK_ANY_CHAMPION)) ||
        t->size != CREATURE_SIZE_QUARTER || t->active->cells == CELL_CENTERED)
        return true;
    uint16_t cell = group_cell(t->active->cells, creature);
    return cell == t->direction || cell == direction_right(t->direction);
}

/* EVENT_UPDATE_BEHAVIOR_CREATURE_0-3: the behavior of one creature of an attacking group: turn, step to the front,
   or attack. */
static void process_creature_behavior(GroupTurn *t)
{
    int16_t creature = t->event_type - EVENT_UPDATE_BEHAVIOR_CREATURE_0;
    ActiveGroup *active = t->active;
    if (active->aspect[creature] & ASPECT_ATTACKING) {
        delay_after_attack(t, creature);
        return;
    }
    if (t->last_creature_index < creature)
        return;
    t->direction = t->directions_to_party.primary;
    t->visible_distance = visible_party_distance(t, t->group, creature, t->x, t->y);
    if (t->visible_distance)
        set_target_to_party(active);
    if (shift_single_creature(t))
        return;
    if (!t->visible_distance ||
        (!(t->info->attributes & CREATURE_SIDE_ATTACK) && group_cell(active->directions, creature) != t->direction)) {
        if (!visible_party_distance(t, t->group, -1, t->x, t->y)) {
            rec_group_set_behavior(t->group, BEHAVIOR_APPROACH);
            if (t->last_creature_index)
                stop_attacking(active, t->x, t->y);
            approach_target(t);
            return;
        }
        set_target_to_party(active);
        rotate_creature(active, t->direction, creature, t->last_creature_index && t->size == CREATURE_SIZE_HALF);
        t->next.map_time += 2;
        t->aspect_update_time = t->next.map_time & EVENT_TIME_MASK;
        t->next.type = t->event_type;
        add_event_with_aspect(t);
        return;
    }
    uint16_t range = creature_attack_range(t->info);
    if (range < t->visible_distance || (t->distance_x && t->distance_y && (range < 2 || (random16() & 7))) ||
        (random16() & ATTACK_RANGE_RANDOM_MASK) + 1 < range) {
        rec_group_set_behavior(t->group, BEHAVIOR_APPROACH);
        if (t->last_creature_index)
            stop_attacking(active, t->x, t->y);
        approach_party(t);
        return;
    }
    if (can_attack_from_cell(t, creature, range)) {
        bool attacking = is_creature_attacking(t, t->group, t->x, t->y, creature);
        t->aspect_update_time = next_aspect_time(active, creature, attacking);
        t->next.map_time += random_bit() + creature_ticks_after_attack(t->info);
        t->next.type = t->event_type;
        add_event_with_aspect(t);
        return;
    }
    if (bring_to_front(t, creature))
        return;
    t->next.map_time += max16(1, random_bit() + (t->info->movement_ticks >> 1));
    t->next.type = t->event_type;
    if (!t->aspect_update_time)
        t->aspect_update_time = next_aspect_time(active, -1, false);
    add_event_with_aspect(t);
}

/* Off the party map a group only wanders: one random step, then the next group behavior event after a delay that
   grows with the map distance to the party. */
static void wander_off_party_map(GroupTurn *t)
{
    int16_t direction = random_2bits();
    if (test_movement(t, direction, false).possible) {
        int16_t target_x = t->x;
        int16_t target_y = t->y;
        map_step(direction, 1, 0, &target_x, &target_y);
        MoveDestination destination;
        if (move_get_move_result(
                &(Move){.thing = t->group, .from.x = t->x, .from.y = t->y, .to.x = target_x, .to.y = target_y},
                &destination) != MOVE_RESULT_DONE)
            return;
        t->next.b = event_square(destination.x, destination.y);
    }
    t->next.type = EVENT_UPDATE_BEHAVIOR_GROUP;
    t->next.map_time += max16(abs16(g_current_map_index - g_party_map_index) << 4, t->info->movement_ticks * 2);
    timeline_add_event(&t->next);
}

/* Freeze Life delays a group event by a few ticks. A reaction is lost. */
static void postpone_for_freeze_life(GroupTurn *t)
{
    if (t->event_type < 0)
        return;
    t->next.type = t->event_type;
    t->next.c = event_group_c(t->ticks);
    t->next.map_time += FREEZE_LIFE_RETRY_TICKS;
    timeline_add_event(&t->next);
}

/* A reaction becomes an EVENT_GROUP_REACTION_* event: next tick when the party is adjacent, else when the group can
   move again. */
static void add_reaction_event(GroupTurn *t)
{
    /* Bug fix: the original leaves byte 8 (the ticks) of this event uninitialized; the port sets 0. */
    t->next.type = t->event_type + EVENT_UPDATE_ASPECT_GROUP;
    int16_t delay = ((t->movement_ticks + 2) >> 2) - t->ticks_since_last_move;
    if (t->event_type == REACTION_PARTY_IS_ADJACENT || delay < 1)
        delay = 1;
    t->next.map_time += delay;
    timeline_add_event(&t->next);
}

/* The group of the event becomes the current group: its square, thing, size, and distance and directions to the party. */
static void set_current_group(GroupTurn *t)
{
    t->last_creature_index = rec_group_last_creature(t->group);
    t->size = creature_size(t->info);
    t->distance_x = abs16(t->x - g_party_map_x);
    t->distance_y = abs16(t->y - g_party_map_y);
    last_turn.at = (Location){t->x, t->y};
    last_turn.thing = t->group;
    t->distance_to_party = group_get_square_distance(t->x, t->y, g_party_map_x, g_party_map_y);
    t->directions_to_party = group_get_directions_to((Location){t->x, t->y}, (Location){g_party_map_x, g_party_map_y});
    t->secondary_direction = t->directions_to_party.secondary;
}

/* EVENT_UPDATE_BEHAVIOR_*: the behavior of the group or of one creature. A fleeing group flees as a whole. Event
   ticks make the aspect update now. */
static void process_behavior_event(GroupTurn *t, uint16_t behavior)
{
    t->ticks_delay_next_event = 0;
    if (t->ticks)
        t->aspect_update_time = g_game_time;
    if (t->event_type == EVENT_UPDATE_BEHAVIOR_GROUP) {
        process_group_behavior(t, behavior);
        return;
    }
    if (behavior == BEHAVIOR_FLEE) {
        if (t->last_creature_index)
            stop_attacking(t->active, t->x, t->y);
        flee(t);
        return;
    }
    process_creature_behavior(t);
}

/* Runs the turn of the group on a square for a reaction or a group event. */
static void process_turn(GroupTurn *t)
{
    int16_t x = t->x;
    int16_t y = t->y;
    int16_t event_type = t->event_type;
    uint16_t group = t->group;
    t->next = (TimelineEvent){
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time),
        .priority = GROUP_EVENT_PRIORITY_MAX - t->info->movement_ticks,
        .b = event_square(x, y),
    };
    if (g_current_map_index != g_party_map_index) {
        wander_off_party_map(t);
        return;
    }
    t->archenemy = (t->info->attributes & CREATURE_ARCHENEMY) != 0;
    if (t->archenemy && g_session.won)
        return;
    t->active = active_of(group);
    t->ticks_since_last_move = (uint8_t)g_game_time - t->active->last_move_time;
    if (t->ticks_since_last_move < 0)
        t->ticks_since_last_move += MOVE_TIME_WRAP;
    t->movement_ticks =
        t->info->movement_ticks == CREATURE_IMMOBILE ? MOVEMENT_TICKS_OF_IMMOBILE_CREATURE : t->info->movement_ticks;
    if (g_party.freeze_life_ticks && !t->archenemy) {
        postpone_for_freeze_life(t);
        return;
    }
    if (event_type < 0) {
        add_reaction_event(t);
        return;
    }
    uint16_t behavior = rec_group_behavior(group);
    set_current_group(t);
    t->ticks_delay_next_event = 1;
    if (event_type < EVENT_UPDATE_ASPECT_GROUP) {
        process_reaction(t, behavior);
        return;
    }
    if (event_type < EVENT_UPDATE_BEHAVIOR_GROUP) {
        process_aspect_event(t, behavior);
        return;
    }
    process_behavior_event(t, behavior);
}

/* Starts the turn of the group on a square, unless the event type does not run off the party map. */
static void process_turn_at(Location at, int16_t event_type, uint16_t ticks)
{
    if (g_current_map_index != g_party_map_index && event_type != EVENT_UPDATE_BEHAVIOR_GROUP &&
        event_type != EVENT_UPDATE_ASPECT_GROUP && event_type != EVENT_UPDATE_BEHAVIOR_CREATURE_0 &&
        event_type != EVENT_UPDATE_ASPECT_CREATURE_0)
        return;
    uint16_t group = map_find_thing(at, THING_GROUP);
    if (group == THING_ENDOFLIST)
        return;
    GroupTurn turn = {
        .x = at.x, .y = at.y, .event_type = event_type, .ticks = ticks, .group = group, .info = group_info(group)};
    process_turn(&turn);
}

void group_process_reaction_event(Location at, int16_t reaction)
{
    process_turn_at(at, reaction, 0);
}

void group_process_event(const TimelineEvent *event)
{
    process_turn_at((Location){event_square_x(event), event_square_y(event)}, event->type, event_group_ticks(event));
}

int16_t group_turn_by_teleporter(uint16_t teleporter, uint16_t group, int16_t map)
{
    uint16_t rotation = rec_teleporter_rotation(teleporter);
    int16_t absolute = rec_teleporter_is_absolute(teleporter);
    uint16_t old_directions = get_directions(group, map);
    uint16_t directions = absolute ? rotation : NORMALIZE(old_directions + rotation);
    uint16_t cells = get_cells(group, map);
    if (cells != CELL_CENTERED) {
        bool quarter = creature_size(group_info(group)) == CREATURE_SIZE_QUARTER;
        uint16_t cell_turn = NORMALIZE(directions + 4 - old_directions);
        uint16_t old_cells = cells;
        for (int16_t i = 0; i <= rec_group_last_creature(group); i++) {
            directions = set_cell_value(directions, i, absolute ? rotation : NORMALIZE(old_directions + rotation));
            if (quarter)
                cell_turn = absolute ? 0 : rotation;
            if (cell_turn)
                cells = set_cell_value(cells, i, NORMALIZE(cell_turn + old_cells));
            old_directions >>= GROUP_CELL_SHIFT;
            old_cells >>= GROUP_CELL_SHIFT;
        }
    }
    group_set_directions(group, directions, map);
    group_set_cells(group, cells, map);
    if (map == g_party_map_index && rec_group_behavior(group) == BEHAVIOR_ATTACK)
        return rec_group_cells(group);
    return -1;
}

/* Creatures that died during a move drop their fixed possessions on the destination square. */
static void drop_moving_possessions(uint16_t group, int16_t x, int16_t y, GroupFallDrops *drops)
{
    uint16_t type = rec_group_creature_type(group);
    while (drops->count) {
        drops->count--;
        drop_fixed_possessions(type, x, y, drops->cells[drops->count], SOUND_MODE_EVENT);
    }
}

bool group_is_killed_by_fall(uint16_t group, Position source, Position landing, GroupFallDrops *drops)
{
    map_set_current_map(source.map);
    drops->count = 0;
    int16_t outcome = damage_all_creatures(group, (Location){source.x, source.y}, FALL_DAMAGE, drops);
    map_set_current_map(landing.map);
    if (outcome == OUTCOME_ALL_CREATURES_KILLED)
        return true;
    if (outcome == OUTCOME_SOME_CREATURES_KILLED)
        drop_moving_possessions(group, landing.x, landing.y, drops);
    return false;
}

void group_remove_after_move(uint16_t group, Position source, Position landing, GroupFallDrops *drops)
{
    drop_moving_possessions(group, landing.x, landing.y, drops);
    group_drop_group_possessions(landing.x, landing.y, group, SOUND_MODE_EVENT);
    map_set_current_map(source.map);
    if (source.x >= 0)
        group_delete(source.x, source.y);
}

/* The group waits off the map and tries to enter the square 5 ticks later (EVENT_MOVE_GROUP_AUDIBLE plays the
   teleporter sound). */
static void postpone_entry(uint16_t group, int16_t x, int16_t y, int16_t map, bool audible)
{
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(map, g_game_time + MOVE_GROUP_RETRY_TICKS),
        .type = audible ? EVENT_MOVE_GROUP_AUDIBLE : EVENT_MOVE_GROUP_SILENT,
        .b = event_square(x, y),
        .c = group,
    };
    timeline_add_event(&event);
}

MoveResult group_enter_square(const GroupArrival *arrival)
{
    MoveResult result = arrival->result;
    map_set_current_map(arrival->landing.map);
    uint8_t active_index = rec_group_cells(arrival->group);
    Location at = {arrival->landing.x, arrival->landing.y};
    bool levitating = move_is_levitating(arrival->group);
    if (map_is_party_on(at) || map_find_thing(at, THING_GROUP) != THING_ENDOFLIST) {
        map_set_current_map(arrival->source.map);
        if (arrival->source.x >= 0)
            delete_events(arrival->source.x, arrival->source.y);
        if (arrival->removed_from_party_map)
            remove_active_group(active_index);
        postpone_entry(arrival->group, at.x, at.y, arrival->landing.map, arrival->audible);
        return MOVE_RESULT_STOPPED;
    }
    sound_request_play(get_creature_movement_sound(rec_group_creature_type(arrival->group)), at, SOUND_MODE_AFTER_TICK,
                       0);
    if (arrival->removed_from_party_map && arrival->landing.map != g_party_map_index) {
        remove_active_group(active_index);
        result = MOVE_RESULT_STOPPED;
    } else if (!arrival->removed_from_party_map && arrival->landing.map == g_party_map_index) {
        group_add_active_group(arrival->group, at.x, at.y);
        result = MOVE_RESULT_STOPPED;
    }
    if (!levitating)
        sensor_process_thing_change(at.x, at.y, arrival->group, false, true);
    else
        map_link_thing_to_list(arrival->group, 0, at.x, at.y);
    if (result != MOVE_RESULT_DONE || arrival->source.x < 0)
        group_start_wandering(at.x, at.y);
    map_set_current_map(arrival->source.map);
    if (arrival->source.x >= 0) {
        if (result == MOVE_RESULT_TURNED)
            stop_attacking(&g_active_groups[arrival->turned_active], arrival->source.x, arrival->source.y);
        else if (result == MOVE_RESULT_STOPPED)
            delete_events(arrival->source.x, arrival->source.y);
    }
    return result;
}

void group_process_move_event(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    int16_t retried = 0;
    for (;;) {
        if (!map_is_party_on((Location){x, y}) && map_find_thing((Location){x, y}, THING_GROUP) == THING_ENDOFLIST) {
            if (event->type == EVENT_MOVE_GROUP_AUDIBLE)
                sound_request_play(SOUND_BUZZ, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
            move_get_move_result(&(Move){.thing = event->c, .to.x = x, .to.y = y, .enters = true}, NULL);
            return;
        }
        if (retried)
            break;
        retried = 1;
        if (rec_group_creature_type(event->c) != CREATURE_LORD_CHAOS || random_2bits())
            break;
        switch (random_2bits()) {
        case 0:
            x--;
            break;
        case 1:
            x++;
            break;
        case 2:
            y--;
            break;
        default:
            y++;
            break;
        }
        if (!map_is_square_open(x, y))
            break;
    }
    event->map_time += MOVE_GROUP_RETRY_TICKS;
    timeline_add_event(event);
}
