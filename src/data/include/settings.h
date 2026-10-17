/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_SETTINGS_H
#define DM_DATA_SETTINGS_H
/* Prefixes: settings_ */

/* The settings file of the advanced mode (SETTINGS.TXT in the saves folder): the sound volume, the music volume
   and the music choice as text lines key=value. Player preferences, not game state: the save file does not hold
   them. It reads the file; it does not apply the values. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The settings file. */
#define SETTINGS_FILE     "SETTINGS.TXT"
#define SETTINGS_MAX_SIZE 1024 /* a larger file gives the defaults */

/* The volume steps: 0 is off, SETTINGS_VOLUME_FULL plays the samples unchanged. */
#define SETTINGS_VOLUME_OFF  0
#define SETTINGS_VOLUME_FULL 7

/* The music choices, the same numbers as the soundtracks of ui/music.h. */
#define SETTINGS_MUSIC_ORIGINAL  0
#define SETTINGS_MUSIC_SUPER_NES 1
#define SETTINGS_MUSIC_FM_TOWNS  2
#define SETTINGS_MUSIC_COUNT     3

/* The values of the settings file. */
typedef struct {
    int16_t sound_volume, music_volume; /* SETTINGS_VOLUME_OFF to SETTINGS_VOLUME_FULL */
    int16_t music;                      /* SETTINGS_MUSIC_* */
} Settings;

/* Queries */
/* The values of the settings text: a missing or unreadable line gives the default of its value (full volumes,
   the original music), an unknown key is ignored, a text above SETTINGS_MAX_SIZE gives all defaults. Super NES
   gives the original music in CSB; exported for the tests. */
Settings settings_parse(const char *file_text, size_t length);
/* The values of the settings file in dir, as settings_parse; a missing file gives the defaults. */
Settings settings_load(const char *dir);

#endif
