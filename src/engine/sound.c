/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/sound.h"
#include "data/graphics.h"
#include "data/tables.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/timeline.h"
#include <stdbool.h>
#include <string.h>

/* Sound period, loudness sums and the empty slot. */
#define SOUND_PERIOD_PER_RATE 72800 /* Paula period = this / sound rate */
#define LOUDNESS_MEDIUM_SUM   15    /* left + right volume of a medium sound */
#define LOUDNESS_LOUD_SUM     26    /* left + right volume of a loud sound */
#define NO_SOUND              (-1)  /* empty pending slot */

SoundPlayHook g_sound_play_hook;
PendingSound g_pending_sounds[SOUND_PENDING_SLOTS] = {
    {NO_SOUND, {0, 0}},
    {NO_SOUND, {0, 0}}
};

static bool is_known_sound(int16_t sound)
{
    return sound >= 0 && sound < SOUND_COUNT && tables_get_sound_graphic(sound) >= 0;
}

const int8_t *sound_get_sample(int16_t sound, uint16_t *length, uint16_t *period)
{
    uint16_t size;
    if (!is_known_sound(sound))
        return NULL;
    const uint8_t *p = gfx_get_item(tables_get_sound_graphic(sound), &size);
    uint8_t rate = tables_get_sound_rate(sound);
    if (!p || size < SOUND_SAMPLE_HEADER || !rate)
        return NULL;
    *length = size - SOUND_SAMPLE_HEADER;
    *period = SOUND_PERIOD_PER_RATE / rate;
    return (const int8_t *)p + SOUND_SAMPLE_HEADER;
}

/* Loudness class of a stereo volume: 0 quiet, 1 medium, 2 loud. */
static uint16_t get_loudness_class(const uint8_t *v)
{
    uint16_t sum = v[SOUND_SIDE_LEFT] + v[SOUND_SIDE_RIGHT];
    return sum < LOUDNESS_MEDIUM_SUM ? 0 : sum < LOUDNESS_LOUD_SUM ? 1 : 2;
}

LoudnessResult sound_compare_loudness(const uint8_t *volumes_a, const uint8_t *volumes_b)
{
    uint16_t ca = get_loudness_class(volumes_a);
    uint16_t cb = get_loudness_class(volumes_b);
    return cb < ca ? LOUDNESS_HIGHER : cb == ca ? LOUDNESS_SAME : LOUDNESS_LOWER;
}

bool sound_get_volume(int16_t map_x, int16_t map_y, uint8_t *volumes)
{
    int16_t dx = map_x - g_party_map_x;
    int16_t dy = map_y - g_party_map_y;
    int16_t side = 0;
    int16_t ahead = 0;
    switch (g_party_direction) {
    case DIRECTION_NORTH:
        side = dx;
        ahead = dy;
        break;
    case DIRECTION_EAST:
        side = dy;
        ahead = -dx;
        break;
    case DIRECTION_SOUTH:
        side = -dx;
        ahead = -dy;
        break;
    case DIRECTION_WEST:
        side = -dy;
        ahead = dx;
        break;
    }
    if (side < -SOUND_HEARING_RANGE || side > SOUND_HEARING_RANGE || ahead < -SOUND_HEARING_RANGE ||
        ahead > SOUND_HEARING_RANGE)
        return false;
    volumes[SOUND_SIDE_RIGHT] = tables_get_sound_volume(ahead, side);
    volumes[SOUND_SIDE_LEFT] = tables_get_sound_volume(ahead, -side);
    return true;
}

static void set_pending(int16_t slot, int16_t sound, const uint8_t *volumes)
{
    g_pending_sounds[slot].sound = sound;
    memcpy(g_pending_sounds[slot].volumes, volumes, SOUND_SIDE_COUNT);
}

/* Queues a sound for the next sound_play_pending. Two slots: a louder sound, or one of
   the same loudness and higher priority, replaces a pending sound. */
static void queue_pending(int16_t sound, const uint8_t *v)
{
    for (int slot = 0; slot < SOUND_PENDING_SLOTS; slot++)
        if (g_pending_sounds[slot].sound == NO_SOUND) {
            set_pending(slot, sound, v);
            return;
        }
    if (sound_compare_loudness(v, g_pending_sounds[0].volumes) != LOUDNESS_HIGHER &&
        sound_compare_loudness(v, g_pending_sounds[1].volumes) != LOUDNESS_HIGHER) {
        uint8_t priority = tables_get_sound_priority(sound);
        if (sound_compare_loudness(v, g_pending_sounds[0].volumes) == LOUDNESS_SAME &&
            tables_get_sound_priority(g_pending_sounds[0].sound) < priority)
            set_pending(0, sound, v);
        else if (sound_compare_loudness(v, g_pending_sounds[1].volumes) == LOUDNESS_SAME &&
                 tables_get_sound_priority(g_pending_sounds[1].sound) < priority)
            set_pending(1, sound, v);
        return;
    }
    set_pending(sound_compare_loudness(g_pending_sounds[0].volumes, g_pending_sounds[1].volumes) == LOUDNESS_HIGHER ? 1
                                                                                                                    : 0,
                sound, v);
}

/* Plays a sound of the table at the volumes through the hook of the audio output. */
static void play_sample(int16_t sound, const uint8_t *volumes)
{
    uint16_t length, period;
    const int8_t *data = sound_get_sample(sound, &length, &period);
    if (data && g_sound_play_hook)
        g_sound_play_hook(data, length, period, volumes);
}

void sound_request_play(int16_t sound, Location at, int16_t mode, uint16_t delay)
{
    uint8_t v[SOUND_SIDE_COUNT];
    if (!is_known_sound(sound))
        return;
    if (mode != SOUND_MODE_NOW && g_current_map_index != g_party_map_index)
        return;
    if (mode == SOUND_MODE_EVENT) {
        TimelineEvent e = {EVENT_MAP_TIME(g_party_map_index, g_game_time + delay), EVENT_PLAY_SOUND,
                           tables_get_sound_priority(sound), event_square(at.x, at.y), sound};
        timeline_add_event(&e);
        return;
    }
    if (!sound_get_volume(at.x, at.y, v))
        return;
    if (mode == SOUND_MODE_NOW)
        play_sample(sound, v);
    else
        queue_pending(sound, v);
}

void sound_play_pending(void)
{
    for (int i = 0; i < SOUND_PENDING_SLOTS; i++)
        if (g_pending_sounds[i].sound != NO_SOUND) {
            play_sample(g_pending_sounds[i].sound, g_pending_sounds[i].volumes);
            g_pending_sounds[i].sound = NO_SOUND;
        }
}
