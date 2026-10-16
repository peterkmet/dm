/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_ENTRANCE_H
#define DM_UI_ENTRANCE_H
/* Prefixes: entrance_ */
/* The entrance of Dungeon Master: the doors, the ENTER and RESUME buttons, the door animation. */

#include <stdbool.h>
#include <stdint.h>

/* The button that ended the entrance (g_entrance_choice). */
#define ENTRANCE_RESUME   0
#define ENTRANCE_NEW_GAME 1
#define ENTRANCE_WAITING  99
extern int16_t g_entrance_choice; /* ENTRANCE_WAITING until a button is pressed */

/* Shows the entrance: graphic 4 with the closed doors and the ENTER, RESUME, and QUIT buttons, in the
   entrance palette on the whole screen. Deviation: no credits. */
void entrance_draw(void);
/* ENTER or RESUME was clicked: shows the pressed button, plays the switch sound, and waits 20 frames. */
void entrance_press_button(bool resume);
/* After ENTER the doors slide apart, 2 pixels each 2 frames, over the dungeon view in the entrance palette.
   The door sound plays at the start and every 40 frames. Needs the frames of g_frame_hook.
   Deviation: the doors open over the view of the start square; the original draws a made-up corridor. */
void entrance_open_doors(void);

#endif
