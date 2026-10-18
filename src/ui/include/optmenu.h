/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_OPTMENU_H
#define DM_UI_OPTMENU_H
/* Prefixes: optmenu_ */

/* The options menu of the advanced mode (not in the original game): a viewport screen with the sound volume,
   the music volume, the music choice and the upscale filter. It changes the values at once and writes SETTINGS.TXT when it closes
   after a change; advanced/ opens and closes the screen. */

#include <stdbool.h>
#include <stdint.h>

/* The upscale filter in use (SETTINGS_FILTER_*). */
extern int16_t g_filter;
/* True when --filter chose the filter: a game start keeps it. platform/ sets it. */
extern bool g_filter_fixed;
/* Shows the image with a filter; false when it fails. platform/ sets it; NULL in the tests. */
extern bool (*g_filter_hook)(int16_t filter);

/* Commands */
/* Selects the upscale filter through g_filter_hook; a failed hook keeps the filter in use. */
void optmenu_set_filter(int16_t filter);
/* Shows the menu in the viewport with the first row selected. */
void optmenu_open(void);
/* A key while the menu is open: Up and Down select a row, Left and Right change its value, Return on
   CONTINUE and Escape close. Returns true when the menu closes. */
bool optmenu_process_key(uint16_t key);
/* A left press while the menu is open: - and + change a volume, the MUSIC and FILTER rows select the next value,
   CONTINUE closes. Returns true when the menu closes. */
bool optmenu_process_click(int16_t x, int16_t y);

#endif
