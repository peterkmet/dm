#ifndef MUSIC_H
#define MUSIC_H
#include <stdint.h>

/* Music of the advanced mode (not in the original Amiga game): the soundtracks of the FM-Towns and
   Super NES versions from MP3 files in <disk A folder>/music/. A track plays one time and does not loop.
   Decisions: docs/superpowers/2026-10-05-music-decisions.md. */

enum { MUSIC_ORIGINAL, MUSIC_SUPER_NES, MUSIC_FM_TOWNS };

#define MUSIC_CSB_MAP_COUNT 10   /* map 10 of CSB (1 x 1) has no entry */
#define MUSIC_CSB_MAP_SIZE 32

extern int16_t g_music_soundtrack;      /* MUSIC_ORIGINAL by default: no music */
extern int16_t g_music_track;           /* the track that plays, 0: silence */
extern int16_t g_music_last_track;      /* the track that started last, also when its file is missing */
extern int16_t g_music_requested_track; /* FM-Towns CSB: the track of the zone, it starts after the current track */
extern const uint8_t g_music_csb_square_tracks[MUSIC_CSB_MAP_COUNT][MUSIC_CSB_MAP_SIZE][MUSIC_CSB_MAP_SIZE];

/* Selects a soundtrack (keys 1, 2, 3 on the automap) and starts the track of the party position.
   Super NES has no CSB soundtrack: the selection does nothing in CSB. */
void music_select(int16_t soundtrack);
/* Starts the track of the party position after the party moved: FM-Towns DM by level, FM-Towns CSB
   by zone, Super NES by trigger square. */
void music_update(void);
/* The track of the end of the game: game won or game over. */
void music_play_end(int16_t won);
/* Stops the track; the soundtrack stays. */
void music_stop(void);
/* Adds the music to interleaved stereo int16 at SOUND_OUTPUT_RATE. */
void music_mix(int16_t *out, int frames);

#endif
