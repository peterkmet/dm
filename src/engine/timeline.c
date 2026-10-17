/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/timeline.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/sensor.h"
#include <stdlib.h>

/* The system error of the original for a full timeline. */
#define SYSTEM_ERROR_TIMELINE_FULL 45
/* map_time of the last free event slot. */
#define FREE_LIST_END              0xffff
/* The sort key of an event: type in the high byte, priority in the low byte. */
#define EVENT_KEY_TYPE_SHIFT       8

Timeline g_timeline;

bool timeline_allocate(int16_t capacity)
{
    free(g_timeline.events);
    free(g_timeline.heap);
    g_timeline.events = calloc(capacity > 0 ? capacity : 0, sizeof *g_timeline.events);
    g_timeline.heap = calloc(capacity > 0 ? capacity : 0, sizeof *g_timeline.heap);
    if (capacity <= 0 || !g_timeline.events || !g_timeline.heap)
        return false;
    for (uint16_t i = 0; i < capacity; i++)
        g_timeline.events[i].map_time = i + 1;
    g_timeline.events[capacity - 1].map_time = FREE_LIST_END;
    return true;
}

void timeline_init(void)
{
    if (!timeline_allocate(g_timeline.capacity))
        game_fatal(SYSTEM_ERROR_MEMORY);
    g_timeline.high_water = 0;
    g_timeline.count = 0;
    g_timeline.first_unused = 0;
    timeline_add_watchdog_event();
}

void timeline_reset(void)
{
    int16_t last_free = -1;
    g_timeline.high_water = 0;
    g_timeline.first_unused = -1;
    for (int16_t i = 0; i < g_timeline.capacity; i++) {
        if (g_timeline.events[i].type) {
            g_timeline.high_water = i + 1;
            continue;
        }
        if (last_free == -1)
            g_timeline.first_unused = i;
        else
            g_timeline.events[last_free].map_time = i;
        g_timeline.events[i].map_time = FREE_LIST_END;
        last_free = i;
    }
}

/* Earlier time first, then higher type and priority, then the lower slot. */
static bool is_before(const TimelineEvent *event_a, const TimelineEvent *event_b)
{
    uint32_t time_a = event_a->map_time & EVENT_TIME_MASK;
    uint32_t time_b = event_b->map_time & EVENT_TIME_MASK;
    uint16_t key_a = event_a->type << EVENT_KEY_TYPE_SHIFT | event_a->priority;
    uint16_t key_b = event_b->type << EVENT_KEY_TYPE_SHIFT | event_b->priority;
    if (time_a != time_b)
        return time_a < time_b;
    if (key_a != key_b)
        return key_a > key_b;
    return event_a <= event_b;
}

uint16_t timeline_get_index(uint16_t event_index)
{
    uint16_t i = 0;
    do {
        if (g_timeline.heap[i] == event_index)
            return i;
    } while (++i < g_timeline.high_water);
    game_fatal(SYSTEM_ERROR_MEMORY);
    return 0;
}

#define BEFORE(i, j) is_before(&g_timeline.events[i], &g_timeline.events[j])

void timeline_fix_chronology(uint16_t timeline_index)
{
    uint16_t count = g_timeline.count;
    uint16_t i = timeline_index;
    uint16_t slot = g_timeline.heap[i];
    bool moved = false;
    if (count == 1)
        return;
    while (i) {
        uint16_t parent = (i - 1) >> 1;
        if (!BEFORE(slot, g_timeline.heap[parent]))
            break;
        g_timeline.heap[i] = g_timeline.heap[parent];
        i = parent;
        moved = true;
    }
    if (!moved)
        while (i <= (uint16_t)(count - 2) >> 1) { /* unsigned as in the original: no children below 2 entries */
            uint16_t child = 2 * i + 1;
            if (child + 1 < g_timeline.count && BEFORE(g_timeline.heap[child + 1], g_timeline.heap[child]))
                child++;
            if (!BEFORE(g_timeline.heap[child], slot))
                break;
            g_timeline.heap[i] = g_timeline.heap[child];
            i = child;
        }
    g_timeline.heap[i] = slot;
}

void timeline_delete_event(uint16_t event_index)
{
    g_timeline.events[event_index].type = 0;
    g_timeline.events[event_index].map_time = (uint16_t)g_timeline.first_unused; /* -1 becomes FREE_LIST_END */
    g_timeline.first_unused = event_index;
    if (--g_timeline.count) {
        uint16_t i = timeline_get_index(event_index);
        if (i != g_timeline.count) {
            g_timeline.heap[i] = g_timeline.heap[g_timeline.count];
            timeline_fix_chronology(i);
        }
    }
}

#define IS_SQUARE_EVENT(type)   ((type) >= EVENT_CORRIDOR && (type) <= EVENT_DOOR)
#define SAME_PLACE(e, f)        ((e)->map_time == (f)->map_time && (e)->b == (f)->b)
/* Set becomes clear and clear becomes set. */
#define OPPOSITE_EFFECT(effect) (EFFECT_CLEAR - (effect))

/* Returns the index of the event that absorbs the new one, or -1. Can change or delete events. */
static int16_t find_mergeable(TimelineEvent *event)
{
    TimelineEvent *e = g_timeline.events;
    for (int16_t i = 0; i < g_timeline.high_water; i++, e++) {
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
    if (g_timeline.count == (uint16_t)g_timeline.capacity) /* unsigned compare as in the original */
        game_fatal(SYSTEM_ERROR_TIMELINE_FULL);
    int16_t index = find_mergeable(event);
    if (index >= 0)
        return index;
    index = g_timeline.first_unused;
    g_timeline.first_unused = g_timeline.events[index].map_time;
    g_timeline.events[index] = *event;
    if (g_timeline.high_water <= index)
        g_timeline.high_water = index + 1;
    g_timeline.heap[g_timeline.count] = index;
    timeline_fix_chronology(g_timeline.count++);
    return index;
}

void timeline_extract_first_event(TimelineEvent *event)
{
    uint16_t index = g_timeline.heap[0];
    *event = g_timeline.events[index];
    timeline_delete_event(index);
}

bool timeline_is_first_event_expired(void)
{
    return g_timeline.count && (g_timeline.events[g_timeline.heap[0]].map_time & EVENT_TIME_MASK) <= g_game_time;
}

void timeline_add_watchdog_event(void)
{
    g_timeline.watchdog_time = g_game_time + WATCHDOG_INTERVAL;
    TimelineEvent event = {.map_time = g_timeline.watchdog_time, .type = EVENT_WATCHDOG};
    timeline_add_event(&event);
}
