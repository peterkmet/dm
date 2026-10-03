#ifndef SAVEFILE_H
#define SAVEFILE_H
#include <stdint.h>

/* DM v3.60 save file container (DMGAME~.DAT). Layout: docs/save-format.md. */

#define SAVE_HEADER_BYTES 512
#define SAVE_BLOCKS 5
#define SAVE_PORTRAIT_BYTES 464 /* ((32 + 15) & ~15) / 2 * 29 */
#define SAVE_PORTRAITS_BYTES (4 * SAVE_PORTRAIT_BYTES)

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

#endif
