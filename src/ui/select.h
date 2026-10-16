/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_SELECT_H
#define DM_UI_SELECT_H
/* Prefixes: select_ */

/* The start screens in the DM font: adventure, mode, and the CSB entrance. Keys and clicks move a
   cursor over the items. The results go to g_adventure and g_session.mode. No SDL calls. */

#include "data/graphics.h"
#include <stdbool.h>
#include <stdint.h>

/* Selection screens. */
#define SELECT_SCREEN_ADVENTURE 0
#define SELECT_SCREEN_MODE      1
#define SELECT_SCREEN_CSB       2
#define SELECT_SCREEN_DONE      3

/* Choices of the CSB screen. */
#define SELECT_CSB_NEW_ADVENTURE 0
#define SELECT_CSB_RESUME        1
#define SELECT_CSB_QUIT          2

/* Keys of the selection screens. */
#define SELECT_KEY_UP     0
#define SELECT_KEY_DOWN   1
#define SELECT_KEY_ENTER  2
#define SELECT_KEY_ESCAPE 3

/* Starts at the first screen that --adventure and --mode do not skip; adventure and mode are -1
   when the flag is absent. --mode alone means DM. csb_present 0 disables CHAOS STRIKES BACK.
   llm_mode_available false removes the LLM item from the mode screen. */
void select_start(int16_t adventure, int16_t mode, bool csb_present, bool llm_mode_available);
/* Shows the CSB screen again, with a message line below the items when message is not NULL. After
   it, Escape does not go back to the mode screen. */
void select_start_csb(const char *message);
/* QUIT on the DM entrance or the CSB screen: the first screen that --adventure and --mode do not skip.
   Returns false when the flags skip every screen: then there is no screen to go back to. */
bool select_restart(void);
/* The current screen, or SELECT_SCREEN_DONE. */
int16_t select_screen(void);
/* SELECT_CSB_NEW_ADVENTURE, SELECT_CSB_RESUME, or SELECT_CSB_QUIT: the CSB screen item that ended the selection. */
int16_t select_csb_choice(void);
/* A key: SELECT_KEY_*. */
void select_key(int16_t key);
/* The click box of item index item on the current screen. Returns false when there is no such item. */
bool select_item_box(int16_t item, Box *box); /* exported for the tests */
/* A left click at screen pixel (x, y): activates the enabled item under it. */
void select_click(int16_t x, int16_t y);
/* Draws the current screen on g_screen with its own palette. The font must be loaded (text_init). */
void select_draw(void);

#endif
