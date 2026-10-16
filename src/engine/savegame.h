#ifndef SAVEGAME_H
#define SAVEGAME_H
/* Prefixes: savegame_ */
#include <stdbool.h>
#include <stdint.h>

/* Game state to and from the save file of the adventure, DMGAME.DAT or CSBGAME.DAT (KAOS save_game and load_game, LOADSAVE.C).
   savefile.c owns the container; this module owns the block contents. Layout: docs/save-format.md. */

enum { SAVEGAME_DAMAGED = -1, SAVEGAME_MISSING = 0, SAVEGAME_LOADED = 1 };
enum { ADVENTURE_FAILED, ADVENTURE_MADE, ADVENTURE_NO_CHAMPIONS, ADVENTURE_NAMES_NOT_UNIQUE };

extern bool g_saved_game_exists; /* set after a save or a load: the endgame offers a restart */
extern uint32_t g_last_save_game_time, g_last_load_game_time;

/* Writes the game to dir/name. With a backup name, the old file becomes dir/backup first. Returns true on success. */
bool savegame_save_as(const char *dir, const char *name, const char *backup);
/* Reads dir/name, else dir/backup when backup is not NULL. Results as savegame_load. */
int16_t savegame_load_from(const char *dir, const char *name, const char *backup);
/* Writes the game to dir/DMGAME.DAT (CSB: CSBGAME.DAT). The old file becomes dir/DMGAME.BAK (CSBGAME.BAK)
   first. Returns 1 on success. */
int16_t savegame_save(const char *dir);
/* Reads dir/DMGAME.DAT, else dir/DMGAME.BAK (CSB: CSBGAME.DAT, CSBGAME.BAK), into the game state: dungeon, globals, active groups,
   champions, party, timeline, and portraits. Returns SAVEGAME_LOADED, SAVEGAME_MISSING, or
   SAVEGAME_DAMAGED. After a damaged file the dungeon is not loaded. */
int16_t savegame_load(const char *dir);
/* The saved game no longer counts: the endgame offers no restart. */
void savegame_forget(void);
/* The quick save and load of the advanced mode, dir/QUICK.DAT: the saved-game flag and the save times stay
   as they were (a quick save is not a saved game). savegame_quick_save returns 1 on success; savegame_quick_load
   returns the results of savegame_load_from, and after a load both times are 0. */
int16_t savegame_quick_save(const char *dir);
int16_t savegame_quick_load(const char *dir);

/* CSB prison (CEDT MAKE NEW ADVENTURE): puts the party into the CSB adventure of mini_path (MINI.DAT),
   writes it to dir/CSBGAME.DAT (the old file becomes CSBGAME.BAK), and loads it. Returns ADVENTURE_MADE,
   ADVENTURE_NO_CHAMPIONS or ADVENTURE_NAMES_NOT_UNIQUE (nothing written), or ADVENTURE_FAILED. */
int16_t savegame_make_adventure(const char *dir, const char *mini_path);

#endif
