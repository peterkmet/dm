#ifndef LAYOUT_H
#define LAYOUT_H
/* Prefixes: layout_ */
#include <stdint.h>
#include "draw.h"

#define ZONE_SCALE_FULL 10000 /* layout_get_zone_box: the full zone size, in 1/10000 */

int layout_init(void);
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h);
int layout_get_zone_box(int16_t zone, int16_t wp, int16_t hp, Box *box);

#endif
