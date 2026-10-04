#include <SDL3/SDL.h>
#include <string.h>
#include "platform.h"
#include "scale.h"
#include "sound.h"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static Uint64 next_tick;
static SDL_AudioStream *stream;
#define FRAMES_PER_SECOND 50              /* PAL vertical blank */
#define TICK_NS (SDL_NS_PER_SECOND / FRAMES_PER_SECOND)
#define FILTER_MAX 3
#define AUDIO_CHANNELS 2
#define AUDIO_QUEUED_TICKS 3
#define COLOR_INDEX_MASK (PALETTE_COLORS - 1)
#define ARGB_BYTES 4

static int filter = 1; /* upscale factor: 1 none, 2 Scale2x, 3 Scale3x */
static uint8_t big[SCREEN_H * FILTER_MAX][SCREEN_W * FILTER_MAX];
enum { POINTER_MAX = 32 };
static uint8_t pointer_px[POINTER_MAX * POINTER_MAX];
static uint32_t pointer_argb[PALETTE_COLORS];
static int pointer_w, pointer_h, pointer_hot_x, pointer_hot_y, mouse_x, mouse_y;

int plat_init(int scale)
{
    if (!SDL_Init(SDL_INIT_VIDEO) ||
        !SDL_CreateWindowAndRenderer("Dungeon Master", SCREEN_W * scale, SCREEN_H * scale,
                                     SDL_WINDOW_RESIZABLE, &win, &ren)) {
        SDL_Log("SDL: %s", SDL_GetError());
        return 0;
    }
    const char *name = SDL_getenv("DM_FILTER");
    if (!plat_set_filter(name ? plat_filter_factor(name) : 1)) return 0;
    next_tick = SDL_GetTicksNS() + TICK_NS;
    return 1;
}

int plat_filter_factor(const char *name)
{
    if (!strcmp(name, "scale2x")) return 2;
    if (!strcmp(name, "scale3x")) return 3;
    return 1;
}

/* Selects the upscale filter (1 none, 2 Scale2x, 3 Scale3x) and recreates the texture. */
int plat_set_filter(int factor)
{
    filter = factor == 2 || factor == 3 ? factor : 1;
    if (tex) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                            SCREEN_W * filter, SCREEN_H * filter);
    if (!tex) {
        SDL_Log("SDL: %s", SDL_GetError());
        return 0;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    return 1;
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
    if (!pointer_w) return;
    if (SDL_CursorVisible()) SDL_HideCursor();
    memcpy(pointer_px, px, (size_t)w * h);
    for (int i = 0; i < PALETTE_COLORS; i++) pointer_argb[i] = video_argb(colors[i]);
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
            int sx = mouse_x - pointer_hot_x + x, sy = mouse_y - pointer_hot_y + y;
            if (!c || sx < 0 || sy < 0 || sx >= SCREEN_W || sy >= SCREEN_H) continue;
            for (int dy = 0; dy < filter; dy++)
                for (int dx = 0; dx < filter; dx++) px[(sy * filter + dy) * stride + sx * filter + dx] = pointer_argb[c & COLOR_INDEX_MASK];
        }
}

void plat_present(void)
{
    uint32_t pal[2 * PALETTE_COLORS], *px;
    int pitch, w, h, s, ox, oy, tw = SCREEN_W * filter;
    const uint8_t *src = filter == 1 ? &fb[0][0] : &big[0][0];
    if (filter == 2) scale2x(&fb[0][0], SCREEN_W, SCREEN_H, &big[0][0]);
    if (filter == 3) scale3x(&fb[0][0], SCREEN_W, SCREEN_H, &big[0][0]);
    for (int i = 0; i < 2 * PALETTE_COLORS; i++) pal[i] = video_argb(palette[i]);
    if (!SDL_LockTexture(tex, NULL, (void **)&px, &pitch)) return;
    int stride = pitch / ARGB_BYTES;
    for (int y = 0; y < SCREEN_H * filter; y++) {
        int row = y / filter, half = row >= palette_split_top && row < palette_split_bottom ? 0 : PALETTE_COLORS;
        for (int x = 0; x < tw; x++)
            px[y * stride + x] = pal[(src[y * tw + x] & COLOR_INDEX_MASK) | half];
    }
    draw_pointer(px, stride);
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
            if (ev.key.repeat) break;
            e->type = PLAT_KEY;
            e->key = ev.key.key;
            e->modifiers = ev.key.mod;
            return 1;
        case SDL_EVENT_MOUSE_MOTION:
            SDL_ConvertEventToRenderCoordinates(ren, &ev); /* window points -> output pixels */
            SDL_GetCurrentRenderOutputSize(ren, &w, &h);
            video_map_mouse(w, h, (int)ev.motion.x, (int)ev.motion.y, &mouse_x, &mouse_y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            SDL_ConvertEventToRenderCoordinates(ren, &ev);
            SDL_GetCurrentRenderOutputSize(ren, &w, &h);
            e->type = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PLAT_MOUSE : PLAT_MOUSE_UP;
            e->button = ev.button.button;
            video_map_mouse(w, h, (int)ev.button.x, (int)ev.button.y, &e->x, &e->y);
            mouse_x = e->x;
            mouse_y = e->y;
            return 1;
        }
    }
    return 0;
}

/* Sleeps until the next 50 Hz tick. */
void plat_wait_tick(void)
{
    plat_audio_pump();
    Uint64 now = SDL_GetTicksNS();
    if (now < next_tick) SDL_DelayPrecise(next_tick - now);
    else next_tick = now; /* fell behind: resync instead of bursting */
    next_tick += TICK_NS;
}

/* Opens the audio stream on first use. Returns 0 when audio is not available. */
static int audio_open(void)
{
    static int tried;
    if (stream || tried) return stream != NULL;
    tried = 1;
    SDL_AudioSpec spec = { SDL_AUDIO_S16, AUDIO_CHANNELS, SOUND_OUTPUT_RATE };
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
    if (!audio_open()) return;
    while (SDL_GetAudioStreamQueued(stream) < (int)sizeof buf * AUDIO_QUEUED_TICKS) {
        sound_mix(buf, FRAMES);
        if (!SDL_PutAudioStreamData(stream, buf, sizeof buf)) return;
    }
}
