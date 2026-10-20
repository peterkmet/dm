/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"
#include "audio.h"
#include "music.h"
#include "video.h"
#include "scale.h"
#include <SDL3/SDL.h>
#include <string.h>

/* Timing, filter, audio, and pixel sizes. */
#define TICK_NS            (SDL_NS_PER_SECOND / FRAMES_PER_SECOND)
#define FILTER_MAX         3
#define AUDIO_CHANNELS     2
#define AUDIO_QUEUED_TICKS 3
#define AUDIO_TICK_FRAMES  (AUDIO_OUTPUT_RATE / FRAMES_PER_SECOND) /* mixer frames of one tick */
#define ARGB_BYTES         4
/* Index frame colors: 0-15 top palette, 16-31 bottom palette, 32-47 pointer colors. */
#define BOTTOM_HALF_FLAG   PALETTE_COLORS
#define POINTER_COLOR_BASE (2 * PALETTE_COLORS)
#define FRAME_COLORS       (3 * PALETTE_COLORS)

static struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint64 next_tick;
    SDL_AudioStream *stream;
    int filter;                        /* upscale factor: 1 none, 2 Scale2x, 3 Scale3x */
    uint8_t frame[SCREEN_H][SCREEN_W]; /* index frame: screen and pointer, before the filter */
    uint8_t scaled_screen[SCREEN_H * FILTER_MAX][SCREEN_W * FILTER_MAX];
    struct {
        uint8_t px[POINTER_SIZE * POINTER_SIZE];
        uint32_t argb[PALETTE_COLORS];
        int w;
        int h;
        int hot_x;
        int hot_y;
    } pointer;
    int mouse_x;
    int mouse_y;
} plat = {.filter = 1};

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

void plat_set_pointer(const PointerImage *image, const uint16_t *colors)
{
    plat.pointer.w = image && image->w <= POINTER_SIZE && image->h <= POINTER_SIZE ? image->w : 0;
    plat.pointer.h = plat.pointer.w ? image->h : 0;
    plat.pointer.hot_x = image ? image->hot_x : 0;
    plat.pointer.hot_y = image ? image->hot_y : 0;
    if (!plat.pointer.w)
        return;
    if (SDL_CursorVisible())
        SDL_HideCursor();
    memcpy(plat.pointer.px, image->px, (size_t)plat.pointer.w * plat.pointer.h);
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

void plat_mouse_position(int16_t *mouse_x, int16_t *mouse_y)
{
    *mouse_x = (int16_t)plat.mouse_x; /* output_map_mouse clamps it to the screen */
    *mouse_y = (int16_t)plat.mouse_y;
}

/* Builds the index frame: each screen pixel with its palette half, the pointer over it at the mouse position. */
static void build_frame(void)
{
    for (int y = 0; y < SCREEN_H; y++) {
        bool top_row = y >= g_screen.split_top && y < g_screen.split_bottom;
        for (int x = 0; x < SCREEN_W; x++) {
            uint8_t px = g_screen.px[y][x];
            int half = top_row && !(px & COLOR_INTERFACE_FLAG) ? 0 : BOTTOM_HALF_FLAG;
            plat.frame[y][x] = (uint8_t)((px & COLOR_INDEX_MASK) | half);
        }
    }
    for (int y = 0; y < plat.pointer.h; y++)
        for (int x = 0; x < plat.pointer.w; x++) {
            uint8_t c = plat.pointer.px[y * plat.pointer.w + x];
            int sx = plat.mouse_x - plat.pointer.hot_x + x;
            int sy = plat.mouse_y - plat.pointer.hot_y + y;
            if (!c || sx < 0 || sy < 0 || sx >= SCREEN_W || sy >= SCREEN_H)
                continue;
            plat.frame[sy][sx] = (uint8_t)(POINTER_COLOR_BASE + (c & COLOR_INDEX_MASK));
        }
}

void plat_present(void)
{
    uint32_t argb[FRAME_COLORS], *px;
    int pitch, tw = SCREEN_W * plat.filter;
    const uint8_t *src = plat.filter == 1 ? &plat.frame[0][0] : &plat.scaled_screen[0][0];
    build_frame();
    if (plat.filter == 2)
        scale2x(&plat.frame[0][0], SCREEN_W, SCREEN_H, &plat.scaled_screen[0][0]);
    if (plat.filter == 3)
        scale3x(&plat.frame[0][0], SCREEN_W, SCREEN_H, &plat.scaled_screen[0][0]);
    for (int i = 0; i < 2 * PALETTE_COLORS; i++)
        argb[i] = video_convert_argb(g_screen.palette[i]);
    memcpy(&argb[POINTER_COLOR_BASE], plat.pointer.argb, sizeof plat.pointer.argb);
    if (!SDL_LockTexture(plat.texture, NULL, (void **)&px, &pitch))
        return;
    int stride = pitch / ARGB_BYTES;
    for (int y = 0; y < SCREEN_H * plat.filter; y++)
        for (int x = 0; x < tw; x++)
            px[y * stride + x] = argb[src[y * tw + x]];
    SDL_UnlockTexture(plat.texture);
    render_frame();
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
            event->character =
                (int)SDL_GetKeyFromScancode(sdl_event.key.scancode, sdl_event.key.mod & SDL_KMOD_SHIFT, false);
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

/* Opens the audio stream on first use. Returns false when audio is not available. */
static bool open_audio(void)
{
    static bool tried;
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

/* Keeps about three ticks of mixer output queued; runs on the game thread, so the mixer needs no lock. */
static void pump_audio(void)
{
    int16_t buf[AUDIO_TICK_FRAMES * AUDIO_CHANNELS];
    if (!open_audio())
        return;
    while (SDL_GetAudioStreamQueued(plat.stream) < (int)sizeof buf * AUDIO_QUEUED_TICKS) {
        audio_mix(buf, AUDIO_TICK_FRAMES);
        audio_filter(buf, AUDIO_TICK_FRAMES);
        music_mix(buf, AUDIO_TICK_FRAMES);
        if (!SDL_PutAudioStreamData(plat.stream, buf, sizeof buf))
            return;
    }
}

void plat_wait_tick(void)
{
    pump_audio();
    Uint64 now = SDL_GetTicksNS();
    if (now < plat.next_tick)
        SDL_DelayPrecise(plat.next_tick - now);
    else
        plat.next_tick = now; /* fell behind: resync instead of bursting */
    plat.next_tick += TICK_NS;
}
