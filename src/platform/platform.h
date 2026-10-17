/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_PLATFORM_H
#define DM_PLATFORM_PLATFORM_H
/* Prefixes: plat_ */
#include "ui/video.h"
#include <stdbool.h>
#include <stdint.h>

#define PLAT_NONE     0
#define PLAT_QUIT     1
#define PLAT_KEY      2
#define PLAT_MOUSE    3
#define PLAT_MOUSE_UP 4

typedef struct {
    int type;
    int key;          /* SDL keycode for PLAT_KEY */
    int modifiers;    /* SDL key modifiers for PLAT_KEY */
    int button, x, y; /* PLAT_MOUSE and PLAT_MOUSE_UP, x and y in game screen pixels */
} PlatEvent;

/* Opens the window at scale times the game screen. */
bool plat_init(int scale);
/* The upscale factor of a filter name: "scale2x" 2, "scale3x" 3, else 1 (none). */
int plat_filter_factor(const char *name);
/* Selects the upscale filter (1 none, 2 Scale2x, 3 Scale3x) and recreates the texture. */
bool plat_set_filter(int factor);
void plat_quit(void);
/* Shows g_screen with the split palette and the pointer, scaled into the window. */
void plat_present(void);
/* Returns true with the next key, mouse button, or quit event; mouse motion only moves the pointer. */
bool plat_poll(PlatEvent *event);
/* Sleeps until the next 50 Hz tick. */
void plat_wait_tick(void);
/* Keeps about three ticks of mixer output queued; runs on the game thread, so the mixer needs no lock. */
void plat_audio_pump(void);
/* The mouse pointer, drawn over the game image: w x h pixels, color 0 transparent, other colors
   from the 16 Amiga colors. NULL px shows no pointer. */
void plat_set_pointer(const uint8_t *px, int w, int h, int hot_x, int hot_y, const uint16_t *colors);
void plat_mouse_position(int *x, int *y);
/* The window and Dock icon: w x h pixels, color transparent see-through, the others from colors. */
void plat_set_icon(const uint8_t *px, int w, int h, int transparent, const uint16_t *colors);

#endif
