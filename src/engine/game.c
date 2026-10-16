/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/game.h"
#include "data/adventure.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/random.h"
#include "engine/sensor.h"
#include "engine/timeline_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSTEM_ERROR_TIMELINE_FULL 45
#define SYSTEM_ERROR_MEMORY        70     /* allocation failed, or an event is missing from the timeline */
#define FREE_LIST_END              0xffff /* map_time of the last free event slot */
#define STAT_HIGH                  120    /* statistic gains above this value are halved */
#define STAT_VERY_HIGH             150    /* statistic gains above this value are quartered */
#define ATTACK_FACTOR_MIN          16     /* below this, the statistic factor divides the attack by 8 */
#define ATTACK_FACTOR_SHIFT        7
#define LUCK_STEP                  2     /* luck change after each luck test */
#define PERCENT_RANGE              100   /* a luck test draws random16() % 100 */
#define ATTACK_LOW_FACTOR_SHIFT    3     /* a statistic factor below ATTACK_FACTOR_MIN divides the attack by 8 */
#define EVENT_KEY_TYPE_SHIFT       8     /* the sort key of an event: type in the high byte, priority in the low byte */
#define TORCH_BURN_INTERVAL_MASK   0x1ff /* torches lose one charge every 512 ticks */
#define TIME_EFFECTS_MASK          0x3f  /* champion time effects every 64 ticks */
#define TIME_EFFECTS_SLEEPING_MASK 0xf   /* every 16 ticks while the party sleeps */
#define FUSE_MAGICAL_LIGHT         200
#define FUSE_SHIELD                100

Party g_party;
Champion g_champions[CHAMPION_MAX_COUNT];
uint16_t g_party_champion_count;
int16_t g_leader_index = -1;
int16_t g_magic_caster_champion_index = -1;
uint16_t g_leader_hand_object = THING_NONE;
bool g_party_is_sleeping, g_party_dead;
uint32_t g_game_time, g_game_id;
int32_t g_last_creature_attack_time, g_last_party_movement_time, g_watchdog_time;
int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
int16_t g_last_projectile_disabled_movement_direction;
uint16_t g_redraw;
int16_t g_game_mode = GAME_MODE_VANILLA;
bool g_cheat_light, g_cheat_immortal, g_cheat_food;

TimelineEvent *g_timeline_events;
uint16_t *g_timeline;
uint16_t g_timeline_count, g_event_high_water;
int16_t g_first_unused_event_index;

ActiveGroup *g_active_groups;
uint16_t g_active_group_count, g_max_active_group_count;

/* Stops the game with the "System Error" message of the original. */
static void fatal_error(int16_t code)
{
    fprintf(stderr, "System Error %d\n", code);
    exit(1);
}

void game_start_new(void)
{
    memset(&g_party, 0, sizeof g_party);
    memset(g_champions, 0, sizeof g_champions);
    memset(g_champion_pending_damage, 0, sizeof g_champion_pending_damage);
    memset(g_champion_pending_wounds, 0, sizeof g_champion_pending_wounds);
    g_party_champion_count = 0;
    g_leader_index = -1;
    g_magic_caster_champion_index = -1;
    g_leader_hand_object = THING_NONE;
    g_party_is_sleeping = false;
    g_party_dead = false;
    g_game_time = 0;
    g_last_creature_attack_time = 0;
    g_last_party_movement_time = 0;
    g_disabled_movement_ticks = 0;
    g_projectile_disabled_movement_ticks = 0;
    g_last_projectile_disabled_movement_direction = 0;
    g_redraw = 0;
    g_dead_champions = 0;
    g_active_group_count = 0;
    uint32_t r = random16();
    g_game_id = r * random16();
    timeline_init();
    game_init_active_groups();
}

void game_reset_session(void)
{
    g_candidate_champion_ordinal = 0;
    g_party_is_sleeping = false;
    g_game_won = false;
    g_cheat_light = false;
    g_cheat_immortal = false;
    g_cheat_food = false;
    memset(g_champion_skill_level_up, 0, sizeof g_champion_skill_level_up);
}

