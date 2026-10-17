/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/platform.h"
#include "ui/audio.h"
#include "ui/music.h"
#include "platform/scale.h"
#include <SDL3/SDL.h>
#include <string.h>

#define TICK_NS            (SDL_NS_PER_SECOND / FRAMES_PER_SECOND)
#define FILTER_MAX         3
#define AUDIO_CHANNELS     2
#define AUDIO_QUEUED_TICKS 3
#define ARGB_BYTES         4
#define POINTER_MAX        32

static struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint64 next_tick;
    SDL_AudioStream *stream;
    int filter; /* upscale factor: 1 none, 2 Scale2x, 3 Scale3x */
    uint8_t scaled_screen[SCREEN_H * FILTER_MAX][SCREEN_W * FILTER_MAX];
    struct {
        uint8_t px[POINTER_MAX * POINTER_MAX];
        uint32_t argb[PALETTE_COLORS];
        int w;
        int h;
        int hot_x;
        int hot_y;
    } pointer;
    int mouse_x;
    int mouse_y;
} plat = {.filter = 1};

static void render_frame(void);

/* Redraws the last frame while macOS blocks the main loop during a live window resize. */
static bool SDLCALL on_expose(void *userdata, SDL_Event *ev)
{
    (void)userdata;
    if (ev->type == SDL_EVENT_WINDOW_EXPOSED && plat.texture)
        render_frame();
    return true;
}

