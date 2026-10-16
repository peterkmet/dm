/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/view.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/magic.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/random.h"
#include "engine/record.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "ui/video.h"
#include "ui/view_internal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* View elements: the square types, and doors and stairs seen from the side or from the front. */
#define ELEMENT_DOOR_SIDE    0x10
#define ELEMENT_DOOR_FRONT   (VIEW_ELEMENT_DOOR_FRONT)
#define ELEMENT_STAIRS_SIDE  0x12
#define ELEMENT_STAIRS_FRONT 0x13

/* random ornament allowed on that side: the north face is the bit of the ornament below on the map */
#define WALL_ORNAMENT_FACE(direction) (WALL_ORNAMENT_BELOW >> (direction))

/* Random wall and floor ornaments: the hash picks one of this many values. */
#define ORNAMENT_RANDOM_MODULO    30
/* Seeds and multipliers of the pseudo-random ornament hash of get_random_ornament. */
#define ORNAMENT_SEED_ROW         32
#define ORNAMENT_SEED_SQUARE_BASE 2000
#define ORNAMENT_SEED_MAP_STEP    64
#define ORNAMENT_SEED_MAP_BASE    3000
#define ORNAMENT_HASH_SQUARE      31417u
#define ORNAMENT_HASH_MAP         11u
#define ORNAMENT_HASH_SHIFT       2

View g_view = {.map = -1};

bool g_do_not_draw_fluxcages;

const ViewResult *view_get_result(void)
{
    return &g_view.result;
}

static bool is_open_floor(uint8_t square)
{
    return rec_square_type(square) == SQUARE_CORRIDOR || rec_square_type(square) == SQUARE_PIT;
}

/* Square byte of the current map. Outside the map: a wall, with a random ornament allowed on the side that
   faces an open floor square at the map edge. */
