#include <stdbool.h>
#include <string.h>
#include "engine/dungeon.h"
#include "engine/game.h"
#include "data/graphics.h"
#include "engine/map.h"
#include "engine/sound.h"
#include "data/tables.h"
#include "engine/timeline_process.h"

#define SOUND_PERIOD_PER_RATE 72800 /* Paula period = this / sound rate */
#define LOUDNESS_MEDIUM_SUM   15    /* left + right volume of a medium sound */
#define LOUDNESS_LOUD_SUM     26    /* left + right volume of a loud sound */
#define NO_SOUND              (-1)  /* empty pending slot */

enum { SIDE_LEFT, SIDE_RIGHT, SIDE_COUNT };
enum { PAIR_A, PAIR_B, PAIR_COUNT };
enum { AMIGA_CHANNELS = 4 };

typedef struct {
    const int8_t *data;
    uint32_t length, period, frame; /* frame: output frames since start */
    uint8_t volume;                 /* 0-64 */
} Voice;

static Voice voices[AMIGA_CHANNELS]; /* Amiga channels: 0, 3 left; 1, 2 right */
static const int pair_channels[PAIR_COUNT][SIDE_COUNT] = {
    {3, 2},
    {0, 1}
}; /* pair A, pair B: left, right */
static uint8_t pair_volumes[PAIR_COUNT][SIDE_COUNT];
/* Music of the CSB intro (ANIM.FTL of CSB disk B): sample segments one after another on pair B. */
static struct {
    const int8_t *data[SOUND_MUSIC_SEGMENTS];
    uint16_t length[SOUND_MUSIC_SEGMENTS];
    int count, next;
    uint16_t period;
    uint8_t volume;
} music;
int16_t g_pending_sounds[SOUND_PENDING_SLOTS] = {NO_SOUND, NO_SOUND};
uint8_t g_pending_sound_volumes[SOUND_PENDING_SLOTS][SIDE_COUNT];

static bool sound_exists(int16_t sound)
{
    return sound >= 0 && sound < SOUND_COUNT && tables_get_sound_graphic(sound) >= 0;
}

/* Sample data of a sound, its length in bytes and its Paula period. */
const int8_t *sound_get_sample(int16_t sound, uint16_t *length, uint16_t *period)
{
    uint16_t size;
    if (!sound_exists(sound))
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
    uint16_t sum = v[SIDE_LEFT] + v[SIDE_RIGHT];
    return sum < LOUDNESS_MEDIUM_SUM ? 0 : sum < LOUDNESS_LOUD_SUM ? 1 : 2;
}

/* 1 when a is louder than b, 0 when equal, -1 when quieter. */
int16_t sound_compare_loudness(const uint8_t *a, const uint8_t *b)
{
    uint16_t ca = get_loudness_class(a);
    uint16_t cb = get_loudness_class(b);
    return cb < ca ? 1 : cb == ca ? 0 : -1;
}

/* Stereo volume of a sound on a square, relative to the party position and direction.
   Returns false when the square is out of hearing range. */
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
    volumes[SIDE_RIGHT] = tables_get_sound_volume(ahead, side);
    volumes[SIDE_LEFT] = tables_get_sound_volume(ahead, -side);
    return true;
}

/* Sample index that the voice plays now, at the output rate. */
static uint64_t get_voice_position(const Voice *v)
{
    return (uint64_t)v->frame * SOUND_PAL_CLOCK / ((uint64_t)v->period * SOUND_OUTPUT_RATE);
}

static bool is_voice_busy(const Voice *v)
{
    return v->data && get_voice_position(v) < v->length;
}

/* Starts a sound on a free stereo pair, or on the quieter pair when both play. */
void sound_play_sample(int16_t sound, const uint8_t *volumes)
{
    uint16_t length, period;
    const int8_t *data = sound_get_sample(sound, &length, &period);
    sound_play_data(data, length, period, volumes);
}

/* Starts sample data on a free stereo pair, or on the quieter pair when both play. */
void sound_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes)
{
    if (!data || !length)
        return;
    /* while the music plays, a sample replaces the sample on pair A */
    int pair = music.count                                                               ? PAIR_A
               : !is_voice_busy(&voices[pair_channels[PAIR_A][SIDE_LEFT]])               ? PAIR_A
               : !is_voice_busy(&voices[pair_channels[PAIR_B][SIDE_LEFT]])               ? PAIR_B
               : sound_compare_loudness(pair_volumes[PAIR_A], pair_volumes[PAIR_B]) == 1 ? PAIR_B
                                                                                         : PAIR_A;
    memcpy(pair_volumes[pair], volumes, SIDE_COUNT);
    for (int side = 0; side < SIDE_COUNT; side++)
        voices[pair_channels[pair][side]] = (Voice){data, length, period, 0, volumes[side]};
}

static void set_pending(int slot, int16_t sound, const uint8_t *volumes)
{
    g_pending_sounds[slot] = sound;
    memcpy(g_pending_sound_volumes[slot], volumes, SIDE_COUNT);
}

