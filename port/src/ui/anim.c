#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "anim.h"
#include "adventure.h"
#include "bytes.h"
#include "command.h"
#include "ftl.h"
#include "graphics.h"
#include "music.h"
#include "sound.h"
#include "startend.h"
#include "ui.h"
#include "video.h"

#define TAG(a, b) ((a) << 8 | (b))

#define CHUNK_HEADER 6        /* tag, payload length, parameter */
#define CHUNK_LENGTH 2
#define CHUNK_PARAMETER 4
#define MAX_SAMPLES 32
#define MAX_LOOP_DEPTH 16
#define ANIMATION_SAMPLE_PERIOD 650     /* Paula period of every animation sample */
#define SAMPLE_HEADER 2       /* sample length word before the sample data */
#define SAMPLE_VOLUME_SHIFT 2 /* volume = (byte + 1) >> 2 */
#define PALETTE_COUNT_BYTES 2 /* PL: entry count word before the entries */
#define PALETTE_ENTRY_BYTES 4 /* color index, red, green, blue: 4 bits each */
#define COLOR_NIBBLE 15       /* one 4-bit color component */
#define RED_SHIFT 8           /* Amiga color: red, green, blue nibbles */
#define GREEN_SHIFT 4
#define SAMPLE_VOLUME_BYTES 2 /* SO: left and right volume bytes after the chunk header */
#define MAX_MUSIC 32
#define MUSIC_PERIOD 328      /* CSB disk B ANIM.FTL 0x101424: every music segment */
#define MUSIC_VOLUME 30
#define MUSIC_PASSES 6        /* the sequence plays 6 times, then the music ends */
#define MUSIC_SEQUENCE_END 0xff
#define MUSIC_COUNT_SIGN 0x80 /* high byte of a negative item count */
#define MUSIC_FADE_FRAMES 3   /* 0x102e4c: the volume goes down by 1 every 3 ticks */

/* Chunk tags that the player runs. The other tags (AN, DO, P8, music, sample files) do nothing for ENDA.DAT and TITL.DAT. */
enum {
    TAG_BREAK = TAG('B', 'R'),
    TAG_PALETTE = TAG('P', 'L'),
    TAG_FULL_FRAME = TAG('E', 'N'),
    TAG_DELTA_FRAME = TAG('D', 'L'),
    TAG_SAMPLE = TAG('S', 'D'),
    TAG_PLAY_SAMPLE = TAG('S', 'O'),
    TAG_LOOP = TAG('F', 'O'),
    TAG_LOOP_NEXT = TAG('N', 'E'),
    TAG_LOOP_BACK = TAG('B', 'N'),
    TAG_SAMPLE_FILE = TAG('S', 'F'),
    TAG_MUSIC_FILE = TAG('M', 'F'),
    TAG_MUSIC = TAG('M', 'I'),
};

typedef struct {
    size_t start;  /* first chunk of the loop body */
    int16_t count; /* passes left after this one */
} Loop;

static uint8_t *file;
static size_t file_len, pos; /* pos: offset of the next chunk */
static const uint8_t *samples[MAX_SAMPLES];
static uint16_t sample_sizes[MAX_SAMPLES];
static int sample_count, loop_depth, palette_changed, input_stops; /* input_stops: BR allows a stop by a key or a click */
static Loop loops[MAX_LOOP_DEPTH];
static uint16_t colors[PALETTE_COLORS];
static char directory[PATH_SIZE];           /* the folder of the animation file: SF and MF names are relative to it */
static uint8_t *loaded[MAX_SAMPLES + MAX_MUSIC]; /* the files of SF and MF */
static size_t music_sizes[MAX_MUSIC];
static const uint8_t *music[MAX_MUSIC];
static int loaded_count, music_count, music_started;

/* Offset of the chunk after the chunk at offset at. */
static size_t next_chunk(size_t at)
{
    return at < file_len ? at + CHUNK_HEADER + BE16(file + at + CHUNK_LENGTH) : file_len;
}

void anim_close(void)
{
    sound_stop_all(); /* the voices point into the file and the loaded files */
    music_stop();
    free(file);
    file = NULL;
    file_len = pos = 0;
    while (loaded_count) free(loaded[--loaded_count]);
    music_count = music_started = 0;
}

