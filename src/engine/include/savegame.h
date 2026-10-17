/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SAVEGAME_H
#define DM_ENGINE_SAVEGAME_H
/* Prefixes: savegame_ */

/* Game state to and from the save file of the adventure, DMGAME.DAT or CSBGAME.DAT (KAOS save_game and
   load_game, LOADSAVE.C), and the save slots of the advanced mode. savefile.c owns the container; this module owns
   the block contents. Layout: docs/formats/save.md. */

#include "os.h"
#include <stdbool.h>
#include <stdint.h>

/* The save slots: slot 0 is the save file of the adventure, slot n the file with the number n + 1. */
#define SAVEGAME_SLOT_COUNT 8
#define SAVEGAME_NAME_SIZE  16 /* an 8.3 file name with its end byte */

/* Results of a load. */
typedef enum {
    SAVEGAME_DAMAGED = -1,
    SAVEGAME_MISSING = 0,
    SAVEGAME_LOADED = 1,
} SavegameResult;
/* Results of the CSB prison (make new adventure). */
typedef enum {
    SAVEGAME_ADVENTURE_FAILED,
    SAVEGAME_ADVENTURE_MADE,
    SAVEGAME_ADVENTURE_NO_CHAMPIONS,
    SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE,
} SavegameAdventureResult;
/* Results of a slot info read. */
typedef enum {
    SAVEGAME_INFO_EMPTY,   /* neither the file nor the backup exists */
    SAVEGAME_INFO_DAMAGED, /* the file does not decode */
    SAVEGAME_INFO_READ,
} SavegameInfoResult;

/* The file names of a save slot. */
typedef struct {
    char save[SAVEGAME_NAME_SIZE];
    char backup[SAVEGAME_NAME_SIZE];
} SavegameSlotFiles;

/* The slot info of the slot list: the level of the party map (0 for the first level) and the file time. */
typedef struct {
    int16_t level;
    bool has_time;   /* false when the file time is not known */
    OsDateTime time; /* the local modification time of the file that the info read */
} SavegameSlotInfo;

/* The file names of a slot 0-7 of the adventure: DMGAME.DAT and DMGAME.BAK, then DMGAME2.DAT and DMGAME2.BAK to
   DMGAME8.DAT and DMGAME8.BAK (CSB: CSBGAME). */
SavegameSlotFiles savegame_get_slot_files(int16_t adventure, int16_t slot); /* exported for the tests */
/* Reads the slot info of a slot 0-7 from dir into *slot_info: the file, else the backup. The game state, the explored
   squares and the cheats do not change. */
SavegameInfoResult savegame_read_slot_info(const char *dir, int16_t slot, SavegameSlotInfo *slot_info);
/* Reads the slot info of the quick save, dir/QUICK.DAT, into *slot_info. */
SavegameInfoResult savegame_read_quick_info(const char *dir, SavegameSlotInfo *slot_info);

/* Writes the game to dir/DMGAME.DAT (CSB: CSBGAME.DAT). The old file becomes dir/DMGAME.BAK (CSBGAME.BAK)
   first. Returns true on success. */
bool savegame_save(const char *dir); /* exported for the tests */
/* Writes the game to the file of a slot 0-7 in dir; the old file becomes the backup of the slot first. Returns true
   on success. */
bool savegame_save_slot(const char *dir, int16_t slot);
/* Writes the game to dir/name. With a backup name, the old file becomes dir/backup first. Returns true on success. */
bool savegame_save_as(const char *dir, const char *name, const char *backup); /* exported for the tests */
/* CSB prison (CEDT MAKE NEW ADVENTURE): puts the party into the CSB adventure of mini_path (MINI.DAT),
   writes it to the file of the slot 0-7 in dir (slot 0: CSBGAME.DAT; the old file becomes the backup of the slot),
   and loads it. Returns SAVEGAME_ADVENTURE_MADE, SAVEGAME_ADVENTURE_NO_CHAMPIONS or
   SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE (nothing written), or SAVEGAME_ADVENTURE_FAILED. */
SavegameAdventureResult savegame_make_adventure(const char *dir, const char *mini_path, int16_t slot);
/* Reads dir/DMGAME.DAT, else dir/DMGAME.BAK (CSB: CSBGAME.DAT, CSBGAME.BAK), into the game state: dungeon, globals, active groups,
   champions, party, timeline, and portraits. Returns SAVEGAME_LOADED, SAVEGAME_MISSING, or
   SAVEGAME_DAMAGED. After a damaged file the dungeon is not loaded. */
SavegameResult savegame_load(const char *dir);
/* Reads the file of a slot 0-7 in dir, else the backup of the slot. Results as savegame_load. */
SavegameResult savegame_load_slot(const char *dir, int16_t slot);
/* Reads dir/name, else dir/backup when backup is not NULL. Results as savegame_load. */
SavegameResult savegame_load_from(const char *dir, const char *name, const char *backup); /* exported for the tests */
/* The saved game no longer counts: the endgame offers no restart. */
void savegame_forget(void);
/* The advanced CSB start by the prison stairs: puts the party into the CSB adventure of mini_path (MINI.DAT) without
   a file. The automap is empty and the saved game no longer counts. Results as savegame_make_adventure; after a
   refusal nothing changes, and after SAVEGAME_ADVENTURE_FAILED the prison game is back. */
SavegameAdventureResult savegame_start_adventure(const char *mini_path);
/* The quick save of the advanced mode, dir/QUICK.DAT: the saved-game flag and the save times stay as they were (a
   quick save is not a saved game). Returns true on success. */
bool savegame_quick_save(const char *dir);
/* The quick load of the advanced mode, dir/QUICK.DAT: returns the results of savegame_load_from; the saved-game flag
   stays as it was, and after a load both times are 0. */
SavegameResult savegame_quick_load(const char *dir);

#endif
