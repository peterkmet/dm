/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_CSBTEXT_H
#define DM_DATA_CSBTEXT_H
/* Prefixes: csb_text_ */
#include "data/ftl.h"

/* The CSB texts as Graphics.DAT text items: CSB keeps them in the APPB data, not in items 699 and 700. */

/* Builds the strings in DM item 700 order and the action names (item 699) from the APPB module, and
   fills the string index tables of the KAOS-layout image out. Returns 0 or -1. */
int csb_text_build(const FtlImage *appb, FtlImage *out);
/* The built text of graphic GFX_STRINGS or GFX_ACTION_NAMES as NUL-separated strings, else NULL. */
const uint8_t *csb_text_get_item(int graphic, uint16_t *size);

#endif
