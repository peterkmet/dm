/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_CHEATS_H
#define DM_UI_CHEATS_H
/* Prefixes: cheats_ */

/* The cheat screen of the advanced mode (not in the original game): a viewport screen that switches the cheats,
   offers quick save and quick load, and lists the spells of the loaded game with their runes; a spell line casts
   its spell (engine/magic). It switches the cheats through engine/game; advanced/ opens and closes the screen and runs quick save and quick load. */

#include "tables.h"
#include <stddef.h>
#include <stdint.h>

/* What a key or a click on the cheat screen asks for. */
typedef enum {
    CHEATS_STAY,       /* the screen stays open */
    CHEATS_CLOSE,      /* the screen closes */
    CHEATS_QUICK_SAVE, /* a quick save; the screen stays open */
    CHEATS_QUICK_LOAD  /* the screen closes, then a quick load */
} CheatsResult;

/* Queries */
/* The text line of a spell: the runes as names, as numbers (X for any power), then the spell name, for example
   "FUL IR (X,4,4) FIREBALL"; a line wider than the viewport has no rune names. (exported for the tests) */
void cheats_get_spell_line(const Spell *spell, char *line, size_t size);

/* Commands */
/* Shows the cheat page in the viewport with the first row selected. */
void cheats_open(void);
/* A key while the screen is open: Up and Down select a row, Return switches or runs it (a spell line casts the
   spell and closes the screen), F5 and F9 ask for the quick save and the quick load, C and Escape close. */
CheatsResult cheats_process_key(uint16_t key);
/* A left press while the screen is open: a click on a row switches the cheat, runs the item, or casts the spell. */
CheatsResult cheats_process_click(int16_t x, int16_t y);

#endif
