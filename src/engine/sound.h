/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SOUND_H
#define DM_ENGINE_SOUND_H
/* Prefixes: sound_ */

/* The sound rules: the sound table, the volume by distance, the pending queue, and the play requests of the
   rules. The audio output plays through g_sound_play_hook. */

#include "engine/map.h"
#include <stdbool.h>
#include <stdint.h>

/* Sound count, pending slots and sample sizes. */
#define SOUND_COUNT         35
#define SOUND_PENDING_SLOTS 2
#define SOUND_MAX_VOLUME    64 /* full volume of one side */
#define SOUND_SAMPLE_HEADER 2  /* sample length word before the sample data */

/* Sounds of the square rules. */
#define SOUND_SWITCH             1
#define SOUND_DOOR               2
#define SOUND_ENTRANCE_DOOR      3
#define SOUND_DOOR_HITS_CREATURE 4
#define SOUND_PARTY_DAMAGED      15

/* Sounds of the creature rules. */
#define SOUND_METALLIC_THUD      0
#define SOUND_WOODEN_THUD        4
#define SOUND_STRONG_EXPLOSION   5
#define SOUND_WEAK_EXPLOSION     6
#define SOUND_SCREAM             7
#define SOUND_CHAMPION_0_DAMAGED 9
#define SOUND_SWING              13
#define SOUND_BUZZ               14
#define SOUND_SPELL              16
#define SOUND_WAR_CRY            17
#define SOUND_BLOW_HORN          18
#define SOUND_SWALLOW            8

/* When a requested sound plays. */
#define SOUND_MODE_NONE       (-1) /* no sound (possession drops of a moving group) */
#define SOUND_MODE_NOW        0
#define SOUND_MODE_AFTER_TICK 1
#define SOUND_MODE_EVENT      2 /* a play sound event after the delay */

/* Sides of a stereo volume pair. */
#define SOUND_SIDE_LEFT  0
#define SOUND_SIDE_RIGHT 1
#define SOUND_SIDE_COUNT 2

/* Plays sample data at a Paula period and the left and right volumes. */
typedef void (*SoundPlayHook)(const int8_t *sample, uint16_t length, uint16_t period, const uint8_t *volumes);
extern SoundPlayHook g_sound_play_hook; /* ui/audio.c sets it; NULL: no sound plays */
/* A sound that plays after the tick, with its volume per side. */
typedef struct {
    int16_t sound; /* -1: empty slot */
    uint8_t volumes[SOUND_SIDE_COUNT];
} PendingSound;
extern PendingSound g_pending_sounds[SOUND_PENDING_SLOTS]; /* exported for the tests */
/* Results of a loudness comparison. */
typedef enum {
    LOUDNESS_LOWER = -1,
    LOUDNESS_SAME = 0,
    LOUDNESS_HIGHER = 1,
} LoudnessResult;

/* Sample data of a sound, its length in bytes and its Paula period. */
const int8_t *sound_get_sample(int16_t sound, uint16_t *length, uint16_t *period);
/* LOUDNESS_HIGHER when a is louder than b, LOUDNESS_SAME when equal, LOUDNESS_LOWER when quieter. Exported for the tests. */
LoudnessResult sound_compare_loudness(const uint8_t *volumes_a, const uint8_t *volumes_b);
/* Stereo volume of a sound on a square, relative to the party position and direction.
   Returns false when the square is out of hearing range. */
bool sound_get_volume(int16_t map_x, int16_t map_y, uint8_t *volumes); /* exported for the tests */
/* Plays a sound of a square now, after the tick, or as a timeline event after delay ticks (SOUND_MODE_EVENT only;
   0 for the other modes). */
void sound_request_play(int16_t sound, Location at, int16_t mode, uint16_t delay);
/* Plays the queued sounds and empties the queue. */
void sound_play_pending(void);

#endif
