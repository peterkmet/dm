/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/logo.h"
#include "data/bytes.h"
#include "data/graphics.h"
#include "data/swsh.h"
#include "engine/sound.h"
#include "ui/audio.h"
#include "ui/frame.h"
#include "ui/music.h"
#include "ui/ui.h"
#include "ui/video.h"
#include <string.h>

#define SCRIPT_SHOW        (-1)
#define SCRIPT_ENTRY_BYTES 4

int logo_play(const char *path)
{
    static const uint8_t volumes[2] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    SwshLogo logo;
    if (!swsh_load(path, &logo))
        return -1;
    uint16_t period = logo.period;
    uint32_t length = logo.sample_length;
    uint16_t colors[PALETTE_COLORS] = {0};
    Bitmap screen = video_get_screen();
    memset(g_screen.px, 0, sizeof g_screen.px);
    ui_set_screen_palette(colors);
    gfx_expand_over(logo.logo, logo.logo_bytes, &screen);
    audio_play_data((const int8_t *)logo.sample, (uint16_t)length, period, volumes); /* the voice takes a word length */
    /* the module waits for the end of the sample */
    int32_t frames = ((uint64_t)length * period * FRAMES_PER_SECOND + AUDIO_PAL_CLOCK - 1) / AUDIO_PAL_CLOCK;
    for (const uint8_t *e = logo.palette_script; e < logo.palette_script_end; e += SCRIPT_ENTRY_BYTES) {
        int16_t index = be16(e);
        uint16_t value = be16(e + 2);
        if (index != SCRIPT_SHOW) {
            if (index >= 0 && index < PALETTE_COLORS)
                colors[index] = value;
            continue;
        }
        ui_set_screen_palette(colors);
        for (uint16_t f = 0; f < value && frame_wait(); f++)
            frames--;
    }
    while (frames-- > 0 && frame_wait())
        ;
    music_stop();
    swsh_free(&logo);
    return 0;
}
