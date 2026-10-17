/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "swsh.h"
#include "bytes.h"
#include "file.h"
#include <stdlib.h>

/* SWSH.FTL image addresses. */
#define SWSH_BASE           0x100000
#define SWSH_PALETTE_SCRIPT 0x10009a /* the script ends where the logo graphic starts */
#define SWSH_LOGO_GRAPHIC   0x100106
#define SWSH_SAMPLE_PERIOD  0x100a54 /* period word, length long, then the 8-bit sample */
#define SWSH_SAMPLE_LENGTH  0x100a56
#define SWSH_SAMPLE_DATA    0x100a5a

bool swsh_load(const char *path, SwshLogo *logo)
{
    size_t len;
    uint8_t *raw = file_read(path, &len);
    *logo = (SwshLogo){0};
    bool loaded = raw && ftl_load(raw, len, SWSH_BASE, &logo->image) && logo->image.size > SWSH_SAMPLE_DATA - SWSH_BASE;
    free(raw);
    if (!loaded) {
        ftl_free(&logo->image);
        return false;
    }
    const FtlImage *image = &logo->image;
    logo->palette_script = ftl_at(image, SWSH_PALETTE_SCRIPT, SWSH_LOGO_GRAPHIC - SWSH_PALETTE_SCRIPT);
    logo->logo = ftl_at(image, SWSH_LOGO_GRAPHIC, SWSH_SAMPLE_PERIOD - SWSH_LOGO_GRAPHIC);
    logo->palette_script_end = logo->logo;
    logo->logo_bytes = SWSH_SAMPLE_PERIOD - SWSH_LOGO_GRAPHIC;
    logo->period = be16(ftl_at(image, SWSH_SAMPLE_PERIOD, sizeof(uint16_t)));
    logo->sample_length = be32(ftl_at(image, SWSH_SAMPLE_LENGTH, sizeof(uint32_t)));
    if (logo->sample_length > logo->image.size - (SWSH_SAMPLE_DATA - SWSH_BASE))
        logo->sample_length = 0;
    logo->sample = ftl_at(image, SWSH_SAMPLE_DATA, logo->sample_length);
    return true;
}

void swsh_free(SwshLogo *logo)
{
    ftl_free(&logo->image);
    *logo = (SwshLogo){0};
}
