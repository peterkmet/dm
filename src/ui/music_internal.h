/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_MUSIC_INTERNAL_H
#define DM_UI_MUSIC_INTERNAL_H
/* Prefixes: music_ */

/* The soundtrack tables of the music module, shared by music.c and music_tables.c. The tables are not in the
   game files: they come from the FM-Towns and Super NES versions. */

#include "music.h"
#include <stdint.h>

/* Sizes of the soundtrack tables. */
#define MUSIC_FM_TOWNS_DM_LEVELS 14
#define MUSIC_SUPER_NES_TRIGGERS 22

/* A Super NES track starts when the party steps on its square (map-relative Amiga squares). */
typedef struct {
    uint8_t map, x, y, track;
} MusicTrigger;

extern const uint8_t g_music_csb_square_tracks[MUSIC_CSB_MAP_COUNT][MUSIC_CSB_MAP_SIZE][MUSIC_CSB_MAP_SIZE];
extern const uint8_t g_music_fm_towns_dm_level_tracks[MUSIC_FM_TOWNS_DM_LEVELS];
extern const MusicTrigger g_music_super_nes_triggers[MUSIC_SUPER_NES_TRIGGERS];

#endif
