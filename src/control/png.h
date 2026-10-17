/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_PNG_H
#define DM_CONTROL_PNG_H
/* Prefixes: png_ */

/* The screenshot of the program player: the game screen as an RGB PNG with stored deflate blocks. */

#include <stdbool.h>

/* Writes the game screen in its colors to path; false when the file cannot be written. */
bool png_write(const char *path);

#endif
