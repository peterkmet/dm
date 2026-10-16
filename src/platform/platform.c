/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/platform.h"
#include "engine/sound.h"
#include "ui/music.h"
#include "platform/scale.h"
#include <SDL3/SDL.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static Uint64 next_tick;
static SDL_AudioStream *stream;
#define TICK_NS            (SDL_NS_PER_SECOND / FRAMES_PER_SECOND)
#define FILTER_MAX         3
#define AUDIO_CHANNELS     2
#define AUDIO_QUEUED_TICKS 3
#define ARGB_BYTES         4

static int filter = 1; /* upscale factor: 1 none, 2 Scale2x, 3 Scale3x */
static uint8_t scaled_screen[SCREEN_H * FILTER_MAX][SCREEN_W * FILTER_MAX];
#define POINTER_MAX 32
static uint8_t pointer_px[POINTER_MAX * POINTER_MAX];
static uint32_t pointer_argb[PALETTE_COLORS];
static int pointer_w, pointer_h, pointer_hot_x, pointer_hot_y, mouse_x, mouse_y;

static void render_frame(void);

/* Redraws the last frame while macOS blocks the main loop during a live window resize. */
static bool SDLCALL on_expose(void *userdata, SDL_Event *ev)
{
    (void)userdata;
    if (ev->type == SDL_EVENT_WINDOW_EXPOSED && texture)
        render_frame();
    return true;
}

bool plat_init(int scale)
{
    if (!SDL_Init(SDL_INIT_VIDEO) || !SDL_CreateWindowAndRenderer("Dungeon Master", SCREEN_W * scale, SCREEN_H * scale,
                                                                  SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL: %s", SDL_GetError());
        return false;
    }
    SDL_AddEventWatch(on_expose, NULL);
    const char *name = SDL_getenv("DM_FILTER");
    if (!plat_set_filter(name ? plat_filter_factor(name) : 1))
        return false;
    next_tick = SDL_GetTicksNS() + TICK_NS;
    return true;
}

int plat_filter_factor(const char *name)
{
    if (!strcmp(name, "scale2x"))
        return 2;
    if (!strcmp(name, "scale3x"))
        return 3;
    return 1;
}

/* Selects the upscale filter (1 none, 2 Scale2x, 3 Scale3x) and recreates the texture. */
bool plat_set_filter(int factor)
{
    filter = factor == 2 || factor == 3 ? factor : 1;
    if (texture)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W * filter,
                                SCREEN_H * filter);
    if (!texture) {
        SDL_Log("SDL: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART); /* even pixels at a fractional scale */
    return true;
}

void plat_quit(void)
{
    SDL_Quit();
}

void plat_set_pointer(const uint8_t *px, int w, int h, int hot_x, int hot_y, const uint16_t *colors)
{
    pointer_w = px && w <= POINTER_MAX && h <= POINTER_MAX ? w : 0;
    pointer_h = pointer_w ? h : 0;
    pointer_hot_x = hot_x;
    pointer_hot_y = hot_y;
    if (!pointer_w)
        return;
    if (SDL_CursorVisible())
        SDL_HideCursor();
    memcpy(pointer_px, px, (size_t)w * h);
    for (int i = 0; i < PALETTE_COLORS; i++)
        pointer_argb[i] = video_convert_argb(colors[i]);
}

void plat_set_icon(const uint8_t *px, int w, int h, int transparent, const uint16_t *colors)
{
    SDL_Surface *icon = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!icon)
        return;
    uint32_t *out = icon->pixels;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t c = px[y * w + x] & COLOR_INDEX_MASK;
            out[y * (icon->pitch / ARGB_BYTES) + x] = c == transparent ? 0 : video_convert_argb(colors[c]);
        }
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

void plat_mouse_position(int *x, int *y)
{
    *x = mouse_x;
    *y = mouse_y;
}

/* Draws the pointer at the mouse position into the filtered frame (row length stride). */
static void draw_pointer(uint32_t *px, int stride)
{
    for (int y = 0; y < pointer_h; y++)
        for (int x = 0; x < pointer_w; x++) {
            uint8_t c = pointer_px[y * pointer_w + x];
            int sx = mouse_x - pointer_hot_x + x;
            int sy = mouse_y - pointer_hot_y + y;
            if (!c || sx < 0 || sy < 0 || sx >= SCREEN_W || sy >= SCREEN_H)
                continue;
            for (int dy = 0; dy < filter; dy++)
                for (int dx = 0; dx < filter; dx++)
                    px[(sy * filter + dy) * stride + sx * filter + dx] = pointer_argb[c & COLOR_INDEX_MASK];
        }
}

void plat_present(void)
{
    uint32_t pal[2 * PALETTE_COLORS], *px;
    int pitch, tw = SCREEN_W * filter;
    const uint8_t *src = filter == 1 ? &fb[0][0] : &scaled_screen[0][0];
    if (filter == 2)
        scale2x(&fb[0][0], SCREEN_W, SCREEN_H, &scaled_screen[0][0]);
    if (filter == 3)
        scale3x(&fb[0][0], SCREEN_W, SCREEN_H, &scaled_screen[0][0]);
    for (int i = 0; i < 2 * PALETTE_COLORS; i++)
        pal[i] = video_convert_argb(palette[i]);
    if (!SDL_LockTexture(texture, NULL, (void **)&px, &pitch))
        return;
    int stride = pitch / ARGB_BYTES;
    for (int y = 0; y < SCREEN_H * filter; y++) {
        int row = y / filter;
        int half = row >= palette_split_top && row < palette_split_bottom ? 0 : PALETTE_COLORS;
        for (int x = 0; x < tw; x++)
            px[y * stride + x] = pal[(src[y * tw + x] & COLOR_INDEX_MASK) | half];
    }
    draw_pointer(px, stride);
    SDL_UnlockTexture(texture);
    render_frame();
}

/* Draws the frame texture letterboxed into the window. */
static void render_frame(void)
{
    int w, h;
    float s, ox, oy;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    video_layout(w, h, &s, &ox, &oy);
    SDL_FRect dst = {ox, oy, SCREEN_W * s, SCREEN_H * s};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}

bool plat_poll(PlatEvent *event)
{
    SDL_Event sdl_event;
    int output_width, output_height;
    while (SDL_PollEvent(&sdl_event)) {
        *event = (PlatEvent){0};
        switch (sdl_event.type) {
        case SDL_EVENT_QUIT:
            event->type = PLAT_QUIT;
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (sdl_event.key.repeat)
                break;
            event->type = PLAT_KEY;
            event->key = sdl_event.key.key;
            event->modifiers = sdl_event.key.mod;
            return true;
        case SDL_EVENT_MOUSE_MOTION:
            SDL_ConvertEventToRenderCoordinates(renderer, &sdl_event); /* window points -> output pixels */
            SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);
            video_map_mouse(output_width, output_height, (int)sdl_event.motion.x, (int)sdl_event.motion.y, &mouse_x,
                            &mouse_y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            SDL_ConvertEventToRenderCoordinates(renderer, &sdl_event);
            SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);
            event->type = sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PLAT_MOUSE : PLAT_MOUSE_UP;
            event->button = sdl_event.button.button;
            video_map_mouse(output_width, output_height, (int)sdl_event.button.x, (int)sdl_event.button.y, &event->x,
                            &event->y);
            mouse_x = event->x;
            mouse_y = event->y;
            return true;
        }
    }
    return false;
}

