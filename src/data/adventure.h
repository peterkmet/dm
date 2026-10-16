/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_ADVENTURE_H
#define DM_DATA_ADVENTURE_H
/* Prefixes: adventure_ */

/* The adventure of the run: Dungeon Master or Chaos Strikes Back. Its data folder and save file names. */

#include <stdbool.h>
#include <stdint.h>

#define ADVENTURE_DM  0
#define ADVENTURE_CSB 1

#define DM_DUNGEON_ID         10 /* the DM Dungeon.DAT */
#define CSB_PRISON_DUNGEON_ID 12 /* the CSB Dungeon.DAT: champion selection; a save makes the adventure */
#define CSB_MINI_FILE         "disk-b/MINI.DAT" /* the CSB adventure without champions, in the CSB folder */
#define CSB_INTRO_FILE        "disk-b/ANIM.DAT" /* the CSB intro with music, in the CSB folder */

#define ADVENTURE_GRAPHICS_FILE "Graphics.DAT" /* the graphics file of both adventures */
#define ADVENTURE_GAME_FILES    4              /* the most files of one adventure */

typedef struct {
    const char *name;
    uint32_t crc;
} GameFile;

typedef struct {
    const char *name;                     /* the --adventure value */
    const char *dir;                      /* the default disk A folder */
    const char *title;                    /* the name in messages */
    const char *engine;                   /* the engine module file */
    const char *save;                     /* the save file name */
    const char *backup;                   /* the backup file name */
    GameFile files[ADVENTURE_GAME_FILES]; /* the game files and their CRC32; a NULL name ends the list */
} Adventure;

typedef enum { ADVENTURE_DISK_INSTALLED, ADVENTURE_DISK_SKIPPED, ADVENTURE_DISK_FAILED } AdventureDisk;

extern int16_t g_adventure; /* ADVENTURE_DM by default; game_start_new does not reset it */

/* The names and files of an adventure: ADVENTURE_DM or ADVENTURE_CSB. */
const Adventure *adventure_get(int16_t adventure);

/* The adventure of a --adventure value ("dm" or "csb"), or -1. */
int16_t adventure_find(const char *name);
/* True when dir has the CSB files: APPB.FTL, Graphics.DAT, Dungeon.DAT, and disk-b/MINI.DAT. */
bool adventure_is_csb_present(const char *dir);
/* The first game file in dir that is missing or differs from the Amiga disk of the adventure (CRC32), or NULL. */
const char *adventure_find_bad_file(int16_t adventure, const char *dir);
/* Loads the engine module and Graphics.DAT of g_adventure from dir. False with the failed file name in *failed. */
bool adventure_load_files(const char *dir, const char **failed);
/* Extracts a game disk image into its default folder: DM, CSB disk A, or CSB disk B. INSTALLED with the folder in
   *dir; SKIPPED when the disk is no game disk or the folder has its files already; FAILED when the extraction
   fails. */
AdventureDisk adventure_install_disk(const char *adf_path, const char **dir);

#endif
