#ifndef LAYOUT_H
#define LAYOUT_H
#include <stdint.h>
#include "draw.h"

int layout_init(void);
int layout_get_zone(int16_t zone, int16_t bitmap_w, int16_t bitmap_h, Box *box, int16_t *w, int16_t *h);

#endif
