#ifndef VIEW_H
#define VIEW_H
#include <stdint.h>
#include "draw.h"

/* Clickable boxes of the dungeon view, in viewport coordinates (x = -1: empty): the object piles of
   view cells 0-3 (D0 back cells, D1C front cells), the alcove pile, and the front wall ornament or
   door button. */
enum { VIEW_CLICK_BOX_ALCOVE = 4, VIEW_CLICK_BOX_WALL = 5, VIEW_CLICK_BOX_COUNT = 6 };
/* View element of the square ahead (g_square_ahead_element): a wall, a door seen from the front. */
enum { VIEW_ELEMENT_WALL = 0, VIEW_ELEMENT_DOOR_FRONT = 0x11 };

extern int16_t g_do_not_draw_fluxcages;
extern Box g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_COUNT];
extern uint16_t g_pile_top_objects[VIEW_CLICK_BOX_WALL];   /* top object of each pile box */
extern int16_t g_square_ahead_element;
extern int16_t g_facing_alcove, g_facing_fountain, g_facing_vi_altar;

void view_load_map(int16_t map);
void view_draw(int16_t dir, int16_t x, int16_t y);
/* Party light amount (magical light plus torches) for set_dungeon_view_palette. Default 99: bright. */
void view_set_light(int16_t amount);
int view_render(int16_t map, int16_t x, int16_t y, int16_t dir);
/* Smooth projectile flight: view_set_motion(step, steps) makes the next views show each projectile
   step/steps of the way from its place at the tick before to its place at the last tick; (0, 0) is a
   normal view. Returns 1 when a projectile of the last view changed its place. */
void view_set_motion(int16_t step, int16_t steps);
int view_projectiles_moving(void);

#endif
