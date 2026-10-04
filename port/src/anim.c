#include <stdlib.h>
#include <string.h>
#include "anim.h"
#include "ftl.h"
#include "graphics.h"
#include "sound.h"
#include "startend.h"
#include "ui.h"
#include "video.h"

#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
#define TAG(a, b) ((a) << 8 | (b))

#define CHUNK_HEADER 6        /* tag, payload length, parameter */
#define CHUNK_LENGTH 2
#define CHUNK_PARAMETER 4
#define MAX_SAMPLES 32
#define MAX_LOOP_DEPTH 16
#define SAMPLE_PERIOD 650     /* Paula period of every animation sample */
#define SAMPLE_HEADER 2       /* sample length word before the sample data */
#define SAMPLE_VOLUME_SHIFT 2 /* volume = (byte + 1) >> 2 */
#define PALETTE_COUNT_BYTES 2 /* PL: entry count word before the entries */
#define PALETTE_ENTRY_BYTES 4 /* color index, red, green, blue: 4 bits each */
#define COLOR_NIBBLE 15       /* one 4-bit color component */
#define RED_SHIFT 8           /* Amiga color: red, green, blue nibbles */
#define GREEN_SHIFT 4
#define SAMPLE_VOLUME_BYTES 2 /* SO: left and right volume bytes after the chunk header */

/* Chunk tags that the player runs. The other tags (BR, AN, DO, music, sample files) do nothing for ENDA.DAT. */
enum {
    TAG_PALETTE = TAG('P', 'L'),
    TAG_FULL_FRAME = TAG('E', 'N'),
    TAG_DELTA_FRAME = TAG('D', 'L'),
    TAG_SAMPLE = TAG('S', 'D'),
    TAG_PLAY_SAMPLE = TAG('S', 'O'),
    TAG_LOOP = TAG('F', 'O'),
    TAG_LOOP_NEXT = TAG('N', 'E'),
    TAG_LOOP_BACK = TAG('B', 'N'),
};

typedef struct {
    size_t start;  /* first chunk of the loop body */
    int16_t count; /* passes left after this one */
} Loop;

static uint8_t *file;
static size_t file_len, pos; /* pos: offset of the next chunk */
static const uint8_t *samples[MAX_SAMPLES];
static uint16_t sample_sizes[MAX_SAMPLES];
static int sample_count, loop_depth, palette_changed;
static Loop loops[MAX_LOOP_DEPTH];
static uint16_t colors[PALETTE_COLORS];

/* Offset of the chunk after the chunk at offset at. */
static size_t next_chunk(size_t at)
{
    return at < file_len ? at + CHUNK_HEADER + BE16(file + at + CHUNK_LENGTH) : file_len;
}

void anim_close(void)
{
    sound_stop_all(); /* the voices point into the file */
    free(file);
    file = NULL;
    file_len = pos = 0;
}

int anim_open(const char *path)
{
    anim_close();
    if (!(file = read_file(path, &file_len))) return -1;
    size_t at = 0;
    while (at + CHUNK_HEADER <= file_len) at = next_chunk(at);
    if (at != file_len) return anim_close(), -1;
    sample_count = loop_depth = palette_changed = 0;
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
            colors[e[0]] = (uint16_t)((e[1] & COLOR_NIBBLE) << RED_SHIFT | (e[2] & COLOR_NIBBLE) << GREEN_SHIFT | (e[3] & COLOR_NIBBLE));
    }
    palette_changed = 1;
}

/* SO: plays SD sample number (1-based). The two bytes after the chunk header are the volumes, also
   when the chunk has no payload (as the original). */
static void play_sample(size_t at, uint16_t number)
{
    if (number < 1 || number > sample_count || at + CHUNK_HEADER + SAMPLE_VOLUME_BYTES > file_len) return;
    const uint8_t *s = samples[number - 1];
    uint16_t length = BE16(s);
    if (length > sample_sizes[number - 1] - SAMPLE_HEADER) return;
    uint8_t volumes[SAMPLE_VOLUME_BYTES] = { (uint8_t)((file[at + CHUNK_HEADER] + 1) >> SAMPLE_VOLUME_SHIFT),
                           (uint8_t)((file[at + CHUNK_HEADER + 1] + 1) >> SAMPLE_VOLUME_SHIFT) };
    sound_play_data((const int8_t *)s + SAMPLE_HEADER, length, SAMPLE_PERIOD, volumes);
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
        case TAG_SAMPLE:
            if (sample_count < MAX_SAMPLES && length >= SAMPLE_HEADER) {
                samples[sample_count] = payload;
                sample_sizes[sample_count++] = length;
            }
            break;
        case TAG_PLAY_SAMPLE:
            play_sample(at, parameter);
            break;
        case TAG_LOOP: /* the chunk after FO runs once, the loop body starts after it */
            if (loop_depth < MAX_LOOP_DEPTH) loops[loop_depth++] = (Loop){ next_chunk(pos), (int16_t)(parameter - 1) };
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
        case TAG_FULL_FRAME:
        case TAG_DELTA_FRAME: { /* a delta frame skips the unchanged pixels */
            Bitmap screen = ui_screen();
            gfx_expand_over(payload, length, &screen);
            if (palette_changed) ui_set_screen_palette(colors);
            palette_changed = 0;
            return (int16_t)parameter;
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
    for (int16_t delay; running && (delay = anim_next_frame()) != ANIM_END;)
        while (delay-- > 0 && (running = wait_frame()))
            ;
    anim_close();
    return 0;
}
