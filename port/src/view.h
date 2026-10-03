#ifndef VIEW_H
#define VIEW_H
#include <stdint.h>

void view_load_map(int16_t map);
void view_draw(int16_t dir, int16_t x, int16_t y);
int view_render(int16_t map, int16_t x, int16_t y, int16_t dir);

#endif
