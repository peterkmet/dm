#include <SDL3/SDL.h>
#include "platform.h"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static Uint64 next_tick;

#define TICK_NS (SDL_NS_PER_SECOND / 50) /* PAL VBL */

int plat_init(int scale)
{
    if (!SDL_Init(SDL_INIT_VIDEO) ||
        !SDL_CreateWindowAndRenderer("Dungeon Master", SCREEN_W * scale, SCREEN_H * scale,
                                     SDL_WINDOW_RESIZABLE, &win, &ren) ||
        !(tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  SCREEN_W, SCREEN_H))) {
        SDL_Log("SDL: %s", SDL_GetError());
        return 0;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    next_tick = SDL_GetTicksNS() + TICK_NS;
    return 1;
}

void plat_quit(void)
{
    SDL_Quit();
}

void plat_present(void)
{
    uint32_t pal[16], *px;
    int pitch, w, h, s, ox, oy;
    for (int i = 0; i < 16; i++) pal[i] = video_argb(palette[i]);
    if (!SDL_LockTexture(tex, NULL, (void **)&px, &pitch)) return;
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            px[y * pitch / 4 + x] = pal[fb[y][x] & 15];
    SDL_UnlockTexture(tex);

    SDL_GetCurrentRenderOutputSize(ren, &w, &h);
    video_layout(w, h, &s, &ox, &oy);
    SDL_FRect dst = { ox, oy, SCREEN_W * s, SCREEN_H * s };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, &dst);
    SDL_RenderPresent(ren);
}

int plat_poll(PlatEvent *e)
{
    SDL_Event ev;
    int w, h;
    while (SDL_PollEvent(&ev)) {
        *e = (PlatEvent){ 0 };
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            e->type = PLAT_QUIT;
            return 1;
        case SDL_EVENT_KEY_DOWN:
            e->type = PLAT_KEY;
            e->key = ev.key.key;
            return 1;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            SDL_ConvertEventToRenderCoordinates(ren, &ev); /* window points -> output pixels */
            SDL_GetCurrentRenderOutputSize(ren, &w, &h);
            e->type = PLAT_MOUSE;
            e->button = ev.button.button;
            video_map_mouse(w, h, (int)ev.button.x, (int)ev.button.y, &e->x, &e->y);
            return 1;
        }
    }
    return 0;
}

/* Sleeps until the next 50 Hz tick. */
void plat_wait_tick(void)
{
    Uint64 now = SDL_GetTicksNS();
    if (now < next_tick) SDL_DelayPrecise(next_tick - now);
    else next_tick = now; /* fell behind: resync instead of bursting */
    next_tick += TICK_NS;
}

/* ponytail: audio stub, opens an SDL audio stream when the sound step needs it */
void plat_audio_play(const int8_t *samples, int len)
{
    (void)samples;
    (void)len;
}
