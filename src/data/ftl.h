/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_FTL_H
#define DM_DATA_FTL_H
/* Prefixes: ftl_ */

/* The FTL module loader of the Amiga engine files: lays a module out in memory at its base address. */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data; /* malloc'd memory image, loaded at base */
    uint32_t size, base, data_base, code_base;
} FtlImage;

/* Lays out an FTL module in memory at base: data, region, code. Returns 0 or -1. */
int ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img);
void ftl_free(FtlImage *img);

/* The bytes of an image at an address of the original, or NULL when they are not all inside the image. */
uint8_t *ftl_at(const FtlImage *image, uint32_t address, uint32_t bytes);

#endif
