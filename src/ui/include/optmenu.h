/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_OPTMENU_H
#define DM_UI_OPTMENU_H
/* Prefixes: optmenu_ */

/* The options menu of the advanced mode (not in the original game): a viewport screen with the sound volume,
   the music volume and the music choice. It changes the values at once and writes SETTINGS.TXT when it closes
   after a change; advanced/ opens and closes the screen. */

#include <stdbool.h>
#include <stdint.h>

/* Commands */
/* Shows the menu in the viewport with the first row selected. */
void optmenu_open(void);
/* A key while the menu is open: Up and Down select a row, Left and Right change its value, Return on
   CONTINUE and Escape close. Returns true when the menu closes. */
bool optmenu_process_key(uint16_t key);
/* A left press while the menu is open: - and + change a volume, the MUSIC row selects the next music,
   CONTINUE closes. Returns true when the menu closes. */
bool optmenu_process_click(int16_t x, int16_t y);

#endif
