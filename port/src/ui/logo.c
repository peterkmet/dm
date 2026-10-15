#include <stdlib.h>
#include <string.h>
#include "logo.h"
#include "bytes.h"
#include "ftl.h"
#include "graphics.h"
#include "music.h"
#include "sound.h"
#include "startend.h"
#include "ui.h"
#include "video.h"

/* SWSH.FTL image addresses. */
#define SWSH_BASE          0x100000
#define PALETTE_SCRIPT     0x10009a /* pairs of words: color index and color; index -1: show the colors, wait frames */
#define PALETTE_SCRIPT_END 0x100106
#define LOGO_GRAPHIC       0x100106 /* 320 x 200, Graphics.DAT packed format */
#define SAMPLE_PERIOD      0x100a54 /* period word, length long, then the 8-bit sample */
#define SAMPLE_LENGTH      0x100a56
#define SAMPLE_DATA        0x100a5a

#define SCRIPT_SHOW        (-1)
#define SCRIPT_ENTRY_BYTES 4

int logo_play(const char *path)
{
    static const uint8_t volumes[2] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    size_t len;
    uint8_t *raw = read_file(path, &len);
    FtlImage img = {0};
    int loaded = raw && !ftl_load(raw, len, SWSH_BASE, &img) && img.size > SAMPLE_DATA - SWSH_BASE;
    free(raw);
    if (!loaded)
        return ftl_free(&img), -1;
    const uint8_t *at = img.data - SWSH_BASE;
    uint16_t period = BE16(at + SAMPLE_PERIOD);
    uint32_t length = BE32(at + SAMPLE_LENGTH);
    if (length > img.size - (SAMPLE_DATA - SWSH_BASE))
        length = 0;
    uint16_t colors[PALETTE_COLORS] = {0};
    Bitmap screen = ui_screen();
    memset(fb, 0, sizeof fb);
    ui_set_screen_palette(colors);
    gfx_expand_over(at + LOGO_GRAPHIC, SAMPLE_PERIOD - LOGO_GRAPHIC, &screen);
    sound_play_data((const int8_t *)at + SAMPLE_DATA, (uint16_t)length, period,
                    volumes); /* the voice takes a word length */
    /* the module waits for the end of the sample */
    int32_t frames = (int32_t)(((uint64_t)length * period * FRAMES_PER_SECOND + SOUND_PAL_CLOCK - 1) / SOUND_PAL_CLOCK);
    for (uint32_t e = PALETTE_SCRIPT; e < PALETTE_SCRIPT_END; e += SCRIPT_ENTRY_BYTES) {
        int16_t index = (int16_t)BE16(at + e);
        uint16_t value = BE16(at + e + 2);
        if (index != SCRIPT_SHOW) {
            if (index >= 0 && index < PALETTE_COLORS)
                colors[index] = value;
            continue;
        }
        ui_set_screen_palette(colors);
        for (uint16_t f = 0; f < value && wait_frame(); f++)
            frames--;
    }
    while (frames-- > 0 && wait_frame())
        ;
    sound_stop_all(); /* the voice points into the image */
    music_stop();
    ftl_free(&img);
    return 0;
}