/* Reads the file name relative to the animation folder. AmigaDOS ignores the case of the names. */
static uint8_t *read_file_any_case(const char *name, size_t *len)
{
    char path[PATH_SIZE], part[PATH_SIZE];
    snprintf(path, sizeof path, "%s", directory);
    for (const char *p = name; *p;) {
        size_t n = strcspn(p, "/");
        snprintf(part, sizeof part, "%.*s", (int)n, p);
        DIR *d = opendir(path);
        struct dirent *e;
        while (d && (e = readdir(d)) && strcasecmp(e->d_name, part))
            ;
        size_t used = strlen(path);
        snprintf(path + used, sizeof path - used, "/%s", d && e ? e->d_name : part);
        if (d) closedir(d);
        p += n + (p[n] == '/');
    }
    return read_file(path, len);
}

/* SF and MF: the file named by the payload, kept until anim_close. NULL when it cannot be read. */
static uint8_t *load_named_file(const uint8_t *payload, uint16_t length, size_t *size)
{
    char name[PATH_SIZE];
    snprintf(name, sizeof name, "%.*s", (int)length, (const char *)payload);
    *size = 0;
    if (loaded_count >= (int)(sizeof loaded / sizeof loaded[0])) return NULL;
    uint8_t *data = read_file_any_case(name, size);
    if (data) loaded[loaded_count++] = data;
    return data;
}

/* MI: the music file is a count n, two copies of a table of n item sizes, then the items. Item 0 is a
   sequence of item numbers that ends with 0xff; the other items are samples (length word, data). */
static void play_music(uint16_t number)
{
    if (number < 1 || number > music_count || !music[number - 1]) return;
    const uint8_t *m = music[number - 1];
    size_t size = music_sizes[number - 1];
    if (size < 2 || m[0] & MUSIC_COUNT_SIGN) return;
    uint16_t n = BE16(m);
    size_t items = 2 + 4 * (size_t)n, offsets[UINT8_MAX + 1];
    if (!n || n > UINT8_MAX || items > size || BE16(m + 2) != BE16(m + 2 + 2 * n)) return;
    for (uint16_t i = 0, at = 0; i < n; i++) {
        offsets[i] = items + at;
        at += BE16(m + 2 + 2 * (n + i));
    }
    const int8_t *segments[SOUND_MUSIC_SEGMENTS];
    uint16_t lengths[SOUND_MUSIC_SEGMENTS];
    int sequence_length = 0, count = 0;
    while (offsets[0] + sequence_length < size && m[offsets[0] + sequence_length] != MUSIC_SEQUENCE_END) sequence_length++;
    for (int i = 0; sequence_length && i < MUSIC_PASSES * sequence_length && count < SOUND_MUSIC_SEGMENTS; i++) {
        uint8_t item = m[offsets[0] + i % sequence_length];
        if (item >= n || offsets[item] + SAMPLE_HEADER > size || BE16(m + offsets[item]) > size - offsets[item] - SAMPLE_HEADER) continue;
        segments[count] = (const int8_t *)m + offsets[item] + SAMPLE_HEADER;
        lengths[count++] = BE16(m + offsets[item]);
    }
    sound_music_play(segments, lengths, count, MUSIC_PERIOD, MUSIC_VOLUME);
    music_started = 1;
}

int anim_open(const char *path)
{
    anim_close();
    const char *slash = strrchr(path, '/');
    snprintf(directory, sizeof directory, "%.*s", slash ? (int)(slash - path) : 1, slash ? path : ".");
    if (!(file = read_file(path, &file_len))) return -1;
    size_t at = 0;
    while (at + CHUNK_HEADER <= file_len) at = next_chunk(at);
    if (at != file_len) return anim_close(), -1;
    sample_count = loop_depth = palette_changed = input_stops = 0;
    memset(colors, 0, sizeof colors);
    return 0;
}

/* PL: color entries for the next frame. */
static void set_colors(const uint8_t *payload, uint16_t length)
{
    uint16_t count = length >= PALETTE_COUNT_BYTES ? BE16(payload) : 0;
    for (uint16_t i = 0; i < count && PALETTE_COUNT_BYTES + (i + 1) * PALETTE_ENTRY_BYTES <= length; i++) {
        const uint8_t *e = payload + PALETTE_COUNT_BYTES + i * PALETTE_ENTRY_BYTES;
        if (e[0] < PALETTE_COLORS)
            colors[e[0]] = (e[1] & COLOR_NIBBLE) << RED_SHIFT | (e[2] & COLOR_NIBBLE) << GREEN_SHIFT | (e[3] & COLOR_NIBBLE);
    }
    palette_changed = 1;
}

/* SO: plays SD sample number (1-based). The two bytes after the chunk header are the volumes, also
   when the chunk has no payload (as the original). */
