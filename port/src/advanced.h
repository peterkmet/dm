#ifndef ADVANCED_H
#define ADVANCED_H
#include <stdint.h>

/* Game modes and the advanced features (not in the original game): automap, debug read-out,
   rune symbols on scrolls, coloured action icons, item stat meters, Escape closes the inventory.
   The game rules are the same in every mode. Plan: docs/superpowers/2026-10-04-modes-plan.md. */

enum { GAME_MODE_VANILLA, GAME_MODE_ADVANCED, GAME_MODE_TURNBASED };

/* Key words of the advanced keys: Amiga raw key in the high byte. No original key table has them. */
enum { KEY_TAB = 0x4200, KEY_M = 0x3700, KEY_F5 = 0x5400, KEY_F6 = 0x5500 };

extern int16_t g_game_mode;            /* GAME_MODE_VANILLA by default */
extern int16_t g_advanced_features;    /* 1: the advanced features show; F6 toggles them outside vanilla */
extern int16_t g_automap_open;         /* 1: the automap covers the viewport and the game time stops */
extern int16_t g_debug_readout;        /* 1: F5 read-out in the viewport */

/* Handles M, Tab, F5, F6, and the keys while the automap is open. Returns 1 when the key is used up. */
int advanced_process_key(uint16_t key);
/* Draws the debug read-out (level, position, facing, light) over the viewport on the screen. */
void advanced_draw_debug(int16_t light);
/* Draws the automap in place of the viewport with the interface palette. */
void advanced_draw_automap(void);

#endif
