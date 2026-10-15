#include <stdbool.h>
#include <stdio.h>
#include "adventure.h"
#include "dungeon.h"
#include "gameloop.h"
#include "music.h"
#include "sound.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#pragma clang diagnostic pop

#define MUSIC_VOLUME_DIVISOR         6 /* the music under the game sounds; raise it for quieter music */
#define MUSIC_MIX_FRAMES             1024
#define MUSIC_CHANNELS               2
#define NO_POSITION                  (-1)
#define FM_TOWNS_DM_GAME_OVER_TRACK  13
#define FM_TOWNS_DM_GAME_WON_TRACK   18
#define FM_TOWNS_CSB_GAME_OVER_TRACK 17
#define FM_TOWNS_CSB_PRISON_TRACK    6
#define SUPER_NES_GAME_WON_TRACK     16 /* the end credits */

/* FM-Towns DM 2.0: the CD track of each level (map 0-13). */
static const uint8_t fm_towns_dm_level_tracks[] = {2, 8, 9, 14, 11, 6, 5, 12, 15, 16, 17, 10, 19, 3};

/* Super NES DM: a track starts when the party steps on its square (map-relative Amiga squares). */
static const struct {
    uint8_t map, x, y, track;
} super_nes_triggers[] = {
    {0,  8,  4,  1 },
    {2,  9,  17, 2 },
    {2,  20, 12, 3 },
    {4,  3,  20, 3 },
    {9,  9,  29, 4 },
    {7,  25, 3,  5 },
    {2,  7,  4,  6 },
    {4,  21, 17, 6 },
    {9,  3,  12, 7 },
    {1,  15, 26, 8 },
    {13, 24, 6,  8 },
    {1,  7,  12, 9 },
    {3,  12, 8,  9 },
    {5,  8,  20, 10},
    {8,  8,  31, 10},
    {6,  14, 6,  11},
    {5,  20, 13, 12},
    {3,  28, 30, 13},
    {12, 12, 11, 13},
    {11, 16, 25, 14},
    {10, 3,  0,  17},
    {11, 1,  22, 17},
};

int16_t g_music_soundtrack, g_music_track, g_music_last_track, g_music_requested_track;

static drmp3 decoder;
static int decoding; /* the decoder has an open file */
static int16_t party_map = NO_POSITION, party_x, party_y;

void music_stop(void)
{
    if (decoding)
        drmp3_uninit(&decoder);
    decoding = 0;
    g_music_track = 0;
}

/* Stops the current track and starts track; a missing or unusable file gives silence. */
static void play(int16_t track)
{
    char path[PATH_SIZE];
    music_stop();
    g_music_last_track = track;
    snprintf(path, sizeof path, "%s/music/%s,Track_%02d,Music.mp3", gameloop_data_dir(),
             g_music_soundtrack == MUSIC_SUPER_NES ? "Super_NES" : "FM-Towns", track);
    if (!drmp3_init_file(&decoder, path, NULL))
        return;
    if (decoder.channels != MUSIC_CHANNELS || decoder.sampleRate != SOUND_OUTPUT_RATE) {
        drmp3_uninit(&decoder);
        return;
    }
    decoding = 1;
    g_music_track = track;
}

void music_select(int16_t soundtrack)
{
    if (soundtrack == MUSIC_SUPER_NES && g_adventure == ADVENTURE_CSB)
        return;
    music_stop();
    g_music_soundtrack = soundtrack;
    g_music_last_track = g_music_requested_track = 0;
    party_map = NO_POSITION;
    music_update();
}

/* FM-Towns CSB: the prison plays its track; in the adventure a zone requests its track, which starts
   when the current track ends. A level change stops the music. */
static void update_fm_towns_csb(bool new_map)
{
    if (g_dungeon_id == CSB_PRISON_DUNGEON_ID) {
        if (g_music_last_track != FM_TOWNS_CSB_PRISON_TRACK)
            play(FM_TOWNS_CSB_PRISON_TRACK);
        return;
    }
    if (new_map)
        music_stop();
    int16_t map = g_party_map_index, x = g_party_map_x, y = g_party_map_y;
    if (map < MUSIC_CSB_MAP_COUNT && x < MUSIC_CSB_MAP_SIZE && y < MUSIC_CSB_MAP_SIZE &&
        g_music_csb_square_tracks[map][y][x])
        g_music_requested_track = g_music_csb_square_tracks[map][y][x];
    if (g_music_requested_track && !g_music_track && g_music_requested_track != g_music_last_track)
        play(g_music_requested_track);
}

/* Super NES: a step on a trigger square starts its track; the same track goes on. */
static void update_super_nes(bool moved)
{
    if (!moved)
        return;
    for (size_t i = 0; i < sizeof super_nes_triggers / sizeof super_nes_triggers[0]; i++)
        if (super_nes_triggers[i].map == g_party_map_index && super_nes_triggers[i].x == g_party_map_x &&
            super_nes_triggers[i].y == g_party_map_y && super_nes_triggers[i].track != g_music_track)
            play(super_nes_triggers[i].track);
}

void music_update(void)
{
    bool new_map = party_map != NO_POSITION && party_map != g_party_map_index;
    bool moved = party_map != g_party_map_index || party_x != g_party_map_x || party_y != g_party_map_y;
    party_map = g_party_map_index;
    party_x = g_party_map_x;
    party_y = g_party_map_y;
    if (g_music_soundtrack == MUSIC_SUPER_NES)
        update_super_nes(moved);
    if (g_music_soundtrack != MUSIC_FM_TOWNS)
        return;
    if (g_adventure == ADVENTURE_CSB)
        update_fm_towns_csb(new_map);
    else if ((size_t)g_party_map_index < sizeof fm_towns_dm_level_tracks &&
             fm_towns_dm_level_tracks[g_party_map_index] != g_music_last_track)
        play(fm_towns_dm_level_tracks[g_party_map_index]);
}

void music_play_end(int16_t won)
{
    int16_t track = 0;
    if (g_music_soundtrack == MUSIC_FM_TOWNS && g_adventure == ADVENTURE_CSB) {
        if (!won)
            track = FM_TOWNS_CSB_GAME_OVER_TRACK; /* the CSB ending is an animation */
    } else if (g_music_soundtrack == MUSIC_FM_TOWNS)
        track = won ? FM_TOWNS_DM_GAME_WON_TRACK : FM_TOWNS_DM_GAME_OVER_TRACK;
    else if (g_music_soundtrack == MUSIC_SUPER_NES && won)
        track = SUPER_NES_GAME_WON_TRACK;
    if (track)
        play(track);
}

void music_mix(int16_t *out, int frames)
{
    int16_t pcm[MUSIC_MIX_FRAMES * MUSIC_CHANNELS];
    for (int done = 0; decoding && done < frames;) {
        int wanted = frames - done < MUSIC_MIX_FRAMES ? frames - done : MUSIC_MIX_FRAMES;
        int got = (int)drmp3_read_pcm_frames_s16(&decoder, (drmp3_uint64)wanted, pcm);
        for (int i = 0; i < got * MUSIC_CHANNELS; i++) {
            int sample = out[done * MUSIC_CHANNELS + i] + pcm[i] / MUSIC_VOLUME_DIVISOR;
            out[done * MUSIC_CHANNELS + i] = sample > INT16_MAX ? INT16_MAX : sample < INT16_MIN ? INT16_MIN : sample;
        }
        done += got;
        if (got < wanted)
            music_stop(); /* the track ended */
    }
}
