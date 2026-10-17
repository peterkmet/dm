/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ANIM_H
#define DM_UI_ANIM_H
/* Prefixes: anim_ */

/* The animation player of ANIM.FTL for the chunk files (CSB ENDA.DAT). Format: docs/csb/data.md
   "ENDA.DAT and the endgame". */

#include <stdbool.h>
#include <stdint.h>

/* Result of anim_next_frame: no frame left. */
#define ANIM_END (-1)

/* Stops the sound and frees the animation file and the files it loaded. */
void anim_close(void); /* exported for the tests */
/* Reads an animation file. Its chunks must end exactly at the file end. Returns true on success. */
bool anim_open(const char *path); /* exported for the tests */
/* Runs the chunks up to the next frame and draws the frame into g_screen with its palette. Returns the
   frames (50 Hz) that the frame stays, or ANIM_END. */
int16_t anim_next_frame(void); /* exported for the tests */
/* Plays an animation file on the screen to its end. Returns false when the file cannot be read. */
bool anim_play(const char *path);
/* True: a key or a click ends the animation at once, not after the time of the shown frame (advanced CSB intro). */
void anim_set_stop_at_once(bool at_once);

#endif
