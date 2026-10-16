/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_DUNGEON_H
#define DM_ENGINE_DUNGEON_H
/* Prefixes: dungeon_ */
#include <stdbool.h>
#include <stdint.h>

#define THING_TYPE_COUNT 16

/* All buffers keep the big-endian byte layout of the original memory. */
extern uint8_t *g_dungeon_header;           /* 44 bytes */
extern uint8_t *g_dungeon_maps;             /* 16 bytes per map */
extern int16_t *g_dungeon_map_first_column; /* native, one per map */
extern int16_t g_dungeon_column_count;
extern uint8_t *g_dungeon_column_first_thing_index; /* word per column */
extern uint8_t *g_square_first_things;              /* word per square with things */
extern uint8_t *g_dungeon_text_data;
extern uint8_t *g_thing_data[THING_TYPE_COUNT];
extern uint8_t *g_dungeon_raw_map_data;
extern uint8_t ***g_dungeon_map_data; /* [map][x] -> column of squares */
extern const uint8_t g_thing_data_byte_count[THING_TYPE_COUNT];
extern uint16_t g_dungeon_id;
extern int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;

/* Each byte is a 2-bit index into the first 4 dictionary bytes, a 4-bit index into the next 16, or a
   literal byte. Returns false when the bit stream ends before byte_count bytes. */
bool dungeon_decompress(const uint8_t *src, int32_t src_size, uint8_t *dst, int32_t byte_count);
/* Loads a new game dungeon file, compressed or not. Returns false with nothing loaded. */
bool dungeon_load(const char *path);
/* The dungeon part of a save file: the parts without compression and free records. */
bool dungeon_load_saved(const uint8_t *data, uint32_t size);
/* Checks the links of the loaded dungeon: thing lists, square thing lists, teleporter targets, and the map below
   stairs and open pits. A corrupt file or save game fails here instead of crashing the game later. */
bool dungeon_validate(void);
/* The dungeon parts in the order of save_game, then the 16-bit sum of their bytes. Returns a
   malloc buffer, or NULL when no dungeon is loaded. */
uint8_t *dungeon_save(uint32_t *size);

int16_t dungeon_map_count(void);
int16_t dungeon_map_width(int16_t map);
int16_t dungeon_map_height(int16_t map);
uint8_t dungeon_get_square(int16_t map, int16_t x, int16_t y);
/* map_get_square_first_thing for any map, not only the current one. */
uint16_t dungeon_get_square_first_thing(int16_t map, int16_t x, int16_t y);
uint8_t *dungeon_get_thing_data(uint16_t thing);
/* dungeon_decode_text modes and buffer sizes. */
#define TEXT_MODE_MESSAGE        1
#define TEXT_MODE_SCROLL         2
#define TEXT_MODE_ANY_VISIBILITY ((int16_t)0x8000)
#define TEXT_SIZE                256
#define SCROLL_TEXT_SIZE         200
/* Mode 0 inscription, 1 message, 2 scroll; bit 15 decodes an invisible text.
   Returns false when the text offset is bad or the text does not fit in dst_size bytes. */
bool dungeon_decode_text(char *dst, int16_t dst_size, uint16_t text_thing, int16_t mode);

#endif
