/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_FTL_H
#define DM_DATA_FTL_H
/* Prefixes: ftl_ kaos_ */
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data; /* malloc'd memory image, loaded at base */
    uint32_t size, base, data_base, code_base;
} FtlImage;

/* Lays out an FTL module in memory at base like BJELoad_R: data, region, code. Returns 0 or -1. */
int ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img);
void ftl_free(FtlImage *img);

extern FtlImage kaos;
/* Loads the engine module at path: KAOS.FTL, or for CSB APPB.FTL as a KAOS-layout image. Returns 0 or -1. */
int kaos_load(const char *path);
uint8_t *kaos_ptr(uint32_t addr);
uint8_t kaos_u8(uint32_t addr);
uint16_t kaos_u16(uint32_t addr);
int16_t kaos_s16(uint32_t addr);
uint32_t kaos_u32(uint32_t addr);

#endif
