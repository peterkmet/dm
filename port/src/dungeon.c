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

#define HEADER_BYTES 44
#define MAP_BYTES 16
#define FREE_SQUARE_THINGS 300 /* free square-first-thing entries of a new game */
#define DUNGEON_PART_COUNT (5 + 16 + 1)

/* Header words: raw map bytes, text words, square-first-thing count, thing counts. */
static uint16_t raw_map_bytes(void) { return BE16(g_dungeon_header + 2); }
static uint16_t text_words(void) { return BE16(g_dungeon_header + 6); }
static uint16_t square_thing_count(void) { return BE16(g_dungeon_header + 10); }
static uint16_t thing_count(int16_t type) { return BE16(g_dungeon_header + 12 + type * 2); }

/* load_dungeon: reads the parts from load_ptr. A new game takes the party start from the header
   and adds free records; a saved game has them already. */
static int16_t parse_dungeon(int new_game)
{
    uint16_t sum = 0, checksum;
    uint8_t *h = g_dungeon_header = alloc(HEADER_BYTES);
    if (!read_summed_block(h, &sum, HEADER_BYTES))
        return 0;
    if (new_game) {
        uint16_t loc = BE16(h + 8);
        g_party_map_x = loc & 0x1f;
        g_party_map_y = loc >> 5 & 0x1f;
        g_party_direction = loc >> 10 & 3;
        g_party_map_index = 0;
    }

    int16_t maps = h[4];
    if (!maps)
        return 0;
    g_dungeon_maps = alloc(maps * MAP_BYTES);
    if (!read_summed_block(g_dungeon_maps, &sum, maps * MAP_BYTES))
        return 0;
    if (!(g_dungeon_map_first_column = alloc(maps * 2)))
        return 0;
    int16_t columns = 0;
    for (int16_t m = 0; m < maps; m++) {
        g_dungeon_map_first_column[m] = columns;
        columns += dungeon_map_width(m);
    }
    g_dungeon_column_count = columns;

    uint16_t square_things = square_thing_count(), extra = new_game ? FREE_SQUARE_THINGS : 0;
    if (square_things > 0xffff - extra)
        return 0;
    WR16(h + 10, square_things + extra);
    g_dungeon_column_first_thing_index = alloc(columns * 2);
    g_square_first_things = alloc((square_things + extra) * 2);
    g_dungeon_text_data = alloc(text_words() * 2);
    if (!read_summed_block(g_dungeon_column_first_thing_index, &sum, columns * 2) ||
        !read_summed_block(g_square_first_things, &sum, square_things * 2))
        return 0;
    memset(g_square_first_things + square_things * 2, 0xff, extra * 2);
    if (!read_summed_block(g_dungeon_text_data, &sum, text_words() * 2))
        return 0;

    if (new_game)
        g_event_max_count = 100;
    for (int16_t t = 0; t < 16; t++) {
        int16_t count = (int16_t)thing_count(t), size = g_thing_data_byte_count[t];
        int16_t total = count, cap = t == 15 ? 0x300 : 0x400;
        if (count < 0 || count > cap)
            return 0;
        if (new_game)
            total = count + kaos_u8(NEW_GAME_EXTRA_THINGS + t) > cap ? cap : count + kaos_u8(NEW_GAME_EXTRA_THINGS + t);
        WR16(h + 12 + t * 2, total);
        g_thing_data[t] = alloc(total * size);
        if (!read_summed_block(g_thing_data[t], &sum, count * size))
            return 0;
        if (new_game && (t == 4 || t > 13))
            g_event_max_count += total;
        for (int16_t i = count; i < total; i++)
            WR16(g_thing_data[t] + i * size, 0xffff);
    }

    g_dungeon_raw_map_data = alloc(raw_map_bytes());
    if (!read_summed_block(g_dungeon_raw_map_data, &sum, raw_map_bytes()))
        return 0;
    if (load_remaining >= 2) {
        checksum = BE16(load_ptr);
        if (checksum != sum)
            return 0;
    }

    for (int16_t m = 0; m < maps; m++)
        if (BE16(g_dungeon_maps + m * MAP_BYTES) + dungeon_map_width(m) * dungeon_map_height(m) > raw_map_bytes())
            return 0;
    g_dungeon_map_data = alloc(maps * sizeof *g_dungeon_map_data);
    uint8_t **cols = alloc(columns * sizeof *cols);
    if (!g_dungeon_map_data || !cols)
        return 0;
    for (int16_t m = 0; m < maps; m++) {
        uint8_t **c = g_dungeon_map_data[m] = cols + g_dungeon_map_first_column[m];
        for (int16_t x = 0; x < dungeon_map_width(m); x++)
            c[x] = g_dungeon_raw_map_data + BE16(g_dungeon_maps + m * MAP_BYTES) + x * dungeon_map_height(m);
    }
    return dungeon_validate();
}

/* Record layout for the validation; map.h has the accessors for the game rules. */
enum { THING_TYPE_TELEPORTER = 1, THING_TYPE_GROUP = 4, THING_TYPE_CONTAINER = 9, THING_TYPE_COUNT = 16 };
enum { SQUARE_TYPE_SHIFT = 5, SQUARE_PIT = 2, SQUARE_STAIRS = 3, SQUARE_HAS_THINGS = 0x10 };
enum { STAIRS_UP = 4, PIT_OPEN = 8, PIT_IMAGINARY = 1 };
#define THING_LIST_END 0xfffe
#define THING_FREE 0xffff
#define THING_INDEX(thing) ((thing) & 0x3ff)
#define THING_TYPE_OF(thing) ((thing) >> 10 & 15)
#define MAP_LEVEL_MASK 0x3f

