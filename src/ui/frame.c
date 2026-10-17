/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/frame.h"

void (*g_frame_hook)(void);
bool g_quit_requested;
int16_t g_frame_wait_depth;

bool frame_wait(void)
{
    if (!g_frame_hook || g_quit_requested)
        return false;
    g_frame_wait_depth++;
    g_frame_hook();
    g_frame_wait_depth--;
    return !g_quit_requested;
}

void frame_wait_count(int16_t count)
{
    while (count-- > 0 && frame_wait())
        ;
}
