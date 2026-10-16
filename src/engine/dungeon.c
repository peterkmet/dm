/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/dungeon.h"
#include "data/adventure.h"
#include "data/file.h"
#include "data/ftl.h"
#include "data/tables.h"
#include "engine/game.h"
#include "engine/map.h"
#include <stdlib.h>
#include <string.h>

uint8_t *g_dungeon_header, *g_dungeon_maps, *g_dungeon_column_first_thing_index;
uint8_t *g_square_first_things, *g_dungeon_text_data, *g_thing_data[THING_TYPE_COUNT], *g_dungeon_raw_map_data;
int16_t *g_dungeon_map_first_column;
int16_t g_dungeon_column_count;
uint8_t ***g_dungeon_map_data;
uint16_t g_dungeon_id = DM_DUNGEON_ID;
int16_t g_event_max_count;
int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;

const uint8_t g_thing_data_byte_count[THING_TYPE_COUNT] = {4, 6, 4, 8, GROUP_RECORD_BYTES, 4, 4, 4, 4, 8, 4, 0,
                                                           0, 0, 8, 4};
#define MAX_DUNGEON_SIZE            (1024 * 1024)
#define DUNGEON_COMPRESSED_MAGIC    0x8104
#define COMPRESSED_HEADER_BYTES     8  /* magic, unpacked size, dungeon id */
#define COMPRESSED_DICTIONARY_BYTES 20 /* 4 two-bit codes, then 16 four-bit codes */

/* Dungeon header fields. */
#define HEADER_BYTES                44
#define HEADER_RAW_MAP_BYTES        2
#define HEADER_MAP_COUNT            4
#define HEADER_TEXT_WORDS           6
#define HEADER_PARTY_START          8 /* x in bits 0-4, y in bits 5-9, direction in bits 10-11 */
#define PARTY_START_Y_SHIFT         5
#define PARTY_START_DIRECTION_SHIFT 10

#define MAP_RAW_DATA_OFFSET 0

#define FREE_SQUARE_THINGS   300 /* free square-first-thing entries of a new game */
#define NEW_GAME_EVENT_COUNT 100
#define MAX_THINGS           1024
#define MAX_EXPLOSIONS       768
#define MAX_BLOCKS           32 /* the buffers of one loaded dungeon */

/* Save order: header, maps, column index, square things, text, the 16 thing types, raw map data. */
#define PART_THINGS_FIRST  5
#define PART_RAW_MAPS      (PART_THINGS_FIRST + THING_TYPE_COUNT)
#define DUNGEON_PART_COUNT 22

static void *blocks[MAX_BLOCKS];
static int block_count;
static const uint8_t *load_cursor;
static int32_t load_remaining;

static void *alloc(size_t n)
{
    return blocks[block_count++] = calloc(n ? n : 1, 1);
}

/* Copies the next n bytes of the dungeon file to dst and adds them to the checksum. */
static bool read_summed_block(uint8_t *dst, uint16_t *sum, int32_t n)
{
    if (!dst || load_remaining < n)
        return false;
    memcpy(dst, load_cursor, n);
    load_cursor += n;
    load_remaining -= n;
    while (n--)
        *sum += *dst++;
    return true;
}

typedef struct {
    const uint8_t *p, *end;
    uint32_t acc;
    int16_t avail;
    bool ok;
} BitReader;

/* The next n bits of the compressed dungeon, high bit first; at the end of the data it clears ok. */
static uint16_t get_bits(BitReader *b, int16_t n)
{
    if (b->avail < n) {
        if (b->end - b->p < 2)
            return b->ok = false;
        b->acc = b->acc << WORD_BITS | BE16(b->p);
        b->p += 2;
        b->avail += WORD_BITS;
    }
    b->avail -= n;
    return b->acc >> b->avail & ((1u << n) - 1);
}

bool dungeon_decompress(const uint8_t *src, int32_t src_size, uint8_t *dst, int32_t byte_count)
{
    BitReader b = {src + COMPRESSED_DICTIONARY_BYTES, src + src_size, 0, 0, true};
    if (src_size < COMPRESSED_DICTIONARY_BYTES)
        return false;
    while (byte_count-- > 0 && b.ok) {
        if (!get_bits(&b, 1))
            *dst++ = src[get_bits(&b, 2)];
        else if (!get_bits(&b, 1))
            *dst++ = src[4 + get_bits(&b, 4)];
        else
            *dst++ = get_bits(&b, 8);
    }
    return b.ok;
}

