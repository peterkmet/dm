#ifndef SELECT_H
#define SELECT_H
/* Prefixes: select_ */
#include <stdbool.h>
#include <stdint.h>
#include "draw.h"

/* The start screens in the DM font: adventure, mode, and the CSB entrance. Keys and clicks move a
   cursor over the items. The results go to g_adventure and g_game_mode. No SDL calls. */

enum { SCREEN_ADVENTURE, SCREEN_MODE, SCREEN_CSB, SCREEN_DONE };
enum { CSB_NEW_ADVENTURE, CSB_RESUME, CSB_QUIT };
enum { SELECT_KEY_UP, SELECT_KEY_DOWN, SELECT_KEY_ENTER, SELECT_KEY_ESCAPE };

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
/* The current screen, or SCREEN_DONE. */
int select_screen(void);
/* CSB_NEW_ADVENTURE, CSB_RESUME, or CSB_QUIT: the CSB screen item that ended the selection. */
int select_csb_choice(void);
/* A key: SELECT_KEY_*. */
void select_key(int key);
/* A left click at screen pixel (x, y): activates the enabled item under it. */
void select_click(int x, int y);
/* The click box of item index item on the current screen. Returns false when there is no such item. */
bool select_item_box(int item, Box *box);
/* Draws the current screen on fb with its own palette. The font must be loaded (text_init). */
void select_draw(void);

#endif
