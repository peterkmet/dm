/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_AUDIO_H
#define DM_UI_AUDIO_H
/* Prefixes: audio_ */

/* The audio output: four Paula voices in two stereo pairs, the CSB intro music, and the mixer that
   platform/ calls on the game thread. It plays what the engine requests through g_sound_play_hook; it holds
   no game rule. */

#include <stdint.h>

/* Output format of the mixer. */
#define AUDIO_OUTPUT_RATE    44100   /* frames per second, stereo int16 */
#define AUDIO_PAL_CLOCK      3546895 /* Paula clock, Hz */
/* Segments of the intro music. */
#define AUDIO_MUSIC_SEGMENTS 128

/* One segment of the intro music: sample data and its length in bytes. */
typedef struct {
    const int8_t *data;
    uint16_t length;
} MusicSegment;

/* Setup */
/* Sets g_sound_play_hook to audio_play_data; ui_init calls it. */
void audio_init(void);

/* Commands */
/* Plays a sound of the sound table at the left and right volumes. */
void audio_play_sample(int16_t sound, const uint8_t *volumes);
/* Copies sample data and plays it on a free stereo pair, or on the quieter pair when both play. */
void audio_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes);
/* Stops every voice and the intro music. */
void audio_stop_all(void);
/* Intro music: the segments play one after another, the same data on both sides of pair B. Samples go to pair
   A while it plays. The data stays owned by the caller until the music ends or stops. */
void audio_music_play(const MusicSegment *segments, int count, uint16_t period, uint8_t volume);
/* Sets the volume of the intro music on both sides of pair B. */
void audio_music_set_volume(uint8_t volume);
/* Renders interleaved stereo int16 at AUDIO_OUTPUT_RATE. Paula has no interpolation: each sample holds for one
   period. Runs on the game thread. */
void audio_mix(int16_t *out, int frames);

#endif
