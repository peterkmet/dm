/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/palette.h"
#include "engine/arith.h"
#include "ui/frame.h"
#include "ui/video.h"
#include <string.h>

/* Steps of one fade, and the levels of a 4-bit color component. */
#define FADE_STEPS     15
#define CHANNEL_LEVELS 16

/* The step table of the video driver: step row, component difference column; 1 moves the component one level.
   Column k sums to k, so after the last step each component is at the target. */
static const uint8_t step_table[FADE_STEPS][CHANNEL_LEVELS] = {
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1},
    {0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1},
    {0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

/* Commands */
/* One step of a 4-bit component: 0 or 1 level from work towards to, by the difference of start and to. */
static int16_t step_channel(int16_t step, uint16_t work, uint16_t start, uint16_t to)
{
    int16_t difference = (int16_t)(to - start);
    int16_t move = step_table[step][abs16(difference)];
    return (int16_t)(difference < 0 ? work - move : work + move);
}

void palette_fade_to(const uint16_t *colors)
{
    uint16_t start[2 * PALETTE_COLORS];
    memcpy(start, g_screen.palette, sizeof start);
    for (int16_t step = 0; step < FADE_STEPS; step++) {
        for (int16_t i = 0; i < 2 * PALETTE_COLORS; i++) {
            uint16_t work = g_screen.palette[i], from = start[i], to = colors[i & COLOR_INDEX_MASK];
            g_screen.palette[i] = video_amiga_color(
                (uint16_t)step_channel(step, video_amiga_red(work), video_amiga_red(from), video_amiga_red(to)),
                (uint16_t)step_channel(step, video_amiga_green(work), video_amiga_green(from), video_amiga_green(to)),
                (uint16_t)step_channel(step, video_amiga_blue(work), video_amiga_blue(from), video_amiga_blue(to)));
        }
        if (!frame_wait())
            break;
    }
    for (int16_t i = 0; i < 2 * PALETTE_COLORS; i++) /* the target, also when the frames end early */
        g_screen.palette[i] = colors[i & COLOR_INDEX_MASK];
}

void palette_fade_to_black(void)
{
    static const uint16_t black[PALETTE_COLORS];
    palette_fade_to(black);
}
