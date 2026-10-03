#include "dungeon.h"
#include "ftl.h"
#include <stdlib.h>
#include <string.h>

#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
#define BE32(p) ((uint32_t)BE16(p) << 16 | BE16((p) + 2))
#define WR16(p, v) ((p)[0] = (uint8_t)((v) >> 8), (p)[1] = (uint8_t)(v))

uint8_t *g_dungeon_header, *g_dungeon_maps, *g_dungeon_column_first_thing_index;
uint8_t *g_square_first_things, *g_dungeon_text_data, *g_thing_data[16], *g_dungeon_raw_map_data;
int16_t *g_dungeon_map_first_column;
int16_t g_dungeon_column_count;
uint8_t ***g_dungeon_map_data;
uint16_t g_dungeon_id = 10;
int16_t g_event_max_count;
int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;

const uint8_t g_thing_data_byte_count[16] = {4, 6, 4, 8, 16, 4, 4, 4, 4, 8, 4, 0, 0, 0, 8, 4};
/* KAOS tables, read at run time: kaos_load() must run first. */
#define NEW_GAME_EXTRA_THINGS 0x101058
#define MESSAGE_ESCAPES 0x1016ea
#define REPLACEMENT_CHARS 0x1017ea
#define INSCRIPTION_ESCAPES 0x10182a

#define MAX_DUNGEON_SIZE 0x100000

static void *blocks[32];
static int block_count;
static const uint8_t *load_ptr;
static int32_t load_remaining;

static void *alloc(size_t n)
{
    return blocks[block_count++] = calloc(n ? n : 1, 1);
}

static int16_t read_summed_block(uint8_t *dst, uint16_t *sum, int32_t n)
{
    if (!dst || load_remaining < n)
        return 0;
    memcpy(dst, load_ptr, n);
    load_ptr += n;
    load_remaining -= n;
    while (n--)
        *sum += *dst++;
    return 1;
}

typedef struct {
    const uint8_t *p, *end;
    uint32_t acc;
    int16_t avail, ok;
} BitReader;

static uint16_t get_bits(BitReader *b, int16_t n)
{
    if (b->avail < n) {
        if (b->end - b->p < 2)
            return b->ok = 0;
        b->acc = b->acc << 16 | BE16(b->p);
        b->p += 2;
        b->avail += 16;
    }
    b->avail -= n;
    return b->acc >> b->avail & ((1u << n) - 1);
}

/* Returns 0 when the bit stream ends before byte_count bytes. */
int16_t decompress_dungeon(const uint8_t *src, int32_t src_size, uint8_t *dst, int32_t byte_count)
{
    BitReader b = {src + 20, src + src_size, 0, 0, 1};
    if (src_size < 20)
        return 0;
    while (byte_count-- > 0 && b.ok) {
        if (!get_bits(&b, 1))
            *dst++ = src[get_bits(&b, 2)];
        else if (!get_bits(&b, 1))
            *dst++ = src[4 + get_bits(&b, 4)];
        else
            *dst++ = (uint8_t)get_bits(&b, 8);
    }
    return b.ok;
}

static void dungeon_free(void)
{
    while (block_count)
        free(blocks[--block_count]);
    g_dungeon_header = g_dungeon_maps = g_dungeon_column_first_thing_index = NULL;
    g_square_first_things = g_dungeon_text_data = g_dungeon_raw_map_data = NULL;
    memset(g_thing_data, 0, sizeof g_thing_data);
    g_dungeon_map_first_column = NULL;
    g_dungeon_map_data = NULL;
    g_dungeon_column_count = 0;
}

