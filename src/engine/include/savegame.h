/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SAVEGAME_H
#define DM_ENGINE_SAVEGAME_H
/* Prefixes: savegame_ */

/* Game state to and from the save file of the adventure, DMGAME.DAT or CSBGAME.DAT (KAOS save_game and
   load_game, LOADSAVE.C). savefile.c owns the container; this module owns the block contents. Layout:
   docs/formats/save.md. */

#include <stdbool.h>
#include <stdint.h>

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

/* Writes the game to dir/DMGAME.DAT (CSB: CSBGAME.DAT). The old file becomes dir/DMGAME.BAK (CSBGAME.BAK)
   first. Returns true on success. */
bool savegame_save(const char *dir);
/* Writes the game to dir/name. With a backup name, the old file becomes dir/backup first. Returns true on success. */
bool savegame_save_as(const char *dir, const char *name, const char *backup); /* exported for the tests */
/* CSB prison (CEDT MAKE NEW ADVENTURE): puts the party into the CSB adventure of mini_path (MINI.DAT),
   writes it to dir/CSBGAME.DAT (the old file becomes CSBGAME.BAK), and loads it. Returns SAVEGAME_ADVENTURE_MADE,
   SAVEGAME_ADVENTURE_NO_CHAMPIONS or SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE (nothing written), or SAVEGAME_ADVENTURE_FAILED. */
SavegameAdventureResult savegame_make_adventure(const char *dir, const char *mini_path);
/* Reads dir/DMGAME.DAT, else dir/DMGAME.BAK (CSB: CSBGAME.DAT, CSBGAME.BAK), into the game state: dungeon, globals, active groups,
   champions, party, timeline, and portraits. Returns SAVEGAME_LOADED, SAVEGAME_MISSING, or
   SAVEGAME_DAMAGED. After a damaged file the dungeon is not loaded. */
SavegameResult savegame_load(const char *dir);
/* Reads dir/name, else dir/backup when backup is not NULL. Results as savegame_load. */
SavegameResult savegame_load_from(const char *dir, const char *name, const char *backup); /* exported for the tests */
/* The saved game no longer counts: the endgame offers no restart. */
void savegame_forget(void);
/* The quick save of the advanced mode, dir/QUICK.DAT: the saved-game flag and the save times stay as they were (a
   quick save is not a saved game). Returns true on success. */
bool savegame_quick_save(const char *dir);
/* The quick load of the advanced mode, dir/QUICK.DAT: returns the results of savegame_load_from; the saved-game flag
   stays as it was, and after a load both times are 0. */
SavegameResult savegame_quick_load(const char *dir);

#endif