void game_set_mode(int16_t mode)
{
    g_game_mode = mode;
}

void game_toggle_cheat(int16_t cheat)
{
    bool *flag = cheat == CHEAT_LIGHT ? &g_cheat_light : cheat == CHEAT_IMMORTAL ? &g_cheat_immortal : &g_cheat_food;
    *flag = !*flag;
}

void game_start_fuse_sequence(void)
{
    g_game_won = true;
    g_party.magical_light_amount = FUSE_MAGICAL_LIGHT;
    g_party.shield_defense = FUSE_SHIELD;
    g_party.spell_shield_defense = FUSE_SHIELD;
    g_party.fire_shield_defense = FUSE_SHIELD;
}

void game_increment_time(void)
{
    g_game_time++;
}

void game_advance_time(void)
{
    game_increment_time();
    if (!(g_game_time & TORCH_BURN_INTERVAL_MASK))
        inventory_decrease_torches_light_power();
    if (g_party.freeze_life_ticks)
        g_party.freeze_life_ticks--;
}

void game_apply_tick_effects(void)
{
    if (!(g_game_time & (g_party_is_sleeping ? TIME_EFFECTS_SLEEPING_MASK : TIME_EFFECTS_MASK)))
        champion_apply_time_effects();
    if (g_disabled_movement_ticks)
        g_disabled_movement_ticks--;
    if (g_projectile_disabled_movement_ticks)
        g_projectile_disabled_movement_ticks--;
}

void game_update_map_coordinates_after_relative_movement(int16_t direction, int16_t steps_forward, int16_t steps_right,
                                                         int16_t *map_x, int16_t *map_y)
{
    int16_t right = turn_right(direction);
    *map_x = *map_x + steps_forward * tables_get_step_east(direction) + steps_right * tables_get_step_east(right);
    *map_y = *map_y + steps_forward * tables_get_step_north(direction) + steps_right * tables_get_step_north(right);
}

void timeline_init(void)
{
    free(g_timeline_events);
    free(g_timeline);
    g_timeline_events = calloc(g_event_max_count, sizeof *g_timeline_events);
    g_timeline = calloc(g_event_max_count, sizeof *g_timeline);
    if (g_event_max_count <= 0 || !g_timeline_events || !g_timeline)
        fatal_error(SYSTEM_ERROR_MEMORY);
    for (uint16_t i = 0; i < g_event_max_count; i++)
        g_timeline_events[i].map_time = i + 1;
    g_timeline_events[g_event_max_count - 1].map_time = FREE_LIST_END;
    g_event_high_water = 0;
    g_timeline_count = 0;
    g_first_unused_event_index = 0;
    timeline_add_watchdog_event();
}

void timeline_reset(void)
{
    int16_t last_free = -1;
    g_event_high_water = 0;
    g_first_unused_event_index = -1;
    for (int16_t i = 0; i < g_event_max_count; i++) {
        if (g_timeline_events[i].type) {
            g_event_high_water = i + 1;
            continue;
        }
        if (last_free == -1)
            g_first_unused_event_index = i;
        else
            g_timeline_events[last_free].map_time = i;
        g_timeline_events[i].map_time = FREE_LIST_END;
        last_free = i;
    }
}

bool timeline_is_event_a_before_b(const TimelineEvent *a, const TimelineEvent *b)
{
    uint32_t time_a = a->map_time & EVENT_TIME_MASK;
    uint32_t time_b = b->map_time & EVENT_TIME_MASK;
    uint16_t key_a = a->type << EVENT_KEY_TYPE_SHIFT | a->priority;
    uint16_t key_b = b->type << EVENT_KEY_TYPE_SHIFT | b->priority;
    if (time_a != time_b)
        return time_a < time_b;
    if (key_a != key_b)
        return key_a > key_b;
    return a <= b;
}

