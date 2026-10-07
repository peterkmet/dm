#ifndef CSBIMAGE_H
#define CSBIMAGE_H
/* Prefixes: csb_image_ */
#include "ftl.h"

/* The CSB engine (APPB.FTL) as a KAOS-layout image: each table that the port reads sits at its DM address. */

#define CSB_SPELLS 0x104160 /* all CSB spells, at the KAOS data base: from here on only CSB tables follow */
#define CSB_SPELL_COUNT 29
#define CSB_RESTART_BOXES 0x104248 /* after the spells: outer and inner box of the CSB RESTART THIS GAME button */
#define CSB_BOX_BYTES 8            /* x, y, width, height words */
#define CSB_IMAGE_END (CSB_RESTART_BOXES + 2 * CSB_BOX_BYTES)

/* One table copied from its APPB address to its KAOS address. */
typedef struct {
    uint32_t kaos, appb;
    uint16_t size;
} CsbCopy;

extern const CsbCopy csb_copies[];
extern const int csb_copy_count;

/* Builds the KAOS-layout image of the loaded APPB module. Returns 0 or -1. */
int csb_image_build(const FtlImage *appb, FtlImage *out);

#endif
