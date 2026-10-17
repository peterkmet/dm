/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_VDEO_H
#define DM_DATA_VDEO_H
/* Prefixes: vdeo_ */

/* The video driver module VDEO.FTL: the step table of the palette fade. It keeps a copy of the table, not the module. */

#include <stdbool.h>
#include <stdint.h>

/* The file name of the video driver, in the game data folder of DM and CSB. */
#define VDEO_FILE "VDEO.FTL"

/* Size of the fade step table: steps of one fade, and the levels of a 4-bit color component. */
#define VDEO_FADE_STEP_COUNT  15
#define VDEO_FADE_LEVEL_COUNT 16

/* Setup */
/* Loads the fade step table of the VDEO.FTL of g_adventure from path. False when the file cannot be read or is
   too short. */
bool vdeo_load(const char *path);

/* Queries */
/* True when a fade step table is loaded. */
bool vdeo_is_loaded(void);
/* The level move (0 or 1) of a color component at this fade step for this distance to the target. Each
   distance column sums to the distance over all steps. */
uint8_t vdeo_get_fade_step(int16_t fade_step, int16_t distance);

#endif
