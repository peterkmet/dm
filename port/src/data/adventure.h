#ifndef ADVENTURE_H
#define ADVENTURE_H
#include <stdint.h>

/* The adventure of the run: Dungeon Master or Chaos Strikes Back. Its data folder and save file names. */

enum { ADVENTURE_DM, ADVENTURE_CSB };

#define CSB_PRISON_DUNGEON_ID 12          /* the CSB Dungeon.DAT: champion selection; a save makes the adventure */
#define CSB_MINI_FILE "disk-b/MINI.DAT"   /* the CSB adventure without champions, in the CSB folder */
#define CSB_INTRO_FILE "disk-b/ANIM.DAT"  /* the CSB intro with music, in the CSB folder */
#define PATH_SIZE 1024                    /* longest file path */

extern int16_t g_adventure;              /* ADVENTURE_DM by default; game_new does not reset it */

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
/* 1 when dir has the CSB files: APPB.FTL, Graphics.DAT, Dungeon.DAT, and disk-b/MINI.DAT. */
int adventure_csb_present(const char *dir);

#endif
