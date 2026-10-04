#include "game.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "random.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIRECTION_TO_STEP_EAST 0x101038
#define DIRECTION_TO_STEP_NORTH 0x101040

Party g_party;
Champion g_champions[4];
uint16_t g_party_champion_count;
int16_t g_leader_index = -1, g_magic_caster_champion_index = -1;
uint16_t g_leader_hand_object = 0xffff;
int16_t g_party_is_sleeping, g_party_dead;
uint32_t g_game_time, g_game_id;
int32_t g_last_creature_attack_time, g_last_party_movement_time, g_watchdog_time;
int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
int16_t g_last_projectile_disabled_movement_direction;

TimelineEvent *g_timeline_events;
uint16_t *g_timeline;
uint16_t g_timeline_count, g_event_high_water;
int16_t g_first_unused_event_index;

ActiveGroup *g_active_groups;
uint16_t g_active_group_count, g_max_active_group_count;

static void fatal_error(int16_t code)
{
    fprintf(stderr, "System Error %d\n", code);
    exit(1);
}

/* load_game (new game) and the end of load_dungeon. load_dungeon must run first. */
void game_new(void)
{
    memset(&g_party, 0, sizeof g_party);
    memset(g_champions, 0, sizeof g_champions);
    memset(g_champion_pending_damage, 0, sizeof g_champion_pending_damage);
    memset(g_champion_pending_wounds, 0, sizeof g_champion_pending_wounds);
    g_party_champion_count = 0;
    g_leader_index = g_magic_caster_champion_index = -1;
    g_leader_hand_object = 0xffff;
    g_party_is_sleeping = g_party_dead = 0;
    g_game_time = 0;
    g_last_creature_attack_time = g_last_party_movement_time = 0;
    g_disabled_movement_ticks = g_projectile_disabled_movement_ticks = 0;
    g_last_projectile_disabled_movement_direction = 0;
    g_active_group_count = 0;
    uint32_t r = random16();
    g_game_id = r * random16();
    timeline_init();
    group_init_active_groups();
}

int16_t min16(int16_t a, int16_t b) { return b <= a ? b : a; }
int16_t max16(int16_t a, int16_t b) { return a <= b ? b : a; }

int16_t bounded_value(int16_t min, int16_t value, int16_t max)
{
    return value <= min ? min : value > max ? max : value;
}

/* asr.l of the 32-bit mulu product: the sign bit is the product's bit 31. */
int32_t scale_value(uint16_t a, int16_t shift, uint16_t b)
{
    int32_t product = (int32_t)((uint32_t)a * b);
    shift &= 63;
    return shift > 31 ? (product < 0 ? -1 : 0) : product >> shift;
}

void update_map_coordinates_after_relative_movement(int16_t direction, int16_t steps_forward, int16_t steps_right, int16_t *map_x, int16_t *map_y)
{
    int16_t right = NORMALIZE(direction + 1);
    *map_x = (int16_t)(*map_x + steps_forward * kaos_s16(DIRECTION_TO_STEP_EAST + direction * 2) + steps_right * kaos_s16(DIRECTION_TO_STEP_EAST + right * 2));
    *map_y = (int16_t)(*map_y + steps_forward * kaos_s16(DIRECTION_TO_STEP_NORTH + direction * 2) + steps_right * kaos_s16(DIRECTION_TO_STEP_NORTH + right * 2));
}

/* New game only. A free slot keeps the next free index in map_time; -1 ends the list. */
void timeline_init(void)
{
    free(g_timeline_events);
    free(g_timeline);
    g_timeline_events = calloc(g_event_max_count, sizeof *g_timeline_events);
    g_timeline = calloc(g_event_max_count, sizeof *g_timeline);
    if (g_event_max_count <= 0 || !g_timeline_events || !g_timeline) fatal_error(0x46);
    for (uint16_t i = 0; i < g_event_max_count; i++)
        g_timeline_events[i].map_time = i + 1;
    g_timeline_events[g_event_max_count - 1].map_time = 0xffff;
    g_event_high_water = g_timeline_count = 0;
    g_first_unused_event_index = 0;
    timeline_add_watchdog_event();
}

/* Earlier time first, then higher type and priority, then the lower slot. */
int16_t timeline_is_event_a_before_b(const TimelineEvent *a, const TimelineEvent *b)
{
    uint32_t time_a = a->map_time & 0xffffff, time_b = b->map_time & 0xffffff;
    uint16_t key_a = a->type << 8 | a->priority, key_b = b->type << 8 | b->priority;
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
    fatal_error(0x46);
    return 0;
}

#define BEFORE(i, j) timeline_is_event_a_before_b(&g_timeline_events[i], &g_timeline_events[j])

