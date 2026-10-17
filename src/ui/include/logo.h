/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_LOGO_H
#define DM_UI_LOGO_H
/* Prefixes: logo_ */

/* The FTL logo of SWSH.FTL (not KAOS): the original shows it before the DM title. */

#include <stdbool.h>

/* Shows the logo with its sound until the sound ends. Returns false when the file cannot be read. */
bool logo_play(const char *path);

#endif
