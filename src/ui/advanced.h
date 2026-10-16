/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ADVANCED_H
#define DM_UI_ADVANCED_H
/* Prefixes: advanced_ */
#include "engine/game.h"
#include <stdbool.h>
#include <stdint.h>

/* Game modes and the advanced features (not in the original game): automap with a position read-out,
   quick save and quick load, WASD/QE keys, rune symbols on scrolls, coloured action icons, item stat
   meters, Escape and Tab close the inventory, the whole party drinks at a fountain.
   The game rules are the same in every mode. Plan: docs/superpowers/archive/2026-10-04-modes-plan.md. */

/* Key words of the advanced keys: Amiga raw key in the high byte. No original key table has them. */
#define KEY_TAB 0x4200
#define KEY_M   0x3700
#define KEY_F5  0x5400
#define KEY_F9  0x5800
#define KEY_L   0x2800
#define KEY_I   0x1700
#define KEY_F   0x2300
#define KEY_1   0x0100
#define KEY_2   0x0200
#define KEY_3   0x0300

extern bool g_advanced_features; /* set outside vanilla: the advanced features show */
extern bool g_automap_open;      /* set: the automap covers the viewport and the game time stops */

/* Handles M, Tab, F5 (quick save), F9 (quick load), and the keys while the automap is open (L, I, and F
   switch the cheats, 1, 2, and 3 select the music in the advanced mode).
   Returns true when the key is used up. */
bool advanced_process_key(uint16_t key);
/* A mouse button while the automap is open: a left press on the map starts a drag, the left release
   ends it. Returns true when the automap is open. */
bool advanced_process_click(int16_t x, int16_t y, int16_t kind);
/* The mouse is at (x, y): during a drag the map follows it in whole squares. */
void advanced_drag(int16_t x, int16_t y);
/* The movement key word of a letter key outside vanilla: W, S, A, D move, Q and E turn. Else 0. */
uint16_t advanced_letter_key(int letter);
/* Draws the automap and the position read-out (level, position, facing, light) in place of the
   viewport with the interface palette. */
void advanced_draw_automap(void);
/* A white line in the message area. */
void advanced_print_message(const char *text);

#endif
