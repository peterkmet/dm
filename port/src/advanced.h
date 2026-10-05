#ifndef ADVANCED_H
#define ADVANCED_H
#include <stdint.h>

/* Game modes and the advanced features (not in the original game): automap with a position read-out,
   quick save and quick load, WASD/QE keys, rune symbols on scrolls, coloured action icons, item stat
   meters, Escape and Tab close the inventory, the whole party drinks at a fountain.
   The game rules are the same in every mode. Plan: docs/superpowers/2026-10-04-modes-plan.md. */

enum { GAME_MODE_VANILLA, GAME_MODE_ADVANCED, GAME_MODE_TURNBASED };

/* Key words of the advanced keys: Amiga raw key in the high byte. No original key table has them. */
enum { KEY_TAB = 0x4200, KEY_M = 0x3700, KEY_F5 = 0x5400, KEY_F9 = 0x5800, KEY_L = 0x2800, KEY_I = 0x1700, KEY_F = 0x2300,
       KEY_1 = 0x0100, KEY_2 = 0x0200, KEY_3 = 0x0300 };

#define QUICK_SAVE_FILE_NAME "QUICK.DAT"

extern int16_t g_game_mode;            /* GAME_MODE_VANILLA by default */
extern int16_t g_advanced_features;    /* 1 outside vanilla: the advanced features show */
extern int16_t g_automap_open;         /* 1: the automap covers the viewport and the game time stops */
/* Cheats of the advanced mode, L, I, and F on the automap. Not saved; a new or loaded game turns them off. */
extern int16_t g_cheat_light;          /* the view draws in full light; the creature rules keep the real light */
extern int16_t g_cheat_immortal;       /* the health of a champion stops at 1 */
extern int16_t g_cheat_food;           /* food and water do not go down */

/* Handles M, Tab, F5 (quick save), F9 (quick load), and the keys while the automap is open (L, I, and F
   switch the cheats, 1, 2, and 3 select the music in the advanced mode).
   Returns 1 when the key is used up. */
int advanced_process_key(uint16_t key);
/* A left click while the automap is open: the map scrolls so the clicked square is at the center.
   Returns 1 when the automap is open. */
int advanced_process_click(int16_t x, int16_t y);
/* The movement key word of a letter key outside vanilla: W, S, A, D move, Q and E turn. Else 0. */
uint16_t advanced_letter_key(int letter);
/* Draws the automap and the position read-out (level, position, facing, light) in place of the
   viewport with the interface palette. */
void advanced_draw_automap(void);
/* A white line in the message area. */
void advanced_print_message(const char *text);

#endif
