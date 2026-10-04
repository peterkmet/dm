#ifndef CSBIMAGE_H
#define CSBIMAGE_H
#include "ftl.h"

/* The CSB engine (APPB.FTL) as a KAOS-layout image: each table that the port reads sits at its DM address. */

#define CSB_SPELLS 0x104160 /* all CSB spells, at the KAOS data base: the port reads no other address from here on */
#define CSB_SPELL_COUNT 29

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