uint16_t timeline_get_index(uint16_t event_index)
{
    uint16_t i = 0;
    do {
        if (g_timeline[i] == event_index)
            return i;
    } while (++i < g_event_high_water);
    fatal_error(SYSTEM_ERROR_MEMORY);
    return 0;
}

#define BEFORE(i, j) timeline_is_event_a_before_b(&g_timeline_events[i], &g_timeline_events[j])

void timeline_fix_chronology(uint16_t timeline_index)
{
    uint16_t count = g_timeline_count;
    uint16_t i = timeline_index;
    uint16_t slot = g_timeline[i];
    bool moved = false;
    if (count == 1)
        return;
    while (i) {
        uint16_t parent = (i - 1) >> 1;
        if (!BEFORE(slot, g_timeline[parent]))
            break;
        g_timeline[i] = g_timeline[parent];
        i = parent;
        moved = true;
    }
    if (!moved)
        while (i <= (uint16_t)(count - 2) >> 1) { /* unsigned as in the original: no children below 2 entries */
            uint16_t child = 2 * i + 1;
            if (child + 1 < g_timeline_count && BEFORE(g_timeline[child + 1], g_timeline[child]))
                child++;
            if (!BEFORE(g_timeline[child], slot))
                break;
            g_timeline[i] = g_timeline[child];
            i = child;
        }
    g_timeline[i] = slot;
}

void timeline_delete_event(uint16_t event_index)
{
    g_timeline_events[event_index].type = 0;
    g_timeline_events[event_index].map_time = (uint16_t)g_first_unused_event_index; /* -1 becomes FREE_LIST_END */
    g_first_unused_event_index = event_index;
    if (--g_timeline_count) {
        uint16_t i = timeline_get_index(event_index);
        if (i != g_timeline_count) {
            g_timeline[i] = g_timeline[g_timeline_count];
            timeline_fix_chronology(i);
        }
    }
}

#define IS_SQUARE_EVENT(type)   ((type) >= EVENT_CORRIDOR && (type) <= EVENT_DOOR)
#define SAME_PLACE(e, f)        ((e)->map_time == (f)->map_time && (e)->b == (f)->b)
/* Set becomes clear and clear becomes set. */
#define OPPOSITE_EFFECT(effect) (EFFECT_CLEAR - (effect))

int16_t timeline_find_mergeable_event(TimelineEvent *event)
{
    TimelineEvent *e = g_timeline_events;
    for (int16_t i = 0; i < g_event_high_water; i++, e++) {
        if (IS_SQUARE_EVENT(event->type)) {
            if (IS_SQUARE_EVENT(e->type)) {
                if (SAME_PLACE(e, event) && (e->type != EVENT_WALL || event_cell(e) == event_cell(event))) {
                    event_set_effect(e, event->c);
                    return i;
                }
            } else if (e->type == EVENT_DOOR_ANIMATION && SAME_PLACE(e, event)) {
                if (event_effect(event) == EFFECT_TOGGLE)
                    event_set_effect(event, OPPOSITE_EFFECT(event_effect(e)));
                timeline_delete_event(i);
                return -1;
            }
        } else if (event->type == EVENT_DOOR_ANIMATION) {
            if (SAME_PLACE(e, event)) {
                if (e->type == EVENT_DOOR) {
                    if (event_effect(e) == EFFECT_TOGGLE)
                        event_set_effect(e, OPPOSITE_EFFECT(event_effect(event)));
                    return i;
                }
                if (e->type == EVENT_DOOR_ANIMATION) {
                    event_set_effect(e, event->c);
                    return i;
                }
            }
        } else if (event->type == EVENT_DOOR_DESTRUCTION) {
            if (e->b == event->b && EVENT_MAP(e) == EVENT_MAP(event) &&
                (e->type == EVENT_DOOR_ANIMATION || e->type == EVENT_DOOR))
                timeline_delete_event(i);
        } else {
            break;
        }
    }
    return -1;
}