/* A thing word that names an existing record. */
static int valid_thing(uint16_t thing)
{
    uint16_t type = THING_TYPE_OF(thing);
    return g_thing_data_byte_count[type] && THING_INDEX(thing) < thing_count(type);
}

static int valid_link(uint16_t thing) { return thing == THING_LIST_END || valid_thing(thing); }

/* The map one level up or down that covers the dungeon position of (x, y) on map, or -1. */
static int16_t map_at_level(int16_t map, int16_t level_delta, int16_t x, int16_t y)
{
    const uint8_t *from = g_dungeon_maps + map * MAP_BYTES;
    int16_t dungeon_x = x + from[6], dungeon_y = y + from[7], level = (from[9] & MAP_LEVEL_MASK) + level_delta;
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        const uint8_t *to = g_dungeon_maps + m * MAP_BYTES;
        if ((to[9] & MAP_LEVEL_MASK) == level && to[6] <= dungeon_x && dungeon_x < to[6] + dungeon_map_width(m) &&
            to[7] <= dungeon_y && dungeon_y < to[7] + dungeon_map_height(m))
            return m;
    }
    return -1;
}

/* A teleporter record points to a square inside a map. */
static int valid_teleporter(const uint8_t *record)
{
    int16_t map = record[4], x = record[3] & 31, y = BE16(record + 2) >> 5 & 31;
    return map < dungeon_map_count() && x < dungeon_map_width(map) && y < dungeon_map_height(map);
}

/* Checks the links of the loaded dungeon: thing lists, square thing lists, teleporter targets, and the map below
   stairs and open pits. A corrupt file or save game fails here instead of crashing the game later. */
int16_t dungeon_validate(void)
{
    for (int16_t type = 0; type < THING_TYPE_COUNT; type++) {
        uint16_t size = g_thing_data_byte_count[type];
        for (uint16_t i = 0; size && i < thing_count(type); i++) {
            const uint8_t *record = g_thing_data[type] + i * size;
            if (BE16(record) == THING_FREE)
                continue;
            if (!valid_link(BE16(record)))
                return 0;
            if ((type == THING_TYPE_GROUP || type == THING_TYPE_CONTAINER) && !valid_link(BE16(record + 2)))
                return 0;
            if (type == THING_TYPE_TELEPORTER && !valid_teleporter(record))
                return 0;
        }
    }
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        for (int16_t x = 0; x < dungeon_map_width(m); x++) {
            uint16_t index = BE16(g_dungeon_column_first_thing_index + (g_dungeon_map_first_column[m] + x) * 2);
            for (int16_t y = 0; y < dungeon_map_height(m); y++) {
                uint8_t square = g_dungeon_map_data[m][x][y];
                int16_t type = square >> SQUARE_TYPE_SHIFT;
                if (square & SQUARE_HAS_THINGS) {
                    if (index >= square_thing_count() || !valid_link(BE16(g_square_first_things + index * 2)))
                        return 0;
                    index++;
                }
                int16_t down = type == SQUARE_PIT && (square & PIT_OPEN) && !(square & PIT_IMAGINARY);
                if (type == SQUARE_STAIRS && map_at_level(m, square & STAIRS_UP ? -1 : 1, x, y) < 0)
                    return 0;
                if (down && map_at_level(m, 1, x, y) < 0)
                    return 0;
            }
        }
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
    ok = ok && size <= MAX_DUNGEON_SIZE && parse_dungeon(1);
    if (!ok)
        dungeon_free();
    free(file);
    free(unpacked);
    return ok;
}

/* The dungeon part of a save file: the parts without compression and free records. */
int16_t dungeon_load_saved(const uint8_t *data, uint32_t size)
{
    dungeon_free();
    load_ptr = data;
    load_remaining = (int32_t)size;
    int16_t ok = data && size <= MAX_DUNGEON_SIZE && parse_dungeon(0);
    if (!ok)
        dungeon_free();
    return ok;
}

/* The dungeon parts in the order of save_game, then the 16-bit sum of their bytes. Returns a
   malloc buffer, or NULL when no dungeon is loaded. */
uint8_t *dungeon_save(uint32_t *size)
{
    if (!g_dungeon_header)
        return NULL;
    const uint8_t *part[DUNGEON_PART_COUNT] = {
        g_dungeon_header, g_dungeon_maps, g_dungeon_column_first_thing_index, g_square_first_things, g_dungeon_text_data,
    };
    uint32_t bytes[DUNGEON_PART_COUNT] = {
        HEADER_BYTES, (uint32_t)dungeon_map_count() * MAP_BYTES, (uint32_t)g_dungeon_column_count * 2,
        (uint32_t)square_thing_count() * 2, (uint32_t)text_words() * 2,
    };
    for (int16_t t = 0; t < 16; t++) {
        part[5 + t] = g_thing_data[t];
        bytes[5 + t] = (uint32_t)thing_count(t) * g_thing_data_byte_count[t];
    }
    part[DUNGEON_PART_COUNT - 1] = g_dungeon_raw_map_data;
    bytes[DUNGEON_PART_COUNT - 1] = raw_map_bytes();
    uint32_t total = 2;
    for (int i = 0; i < DUNGEON_PART_COUNT; i++)
        total += bytes[i];
    uint8_t *out = malloc(total), *p = out;
    if (!out)
        return NULL;
    uint16_t sum = 0;
    for (int i = 0; i < DUNGEON_PART_COUNT; i++) {
        if (bytes[i])
            memcpy(p, part[i], bytes[i]);
        for (uint32_t k = 0; k < bytes[i]; k++)
            sum += p[k];
        p += bytes[i];
    }
    WR16(p, sum);
    *size = total;
    return out;
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
