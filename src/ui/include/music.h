/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_MUSIC_H
#define DM_UI_MUSIC_H
/* Prefixes: music_ */

/* Music of the advanced mode (not in the original Amiga game): the soundtracks of the FM-Towns and
   Super NES versions from MP3 files in <disk A folder>/music/. A track plays one time and does not loop. */

#include <stdbool.h>
#include <stdint.h>

/* The soundtracks of g_music_soundtrack. */
#define MUSIC_ORIGINAL  0
#define MUSIC_SUPER_NES 1
#define MUSIC_FM_TOWNS  2

/* The size of the FM-Towns CSB track table: maps, and squares per side. */
#define MUSIC_CSB_MAP_COUNT 10 /* map 10 of CSB (1 x 1) has no entry */
#define MUSIC_CSB_MAP_SIZE  32

extern int16_t g_music_soundtrack; /* MUSIC_ORIGINAL by default: no music */
extern int16_t g_music_volume;     /* the music volume step, SETTINGS_VOLUME_OFF to SETTINGS_VOLUME_FULL */
/* The track that plays, 0: silence; exported for the tests. */
extern int16_t g_music_track;
/* The track that started last, also when its file is missing; exported for the tests. */
extern int16_t g_music_last_track;
/* FM-Towns CSB: the track of the zone, it starts after the current track; exported for the tests. */
extern int16_t g_music_requested_track;

/* Stops the track; the soundtrack stays. */
void music_stop(void);
/* Selects a soundtrack (the options menu, or the settings at a game start) and starts the track of the party
   position. Super NES has no CSB soundtrack: the selection does nothing in CSB. */
void music_select(int16_t soundtrack);
/* Starts the track of the party position after the party moved: FM-Towns DM by level, FM-Towns CSB
   by zone, Super NES by trigger square. */
void music_update(void);
/* The track of the end of the game: game won or game over. */
void music_play_end(bool won);
/* Adds the music, scaled by g_music_volume, to interleaved stereo int16 at AUDIO_OUTPUT_RATE.
   Runs on the game thread, after audio_mix. */
void music_mix(int16_t *samples, int frames);

#endif