int16_t timeline_add_event(TimelineEvent *event)
{
    if (!event->type)
        return -1;
    if (g_timeline_count == (uint16_t)g_event_max_count) /* unsigned compare as in the original */
        fatal_error(SYSTEM_ERROR_TIMELINE_FULL);
    int16_t index = timeline_find_mergeable_event(event);
    if (index >= 0)
        return index;
    index = g_first_unused_event_index;
    g_first_unused_event_index = g_timeline_events[index].map_time;
    g_timeline_events[index] = *event;
    if (g_event_high_water <= index)
        g_event_high_water = index + 1;
    g_timeline[g_timeline_count] = index;
    timeline_fix_chronology(g_timeline_count++);
    return index;
}

void timeline_extract_first_event(TimelineEvent *event)
{
    uint16_t index = g_timeline[0];
    *event = g_timeline_events[index];
    timeline_delete_event(index);
}

bool timeline_is_first_event_expired(void)
{
    return g_timeline_count && (g_timeline_events[g_timeline[0]].map_time & EVENT_TIME_MASK) <= g_game_time;
}

void timeline_add_watchdog_event(void)
{
    g_watchdog_time = g_game_time + WATCHDOG_INTERVAL;
    TimelineEvent event = {.map_time = g_watchdog_time, .type = EVENT_WATCHDOG};
    timeline_add_event(&event);
}

void game_init_active_groups(void)
{
    g_max_active_group_count = g_adventure == ADVENTURE_CSB ? CSB_ACTIVE_GROUP_MAX_COUNT : ACTIVE_GROUP_MAX_COUNT;
    free(g_active_groups);
    g_active_groups = malloc(g_max_active_group_count * sizeof *g_active_groups);
    if (!g_active_groups)
        fatal_error(SYSTEM_ERROR_MEMORY);
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        g_active_groups[i].group_index = -1;
}

int16_t game_get_stamina_adjusted_value(const Champion *champion, int16_t value)
{
    int16_t half = champion->maximum_stamina >> 1;
    if (champion->current_stamina < half)
        value = (value >> 1) + (int16_t)((int32_t)value * champion->current_stamina / half); /* 16-bit quotient */
    return value;
}

uint16_t game_get_statistic_adjusted_attack(const Champion *champion, uint16_t statistic, uint16_t attack)
{
    int16_t factor = STAT_LIMIT - champion->statistics[statistic][STAT_CURRENT];
    return factor < ATTACK_FACTOR_MIN ? attack >> ATTACK_LOW_FACTOR_SHIFT
                                      : scale_value(attack, ATTACK_FACTOR_SHIFT, factor);
}

void game_adjust_statistic_current_value(Champion *champion, uint16_t statistic, int16_t delta)
{
    uint8_t *s = champion->statistics[statistic];
    if (delta < 0) {
        delta = max16(delta, s[STAT_MINIMUM] - s[STAT_CURRENT]);
    } else {
        if (s[STAT_CURRENT] > STAT_HIGH)
            delta = (s[STAT_CURRENT] > STAT_VERY_HIGH ? delta >> 2 : delta >> 1) + 1;
        delta = min16(delta, STAT_LIMIT - s[STAT_CURRENT]);
    }
    s[STAT_CURRENT] += delta;
}

bool game_is_lucky(Champion *champion, uint16_t percentage)
{
    uint8_t *luck = champion->statistics[STAT_LUCK];
    if (random_bit() && percentage < random16() % PERCENT_RANGE)
        return true;
    bool lucky = (int8_t)luck[STAT_CURRENT] > 0 && percentage < random16() % (luck[STAT_CURRENT] * 2);
    int16_t change = lucky ? -LUCK_STEP : LUCK_STEP;
    luck[STAT_CURRENT] =
        bounded_value((int8_t)luck[STAT_MINIMUM], (int8_t)luck[STAT_CURRENT] + change, (int8_t)luck[STAT_MAXIMUM]);
    return lucky;
}
