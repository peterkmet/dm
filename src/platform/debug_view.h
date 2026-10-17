/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_DEBUG_VIEW_H
#define DM_PLATFORM_DEBUG_VIEW_H
/* Prefixes: debug_view_ */

/* The developer modes: --show draws one graphic, --view draws the dungeon view with free movement.
   Not part of the game. */

#include <stdbool.h>
#include <stdint.h>

/* Loads the game files of dir and draws graphic (>= 0) or, with graphic -1, the view at the party start. */
bool debug_view_open(const char *dir, int16_t graphic);
/* Free movement in the view: up/down move, left/right turn, A/D strafe; walls block. */
void debug_view_key(int keycode);

#endif
