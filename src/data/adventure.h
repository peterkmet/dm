#ifndef ADVENTURE_H
#define ADVENTURE_H
/* Prefixes: adventure_ */
#include <stdbool.h>
#include <stdint.h>

/* The adventure of the run: Dungeon Master or Chaos Strikes Back. Its data folder and save file names. */

enum { ADVENTURE_DM, ADVENTURE_CSB };

#define DM_DUNGEON_ID         10 /* the DM Dungeon.DAT */
#define CSB_PRISON_DUNGEON_ID 12 /* the CSB Dungeon.DAT: champion selection; a save makes the adventure */
#define CSB_MINI_FILE         "disk-b/MINI.DAT" /* the CSB adventure without champions, in the CSB folder */
#define CSB_INTRO_FILE        "disk-b/ANIM.DAT" /* the CSB intro with music, in the CSB folder */
#define PATH_SIZE             1024              /* longest file path */

typedef struct {
    const char *name, *dir, *title;     /* the --adventure value, the default disk A folder, the name in messages */
    const char *engine, *save, *backup; /* the engine module, the save file and the backup file names */
} Adventure;

extern int16_t g_adventure; /* ADVENTURE_DM by default; game_start_new does not reset it */

/* The names and files of an adventure: ADVENTURE_DM or ADVENTURE_CSB. */
const Adventure *adventure_get(int16_t adventure);

/* The adventure of a --adventure value ("dm" or "csb"), or -1. */
int adventure_of(const char *name);
/* The default disk A folder: "game" (DM) or "game-csb" (CSB). */
const char *adventure_default_dir(int16_t adventure);
/* The engine module of g_adventure: KAOS.FTL or APPB.FTL. */
const char *adventure_engine_name(void);
/* The save file name of g_adventure: DMGAME.DAT or CSBGAME.DAT. */
const char *adventure_save_name(void);
/* The backup file name of g_adventure: DMGAME.BAK or CSBGAME.BAK. */
const char *adventure_backup_name(void);
/* True when dir has the CSB files: APPB.FTL, Graphics.DAT, Dungeon.DAT, and disk-b/MINI.DAT. */
bool adventure_csb_present(const char *dir);
/* The first game file in dir that is missing or differs from the Amiga disk of the adventure (CRC32), or NULL. */
const char *adventure_bad_file(int16_t adventure, const char *dir);
/* Extracts a game disk image into its default folder: DM, CSB disk A, or CSB disk B. Returns 1 and the folder in
   *dir, 0 when the disk is no game disk or the folder has its files already, or -1 when the extraction fails. */
int adventure_install_disk(const char *adf_path, const char **dir);

#endif