/* New game only: the save game path adds no free records. */
static int16_t parse_dungeon(void)
{
    uint16_t sum = 0, checksum;
    uint8_t *h = g_dungeon_header = alloc(44);
    if (!read_summed_block(h, &sum, 44))
        return 0;
    uint16_t loc = BE16(h + 8);
    g_party_map_x = loc & 0x1f;
    g_party_map_y = loc >> 5 & 0x1f;
    g_party_direction = loc >> 10 & 3;
    g_party_map_index = 0;

    int16_t maps = h[4];
    if (!maps)
        return 0;
    g_dungeon_maps = alloc(maps * 16);
    if (!read_summed_block(g_dungeon_maps, &sum, maps * 16))
        return 0;
    if (!(g_dungeon_map_first_column = alloc(maps * 2)))
        return 0;
    int16_t columns = 0;
    for (int16_t m = 0; m < maps; m++) {
        g_dungeon_map_first_column[m] = columns;
        columns += dungeon_map_width(m);
    }
    g_dungeon_column_count = columns;

    uint16_t square_things = BE16(h + 10);
    if (square_things > 0xffff - 300)
        return 0;
    WR16(h + 10, square_things + 300);
    g_dungeon_column_first_thing_index = alloc(columns * 2);
    g_square_first_things = alloc((square_things + 300) * 2);
    g_dungeon_text_data = alloc(BE16(h + 6) * 2);
    if (!read_summed_block(g_dungeon_column_first_thing_index, &sum, columns * 2) ||
        !read_summed_block(g_square_first_things, &sum, square_things * 2))
        return 0;
    memset(g_square_first_things + square_things * 2, 0xff, 300 * 2);
    if (!read_summed_block(g_dungeon_text_data, &sum, BE16(h + 6) * 2))
        return 0;

    g_event_max_count = 100;
    for (int16_t t = 0; t < 16; t++) {
        int16_t count = (int16_t)BE16(h + 12 + t * 2), size = g_thing_data_byte_count[t];
        int16_t total = count + kaos_u8(NEW_GAME_EXTRA_THINGS + t), cap = t == 15 ? 0x300 : 0x400;
        if (count < 0 || count > cap)
            return 0;
        if (total > cap)
            total = cap;
        WR16(h + 12 + t * 2, total);
        g_thing_data[t] = alloc(total * size);
        if (!read_summed_block(g_thing_data[t], &sum, count * size))
            return 0;
        if (t == 4 || t > 13)
            g_event_max_count += total;
        for (int16_t i = count; i < total; i++)
            WR16(g_thing_data[t] + i * size, 0xffff);
    }

    g_dungeon_raw_map_data = alloc(BE16(h + 2));
    if (!read_summed_block(g_dungeon_raw_map_data, &sum, BE16(h + 2)))
        return 0;
    if (load_remaining >= 2) {
        checksum = BE16(load_ptr);
        if (checksum != sum)
            return 0;
    }

    for (int16_t m = 0; m < maps; m++)
        if (BE16(g_dungeon_maps + m * 16) + dungeon_map_width(m) * dungeon_map_height(m) > BE16(h + 2))
            return 0;
    g_dungeon_map_data = alloc(maps * sizeof *g_dungeon_map_data);
    uint8_t **cols = alloc(columns * sizeof *cols);
    if (!g_dungeon_map_data || !cols)
        return 0;
    for (int16_t m = 0; m < maps; m++) {
        uint8_t **c = g_dungeon_map_data[m] = cols + g_dungeon_map_first_column[m];
        for (int16_t x = 0; x < dungeon_map_width(m); x++)
            c[x] = g_dungeon_raw_map_data + BE16(g_dungeon_maps + m * 16) + x * dungeon_map_height(m);
    }
    return 1;
}

int16_t load_dungeon(const char *path)
{
    size_t size;
    uint8_t *file = read_file(path, &size), *unpacked = NULL;
    int16_t ok = kaos.data && file && size >= 8;
    dungeon_free();
    load_ptr = file;
    load_remaining = (int32_t)size;
    if (ok && BE16(file) == 0x8104) {
        load_remaining = (int32_t)BE32(file + 2);
        g_dungeon_id = BE16(file + 6);
        load_ptr = unpacked = load_remaining > 0 && load_remaining <= MAX_DUNGEON_SIZE ? malloc(load_remaining) : NULL;
        ok = unpacked && decompress_dungeon(file + 8, (int32_t)size - 8, unpacked, load_remaining);
    }
    ok = ok && size <= MAX_DUNGEON_SIZE && parse_dungeon();
    if (!ok)
        dungeon_free();
    free(file);
    free(unpacked);
    return ok;
}

