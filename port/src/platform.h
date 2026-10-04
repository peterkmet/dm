#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include "video.h"

enum { PLAT_NONE, PLAT_QUIT, PLAT_KEY, PLAT_MOUSE, PLAT_MOUSE_UP };

typedef struct {
    int type;
    int key;          /* SDL keycode for PLAT_KEY */
    int modifiers;    /* SDL key modifiers for PLAT_KEY */
    int button, x, y; /* PLAT_MOUSE and PLAT_MOUSE_UP, x/y in 320x200 game space */
} PlatEvent;

int plat_init(int scale);
int plat_set_filter(int factor);
void plat_quit(void);
void plat_present(void);
int plat_poll(PlatEvent *e);
void plat_wait_tick(void);
void plat_audio_pump(void);
/* The mouse pointer, drawn over the game image: w x h pixels, color 0 transparent, other colors
   from the 16 Amiga colors. NULL px shows no pointer. */
void plat_set_pointer(const uint8_t *px, int w, int h, int hot_x, int hot_y, const uint16_t *colors);
void plat_mouse_position(int *x, int *y);

#endif
