/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ADVANCED_H
#define DM_UI_ADVANCED_H
/* Prefixes: advanced_ */

/* Game modes and the advanced features (not in the original game): automap with a position read-out,
   quick save and quick load, WASD/QE keys, rune symbols on scrolls, coloured action icons, item stat
   meters, Escape and Tab close the inventory, the whole party drinks at a fountain.
   The game rules are the same in every mode. Plan: docs/superpowers/archive/2026-10-04-modes-plan.md. */

#include <stdbool.h>
#include <stdint.h>

/* The viewport screens of g_advanced_screen: at most one covers the viewport, and the game time stops while it
   is open. */
#define ADVANCED_SCREEN_NONE    0
#define ADVANCED_SCREEN_AUTOMAP 1

extern bool g_advanced_features;  /* set outside vanilla: the advanced features show */
extern int16_t g_advanced_screen; /* the open viewport screen, ADVANCED_SCREEN_NONE for the dungeon view */

/* The mouse is at (x, y): during a drag the map follows it in whole squares. */
void advanced_drag(int16_t x, int16_t y);
/* A mouse button while a viewport screen is open: on the automap a left press on the map starts a drag, the
   left release ends it. Returns true when a screen is open: it uses up every click. */
bool advanced_process_click(int16_t x, int16_t y, int16_t kind);
/* A white line in the message area. */
void advanced_print_message(const char *text);
/* Handles M, Tab, F5 (quick save), F9 (quick load), and the keys while the automap is open (L, I, and F
   switch the cheats, 1, 2, and 3 select the music in the advanced mode).
   Returns true when the key is used up. */
bool advanced_process_key(uint16_t key);
/* The movement key word of a letter key outside vanilla: W, S, A, D move, Q and E turn. Else 0. */
uint16_t advanced_letter_key(int letter);

#endif