static void play_sample(size_t at, uint16_t number)
{
    if (number < 1 || number > sample_count || !samples[number - 1] || at + CHUNK_HEADER + SAMPLE_VOLUME_BYTES > file_len) return;
    const uint8_t *s = samples[number - 1];
    uint16_t length = BE16(s);
    if (length > sample_sizes[number - 1] - SAMPLE_HEADER) return;
    uint8_t volumes[SAMPLE_VOLUME_BYTES] = { (uint8_t)((file[at + CHUNK_HEADER] + 1) >> SAMPLE_VOLUME_SHIFT),
                           (uint8_t)((file[at + CHUNK_HEADER + 1] + 1) >> SAMPLE_VOLUME_SHIFT) };
    sound_play_data((const int8_t *)s + SAMPLE_HEADER, length, ANIMATION_SAMPLE_PERIOD, volumes);
}

/* FO, NE, and BN: the start of a loop of parameter passes, the end of a pass, and the jump back to the loop body. */
static void loop_chunk(uint16_t tag, uint16_t parameter)
{
    switch (tag) {
    case TAG_LOOP: /* the chunk after FO runs once, the loop body starts after it */
        if (loop_depth < MAX_LOOP_DEPTH) loops[loop_depth++] = (Loop){ next_chunk(pos), parameter - 1 };
        break;
    case TAG_LOOP_NEXT: /* the last pass skips the two chunks after NE: the end-of-pass chunk and BN */
        if (loop_depth && loops[loop_depth - 1].count-- == 0) {
            loop_depth--;
            pos = next_chunk(next_chunk(pos));
        }
        break;
    case TAG_LOOP_BACK:
        if (loop_depth) pos = loops[loop_depth - 1].start;
        break;
    }
}

int16_t anim_next_frame(void)
{
    while (pos < file_len) {
        const uint8_t *chunk = file + pos, *payload = chunk + CHUNK_HEADER;
        uint16_t length = BE16(chunk + CHUNK_LENGTH), parameter = BE16(chunk + CHUNK_PARAMETER);
        size_t at = pos;
        pos = next_chunk(at);
        switch (BE16(chunk)) {
        case TAG_PALETTE:
            set_colors(payload, length);
            break;
        case TAG_BREAK:
            input_stops = parameter != 0;
            break;
        case TAG_SAMPLE:
            if (sample_count < MAX_SAMPLES && length >= SAMPLE_HEADER) {
                samples[sample_count] = payload;
                sample_sizes[sample_count++] = length;
            }
            break;
        case TAG_PLAY_SAMPLE:
            play_sample(at, parameter);
            break;
        case TAG_SAMPLE_FILE: { /* numbered with the SD samples; a missing file keeps its number */
            size_t size;
            const uint8_t *data = load_named_file(payload, length, &size);
            if (sample_count < MAX_SAMPLES) {
                samples[sample_count] = data;
                sample_sizes[sample_count++] = size < UINT16_MAX ? size : UINT16_MAX;
            }
            break;
        }
        case TAG_MUSIC_FILE:
            if (music_count < MAX_MUSIC) {
                music[music_count] = load_named_file(payload, length, &music_sizes[music_count]);
                music_count++;
            }
            break;
        case TAG_MUSIC:
            play_music(parameter);
            break;
        case TAG_LOOP:
        case TAG_LOOP_NEXT:
        case TAG_LOOP_BACK:
            loop_chunk(BE16(chunk), parameter);
            break;
        case TAG_FULL_FRAME:
        case TAG_DELTA_FRAME: { /* a delta frame skips the unchanged pixels */
            Bitmap screen = ui_screen();
            gfx_expand_over(payload, length, &screen);
            if (palette_changed) ui_set_screen_palette(colors);
            palette_changed = 0;
            return parameter;
        }
        }
    }
    return ANIM_END;
}

int anim_play(const char *path)
{
    if (anim_open(path)) return -1;
    memset(fb, 0, sizeof fb);
    ui_set_screen_palette(colors);
    int running = 1;
    command_discard_all_input();
    g_input_received = 0;
    /* a stop keeps the shown frame for its delay, as the original */
    for (int16_t delay; running && (delay = anim_next_frame()) != ANIM_END;) {
        while (delay-- > 0 && (running = wait_frame()))
            ;
        if (input_stops && g_input_received) running = 0;
    }
    /* the music fades out at the end and after a stop (0x102e4c) */
    for (int volume = MUSIC_VOLUME - 1; music_started && volume > 0 && !g_quit_requested; volume--) {
        sound_music_set_volume((uint8_t)volume);
        wait_frames(MUSIC_FADE_FRAMES);
    }
    anim_close();
    return 0;
}
