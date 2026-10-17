/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_PALETTE_H
#define DM_UI_PALETTE_H
/* Prefixes: palette_ */

/* The palette fade of the original: it changes both palette halves of g_screen step by step, one frame for each
   step through g_frame_hook. It must not draw. */

#include <stdint.h>

/* Commands */
/* Fades both palette halves from their colors to colors (16 colors, for both halves) in 15 steps of one frame.
   Without frames (tests, turn-based mode) or without VDEO.FTL the colors apply at once. */
void palette_fade_to(const uint16_t *colors);
/* Fades both palette halves to black. */
void palette_fade_to_black(void);

#endif
