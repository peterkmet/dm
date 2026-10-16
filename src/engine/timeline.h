/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_TIMELINE_H
#define DM_ENGINE_TIMELINE_H
/* Prefixes: timeline_ */
/* The timeline: the event slots and the min-heap that orders them by time. It stores events; engine/event.h runs them. */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t map_time; /* map bits 24-31, time bits 0-23; free slot: next free index */
    uint8_t type;      /* 0: free slot */
    uint8_t priority;  /* champion events: champion index */
    uint16_t b;        /* square events: x << 8 | y */
    uint16_t c;        /* square events: cell << 8 | effect */
} TimelineEvent;

typedef struct {
    TimelineEvent *events; /* capacity slots, owned */
    uint16_t *heap;        /* min-heap of slot indices, owned */
    int16_t capacity;      /* the dungeon loader and the save set it */
    uint16_t count;        /* events in the heap */
    uint16_t high_water;   /* slots below this index were used */
    int16_t first_unused;  /* first free slot, -1 none */
    int32_t watchdog_time; /* game time of the next watchdog event */
} Timeline;

extern Timeline g_timeline;

/* Allocates capacity free event slots and an empty heap; false when the allocation fails. */
bool timeline_allocate(int16_t capacity);
/* New game only. A free slot keeps the next free index in map_time. */
void timeline_init(void);
/* Rebuilds the free list and the high water mark from the slot types (before a save, after a load). */
void timeline_reset(void);
/* Earlier time first, then higher type and priority, then the lower slot. */
bool timeline_is_event_a_before_b(const TimelineEvent *a, const TimelineEvent *b);
/* Position of an event slot in the timeline heap. */
uint16_t timeline_get_index(uint16_t event_index);
/* Moves a heap entry up or down until the timeline is in time order again. */
void timeline_fix_chronology(uint16_t timeline_index);
/* Frees an event slot and removes it from the timeline. */
void timeline_delete_event(uint16_t event_index);
/* Returns the index of the event that absorbs the new one, or -1. Can change or delete events. */
int16_t timeline_find_mergeable_event(TimelineEvent *event);
/* Adds an event to the timeline, or merges it into an existing event. Returns the slot index. */
int16_t timeline_add_event(TimelineEvent *event);
/* Removes the earliest event from the timeline and copies it to event. */
void timeline_extract_first_event(TimelineEvent *event);
/* True when the earliest event is due at the current game time. */
bool timeline_is_first_event_expired(void);
/* The original leaves priority, b, and c as stack garbage. */
void timeline_add_watchdog_event(void);

#endif
