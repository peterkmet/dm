#ifndef PLATFORM_H
#define PLATFORM_H
/* Prefixes: plat_ */
#include <stdint.h>
#include "video.h"

enum { PLAT_NONE, PLAT_QUIT, PLAT_KEY, PLAT_MOUSE, PLAT_MOUSE_UP };

typedef struct {
    int type;
    int key;          /* SDL keycode for PLAT_KEY */
    int modifiers;    /* SDL key modifiers for PLAT_KEY */
    int button, x, y; /* PLAT_MOUSE and PLAT_MOUSE_UP, x and y in game screen pixels */
} PlatEvent;

/* Opens the window at scale times the game screen. DM_FILTER selects the upscale filter. */
int plat_init(int scale);
/* The upscale factor of a filter name: "scale2x" 2, "scale3x" 3, else 1 (none). */
int plat_filter_factor(const char *name);
int plat_set_filter(int factor);
void plat_quit(void);
/* Shows fb with the split palette and the pointer, scaled into the window. */
void plat_present(void);
/* Returns 1 with the next key, mouse button, or quit event; mouse motion only moves the pointer. */
int plat_poll(PlatEvent *e);
void plat_wait_tick(void);
void plat_audio_pump(void);
/* The mouse pointer, drawn over the game image: w x h pixels, color 0 transparent, other colors
   from the 16 Amiga colors. NULL px shows no pointer. */
void plat_set_pointer(const uint8_t *px, int w, int h, int hot_x, int hot_y, const uint16_t *colors);
void plat_mouse_position(int *x, int *y);
/* The window and Dock icon: w x h pixels, color transparent see-through, the others from colors. */
void plat_set_icon(const uint8_t *px, int w, int h, int transparent, const uint16_t *colors);

#endif
