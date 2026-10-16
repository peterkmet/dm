/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_SWSH_H
#define DM_DATA_SWSH_H
/* Prefixes: swsh_ */

/* The FTL logo module SWSH.FTL: a palette script, the logo graphic, and its sound. */

#include "data/ftl.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const uint8_t *palette_script, *palette_script_end; /* pairs of words: color index and color; index -1: show */
    const uint8_t *logo;                                /* 320 x 200, Graphics.DAT packed format */
    uint32_t logo_bytes;
    const uint8_t *sample; /* 8-bit signed sample */
    uint32_t sample_length;
    uint16_t period;
    FtlImage image; /* the loaded module that the pointers point into */
} SwshLogo;

/* Loads SWSH.FTL from path. Returns false when the file cannot be read or is too short. */
bool swsh_load(const char *path, SwshLogo *out);
/* Frees the module of a loaded logo. */
void swsh_free(SwshLogo *logo);

#endif
