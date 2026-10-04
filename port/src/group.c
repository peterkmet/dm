#include "group.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "inventory.h"
#include "move.h"
#include "projectile.h"
#include "random.h"
#include "sensor.h"
#include "sound.h"
#include "timeline_process.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

/* KAOS data addresses. */
#define KAOS_ORDERED_CELLS_TO_ATTACK 0x100b66      /* uint8_t[8][4] */
#define KAOS_WOUND_PROBABILITY_INDEX_TO_MASK 0x100b86 /* uint8_t[5] */
#define KAOS_SLOTS_TO_STEAL_FROM 0x100b8a          /* uint8_t[8] */

/* Creature graphic info (CreatureInfo.graphic_info). */
enum { GRAPHIC_FLIP_NON_ATTACK = 0x0004, GRAPHIC_FLIP_ATTACK = 0x0200, GRAPHIC_FLIP_DURING_ATTACK = 0x0400 };
#define GRAPHIC_HORIZONTAL_OFFSET_RANGE(info) ((info) >> 12 & 3)
#define GRAPHIC_VERTICAL_OFFSET_RANGE(info) ((info) >> 14)
enum { ASPECT_OFFSET_MASK = 7, ASPECT_VERTICAL_OFFSET_SHIFT = 3, ASPECT_KEEP_FLAGS = ASPECT_ATTACKING | ASPECT_FLIPPED };

/* Object info index ranges of the fixed possessions (see get_object_info_index). */
enum { OBJECT_INFO_FIRST_WEAPON = 0x17, OBJECT_INFO_FIRST_ARMOUR = 0x45, OBJECT_INFO_FIRST_JUNK = 0x7f };
enum { FIXED_POSSESSION_RANDOM_DROP = 0x8000, FIXED_POSSESSION_INDEX_MASK = 0x7fff };

enum { SYSTEM_ERROR_NO_FREE_ACTIVE_GROUP = 71, ACTIVE_GROUP_RESERVE = 5, THING_INDEX_MASK = 0x3ff };
enum { MOVEMENT_TICKS_OF_IMMOBILE_CREATURE = 100, FREEZE_LIFE_RETRY_TICKS = 4, MOVE_GROUP_RETRY_TICKS = 5 };
enum { WARINESS_AVOIDS_FORBIDDEN_TELEPORTER = 10, FALL_DAMAGE = 20, DOOR_DAMAGE = 5 };
enum { SKILL_PARRY_DEFENSE = 7, INVISIBLE_PARTY_DODGE_BONUS = 16, LUCK_PERCENT_AGAINST_CREATURE = 60, LUCK_PERCENT_AGAINST_MISS = 75 };
enum { ICON_DIAMOND_EDGE = 39, ICON_HALF_DAMAGE_ON_MATERIAL = 40, ICON_VORPAL_BLADE = 43 };
enum { SLOT_ACTION_HAND = 1 };
enum { STEAL_CHANCE_STEP = 20 };
enum { HIT_PROBABILITY_ALWAYS_HITS = 0x8000, HIT_PROBABILITY_MASK = 0x7fff };
enum { CELLS_OF_THREE_CREATURES = 0x3f };
enum { SMOKE_ATTACK_QUARTER = 110, SMOKE_ATTACK_HALF = 190, SMOKE_ATTACK_FULL = 255 };

int16_t g_creature_damage_outcome;
int16_t g_group_secondary_direction;
int16_t g_current_group_map_x, g_current_group_map_y;
uint16_t g_current_group_thing, g_current_group_distance_to_party;
int16_t g_current_group_primary_direction_to_party, g_current_group_secondary_direction_to_party;
int16_t g_dungeon_view_palette_index;

static uint8_t movement_tested_directions[4];
static uint8_t flux_cages[4];
static int16_t flux_cage_count;
static int16_t movement_blocked_by_wall, movement_blocked_by_door, movement_blocked_by_party;
static uint16_t movement_blocked_by_group;
static int16_t drop_moving_creature_possessions_cell_count;
static uint8_t drop_moving_creature_possessions_cells[GROUP_MAX_CREATURES];
static uint32_t set_direction_time;
static const ActiveGroup *set_direction_group;

static uint16_t group_thing_of_active(const ActiveGroup *active) { return (uint16_t)(THING_GROUP << 10 | active->group_index); }
static uint16_t cell_of(uint16_t packed, int16_t creature) { return packed >> (2 * creature) & 3; }
static int16_t abs16(int16_t value) { return value < 0 ? (int16_t)-value : value; }
static int16_t is_party_on(int16_t x, int16_t y) { return g_current_map_index == g_party_map_index && x == g_party_map_x && y == g_party_map_y; }
static void set_target_to_party(ActiveGroup *active) { active->target_x = (uint8_t)g_party_map_x; active->target_y = (uint8_t)g_party_map_y; }

static void step(int16_t direction, int16_t count, int16_t *x, int16_t *y)
{
    update_map_coordinates_after_relative_movement(direction, count, 0, x, y);
}

/* The movement sound of a creature type, or -1 while the party sleeps (move_get_creature_sound). */
static int16_t creature_movement_sound(uint16_t creature_type)
{
    uint8_t ordinal = tables_creature_info(creature_type)->attack_sound_ordinal;
    if (g_party_is_sleeping || !ordinal)
        return -1;
    return tables_creature_sound(ordinal - 1)->movement;
}

uint8_t get_group_cells(uint16_t group, int16_t map)
{
    return map == g_party_map_index ? group_active(group)->cells : group_cells_byte(group);
}

void set_group_cells(uint16_t group, uint8_t cells, int16_t map)
{
    if (map == g_party_map_index)
        group_active(group)->cells = cells;
    else
        group_set_cells_byte(group, cells);
}

/* Off the party map all creatures of a group share the direction of the record. */
uint8_t get_group_directions(uint16_t group, int16_t map)
{
    if (map == g_party_map_index)
        return group_active(group)->directions;
    uint16_t direction = group_direction(group);
    return (uint8_t)(direction | direction << 2 | direction << 4 | direction << 6);
}

void set_group_directions(uint16_t group, uint8_t directions, int16_t map)
{
    if (map == g_party_map_index)
        group_active(group)->directions = directions;
    else
        group_set_direction_field(group, directions);
}

/* The creature type list of a map follows its last square. */
int16_t is_creature_allowed_on_map(uint16_t group, int16_t map)
{
    const uint8_t *types = &g_dungeon_map_data[map][map_last_x(map)][map_last_y(map) + 1];
    for (int16_t n = MAP_DESCRIPTOR(map)[13] >> 4; n > 0; n--, types++)
        if (group_creature_type(group) == *types)
            return 1;
    return 0;
}

/* Ordinal (index + 1) of the creature in a cell, or 0. Half-square creatures fill two cells side by side. */
int16_t group_get_creature_ordinal_in_cell(uint16_t group, uint16_t cell)
{
    uint8_t cells = get_group_cells(group, g_current_map_index);
    if (cells == CELLS_CENTERED)
        return 1;
    int16_t half_square = creature_size(group_info(group)) == CREATURE_SIZE_HALF;
    if (half_square && (cell & 1) == (get_group_directions(group, g_current_map_index) & 1))
        cell = (cell - 1) & 3;
    for (int16_t i = group_last_creature_index(group); i >= 0; i--) {
        uint16_t creature_cell = cell_of(cells, i);
        if (cell == creature_cell || (half_square && ((cell + 1) & 3) == creature_cell))
            return i + 1;
    }
    return 0;
}

/* The creature that a champion in a cell hits first. */
int16_t group_get_melee_target_creature_ordinal(int16_t group_x, int16_t group_y, int16_t party_x, int16_t party_y, int16_t champion_cell)
{
    uint16_t group = group_get_thing(group_x, group_y);
    if (group == THING_ENDOFLIST)
        return 0;
    uint8_t cells[4];
    group_set_ordered_cells_to_attack(cells, group_x, group_y, party_x, party_y, champion_cell);
    for (int16_t i = 0; i < 4; i++) {
        int16_t ordinal = group_get_creature_ordinal_in_cell(group, cells[i]);
        if (ordinal)
            return ordinal;
    }
    return 0;
}

uint16_t group_set_cell_value(uint16_t packed, uint16_t creature_index, uint16_t value)
{
    uint16_t shift = (uint16_t)(2 * creature_index);
    return (uint16_t)((packed & ~(3 << shift)) | (value & 3) << shift);
}

/* A random offset of the creature graphic in a range, mirrored half of the time. */
static uint8_t random_aspect_offset(uint16_t range)
{
    uint8_t offset = (uint8_t)(random16() % range);
    if (random_bit())
        offset = (uint8_t)(-offset & ASPECT_OFFSET_MASK);
    return offset;
}

