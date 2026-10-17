/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_CSB_IMAGE_H
#define DM_DATA_CSB_IMAGE_H
/* Prefixes: csb_image_ */

/* The CSB engine (APPB.FTL) as a KAOS-layout image: each table that the port reads sits at its DM address. */

#include "data/ftl.h"
#include <stdbool.h>

/* Addresses of the CSB tables in the KAOS-layout image, after the DM tables. */
#define CSB_SPELLS        0x104160 /* all CSB spells, at the KAOS data base: from here on only CSB tables follow */
#define CSB_RESTART_BOXES 0x104248 /* after the spells: outer and inner box of the CSB RESTART THIS GAME button */
#define CSB_IMAGE_END     (CSB_RESTART_BOXES + 2 * RESTART_BOX_BYTES)

/* The CSB spell count and the size of a restart box. */
#define SPELL_COUNT_CSB   29
#define RESTART_BOX_BYTES 8 /* x, y, width, height words */

/* One table copied from its APPB address to its KAOS address. */
typedef struct {
    uint32_t kaos, appb;
    uint16_t size;
} CsbCopy;

extern const CsbCopy g_csb_copies[]; /* exported for the tests */
extern const int g_csb_copy_count;   /* exported for the tests */

/* Builds the KAOS-layout image of the loaded APPB module: copies the tables, converts the action sets, moves the
   dialog pointers to the KAOS tables, and builds the texts. False when the module does not fit. */
bool csb_image_build(const FtlImage *appb, FtlImage *out);

#endif
