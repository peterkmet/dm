/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SOUND_H
#define DM_ENGINE_SOUND_H
/* Prefixes: sound_ */
#include <stdbool.h>
#include <stdint.h>

#define SOUND_COUNT         35
#define SOUND_OUTPUT_RATE   44100
#define SOUND_PAL_CLOCK     3546895 /* Paula clock, Hz */
#define SOUND_PENDING_SLOTS 2
#define SOUND_MAX_VOLUME    64 /* full volume of one side */
#define SOUND_SAMPLE_HEADER 2  /* sample length word before the sample data */

/* Sounds of the square rules. Mode: 0 plays now, 1 plays after the tick. */
#define SOUND_SWITCH             1
#define SOUND_DOOR               2
#define SOUND_ENTRANCE_DOOR      3
#define SOUND_DOOR_HITS_CREATURE 4
#define SOUND_TELEPORT           14
#define SOUND_PARTY_DAMAGED      15
#define SOUND_MODE_NOW           0
#define SOUND_MODE_AFTER_TICK    1

/* Sounds of the creature rules. */
#define SOUND_METALLIC_THUD       0
#define SOUND_WOODEN_THUD         4
#define SOUND_STRONG_EXPLOSION    5
#define SOUND_WEAK_EXPLOSION      6
#define SOUND_SCREAM              7
#define SOUND_CHAMPION_0_DAMAGED  9
#define SOUND_SWING               13
#define SOUND_BUZZ                14
#define SOUND_SPELL               16
#define SOUND_WAR_CRY             17
#define SOUND_BLOW_HORN           18
#define SOUND_MODE_ONE_TICK_LATER 2
#define SOUND_SWALLOW             8

extern int16_t g_pending_sounds[SOUND_PENDING_SLOTS];           /* -1: empty slot */
extern uint8_t g_pending_sound_volumes[SOUND_PENDING_SLOTS][2]; /* left, right */

const int8_t *sound_get_sample(int16_t sound, uint16_t *length, uint16_t *period);
int16_t sound_compare_loudness(const uint8_t *a, const uint8_t *b);
bool sound_get_volume(int16_t map_x, int16_t map_y, uint8_t *volumes);
void sound_play_sample(int16_t sound, const uint8_t *volumes);
void sound_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes);
void sound_request_play(int16_t sound, int16_t map_x, int16_t map_y, uint16_t mode);
void sound_play_pending(void);
/* Stops every voice, the music, and the MP3 music. */
void sound_stop_all(void);
#define SOUND_MUSIC_SEGMENTS 128
/* Music: the segments play one after another, the same data on both sides of pair B. Samples go to pair A
   while it plays. The data stays owned by the caller until the music ends or stops. */
void sound_music_play(const int8_t *const *data, const uint16_t *lengths, int count, uint16_t period, uint8_t volume);
void sound_music_set_volume(uint8_t volume);
void sound_music_stop(void);
void sound_mix(int16_t *out, int frames);
void sound_demo_play(int index);

#endif
