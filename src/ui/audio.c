/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "settings.h"
#include "sound.h"
#include "advanced.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

/* Stereo pairs of the Amiga channels. */
#define PAIR_A        0
#define PAIR_B        1
#define PAIR_COUNT    8 /* pairs 0 and 1 are the Amiga pairs; the rest play only in the advanced mode */
#define VANILLA_PAIRS 2
#define VOICE_COUNT   (PAIR_COUNT * SOUND_SIDE_COUNT)

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
    {3,   2  },
    {0,   1  },
    {4,   5  },
    {6,   7  },
    {8,   9  },
    {0xA, 0xB},
    {0xC, 0xD},
    {0xE, 0xF}
}; /* pair A, pair B, then the extra pairs: left, right */
/* The filter stages: the fixed RC stage, then the LED stages; each is y += alpha * (x - y), alpha = dt / (RC + dt). */
#define FILTER_STAGES        (1 + FILTER_LED_POLES)
#define FILTER_ALPHA(cutoff) (1.0f / (1.0f + AUDIO_OUTPUT_RATE / (6.2831853f * (cutoff))))

int16_t g_audio_volume = SETTINGS_VOLUME_FULL;
static const float filter_alpha[FILTER_STAGES] = {
    FILTER_ALPHA(FILTER_FIXED_CUTOFF_HZ), FILTER_ALPHA(FILTER_LED_CUTOFF_HZ), FILTER_ALPHA(FILTER_LED_CUTOFF_HZ)};
static float filter_state[SOUND_SIDE_COUNT][FILTER_STAGES]; /* the output of each stage of each side */
static struct {
    Voice voices[VOICE_COUNT]; /* Amiga channels: 0, 3 left; 1, 2 right; 4 and up: the extra pairs */
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

/* The first free pair; when all play, the quietest one (pair A or B, as the original, outside the advanced mode). */
static int choose_pair(void)
{
    int count = g_advanced_features ? PAIR_COUNT : VANILLA_PAIRS;
    int quietest = PAIR_A;
    for (int p = 0; p < count; p++)
        if (!is_voice_busy(&audio.voices[pair_channels[p][SOUND_SIDE_LEFT]]))
            return p;
    for (int p = PAIR_B; p < count; p++)
        if (sound_compare_loudness(audio.pair_volumes[quietest], audio.pair_volumes[p]) == LOUDNESS_HIGHER)
            quietest = p;
    return quietest;
}

void audio_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes)
{
    if (!data || !length)
        return;
    /* while the music plays, a sample replaces the sample on pair A */
    int pair = audio.music.count ? PAIR_A : choose_pair();
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
        int mix[VOICE_COUNT] = {0}, sum[SOUND_SIDE_COUNT] = {0};
        continue_music();
        for (int c = 0; c < VOICE_COUNT; c++) {
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
        for (int p = 0; p < PAIR_COUNT; p++)
            for (int side = 0; side < SOUND_SIDE_COUNT; side++)
                sum[side] += mix[pair_channels[p][side]];
        for (int side = 0; side < SOUND_SIDE_COUNT; side++)
            sum[side] = sum[side] * g_audio_volume / SETTINGS_VOLUME_FULL;
        for (int side = 0; side < SOUND_SIDE_COUNT; side++)
            samples[2 * f + side] = g_advanced_features ? (int16_t)(sum[side] > INT16_MAX   ? INT16_MAX
                                                                    : sum[side] < INT16_MIN ? INT16_MIN
                                                                                            : sum[side])
                                                        : (int16_t)sum[side];
    }
}

void audio_filter(int16_t *samples, int frames)
{
    for (int f = 0; f < frames; f++)
        for (int side = 0; side < SOUND_SIDE_COUNT; side++) {
            float x = samples[2 * f + side];
            for (int stage = 0; stage < FILTER_STAGES; stage++) {
                float *y = &filter_state[side][stage];
                *y += filter_alpha[stage] * (x - *y);
                x = *y;
            }
            samples[2 * f + side] = (int16_t)lroundf(x);
        }
}
