/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kaos.h"
#include "adventure.h"
#include "bytes.h"
#include "csb_image.h"
#include "file.h"
#include "ftl.h"
#include "kaos_addresses.h"
#include "os.h"
#include <inttypes.h>
#include <stdlib.h>

static FtlImage image; /* the engine image; image.data is owned */

/* Setup */

bool kaos_load(const char *path)
{
    size_t len;
    uint8_t *raw = file_read(path, &len);
    FtlImage appb = {0};
    bool csb = g_adventure == ADVENTURE_CSB;
    kaos_free();
    bool ok = raw && ftl_load(raw, len, KAOS_BASE, csb ? &appb : &image);
    free(raw);
    if (ok && csb)
        ok = csb_image_build(&appb, &image);
    ftl_free(&appb);
    return ok;
}
void kaos_free(void)
{
    ftl_free(&image);
}

/* Queries */

bool kaos_is_loaded(void)
{
    return image.data != NULL;
}

/* The bytes of the image at an address of the original; ends the program outside the image. */
static uint8_t *get_bytes(uint32_t address, uint32_t bytes)
{
    uint8_t *p = ftl_at(&image, address, bytes);
    if (!p)
        os_fatal("kaos address %#" PRIx32 " outside the image", address);
    return p;
}
const uint8_t *kaos_ptr(uint32_t address)
{
    return get_bytes(address, 1);
}
uint8_t kaos_u8(uint32_t address)
{
    return *get_bytes(address, 1);
}
int8_t kaos_s8(uint32_t address)
{
    return (int8_t)*get_bytes(address, 1); /* sign of the byte */
}
uint16_t kaos_u16(uint32_t address)
{
    return be16(get_bytes(address, sizeof(uint16_t)));
}
int16_t kaos_s16(uint32_t address)
{
    return (int16_t)kaos_u16(address); /* sign of the word */
}
uint32_t kaos_u32(uint32_t address)
{
    return be32(get_bytes(address, sizeof(uint32_t)));
}

/* Commands */

void kaos_write_u16(uint32_t address, uint16_t value)
{
    wr16(get_bytes(address, sizeof(uint16_t)), value);
}
