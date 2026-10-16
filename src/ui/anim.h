/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ANIM_H
#define DM_UI_ANIM_H
/* Prefixes: anim_ */
#include <stdint.h>

/* The animation player of ANIM.FTL for the chunk files (CSB ENDA.DAT). Format: docs/csb/data.md
   "ENDA.DAT and the endgame". */

#define ANIM_END (-1) /* anim_next_frame: no frame left */

/* Reads an animation file. Its chunks must end exactly at the file end. Returns 0 or -1. */
int anim_open(const char *path); /* exported for the tests */
/* Runs the chunks up to the next frame and draws the frame into g_screen with its palette. Returns the
   frames (50 Hz) that the frame stays, or ANIM_END. */
int16_t anim_next_frame(void); /* exported for the tests */
void anim_close(void);         /* exported for the tests */
/* Plays an animation file on the screen to its end. Returns -1 when the file cannot be read. */
int anim_play(const char *path);

#endif
