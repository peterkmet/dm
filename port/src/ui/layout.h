#ifndef LAYOUT_H
#define LAYOUT_H
/* Prefixes: layout_ */
#include <stdint.h>
#include "draw.h"

#define LAYOUT_BITMAPS 0x1025f2 /* g_layout_bitmaps[]: 14 bytes each, layout bitmap -1 - n */

int layout_init(void);
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h);
int layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box);

#endif
