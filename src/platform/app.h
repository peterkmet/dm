/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_APP_H
#define DM_PLATFORM_APP_H
/* Prefixes: app_ */

/* The application: the start screens, the choice of the adventure and the mode, the frame loop, and the turn-based
   socket. It owns the run state; the game rules stay in engine/ and the screens in ui/. */

#include "platform/options.h"
#include <stdbool.h>

/* Result of app_run_frame. */
typedef enum {
    APP_FRAME_NEXT,  /* run the next frame */
    APP_FRAME_QUIT,  /* the player quit */
    APP_FRAME_FAILED /* the game files or the socket cannot be opened: exit code 1 */
} AppFrame;

/* Setup */
/* Installs the disk images, checks the adventure, opens the window and the first screen; false on an error. */
bool app_open(const Options *options);
/* Closes the socket and the window. */
void app_close(void);

/* Commands */
/* One frame: the input, the socket, the start screens or the game, and the picture. */
AppFrame app_run_frame(void);

#endif
