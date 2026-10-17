/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/audio.h"
#include "engine/sound.h"
#include <stdbool.h>
#include <string.h>

/* Stereo pairs of the Amiga channels. */
#define PAIR_A         0
#define PAIR_B         1
#define PAIR_COUNT     2
#define AMIGA_CHANNELS 4

typedef struct {
    const int8_t *data;
    uint32_t length, period, frame; /* frame: output frames since start */
    uint8_t volume;                 /* 0-64 */
} Voice;

/* Music of the CSB intro (ANIM.FTL of CSB disk B): sample segments one after another on pair B. */
typedef struct {
    MusicSegment segments[AUDIO_MUSIC_SEGMENTS];
    int count, next;
    uint16_t period;
    uint8_t volume;
} Music;

static const int pair_channels[PAIR_COUNT][SOUND_SIDE_COUNT] = {
    {3, 2},
    {0, 1}
}; /* pair A, pair B: left, right */
static struct {
    Voice voices[AMIGA_CHANNELS]; /* Amiga channels: 0, 3 left; 1, 2 right */
    uint8_t pair_volumes[PAIR_COUNT][SOUND_SIDE_COUNT];
    int8_t pair_samples[PAIR_COUNT][UINT16_MAX]; /* the copied sample of each pair */
    Music music;
} audio;

/* Sample index that the voice plays now, at the output rate. */
static uint64_t get_voice_position(const Voice *v)
{
    return (uint64_t)v->frame * AUDIO_PAL_CLOCK / ((uint64_t)v->period * AUDIO_OUTPUT_RATE);
}

static bool is_voice_busy(const Voice *v)
{
    return v->data && get_voice_position(v) < v->length;
}

/* Setup */

void audio_init(void)
{
    g_sound_play_hook = audio_play_data;
}

/* Commands */

void audio_play_sample(int16_t sound, const uint8_t *volumes)
{
    uint16_t length, period;
    const int8_t *data = sound_get_sample(sound, &length, &period);
    audio_play_data(data, length, period, volumes);
}

void audio_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes)
{
    if (!data || !length)
        return;
    /* while the music plays, a sample replaces the sample on pair A */
    int pair = audio.music.count                                                       ? PAIR_A
               : !is_voice_busy(&audio.voices[pair_channels[PAIR_A][SOUND_SIDE_LEFT]]) ? PAIR_A
               : !is_voice_busy(&audio.voices[pair_channels[PAIR_B][SOUND_SIDE_LEFT]]) ? PAIR_B
               : sound_compare_loudness(audio.pair_volumes[PAIR_A], audio.pair_volumes[PAIR_B]) == LOUDNESS_HIGHER
                   ? PAIR_B
                   : PAIR_A;
    memcpy(audio.pair_volumes[pair], volumes, SOUND_SIDE_COUNT);
    memcpy(audio.pair_samples[pair], data, length);
    for (int side = 0; side < SOUND_SIDE_COUNT; side++)
        audio.voices[pair_channels[pair][side]] = (Voice){audio.pair_samples[pair], length, period, 0, volumes[side]};
}

void audio_stop_all(void)
{
    memset(audio.voices, 0, sizeof audio.voices);
    audio.music.count = 0;
}

void audio_music_play(const MusicSegment *segments, int segment_count, uint16_t period, uint8_t volume)
{
    audio.music.count = 0;
    for (int i = 0; i < segment_count && i < AUDIO_MUSIC_SEGMENTS; i++)
        audio.music.segments[i] = segments[i];
    audio.music.count = segment_count < AUDIO_MUSIC_SEGMENTS ? segment_count : AUDIO_MUSIC_SEGMENTS;
    audio.music.next = 0;
    audio.music.period = period;
    audio.music.volume = volume;
    for (int side = 0; side < SOUND_SIDE_COUNT; side++)
        audio.voices[pair_channels[PAIR_B][side]].data = NULL;
}

void audio_music_set_volume(uint8_t volume)
{
    audio.music.volume = volume;
    for (int side = 0; side < SOUND_SIDE_COUNT && audio.music.count; side++)
        audio.voices[pair_channels[PAIR_B][side]].volume = volume;
}

/* The next music segment starts on pair B when the one before ends; after the last one the music ends. */
static void continue_music(void)
{
    Music *m = &audio.music;
    if (!m->count || is_voice_busy(&audio.voices[pair_channels[PAIR_B][SOUND_SIDE_LEFT]]))
        return;
    if (m->next >= m->count) {
        m->count = 0;
        return;
    }
    for (int side = 0; side < SOUND_SIDE_COUNT; side++)
        audio.voices[pair_channels[PAIR_B][side]] =
            (Voice){m->segments[m->next].data, m->segments[m->next].length, m->period, 0, m->volume};
    m->next++;
}

void audio_mix(int16_t *samples, int frames)
{
    for (int f = 0; f < frames; f++) {
        int mix[AMIGA_CHANNELS] = {0};
        continue_music();
        for (int c = 0; c < AMIGA_CHANNELS; c++) {
            Voice *v = &audio.voices[c];
            if (!v->data)
                continue;
            uint64_t i = get_voice_position(v);
            if (i >= v->length) {
                v->data = NULL;
                continue;
            }
            mix[c] = v->data[i] * v->volume * 2;
            v->frame++;
        }
        samples[2 * f] = mix[pair_channels[PAIR_A][SOUND_SIDE_LEFT]] + mix[pair_channels[PAIR_B][SOUND_SIDE_LEFT]];
        samples[2 * f + 1] =
            mix[pair_channels[PAIR_A][SOUND_SIDE_RIGHT]] + mix[pair_channels[PAIR_B][SOUND_SIDE_RIGHT]];
    }
}