static uint8_t get_square(int16_t x, int16_t y)
{
    bool in_x = x >= 0 && x < g_view.map_width;
    bool in_y = y >= 0 && y < g_view.map_height;
    if (in_x && in_y)
        return rec_square_byte(g_view.map, x, y);
    if (in_x) {
        if (y == -1 && is_open_floor(rec_square_byte(g_view.map, x, 0)))
            return WALL_ORNAMENT_FACE(DIRECTION_SOUTH);
        if (y == g_view.map_height && is_open_floor(rec_square_byte(g_view.map, x, g_view.map_height - 1)))
            return WALL_ORNAMENT_FACE(DIRECTION_NORTH);
    } else if (in_y) {
        if (x == -1 && is_open_floor(rec_square_byte(g_view.map, 0, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_EAST);
        if (x == g_view.map_width && is_open_floor(rec_square_byte(g_view.map, g_view.map_width - 1, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_WEST);
    }
    return SQUARE_WALL;
}

bool view_is_alcove(int16_t i)
{
    return i >= 0 && (i == g_view.alcoves[0] || i == g_view.alcoves[1] || i == g_view.alcoves[2]);
}

/* A pseudo-random ornament index (-1: none) from the square position. */
static int16_t get_random_ornament(bool allowed, int16_t count, int16_t x, int16_t y)
{
    if (!allowed)
        return -1;
    uint16_t square_seed = y + x * ORNAMENT_SEED_ROW + ORNAMENT_SEED_SQUARE_BASE;
    uint16_t map_seed =
        g_view.map_height + g_view.map_width + g_view.map * ORNAMENT_SEED_MAP_STEP + ORNAMENT_SEED_MAP_BASE;
    /* 16-bit products and sum as in the original */
    uint16_t hash = (uint16_t)(rec_dungeon_ornament_seed() + ((uint16_t)(square_seed * ORNAMENT_HASH_SQUARE) >> 1) +
                               (uint16_t)(map_seed * ORNAMENT_HASH_MAP)) >>
                    ORNAMENT_HASH_SHIFT;
    int16_t i = hash % ORNAMENT_RANDOM_MODULO;
    return i < count ? i : -1;
}

/* Things that set_square_aspect reads. */
#define SCAN_NONE      0
#define SCAN_FLOOR     1
#define SCAN_WALL      2
#define SCAN_FAKE_WALL 3

/* The random ornament of each side of wall square (x, y) seen in direction dir; no alcove on the walls
   around the map. */
static void set_random_wall_ornaments(Aspect *a, uint8_t s, int16_t dir, int16_t x, int16_t y)
{
    for (int k = 0; k < WALL_SIDES; k++) {
        int16_t face = NORMALIZE(dir + k);
        a->side[k] = get_random_ornament(s & WALL_ORNAMENT_FACE(face), rec_map_random_wall_ornaments(g_view.map), x,
                                         (face + 1) * (y + 1));
    }
    if (x < 0 || x >= g_view.map_width || y < 0 ||
        y >= g_view.map_height) /* no random alcove on the walls around the map */
        for (int k = 0; k < WALL_SIDES; k++)
            if (view_is_alcove(a->side[k]))
                a->side[k] = -1;
}

/* The ornaments that the doors, teleporters, texts, and sensors of a square show: the floor ornament of a
   floor sensor, the wall ornaments, the inscription, and the champion portrait. Returns the first thing after them. */
static uint16_t scan_ornament_things(Aspect *aspect, uint16_t thing, int16_t dir, int scan)
{
    for (; thing != THING_ENDOFLIST && THING_TYPE(thing) <= THING_SENSOR; thing = rec_thing_next(thing)) {
        if (scan == SCAN_FLOOR && THING_TYPE(thing) == THING_SENSOR)
            aspect->floor_ornament = rec_sensor_ornament(thing);
        if (scan != SCAN_WALL && scan != SCAN_FAKE_WALL)
            continue;
        int16_t *side = &aspect->side[NORMALIZE(THING_CELL(thing) - dir)];
        if (THING_TYPE(thing) == THING_TEXT) {
            if (rec_text_is_visible(thing)) {
                *side = g_view.inscription;
                aspect->text = thing;
            }
        } else {
            *side = rec_sensor_ornament(thing);
            if (rec_sensor_type(thing) == SENSOR_WALL_CHAMPION_PORTRAIT)
                aspect->portrait = rec_sensor_data(thing);
        }
    }
    return thing;
}

/* set_square_aspect without the footprints bit: the element, the ornaments, and the first drawn thing of
   square (x, y) seen in direction dir. */
static void get_square_aspect(Aspect *aspect, int16_t dir, int16_t x, int16_t y)
{
    memset(aspect, 0, sizeof *aspect);
    for (int k = 0; k < WALL_SIDES; k++) /* no ornaments; the door and stairs cases set their fields */
        aspect->side[k] = -1;
    aspect->floor_ornament = -1;
    aspect->portrait = -1;
    uint16_t thing = map_get_first_thing_at((Position){g_view.map, x, y});
    uint8_t square = get_square(x, y);
    int scan = SCAN_NONE;
    aspect->element = rec_square_type(square);
    aspect->square = square;
    switch (rec_square_type(square)) {
    case SQUARE_WALL:
        set_random_wall_ornaments(aspect, square, dir, x, y);
        scan = SCAN_WALL;
        break;
    case SQUARE_FAKE_WALL:
        if (!(square & FAKE_WALL_OPEN)) {
            aspect->element = SQUARE_WALL;
            scan = SCAN_FAKE_WALL;
            break;
        }
        aspect->element = SQUARE_CORRIDOR;
        square &= SQUARE_RANDOM_ORNAMENT;
        /* fall through */
        __attribute__((fallthrough));
    case SQUARE_CORRIDOR:
        aspect->floor_ornament =
            get_random_ornament(square & SQUARE_RANDOM_ORNAMENT, rec_map_random_floor_ornaments(g_view.map), x, y);
        scan = SCAN_FLOOR;
        break;
    case SQUARE_PIT:
        if (!(square & PIT_OPEN))
            aspect->element = SQUARE_CORRIDOR;
        else
            aspect->state = square & PIT_INVISIBLE;
        scan = SCAN_FLOOR;
        break;
    case SQUARE_STAIRS:
        aspect->element = (dir & 1) == !!(square & STAIRS_NORTH_SOUTH) ? ELEMENT_STAIRS_SIDE : ELEMENT_STAIRS_FRONT;
        aspect->state = square & STAIRS_UP;
        break;
    case SQUARE_DOOR:
        aspect->element = (dir & 1) == !!(square & DOOR_NORTH_SOUTH) ? ELEMENT_DOOR_SIDE : ELEMENT_DOOR_FRONT;
        aspect->state = rec_door_state(square);
        aspect->door = THING_INDEX(thing);
        break;
    case SQUARE_TELEPORTER:
        if (!(square & TELEPORTER_OPEN) || !(square & TELEPORTER_VISIBLE))
            aspect->element = SQUARE_CORRIDOR;
        scan = SCAN_FLOOR;
        break;
    }
    thing = scan_ornament_things(aspect, thing, dir, scan);
    /* a closed fake wall shows its things only when the party stands in its row or column */
    aspect->first_thing = scan == SCAN_FAKE_WALL && g_party_map_x != x && g_party_map_y != y ? THING_ENDOFLIST : thing;
}

/* The layout slot of a LAYOUT_* id (the ids count down from -1). */
static int16_t get_layout_slot(int16_t layout_id)
{
    return -1 - layout_id;
}

WallBitmap view_get_wall_bitmap(int16_t id)
{
    return id < 0 ? (WallBitmap){-1, get_layout_slot(id)} : (WallBitmap){id, -1};
}

void view_draw_wall_bitmap(WallBitmap bitmap, int16_t zone, int16_t mode)
{
    int16_t graphic = bitmap.graphic;
    int16_t flip = mode == BLIT_FLIPPED ? FLIP_H : 0;
    int16_t sx = 0;
    int16_t sy = 0;
    int16_t width = 0;
    int16_t height = 0;
    if (bitmap.slot >= 0) { /* the slot gives the graphic and the source rectangle */
        int16_t slot = bitmap.slot;
        Box slot_rect = tables_get_layout_bitmap(slot);
        graphic = g_view.slot_graphics[slot];
        flip ^= g_view.slot_flip[slot] ? FLIP_H : 0;
        sx = slot_rect.x;
        sy = slot_rect.y;
        width = slot_rect.w;
        height = slot_rect.h;
    }
    const Bitmap *bm = gfx_get_bitmap(graphic);
    Box b;
    if (bm && layout_get_zone(zone, bm->w, bm->h, &b, &width, &height))
        draw_blit(bm, width + sx, height + sy, &g_view.viewport, &b, mode == BLIT_OPAQUE ? -1 : TRANSPARENT_COLOR,
                  flip);
}

bool view_draw_in_zone(const Bitmap *bm, Bitmap *dst, const ZoneBlit *blit, Box *drawn)
{
    int16_t zone = blit->zone;
    int16_t w = 0;
    int16_t h = 0;
    Box b;
    if (!bm || zone == -1)
        return false;
    if (zone & (ZONE_SHIFTED | ZONE_SIZED)) {
        w = blit->place.zone_dx;
        h = blit->place.zone_dy;
        zone &= ~ZONE_SIZED;
    }
    if (!layout_get_zone(zone, bm->w, bm->h, &b, &w, &h))
        return false;
    if (drawn)
        *drawn = b;
    b.x += blit->place.shift_x;
    b.y += blit->place.shift_y;
    draw_blit(bm, w, h, dst, &b, blit->transparent, blit->flip);
    return true;
}

ScaledBitmap view_get_scaled(int16_t graphic, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes)
{
    ScaledBitmap s = {gfx_get_bitmap(graphic), {0}};
    if (!s.bitmap || (scale_x == SCALE_NATIVE && scale_y == SCALE_NATIVE && !palette_changes))
        return s;
    /* 16-bit sizes as in the original */
    draw_shrink(s.bitmap, (int16_t)((scale_x >> 1) + s.bitmap->w * scale_x) >> SCALE_SHIFT,
                (int16_t)((scale_y >> 1) + s.bitmap->h * scale_y) >> SCALE_SHIFT, palette_changes, &s.own);
    s.bitmap = NULL;
    return s;
}

const Bitmap *view_get_scaled_bitmap(const ScaledBitmap *scaled)
{
    return scaled->own.px ? &scaled->own : scaled->bitmap;
}

void view_free_scaled(ScaledBitmap *scaled)
{
    free(scaled->own.px);
    scaled->own.px = NULL;
}

/* The squares that the view draws, and the D4 squares with objects. */
#define DRAWN_SQUARE_COUNT 16
#define D4_SQUARE_COUNT    3

/* The 16 squares that the view draws, back to front. A field that the draw functions do not read for a row is -1.
   Id enums: WS_*: wall set layout ids; VIEW_SQUARE_*: view square table; VIEW_D*: floor ornament and door views;
   VIEW_WALL_*: wall ornament views; BUTTON_*: door buttons. */
static const SquareView square_views[DRAWN_SQUARE_COUNT] = {
    {.forward = 3,
     .right = -2,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D3L2,
     .view_square = VIEW_SQUARE_D3L2,
     .wall = {WS_WALL_D3L2, ZONE_WALL_D3L2, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_RIGHT,
     .side_ornament_view = VIEW_WALL_D3L2_RIGHT,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D3L2,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3L2,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = CELLS_BL_BR_FR,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = LAYOUT_STAIRS_UP_FRONT_D3L2,
     .stairs_front_down = LAYOUT_STAIRS_DOWN_FRONT_D3L2,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3L2,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .button = -1,
     .ceiling = -1,
     .ceiling_zone = -1,
     .flags = SQ_STAIRS_LAYOUT_IDS},
    {.forward = 3,
     .right = 2,
     .shape = SHAPE_FULL,
     .mirror = BLIT_FLIPPED,
     .view = VIEW_D3R2,
     .view_square = VIEW_SQUARE_D3R2,
     .wall = {WS_WALL_D3R2, ZONE_WALL_D3R2, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_LEFT,
     .side_ornament_view = VIEW_WALL_D3R2_LEFT,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D3L2,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3R2,
     .cells = CELLS_BR_BL_FR_FL,
     .side_cells = CELLS_BR_BL_FL,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = LAYOUT_STAIRS_UP_FRONT_D3R2,
     .stairs_front_down = LAYOUT_STAIRS_DOWN_FRONT_D3R2,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3R2,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BR_BL,
     .door_front = DOOR_FRONT_FR_FL,
     .button = -1,
     .ceiling = -1,
     .ceiling_zone = -1,
     .flags = SQ_STAIRS_LAYOUT_IDS},
    {.forward = 3,
     .right = -1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D3L,
     .view_square = VIEW_SQUARE_D3L,
     .wall = {WS_WALL_D3L, ZONE_WALL_D3L, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_RIGHT,
     .side_ornament_view = VIEW_WALL_D3L_RIGHT,
     .front_ornament_view = VIEW_WALL_D3L_FRONT,
     .pit = GFX_PIT_D3L,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3L,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = CELLS_BL_BR_FR,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D3L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3L,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_LEFT_D3L, ZONE_DOOR_FRAME_LEFT_D3L, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_RIGHT_D3L, ZONE_DOOR_FRAME_RIGHT_D3L, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = -1,
     .ceiling = -1,
     .ceiling_zone = -1},
    {.forward = 3,
     .right = 1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_FLIPPED,
     .view = VIEW_D3R,
     .view_square = VIEW_SQUARE_D3R,
     .wall = {WS_WALL_D3R, ZONE_WALL_D3R, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_LEFT,
     .side_ornament_view = VIEW_WALL_D3R_LEFT,
     .front_ornament_view = VIEW_WALL_D3R_FRONT,
     .pit = GFX_PIT_D3L,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3R,
     .cells = CELLS_BR_BL_FR_FL,
     .side_cells = CELLS_BR_BL_FL,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D3L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3R,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BR_BL,
     .door_front = DOOR_FRONT_FR_FL,
     .frames = {{WS_DOOR_FRAME_LEFT_D3R, ZONE_DOOR_FRAME_LEFT_D3R, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_LEFT_D3L, ZONE_DOOR_FRAME_RIGHT_D3R, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = BUTTON_D3R,
     .ceiling = -1,
     .ceiling_zone = -1},
    {.forward = 3,
     .right = 0,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D3C,
     .view_square = VIEW_SQUARE_D3C,
     .wall = {WS_WALL_D3C, ZONE_WALL_D3C, BLIT_OPAQUE},
     .side_face = SIDE_FACE_NONE,
     .side_ornament_view = -1,
     .front_ornament_view = VIEW_WALL_D3C_FRONT,
     .pit = GFX_PIT_D3C,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D3C,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3C,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_D3C, ZONE_DOOR_FRAME_LEFT_D3C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_D3C, ZONE_DOOR_FRAME_RIGHT_D3C, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = BUTTON_D3C,
     .ceiling = -1,
     .ceiling_zone = -1},
    {.forward = 2,
     .right = -2,
     .shape = SHAPE_WALL_OR_FIELD,
     .mirror = -1,
     .view = -1,
     .view_square = VIEW_SQUARE_D2L2,
     .wall = {WS_WALL_D2L2, ZONE_WALL_D2L2, BLIT_TRANSPARENT},
     .side_ornament_view = -1,
     .front_ornament_view = -1,
     .pit = -1,
     .invisible_pit = -1,
     .pit_zone = -1,
     .cells = -1,
     .side_cells = -1,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = -1,
     .stairs_front_down = -1,
     .stairs_front_zone = -1,
     .stairs_front_flipped_zone = -1,
     .door_back = -1,
     .door_front = -1,
     .button = -1,
     .ceiling = -1,
     .ceiling_zone = -1},
    {.forward = 2,
     .right = 2,
     .shape = SHAPE_WALL_OR_FIELD,
     .mirror = -1,
     .view = -1,
     .view_square = VIEW_SQUARE_D2R2,
     .wall = {WS_WALL_D2R2, ZONE_WALL_D2R2, BLIT_TRANSPARENT},
     .side_ornament_view = -1,
     .front_ornament_view = -1,
     .pit = -1,
     .invisible_pit = -1,
     .pit_zone = -1,
     .cells = -1,
     .side_cells = -1,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = -1,
     .stairs_front_down = -1,
     .stairs_front_zone = -1,
     .stairs_front_flipped_zone = -1,
     .door_back = -1,
     .door_front = -1,
     .button = -1,
     .ceiling = -1,
     .ceiling_zone = -1},
    {.forward = 2,
     .right = -1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D2L,
     .view_square = VIEW_SQUARE_D2L,
     .wall = {WS_WALL_D2L, ZONE_WALL_D2L, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_RIGHT,
     .side_ornament_view = VIEW_WALL_D2L_RIGHT,
     .front_ornament_view = VIEW_WALL_D2L_FRONT,
     .pit = GFX_PIT_D2L,
     .invisible_pit = GFX_INVISIBLE_PIT_D2L,
     .pit_zone = ZONE_PIT_D2L,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = CELLS_BR_FL_FR,
     .stairs_side = STAIRS_SIDE_D2L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D2L,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D2L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2L,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_TOP_D2L, ZONE_DOOR_FRAME_TOP_D2L, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D2L,
     .ceiling_zone = ZONE_CEILING_PIT_D2L,
     .flags = SQ_CEILING_IN_ALCOVE | SQ_CEILING_AFTER_DOOR},
    {.forward = 2,
     .right = 1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_FLIPPED,
     .view = VIEW_D2R,
     .view_square = VIEW_SQUARE_D2R,
     .wall = {WS_WALL_D2R, ZONE_WALL_D2R, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_LEFT,
     .side_ornament_view = VIEW_WALL_D2R_LEFT,
     .front_ornament_view = VIEW_WALL_D2R_FRONT,
     .pit = GFX_PIT_D2L,
     .invisible_pit = GFX_INVISIBLE_PIT_D2L,
     .pit_zone = ZONE_PIT_D2R,
     .cells = CELLS_BR_BL_FR_FL,
     .side_cells = CELLS_BL_FR_FL,
     .stairs_side = STAIRS_SIDE_D2L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D2R,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D2L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2R,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BR_BL,
     .door_front = DOOR_FRONT_FR_FL,
     .frames = {{WS_DOOR_FRAME_TOP_D2R, ZONE_DOOR_FRAME_TOP_D2R, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D2L,
     .ceiling_zone = ZONE_CEILING_PIT_D2R},
    {.forward = 2,
     .right = 0,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D2C,
     .view_square = VIEW_SQUARE_D2C,
     .wall = {WS_WALL_D2C, ZONE_WALL_D2C, BLIT_OPAQUE},
     .side_face = SIDE_FACE_NONE,
     .side_ornament_view = -1,
     .front_ornament_view = VIEW_WALL_D2C_FRONT,
     .pit = GFX_PIT_D2C,
     .invisible_pit = GFX_INVISIBLE_PIT_D2C,
     .pit_zone = ZONE_PIT_D2C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D2C,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2C,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_TOP_D2C, ZONE_DOOR_FRAME_TOP_D2C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_D2C, ZONE_DOOR_FRAME_LEFT_D2C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_D2C, ZONE_DOOR_FRAME_RIGHT_D2C, BLIT_FLIPPED}},
     .frame_count = 3,
     .button = BUTTON_D2C,
     .ceiling = GFX_CEILING_PIT_D2C,
     .ceiling_zone = ZONE_CEILING_PIT_D2C},
    {.forward = 1,
     .right = -1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D1L,
     .view_square = VIEW_SQUARE_D1L,
     .wall = {WS_WALL_D1L, ZONE_WALL_D1L, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_RIGHT,
     .side_ornament_view = VIEW_WALL_D1L_RIGHT,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D1L,
     .invisible_pit = GFX_INVISIBLE_PIT_D1L,
     .pit_zone = ZONE_PIT_D1L,
     .cells = CELLS_BR_FR,
     .side_cells = CELLS_BR_FR,
     .stairs_side = STAIRS_UP_SIDE_D1L,
     .stairs_side_zone = ZONE_STAIRS_UP_SIDE_D1L,
     .stairs_side_down = STAIRS_DOWN_SIDE_D1L,
     .stairs_side_down_zone = ZONE_STAIRS_DOWN_SIDE_D1L,
     .stairs_front = STAIRS_FRONT_D1L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1L,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BR,
     .door_front = DOOR_FRONT_FR,
     .frames = {{WS_DOOR_FRAME_TOP_D1L, ZONE_DOOR_FRAME_TOP_D1L, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D1L,
     .ceiling_zone = ZONE_CEILING_PIT_D1L},
    {.forward = 1,
     .right = 1,
     .shape = SHAPE_FULL,
     .mirror = BLIT_FLIPPED,
     .view = VIEW_D1R,
     .view_square = VIEW_SQUARE_D1R,
     .wall = {WS_WALL_D1R, ZONE_WALL_D1R, BLIT_TRANSPARENT},
     .side_face = SIDE_FACE_LEFT,
     .side_ornament_view = VIEW_WALL_D1R_LEFT,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D1L,
     .invisible_pit = GFX_INVISIBLE_PIT_D1L,
     .pit_zone = ZONE_PIT_D1R,
     .cells = CELLS_BL_FL,
     .side_cells = CELLS_BL_FL,
     .stairs_side = STAIRS_UP_SIDE_D1L,
     .stairs_side_zone = ZONE_STAIRS_UP_SIDE_D1R,
     .stairs_side_down = STAIRS_DOWN_SIDE_D1L,
     .stairs_side_down_zone = ZONE_STAIRS_DOWN_SIDE_D1R,
     .stairs_front = STAIRS_FRONT_D1L,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1R,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL,
     .door_front = DOOR_FRONT_FL,
     .frames = {{WS_DOOR_FRAME_TOP_D1R, ZONE_DOOR_FRAME_TOP_D1R, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D1L,
     .ceiling_zone = ZONE_CEILING_PIT_D1R},
    {.forward = 1,
     .right = 0,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D1C,
     .view_square = VIEW_SQUARE_D1C,
     .wall = {WS_WALL_D1C, ZONE_WALL_D1C, BLIT_OPAQUE},
     .side_face = SIDE_FACE_NONE,
     .side_ornament_view = -1,
     .front_ornament_view = VIEW_WALL_D1C_FRONT,
     .pit = GFX_PIT_D1C,
     .invisible_pit = GFX_INVISIBLE_PIT_D1C,
     .pit_zone = ZONE_PIT_D1C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D1C,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1C,
     .stairs_front_flipped_zone = -1,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_TOP_D1C, ZONE_DOOR_FRAME_TOP_D1C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_LEFT_D1C, ZONE_DOOR_FRAME_LEFT_D1C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_RIGHT_D1C, ZONE_DOOR_FRAME_RIGHT_D1C, BLIT_TRANSPARENT}},
     .frame_count = 3,
     .button = BUTTON_D1C,
     .ceiling = GFX_CEILING_PIT_D1C,
     .ceiling_zone = ZONE_CEILING_PIT_D1C,
     .flags = SQ_FRONT},
    {.forward = 0,
     .right = -1,
     .shape = SHAPE_D0_SIDE,
     .mirror = BLIT_TRANSPARENT,
     .view = -1,
     .view_square = VIEW_SQUARE_D0L,
     .wall = {WS_WALL_D0L, ZONE_WALL_D0L, BLIT_TRANSPARENT},
     .side_ornament_view = -1,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D0L,
     .invisible_pit = GFX_INVISIBLE_PIT_D0L,
     .pit_zone = ZONE_PIT_D0L,
     .cells = CELLS_BR,
     .side_cells = -1,
     .stairs_side = STAIRS_SIDE_D0L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D0L,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = -1,
     .stairs_front_down = -1,
     .stairs_front_zone = -1,
     .stairs_front_flipped_zone = -1,
     .door_back = -1,
     .door_front = -1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D0L,
     .ceiling_zone = ZONE_CEILING_PIT_D0L},
    {.forward = 0,
     .right = 1,
     .shape = SHAPE_D0_SIDE,
     .mirror = BLIT_FLIPPED,
     .view = -1,
     .view_square = VIEW_SQUARE_D0R,
     .wall = {WS_WALL_D0R, ZONE_WALL_D0R, BLIT_TRANSPARENT},
     .side_ornament_view = -1,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D0L,
     .invisible_pit = GFX_INVISIBLE_PIT_D0L,
     .pit_zone = ZONE_PIT_D0R,
     .cells = CELLS_BL,
     .side_cells = -1,
     .stairs_side = STAIRS_SIDE_D0L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D0R,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = -1,
     .stairs_front_down = -1,
     .stairs_front_zone = -1,
     .stairs_front_flipped_zone = -1,
     .door_back = -1,
     .door_front = -1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D0L,
     .ceiling_zone = ZONE_CEILING_PIT_D0R},
    {.forward = 0,
     .right = 0,
     .shape = SHAPE_D0C,
     .mirror = BLIT_TRANSPARENT,
     .view = -1,
     .view_square = VIEW_SQUARE_D0C,
     .wall = {.zone = ZONE_WALL_D0C},
     .side_ornament_view = -1,
     .front_ornament_view = -1,
     .pit = GFX_PIT_D0C,
     .invisible_pit = GFX_INVISIBLE_PIT_D0C,
     .pit_zone = ZONE_PIT_D0C,
     .cells = CELLS_BL_BR,
     .side_cells = -1,
     .stairs_side = -1,
     .stairs_side_zone = -1,
     .stairs_side_down = -1,
     .stairs_side_down_zone = -1,
     .stairs_front = STAIRS_FRONT_D0C,
     .stairs_front_down = -1,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D0C_LEFT,
     .stairs_front_flipped_zone = ZONE_STAIRS_UP_FRONT_D0C_RIGHT,
     .door_back = -1,
     .door_front = -1,
     .frames = {{WS_DOOR_FRAME_D0C, ZONE_DOOR_FRAME_D0C, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .button = -1,
     .ceiling = GFX_CEILING_PIT_D0C,
     .ceiling_zone = ZONE_CEILING_PIT_D0C},
};

/* A D0 side square: the wall or the stairs side, else the pit, the ceiling pit, and the things. */
static void show_d0_side_square(const SquareView *s, const Aspect *a, int16_t dir, Location at)
{
    switch (a->element) {
    case SQUARE_WALL:
        view_draw_wall_part(&s->wall);
        return;
    case ELEMENT_STAIRS_SIDE:
        view_draw_stairs_side(s, a->state);
        return;
    case SQUARE_PIT:
        view_draw_square_pit(s, a->state); /* fall through */
        __attribute__((fallthrough));
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        view_draw_ceiling_pit(s, at);
        view_draw_square_things(a->first_thing, dir, at, s->view_square, s->cells);
    }
    if (a->element == SQUARE_TELEPORTER)
        view_draw_teleporter_field(s->view_square, s->wall.zone);
}

/* The square of the party: a door frame, the pit, or the stairs on both sides, then the ceiling pit and the
   things. */
static void show_d0c_square(const SquareView *s, const Aspect *a, int16_t dir, Location at)
{
    if (a->element == ELEMENT_DOOR_SIDE)
        view_draw_wall_part(&s->frames[0]);
    else if (a->element == SQUARE_PIT)
        view_draw_square_pit(s, a->state);
    else if (a->element == ELEMENT_STAIRS_FRONT)
        view_draw_d0c_stairs(s, a->state);
    view_draw_ceiling_pit(s, at);
    view_draw_square_things(a->first_thing, dir, at, s->view_square, s->cells);
    if (a->element == SQUARE_TELEPORTER)
        view_draw_teleporter_field(s->view_square, s->wall.zone);
}

/* Draws one view square: its wall, pit, stairs, or door, then the floor ornament, the things, and a teleporter
   field. */
static void show_square(const SquareView *s, int16_t dir, Location at)
{
    Aspect a;
    get_square_aspect(&a, dir, at.x, at.y);
    switch (s->shape) {
    case SHAPE_WALL_OR_FIELD:
        if (a.element == SQUARE_WALL)
            view_draw_wall_part(&s->wall);
        else if (a.element == SQUARE_TELEPORTER)
            view_draw_teleporter_field(s->view_square, s->wall.zone);
        return;
    case SHAPE_D0_SIDE:
        show_d0_side_square(s, &a, dir, at);
        return;
    case SHAPE_D0C:
        show_d0c_square(s, &a, dir, at);
        return;
    }
    if (s->flags & SQ_FRONT)
        g_view.result.square_ahead_element = a.element;
    uint16_t cells = s->cells;
    switch (a.element) {
    case SQUARE_WALL:
        view_draw_square_wall(s, &a, dir, at);
        return;
    case SQUARE_PIT:
        view_draw_square_pit(s, a.state);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        if (!s->side_cells)
            return;
        if (a.element == ELEMENT_STAIRS_SIDE)
            view_draw_stairs_side(s, a.state);
        cells = s->side_cells;
        break;
    case ELEMENT_DOOR_FRONT:
        view_draw_door_front(s, &a, dir, at);
        return;
    case ELEMENT_STAIRS_FRONT:
        view_draw_stairs_front(s, a.state);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    view_draw_floor_ornament(a.floor_ornament, s->view);
    view_draw_ceiling_pit(s, at);
    view_draw_square_things(a.first_thing, dir, at, s->view_square, cells);
    if (a.element == SQUARE_TELEPORTER)
        view_draw_teleporter_field(s->view_square, s->wall.zone);
}

/* Color 9 or 10 of the 6 light palettes and of the creature
   palette changes from replacement set n. */
#define COLOR_CREATURE_1      9
#define COLOR_CREATURE_2      10
#define REPLACEMENT_DEFAULT_1 8
#define REPLACEMENT_DEFAULT_2 12
#define REPLACEMENT_SET_SHIFT 4

/* Creature aspect byte: the set of color 9 in the low nibble, of color 10 in the high nibble. */
#define REPLACEMENT_SET_MASK 0xf

/* Puts replacement color set n of a creature type into color of the dungeon palettes and the D2 and D3 changes. */
static void set_creature_replacement_colors(int16_t color, int16_t n)
{
    for (int l = 0; l < LIGHT_LEVELS; l++) /* a color per light level, then the D2 and D3 color */
        g_view.dungeon_palettes[l][color] = tables_get_replacement_color(n, l);
    g_view.creature_palette_changes_d2[color] = tables_get_replacement_change(n, DEPTH_D2);
    g_view.creature_palette_changes_d3[color] = tables_get_replacement_change(n, DEPTH_D3);
}

/* Sets every wall set entry to the layout bitmap of the game tables. */
static void reset_wall_set_ids(void)
{
    for (int i = 0; i < WS_COUNT; i++)
        g_view.wall_set[i] = view_get_wall_bitmap(tables_get_wall_layout_id(i));
}

/* The layout slot of a wall set entry. */
static int16_t get_wall_set_slot(int entry)
{
    return g_view.wall_set[entry].slot;
}

/* Gives the layout slot of a wall set entry its graphic, mirrored or not. */
static void set_slot(int entry, int16_t graphic, bool flip)
{
    g_view.slot_graphics[get_wall_set_slot(entry)] = graphic;
    g_view.slot_flip[get_wall_set_slot(entry)] = flip;
}

/* Loads the floor set and the wall set: the graphic of each layout bitmap slot, and the slots that hold a
   mirrored graphic. */
static void load_wall_set(int16_t floor_set, int16_t wall_set)
{
    /* The mirrored entries hold the mirror of these walls */
    static const uint8_t mirrored_walls[WALL_COUNT] = {
        WS_WALL_D0L, WS_WALL_D0R, WS_WALL_D1L,  WS_WALL_D1R,  WS_WALL_D1C, WS_WALL_D2L2, WS_WALL_D2R2, WS_WALL_D2L,
        WS_WALL_D2R, WS_WALL_D2C, WS_WALL_D3L2, WS_WALL_D3R2, WS_WALL_D3L, WS_WALL_D3R,  WS_WALL_D3C,
    };
    int16_t first = GFX_WALLSET_FIRST + GFX_WALLSET_SIZE * wall_set;
    reset_wall_set_ids();
    set_slot(WS_FLOOR, GFX_FLOOR_SET_FIRST + 2 * floor_set, false);
    set_slot(WS_CEILING, GFX_FLOOR_SET_FIRST + 2 * floor_set + 1, false);
    for (int i = 0; i < WALL_COUNT; i++)
        set_slot(WS_WALL_FIRST + i, first + WALLSET_WALL_FIRST + i, false);
    set_slot(WS_DOOR_FRAME_D0C, first + WALLSET_DOOR_FRAME_D0C, false);
    set_slot(WS_DOOR_FRAME_LEFT_D1C, first + WALLSET_DOOR_FRAME_D1C, false);
    set_slot(WS_DOOR_FRAME_D2C, first + WALLSET_DOOR_FRAME_D2C, false);
    set_slot(WS_DOOR_FRAME_D3C, first + WALLSET_DOOR_FRAME_D3C, false);
    set_slot(WS_DOOR_FRAME_LEFT_D3L, first + WALLSET_DOOR_FRAME_D3_SIDE, false);
    set_slot(WS_DOOR_FRAME_TOP_D1C, first + WALLSET_DOOR_FRAME_TOP_D1, false);
    set_slot(WS_DOOR_FRAME_TOP_D2C, first + WALLSET_DOOR_FRAME_TOP_D2, false);
    set_slot(WS_DOOR_FRAME_RIGHT_D1C, first + WALLSET_DOOR_FRAME_D1C, true);
    set_slot(WS_DOOR_FRAME_TOP_D2R, first + WALLSET_DOOR_FRAME_TOP_D2,
             false); /* the same bitmap as the D2C door frame top */
    set_slot(WS_DOOR_FRAME_TOP_D2L, first + WALLSET_DOOR_FRAME_TOP_D2, false);
    set_slot(WS_DOOR_FRAME_TOP_D1R, first + WALLSET_DOOR_FRAME_TOP_D1, false);
    set_slot(WS_DOOR_FRAME_TOP_D1L, first + WALLSET_DOOR_FRAME_TOP_D1, false);
    set_slot(WS_DOOR_FRAME_RIGHT_D3L, first + WALLSET_DOOR_FRAME_D3_SIDE, false);
    set_slot(WS_DOOR_FRAME_LEFT_D3R, first + WALLSET_DOOR_FRAME_D3_SIDE, false);
    g_view.slot_graphics[get_layout_slot(LAYOUT_INSCRIPTION_WALL)] =
        g_view.slot_graphics[get_wall_set_slot(WS_WALL_D1C)];
    for (int i = 0; i < WALL_COUNT; i++)
        set_slot(WS_MIRRORED_WALL_FIRST + i, g_view.slot_graphics[get_wall_set_slot(mirrored_walls[i])], true);
    g_view.stairs = first + WALLSET_STAIRS_FIRST;
    g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_UP_FRONT_D3R2)] = g_view.stairs + STAIRS_FRONT_D3L;
    g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_UP_FRONT_D3L2)] =
        g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_UP_FRONT_D3R2)];
    g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_DOWN_FRONT_D3R2)] =
        g_view.stairs + STAIRS_FRONT_D3L + STAIRS_DOWN;
    g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_DOWN_FRONT_D3L2)] =
        g_view.slot_graphics[get_layout_slot(LAYOUT_STAIRS_DOWN_FRONT_D3R2)];
}

/* The door graphics and door info of the two door types of map m. */
static void load_doors(int16_t m)
{
    for (int type = 0; type < DOOR_TYPES; type++) {
        int16_t set = rec_map_door_set(m, type);
        g_view.door_type_attributes[type] = tables_get_door_attributes(set);
        for (int depth = 0; depth < DOOR_DEPTHS; depth++)
            g_view.door_graphics[depth][type] = GFX_DOOR_FIRST + DOOR_DEPTHS * set + depth;
    }
}

/* The wall, floor, and door ornaments of map from its ornament list, and the special ornaments: the
   inscription plaque, the footprints, and the door masks. */
static void load_ornaments(int16_t map, const uint8_t *ornaments)
{
    int16_t walls = rec_map_wall_ornaments(map);
    int16_t floors = rec_map_floor_ornaments(map);
    int16_t doors = rec_map_door_ornaments(map);
    int16_t alcove_count = 0;
    g_view.alcoves[0] = -1;
    g_view.alcoves[1] = -1;
    g_view.alcoves[2] = -1;
    g_view.vi_altar = -1;
    g_view.fountain = -1;
    g_view.inscription = walls; /* after the map ornaments: the inscription plaque, ornament 0 */
    for (int16_t i = 0; i <= walls; i++) {
        uint8_t ornament = i < walls ? ornaments[i] : 0;
        g_view.wall_ornaments[i] = (Ornament){GFX_WALL_ORNAMENT_FIRST + WALL_ORNAMENT_GRAPHICS * ornament,
                                              tables_get_wall_ornament_coords(ornament)};
        for (int k = 0; k < ALCOVE_ORNAMENT_COUNT; k++)
            if (tables_get_alcove_ornament(k) == ornament) {
                g_view.alcoves[alcove_count++] = i;
                if (ornament == GFX_ORNAMENT_WALL_VI_ALTAR)
                    g_view.vi_altar = i;
            }
        if (tables_get_fountain_ornament() == ornament)
            g_view.fountain = i;
    }
    for (int16_t i = 0; i < floors; i++) {
        uint8_t ornament = ornaments[walls + i];
        g_view.floor_ornaments[i] = (Ornament){GFX_FLOOR_ORNAMENT_FIRST + FLOOR_ORNAMENT_GRAPHICS * ornament,
                                               tables_get_floor_ornament_coords(ornament)};
    }
    g_view.floor_ornaments[FLOOR_ORNAMENT_FOOTPRINTS] = (Ornament){GFX_FOOTPRINTS, 0};
    for (int16_t i = 0; i < doors; i++) {
        uint8_t ornament = ornaments[walls + floors + i];
        g_view.door_ornaments[i] =
            (Ornament){GFX_DOOR_ORNAMENT_FIRST + ornament, tables_get_door_ornament_coords(ornament)};
    }
    g_view.door_ornaments[DOOR_ORNAMENT_DESTROYED] = (Ornament){GFX_DOOR_MASK_DESTROYED, DOOR_MASK_COORD_SET};
    g_view.door_ornaments[DOOR_ORNAMENT_THIEVES_EYE] = (Ornament){GFX_DOOR_MASK_THIEVES_EYE, DOOR_MASK_COORD_SET};
}

/* The light palettes and the creature palette changes with the replacement colors of the creature types of
   the map. */
static void load_creature_colors(const uint8_t *creature_types, int16_t count)
{
    for (int l = 0; l < LIGHT_LEVELS; l++)
        tables_get_palette(TABLES_PALETTE_DUNGEON + l, g_view.dungeon_palettes[l]);
    memcpy(g_view.creature_palette_changes_d3, tables_get_palette_changes(TABLES_CHANGES_CREATURE_D3),
           sizeof g_view.creature_palette_changes_d3);
    memcpy(g_view.creature_palette_changes_d2, tables_get_palette_changes(TABLES_CHANGES_CREATURE_D2),
           sizeof g_view.creature_palette_changes_d2);
    set_creature_replacement_colors(COLOR_CREATURE_1, REPLACEMENT_DEFAULT_1);
    set_creature_replacement_colors(COLOR_CREATURE_2, REPLACEMENT_DEFAULT_2);
    for (int i = 0; i < count; i++) { /* the original indexes the aspects by creature type */
        uint8_t sets = tables_get_creature_aspect(creature_types[i])[CREATURE_ASPECT_REPLACEMENT];
        if (sets & REPLACEMENT_SET_MASK)
            set_creature_replacement_colors(COLOR_CREATURE_1, (sets & REPLACEMENT_SET_MASK) - 1);
        if (sets >> REPLACEMENT_SET_SHIFT)
            set_creature_replacement_colors(COLOR_CREATURE_2, (sets >> REPLACEMENT_SET_SHIFT) - 1);
    }
}

void view_load_map(int16_t m)
{
    g_view.map = m;
    g_view.map_width = dungeon_map_width(m);
    g_view.map_height = dungeon_map_height(m);
    const uint8_t *creature_types = rec_map_type_lists(m); /* after the squares: creature types, then ornaments */
    int16_t creature_type_count = rec_map_creature_types(m);
    load_wall_set(rec_map_floor_set(m), rec_map_wall_set(m));
    load_doors(m);
    load_ornaments(m, creature_types + creature_type_count);
    load_creature_colors(creature_types, creature_type_count);
}

int16_t view_get_wall_ornament(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (g_view.map != g_party_map_index)
        return VIEW_ORNAMENT_NONE;
    get_square_aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || a.side[SIDE_FAR] < 0)
        return VIEW_ORNAMENT_NONE;
    int16_t i = a.side[SIDE_FAR];
    if (i == g_view.inscription)
        return VIEW_ORNAMENT_TEXT;
    if (i == g_view.vi_altar)
        return VIEW_ORNAMENT_VI_ALTAR;
    if (view_is_alcove(i))
        return VIEW_ORNAMENT_ALCOVE;
    if (i == g_view.fountain)
        return VIEW_ORNAMENT_FOUNTAIN;
    return VIEW_ORNAMENT_OTHER;
}

int16_t view_get_wall_ornament_index(int16_t ornament)
{
    if (g_view.map != g_party_map_index || ornament < 0 || ornament >= rec_map_wall_ornaments(g_view.map))
        return -1;
    return (g_view.wall_ornaments[ornament].graphic - GFX_WALL_ORNAMENT_FIRST) / WALL_ORNAMENT_GRAPHICS;
}

int16_t view_get_face_ornament_index(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (g_view.map != g_party_map_index)
        return -1;
    get_square_aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || a.side[SIDE_FAR] < 0)
        return -1;
    return view_get_wall_ornament_index(a.side[SIDE_FAR]);
}

int16_t view_get_floor_ornament(int16_t x, int16_t y)
{
    Aspect a;
    if (g_view.map != g_party_map_index)
        return -1;
    get_square_aspect(&a, DIRECTION_NORTH, x, y);
    if (a.element != SQUARE_CORRIDOR && a.element != SQUARE_PIT && a.element != SQUARE_TELEPORTER)
        return -1;
    int16_t i = a.floor_ornament;
    return i < 0 ? -1 : (g_view.floor_ornaments[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS;
}

bool view_is_pressure_pad(int16_t x, int16_t y)
{
    int16_t ornament = view_get_floor_ornament(x, y);
    return ornament == GFX_ORNAMENT_FLOOR_PRESSURE_PAD || ornament == GFX_ORNAMENT_FLOOR_ROUND_PRESSURE_PAD ||
           ornament == GFX_ORNAMENT_FLOOR_TINY_PRESSURE_PAD;
}

void view_draw(int16_t dir, int16_t x, int16_t y)
{
    static const int8_t d4_right[D4_SQUARE_COUNT] = {-1, 1, 0};
    static const int16_t d4_squares[D4_SQUARE_COUNT] = {VIEW_SQUARE_D4L, VIEW_SQUARE_D4R, VIEW_SQUARE_D4C};
    static uint8_t px[VIEWPORT_MAX_H * VIEWPORT_MAX_W];
    g_view.viewport = (Bitmap){tables_get_viewport_width(), tables_get_viewport_height(), px};
    if (g_view.map < 0 || (size_t)g_view.viewport.w * g_view.viewport.h > sizeof px)
        return;
    memset(px, 0, sizeof px);
    view_start_tracks(dir, (Location){x, y});
    for (int i = 0; i < VIEW_CLICK_BOX_COUNT; i++)
        g_view.result.clickable_boxes[i].x = -1;
    memset(g_view.result.pile_object_counts, 0, sizeof g_view.result.pile_object_counts);
    reset_wall_set_ids();
    g_view.use_flipped_walls = (dir + x + y) & 1;
    if (g_view.use_flipped_walls) { /* the mirrored-walls memory option is on, as on a 1 MB Amiga */
        memcpy(&g_view.wall_set[WS_WALL_FIRST], &g_view.wall_set[WS_MIRRORED_WALL_FIRST],
               WALL_COUNT * sizeof g_view.wall_set[0]);
        view_draw_wall_bitmap(g_view.wall_set[WS_CEILING], ZONE_CEILING, BLIT_OPAQUE);
        view_draw_wall_bitmap(g_view.wall_set[WS_FLOOR], ZONE_FLOOR, BLIT_FLIPPED);
    } else {
        view_draw_wall_bitmap(g_view.wall_set[WS_CEILING], ZONE_CEILING, BLIT_FLIPPED);
        view_draw_wall_bitmap(g_view.wall_set[WS_FLOOR], ZONE_FLOOR, BLIT_OPAQUE);
    }
    for (int i = 0; i < D4_SQUARE_COUNT; i++) { /* objects only on D4L, D4R, D4C */
        int16_t sx = x;
        int16_t sy = y;
        map_step(dir, DEPTH_D4, d4_right[i], &sx, &sy);
        view_draw_square_things(map_get_first_thing_at((Position){g_view.map, sx, sy}), dir, (Location){sx, sy},
                                d4_squares[i], CELLS_BL);
    }
    for (int i = 0; i < DRAWN_SQUARE_COUNT; i++) {
        int16_t sx = x;
        int16_t sy = y;
        map_step(dir, square_views[i].forward, square_views[i].right, &sx, &sy);
        show_square(&square_views[i], dir, (Location){sx, sy});
    }
    Bitmap screen = video_get_screen();
    int16_t width = g_view.viewport.w;
    int16_t height = g_view.viewport.h;
    Box box;
    if (layout_get_zone(ZONE_VIEWPORT, g_view.viewport.w, g_view.viewport.h, &box, &width, &height))
        draw_blit(&g_view.viewport, width, height, &screen, &box, -1, 0);
}

/* CSB magic map: a grid of 7 x 7 squares, each a 7 x 7 pixel tile with a 1 pixel border. */
#define MAP_SQUARES                   7
#define MAP_HALF                      3
#define MAP_TILE_W                    7
#define MAP_TILE_H                    7
#define MAP_GAP_X                     1
#define MAP_GAP_Y                     1
#define MAP_TILES_PER_ROW             16
#define MAP_COLOR_PARTY               8
#define MAP_COLOR_FOOTPRINTS          4
/* Tiles of GFX_MAGIC_MAP_TILES. A tile with "+ cell" or "+ side" has 4 tiles, one per side relative to the map. */
#define MAP_TILE_WALL                 0
#define MAP_TILE_FAKE_WALL_BLINK      1
#define MAP_TILE_FLOOR                2
#define MAP_TILE_PIT                  3
#define MAP_TILE_PIT_INVISIBLE        4
#define MAP_TILE_PIT_BLINK            5
#define MAP_TILE_TELEPORTER_FIRST     6
#define MAP_TILE_DOOR_FRONT           10
#define MAP_TILE_DOOR_SIDE            11
#define MAP_TILE_DOOR_NO_BUTTON       2
#define MAP_TILE_DOOR_OPEN            4
#define MAP_TILE_STAIRS_FIRST         18
#define MAP_TILE_ALCOVE_FIRST         22
#define MAP_TILE_VI_ALTAR_FIRST       26
#define MAP_TILE_FOUNTAIN_FIRST       30
#define MAP_TILE_ZOKATHRA_FIRST       34
#define MAP_TILE_IN_WALL              4
#define MAP_TILE_LIGHTNING_FIRST      42
#define MAP_TILE_LIGHTNING_ACROSS     4
#define MAP_TILE_FIREBALL_FIRST       50
#define MAP_TILE_SPELL_FIRST          54
#define MAP_TILE_SLIME_FIRST          58
#define MAP_TILE_POISON_FIRST         62
#define MAP_TILE_EXPLOSION_FIRE       66
#define MAP_TILE_EXPLOSION_OTHER      67
#define MAP_TILE_POISON_CLOUD_FIRST   68
#define MAP_TILE_FLUXCAGE_FIRST       72
#define MAP_TILE_CREATURE_FIRST       76
#define MAP_TILE_FLOOR_ORNAMENT_FIRST 78
#define MAP_TIME_BLINK_PIT            1 /* game time bits of the blinking tiles */
#define MAP_TIME_BLINK_WALL           2

/* Draws one map tile at (px, py). */
static void show_map_tile(Bitmap *dst, int16_t tile, int16_t px, int16_t py)
{
    const Bitmap *tiles = gfx_get_bitmap(GFX_MAGIC_MAP_TILES);
    Box b = {px, py, MAP_TILE_W, MAP_TILE_H};
    if (tiles)
        draw_blit(tiles, tile % MAP_TILES_PER_ROW * MAP_TILE_W, tile / MAP_TILES_PER_ROW * MAP_TILE_H, dst, &b,
                  TRANSPARENT_COLOR, 0);
}

/* Footprints of the footprints spell on a floor square (the footprints bit of set_square_aspect). */
static bool has_map_footprints(const Aspect *a, int16_t x, int16_t y)
{
    bool allowed = a->element == SQUARE_PIT ? a->square & PIT_IMAGINARY
                                            : a->element != SQUARE_WALL && a->element != ELEMENT_STAIRS_SIDE &&
                                                  a->element != ELEMENT_STAIRS_FRONT;
    int16_t scent = allowed ? group_get_party_scent(x, y) : -1;
    return scent >= 0 && scent >= g_party.first_scent_index && scent < g_party.last_scent_index;
}

/* The tile of a flying thing, or -1: spell projectiles and a flying zokathra. */
static int16_t get_map_projectile_tile(uint16_t projectile, int16_t dir)
{
    uint16_t slot = rec_projectile_slot(projectile);
    ProjectileAspect flying = view_get_projectile_aspect(slot);
    switch (flying.projectile ? flying.index : -1) {
    case PROJECTILE_ASPECT_FIREBALL:
        return MAP_TILE_FIREBALL_FIRST;
    case PROJECTILE_ASPECT_SPELL:
        return MAP_TILE_SPELL_FIRST;
    case PROJECTILE_ASPECT_POISON:
        return MAP_TILE_POISON_FIRST;
    case PROJECTILE_ASPECT_SLIME:
        return MAP_TILE_SLIME_FIRST;
    case PROJECTILE_ASPECT_LIGHTNING: {
        return MAP_TILE_LIGHTNING_FIRST +
               ((projectile_get_flight_direction(projectile) & 1) != (dir & 1) ? MAP_TILE_LIGHTNING_ACROSS : 0);
    }
    }
    return !flying.projectile && object_get_type(slot) == OBJECT_TYPE_ZOKATHRA ? MAP_TILE_ZOKATHRA_FIRST : -1;
}

/* The tile of an explosion, or -1: smoke and rebirth do not show. */
static int16_t get_map_explosion_tile(uint16_t explosion)
{
    switch (rec_explosion_type(explosion)) {
    case EXPLOSION_FIREBALL:
    case EXPLOSION_LIGHTNING_BOLT:
        return MAP_TILE_EXPLOSION_FIRE;
    case EXPLOSION_POISON_BOLT:
    case EXPLOSION_POISON_CLOUD:
        return MAP_TILE_POISON_CLOUD_FIRST + random_2bits();
    case EXPLOSION_FLUXCAGE:
        return MAP_TILE_FLUXCAGE_FIRST + random_2bits();
    }
    return rec_explosion_type(explosion) < EXPLOSION_SMOKE ? MAP_TILE_EXPLOSION_OTHER : -1;
}

/* One magic map square draw. */
typedef struct {
    Bitmap *dst;
    const uint8_t *spells;
    bool blink;
    int16_t dir;
    int16_t px, py;
} MapDraw;

/* The things of a square that the spells show at (m->px, m->py): with DAIN the zokathra objects, the projectiles,
   and the explosions, with KU the creatures. m->blink: the square is a fake wall that blinks. */
static void show_map_square_things(const MapDraw *m, const Aspect *aspect)
{
    Bitmap *dst = m->dst;
    const uint8_t *spells = m->spells;
    int16_t dir = m->dir;
    int16_t px = m->px;
    int16_t py = m->py;
    uint16_t explosion = THING_ENDOFLIST;
    uint16_t projectile = THING_ENDOFLIST;
    uint16_t group = THING_ENDOFLIST;
    for (uint16_t thing = aspect->first_thing; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        int16_t type = THING_TYPE(thing);
        if (type == THING_GROUP && spells[MAGIC_MAP_KU])
            group = thing;
        else if (!spells[MAGIC_MAP_DAIN])
            continue;
        else if (type == THING_PROJECTILE && projectile == THING_ENDOFLIST)
            projectile = thing;
        else if (type == THING_EXPLOSION && explosion == THING_ENDOFLIST)
            explosion = thing;
        else if (type == THING_JUNK && object_get_type(thing) == OBJECT_TYPE_ZOKATHRA)
            show_map_tile(dst,
                          MAP_TILE_ZOKATHRA_FIRST + NORMALIZE(THING_CELL(thing) - dir) +
                              (aspect->element == SQUARE_WALL && !m->blink ? MAP_TILE_IN_WALL : 0),
                          px, py);
    }
    if (group != THING_ENDOFLIST && !(map_get_creature_attributes(group) & CREATURE_NOT_ON_MAP))
        show_map_tile(dst, MAP_TILE_CREATURE_FIRST + (g_game_time & 1), px, py);
    for (uint16_t thing = projectile; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        int16_t tile = THING_TYPE(thing) == THING_PROJECTILE ? get_map_projectile_tile(thing, dir) : -1;
        if (tile >= 0)
            show_map_tile(dst, tile + NORMALIZE(THING_CELL(thing) - dir), px, py);
    }
    for (uint16_t thing = explosion; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        int16_t tile = THING_TYPE(thing) == THING_EXPLOSION ? get_map_explosion_tile(thing) : -1;
        if (tile >= 0)
            show_map_tile(dst, tile, px, py);
    }
}

/* A wall on the magic map: blinking when ROS shows a fake wall, else the wall with its vi altar, alcove, and
   fountain sides. */
static void show_map_wall(const MapDraw *m, const Aspect *aspect)
{
    Bitmap *dst = m->dst;
    int16_t px = m->px;
    int16_t py = m->py;
    if (m->spells[MAGIC_MAP_ROS] && m->blink && (g_game_time & MAP_TIME_BLINK_WALL)) {
        show_map_tile(dst, MAP_TILE_FAKE_WALL_BLINK, px, py);
        return;
    }
    show_map_tile(dst, MAP_TILE_WALL, px, py);
    for (int16_t k = 0; k < WALL_SIDES; k++) {
        int16_t i = aspect->side[k];
        int16_t first = i < 0                  ? -1
                        : i == g_view.vi_altar ? MAP_TILE_VI_ALTAR_FIRST
                        : view_is_alcove(i)    ? MAP_TILE_ALCOVE_FIRST
                        : i == g_view.fountain ? MAP_TILE_FOUNTAIN_FIRST
                                               : -1;
        if (first >= 0)
            show_map_tile(dst, first + k, px, py);
    }
}

/* The magic map tile of a door: front or side, with or without a button, open or closed. */
static int16_t get_map_door_tile(const Aspect *aspect)
{
    int16_t tile = aspect->element == ELEMENT_DOOR_SIDE ? MAP_TILE_DOOR_SIDE : MAP_TILE_DOOR_FRONT;
    if (!rec_door_has_button(THING_MAKE(THING_DOOR, aspect->door)))
        tile += MAP_TILE_DOOR_NO_BUTTON;
    if (aspect->state == DOOR_OPEN || aspect->state == DOOR_DESTROYED)
        tile += MAP_TILE_DOOR_OPEN;
    return tile;
}

/* The tiles of square at seen in direction m->dir at (m->px, m->py): the square, then with DAIN the footprints, the
   zokathra objects, the projectiles, and the explosions, with ROS the fake walls and invisible pits, with KU the
   creatures. */
static void show_map_square(const MapDraw *m, Location at)
{
    Bitmap *dst = m->dst;
    const uint8_t *spells = m->spells;
    int16_t dir = m->dir;
    int16_t px = m->px;
    int16_t py = m->py;
    Aspect aspect;
    get_square_aspect(&aspect, dir, at.x, at.y);
    MapDraw square = *m;
    square.blink = rec_square_type(aspect.square) == SQUARE_FAKE_WALL && (aspect.square & FAKE_WALL_IMAGINARY);
    int16_t color = at.x == g_party_map_x && at.y == g_party_map_y ? MAP_COLOR_PARTY
                    : spells[MAGIC_MAP_DAIN] && aspect.element != SQUARE_WALL && has_map_footprints(&aspect, at.x, at.y)
                        ? MAP_COLOR_FOOTPRINTS
                        : -1;
    if (color >= 0) {
        Box b = {px - MAP_GAP_X, py - MAP_GAP_Y, MAP_TILE_W + 2 * MAP_GAP_X, MAP_TILE_H + 2 * MAP_GAP_Y};
        draw_fill(dst, &b, (uint8_t)color);
    }
    switch (aspect.element) {
    case SQUARE_WALL:
        show_map_wall(&square, &aspect);
        break;
    case SQUARE_CORRIDOR: {
        show_map_tile(dst, MAP_TILE_FLOOR, px, py);
        int16_t i = aspect.floor_ornament;
        if (i >= 0)
            show_map_tile(dst,
                          MAP_TILE_FLOOR_ORNAMENT_FIRST +
                              (g_view.floor_ornaments[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS,
                          px, py);
        break;
    }
    case SQUARE_TELEPORTER:
        show_map_tile(dst, MAP_TILE_TELEPORTER_FIRST + random_2bits(), px, py);
        break;
    case ELEMENT_DOOR_FRONT:
    case ELEMENT_DOOR_SIDE:
        show_map_tile(dst, get_map_door_tile(&aspect), px, py);
        break;
    case ELEMENT_STAIRS_SIDE:
    case ELEMENT_STAIRS_FRONT: {
        int16_t side = NORMALIZE(map_get_stairs_exit_direction(at.x, at.y) - dir);
        if (aspect.state)
            side = direction_back(side);
        show_map_tile(dst, MAP_TILE_STAIRS_FIRST + side, px, py);
        break;
    }
    case SQUARE_PIT:
        show_map_tile(dst,
                      !aspect.state                                                 ? MAP_TILE_PIT
                      : spells[MAGIC_MAP_ROS] && (g_game_time & MAP_TIME_BLINK_PIT) ? MAP_TILE_PIT_BLINK
                                                                                    : MAP_TILE_PIT_INVISIBLE,
                      px, py);
        break;
    }
    if (spells[MAGIC_MAP_KU] || spells[MAGIC_MAP_DAIN])
        show_map_square_things(&square, &aspect);
}

void view_draw_magic_map(Bitmap *dst, const uint8_t *spells, int16_t px, int16_t py)
{
    int16_t map = g_party_map_index;
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    int16_t dir = g_party_direction;
    int16_t saved_map = g_current_map_index;
    int16_t party_px = -1;
    int16_t party_py = 0;
    if (g_view.map != g_party_map_index)
        view_load_map(g_party_map_index);
    int16_t saved[3] = {g_view.map, g_view.map_width, g_view.map_height};
    if (spells[MAGIC_MAP_NETA]) {
        Position kept = magic_get_map_position(spells);
        map = kept.map;
        x = kept.x;
        y = kept.y;
        dir = spells[MAGIC_MAP_NETA] - 1;
    }
    map_set_current_map(map);
    g_view.map = map;
    g_view.map_width = dungeon_map_width(map);
    g_view.map_height = dungeon_map_height(map);
    map_step(dir, MAP_HALF, -MAP_HALF, &x, &y);
    for (int16_t row = 0; row < MAP_SQUARES; row++, py += MAP_TILE_H + MAP_GAP_Y) {
        int16_t sx = x;
        int16_t sy = y;
        int16_t cx = px;
        for (int16_t col = 0; col < MAP_SQUARES; col++, cx += MAP_TILE_W + MAP_GAP_X) {
            if (map == g_party_map_index && sx == g_party_map_x && sy == g_party_map_y) {
                party_px = cx;
                party_py = py;
            } else {
                show_map_square(&(MapDraw){dst, spells, false, dir, cx, py}, (Location){sx, sy});
            }
            map_step(dir, 0, 1, &sx, &sy);
        }
        map_step(dir, -1, 0, &x, &y);
    }
    if (party_px >= 0)
        show_map_square(&(MapDraw){dst, spells, false, dir, party_px, party_py},
                        (Location){g_party_map_x, g_party_map_y});
    g_view.map = saved[0];
    g_view.map_width = saved[1];
    g_view.map_height = saved[2];
    if (saved_map >= 0)
        map_set_current_map(saved_map);
}

bool view_render(int16_t m, int16_t x, int16_t y, int16_t dir)
{
    if (!g_dungeon_maps || m < 0 || m >= dungeon_map_count())
        return false;
    if (m != g_view.map)
        view_load_map(m);
    int16_t light = g_session.cheat_light ? 0 : g_party_light_level;
    memcpy(g_screen.palette, g_view.dungeon_palettes[light], sizeof g_view.dungeon_palettes[light]);
    view_draw(NORMALIZE(dir), x, y);
    return true;
}
