#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include "video.h"

enum { PLAT_NONE, PLAT_QUIT, PLAT_KEY, PLAT_MOUSE };

typedef struct {
    int type;
    int key;          /* SDL keycode for PLAT_KEY */
    int button, x, y; /* PLAT_MOUSE, x/y in 320x200 game space */
} PlatEvent;

int plat_init(int scale);
void plat_quit(void);
void plat_present(void);
int plat_poll(PlatEvent *e);
void plat_wait_tick(void);
void plat_audio_play(const int8_t *samples, int len);

#endif