/* Frees the loaded dungeon and clears its pointers. */
static void dungeon_free(void)
{
    while (block_count)
        free(blocks[--block_count]);
    g_dungeon_header = NULL;
    g_dungeon_maps = NULL;
    g_dungeon_column_first_thing_index = NULL;
    g_square_first_things = NULL;
    g_dungeon_text_data = NULL;
    g_dungeon_raw_map_data = NULL;
    memset(g_thing_data, 0, sizeof g_thing_data);
    g_dungeon_map_first_column = NULL;
    g_dungeon_map_data = NULL;
    g_dungeon_column_count = 0;
}

static uint16_t raw_map_bytes(void)
{
    return BE16(g_dungeon_header + HEADER_RAW_MAP_BYTES);
}
static uint16_t count_text_words(void)
{
    return BE16(g_dungeon_header + HEADER_TEXT_WORDS);
}
static uint16_t thing_count(int16_t type)
{
    return BE16(g_dungeon_header + DUNGEON_HEADER_THING_COUNTS + type * 2);
}
static void set_thing_count(int16_t type, uint16_t count)
{
    WR16(g_dungeon_header + DUNGEON_HEADER_THING_COUNTS + type * 2, count);
}
static uint16_t map_raw_data_offset(int16_t map)
{
    return BE16(MAP_DESCRIPTOR(map) + MAP_RAW_DATA_OFFSET);
}
static uint16_t get_column_first_thing_index(int16_t map, int16_t x)
{
    return BE16(g_dungeon_column_first_thing_index + (g_dungeon_map_first_column[map] + x) * 2);
}

/* Reads the header. A new game takes the party start from it. */
static bool read_header(bool new_game, uint16_t *sum)
{
    g_dungeon_header = alloc(HEADER_BYTES);
    uint8_t *h = g_dungeon_header;
    if (!read_summed_block(h, sum, HEADER_BYTES))
        return false;
    if (new_game) {
        uint16_t start = BE16(h + HEADER_PARTY_START);
        g_party_map_x = start & MAP_COORD_MASK;
        g_party_map_y = start >> PARTY_START_Y_SHIFT & MAP_COORD_MASK;
        g_party_direction = NORMALIZE(start >> PARTY_START_DIRECTION_SHIFT);
        g_party_map_index = 0;
    }
    return true;
}

/* Reads the map descriptors and numbers the columns of all maps. */
static bool read_maps(int16_t maps, uint16_t *sum)
{
    g_dungeon_maps = alloc(maps * MAP_BYTES);
    if (!read_summed_block(g_dungeon_maps, sum, maps * MAP_BYTES))
        return false;
    if (!(g_dungeon_map_first_column = alloc(maps * 2)))
        return false;
    int16_t columns = 0;
    for (int16_t m = 0; m < maps; m++) {
        g_dungeon_map_first_column[m] = columns;
        columns += dungeon_map_width(m);
    }
    g_dungeon_column_count = columns;
    return true;
}

/* Reads the column index, the square first things, and the text words. A new game adds free square entries. */
static bool read_square_things_and_text(bool new_game, uint16_t *sum)
{
    uint16_t square_things = rec_dungeon_square_first_thing_count();
    uint16_t extra = new_game ? FREE_SQUARE_THINGS : 0;
    if (square_things > UINT16_MAX - extra)
        return false;
    WR16(g_dungeon_header + DUNGEON_HEADER_SQUARE_THINGS, square_things + extra);
    g_dungeon_column_first_thing_index = alloc(g_dungeon_column_count * 2);
    g_square_first_things = alloc((square_things + extra) * 2);
    g_dungeon_text_data = alloc(count_text_words() * 2);
    if (!read_summed_block(g_dungeon_column_first_thing_index, sum, g_dungeon_column_count * 2) ||
        !read_summed_block(g_square_first_things, sum, square_things * 2))
        return false;
    for (uint16_t i = square_things; i < square_things + extra; i++)
        WR16(g_square_first_things + i * 2, THING_NONE);
    return read_summed_block(g_dungeon_text_data, sum, count_text_words() * 2);
}

/* Reads the records of each thing type. A new game adds free records from the KAOS table and counts the
   timeline events that groups, projectiles, and explosions can need. */
