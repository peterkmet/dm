#ifndef LAYOUT_H
#define LAYOUT_H
/* Prefixes: layout_ */
#include <stdint.h>
#include "draw.h"

#define LAYOUT_BITMAPS 0x1025f2 /* g_layout_bitmaps[]: 14 bytes each, layout bitmap -1 - n */
/* Layout bitmap record (14 bytes): the source rectangle of the slot in its graphic (0: the bitmap size). */
enum { LAYOUT_BITMAP_SIZE = 14, LAYOUT_BITMAP_X = 2, LAYOUT_BITMAP_Y = 4, LAYOUT_BITMAP_W = 6, LAYOUT_BITMAP_H = 8 };
#define ZONE_SCALE_FULL 10000   /* layout_get_zone_box: the full zone size, in 1/10000 */

int layout_init(void);
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h);
int layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box);

#endif
