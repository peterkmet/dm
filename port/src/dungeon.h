#ifndef DUNGEON_H
#define DUNGEON_H
#include <stdint.h>

/* All buffers keep the big-endian byte layout of the original memory. */
extern uint8_t *g_dungeon_header;                   /* 44 bytes */
extern uint8_t *g_dungeon_maps;                     /* 16 bytes per map */
extern int16_t *g_dungeon_map_first_column;         /* native, one per map */
extern int16_t g_dungeon_column_count;
extern uint8_t *g_dungeon_column_first_thing_index; /* word per column */
extern uint8_t *g_square_first_things;              /* word per square with things */
extern uint8_t *g_dungeon_text_data;
extern uint8_t *g_thing_data[16];
extern uint8_t *g_dungeon_raw_map_data;
extern uint8_t ***g_dungeon_map_data;               /* [map][x] -> column of squares */
extern const uint8_t g_thing_data_byte_count[16];
extern uint16_t g_dungeon_id;
extern int16_t g_event_max_count;
extern int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;

int16_t decompress_dungeon(const uint8_t *src, int32_t src_size, uint8_t *dst, int32_t byte_count);
int16_t load_dungeon(const char *path);

int16_t dungeon_map_count(void);
int16_t dungeon_map_width(int16_t map);
int16_t dungeon_map_height(int16_t map);
uint8_t dungeon_square(int16_t map, int16_t x, int16_t y);
uint16_t dungeon_square_first_thing(int16_t map, int16_t x, int16_t y);
uint8_t *get_thing_data(uint16_t thing);
int16_t decode_text(char *dst, int16_t dst_size, uint16_t text_thing, int16_t mode);

#endif