static bool read_things(bool new_game, uint16_t *sum)
{
    if (new_game)
        g_event_max_count = NEW_GAME_EVENT_COUNT;
    for (int16_t t = 0; t < THING_TYPE_COUNT; t++) {
        /* a count above 32767 is negative */
        int16_t count = (int16_t)thing_count(t);
        int16_t size = g_thing_data_byte_count[t];
        int16_t total = count;
        int16_t cap = t == THING_EXPLOSION ? MAX_EXPLOSIONS : MAX_THINGS;
        if (count < 0 || count > cap)
            return false;
        if (new_game) {
            int16_t extra = tables_get_new_game_extra_things(t);
            total = count + extra > cap ? cap : count + extra;
        }
        set_thing_count(t, total);
        g_thing_data[t] = alloc(total * size);
        if (!read_summed_block(g_thing_data[t], sum, count * size))
            return false;
        if (new_game && (t == THING_GROUP || t == THING_PROJECTILE || t == THING_EXPLOSION))
            g_event_max_count += total;
        for (int16_t i = count; i < total; i++)
            WR16(g_thing_data[t] + i * size, THING_NONE);
    }
    return true;
}

/* Points each map column into the raw map data, after a check that each map fits in it. */
static bool build_map_columns(int16_t maps)
{
    for (int16_t m = 0; m < maps; m++)
        if (map_raw_data_offset(m) + dungeon_map_width(m) * dungeon_map_height(m) > raw_map_bytes())
            return false;
    g_dungeon_map_data = alloc(maps * sizeof *g_dungeon_map_data);
    uint8_t **cols = alloc(g_dungeon_column_count * sizeof *cols);
    if (!g_dungeon_map_data || !cols)
        return false;
    for (int16_t m = 0; m < maps; m++) {
        g_dungeon_map_data[m] = cols + g_dungeon_map_first_column[m];
        uint8_t **c = g_dungeon_map_data[m];
        for (int16_t x = 0; x < dungeon_map_width(m); x++)
            c[x] = g_dungeon_raw_map_data + map_raw_data_offset(m) + x * dungeon_map_height(m);
    }
    return true;
}

/* Reads the dungeon parts from the load cursor. A new game takes the party start from the header
   and adds free records; a saved game has them already. */
static bool parse_dungeon(bool new_game)
{
    uint16_t sum = 0;
    if (!read_header(new_game, &sum))
        return false;
    int16_t maps = dungeon_map_count();
    if (!maps || !read_maps(maps, &sum) || !read_square_things_and_text(new_game, &sum) || !read_things(new_game, &sum))
        return false;
    g_dungeon_raw_map_data = alloc(raw_map_bytes());
    if (!read_summed_block(g_dungeon_raw_map_data, &sum, raw_map_bytes()))
        return false;
    if (load_remaining >= 2 && BE16(load_cursor) != sum)
        return false;
    return build_map_columns(maps) && dungeon_validate();
}

/* A thing word that names an existing record. */
static bool is_valid_thing(uint16_t thing)
{
    uint16_t type = THING_TYPE(thing);
    return g_thing_data_byte_count[type] && THING_INDEX(thing) < thing_count(type);
}

static bool valid_link(uint16_t thing)
{
    return thing == THING_ENDOFLIST || is_valid_thing(thing);
}

/* Groups and containers: word 1 starts the list of the things they carry. */
static uint16_t get_carried_first_thing(uint16_t thing)
{
    return BE16(REC(thing) + 2);
}

/* The map one level up or down that covers the dungeon position of (x, y) on map, or -1. */
static int16_t map_at_level(int16_t map, int16_t level_delta, int16_t x, int16_t y)
{
    int16_t dungeon_x = x + map_offset_x(map);
    int16_t dungeon_y = y + map_offset_y(map);
    int16_t level = map_level(map) + level_delta;
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        if (map_level(m) == level && map_offset_x(m) <= dungeon_x &&
            dungeon_x < map_offset_x(m) + dungeon_map_width(m) && map_offset_y(m) <= dungeon_y &&
            dungeon_y < map_offset_y(m) + dungeon_map_height(m))
            return m;
    }
    return -1;
}

/* A teleporter record points to a square inside a map. */
static bool is_valid_teleporter(uint16_t thing)
{
    int16_t map = teleporter_target_map(thing);
    return map < dungeon_map_count() && teleporter_target_x(thing) < dungeon_map_width(map) &&
           teleporter_target_y(thing) < dungeon_map_height(map);
}

/* Checks the thing lists of all used records and the teleporter targets. */
static bool validate_records(void)
{
    for (int16_t type = 0; type < THING_TYPE_COUNT; type++) {
        for (uint16_t i = 0; g_thing_data_byte_count[type] && i < thing_count(type); i++) {
            uint16_t thing = THING_MAKE(type, i);
            uint16_t next = map_get_next_thing(thing);
            if (next == THING_NONE)
                continue;
            if (!valid_link(next))
                return false;
            if ((type == THING_GROUP || type == THING_CONTAINER) && !valid_link(get_carried_first_thing(thing)))
                return false;
            if (type == THING_TELEPORTER && !is_valid_teleporter(thing))
                return false;
        }
    }
    return true;
}

