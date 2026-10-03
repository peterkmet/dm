#include <string.h>
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "sound.h"

#define G_SOUNDS 0x100d52           /* 12-byte entries: graphic, rate, priority */
#define G_SOUND_VOLUME_TABLE 0x100696 /* uint8_t[25][25] */

typedef struct {
    const int8_t *data;
    uint32_t length, period, frame; /* frame: output frames since start */
    uint8_t volume;                 /* 0-64 */
} Voice;

static Voice voices[4];                  /* Amiga channels: 0, 3 left; 1, 2 right */
static const int pair_channels[2][2] = { { 3, 2 }, { 0, 1 } }; /* pair A, pair B: left, right */
static uint8_t pair_volumes[2][2];
int16_t g_pending_sounds[2] = { -1, -1 };
uint8_t g_pending_sound_volumes[2][2];

/* Sample of a sound like sound_play_sample: graphic data after the 2-byte header, length = size - 2. */
const int8_t *sound_sample(int16_t sound, uint16_t *length, uint16_t *period)
{
    uint16_t size;
    if (sound < 0 || sound >= SOUND_COUNT || kaos_s16(G_SOUNDS + 12 * sound) < 0) return NULL;
    const uint8_t *p = gfx_item(kaos_s16(G_SOUNDS + 12 * sound), &size);
    uint8_t rate = kaos_u8(G_SOUNDS + 12 * sound + 3);
    if (!p || size < 2 || !rate) return NULL;
    *length = size - 2;
    *period = 0x11c60 / rate;
    return (const int8_t *)p + 2;
}

static uint16_t loudness_class(const uint8_t *v)
{
    uint16_t sum = v[0] + v[1];
    return sum < 15 ? 0 : sum < 26 ? 1 : 2;
}

int16_t sound_compare_loudness(const uint8_t *a, const uint8_t *b)
{
    uint16_t ca = loudness_class(a), cb = loudness_class(b);
    return cb < ca ? 1 : cb == ca ? 0 : -1;
}

/* volumes[0] left, volumes[1] right. Returns 0 when the square is more than 12 squares away. */
int16_t sound_get_volume(int16_t map_x, int16_t map_y, uint8_t *volumes)
{
    int16_t dx = map_x - g_party_map_x, dy = map_y - g_party_map_y, side = 0, ahead = 0;
    switch (g_party_direction) {
    case 0: side = dx; ahead = dy; break;
    case 1: side = dy; ahead = -dx; break;
    case 2: side = -dx; ahead = -dy; break;
    case 3: side = -dy; ahead = dx; break;
    }
    if (side < -12 || side > 12 || ahead < -12 || ahead > 12) return 0;
    volumes[1] = kaos_u8(G_SOUND_VOLUME_TABLE + (side + 12) + (ahead + 12) * 25);
    volumes[0] = kaos_u8(G_SOUND_VOLUME_TABLE + (12 - side) + (ahead + 12) * 25);
    return 1;
}

static int voice_busy(const Voice *v)
{
    return v->data && (uint64_t)v->frame * SOUND_PAL_CLOCK / ((uint64_t)v->period * SOUND_OUTPUT_RATE) < v->length;
}

/* Starts a sound on a free stereo pair, or on the quieter pair when both play. */
void sound_play_sample(int16_t sound, const uint8_t *volumes)
{
    uint16_t length, period;
    const int8_t *data = sound_sample(sound, &length, &period);
    if (!data || !length) return;
    int pair = !voice_busy(&voices[3]) ? 0
             : !voice_busy(&voices[0]) ? 1
             : sound_compare_loudness(pair_volumes[0], pair_volumes[1]) == 1 ? 1 : 0;
    memcpy(pair_volumes[pair], volumes, 2);
    for (int side = 0; side < 2; side++)
        voices[pair_channels[pair][side]] = (Voice){ data, length, period, 0, volumes[side] };
}

static void set_pending(int slot, int16_t sound, const uint8_t *volumes)
{
    g_pending_sounds[slot] = sound;
    memcpy(g_pending_sound_volumes[slot], volumes, 2);
}

/* Mode 0 plays now, mode 1 queues for sound_play_pending, mode >= 2 adds event 20 at game time + mode - 1. */
void sound_request_play(int16_t sound, int16_t map_x, int16_t map_y, uint16_t mode)
{
    uint8_t v[2];
    /* ponytail: no "current map == party map" check for mode >= 1 until the port has g_current_map_index */
    if (sound < 0 || sound >= SOUND_COUNT || kaos_s16(G_SOUNDS + 12 * sound) < 0) return;
    if (mode >= 2) {
        TimelineEvent e = { (uint32_t)g_party_map_index << 24 | (g_game_time + (mode - 1)), 20,
                            kaos_u8(G_SOUNDS + 12 * sound + 4), (uint16_t)((map_x & 0xff) << 8 | (map_y & 0xff)), (uint16_t)sound };
        timeline_add_event(&e);
        return;
    }
    if (!sound_get_volume(map_x, map_y, v)) return;
    if (mode == 0) {
        sound_play_sample(sound, v);
        return;
    }
    if (g_pending_sounds[0] == -1) {
        set_pending(0, sound, v);
        return;
    }
    if (g_pending_sounds[1] == -1) {
        set_pending(1, sound, v);
        return;
    }
    if (sound_compare_loudness(v, g_pending_sound_volumes[0]) != 1 && sound_compare_loudness(v, g_pending_sound_volumes[1]) != 1) {
        uint8_t priority = kaos_u8(G_SOUNDS + 12 * sound + 4);
        if (sound_compare_loudness(v, g_pending_sound_volumes[0]) == 0 && kaos_u8(G_SOUNDS + 12 * g_pending_sounds[0] + 4) < priority)
            set_pending(0, sound, v);
        else if (sound_compare_loudness(v, g_pending_sound_volumes[1]) == 0 && kaos_u8(G_SOUNDS + 12 * g_pending_sounds[1] + 4) < priority)
            set_pending(1, sound, v);
        return;
    }
    set_pending(sound_compare_loudness(g_pending_sound_volumes[0], g_pending_sound_volumes[1]) == 1 ? 1 : 0, sound, v);
}

void sound_play_pending(void)
{
    for (int i = 0; i < 2; i++)
        if (g_pending_sounds[i] != -1) {
            sound_play_sample(g_pending_sounds[i], g_pending_sound_volumes[i]);
            g_pending_sounds[i] = -1;
        }
}

void sound_stop_all(void)
{
    memset(voices, 0, sizeof voices);
}

/* Renders interleaved stereo int16 at SOUND_OUTPUT_RATE. Paula has no interpolation: each sample holds for one period. */
void sound_mix(int16_t *out, int frames)
{
    for (int f = 0; f < frames; f++) {
        int mix[4] = { 0 };
        for (int c = 0; c < 4; c++) {
            Voice *v = &voices[c];
            if (!v->data) continue;
            uint64_t i = (uint64_t)v->frame * SOUND_PAL_CLOCK / ((uint64_t)v->period * SOUND_OUTPUT_RATE);
            if (i >= v->length) {
                v->data = NULL;
                continue;
            }
            mix[c] = v->data[i] * v->volume * 2;
            v->frame++;
        }
        out[2 * f] = (int16_t)(mix[0] + mix[3]);
        out[2 * f + 1] = (int16_t)(mix[1] + mix[2]);
    }
}

/* Plays a sound at full volume on both sides. */
void sound_demo_play(int index)
{
    sound_play_sample((int16_t)index, (const uint8_t[]){ 64, 64 });
}
