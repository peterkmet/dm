#ifndef DM_BYTES_H
#define DM_BYTES_H

#include <stdint.h>

/* Big-endian access to the Amiga data: game files, save files, module images. */
#define BE16(p)    ((uint16_t)((p)[0] << 8 | (p)[1]))
#define WR16(p, v) ((p)[0] = (uint8_t)((uint16_t)(v) >> 8), (p)[1] = (uint8_t)(v))
#define BE32(p)    ((uint32_t)BE16(p) << 16 | BE16((p) + 2))
#define WR32(p, v) (WR16(p, (uint32_t)(v) >> 16), WR16((p) + 2, v))
#define WORD_BITS  16 /* bits of a 68000 word */

#endif