/* Checks the square thing lists, and that stairs and open pits lead to a map. */
static bool validate_squares(void)
{
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        for (int16_t x = 0; x < dungeon_map_width(m); x++) {
            uint16_t index = get_column_first_thing_index(m, x);
            for (int16_t y = 0; y < dungeon_map_height(m); y++) {
                uint8_t square = g_dungeon_map_data[m][x][y];
                int16_t type = SQUARE_TYPE(square);
                if (square & SQUARE_HAS_THINGS) {
                    if (index >= rec_dungeon_square_first_thing_count() ||
                        !valid_link(BE16(g_square_first_things + index * 2)))
                        return false;
                    index++;
                }
                bool down = type == SQUARE_PIT && (square & PIT_OPEN) && !(square & PIT_IMAGINARY);
                if (type == SQUARE_STAIRS && map_at_level(m, square & STAIRS_UP ? LEVEL_UP : LEVEL_DOWN, x, y) < 0)
                    return false;
                if (down && map_at_level(m, LEVEL_DOWN, x, y) < 0)
                    return false;
            }
        }
    }
    return true;
}

bool dungeon_validate(void)
{
    return validate_records() && validate_squares();
}

bool dungeon_load(const char *path)
{
    size_t size;
    uint8_t *file = file_read(path, &size);
    uint8_t *unpacked = NULL;
    bool ok = kaos.data && file && size >= COMPRESSED_HEADER_BYTES;
    dungeon_free();
    load_cursor = file;
    load_remaining = size;
    if (ok && BE16(file) == DUNGEON_COMPRESSED_MAGIC) {
        load_remaining = BE32(file + 2);
        g_dungeon_id = BE16(file + 6);
        unpacked = load_remaining > 0 && load_remaining <= MAX_DUNGEON_SIZE ? malloc(load_remaining) : NULL;
        load_cursor = unpacked;
        ok = unpacked && dungeon_decompress(file + COMPRESSED_HEADER_BYTES, (int32_t)size - COMPRESSED_HEADER_BYTES,
                                            unpacked, load_remaining);
    }
    ok = ok && size <= MAX_DUNGEON_SIZE && parse_dungeon(true);
    if (!ok)
        dungeon_free();
    free(file);
    free(unpacked);
    return ok;
}

bool dungeon_load_saved(const uint8_t *data, uint32_t size)
{
    dungeon_free();
    load_cursor = data;
    load_remaining = size;
    bool ok = data && size <= MAX_DUNGEON_SIZE && parse_dungeon(false);
    if (!ok)
        dungeon_free();
    return ok;
}

uint8_t *dungeon_save(uint32_t *size)
{
    if (!g_dungeon_header)
        return NULL;
    const uint8_t *part[DUNGEON_PART_COUNT] = {
        g_dungeon_header,      g_dungeon_maps,      g_dungeon_column_first_thing_index,
        g_square_first_things, g_dungeon_text_data,
    };
    uint32_t bytes[DUNGEON_PART_COUNT] = {
        HEADER_BYTES,
        (uint32_t)dungeon_map_count() * MAP_BYTES,
        (uint32_t)g_dungeon_column_count * 2,
        (uint32_t)rec_dungeon_square_first_thing_count() * 2,
        (uint32_t)count_text_words() * 2,
    };
    for (int16_t t = 0; t < THING_TYPE_COUNT; t++) {
        part[PART_THINGS_FIRST + t] = g_thing_data[t];
        bytes[PART_THINGS_FIRST + t] = (uint32_t)thing_count(t) * g_thing_data_byte_count[t];
    }
    part[PART_RAW_MAPS] = g_dungeon_raw_map_data;
    bytes[PART_RAW_MAPS] = raw_map_bytes();
    uint32_t total = 2;
    for (int i = 0; i < DUNGEON_PART_COUNT; i++)
        total += bytes[i];
    uint8_t *out = malloc(total);
    uint8_t *p = out;
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
    return g_dungeon_header ? g_dungeon_header[HEADER_MAP_COUNT] : 0;
}

int16_t dungeon_map_width(int16_t map)
{
    return map_last_x(map) + 1;
}

int16_t dungeon_map_height(int16_t map)
{
    return map_last_y(map) + 1;
}

uint8_t dungeon_get_square(int16_t map, int16_t x, int16_t y)
{
    return g_dungeon_map_data[map][x][y];
}