/* Queues a sound for the next sound_play_pending. Two slots: a louder sound, or one of
   the same loudness and higher priority, replaces a pending sound. */
static void queue_pending(int16_t sound, const uint8_t *v)
{
    for (int slot = 0; slot < SOUND_PENDING_SLOTS; slot++)
        if (g_pending_sounds[slot] == NO_SOUND) {
            set_pending(slot, sound, v);
            return;
        }
    if (sound_compare_loudness(v, g_pending_sound_volumes[0]) != 1 &&
        sound_compare_loudness(v, g_pending_sound_volumes[1]) != 1) {
        uint8_t priority = tables_get_sound_priority(sound);
        if (sound_compare_loudness(v, g_pending_sound_volumes[0]) == 0 &&
            tables_get_sound_priority(g_pending_sounds[0]) < priority)
            set_pending(0, sound, v);
        else if (sound_compare_loudness(v, g_pending_sound_volumes[1]) == 0 &&
                 tables_get_sound_priority(g_pending_sounds[1]) < priority)
            set_pending(1, sound, v);
        return;
    }
    set_pending(sound_compare_loudness(g_pending_sound_volumes[0], g_pending_sound_volumes[1]) == 1 ? 1 : 0, sound, v);
}

/* Mode 0 plays now, mode 1 queues for sound_play_pending, mode >= 2 adds a play sound event at game time + mode - 1. */
void sound_request_play(int16_t sound, int16_t map_x, int16_t map_y, uint16_t mode)
{
    uint8_t v[SIDE_COUNT];
    /* ponytail: no "current map == party map" check for mode >= 1 until the port has g_current_map_index */
    if (!sound_exists(sound))
        return;
    if (mode > SOUND_MODE_AFTER_TICK) {
        TimelineEvent e = {EVENT_MAP_TIME(g_party_map_index, g_game_time + (mode - 1)), EVENT_PLAY_SOUND,
                           tables_get_sound_priority(sound), event_square(map_x, map_y), sound};
        timeline_add_event(&e);
        return;
    }
    if (!sound_get_volume(map_x, map_y, v))
        return;
    if (mode == SOUND_MODE_NOW)
        sound_play_sample(sound, v);
    else
        queue_pending(sound, v);
}

/* Plays the queued sounds and empties the queue. */
void sound_play_pending(void)
{
    for (int i = 0; i < SOUND_PENDING_SLOTS; i++)
        if (g_pending_sounds[i] != NO_SOUND) {
            sound_play_sample(g_pending_sounds[i], g_pending_sound_volumes[i]);
            g_pending_sounds[i] = NO_SOUND;
        }
}

void sound_stop_all(void)
{
    memset(voices, 0, sizeof voices);
    music.count = 0;
}

void sound_music_play(const int8_t *const *data, const uint16_t *lengths, int count, uint16_t period, uint8_t volume)
{
    music.count = 0;
    for (int i = 0; i < count && i < SOUND_MUSIC_SEGMENTS; i++) {
        music.data[i] = data[i];
        music.length[i] = lengths[i];
    }
    music.count = count < SOUND_MUSIC_SEGMENTS ? count : SOUND_MUSIC_SEGMENTS;
    music.next = 0;
    music.period = period;
    music.volume = volume;
    for (int side = 0; side < SIDE_COUNT; side++)
        voices[pair_channels[PAIR_B][side]].data = NULL;
}

void sound_music_set_volume(uint8_t volume)
{
    music.volume = volume;
    for (int side = 0; side < SIDE_COUNT && music.count; side++)
        voices[pair_channels[PAIR_B][side]].volume = volume;
}

void sound_music_stop(void)
{
    if (!music.count)
        return;
    music.count = 0;
    for (int side = 0; side < SIDE_COUNT; side++)
        voices[pair_channels[PAIR_B][side]].data = NULL;
}

/* The next music segment starts on pair B when the one before ends; after the last one the music ends. */
static void continue_music(void)
{
    if (!music.count || is_voice_busy(&voices[pair_channels[PAIR_B][SIDE_LEFT]]))
        return;
    if (music.next >= music.count) {
        music.count = 0;
        return;
    }
    for (int side = 0; side < SIDE_COUNT; side++)
        voices[pair_channels[PAIR_B][side]] =
            (Voice){music.data[music.next], music.length[music.next], music.period, 0, music.volume};
    music.next++;
}

/* Renders interleaved stereo int16 at SOUND_OUTPUT_RATE. Paula has no interpolation: each sample holds for one period. */
void sound_mix(int16_t *out, int frames)
{
    for (int f = 0; f < frames; f++) {
        int mix[AMIGA_CHANNELS] = {0};
        continue_music();
        for (int c = 0; c < AMIGA_CHANNELS; c++) {
            Voice *v = &voices[c];
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
        out[2 * f] = mix[0] + mix[3];
        out[2 * f + 1] = mix[1] + mix[2];
    }
}

/* Plays a sound at full volume on both sides. */
void sound_demo_play(int index)
{
    sound_play_sample(index, (const uint8_t[]){SOUND_MAX_VOLUME, SOUND_MAX_VOLUME});
}
