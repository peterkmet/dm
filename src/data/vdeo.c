/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vdeo.h"
#include "adventure.h"
#include "file.h"
#include "ftl.h"
#include <stdlib.h>
#include <string.h>

/* VDEO.FTL image addresses. */
#define VDEO_BASE           0x100000
#define VDEO_FADE_STEPS     0x100248 /* VDEO_FADE_STEP_COUNT rows of VDEO_FADE_LEVEL_COUNT bytes */
#define VDEO_CSB_FADE_STEPS 0x100254 /* the same table in the VDEO.FTL of CSB */

static struct {
    uint8_t steps[VDEO_FADE_STEP_COUNT][VDEO_FADE_LEVEL_COUNT];
    bool loaded;
} vdeo;

/* Setup */

bool vdeo_load(const char *path)
{
    size_t len;
    FtlImage image = {0};
    uint8_t *raw = file_read(path, &len);
    const uint8_t *steps = NULL;
    if (raw && ftl_load(raw, len, VDEO_BASE, &image))
        steps = ftl_at(&image, g_adventure == ADVENTURE_CSB ? VDEO_CSB_FADE_STEPS : VDEO_FADE_STEPS, sizeof vdeo.steps);
    free(raw);
    vdeo.loaded = steps != NULL;
    if (steps)
        memcpy(vdeo.steps, steps, sizeof vdeo.steps);
    ftl_free(&image);
    return vdeo.loaded;
}

/* Queries */

bool vdeo_is_loaded(void)
{
    return vdeo.loaded;
}

uint8_t vdeo_get_fade_step(int16_t fade_step, int16_t distance)
{
    return vdeo.steps[fade_step][distance];
}