/* Sleeps until the next 50 Hz tick. */
void plat_wait_tick(void)
{
    plat_audio_pump();
    Uint64 now = SDL_GetTicksNS();
    if (now < next_tick)
        SDL_DelayPrecise(next_tick - now);
    else
        next_tick = now; /* fell behind: resync instead of bursting */
    next_tick += TICK_NS;
}

/* Opens the audio stream on first use. Returns 0 when audio is not available. */
static int audio_open(void)
{
    static int tried;
    if (stream || tried)
        return stream != NULL;
    tried = 1;
    SDL_AudioSpec spec = {SDL_AUDIO_S16, AUDIO_CHANNELS, SOUND_OUTPUT_RATE};
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO) ||
        !(stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL))) {
        SDL_Log("SDL audio: %s", SDL_GetError());
        return 0;
    }
    return SDL_ResumeAudioStreamDevice(stream);
}

/* Keeps about three ticks of mixer output queued; runs on the game thread, so the mixer needs no lock. */
void plat_audio_pump(void)
{
    enum { FRAMES = SOUND_OUTPUT_RATE / FRAMES_PER_SECOND };
    int16_t buf[FRAMES * AUDIO_CHANNELS];
    if (!audio_open())
        return;
    while (SDL_GetAudioStreamQueued(stream) < (int)sizeof buf * AUDIO_QUEUED_TICKS) {
        sound_mix(buf, FRAMES);
        music_mix(buf, FRAMES);
        if (!SDL_PutAudioStreamData(stream, buf, sizeof buf))
            return;
    }
}
