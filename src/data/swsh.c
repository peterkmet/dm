/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/swsh.h"
#include "data/bytes.h"
#include "data/file.h"
#include <stdlib.h>

/* SWSH.FTL image addresses. */
#define SWSH_BASE           0x100000
#define SWSH_PALETTE_SCRIPT 0x10009a /* the script ends where the logo graphic starts */
#define SWSH_LOGO_GRAPHIC   0x100106
#define SWSH_SAMPLE_PERIOD  0x100a54 /* period word, length long, then the 8-bit sample */
#define SWSH_SAMPLE_LENGTH  0x100a56
#define SWSH_SAMPLE_DATA    0x100a5a

/* Reads the module and points the logo fields into it. */
bool swsh_load(const char *path, SwshLogo *out)
{
    size_t len;
    uint8_t *raw = file_read(path, &len);
    *out = (SwshLogo){0};
    bool loaded = raw && !ftl_load(raw, len, SWSH_BASE, &out->image) && out->image.size > SWSH_SAMPLE_DATA - SWSH_BASE;
    free(raw);
    if (!loaded) {
        ftl_free(&out->image);
        return false;
    }
    const uint8_t *at = out->image.data - SWSH_BASE;
    out->palette_script = at + SWSH_PALETTE_SCRIPT;
    out->logo = at + SWSH_LOGO_GRAPHIC;
    out->palette_script_end = out->logo;
    out->logo_bytes = SWSH_SAMPLE_PERIOD - SWSH_LOGO_GRAPHIC;
    out->period = BE16(at + SWSH_SAMPLE_PERIOD);
    out->sample_length = BE32(at + SWSH_SAMPLE_LENGTH);
    if (out->sample_length > out->image.size - (SWSH_SAMPLE_DATA - SWSH_BASE))
        out->sample_length = 0;
    out->sample = at + SWSH_SAMPLE_DATA;
    return true;
}

/* Frees the module and clears the logo. */
void swsh_free(SwshLogo *logo)
{
    ftl_free(&logo->image);
    *logo = (SwshLogo){0};
}
