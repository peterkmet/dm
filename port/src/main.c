#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "platform.h"

static void draw(int frame)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            fb[y][x] = x / 20;
    for (int x = 0; x < SCREEN_W; x++)
        fb[frame % SCREEN_H][x] = 15 - x / 20;
}

int main(int argc, char **argv)
{
    long frames = -1;
    for (int i = 1; i < argc - 1; i++)
        if (!strcmp(argv[i], "--frames")) frames = strtol(argv[i + 1], NULL, 10);

    for (int i = 0; i < 16; i++) /* gray ramp, 0x0iii */
        palette[i] = i << 8 | i << 4 | i;
    if (!plat_init(3)) return 1;

    for (int frame = 0; frames < 0 || frame < frames; frame++) {
        PlatEvent e;
        while (plat_poll(&e)) {
            if (e.type == PLAT_QUIT || (e.type == PLAT_KEY && e.key == SDLK_ESCAPE)) goto done;
            if (e.type == PLAT_MOUSE) SDL_Log("mouse %d at %d,%d", e.button, e.x, e.y);
        }
        draw(frame);
        plat_present();
        plat_wait_tick();
    }
done:
    plat_quit();
    return 0;
}
