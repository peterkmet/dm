#ifndef VIEW_H
#define VIEW_H
/* Prefixes: view_ */
#include <stdbool.h>
#include <stdint.h>
#include "draw.h"

/* Door square bit that map.h does not name: the door passage runs north-south. */
enum { DOOR_NORTH_SOUTH = 8 };
enum { VIEWPORT_MAX_W = 224, VIEWPORT_MAX_H = 136 }; /* the largest viewport bitmap */
enum { VIEW_DEPTH_COUNT = 4 };                       /* the view draws the party row and 3 rows ahead */

/* Clickable boxes of the dungeon view, in viewport coordinates (x = -1: empty): the object piles of
   view cells 0-3 (D0 back cells, D1C front cells), the alcove pile, and the front wall ornament or
   door button. */
enum { VIEW_CLICK_BOX_ALCOVE = 4, VIEW_CLICK_BOX_WALL = 5, VIEW_CLICK_BOX_COUNT = 6 };
enum { VIEW_PILE_OBJECT_MAX = 32 }; /* a pile records at most this many objects; the view drops the rest */
/* View element of the square ahead (g_square_ahead_element): a wall, a door seen from the front. */
enum { VIEW_ELEMENT_WALL = 0, VIEW_ELEMENT_DOOR_FRONT = 0x11 };

extern int16_t g_do_not_draw_fluxcages;
extern Box g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_COUNT];
extern uint16_t g_pile_top_objects[VIEW_CLICK_BOX_WALL];                   /* top object of each pile box */
extern uint16_t g_pile_objects[VIEW_CLICK_BOX_WALL][VIEW_PILE_OBJECT_MAX]; /* objects of each pile box in draw order */
extern int16_t g_pile_object_counts[VIEW_CLICK_BOX_WALL];
extern int16_t g_square_ahead_element;
extern int16_t g_facing_alcove, g_facing_fountain, g_facing_vi_altar;

void view_load_map(int16_t map);
/* The ornament that the view draws on the side of wall square (x, y) of the party map that faces
   direction face (NONE when the view has not loaded the party map). */
enum {
    VIEW_ORNAMENT_NONE,
    VIEW_ORNAMENT_OTHER,
    VIEW_ORNAMENT_TEXT,
    VIEW_ORNAMENT_ALCOVE,
    VIEW_ORNAMENT_FOUNTAIN,
    VIEW_ORNAMENT_VI_ALTAR
};
int16_t view_wall_ornament(int16_t x, int16_t y, int16_t face);
/* The wall ornament of ordinal (1-based, from a sensor) of the party map: its index in the wall ornament
   graphics of the game, or -1 (no ornament, or the view has not loaded the party map). */
int16_t view_wall_ornament_index(uint16_t ordinal);
/* The wall ornament that the view draws on the side of wall square (x, y) that faces direction face: its index
   in the wall ornament graphics of the game, or -1 (none, or the view has not loaded the party map). */
int16_t view_wall_face_ornament_index(int16_t x, int16_t y, int16_t face);
/* The floor ornament that the view draws on square (x, y) of the party map from depth 1: its index in the
   floor ornament graphics of the game, or -1 (none, or the view has not loaded the party map). */
int16_t view_floor_ornament(int16_t x, int16_t y);
/* 1 when the view draws a pressure pad on square (x, y) of the party map. */
bool view_floor_pressure_pad(int16_t x, int16_t y);
void view_draw(int16_t dir, int16_t x, int16_t y);
/* CSB: draws the magic map grid of a champion (Champion.magic_map) with its top left square at (px, py). */
void view_draw_magic_map(Bitmap *dst, const uint8_t *spells, int16_t px, int16_t py);
/* Sets g_dungeon_view_palette_index from the party light amount (magical light plus torches) on map m. */
void view_set_light(int16_t m, int16_t amount);
bool view_render(int16_t map, int16_t x, int16_t y, int16_t dir);
/* Smooth projectile flight: view_set_motion(step, steps) makes the next views show each projectile
   step/steps of the way from its place at the tick before to its place at the last tick; (0, 0) is a
   normal view. Returns 1 when a projectile of the last view changed its place. */
void view_set_motion(int16_t step, int16_t steps);
bool view_projectiles_moving(void);

#endif
