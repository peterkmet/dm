/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_KAOS_H
#define DM_DATA_KAOS_H
/* Prefixes: kaos_ */

/* The loaded engine image (KAOS.FTL, or the KAOS-layout image of the CSB APPB.FTL) and its readers at the
   addresses of the original. Only data/ reads it; the other layers call tables_get_*. */

#include <stdbool.h>
#include <stdint.h>

/* Setup */
/* Loads the engine module at path: KAOS.FTL, or for CSB APPB.FTL as a KAOS-layout image. Returns 0 or -1. */
int kaos_load(const char *path);
/* Frees the image. */
void kaos_free(void);

/* Queries */
/* True after a successful kaos_load. */
bool kaos_is_loaded(void);
/* The image byte at an address of the original; a port bug outside the image. */
const uint8_t *kaos_ptr(uint32_t address);
/* The unsigned byte at an address of the original. */
uint8_t kaos_u8(uint32_t address);
/* The signed byte at an address of the original. */
int8_t kaos_s8(uint32_t address);
/* The big-endian unsigned word at an address of the original. */
uint16_t kaos_u16(uint32_t address);
/* The big-endian signed word at an address of the original. */
int16_t kaos_s16(uint32_t address);
/* The big-endian long at an address of the original. */
uint32_t kaos_u32(uint32_t address);

/* Commands */
/* Writes a big-endian word into the image: the mouse input boxes of the endgame screen. */
void kaos_write_u16(uint32_t address, uint16_t value);

#endif