/* New graphic offsets and flip state of one creature (or all with creature_index -1). Returns the next aspect time. */
uint32_t group_get_creature_aspect_update_time(ActiveGroup *active, int16_t creature_index, int16_t attacking)
{
    uint16_t group = group_thing_of_active(active), type = group_creature_type(group);
    uint16_t graphic = tables_creature_info(type)->graphic_info;
    int16_t i = creature_index < 0 ? group_last_creature_index(group) : creature_index;
    for (;;) {
        uint8_t aspect = active->aspect[i] & ASPECT_KEEP_FLAGS;
        if (GRAPHIC_HORIZONTAL_OFFSET_RANGE(graphic))
            aspect |= random_aspect_offset(GRAPHIC_HORIZONTAL_OFFSET_RANGE(graphic));
        if (GRAPHIC_VERTICAL_OFFSET_RANGE(graphic))
            aspect |= (uint8_t)(random_aspect_offset(GRAPHIC_VERTICAL_OFFSET_RANGE(graphic)) << ASPECT_VERTICAL_OFFSET_SHIFT);
        if (!attacking) {
            if (!(graphic & GRAPHIC_FLIP_NON_ATTACK)) {
                aspect &= ~ASPECT_FLIPPED;
            } else if (type == CREATURE_COUATL) {
                if (random_bit()) {
                    aspect ^= ASPECT_FLIPPED;
                    sound_request_play(creature_movement_sound(CREATURE_COUATL), g_current_group_map_x, g_current_group_map_y, SOUND_MODE_AFTER_TICK);
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
                    sound_request_play(SOUND_SWING, g_current_group_map_x, g_current_group_map_y, SOUND_MODE_AFTER_TICK);
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
    const CreatureInfo *info = tables_creature_info(type);
    uint16_t ticks = attacking ? creature_ticks_attack_aspect(info) : creature_ticks_non_attack_aspect(info);
    return g_game_time + random_bit() + ticks;
}

/* A new group behavior event (37) after one tick. Faster creatures get a higher event priority. */
void group_start_wandering(int16_t x, int16_t y)
{
    uint16_t group = group_get_thing(x, y);
    if (group_behavior(group) > BEHAVIOR_USELESS_3)
        group_set_behavior(group, BEHAVIOR_WANDER);
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + 1), .type = EVENT_UPDATE_BEHAVIOR_GROUP,
        .priority = (uint8_t)(255 - group_info(group)->movement_ticks), .b = EVENT_SQUARE(x, y),
    };
    timeline_add_event(&event);
}

void group_delete_events(int16_t x, int16_t y)
{
    for (uint16_t i = 0; i < g_event_high_water; i++) {
        TimelineEvent *event = &g_timeline_events[i];
        if (EVENT_MAP(event) == g_current_map_index && event->type >= EVENT_GROUP_REACTION_DANGER_ON_SQUARE &&
            event->type <= EVENT_UPDATE_BEHAVIOR_CREATURE_3 && EVENT_X(event) == x && EVENT_Y(event) == y)
            timeline_delete_event(i);
    }
}

void group_stop_attacking(ActiveGroup *active, int16_t x, int16_t y)
{
    for (int16_t i = 0; i < GROUP_MAX_CREATURES; i++)
        active->aspect[i] &= ~ASPECT_ATTACKING;
    group_delete_events(x, y);
}

/* A group on the party map gets an active group entry. The record keeps the entry index in its cells byte. */
void group_add_active_group(uint16_t group, int16_t x, int16_t y)
{
    uint16_t index = 0;
    while (g_active_groups[index].group_index >= 0) {
        if (++index >= g_max_active_group_count) {
            fprintf(stderr, "System Error %d\n", SYSTEM_ERROR_NO_FREE_ACTIVE_GROUP);
            exit(1);
        }
    }
    ActiveGroup *active = &g_active_groups[index];
    g_active_group_count++;
    active->group_index = (int16_t)(group & THING_INDEX_MASK);
    active->cells = group_cells_byte(group);
    group_set_cells_byte(group, (uint8_t)index);
    active->home_x = active->prior_x = (uint8_t)x;
    active->home_y = active->prior_y = (uint8_t)y;
    active->last_move_time = (uint8_t)(g_game_time - 127);
    for (int16_t i = group_last_creature_index(group); i >= 0; i--) {
        active->directions = (uint8_t)group_set_cell_value(active->directions, i, group_direction(group));
        active->aspect[i] = 0;
    }
    group_get_creature_aspect_update_time(active, -1, 0);
}

/* The record gets back its cells and one direction. A fleeing or attacking group wanders again. */
void group_remove_active_group(uint16_t active_index)
{
    /* The original also accepts active_index == g_max_active_group_count (one past the table). */
    if (active_index >= g_max_active_group_count || g_active_groups[active_index].group_index < 0)
        return;
    ActiveGroup *active = &g_active_groups[active_index];
    uint16_t group = group_thing_of_active(active);
    g_active_group_count--;
    group_set_cells_byte(group, active->cells);
    group_set_direction_field(group, active->directions);
    if (group_behavior(group) > BEHAVIOR_USELESS_3)
        group_set_behavior(group, BEHAVIOR_WANDER);
    active->group_index = -1;
}

void group_remove_all_active_groups(void)
{
    for (uint16_t i = 0; g_active_group_count; i++)
        if (g_active_groups[i].group_index >= 0)
            group_remove_active_group(i);
}

/* Each group of the new party map becomes active and wanders. */
void group_add_all_active_groups(void)
{
    for (int16_t x = 0; x < g_current_map_width; x++) {
        for (int16_t y = 0; y < g_current_map_height; y++) {
            if (!(g_current_map_data[x][y] & SQUARE_HAS_THINGS))
                continue;
            uint16_t group = group_get_thing(x, y);
            if (group == THING_ENDOFLIST)
                continue;
            group_delete_events(x, y);
            group_add_active_group(group, x, y);
            group_start_wandering(x, y);
        }
    }
}

/* A new group of creatures on a square. Health is base health * multiplier plus a random part. */
uint16_t group_get_generated(uint16_t creature_type, int16_t health_multiplier, uint16_t last_creature_index, int16_t direction, int16_t x, int16_t y)
{
    if ((uint16_t)(g_max_active_group_count - ACTIVE_GROUP_RESERVE) <= g_active_group_count && g_current_map_index == g_party_map_index)
        return THING_NONE;
    uint16_t group = get_unused_thing(THING_GROUP);
    if (group == THING_NONE)
        return THING_NONE;
    group_set_possessions(group, THING_ENDOFLIST);
    group_clear_do_not_discard(group);
    group_set_direction_field(group, (uint16_t)direction);
    group_set_last_creature_index(group, last_creature_index);
    /* Bug fix: the original starts a group of several creatures from a stale register; the port starts from 0. */
    uint16_t cells = 0, cell = 0;
    if (last_creature_index == 0)
        cells = CELLS_CENTERED;
    else
        cell = random_2bits();
    group_set_creature_type(group, creature_type);
    const CreatureInfo *info = tables_creature_info(creature_type);
    for (int16_t i = (int16_t)last_creature_index; i >= 0; i--) {
        uint16_t random = random16();
        group_set_health(group, i, (uint16_t)(info->base_health * health_multiplier + random % ((info->base_health >> 2) + 1)));
        if (last_creature_index) {
            cells = group_set_cell_value(cells, i, cell);
            cell = (cell + (creature_size(info) == CREATURE_SIZE_HALF ? 2 : 1)) & 3;
        }
    }
    group_set_cells_byte(group, (uint8_t)cells);
    if (move_get_move_result(group, -1, 0, x, y))
        return THING_NONE;
    sound_request_play(SOUND_BUZZ, x, y, SOUND_MODE_AFTER_TICK);
    return group;
}

/* Group generator sensor (word 4 bits 7-10: creature count, word 6 bits 4-7: health multiplier). */
static uint16_t generator_count_field(uint16_t sensor) { return BE16(REC(sensor) + 4) >> 7 & 15; }
static uint16_t generator_health_multiplier(uint16_t sensor) { return BE16(REC(sensor) + 6) >> 4 & 15; }
enum { GENERATOR_RANDOM_COUNT = 8, GENERATOR_COUNT_MASK = 7 };

/* The creature generator of a corridor event: count, health multiplier (0: map difficulty), random direction. */
void group_generate_from_sensor(uint16_t sensor, int16_t x, int16_t y)
{
    uint16_t field = generator_count_field(sensor), last_creature_index;
    if (field & GENERATOR_RANDOM_COUNT)
        last_creature_index = random16() % (field & GENERATOR_COUNT_MASK);
    else
        last_creature_index = (field & 15) - 1;
    int16_t multiplier = (int16_t)generator_health_multiplier(sensor);
    if (!multiplier)
        multiplier = (int16_t)current_map_difficulty();
    int16_t direction = random_2bits();
    group_get_generated(sensor_data(sensor) & 0xff, multiplier, last_creature_index, direction, x, y);
}

/* First word of the fixed possession list of each creature type, or -1 (the switch of the original). */
static int16_t fixed_possession_list_start(uint16_t creature_type)
{
    switch (creature_type) {
    case CREATURE_SKELETON: return 0;
    case CREATURE_STONE_GOLEM: return 3;
    case CREATURE_TROLIN: return 5;
    case CREATURE_ANIMATED_ARMOUR: return 7;
    case CREATURE_ROCK_PILE: return 14;
    case CREATURE_PAIN_RAT: return 19;
    case CREATURE_SCREAMER: return 22;
    case CREATURE_MAGENTA_WORM: return 25;
    case CREATURE_RED_DRAGON: return 29;
    default: return -1;
    }
}

/* Object record (weapon, armour, junk): word 2 bits 0-6 type, bit 8 cursed. */
static void object_set_type_and_curse(uint16_t thing, uint16_t type, int16_t cursed)
{
    REC(thing)[3] = (uint8_t)((REC(thing)[3] & 0x80) | (type & 0x7f));
    REC(thing)[2] = (uint8_t)((REC(thing)[2] & 0xfe) | (cursed != 0));
}

/* A dead creature leaves the fixed possessions of its type. Possessions with bit 15 drop half of the time. */
void group_drop_creature_fixed_possessions(uint16_t creature_type, int16_t x, int16_t y, uint16_t cell, int16_t sound_mode)
{
    int16_t index = fixed_possession_list_start(creature_type);
    if (index < 0)
        return;
    int16_t cursed = creature_type == CREATURE_ANIMATED_ARMOUR, weapon_dropped = 0;
    for (uint16_t entry; (entry = *tables_fixed_possession(index)) != 0; index++) {
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
            weapon_dropped = 1;
            thing_type = THING_WEAPON;
            object_type = info_index - OBJECT_INFO_FIRST_WEAPON;
        }
        uint16_t thing = get_unused_thing(thing_type);
        if (thing == THING_NONE)
            continue;
        object_set_type_and_curse(thing, object_type, cursed);
        uint16_t drop_cell = cell;
        if (cell == CELLS_CENTERED || random_2bits() == 0)
            drop_cell = random_2bits();
        move_get_move_result(THING_WITH_CELL(thing, drop_cell), -1, 0, x, y);
    }
    sound_request_play(weapon_dropped ? SOUND_METALLIC_THUD : SOUND_WOODEN_THUD, x, y, (uint16_t)sound_mode);
}

/* Creatures that died during a move drop their fixed possessions on the destination square. */
void group_drop_moving_creature_fixed_possessions(uint16_t group, int16_t x, int16_t y)
{
    uint16_t type = group_creature_type(group);
    while (drop_moving_creature_possessions_cell_count) {
        drop_moving_creature_possessions_cell_count--;
        group_drop_creature_fixed_possessions(type, x, y, drop_moving_creature_possessions_cells[drop_moving_creature_possessions_cell_count], SOUND_MODE_ONE_TICK_LATER);
    }
}

/* All creatures drop their fixed possessions (sound_mode >= 0), then the carried objects fall on random cells. */
void group_drop_group_possessions(int16_t x, int16_t y, uint16_t group, int16_t sound_mode)
{
    uint16_t type = group_creature_type(group);
    if (sound_mode >= 0 && (tables_creature_info(type)->attributes & CREATURE_DROPS_FIXED_POSSESSIONS)) {
        uint8_t cells = get_group_cells(group, g_current_map_index);
        for (int16_t i = group_last_creature_index(group); i >= 0; i--)
            group_drop_creature_fixed_possessions(type, x, y, cells == CELLS_CENTERED ? CELLS_CENTERED : cell_of(cells, i), sound_mode);
    }
    uint16_t thing = group_possessions(group);
    if (thing == THING_ENDOFLIST)
        return;
    int16_t weapon_dropped = 0;
    uint16_t next;
    do {
        next = get_next_thing(thing);
        uint16_t cell = random_2bits();
        if (THING_TYPE(thing) == THING_WEAPON)
            weapon_dropped = 1;
        move_get_move_result(THING_WITH_CELL(thing, cell), -1, 0, x, y);
        thing = next;
    } while (next != THING_ENDOFLIST);
    if (sound_mode >= 0)
        sound_request_play(weapon_dropped ? SOUND_METALLIC_THUD : SOUND_WOODEN_THUD, x, y, (uint16_t)sound_mode);
}

/* The group record becomes free. Health is cleared first, so that no projectile impact runs on the removal. */
void group_delete(int16_t x, int16_t y)
{
    uint16_t group = group_get_thing(x, y);
    if (group == THING_ENDOFLIST)
        return;
    for (int16_t i = 0; i < GROUP_MAX_CREATURES; i++)
        group_set_health(group, i, 0);
    move_get_move_result(group, x, y, -1, 0);
    thing_set_free(group);
    if (g_current_map_index == g_party_map_index) {
        g_active_groups[group_cells_byte(group)].group_index = -1;
        g_active_group_count--;
    }
    group_delete_events(x, y);
}

/* Events of the creatures after a dead creature move down one index; the events of the dead creature go away. */
static void shift_creature_events(int16_t x, int16_t y, uint16_t dead_creature)
{
    for (uint16_t i = 0; i < g_event_high_water; i++) {
        TimelineEvent *event = &g_timeline_events[i];
        if (EVENT_MAP(event) != g_current_map_index || EVENT_X(event) != x || EVENT_Y(event) != y ||
            event->type < EVENT_UPDATE_ASPECT_CREATURE_0 || event->type > EVENT_UPDATE_BEHAVIOR_CREATURE_3)
            continue;
        uint16_t first = event->type < EVENT_UPDATE_BEHAVIOR_GROUP ? EVENT_UPDATE_ASPECT_CREATURE_0 : EVENT_UPDATE_BEHAVIOR_CREATURE_0;
        uint16_t creature = event->type - first;
        if (creature == dead_creature) {
            timeline_delete_event(i);
        } else if (dead_creature < creature) {
            event->type--;
            timeline_fix_chronology(timeline_get_index(i));
        }
    }
}

/* Damage to one creature. A dead creature leaves smoke; the last one also deletes the group (with notify).
   Returns OUTCOME_NO_CREATURE_KILLED, OUTCOME_SOME_CREATURES_KILLED, or OUTCOME_ALL_CREATURES_KILLED. */
int16_t group_get_damage_creature_outcome(uint16_t group, uint16_t creature_index, int16_t x, int16_t y, int16_t damage, int16_t notify)
{
    uint16_t type = group_creature_type(group);
    const CreatureInfo *info = tables_creature_info(type);
    if (info->defense == CREATURE_INVULNERABLE)
        return OUTCOME_NO_CREATURE_KILLED;
    if (group_health(group, creature_index) > (uint16_t)damage) {
        if (damage > 0)
            group_set_health(group, creature_index, group_health(group, creature_index) - damage);
        return OUTCOME_NO_CREATURE_KILLED;
    }
    uint8_t cells = get_group_cells(group, g_current_map_index);
    uint16_t cell = cells == CELLS_CENTERED ? CELLS_CENTERED : cell_of(cells, creature_index);
    uint16_t last = group_last_creature_index(group);
    int16_t outcome;
    if (last == 0) {
        if (notify) {
            group_drop_group_possessions(x, y, group_get_thing(x, y), SOUND_MODE_ONE_TICK_LATER);
            group_delete(x, y);
        }
        outcome = OUTCOME_ALL_CREATURES_KILLED;
    } else {
        uint16_t directions = get_group_directions(group, g_current_map_index);
        if (info->attributes & CREATURE_DROPS_FIXED_POSSESSIONS) {
            if (notify)
                group_drop_creature_fixed_possessions(type, x, y, cell, SOUND_MODE_ONE_TICK_LATER);
            else
                drop_moving_creature_possessions_cells[drop_moving_creature_possessions_cell_count++] = (uint8_t)cell;
        }
        int16_t on_party_map = g_current_map_index == g_party_map_index;
        ActiveGroup *active = on_party_map ? group_active(group) : NULL;
        if (group_behavior(group) == BEHAVIOR_ATTACK) {
            shift_creature_events(x, y, creature_index);
            uint16_t fear = creature_fear_resistance(info);
            if (on_party_map && fear != FEAR_RESISTANCE_NONE_FEARLESS) {
                fear += last - 1;
                if (fear < (random16() & 15)) {
                    active->delay_fleeing = (uint8_t)(random16() % (uint16_t)(100 - 4 * fear) + 20);
                    group_set_behavior(group, BEHAVIOR_FLEE);
                }
            }
        }
        for (uint16_t i = creature_index; i < last; i++) {
            group_set_health(group, i, group_health(group, i + 1));
            directions = group_set_cell_value(directions, i, cell_of(directions, i + 1));
            cells = (uint8_t)group_set_cell_value(cells, i, cell_of(cells, i + 1));
            if (on_party_map)
                active->aspect[i] = active->aspect[i + 1];
        }
        set_group_cells(group, cells & CELLS_OF_THREE_CREATURES, g_current_map_index);
        set_group_directions(group, (uint8_t)directions, g_current_map_index);
        group_set_last_creature_index(group, last - 1);
        outcome = OUTCOME_SOME_CREATURES_KILLED;
    }
    static const uint16_t smoke_attack[] = {SMOKE_ATTACK_QUARTER, SMOKE_ATTACK_HALF, SMOKE_ATTACK_FULL, SMOKE_ATTACK_FULL};
    explosion_create(EXPLOSION_THING(EXPLOSION_SMOKE), smoke_attack[creature_size(info)], (uint16_t)x, (uint16_t)y, (int16_t)cell);
    return outcome;
}

/* The same attack, with a random spread of 1/8, on every creature of the group. */
int16_t group_get_damage_all_creatures_outcome(uint16_t group, int16_t x, int16_t y, int16_t attack, int16_t notify)
{
    drop_moving_creature_possessions_cell_count = 0;
    if (attack < 1)
        return OUTCOME_NO_CREATURE_KILLED;
    int16_t spread = (attack >> 3) + 1, all_killed = 1, some_killed = 0;
    for (int16_t i = group_last_creature_index(group); i >= 0; i--) {
        int16_t damage = (int16_t)(attack - spread + random16() % (uint16_t)(spread * 2));
        int16_t outcome = group_get_damage_creature_outcome(group, (uint16_t)i, x, y, damage, notify);
        all_killed = all_killed && outcome;
        some_killed = some_killed || outcome;
    }
    return all_killed ? OUTCOME_ALL_CREATURES_KILLED : some_killed ? OUTCOME_SOME_CREATURES_KILLED : OUTCOME_NO_CREATURE_KILLED;
}

uint16_t group_get_resistance_adjusted_poison_attack(uint16_t creature_type, int16_t poison_attack)
{
    uint16_t resistance = creature_poison_resistance(tables_creature_info(creature_type));
    if (!poison_attack || resistance == POISON_IMMUNE)
        return 0;
    return (uint16_t)((poison_attack + random_2bits()) * 8) / (resistance + 1);
}

/* A closing door hits the creatures on its square; the survivors react to the danger. */
void group_door_damages_creatures(int16_t x, int16_t y)
{
    uint16_t group = group_get_thing(x, y);
    if (group_get_damage_all_creatures_outcome(group, x, y, DOOR_DAMAGE, 1) != OUTCOME_ALL_CREATURES_KILLED)
        group_process_events_29_to_41(x, y, REACTION_DANGER_ON_SQUARE, 0);
}

/* A Giggler steals objects from random slots while the champion fails the dexterity and luck checks, then flees. */
static void group_steal_from_champion(uint16_t group, uint16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    int16_t stole = 0, chance = (int16_t)(100 - champion_get_dexterity(champion));
    uint16_t slot_index = random16();
    while (chance > 0 && !champion_is_lucky(champion, (uint16_t)chance)) {
        uint16_t slot = kaos_u8(KAOS_SLOTS_TO_STEAL_FROM + (slot_index & 7));
        if (champion->slots[slot] != THING_NONE) {
            stole = 1;
            uint16_t thing = champion_get_object_removed_from_slot(champion_index, slot);
            if (group_possessions(group) == THING_ENDOFLIST)
                group_set_possessions(group, thing);
            else
                link_thing_to_list(thing, group_possessions(group), -1, 0);
            champion_draw_state(champion_index);
        }
        slot_index = (slot_index & 7) + 1;
        chance -= STEAL_CHANCE_STEP;
    }
    if ((random16() & 7) && (!stole || !random_bit()))
        return;
    group_active(group)->delay_fleeing = (uint8_t)((random16() & 63) + 20);
    group_set_behavior(group, BEHAVIOR_FLEE);
}

/* A wall, a solid fake wall, or a closed door that is not see-through hides the party. */
static int16_t group_is_view_party_blocked(int16_t x, int16_t y)
{
    uint8_t square = g_current_map_data[x][y];
    switch (SQUARE_TYPE(square)) {
    case SQUARE_DOOR: {
        int16_t state = door_state(square);
        int16_t see_through = g_current_map_door_info[door_type(get_square_first_thing(x, y))] >> 8 & 1;
        return (state == 3 || state == DOOR_CLOSED) && !see_through;
    }
    case SQUARE_WALL:
        return 1;
    case SQUARE_FAKE_WALL:
        return !(square & FAKE_WALL_OPEN);
    default:
        return 0;
    }
}

/* Only walls and solid fake walls stop the smell of the party. */
static int16_t group_is_smell_party_blocked(int16_t x, int16_t y)
{
    uint8_t square = g_current_map_data[x][y];
    return SQUARE_TYPE(square) == SQUARE_WALL || (SQUARE_TYPE(square) == SQUARE_FAKE_WALL && !(square & (FAKE_WALL_OPEN | FAKE_WALL_IMAGINARY)));
}

/* Slope of the line from the source in fixed point (* 64); 128 for a zero divisor. */
static int16_t line_slope(int16_t along, int16_t across)
{
    return along ? (int16_t)((int16_t)(across * 64) / along) : 128;
}

/* Walks a line from the destination back to the source. Returns the square distance, or 0 when a square blocks. */
static int16_t group_get_distance_between_unblocked_squares(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y,
                                                           int16_t (*is_blocked)(int16_t, int16_t))
{
    if (abs16(src_x - dst_x) + abs16(src_y - dst_y) <= 1)
        return 1;
    int16_t dx = abs16(dst_x - src_x), dy = abs16(dst_y - src_y);
    int16_t y_major = dy > dx, diagonal = dy == dx;
    int16_t step_x = dst_x - src_x > 0 ? -1 : 1, step_y = dst_y - src_y > 0 ? -1 : 1;
    int16_t x = dst_x, y = dst_y;
    int16_t slope = y_major ? line_slope(dst_y - src_y, dst_x - src_x) : line_slope(dst_x - src_x, dst_y - src_y);
    do {
        if (diagonal) {
            if (is_blocked(x + step_x, y) && is_blocked(x, y + step_y))
                return 0;
            y += step_y;
            x += step_x;
            if (is_blocked(x, y))
                return 0;
        } else {
            int16_t error_x = y_major ? line_slope(y - src_y, x + step_x - src_x) : line_slope(x + step_x - src_x, y - src_y);
            int16_t error_y = y_major ? line_slope(y + step_y - src_y, x - src_x) : line_slope(x - src_x, y + step_y - src_y);
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
    return get_square_distance(src_x, src_y, dst_x, dst_y);
}

/* Distance to the party when a creature (or any creature with -1) sees it, else 0. Light, invisibility, and
   the sight range limit the view. Quirk kept: the invisibility range -10 is compared unsigned, so it does not hide
   the party from a creature that does not see invisible. */
static uint16_t group_get_distance_to_visible_party(uint16_t group, int16_t creature_index, int16_t x, int16_t y)
{
    const CreatureInfo *info = group_info(group);
    if (!(info->attributes & CREATURE_SIDE_ATTACK)) {
        uint8_t directions = group_active(group)->directions;
        uint16_t seen[GROUP_MAX_CREATURES];
        int16_t count = 0;
        if (creature_index < 0) {
            for (int16_t i = group_last_creature_index(group); i >= 0; i--) {
                uint16_t direction = cell_of(directions, i);
                int16_t known = 0;
                for (int16_t k = 0; k < count; k++)
                    known |= seen[k] == direction;
                if (!known)
                    seen[count++] = direction;
            }
        } else {
            seen[count++] = cell_of(directions, creature_index);
        }
        int16_t visible = 0;
        while (count && !visible) {
            count--;
            visible = group_is_destination_visible_from_source((int16_t)seen[count], x, y, g_party_map_x, g_party_map_y);
        }
        if (!visible)
            return 0;
    }
    uint16_t range = creature_sight_range(info);
    if (g_party.event71_count_invisibility && !(info->attributes & CREATURE_SEES_INVISIBLE))
        range = (uint16_t)-10;
    else if (!(info->attributes & CREATURE_NIGHT_VISION))
        range -= g_dungeon_view_palette_index >> 1;
    if (range < g_current_group_distance_to_party) {
        if (g_current_group_distance_to_party == 1) {
            uint16_t random1 = random16(), random2 = random16();
            range += random1 % ((info->ranges >> 4 & 15) + 1) + random2 % (creature_smell_range(info) + 1);
            if (!(random16() & 7))
                range += random_bit() + 5;
        }
        if ((uint16_t)(range + (random16() & 7) - 3) < g_current_group_distance_to_party)
            return 0;
    }
    return (uint16_t)group_get_distance_between_unblocked_squares(x, y, g_party_map_x, g_party_map_y, group_is_view_party_blocked);
}

/* Ordinal of the latest party scent on a square (champion_get_scent_ordinal), or 0. */
static uint16_t party_scent_ordinal(int16_t x, int16_t y)
{
    uint16_t scent = (uint16_t)(x << 11 | (y & 31) << 6 | (g_current_map_index & 63));
    for (uint16_t ordinal = g_party.scent_count; ordinal; ordinal--)
        if (g_party.scents[ordinal - 1] == scent)
            return ordinal;
    return 0;
}

/* Direction ordinal toward the party by smell: straight when close enough, else along the scent trail. 0: none. */
static int16_t group_get_smelled_party_direction_ordinal(const CreatureInfo *info, int16_t x, int16_t y)
{
    uint16_t smell = creature_smell_range(info);
    if (!smell)
        return 0;
    if (g_current_group_distance_to_party <= (smell + 1) >> 1 &&
        group_get_distance_between_unblocked_squares(x, y, g_party_map_x, g_party_map_y, group_is_smell_party_blocked)) {
        g_group_secondary_direction = g_current_group_secondary_direction_to_party;
        return g_current_group_primary_direction_to_party + 1;
    }
    uint16_t ordinal = party_scent_ordinal(x, y);
    if (ordinal && (uint16_t)(30 - 2 * smell) < (uint16_t)(random_2bits() + g_party.scent_strengths[ordinal - 1])) {
        uint16_t next = g_party.scents[ordinal];
        return group_get_directions_to_destination(x, y, next >> 11, next >> 6 & 31) + 1;
    }
    return 0;
}

/* Can the creature step one square in a direction? Sets the movement_blocked_by_* results. Lord Chaos stops at
   fluxcages. A wary creature avoids a teleporter to a map that does not allow its type. */
static int16_t group_is_movement_possible(const CreatureInfo *info, int16_t x, int16_t y, uint16_t direction, int16_t allow_imaginary)
{
    movement_tested_directions[direction] = 1;
    movement_blocked_by_group = THING_ENDOFLIST;
    movement_blocked_by_door = movement_blocked_by_party = 0;
    if (info->movement_ticks == CREATURE_IMMOBILE)
        return 0;
    step((int16_t)direction, 1, &x, &y);
    uint8_t square = 0;
    int16_t type = SQUARE_WALL, open = 0;
    if (x >= 0 && x < g_current_map_width && y >= 0 && y < g_current_map_height) {
        square = g_current_map_data[x][y];
        type = SQUARE_TYPE(square);
        int16_t imaginary_allowed = (square & PIT_IMAGINARY) && allow_imaginary;
        open = type != SQUARE_WALL && type != SQUARE_STAIRS &&
               (type != SQUARE_PIT || imaginary_allowed || !(square & PIT_OPEN) || (info->attributes & CREATURE_LEVITATES)) &&
               (type != SQUARE_FAKE_WALL || (square & FAKE_WALL_OPEN) || imaginary_allowed);
    }
    movement_blocked_by_wall = !open;
    if (!open)
        return 0;
    if (info->attributes & CREATURE_ARCHENEMY) {
        for (uint16_t t = get_square_first_thing(x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
            if (THING_TYPE(t) == THING_EXPLOSION && explosion_type(t) == EXPLOSION_FLUXCAGE) {
                flux_cages[direction] = 1;
                flux_cage_count++;
                movement_blocked_by_wall = 1;
                return 0;
            }
        }
    }
    if (type == SQUARE_TELEPORTER && (square & TELEPORTER_OPEN) && creature_wariness(info) >= WARINESS_AVOIDS_FORBIDDEN_TELEPORTER) {
        uint16_t teleporter = get_square_first_thing(x, y);
        if ((teleporter_scope(teleporter) & TELEPORTER_SCOPE_CREATURES) && !is_creature_allowed_on_map(g_current_group_thing, teleporter_target_map(teleporter))) {
            movement_blocked_by_wall = 1;
            return 0;
        }
    }
    movement_blocked_by_party = is_party_on(x, y);
    if (movement_blocked_by_party)
        return 0;
    if (type == SQUARE_DOOR) {
        int16_t height = door_checks_creature_height(get_square_first_thing(x, y)) ? creature_height(info->attributes) : 1;
        int16_t state = door_state(square);
        if (height < state && state != DOOR_DESTROYED && !(info->attributes & CREATURE_NON_MATERIAL)) {
            movement_blocked_by_door = 1;
            return 0;
        }
    }
    movement_blocked_by_group = group_get_thing(x, y);
    return movement_blocked_by_group == THING_ENDOFLIST;
}

/* Ordinal of the first untested direction where the creature can move, or 0. */
static int16_t group_get_first_possible_movement_direction_ordinal(const CreatureInfo *info, int16_t x, int16_t y, int16_t allow_imaginary)
{
    for (uint16_t direction = 0; direction < 4; direction++)
        if (!movement_tested_directions[direction] && group_is_movement_possible(info, x, y, direction, allow_imaginary))
            return (int16_t)direction + 1;
    return 0;
}

/* Lord Chaos jumps two squares: the first square must have no fluxcage, the second must be free. */
static int16_t group_is_archenemy_double_movement_possible(const CreatureInfo *info, int16_t x, int16_t y, uint16_t direction)
{
    if (flux_cages[direction])
        return 0;
    step((int16_t)direction, 1, &x, &y);
    return group_is_movement_possible(info, x, y, direction, 0);
}

/* Turns one creature. A creature never turns around at once: it turns left or right first. Two half-square
   creatures turn together, once per game tick. */
static void group_set_direction(ActiveGroup *active, int16_t direction, int16_t creature_index, int16_t turn_pair)
{
    if (turn_pair && g_game_time == set_direction_time && active == set_direction_group)
        return;
    uint16_t directions = active->directions;
    if (((cell_of(directions, creature_index) - direction) & 3) == 2)
        direction = (int16_t)(((random16() & 2) + direction + 1) & 3);
    directions = group_set_cell_value(directions, (uint16_t)creature_index, (uint16_t)direction);
    if (turn_pair) {
        directions = group_set_cell_value(directions, (uint16_t)(creature_index ^ 1), (uint16_t)direction);
        set_direction_time = g_game_time;
        set_direction_group = active;
    }
    active->directions = (uint8_t)directions;
}

/* Turns the group: the first creature always, the others half of the time. */
static void group_set_direction_group(ActiveGroup *active, int16_t direction, int16_t last_creature_index, uint16_t size)
{
    int16_t turn_pair = last_creature_index && size == CREATURE_SIZE_HALF;
    if (turn_pair)
        last_creature_index--;
    for (int16_t i = last_creature_index; i >= 0; i--)
        if (i == 0 || random_bit())
            group_set_direction(active, direction, i, turn_pair);
}

/* Explosion type of the spell that a creature type casts at the party. */
static uint16_t creature_spell_explosion(uint16_t creature_type, int16_t *spell_sound)
{
    *spell_sound = 1;
    switch (creature_type) {
    case CREATURE_SWAMP_SLIME:
        *spell_sound = 0;
        return EXPLOSION_SLIME;
    case CREATURE_WIZARD_EYE:
        return (random16() & 7) ? EXPLOSION_LIGHTNING_BOLT : EXPLOSION_OPEN_DOOR;
    case CREATURE_VEXIRK:
    case CREATURE_LORD_CHAOS:
        if (random_bit())
            return EXPLOSION_FIREBALL;
        switch (random_2bits()) {
        case 0: return EXPLOSION_HARM_NON_MATERIAL;
        case 1: return EXPLOSION_LIGHTNING_BOLT;
        case 2: return EXPLOSION_POISON_CLOUD;
        default: return EXPLOSION_OPEN_DOOR;
        }
    case CREATURE_MATERIALIZER:
        return random_bit() ? EXPLOSION_POISON_CLOUD : EXPLOSION_FIREBALL;
    default:
        return EXPLOSION_FIREBALL;
    }
}

/* One creature attacks: a melee hit (or theft) on a champion in front, or a spell projectile toward the party.
   Returns 1 when the creature attacked. */
static int16_t group_is_creature_attacking(uint16_t group, int16_t x, int16_t y, int16_t creature_index)
{
    int16_t primary = g_current_group_primary_direction_to_party, spell = 0, spell_sound = 1;
    g_last_creature_attack_time = (int32_t)g_game_time;
    uint16_t type = group_creature_type(group);
    const CreatureInfo *info = tables_creature_info(type);
    uint8_t cells = group_active(group)->cells;
    uint16_t side = cells == CELLS_CENTERED ? random_bit() : ((cell_of(cells, creature_index) + 5 - primary) & 2) >> 1;
    int16_t cell = (int16_t)((primary + side) & 3);
    if (creature_attack_range(info) <= 1 || (g_current_group_distance_to_party <= 1 && !random_bit())) {
        int16_t champion_index;
        if (!(info->attributes & CREATURE_ATTACK_ANY_CHAMPION)) {
            champion_index = champion_get_target_champion_index(x, y, cell);
            if (champion_index < 0)
                return 0;
        } else {
            champion_index = random_2bits();
            int16_t tried = 0;
            while (tried < CHAMPION_MAX_COUNT && !g_champions[champion_index].current_health) {
                champion_index = (champion_index + 1) & 3;
                tried++;
            }
            if (tried == CHAMPION_MAX_COUNT)
                return 0;
        }
        if (type == CREATURE_GIGGLER) {
            group_steal_from_champion(group, (uint16_t)champion_index);
        } else {
            int16_t damage = group_get_champion_damage(group, (uint16_t)champion_index) + 1;
            Champion *champion = &g_champions[champion_index];
            if (champion->maximum_damage_received < damage) {
                champion->maximum_damage_received = (uint8_t)damage;
                champion->direction_maximum_damage_received = (uint8_t)((primary + 2) & 3);
            }
        }
    } else {
        spell = 1;
        uint16_t explosion = creature_spell_explosion(type, &spell_sound);
        uint16_t kinetic_energy = (info->attack >> 2) + 1;
        kinetic_energy += random16() % kinetic_energy;
        kinetic_energy += random16() % kinetic_energy;
        projectile_create(EXPLOSION_THING(explosion), x, y, cell, primary, (uint8_t)bounded_value(20, (int16_t)kinetic_energy, 255),
                          info->dexterity, 8);
    }
    int16_t sound = info->attack_sound_ordinal ? tables_creature_sound(info->attack_sound_ordinal - 1)->attack : -1;
    if (sound != -1)
        sound_request_play(sound, x, y, SOUND_MODE_AFTER_TICK);
    else if (spell && spell_sound)
        sound_request_play(SOUND_SPELL, x, y, SOUND_MODE_AFTER_TICK);
    return 1;
}

/* Adds the next event of a group. When the aspect update comes first, the event becomes an aspect event and keeps
   the delay to the behavior update in byte 8. */
static void group_add_event(TimelineEvent *event, uint32_t aspect_update_time)
{
    uint32_t time = event->map_time & EVENT_TIME_MASK;
    if (aspect_update_time < time) {
        event->type -= EVENT_BEHAVIOR_TO_ASPECT;
        event->c = (uint16_t)((uint8_t)(time - aspect_update_time) << 8);
        event->map_time = (event->map_time & ~(uint32_t)EVENT_TIME_MASK) | aspect_update_time;
    } else {
        event->c = (uint16_t)((uint8_t)(aspect_update_time - time) << 8);
    }
    timeline_add_event(event);
}

int16_t get_square_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    return (int16_t)(abs16(x1 - x2) + abs16(y1 - y2));
}

/* True when the destination is in the 90 degree cone in front of the source. */
int16_t group_is_destination_visible_from_source(int16_t direction, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y)
{
    int16_t ahead, side;
    switch (direction) {
    case DIRECTION_NORTH: ahead = src_y - dst_y; side = src_x - dst_x; break;
    case DIRECTION_EAST: ahead = dst_x - src_x; side = dst_y - src_y; break;
    case DIRECTION_SOUTH: ahead = dst_y - src_y; side = dst_x - src_x; break;
    default: ahead = src_x - dst_x; side = src_y - dst_y; break;
    }
    ahead++;
    return ahead > 0 && abs16(side) <= ahead;
}

/* Main direction from a source square to a destination. The second direction goes to g_group_secondary_direction. */
int16_t group_get_directions_to_destination(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y)
{
    if (dst_x == src_x) {
        g_group_secondary_direction = (int16_t)((random16() & 2) + 1);
        return src_y <= dst_y ? DIRECTION_SOUTH : DIRECTION_NORTH;
    }
    if (dst_y == src_y) {
        g_group_secondary_direction = (int16_t)(random16() & 2);
        return src_x <= dst_x ? DIRECTION_EAST : DIRECTION_WEST;
    }
    int16_t primary = 0;
    while (!group_is_destination_visible_from_source(primary, src_x, src_y, dst_x, dst_y))
        primary++;
    int16_t secondary = (primary + 1) & 3;
    int16_t both_visible = group_is_destination_visible_from_source(secondary, src_x, src_y, dst_x, dst_y);
    if (!both_visible && primary == 0) {
        secondary = DIRECTION_WEST;
        both_visible = group_is_destination_visible_from_source(secondary, src_x, src_y, dst_x, dst_y);
    }
    if (!both_visible) {
        g_group_secondary_direction = (int16_t)((primary + (random16() & 2) + 1) & 3);
        return primary;
    }
    if (random_bit()) {
        g_group_secondary_direction = primary;
        return secondary;
    }
    g_group_secondary_direction = secondary;
    return primary;
}

/* The 4 cells of the target square in the order the attacker reaches them. */
void group_set_ordered_cells_to_attack(uint8_t *ordered_cells, int16_t target_x, int16_t target_y, int16_t group_x, int16_t group_y, int16_t group_cell)
{
    int16_t direction = group_get_directions_to_destination(target_x, target_y, group_x, group_y);
    if (!(direction & 1))
        group_cell++;
    uint32_t row = KAOS_ORDERED_CELLS_TO_ATTACK + (uint32_t)(((((uint16_t)group_cell >> 1) & 1) + direction * 2) * 4);
    for (int16_t i = 0; i < 4; i++)
        ordered_cells[i] = kaos_u8(row + i);
}

/* The champion that a creature on an adjacent square attacks, or -1. */
int16_t champion_get_target_champion_index(int16_t x, int16_t y, int16_t cell)
{
    if (!g_party_champion_count || abs16(x - g_party_map_x) + abs16(y - g_party_map_y) > 1)
        return -1;
    uint8_t cells[4];
    group_set_ordered_cells_to_attack(cells, g_party_map_x, g_party_map_y, x, y, cell);
    for (int16_t i = 0; i < 4; i++) {
        int16_t champion_index = champion_get_index_in_cell(cells[i]);
        if (champion_index >= 0)
            return champion_index;
    }
    return -1;
}

/* Wounds of a creature hit: random wound masks weighted by the wound probability nibbles of the creature. */
static uint16_t creature_hit_wounds(const CreatureInfo *info)
{
    uint16_t random = random16();
    if (!(random & 0x70))
        return random & 1;
    uint16_t probabilities = info->wound_probabilities, index = 0;
    while ((probabilities & 15) < (random & 15)) {
        index++;
        probabilities >>= 4;
    }
    return kaos_u8(KAOS_WOUND_PROBABILITY_INDEX_TO_MASK + index);
}

/* A creature hits a champion: dodge by dexterity and luck, damage from attack, map difficulty, and parry skill,
   then poison. Returns the damage. */
int16_t group_get_champion_damage(uint16_t group, uint16_t champion_index)
{
    Champion *champion = &g_champions[champion_index];
    if (champion_index >= g_party_champion_count || !champion->current_health)
        return 0;
    if (g_party_is_sleeping)
        champion_wake_up();
    int16_t difficulty = (int16_t)(current_map_difficulty() * 2);
    const CreatureInfo *info = group_info(group);
    champion_add_skill_experience(champion_index, SKILL_PARRY_DEFENSE, creature_experience(info));
    int16_t dodge_bonus;
    if (g_party.event71_count_invisibility && !(info->attributes & CREATURE_SEES_INVISIBLE))
        dodge_bonus = INVISIBLE_PARTY_DODGE_BONUS;
    else if (info->attributes & CREATURE_NIGHT_VISION)
        dodge_bonus = 0;
    else
        dodge_bonus = (int16_t)(g_dungeon_view_palette_index * 2);
    if (!g_party_is_sleeping && info->dexterity != CREATURE_NEVER_HITS) {
        uint16_t dexterity = (uint16_t)champion_get_dexterity(champion);
        uint16_t aim = (uint16_t)((random16() & 31) + info->dexterity + difficulty + dodge_bonus - 16);
        if (aim <= dexterity && random_2bits())
            return 0;
        if (champion_is_lucky(champion, LUCK_PERCENT_AGAINST_CREATURE))
            return 0;
    }
    uint16_t wounds = creature_hit_wounds(info);
    uint16_t random = random16();
    int16_t damage = (int16_t)((random & 15) + info->attack + difficulty - 2 * champion_get_skill_level((int16_t)champion_index, SKILL_PARRY_DEFENSE));
    if (damage <= 1) {
        if (random_bit())
            return 0;
        damage = random_2bits() + 2;
    }
    damage >>= 1;
    random = random16();
    damage += (int16_t)(random % (uint16_t)damage + random_2bits());
    damage += (int16_t)(random16() % (uint16_t)damage);
    damage >>= 2;
    damage += random_2bits() + 1;
    if (random_bit())
        damage -= (int16_t)(random16() % (uint16_t)((damage >> 1) + 1) - 1);
    damage = champion_add_pending_damage_and_wounds((int16_t)champion_index, damage, wounds, info->attack_type);
    if (!damage)
        return 0;
    sound_request_play(SOUND_CHAMPION_0_DAMAGED + champion_index, g_party_map_x, g_party_map_y, SOUND_MODE_ONE_TICK_LATER);
    if (info->poison_attack && random_bit()) {
        uint16_t poison = champion_get_statistic_adjusted_attack(champion, STAT_VITALITY, info->poison_attack);
        champion_poison((int16_t)champion_index, poison);
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
    base += random16() & 15;
    if (base > 0 || random_bit()) {
        damage += random_2bits();
        if (!random_2bits())
            damage += max16(0, (int16_t)(base + (random16() & 15)));
    }
    return damage;
}

/* A champion hits a creature with a melee action: hit chance by dexterity, damage by strength, the action damage
   factor, and the creature defense. Returns the damage (0 for a miss). */
int16_t group_get_melee_action_damage(int16_t champion_index, uint16_t group, int16_t creature_index, int16_t x, int16_t y,
                                      uint16_t hit_probability, uint16_t damage_factor, int16_t skill_index)
{
    if (champion_index >= (int16_t)g_party_champion_count)
        return 0;
    Champion *champion = &g_champions[champion_index];
    if (!champion->current_health)
        return 0;
    int16_t difficulty = (int16_t)(current_map_difficulty() * 2), damage = 0, outcome = OUTCOME_NO_CREATURE_KILLED;
    const CreatureInfo *info = group_info(group);
    int16_t hit = 0;
    if (info->dexterity != CREATURE_NEVER_HITS) {
        int16_t icon = object_get_icon_index(champion->slots[SLOT_ACTION_HAND]);
        int16_t always_hits = (hit_probability & HIT_PROBABILITY_ALWAYS_HITS) != 0;
        hit_probability &= HIT_PROBABILITY_MASK;
        if (!(info->attributes & CREATURE_NON_MATERIAL) || always_hits) {
            uint16_t dexterity = (uint16_t)champion_get_dexterity(champion);
            uint16_t defense = (uint16_t)((random16() & 31) + info->dexterity + difficulty + g_dungeon_view_palette_index * 2 - 16);
            hit = defense < dexterity || !random_2bits() || champion_is_lucky(champion, (uint16_t)(LUCK_PERCENT_AGAINST_MISS - hit_probability));
        }
        if (hit) {
            /* Bug fix: without strength the original uses a stale register as the base; the port uses 0. */
            int16_t strength = champion_get_strength(champion_index, SLOT_ACTION_HAND), base = 0;
            if (strength) {
                strength += (int16_t)(random16() % (uint16_t)((strength >> 1) + 1));
                damage = (int16_t)(((int32_t)strength * damage_factor) >> 5);
                int16_t armour = (int16_t)((random16() & 31) + info->defense + difficulty);
                if (icon == ICON_DIAMOND_EDGE)
                    armour -= armour >> 2;
                else if (icon == ICON_VORPAL_BLADE)
                    armour -= armour >> 3;
                damage += (int16_t)(random16() & 31);
                damage -= armour;
                base = damage;
            }
            if (damage <= 1)
                damage = weak_hit_damage(base);
            if (damage) {
                damage >>= 1;
                uint16_t random = random16();
                damage += (int16_t)(random % (uint16_t)damage + random_2bits());
                damage += (int16_t)(random16() % (uint16_t)damage);
                damage >>= 2;
                damage += random_2bits() + 1;
                if (icon == ICON_HALF_DAMAGE_ON_MATERIAL && !(info->attributes & CREATURE_NON_MATERIAL))
                    damage >>= 1;
            }
            hit = damage != 0;
        }
    }
    if (hit) {
        uint16_t random = random16();
        if ((random & 63) < (uint16_t)champion_get_skill_level(champion_index, (uint16_t)skill_index))
            damage = damage * 2 + 10;
        outcome = group_get_damage_creature_outcome(group, (uint16_t)creature_index, x, y, damage, 1);
        champion_add_skill_experience((uint16_t)champion_index, (uint16_t)skill_index, (uint16_t)((uint16_t)(creature_experience(info) * damage) >> 4) + 3);
        champion_decrement_stamina(champion_index, random_2bits() + 4);
    } else {
        damage = 0;
        champion_decrement_stamina(champion_index, random_bit() + 2);
    }
    champion_draw_state((uint16_t)champion_index);
    if (outcome != OUTCOME_ALL_CREATURES_KILLED)
        group_process_events_29_to_41(x, y, REACTION_PARTY_IS_ADJACENT, 0);
    return damage;
}

/* State of one run of group_process_events_29_to_41. */
typedef struct {
    int16_t x, y;
    int16_t event_type;              /* reactions are REACTION_* here */
    uint16_t ticks;
    uint16_t group;
    const CreatureInfo *info;
    ActiveGroup *active;
    TimelineEvent next;              /* the next event of the group */
    uint32_t aspect_update_time;     /* 0: the aspect of the whole group is updated */
    int16_t ticks_delay_next_event;  /* 0: ticks delay the aspect update instead (behavior events) */
    int16_t ticks_since_last_move;   /* -1: the group waits to move */
    int16_t movement_ticks;
    int16_t archenemy;
    uint16_t last_creature_index, size;
    int16_t distance_x, distance_y;
    int16_t moved;
    int16_t was_attacking;
    uint16_t visible_distance;
    int16_t direction;               /* the direction the group wants to move in */
} GroupTurn;

/* Adds the next event with the aspect update time, and the ticks that the event carried. */
static void add_event_with_aspect(GroupTurn *t)
{
    if (t->ticks_delay_next_event)
        t->next.map_time += t->ticks;
    else
        t->aspect_update_time += t->ticks;
    group_add_event(&t->next, t->aspect_update_time);
}

/* The next event is a group behavior event; the aspect of the whole group changes when no time is set. */
static void add_behavior_event(GroupTurn *t)
{
    t->next.type = EVENT_UPDATE_BEHAVIOR_GROUP;
    if (!t->aspect_update_time)
        t->aspect_update_time = group_get_creature_aspect_update_time(t->active, -1, 0);
    add_event_with_aspect(t);
}

/* The group acts again after about its movement ticks. */
static void add_wander_event(GroupTurn *t)
{
    t->next.map_time += max16(1, (int16_t)(t->movement_ticks + random_2bits() - 1));
    add_behavior_event(t);
}

/* The group approaches the party it sees. */
static void set_behavior_approach(GroupTurn *t)
{
    group_set_behavior(t->group, BEHAVIOR_APPROACH);
    set_target_to_party(t->active);
    t->next.map_time++;
    add_behavior_event(t);
}

/* The group attacks: each creature turns to the party and gets its own behavior event. */
static void set_behavior_attack(GroupTurn *t)
{
    if (t->event_type == REACTION_HIT_BY_PROJECTILE)
        group_delete_events(t->x, t->y);
    set_target_to_party(t->active);
    group_set_behavior(t->group, BEHAVIOR_ATTACK);
    int16_t primary = g_current_group_primary_direction_to_party;
    int16_t turn_pair = t->last_creature_index && t->size == CREATURE_SIZE_HALF;
    for (int16_t i = (int16_t)t->last_creature_index; i >= 0; i--) {
        uint32_t map = t->next.map_time & ~(uint32_t)EVENT_TIME_MASK;
        if (cell_of(t->active->directions, i) == (uint16_t)primary || (i && random_bit())) {
            t->next.map_time = map | (g_game_time + 1);
        } else {
            group_set_direction(t->active, primary, i, turn_pair);
            t->next.map_time = map | (g_game_time + random_2bits() + 2);
        }
        if (t->ticks_delay_next_event)
            t->next.map_time += min16((int16_t)(random_2bits() + (t->info->attack_ticks >> 1)), (int16_t)t->ticks);
        t->next.type = (uint8_t)(EVENT_UPDATE_BEHAVIOR_CREATURE_0 + i);
        group_add_event(&t->next, group_get_creature_aspect_update_time(t->active, i, 0));
    }
}

/* The group moves one or two squares when its movement ticks have passed. Returns 1 when the move ended the turn
   (the group left the map, died, or changed its active entry). */
static int16_t try_move(GroupTurn *t, int16_t target_x, int16_t target_y)
{
    int16_t remaining = (int16_t)((t->movement_ticks >> 1) - t->ticks_since_last_move);
    t->moved = remaining < 1;
    if (!t->moved) {
        t->ticks_since_last_move = -1;
        t->movement_ticks = remaining;
        return 0;
    }
    if (move_get_move_result(t->group, t->x, t->y, target_x, target_y))
        return 1;
    t->next.b = EVENT_SQUARE(g_move_result_map_x, g_move_result_map_y);
    t->active->prior_x = (uint8_t)t->x;
    t->active->prior_y = (uint8_t)t->y;
    t->active->last_move_time = (uint8_t)g_game_time;
    return 0;
}

/* After a move or a failed move: the group turns, a reaction ends the attack, and the next event follows. */
static void turn_and_finish(GroupTurn *t, int16_t direction)
{
    if (!t->moved && t->ticks_since_last_move >= 0)
        direction = random_2bits();
    group_set_direction_group(t->active, direction, (int16_t)t->last_creature_index, t->size);
    if (t->event_type < REACTION_PARTY_IS_ADJACENT) {
        if (!t->moved)
            return;
        if (t->was_attacking)
            group_set_behavior(t->group, BEHAVIOR_APPROACH);
        group_stop_attacking(t->active, t->x, t->y);
    }
    add_wander_event(t);
}

/* A group that did not move keeps its direction when it smells the party, except after danger. */
static void finish_movement(GroupTurn *t, int16_t direction)
{
    if (!t->moved) {
        int16_t keep_direction = random_2bits() != 0 && t->visible_distance > creature_smell_range(t->info);
        if (keep_direction || t->event_type == REACTION_DANGER_ON_SQUARE) {
            if (t->event_type < REACTION_PARTY_IS_ADJACENT && !t->moved)
                return;
            add_wander_event(t);
            return;
        }
    }
    turn_and_finish(t, direction);
}

static int16_t archenemy_jump(GroupTurn *t);

/* After a move attempt Lord Chaos may jump in a random direction; any other group finishes its movement. */
static int16_t after_move(GroupTurn *t, int16_t direction)
{
    if (t->moved || t->ticks_since_last_move == -1 || !t->archenemy || (t->event_type != REACTION_DANGER_ON_SQUARE && random_2bits())) {
        finish_movement(t, direction);
        return 0;
    }
    t->direction = random_2bits();
    g_group_secondary_direction = (t->direction + 1) & 3;
    return archenemy_jump(t);
}

/* Lord Chaos jumps two squares over a free square, away from fluxcages. Returns 1 when the turn ended at once. */
static int16_t archenemy_jump(GroupTurn *t)
{
    for (;;) {
        group_get_first_possible_movement_direction_ordinal(t->info, t->x, t->y, 0);
        int16_t direction = t->direction;
        int16_t possible = group_is_archenemy_double_movement_possible(t->info, t->x, t->y, (uint16_t)direction);
        if (!possible) {
            direction = g_group_secondary_direction;
            possible = group_is_archenemy_double_movement_possible(t->info, t->x, t->y, (uint16_t)direction);
        }
        if (!possible && flux_cage_count) {
            direction = (g_group_secondary_direction + 2) & 3;
            possible = group_is_archenemy_double_movement_possible(t->info, t->x, t->y, (uint16_t)direction);
        }
        if (!possible && flux_cage_count >= 2) {
            direction = (t->direction + 2) & 3;
            possible = group_is_archenemy_double_movement_possible(t->info, t->x, t->y, (uint16_t)direction);
        }
        if (!possible) {
            group_set_direction_group(t->active, t->direction, (int16_t)t->last_creature_index, t->size);
            add_wander_event(t);
            return 0;
        }
        int16_t x = t->x, y = t->y;
        step(direction, 2, &x, &y);
        sound_request_play(SOUND_BUZZ, x, y, SOUND_MODE_AFTER_TICK);
        if (try_move(t, x, y))
            return 1;
        if (t->moved || t->ticks_since_last_move == -1 || (t->event_type != REACTION_DANGER_ON_SQUARE && random_2bits())) {
            finish_movement(t, direction);
            return 0;
        }
        t->direction = random_2bits();
        g_group_secondary_direction = (t->direction + 1) & 3;
    }
}

/* Moves one square in a direction that is possible. */
static int16_t move_in_direction(GroupTurn *t, int16_t direction)
{
    int16_t x = t->x, y = t->y;
    step(direction, 1, &x, &y);
    if (try_move(t, x, y))
        return 1;
    return after_move(t, direction);
}

/* Moves toward t->direction, else toward the second direction, then the other sides. */
static int16_t move_toward(GroupTurn *t, int16_t allow_imaginary)
{
    int16_t direction = t->direction;
    if (group_is_movement_possible(t->info, t->x, t->y, (uint16_t)direction, allow_imaginary))
        return move_in_direction(t, direction);
    int16_t allow_second = allow_imaginary && random_bit();
    direction = g_group_secondary_direction;
    if (group_is_movement_possible(t->info, t->x, t->y, (uint16_t)direction, allow_second))
        return move_in_direction(t, direction);
    direction = (direction + 2) & 3;
    if (group_is_movement_possible(t->info, t->x, t->y, (uint16_t)direction, 0))
        return move_in_direction(t, direction);
    if (!random_2bits()) {
        direction = (t->direction + 2) & 3;
        if (group_is_movement_possible(t->info, t->x, t->y, (uint16_t)direction, 0))
            return move_in_direction(t, direction);
    }
    if (t->archenemy)
        return archenemy_jump(t);
    group_set_direction_group(t->active, t->direction, (int16_t)t->last_creature_index, t->size);
    add_wander_event(t);
    return 0;
}

/* Wanders in a random direction, avoiding the square it came from. A party in the way starts an attack. */
static int16_t random_move(GroupTurn *t)
{
    int16_t first = random_2bits(), direction = first;
    do {
        int16_t x = t->x, y = t->y;
        step(direction, 1, &x, &y);
        if (((uint16_t)x != t->active->prior_x || (uint16_t)y != t->active->prior_y || !random_2bits()) &&
            group_is_movement_possible(t->info, t->x, t->y, (uint16_t)direction, 0)) {
            if (try_move(t, x, y))
                return 1;
            return after_move(t, direction);
        }
        if (movement_blocked_by_party) {
            if (t->event_type != REACTION_DANGER_ON_SQUARE && t->info->attack_ticks != CREATURE_NO_ATTACK &&
                (group_behavior(t->group) != BEHAVIOR_FLEE || !group_get_first_possible_movement_direction_ordinal(t->info, t->x, t->y, 0) || random_bit())) {
                set_behavior_attack(t);
                return 0;
            }
            set_target_to_party(t->active);
        }
        direction = (direction + 1) & 3;
    } while (direction != first);
    return after_move(t, direction);
}

/* Moves toward the target square, or wanders when the group stands on it. */
static int16_t approach_target(GroupTurn *t)
{
    int16_t target_x = t->active->target_x, target_y = t->active->target_y;
    if (target_x == t->x && target_y == t->y) {
        t->moved = 0;
        group_set_behavior(t->group, BEHAVIOR_WANDER);
        turn_and_finish(t, 0);
        return 0;
    }
    t->direction = group_get_directions_to_destination(t->x, t->y, target_x, target_y);
    return move_toward(t, 1);
}

/* Moves toward the party at double speed. */
static int16_t approach_party(GroupTurn *t)
{
    t->movement_ticks = (int16_t)((t->movement_ticks + 1) >> 1);
    set_target_to_party(t->active);
    t->direction = group_get_directions_to_destination(t->x, t->y, g_party_map_x & 0xff, g_party_map_y & 0xff);
    return move_toward(t, 1);
}

/* Runs away from the party (or the last target), sometimes home. Fleeing ends after a delay without sight. */
static int16_t flee(GroupTurn *t)
{
    t->visible_distance = group_get_distance_to_visible_party(t->group, -1, t->x, t->y);
    int16_t from_x, from_y;
    if (!t->visible_distance) {
        if (--t->active->delay_fleeing == 0) {
            t->moved = 0;
            group_set_behavior(t->group, BEHAVIOR_WANDER);
            turn_and_finish(t, 0);
            return 0;
        }
        if (random_bit()) {
            if (!group_get_first_possible_movement_direction_ordinal(t->info, t->x, t->y, 0) &&
                abs16(t->x - g_party_map_x) + abs16(t->y - g_party_map_y) < 2) {
                t->moved = 0;
                group_set_behavior(t->group, BEHAVIOR_WANDER);
                turn_and_finish(t, 0);
                return 0;
            }
            t->direction = group_get_directions_to_destination(t->x, t->y, t->active->home_x, t->active->home_y);
            return move_toward(t, 1);
        }
        from_x = t->active->target_x;
        from_y = t->active->target_y;
    } else {
        set_target_to_party(t->active);
        from_x = g_party_map_x & 0xff;
        from_y = g_party_map_y & 0xff;
    }
    t->direction = (group_get_directions_to_destination(t->x, t->y, from_x, from_y) + 2) & 3;
    g_group_secondary_direction = (g_group_secondary_direction + 2) & 3;
    t->movement_ticks -= t->movement_ticks >> 2;
    return move_toward(t, 1);
}

/* The party is in attack range, in line, and the creature can attack. */
static int16_t can_start_attack(const GroupTurn *t)
{
    return t->visible_distance <= creature_attack_range(t->info) && t->info->attack_ticks != CREATURE_NO_ATTACK &&
           (!t->distance_x || !t->distance_y);
}

/* Events 29-31: reactions to danger, a projectile hit, or the party next to the group. */
static void process_reaction(GroupTurn *t, uint16_t behavior)
{
    switch (t->event_type) {
    case EVENT_GROUP_REACTION_HIT_BY_PROJECTILE: {
        t->event_type = REACTION_HIT_BY_PROJECTILE;
        if (behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE)
            return;
        int16_t useless = behavior == BEHAVIOR_USELESS_3 || behavior == BEHAVIOR_USELESS_2;
        if (useless || random_2bits()) {
            if (!group_get_distance_to_visible_party(t->group, -1, t->x, t->y)) {
                turn_and_finish(t, 0);
                return;
            }
            if (useless || random_2bits())
                return;
        }
        random_move(t);
        return;
    }
    case EVENT_GROUP_REACTION_PARTY_IS_ADJACENT:
        t->event_type = REACTION_PARTY_IS_ADJACENT;
        if (t->info->attack_ticks == CREATURE_NO_ATTACK || behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE) {
            set_target_to_party(t->active);
            return;
        }
        group_delete_events(t->x, t->y);
        set_behavior_attack(t);
        return;
    default:
        t->event_type = REACTION_DANGER_ON_SQUARE;
        t->was_attacking = behavior == BEHAVIOR_ATTACK;
        random_move(t);
        return;
    }
}

/* Events 32-36: the aspect of the group (32) or of one creature changes; a group that sees the party may react. */
static void process_aspect_event(GroupTurn *t, uint16_t behavior)
{
    t->next.type = (uint8_t)(t->event_type + EVENT_BEHAVIOR_TO_ASPECT);
    if (group_get_distance_to_visible_party(t->group, -1, t->x, t->y)) {
        int16_t attacks_or_flees = behavior == BEHAVIOR_ATTACK || behavior == BEHAVIOR_FLEE;
        if (!attacks_or_flees && t->info->attack_ticks != CREATURE_NO_ATTACK &&
            abs16(g_party_map_x - t->x) + abs16(g_party_map_y - t->y) <= 1) {
            set_behavior_attack(t);
            return;
        }
        if (!attacks_or_flees && (behavior == BEHAVIOR_WANDER || behavior == BEHAVIOR_USELESS_3)) {
            set_behavior_approach(t);
            return;
        }
        set_target_to_party(t->active);
    }
    if (behavior == BEHAVIOR_ATTACK) {
        /* For event 32 (all creatures) the original reads the byte before aspect[0] (home y, always below 128). */
        int16_t creature = t->event_type - EVENT_UPDATE_ASPECT_CREATURE_0;
        t->aspect_update_time = group_get_creature_aspect_update_time(t->active, creature, creature >= 0 && (t->active->aspect[creature] & ASPECT_ATTACKING));
        add_event_with_aspect(t);
        return;
    }
    if (t->distance_x > 3 || t->distance_y > 3) {
        t->aspect_update_time = g_game_time + creature_ticks_non_attack_aspect(t->info);
        add_event_with_aspect(t);
        return;
    }
    if (!t->aspect_update_time)
        t->aspect_update_time = group_get_creature_aspect_update_time(t->active, -1, 0);
    add_event_with_aspect(t);
}

/* Event 37: the behavior of the whole group. */
static void process_group_behavior(GroupTurn *t, uint16_t behavior)
{
    switch (behavior) {
    case BEHAVIOR_WANDER:
    case BEHAVIOR_USELESS_2:
    case BEHAVIOR_USELESS_3:
        t->visible_distance = group_get_distance_to_visible_party(t->group, -1, t->x, t->y);
        if (!t->visible_distance) {
            if (behavior != BEHAVIOR_WANDER) {
                add_wander_event(t);
                return;
            }
            int16_t ordinal = group_get_smelled_party_direction_ordinal(t->info, t->x, t->y);
            if (ordinal) {
                t->direction = ordinal - 1;
                move_toward(t, 0);
            } else if (!random_bit()) {
                after_move(t, 0);
            } else {
                random_move(t);
            }
            return;
        }
        if (can_start_attack(t))
            set_behavior_attack(t);
        else if (behavior != BEHAVIOR_USELESS_2)
            set_behavior_approach(t);
        else
            add_wander_event(t);
        return;
    case BEHAVIOR_APPROACH:
        t->visible_distance = group_get_distance_to_visible_party(t->group, -1, t->x, t->y);
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
   Returns 1 when all creatures died. */
static int16_t shift_single_creature(GroupTurn *t)
{
    uint16_t random = random16();
    if (t->last_creature_index || t->size == CREATURE_SIZE_FULL || (random & 0xc0))
        return 0;
    uint16_t cell = random;
    if (t->active->cells != CELLS_CENTERED) {
        if (!(random & 0x38))
            cell = ((random & 1 ? 1 : -1) + (t->active->cells & 3)) & 3;
        else
            t->active->cells = CELLS_CENTERED;
    }
    if ((cell & 0x38) || t->visible_distance == 1 || t->size != CREATURE_SIZE_QUARTER)
        return 0;
    if (projectile_get_impact_count(IMPACT_CREATURE, t->x, t->y, t->active->cells) && g_creature_damage_outcome == OUTCOME_ALL_CREATURES_KILLED)
        return 1;
    t->active->cells = (uint8_t)(cell & 3);
    return 0;
}

/* A creature steps to another cell of the square, toward the front. Returns 1 when all creatures died. */
static int16_t move_creature_to_front(GroupTurn *t, int16_t creature)
{
    uint16_t position = t->active->cells >> (2 * creature), cell = position & 3;
    if (!t->last_creature_index && random_bit()) {
        t->active->cells = CELLS_CENTERED;
        return 0;
    }
    cell = ((position & 1) == (uint16_t)(t->direction & 1) ? cell - 1 : cell + 1) & 3;
    if (group_get_creature_ordinal_in_cell(t->group, cell)) {
        if (!random_bit())
            return 0;
        cell = (cell + 2) & 3;
        if (group_get_creature_ordinal_in_cell(t->group, cell))
            return 0;
    }
    if (projectile_get_impact_count(IMPACT_CREATURE, t->x, t->y, t->active->cells) && g_creature_damage_outcome == OUTCOME_ALL_CREATURES_KILLED)
        return 1;
    if (g_creature_damage_outcome != OUTCOME_SOME_CREATURES_KILLED)
        t->active->cells = (uint8_t)group_set_cell_value(t->active->cells, (uint16_t)creature, cell);
    return 0;
}

/* Events 38-41: the behavior of one creature of an attacking group: turn, step to the front, or attack. */
static void process_creature_behavior(GroupTurn *t)
{
    int16_t creature = t->event_type - EVENT_UPDATE_BEHAVIOR_CREATURE_0;
    ActiveGroup *active = t->active;
    if (active->aspect[creature] & ASPECT_ATTACKING) {
        t->aspect_update_time = group_get_creature_aspect_update_time(active, creature, 0);
        t->next.map_time += (uint16_t)(t->info->attack_ticks + random_2bits() - 1);
        if (t->info->attack_ticks > 15)
            t->next.map_time += (uint16_t)((random16() & 7) - 2);
        t->next.type = (uint8_t)t->event_type;
        add_event_with_aspect(t);
        return;
    }
    if ((int16_t)t->last_creature_index < creature)
        return;
    t->direction = g_current_group_primary_direction_to_party;
    t->visible_distance = group_get_distance_to_visible_party(t->group, creature, t->x, t->y);
    if (t->visible_distance)
        set_target_to_party(active);
    if (shift_single_creature(t))
        return;
    if (!t->visible_distance || (!(t->info->attributes & CREATURE_SIDE_ATTACK) && cell_of(active->directions, creature) != (uint16_t)t->direction)) {
        if (!group_get_distance_to_visible_party(t->group, -1, t->x, t->y)) {
            group_set_behavior(t->group, BEHAVIOR_APPROACH);
            if (t->last_creature_index)
                group_stop_attacking(active, t->x, t->y);
            approach_target(t);
            return;
        }
        set_target_to_party(active);
        group_set_direction(active, t->direction, creature, t->last_creature_index && t->size == CREATURE_SIZE_HALF);
        t->next.map_time += 2;
        t->aspect_update_time = t->next.map_time & EVENT_TIME_MASK;
        t->next.type = (uint8_t)t->event_type;
        add_event_with_aspect(t);
        return;
    }
    int16_t range = (int16_t)creature_attack_range(t->info);
    if (range < (int16_t)t->visible_distance ||
        (t->distance_x && t->distance_y && (range < 2 || (random16() & 7))) ||
        (random16() & 15) + 1 < (uint16_t)range) {
        group_set_behavior(t->group, BEHAVIOR_APPROACH);
        if (t->last_creature_index)
            group_stop_attacking(active, t->x, t->y);
        approach_party(t);
        return;
    }
    uint16_t attributes = t->info->attributes;
    int16_t attack_now = range != 1 || ((attributes & CREATURE_PREFER_BACK_ROW) && random_2bits() && (attributes & CREATURE_ATTACK_ANY_CHAMPION)) ||
                         t->size != CREATURE_SIZE_QUARTER || active->cells == CELLS_CENTERED;
    if (!attack_now) {
        uint16_t cell = cell_of(active->cells, creature);
        attack_now = cell == (uint16_t)t->direction || cell == (uint16_t)((t->direction + 1) & 3);
    }
    if (attack_now) {
        int16_t attacking = group_is_creature_attacking(t->group, t->x, t->y, creature);
        t->aspect_update_time = group_get_creature_aspect_update_time(active, creature, attacking);
        t->next.map_time += random_bit() + creature_ticks_after_attack(t->info);
        t->next.type = (uint8_t)t->event_type;
        add_event_with_aspect(t);
        return;
    }
    if (move_creature_to_front(t, creature))
        return;
    t->next.map_time += max16(1, (int16_t)(random_bit() + (t->info->movement_ticks >> 1)));
    t->next.type = (uint8_t)t->event_type;
    if (!t->aspect_update_time)
        t->aspect_update_time = group_get_creature_aspect_update_time(active, -1, 0);
    add_event_with_aspect(t);
}

/* Events 32-41 and the reactions -1..-3 (events 29-31 later) of the group on a square. Off the party map a group
   only wanders. Lord Chaos ignores Freeze Life. */
void group_process_events_29_to_41(int16_t x, int16_t y, int16_t event_type, uint16_t ticks)
{
    if (g_current_map_index != g_party_map_index && event_type != EVENT_UPDATE_BEHAVIOR_GROUP && event_type != EVENT_UPDATE_ASPECT_GROUP &&
        event_type != EVENT_UPDATE_BEHAVIOR_CREATURE_0 && event_type != EVENT_UPDATE_ASPECT_CREATURE_0)
        return;
    uint16_t group = group_get_thing(x, y);
    if (group == THING_ENDOFLIST)
        return;
    GroupTurn turn = {.x = x, .y = y, .event_type = event_type, .ticks = ticks, .group = group, .info = group_info(group)};
    GroupTurn *t = &turn;
    t->next = (TimelineEvent){
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time),
        .priority = (uint8_t)(255 - t->info->movement_ticks), .b = EVENT_SQUARE(x, y),
    };
    if (g_current_map_index != g_party_map_index) {
        int16_t direction = random_2bits();
        if (group_is_movement_possible(t->info, x, y, (uint16_t)direction, 0)) {
            int16_t target_x = x, target_y = y;
            step(direction, 1, &target_x, &target_y);
            if (move_get_move_result(group, x, y, target_x, target_y))
                return;
            t->next.b = EVENT_SQUARE(g_move_result_map_x, g_move_result_map_y);
        }
        t->next.type = EVENT_UPDATE_BEHAVIOR_GROUP;
        t->next.map_time += max16((int16_t)(abs16(g_current_map_index - g_party_map_index) << 4), (int16_t)(t->info->movement_ticks * 2));
        timeline_add_event(&t->next);
        return;
    }
    t->archenemy = (t->info->attributes & CREATURE_ARCHENEMY) != 0;
    if (t->archenemy) {
        if (g_game_won)
            return;
        flux_cage_count = 0;
        for (int16_t i = 0; i < 4; i++)
            flux_cages[i] = 0;
    }
    t->active = group_active(group);
    t->ticks_since_last_move = (int16_t)((uint8_t)g_game_time - t->active->last_move_time);
    if (t->ticks_since_last_move < 0)
        t->ticks_since_last_move += 256;
    t->movement_ticks = t->info->movement_ticks == CREATURE_IMMOBILE ? MOVEMENT_TICKS_OF_IMMOBILE_CREATURE : t->info->movement_ticks;
    if (g_party.freeze_life_ticks && !t->archenemy) {
        if (event_type < 0)
            return;
        t->next.type = (uint8_t)event_type;
        t->next.c = (uint16_t)((uint8_t)ticks << 8);
        t->next.map_time += FREEZE_LIFE_RETRY_TICKS;
        timeline_add_event(&t->next);
        return;
    }
    if (event_type < 0) {
        /* Bug fix: the original leaves byte 8 (the ticks) of this event uninitialized; the port sets 0. */
        t->next.type = (uint8_t)(event_type + EVENT_UPDATE_ASPECT_GROUP);
        int16_t delay = (int16_t)(((t->movement_ticks + 2) >> 2) - t->ticks_since_last_move);
        if (event_type == REACTION_PARTY_IS_ADJACENT || delay < 1)
            delay = 1;
        t->next.map_time += delay;
        timeline_add_event(&t->next);
        return;
    }
    uint16_t behavior = group_behavior(group);
    t->last_creature_index = group_last_creature_index(group);
    t->size = creature_size(t->info);
    t->distance_x = abs16(x - g_party_map_x);
    t->distance_y = abs16(y - g_party_map_y);
    g_current_group_map_x = x;
    g_current_group_map_y = y;
    g_current_group_thing = group;
    for (int16_t i = 0; i < 4; i++)
        movement_tested_directions[i] = 0;
    g_current_group_distance_to_party = (uint16_t)get_square_distance(x, y, g_party_map_x, g_party_map_y);
    g_current_group_primary_direction_to_party = group_get_directions_to_destination(x, y, g_party_map_x, g_party_map_y);
    g_current_group_secondary_direction_to_party = g_group_secondary_direction;
    t->ticks_delay_next_event = 1;
    if (event_type < EVENT_UPDATE_ASPECT_GROUP) {
        process_reaction(t, behavior);
        return;
    }
    if (event_type < EVENT_UPDATE_BEHAVIOR_GROUP) {
        process_aspect_event(t, behavior);
        return;
    }
    t->ticks_delay_next_event = 0;
    if (ticks)
        t->aspect_update_time = g_game_time;
    if (event_type == EVENT_UPDATE_BEHAVIOR_GROUP) {
        process_group_behavior(t, behavior);
        return;
    }
    if (behavior == BEHAVIOR_FLEE) {
        if (t->last_creature_index)
            group_stop_attacking(t->active, x, y);
        flee(t);
        return;
    }
    process_creature_behavior(t);
}

/* A teleporter turns the creatures (relative or absolute rotation). Quarter-size creatures keep their cells with an
   absolute rotation. Returns the active entry + 2 for an attacking group on the party map, else 1. */
static int16_t move_get_teleporter_rotated_group_result(uint16_t teleporter, uint16_t group, int16_t map)
{
    uint16_t rotation = teleporter_rotation(teleporter);
    int16_t absolute = teleporter_is_rotation_absolute(teleporter);
    uint16_t old_directions = get_group_directions(group, map);
    uint16_t directions = absolute ? rotation : (old_directions + rotation) & 3;
    uint16_t cells = get_group_cells(group, map);
    if (cells != CELLS_CENTERED) {
        int16_t quarter = creature_size(group_info(group)) == CREATURE_SIZE_QUARTER;
        uint16_t cell_turn = (directions + 4 - old_directions) & 3, old_cells = cells;
        for (int16_t i = 0; i <= group_last_creature_index(group); i++) {
            directions = group_set_cell_value(directions, (uint16_t)i, absolute ? rotation : (old_directions + rotation) & 3);
            if (quarter)
                cell_turn = absolute ? 0 : rotation;
            if (cell_turn)
                cells = group_set_cell_value(cells, (uint16_t)i, (cell_turn + old_cells) & 3);
            old_directions >>= 2;
            old_cells >>= 2;
        }
    }
    set_group_directions(group, (uint8_t)directions, map);
    set_group_cells(group, (uint8_t)cells, map);
    if (map == g_party_map_index && group_behavior(group) == BEHAVIOR_ATTACK)
        return group_cells_byte(group) + 2;
    return 1;
}

/* move_get_move_result for a group. Teleporters, pits (20 damage), and stairs move it on. A group that dies or
   reaches a map that does not allow its type is deleted (returns 1). An occupied destination makes the group wait
   for event 60/61 (returns 1). Returns 0, 1, or the active entry + 2 (see the teleporter rotation). */
int16_t group_get_move_result(uint16_t group, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y)
{
    int16_t src_map = g_current_map_index, map = src_map;
    int16_t levitating = (get_creature_attributes(group) & CREATURE_LEVITATES) != 0;
    uint16_t src_cell = THING_CELL(group);
    int16_t result = 0, audible = 0, removed_from_party_map = 0;
    if (src_x >= 0 && g_current_map_index == g_party_map_index && move_is_killed_by_projectile_impact(src_x, src_y, dst_x, dst_y, group))
        return 1;
    if (dst_x >= 0) {
        int16_t killed = 0;
        removed_from_party_map = g_current_map_index == g_party_map_index && src_x >= 0;
        for (int16_t n = 99; n; n--) {
            uint8_t square = g_current_map_data[dst_x][dst_y];
            int16_t type = SQUARE_TYPE(square);
            if (type == SQUARE_TELEPORTER) {
                if (!(square & TELEPORTER_OPEN))
                    break;
                uint16_t teleporter = get_square_first_thing(dst_x, dst_y);
                if (!(teleporter_scope(teleporter) & TELEPORTER_SCOPE_CREATURES))
                    break;
                int16_t points_to_itself = teleporter_target_x(teleporter) == dst_x && teleporter_target_y(teleporter) == dst_y && teleporter_target_map(teleporter) == map;
                dst_x = teleporter_target_x(teleporter);
                dst_y = teleporter_target_y(teleporter);
                audible = teleporter_is_audible(teleporter);
                map = teleporter_target_map(teleporter);
                set_current_map(map);
                if (audible)
                    sound_request_play(SOUND_BUZZ, dst_x, dst_y, SOUND_MODE_AFTER_TICK);
                result = move_get_teleporter_rotated_group_result(teleporter, group, src_map);
                if (points_to_itself)
                    break;
            } else if (type == SQUARE_PIT && !levitating && (square & PIT_OPEN) && !(square & PIT_IMAGINARY)) {
                map = get_location_after_level_change(map, LEVEL_DOWN, &dst_x, &dst_y);
                set_current_map(src_map);
                int16_t outcome = group_get_damage_all_creatures_outcome(group, src_x, src_y, FALL_DAMAGE, 0);
                set_current_map(map);
                killed = outcome == OUTCOME_ALL_CREATURES_KILLED;
                if (killed)
                    break;
                if (outcome == OUTCOME_SOME_CREATURES_KILLED)
                    group_drop_moving_creature_fixed_possessions(group, dst_x, dst_y);
            } else if (type == SQUARE_STAIRS) {
                if (!(square & STAIRS_UP)) {
                    map = get_location_after_level_change(map, LEVEL_DOWN, &dst_x, &dst_y);
                    set_current_map(map);
                }
                int16_t exit = get_stairs_exit_direction(dst_x, dst_y);
                step(exit, 1, &dst_x, &dst_y);
                int16_t start_direction = (exit + 2) & 3;
                group = THING_WITH_CELL(group, start_direction + ((((THING_CELL(group) - start_direction) + 1) & 2) >> 1));
            } else {
                break;
            }
        }
        if (killed || !is_creature_allowed_on_map(group, map)) {
            group_drop_moving_creature_fixed_possessions(group, dst_x, dst_y);
            group_drop_group_possessions(dst_x, dst_y, group, SOUND_MODE_ONE_TICK_LATER);
            set_current_map(src_map);
            if (src_x >= 0)
                group_delete(src_x, src_y);
            return 1;
        }
        g_move_result_cell = THING_CELL(group);
        int16_t same_square = map == src_map && dst_x == src_x && dst_y == src_y;
        g_move_result_map_x = dst_x;
        g_move_result_map_y = dst_y;
        g_move_result_map_index = (uint16_t)map;
        if (!same_square) {
            if (map != src_map)
                set_current_map(src_map);
        } else if (g_move_result_cell == src_cell) {
            return 0;
        }
    }
    if (src_x >= 0) {
        if (!levitating)
            sensor_process_thing_addition_or_removal(src_x, src_y, group, is_party_on(src_x, src_y), 0);
        else
            unlink_thing_from_list(group, 0, src_x, src_y);
    }
    if (dst_x < 0)
        return 0;
    set_current_map(map);
    uint8_t active_index = group_cells_byte(group);
    if (is_party_on(dst_x, dst_y) || group_get_thing(dst_x, dst_y) != THING_ENDOFLIST) {
        set_current_map(src_map);
        if (src_x >= 0)
            group_delete_events(src_x, src_y);
        if (removed_from_party_map)
            group_remove_active_group(active_index);
        move_create_event_move_group(group, dst_x, dst_y, map, audible);
        return 1;
    }
    sound_request_play(creature_movement_sound(group_creature_type(group)), dst_x, dst_y, SOUND_MODE_AFTER_TICK);
    if (removed_from_party_map && map != g_party_map_index) {
        group_remove_active_group(active_index);
        result = 1;
    } else if (!removed_from_party_map && map == g_party_map_index) {
        group_add_active_group(group, dst_x, dst_y);
        result = 1;
    }
    if (!levitating)
        sensor_process_thing_addition_or_removal(dst_x, dst_y, group, 0, 1);
    else
        link_thing_to_list(group, 0, dst_x, dst_y);
    if (result || src_x < 0)
        group_start_wandering(dst_x, dst_y);
    set_current_map(src_map);
    if (src_x >= 0) {
        if (result >= 2)
            group_stop_attacking(&g_active_groups[result - 2], src_x, src_y);
        else if (result == 1)
            group_delete_events(src_x, src_y);
    }
    return result;
}

/* The group waits off the map and tries to enter the square 5 ticks later (event 61 plays the teleporter sound). */
void move_create_event_move_group(uint16_t group, int16_t x, int16_t y, int16_t map, int16_t audible)
{
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(map, g_game_time + MOVE_GROUP_RETRY_TICKS),
        .type = audible ? EVENT_MOVE_GROUP_AUDIBLE : EVENT_MOVE_GROUP_SILENT, .b = EVENT_SQUARE(x, y), .c = group,
    };
    timeline_add_event(&event);
}

/* A waiting group enters its square when the square is free. Lord Chaos may take an adjacent open square. */
void timeline_process_event60_61_move_group(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event), retried = 0;
    for (;;) {
        if (!is_party_on(x, y) && group_get_thing(x, y) == THING_ENDOFLIST) {
            if (event->type == EVENT_MOVE_GROUP_AUDIBLE)
                sound_request_play(SOUND_BUZZ, x, y, SOUND_MODE_AFTER_TICK);
            move_get_move_result(event->c, -1, 0, x, y);
            return;
        }
        if (retried)
            break;
        retried = 1;
        if (group_creature_type(event->c) != CREATURE_LORD_CHAOS || random_2bits())
            break;
        switch (random_2bits()) {
        case 0: x--; break;
        case 1: x++; break;
        case 2: y--; break;
        default: y++; break;
        }
        if (!is_square_open(x, y))
            break;
    }
    event->map_time += MOVE_GROUP_RETRY_TICKS;
    timeline_add_event(event);
}
