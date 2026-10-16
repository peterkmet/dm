/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_SAVEFILE_H
#define DM_ENGINE_SAVEFILE_H
/* Prefixes: save_ */
#include <stdbool.h>
#include <stdint.h>

/* DM v3.60 save file container (DMGAME~.DAT); CSB (CSBGAME.DAT) with g_adventure. Layout: docs/formats/save.md,
   CSB header: docs/csb/data.md "Save file". */

#define SAVE_HEADER_BYTES                    512
#define SAVE_BLOCKS                          5
#define SAVE_BLOCK_GLOBALS                   0
#define SAVE_BLOCK_GROUPS                    1
#define SAVE_BLOCK_CHAMPIONS                 2
#define SAVE_BLOCK_EVENTS                    3
#define SAVE_BLOCK_TIMELINE                  4
#define SAVE_GLOBALS_BYTES                   128
/* Block 0: byte offsets of the globals. */
#define G_GAME_TIME                          0
#define G_RANDOM_SEED                        4
#define G_LEADER_HAND_OBJECT                 8
#define G_PARTY_CHAMPION_COUNT               10
#define G_PARTY_MAP_X                        12
#define G_PARTY_MAP_Y                        14
#define G_PARTY_DIRECTION                    16
#define G_PARTY_MAP_INDEX                    18
#define G_LEADER_INDEX                       20
#define G_MAGIC_CASTER                       22
#define G_TIMELINE_COUNT                     24
#define G_FIRST_UNUSED_EVENT                 26
#define G_EVENT_MAX_COUNT                    28
#define G_ACTIVE_GROUP_COUNT                 30
#define G_LAST_CREATURE_ATTACK_TIME          32
#define G_LAST_PARTY_MOVEMENT_TIME           36
#define G_DISABLED_MOVEMENT_TICKS            40
#define G_PROJECTILE_DISABLED_MOVEMENT_TICKS 42
#define G_LAST_PROJECTILE_DIRECTION          44
#define G_MAX_ACTIVE_GROUP_COUNT             46
#define SAVE_CHAMPIONS_BYTES                 1408 /* 4 champion records and 128 bytes of the party */
#define SAVE_PARTY_BYTES                     128
#define SAVE_ACTIVE_GROUP_BYTES              16
#define SAVE_EVENT_BYTES                     10
#define SAVE_PAYLOAD_WORDS                   128
#define SAVE_PORTRAIT_BYTES                  464 /* ((32 + 15) & ~15) / 2 * 29 */
#define SAVE_PORTRAITS_BYTES                 (4 * SAVE_PORTRAIT_BYTES)
#define SAVE_HEADER_EXTRA_BYTES              134 /* extra header bytes, copied unchanged; CSB has 132 */

/* All buffers keep the big-endian byte layout of the original memory. */
typedef struct {
    uint8_t *data;               /* save_decode: owned plain copy of the file */
    uint8_t *header;             /* 512 bytes; the payload at byte 256 is plain */
    uint8_t *block[SAVE_BLOCKS]; /* plain blocks: globals, groups, champions, events, timeline */
    uint16_t block_size[SAVE_BLOCKS];
    uint8_t *portraits; /* 4 portraits, not encrypted */
    uint8_t *dungeon;   /* uncompressed dungeon parts and byte-sum word */
    uint32_t dungeon_size;
} SaveFile;

uint16_t save_block_xor(uint8_t *bytes, uint16_t key, uint16_t word_count);
bool save_decode(const uint8_t *file, uint32_t size, SaveFile *s);
uint8_t *save_encode(const SaveFile *s, uint32_t *size);
void save_free(SaveFile *s);

/* Plain header fields. save_header_init fills a new header with random16 words and keys. */
void save_header_init(uint8_t *h, uint32_t game_id, uint16_t dungeon_id, const uint8_t *extra);
/* New random block keys and chain words; the payload fields stay. */
void save_header_randomize(uint8_t *h);
/* The header of a new adventure made from a saved game with ID game_id (CEDT merge), new keys included. */
void save_header_new_adventure(uint8_t *h, uint32_t game_id);
uint32_t save_header_game_id(const uint8_t *h);
uint16_t save_header_dungeon_id(const uint8_t *h);
const uint8_t *save_header_extra(const uint8_t *h);
/* The extra bytes of g_adventure: 134 (DM) or 132 (CSB). */
uint16_t save_header_extra_size(void);

#endif
