/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_BYTES_H
#define DM_DATA_BYTES_H
/* Prefixes: be16 be32 wr16 wr32 */

/* Big-endian access to the Amiga data: game files, save files, module images. */

#include <assert.h>
#include <stdint.h>

/* Bit widths of the 68000 operands. */
#define WORD_BITS 16 /* bits of a 68000 word */
#define BYTE_BITS 8

static_assert((-2 >> 1) == -1, "the port assumes an arithmetic right shift of a negative value");

static inline uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(p[0] << BYTE_BITS | p[1]);
}
static inline uint32_t be32(const uint8_t *p)
{
    return (uint32_t)be16(p) << WORD_BITS | be16(p + sizeof(uint16_t));
}
static inline void wr16(uint8_t *p, uint16_t word)
{
    p[0] = (uint8_t)(word >> BYTE_BITS);
    p[1] = (uint8_t)word;
}
static inline void wr32(uint8_t *p, uint32_t dword)
{
    wr16(p, (uint16_t)(dword >> WORD_BITS));
    wr16(p + sizeof(uint16_t), (uint16_t)dword);
}

/* Number of elements of an array, as an int. */
#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#endif