uint16_t dungeon_get_square_first_thing(int16_t map, int16_t x, int16_t y)
{
    if (x < 0 || x >= dungeon_map_width(map) || y < 0 || y >= dungeon_map_height(map))
        return THING_ENDOFLIST;
    const uint8_t *col = g_dungeon_map_data[map][x];
    if (!(col[y] & SQUARE_HAS_THINGS))
        return THING_ENDOFLIST;
    uint16_t index = get_column_first_thing_index(map, x);
    for (int16_t i = 0; i < y; i++)
        index += (col[i] & SQUARE_HAS_THINGS) != 0;
    if (index >= rec_dungeon_square_first_thing_count())
        return THING_ENDOFLIST;
    return BE16(g_square_first_things + index * 2);
}

uint8_t *dungeon_get_thing_data(uint16_t thing)
{
    uint16_t type = THING_TYPE(thing);
    return g_thing_data[type] + g_thing_data_byte_count[type] * THING_INDEX(thing);
}

/* dungeon_decode_text modes; MODE_ANY_VISIBILITY also decodes an invisible text. */
#define MODE_INSCRIPTION        0
#define MODE_MESSAGE            0x0001
#define MODE_SCROLL             0x0002
#define MODE_MASK               0x7fff
/* 5-bit text codes: 0-25 letters, then space, period, separator, two escapes, end. */
#define TEXT_CODE_BITS          5
#define TEXT_CODE_MASK          0x1f
#define TEXT_CODES_PER_WORD     3
#define CODE_SPACE              26
#define CODE_PERIOD             27
#define CODE_SEPARATOR          28
#define CODE_ESCAPE_REPLACEMENT 29
#define CODE_ESCAPE_STRING      30
#define CODE_END                31
#define INSCRIPTION_SEPARATOR   ((char)0x80) /* the wall text font ends a line here */
#define INSCRIPTION_END         ((char)0x81)

typedef struct {
    char *p, *end;
    bool ok;
} TextOut;

/* Appends one character of a decoded text; a full buffer clears ok. */
static void put(TextOut *o, char c)
{
    if (o->p == o->end)
        o->ok = false;
    else
        *o->p++ = c;
}

static void put_string(TextOut *o, const char *s)
{
    for (; *s && o->ok; s++)
        put(o, *s);
}

/* The string of an escape code: a replacement, or an inscription or message string. */
static const char *escape_string(uint16_t escape, uint16_t code, int16_t mode)
{
    if (escape == CODE_ESCAPE_REPLACEMENT)
        return tables_get_escape_replacement(code);
    return mode == MODE_INSCRIPTION ? tables_get_inscription_escape(code) : tables_get_message_escape(code);
}

/* Decodes the 5-bit codes from word offset w, three per word, until the end code. */
static void decode_codes(TextOut *o, uint16_t w, int16_t mode)
{
    char separator = mode == MODE_INSCRIPTION ? INSCRIPTION_SEPARATOR : mode == MODE_MESSAGE ? ' ' : '\n';
    uint16_t word = 0;
    uint16_t escape = 0;
    if (mode == MODE_MESSAGE)
        put(o, '\n');
    for (int16_t k = 0; o->ok; k = (k + 1) % TEXT_CODES_PER_WORD) {
        if (k == 0) {
            if (w >= count_text_words()) {
                o->ok = false;
                break;
            }
            word = BE16(g_dungeon_text_data + w * 2);
            w++;
        }
        uint16_t code = word >> (TEXT_CODES_PER_WORD - 1 - k) * TEXT_CODE_BITS & TEXT_CODE_MASK;
        if (escape) {
            put_string(o, escape_string(escape, code, mode));
            escape = 0;
        } else if (code <= CODE_PERIOD) {
            put(o, mode == MODE_INSCRIPTION ? code : code == CODE_SPACE ? ' ' : code == CODE_PERIOD ? '.' : code + 'A');
        } else if (code == CODE_SEPARATOR) {
            put(o, separator);
        } else if (code == CODE_END) {
            break;
        } else {
            escape = code;
        }
    }
}

bool dungeon_decode_text(char *dst, int16_t dst_size, uint16_t text_thing, int16_t mode)
{
    if (dst_size < 1)
        return false;
    TextOut o = {dst, dst + dst_size - 1, true};
    uint16_t offset = BE16(REC(text_thing) + 2) >> 3; /* text word offset in bits 3-15 */
    if (rec_text_is_visible(text_thing) || mode < 0) {
        mode &= MODE_MASK;
        decode_codes(&o, offset, mode);
    }
    *o.p = mode == MODE_INSCRIPTION ? INSCRIPTION_END : 0;
    return o.ok;
}