void timeline_fix_chronology(uint16_t timeline_index)
{
    uint16_t count = g_timeline_count, i = timeline_index, slot = g_timeline[i];
    int moved = 0;
    if (count == 1)
        return;
    while (i) {
        uint16_t parent = (uint16_t)(i - 1) >> 1;
        if (!BEFORE(slot, g_timeline[parent]))
            break;
        g_timeline[i] = g_timeline[parent];
        i = parent;
        moved = 1;
    }
    if (!moved)
        while (i <= (uint16_t)(count - 2) >> 1) {
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
    g_timeline_events[event_index].map_time = (uint16_t)g_first_unused_event_index;
    g_first_unused_event_index = event_index;
    if (--g_timeline_count) {
        uint16_t i = timeline_get_index(event_index);
        if (i != g_timeline_count) {
            g_timeline[i] = g_timeline[g_timeline_count];
            timeline_fix_chronology(i);
        }
    }
}

#define IS_SQUARE_EVENT(type) ((type) >= 5 && (type) <= 10)
#define SAME_PLACE(e, f) ((e)->map_time == (f)->map_time && (e)->b == (f)->b)
#define SET_EFFECT(e, v) ((e)->c = ((e)->c & 0xff00) | (uint8_t)(v))

/* Returns the index of the event that absorbs the new one, or -1. Can change or delete events. */
int16_t timeline_find_mergeable_event(TimelineEvent *event)
{
    TimelineEvent *e = g_timeline_events;
    for (int16_t i = 0; i < g_event_high_water; i++, e++) {
        if (IS_SQUARE_EVENT(event->type)) {
            if (IS_SQUARE_EVENT(e->type)) {
                if (SAME_PLACE(e, event) && (e->type != 6 || e->c >> 8 == event->c >> 8)) {
                    SET_EFFECT(e, event->c);
                    return i;
                }
            } else if (e->type == 1 && SAME_PLACE(e, event)) {
                if ((uint8_t)event->c == 2)
                    SET_EFFECT(event, 1 - (uint8_t)e->c);
                timeline_delete_event(i);
                return -1;
            }
        } else if (event->type == 1) {
            if (SAME_PLACE(e, event)) {
                if (e->type == 10) {
                    if ((uint8_t)e->c == 2)
                        SET_EFFECT(e, 1 - (uint8_t)event->c);
                    return i;
                }
                if (e->type == 1) {
                    SET_EFFECT(e, event->c);
                    return i;
                }
            }
        } else if (event->type == 2) {
            if (e->b == event->b && e->map_time >> 24 == event->map_time >> 24 && (e->type == 1 || e->type == 10))
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
    if (g_timeline_count == (uint16_t)g_event_max_count)
        fatal_error(0x2d);
    int16_t index = timeline_find_mergeable_event(event);
    if (index >= 0)
        return index;
    index = g_first_unused_event_index;
    g_first_unused_event_index = (int16_t)g_timeline_events[index].map_time;
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

int16_t timeline_is_first_event_expired(void)
{
    return g_timeline_count && (g_timeline_events[g_timeline[0]].map_time & 0xffffff) <= g_game_time;
}

/* The original leaves priority, b, and c as stack garbage. */
void timeline_add_watchdog_event(void)
{
    TimelineEvent event = {.map_time = g_watchdog_time = g_game_time + 300, .type = 53};
    timeline_add_event(&event);
}

/* New game only: 110 entries. */
void group_init_active_groups(void)
{
    g_max_active_group_count = 110;
    free(g_active_groups);
    g_active_groups = malloc(g_max_active_group_count * sizeof *g_active_groups);
    if (!g_active_groups) fatal_error(0x46);
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        g_active_groups[i].group_index = -1;
}

int16_t champion_get_stamina_adjusted_value(const Champion *champion, int16_t value)
{
    int16_t half = champion->maximum_stamina >> 1;
    if (champion->current_stamina < half)
        value = (int16_t)((value >> 1) + (int16_t)((int32_t)value * champion->current_stamina / half));
    return value;
}

uint16_t champion_get_statistic_adjusted_attack(const Champion *champion, uint16_t statistic, uint16_t attack)
{
    int16_t factor = 170 - champion->statistics[statistic][STAT_CURRENT];
    return factor < 16 ? attack >> 3 : (uint16_t)scale_value(attack, 7, factor);
}

void adjust_statistic_current_value(Champion *champion, uint16_t statistic, int16_t delta)
{
    uint8_t *s = champion->statistics[statistic];
    if (delta < 0) {
        delta = max16(delta, s[STAT_MINIMUM] - s[STAT_CURRENT]);
    } else {
        if (s[STAT_CURRENT] > 120)
            delta = (s[STAT_CURRENT] > 150 ? delta >> 2 : delta >> 1) + 1;
        delta = min16(delta, 170 - s[STAT_CURRENT]);
    }
    s[STAT_CURRENT] += (uint8_t)delta;
}

/* The original reads the luck bytes as signed. A lucky first roll does not change luck. */
int16_t champion_is_lucky(Champion *champion, uint16_t percentage)
{
    uint8_t *luck = champion->statistics[STAT_LUCK];
    if (random_bit() && percentage < random16() % 100)
        return 1;
    int16_t lucky = (int8_t)luck[STAT_CURRENT] > 0 && percentage < random16() % (uint16_t)(luck[STAT_CURRENT] * 2);
    luck[STAT_CURRENT] = (uint8_t)bounded_value((int8_t)luck[STAT_MINIMUM], (int8_t)luck[STAT_CURRENT] + (lucky ? -2 : 2), (int8_t)luck[STAT_MAXIMUM]);
    return lucky;
}