bool plat_init(int scale)
{
    if (!SDL_Init(SDL_INIT_VIDEO) || !SDL_CreateWindowAndRenderer("Dungeon Master", SCREEN_W * scale, SCREEN_H * scale,
                                                                  SDL_WINDOW_RESIZABLE, &plat.window, &plat.renderer)) {
        SDL_Log("SDL: %s", SDL_GetError());
        return false;
    }
    SDL_AddEventWatch(on_expose, NULL);
    if (!plat_set_filter(1))
        return false;
    plat.next_tick = SDL_GetTicksNS() + TICK_NS;
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

bool plat_set_filter(int factor)
{
    plat.filter = factor == 2 || factor == 3 ? factor : 1;
    if (plat.texture)
        SDL_DestroyTexture(plat.texture);
    plat.texture = SDL_CreateTexture(plat.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     SCREEN_W * plat.filter, SCREEN_H * plat.filter);
    if (!plat.texture) {
        SDL_Log("SDL: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(plat.texture, SDL_SCALEMODE_PIXELART); /* even pixels at a fractional scale */
    return true;
}

void plat_quit(void)
{
    SDL_Quit();
}

void plat_set_pointer(const uint8_t *px, int w, int h, int hot_x, int hot_y, const uint16_t *colors)
{
    plat.pointer.w = px && w <= POINTER_MAX && h <= POINTER_MAX ? w : 0;
    plat.pointer.h = plat.pointer.w ? h : 0;
    plat.pointer.hot_x = hot_x;
    plat.pointer.hot_y = hot_y;
    if (!plat.pointer.w)
        return;
    if (SDL_CursorVisible())
        SDL_HideCursor();
    memcpy(plat.pointer.px, px, (size_t)w * h);
    for (int i = 0; i < PALETTE_COLORS; i++)
        plat.pointer.argb[i] = video_convert_argb(colors[i]);
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
    SDL_SetWindowIcon(plat.window, icon);
    SDL_DestroySurface(icon);
}

void plat_mouse_position(int *x, int *y)
{
    *x = plat.mouse_x;
    *y = plat.mouse_y;
}

/* Draws the pointer at the mouse position into the filtered frame (row length stride). */
static void draw_pointer(uint32_t *px, int stride)
{
    for (int y = 0; y < plat.pointer.h; y++)
        for (int x = 0; x < plat.pointer.w; x++) {
            uint8_t c = plat.pointer.px[y * plat.pointer.w + x];
            int sx = plat.mouse_x - plat.pointer.hot_x + x;
            int sy = plat.mouse_y - plat.pointer.hot_y + y;
            if (!c || sx < 0 || sy < 0 || sx >= SCREEN_W || sy >= SCREEN_H)
                continue;
            for (int dy = 0; dy < plat.filter; dy++)
                for (int dx = 0; dx < plat.filter; dx++)
                    px[(sy * plat.filter + dy) * stride + sx * plat.filter + dx] =
                        plat.pointer.argb[c & COLOR_INDEX_MASK];
        }
}

void plat_present(void)
{
    uint32_t pal[2 * PALETTE_COLORS], *px;
    int pitch, tw = SCREEN_W * plat.filter;
    const uint8_t *src = plat.filter == 1 ? &g_screen.px[0][0] : &plat.scaled_screen[0][0];
    if (plat.filter == 2)
        scale2x(&g_screen.px[0][0], SCREEN_W, SCREEN_H, &plat.scaled_screen[0][0]);
    if (plat.filter == 3)
        scale3x(&g_screen.px[0][0], SCREEN_W, SCREEN_H, &plat.scaled_screen[0][0]);
    for (int i = 0; i < 2 * PALETTE_COLORS; i++)
        pal[i] = video_convert_argb(g_screen.palette[i]);
    if (!SDL_LockTexture(plat.texture, NULL, (void **)&px, &pitch))
        return;
    int stride = pitch / ARGB_BYTES;
    for (int y = 0; y < SCREEN_H * plat.filter; y++) {
        int row = y / plat.filter;
        int half = row >= g_screen.split_top && row < g_screen.split_bottom ? 0 : PALETTE_COLORS;
        for (int x = 0; x < tw; x++)
            px[y * stride + x] = pal[(src[y * tw + x] & COLOR_INDEX_MASK) | half];
    }
    draw_pointer(px, stride);
    SDL_UnlockTexture(plat.texture);
    render_frame();
}

/* Draws the frame texture letterboxed into the window. */
static void render_frame(void)
{
    int w, h;
    SDL_GetCurrentRenderOutputSize(plat.renderer, &w, &h);
    OutputLayout layout = output_layout(w, h);
    SDL_FRect dst = {layout.x, layout.y, SCREEN_W * layout.scale, SCREEN_H * layout.scale};
    SDL_SetRenderDrawColor(plat.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(plat.renderer);
    SDL_RenderTexture(plat.renderer, plat.texture, NULL, &dst);
    SDL_RenderPresent(plat.renderer);
}

bool plat_poll(PlatEvent *event)
{
    SDL_Event sdl_event;
    int output_width, output_height;
    OutputLayout layout;
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
            SDL_ConvertEventToRenderCoordinates(plat.renderer, &sdl_event); /* window points -> output pixels */
            SDL_GetCurrentRenderOutputSize(plat.renderer, &output_width, &output_height);
            layout = output_layout(output_width, output_height);
            output_map_mouse(&layout, (int)sdl_event.motion.x, (int)sdl_event.motion.y, &plat.mouse_x, &plat.mouse_y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            SDL_ConvertEventToRenderCoordinates(plat.renderer, &sdl_event);
            SDL_GetCurrentRenderOutputSize(plat.renderer, &output_width, &output_height);
            layout = output_layout(output_width, output_height);
            event->type = sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PLAT_MOUSE : PLAT_MOUSE_UP;
            event->button = sdl_event.button.button;
            output_map_mouse(&layout, (int)sdl_event.button.x, (int)sdl_event.button.y, &event->x, &event->y);
            plat.mouse_x = event->x;
            plat.mouse_y = event->y;
            return true;
        }
    }
    return false;
}

void plat_wait_tick(void)
{
    plat_audio_pump();
    Uint64 now = SDL_GetTicksNS();
    if (now < plat.next_tick)
        SDL_DelayPrecise(plat.next_tick - now);
    else
        plat.next_tick = now; /* fell behind: resync instead of bursting */
    plat.next_tick += TICK_NS;
}

/* Opens the audio stream on first use. Returns 0 when audio is not available. */
static int audio_open(void)
{
    static int tried;
    if (plat.stream || tried)
        return plat.stream != NULL;
    tried = true;
    SDL_AudioSpec spec = {SDL_AUDIO_S16, AUDIO_CHANNELS, AUDIO_OUTPUT_RATE};
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO) ||
        !(plat.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL))) {
        SDL_Log("SDL audio: %s", SDL_GetError());
        return false;
    }
    return SDL_ResumeAudioStreamDevice(plat.stream);
}

void plat_audio_pump(void)
{
    enum { FRAMES = AUDIO_OUTPUT_RATE / FRAMES_PER_SECOND };
    int16_t buf[FRAMES * AUDIO_CHANNELS];
    if (!audio_open())
        return;
    while (SDL_GetAudioStreamQueued(plat.stream) < (int)sizeof buf * AUDIO_QUEUED_TICKS) {
        audio_mix(buf, FRAMES);
        music_mix(buf, FRAMES);
        if (!SDL_PutAudioStreamData(plat.stream, buf, sizeof buf))
            return;
    }
}