int16_t dungeon_map_count(void)
{
    return g_dungeon_header ? g_dungeon_header[4] : 0;
}

int16_t dungeon_map_width(int16_t map)
{
    return (BE16(g_dungeon_maps + map * 16 + 8) >> 6 & 0x1f) + 1;
}

int16_t dungeon_map_height(int16_t map)
{
    return (BE16(g_dungeon_maps + map * 16 + 8) >> 11) + 1;
}

uint8_t dungeon_square(int16_t map, int16_t x, int16_t y)
{
    return g_dungeon_map_data[map][x][y];
}

/* get_square_first_thing (0x10ac4e) for any map, not only the current one. */
uint16_t dungeon_square_first_thing(int16_t map, int16_t x, int16_t y)
{
    if (x < 0 || x >= dungeon_map_width(map) || y < 0 || y >= dungeon_map_height(map))
        return 0xfffe;
    const uint8_t *col = g_dungeon_map_data[map][x];
    if (!(col[y] & 0x10))
        return 0xfffe;
    uint16_t index = BE16(g_dungeon_column_first_thing_index + (g_dungeon_map_first_column[map] + x) * 2);
    for (int16_t i = 0; i < y; i++)
        index += (col[i] & 0x10) != 0;
    if (index >= BE16(g_dungeon_header + 10))
        return 0xfffe;
    return BE16(g_square_first_things + index * 2);
}

uint8_t *get_thing_data(uint16_t thing)
{
    uint16_t type = thing >> 10 & 0xf;
    return g_thing_data[type] + g_thing_data_byte_count[type] * (thing & 0x3ff);
}

#define PUT(c) do { if (dst == end) { ok = 0; goto done; } *dst++ = (char)(c); } while (0)

/* Mode 0 inscription, 1 message, 2 scroll; bit 15 decodes an invisible text.
   Returns 0 when the text offset is bad or the text does not fit in dst_size bytes. */
int16_t decode_text(char *dst, int16_t dst_size, uint16_t text_thing, int16_t mode)
{
    if (dst_size < 1)
        return 0;
    char *end = dst + dst_size - 1;
    uint16_t bits = BE16(get_thing_data(text_thing) + 2), w = bits >> 3, words = BE16(g_dungeon_header + 6);
    int16_t ok = 1;
    if ((bits & 1) || mode < 0) {
        mode &= 0x7fff;
        char separator = mode == 0 ? (char)0x80 : mode == 1 ? ' ' : '\n';
        if (mode == 1)
            PUT('\n');
        uint16_t word = 0, code, escape = 0;
        for (int16_t k = 0;; k = (k + 1) % 3) {
            if (k == 0) {
                if (w >= words) {
                    ok = 0;
                    break;
                }
                word = BE16(g_dungeon_text_data + w * 2);
                w++;
            }
            code = word >> (10 - 5 * k) & 0x1f;
            if (escape) {
                for (const char *s = (const char *)kaos_ptr(escape == 29 ? REPLACEMENT_CHARS + code * 2 : (mode == 0 ? INSCRIPTION_ESCAPES : MESSAGE_ESCAPES) + code * 8); *s; s++)
                    PUT(*s);
                escape = 0;
            } else if (code <= 27) {
                PUT(mode == 0 ? code : code == 26 ? ' ' : code == 27 ? '.' : code + 'A');
            } else if (code == 28) {
                PUT(separator);
            } else if (code == 31) {
                break;
            } else {
                escape = code;
            }
        }
    }
done:
    *dst = mode == 0 ? (char)0x81 : 0;
    return ok;
}
