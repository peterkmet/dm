#ifndef SAVEFILE_H
#define SAVEFILE_H
#include <stdint.h>

/* DM v3.60 save file container (DMGAME~.DAT). Layout: docs/save-format.md. */

#define SAVE_HEADER_BYTES 512
#define SAVE_BLOCKS 5
enum { SAVE_BLOCK_GLOBALS, SAVE_BLOCK_GROUPS, SAVE_BLOCK_CHAMPIONS, SAVE_BLOCK_EVENTS, SAVE_BLOCK_TIMELINE };
#define SAVE_GLOBALS_BYTES 128
#define SAVE_CHAMPIONS_BYTES 0x580  /* 4 champion records and 128 bytes of the party */
#define SAVE_PARTY_BYTES 128
#define SAVE_ACTIVE_GROUP_BYTES 16
#define SAVE_EVENT_BYTES 10
#define SAVE_PAYLOAD_WORDS 128
#define SAVE_PORTRAIT_BYTES 464 /* ((32 + 15) & ~15) / 2 * 29 */
#define SAVE_PORTRAITS_BYTES (4 * SAVE_PORTRAIT_BYTES)
#define SAVE_HEADER_EXTRA_BYTES 134 /* g_save_header_extra, copied unchanged */

/* All buffers keep the big-endian byte layout of the original memory. */
typedef struct {
    uint8_t *data;                       /* save_decode: owned plain copy of the file */
    uint8_t *header;                     /* 512 bytes; the payload at 0x100 is plain */
    uint8_t *block[SAVE_BLOCKS];         /* plain blocks: globals, groups, champions, events, timeline */
    uint16_t block_size[SAVE_BLOCKS];
    uint8_t *portraits;                  /* 4 portraits, not encrypted */
    uint8_t *dungeon;                    /* uncompressed dungeon parts and byte-sum word */
    uint32_t dungeon_size;
} SaveFile;

uint16_t save_block_xor(uint8_t *bytes, uint16_t key, uint16_t word_count);
int16_t save_decode(const uint8_t *file, uint32_t size, SaveFile *s);
uint8_t *save_encode(const SaveFile *s, uint32_t *size);
void save_free(SaveFile *s);

/* Plain header fields. save_header_init fills a new header with random16 words and keys. */
void save_header_init(uint8_t *h, uint32_t game_id, uint16_t dungeon_id, const uint8_t *extra);
uint32_t save_header_game_id(const uint8_t *h);
uint16_t save_header_dungeon_id(const uint8_t *h);
const uint8_t *save_header_extra(const uint8_t *h);

#endif
