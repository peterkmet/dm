/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_FRAME_H
#define DM_UI_FRAME_H
/* Prefixes: frame_ */

/* The frame wait of the blocking sequences: dialogs, the entrance, the endgame, the animations. The program
   shows each frame through g_frame_hook. */

#include <stdbool.h>
#include <stdint.h>

/* Shows one 50 Hz frame and reads the input into the command queue; it runs no game logic, so a wait inside a
   game tick is safe. NULL (tests, turn-based mode): waits end at once and waits for input end without input. */
extern void (*g_frame_hook)(void);
extern bool g_quit_requested;      /* the player quits: the program ends */
extern int16_t g_frame_wait_depth; /* not 0 while frame_wait runs a frame */

/* Returns true after one frame, false when there are no frames or the player quits. */
bool frame_wait(void);
/* Waits count frames, or fewer when frame_wait shows no frame. */
void frame_wait_count(int16_t count);

#endif
