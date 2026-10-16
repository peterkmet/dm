/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/view.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
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
#include "engine/recruit.h"
#include "ui/advanced.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "ui/video.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* tables_get_pile_shift pair indices */
#define PILE_FLOOR_SHIFTS 16
#define PILE_ODD_CELL     8
#define PILE_ALCOVE_FIRST 2
#define PILE_ALCOVE_LAST  13

#define LAYOUT_SLOTS 64

#define WALL_COUNT 15
#define WALL_SIDES 4 /* the sides of a wall square */

/* Entries of tables_get_wall_layout_id: layout bitmap ids of the floor set, the walls, the door frames, and the
   mirrored walls. The right square of a pair draws the mirrored frame of the left square. */
#define WS_FLOOR                0
#define WS_CEILING              1
#define WS_WALL_D0R             2
#define WS_WALL_D0L             3
#define WS_WALL_D1R             4
#define WS_WALL_D1L             5
#define WS_WALL_D1C             6
#define WS_WALL_D2R2            7
#define WS_WALL_D2L2            8
#define WS_WALL_D2R             9
#define WS_WALL_D2L             10
#define WS_WALL_D2C             11
#define WS_WALL_D3R2            12
#define WS_WALL_D3L2            13
#define WS_WALL_D3R             14
#define WS_WALL_D3L             15
#define WS_WALL_D3C             16
#define WS_DOOR_FRAME_TOP_D1R   17
#define WS_DOOR_FRAME_TOP_D1L   18
#define WS_DOOR_FRAME_TOP_D1C   19
#define WS_DOOR_FRAME_TOP_D2R   20
#define WS_DOOR_FRAME_TOP_D2L   21
#define WS_DOOR_FRAME_TOP_D2C   22
#define WS_DOOR_FRAME_D0C       23
#define WS_DOOR_FRAME_RIGHT_D1C 24
#define WS_DOOR_FRAME_LEFT_D1C  25
#define WS_DOOR_FRAME_D2C       26
#define WS_DOOR_FRAME_D3C       27
#define WS_DOOR_FRAME_LEFT_D3L  28
#define WS_DOOR_FRAME_RIGHT_D3L 29
#define WS_DOOR_FRAME_LEFT_D3R  30
#define WS_MIRRORED_WALL_FIRST  31
#define WS_COUNT                (WS_MIRRORED_WALL_FIRST + WALL_COUNT)
#define WS_WALL_FIRST           (WS_WALL_D0R)

/* Graphics of a wall set, from GFX_WALLSET_FIRST + GFX_WALLSET_SIZE * wall set. */
#define WALLSET_DOOR_FRAME_D0C     0
#define WALLSET_DOOR_FRAME_D1C     1
#define WALLSET_DOOR_FRAME_D2C     2
#define WALLSET_DOOR_FRAME_D3C     3
#define WALLSET_DOOR_FRAME_D3_SIDE 4
#define WALLSET_DOOR_FRAME_TOP_D1  5
#define WALLSET_DOOR_FRAME_TOP_D2  6
#define WALLSET_WALL_FIRST         7
#define WALLSET_STAIRS_FIRST       (WALLSET_WALL_FIRST + WALL_COUNT)

/* Stairs graphics, from WALLSET_STAIRS_FIRST. Right squares mirror the left graphic. */
#define STAIRS_FRONT_D3L     0
#define STAIRS_FRONT_D3C     1
#define STAIRS_FRONT_D2L     2
#define STAIRS_FRONT_D2C     3
#define STAIRS_FRONT_D1L     4
#define STAIRS_FRONT_D1C     5
#define STAIRS_FRONT_D0C     6
#define STAIRS_DOWN          7 /* down stairs front graphic = up stairs front graphic + STAIRS_DOWN */
#define STAIRS_SIDE_D2L      14
#define STAIRS_UP_SIDE_D1L   15
#define STAIRS_DOWN_SIDE_D1L 16
#define STAIRS_SIDE_D0L      17

#define BLIT_TRANSPARENT          0 /* draw_wall_set_bitmap variants */
#define BLIT_OPAQUE               1
#define BLIT_FLIPPED              2
#define TRANSPARENT_COLOR         10
#define DOOR_ORNAMENT_TRANSPARENT 9
#define PORTRAIT_TRANSPARENT      1

/* View elements: the square types, and doors and stairs seen from the side or from the front. */
#define ELEMENT_DOOR_SIDE    0x10
#define ELEMENT_DOOR_FRONT   (VIEW_ELEMENT_DOOR_FRONT)
#define ELEMENT_STAIRS_SIDE  0x12
#define ELEMENT_STAIRS_FRONT 0x13

/* random ornament allowed on that side: the north face is the bit of the ornament below on the map */
#define WALL_ORNAMENT_FACE(direction) (WALL_ORNAMENT_BELOW >> (direction))
#define ORNAMENT_RANDOM_MODULO        30
/* Seeds and multipliers of the pseudo-random ornament hash of get_random_ornament. */
#define ORNAMENT_SEED_ROW             32
#define ORNAMENT_SEED_SQUARE_BASE     2000
#define ORNAMENT_SEED_MAP_STEP        64
#define ORNAMENT_SEED_MAP_BASE        3000
#define ORNAMENT_HASH_SQUARE          31417u
#define ORNAMENT_HASH_MAP             11u
#define ORNAMENT_HASH_SHIFT           2

/* View squares: index of the g_view_square_* tables. */
#define VIEW_SQUARE_D0C  0
#define VIEW_SQUARE_D0L  1
#define VIEW_SQUARE_D0R  2
#define VIEW_SQUARE_D1C  3
#define VIEW_SQUARE_D1L  4
#define VIEW_SQUARE_D1R  5
#define VIEW_SQUARE_D2C  6
#define VIEW_SQUARE_D2L  7
#define VIEW_SQUARE_D2R  8
#define VIEW_SQUARE_D2L2 9
#define VIEW_SQUARE_D2R2 10
#define VIEW_SQUARE_D3C  11
#define VIEW_SQUARE_D3L  12
#define VIEW_SQUARE_D3R  13
#define VIEW_SQUARE_D3L2 14
#define VIEW_SQUARE_D3R2 15
#define VIEW_SQUARE_D4C  16
#define VIEW_SQUARE_D4L  17
#define VIEW_SQUARE_D4R  18

/* Floor views of the floor ornaments, and door views of the door zones. */
#define VIEW_D3L2 0
#define VIEW_D3R2 1
#define VIEW_D3L  2
#define VIEW_D3C  3
#define VIEW_D3R  4
#define VIEW_D2L  5
#define VIEW_D2C  6
#define VIEW_D2R  7
#define VIEW_D1L  8
#define VIEW_D1C  9
#define VIEW_D1R  10

/* View walls of draw_wall_ornament: the visible side of each wall square. */
#define VIEW_WALL_D3L2_RIGHT 0
#define VIEW_WALL_D3R2_LEFT  1
#define VIEW_WALL_D3L_RIGHT  2
#define VIEW_WALL_D3R_LEFT   3
#define VIEW_WALL_D3L_FRONT  4
#define VIEW_WALL_D3C_FRONT  5
#define VIEW_WALL_D3R_FRONT  6
#define VIEW_WALL_D2L_RIGHT  7
#define VIEW_WALL_D2R_LEFT   8
#define VIEW_WALL_D2L_FRONT  9
#define VIEW_WALL_D2C_FRONT  10
#define VIEW_WALL_D2R_FRONT  11
#define VIEW_WALL_D1L_RIGHT  12
#define VIEW_WALL_D1R_LEFT   13
#define VIEW_WALL_D1C_FRONT  14

#define BUTTON_D3R            0 /* door button views */
#define BUTTON_D3C            1
#define BUTTON_D2C            2
#define BUTTON_D1C            3
#define DOOR_DEPTH_D3         0
#define DOOR_DEPTH_D2         1
#define DOOR_DEPTH_D1         2
#define DOOR_DEPTHS           3
#define DOOR_TYPES            2
#define ALCOVE_ORNAMENT_COUNT 3     /* wall ornaments that are alcoves */
#define DOOR_ANIMATED         0x400 /* door info attribute: the door mirrors at random on each draw */

/* Ornament indices; -1 for none. */
#define MAP_ORNAMENTS             16
#define FLOOR_ORNAMENT_FOOTPRINTS 15
#define DOOR_ORNAMENT_DESTROYED   15
#define DOOR_ORNAMENT_THIEVES_EYE 16
#define DOOR_ORNAMENTS            17
#define WALL_ORNAMENT_GRAPHICS    2
#define WALL_ORNAMENT_FRONT       1
#define FLOOR_ORNAMENT_GRAPHICS   6
#define DOOR_MASK_COORD_SET       1
#define WALL_TEXT_NEW_LINE        0x80 /* dungeon_decode_text mode 0 codes */
#define WALL_TEXT_END             0x81
#define WALL_TEXT_SIZE            80
#define SCALE_D3_FAR_SIDE_W       30 /* the D3L2 and D3R2 wall sides are wider than high */

/* Cell orders of draw_objects_creatures_projectiles_explosions: a nibble per view cell + 1, low nibble first;
   0 is the alcove. Bit 3 marks a door pass, bit 0 then selects the pass behind (0) or in front of the door (1),
   and the next nibbles hold the cells. B back, F front, L left, R right. */
#define CELLS_ALCOVE      0
#define CELLS_BL          0x1
#define CELLS_BR          0x2
#define CELLS_BL_BR       0x21
#define CELLS_BL_FL       0x41
#define CELLS_BR_FR       0x32
#define CELLS_BL_BR_FR    0x321
#define CELLS_BR_BL_FL    0x412
#define CELLS_BR_FL_FR    0x342
#define CELLS_BL_FR_FL    0x431
#define CELLS_BL_BR_FL_FR 0x3421
#define CELLS_BR_BL_FR_FL 0x4312
#define DOOR_BACK_BL      0x18
#define DOOR_BACK_BR      0x28
#define DOOR_BACK_BL_BR   0x218
#define DOOR_BACK_BR_BL   0x128
#define DOOR_FRONT_FL     0x49
#define DOOR_FRONT_FR     0x39
#define DOOR_FRONT_FL_FR  0x349
#define DOOR_FRONT_FR_FL  0x439

#define LIGHT_LEVELS 6

/* Sides of Aspect.side, clockwise from the far side. */
#define SIDE_FAR   0
#define SIDE_RIGHT 1
#define SIDE_FRONT 2
#define SIDE_LEFT  3

/* What the view draws of a square. */
typedef struct {
    uint16_t element;         /* square type, or a door or stairs element */
    uint16_t square;          /* square byte */
    uint16_t first_thing;     /* first thing after the doors, teleporters, texts, and sensors */
    int16_t side[WALL_SIDES]; /* wall: ornament index of each side, clockwise from the far side; -1 none */
    uint16_t state;           /* door state, pit invisible, stairs up */
    uint16_t door;            /* the door: index of its door thing */
    int16_t floor_ornament;   /* floor ornament index, -1 none */
    uint16_t text;            /* the text thing of an inscription on the wall */
    int16_t portrait;         /* a champion portrait on the wall: portrait index, or -1 */
} Aspect;

/* An ornament graphic and its coordinate set. */
typedef struct {
    int16_t graphic, coord; /* first graphic, coordinate set */
} Ornament;

/* The state of the dungeon view: the loaded map, its graphics, and the palettes. */
typedef struct {
    Bitmap viewport;                     /* the dungeon view bitmap */
    int16_t map, map_width, map_height;  /* the loaded map, -1 none, and its size */
    int16_t wall_set_ids[WS_COUNT];      /* layout ids of the wall set, swapped for mirrored views */
    int16_t slot_graphics[LAYOUT_SLOTS]; /* graphic of each layout bitmap slot */
    bool slot_flip[LAYOUT_SLOTS];        /* the slot holds the mirrored graphic */
    Ornament wall_ornaments[MAP_ORNAMENTS];
    Ornament floor_ornaments[MAP_ORNAMENTS];
    Ornament door_ornaments[DOOR_ORNAMENTS];
    int16_t alcoves[ALCOVE_ORNAMENT_COUNT];
    int16_t inscription, stairs;
    int16_t door_graphics[DOOR_DEPTHS][DOOR_TYPES];
    uint16_t door_type_attributes[DOOR_TYPES];
    bool use_flipped_walls; /* the view draws the mirrored wall set */
    /* the dungeon palettes with the creature replacement colors */
    uint16_t dungeon_palettes[LIGHT_LEVELS][PALETTE_COLORS];
    uint8_t creature_palette_changes_d3[PALETTE_COLORS]; /* the D3 and D2 creature palette changes of the map */
    uint8_t creature_palette_changes_d2[PALETTE_COLORS];
    int16_t vi_altar, fountain; /* wall ornament index of the vi altar and the fountain, or -1 */
} View;

static View view = {.map = -1};

/* Where blit_in_zone puts a bitmap: the relative position that a ZONE_SHIFTED or ZONE_SIZED zone passes to
   layout_get_zone, and the shift of a projectile between two ticks. */
typedef struct {
    int16_t zone_dx, zone_dy;
    int16_t shift_x, shift_y;
} Placement;

/* One bitmap draw into a zone of the viewport. */
typedef struct {
    int16_t zone;
    int16_t flip;        /* FLIP_H, FLIP_V, both, or 0 */
    int16_t transparent; /* transparent color, or COLOR_NONE */
    Placement place;
} ZoneBlit;

/* Smooth projectile flight (advanced feature, no rule change): the box of each projectile at the
   last tick and at the tick before. Between ticks the view is drawn with each projectile part of the
   way from the old box to the new box. */
#define PROJECTILE_TRACK_COUNT 32
typedef struct {
    uint16_t thing;
    Box from, to;
} ProjectileTrack;
static struct {
    ProjectileTrack tracks[PROJECTILE_TRACK_COUNT], last_tracks[PROJECTILE_TRACK_COUNT];
    int track_count, last_track_count;
    int32_t track_view;                /* map, position, and direction of the tracks */
    int16_t motion_step, motion_steps; /* the frame between two ticks; 0 steps: a tick view */
} flight = {.track_view = -1};
bool g_do_not_draw_fluxcages;
Box g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_COUNT];
uint16_t g_pile_top_objects[VIEW_CLICK_BOX_WALL];
uint16_t g_pile_objects[VIEW_CLICK_BOX_WALL][VIEW_PILE_OBJECT_MAX];
int16_t g_pile_object_counts[VIEW_CLICK_BOX_WALL];
int16_t g_square_ahead_element;
bool g_facing_alcove, g_facing_fountain, g_facing_vi_altar;

static bool is_open_floor(uint8_t square)
{
    return rec_square_type(square) == SQUARE_CORRIDOR || rec_square_type(square) == SQUARE_PIT;
}

/* Square byte of the current map. Outside the map: a wall, with a random ornament allowed on the side that
   faces an open floor square at the map edge. */
static uint8_t get_square(int16_t x, int16_t y)
{
    bool in_x = x >= 0 && x < view.map_width;
    bool in_y = y >= 0 && y < view.map_height;
    if (in_x && in_y)
        return rec_square_byte(view.map, x, y);
    if (in_x) {
        if (y == -1 && is_open_floor(rec_square_byte(view.map, x, 0)))
            return WALL_ORNAMENT_FACE(DIRECTION_SOUTH);
        if (y == view.map_height && is_open_floor(rec_square_byte(view.map, x, view.map_height - 1)))
            return WALL_ORNAMENT_FACE(DIRECTION_NORTH);
    } else if (in_y) {
        if (x == -1 && is_open_floor(rec_square_byte(view.map, 0, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_EAST);
        if (x == view.map_width && is_open_floor(rec_square_byte(view.map, view.map_width - 1, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_WEST);
    }
    return SQUARE_WALL;
}

static bool is_alcove(int16_t i)
{
    return i >= 0 && (i == view.alcoves[0] || i == view.alcoves[1] || i == view.alcoves[2]);
}

/* A pseudo-random ornament index (-1: none) from the square position. */
static int16_t get_random_ornament(bool allowed, int16_t count, int16_t x, int16_t y)
{
    if (!allowed)
        return -1;
    uint16_t square_seed = y + x * ORNAMENT_SEED_ROW + ORNAMENT_SEED_SQUARE_BASE;
    uint16_t map_seed = view.map_height + view.map_width + view.map * ORNAMENT_SEED_MAP_STEP + ORNAMENT_SEED_MAP_BASE;
    /* 16-bit products and sum as in the original */
    uint16_t hash = (uint16_t)(rec_dungeon_ornament_seed() + ((uint16_t)(square_seed * ORNAMENT_HASH_SQUARE) >> 1) +
                               (uint16_t)(map_seed * ORNAMENT_HASH_MAP)) >>
                    ORNAMENT_HASH_SHIFT;
    int16_t i = hash % ORNAMENT_RANDOM_MODULO;
    return i < count ? i : -1;
}

#define SCAN_NONE      0 /* things that set_square_aspect reads */
#define SCAN_FLOOR     1
#define SCAN_WALL      2
#define SCAN_FAKE_WALL 3

/* The random ornament of each side of wall square (x, y) seen in direction dir; no alcove on the walls
   around the map. */
static void set_random_wall_ornaments(Aspect *a, uint8_t s, int16_t dir, int16_t x, int16_t y)
{
    for (int k = 0; k < WALL_SIDES; k++) {
        int16_t face = NORMALIZE(dir + k);
        a->side[k] = get_random_ornament(s & WALL_ORNAMENT_FACE(face), rec_map_random_wall_ornaments(view.map), x,
                                         (face + 1) * (y + 1));
    }
    if (x < 0 || x >= view.map_width || y < 0 ||
        y >= view.map_height) /* no random alcove on the walls around the map */
        for (int k = 0; k < WALL_SIDES; k++)
            if (is_alcove(a->side[k]))
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
                *side = view.inscription;
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
    uint16_t thing = map_get_first_thing_at((Position){view.map, x, y});
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
            get_random_ornament(square & SQUARE_RANDOM_ORNAMENT, rec_map_random_floor_ornaments(view.map), x, y);
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

/* draw_wall_set_bitmap and its _opaque and _flipped variants: a graphic (bitmap_id >= 0) or a layout bitmap
   slot (bitmap_id < 0) at zone. */
static void draw_wall_bitmap(int16_t bitmap_id, int16_t zone, int mode)
{
    int16_t graphic = bitmap_id;
    int16_t flip = mode == BLIT_FLIPPED ? FLIP_H : 0;
    int16_t sx = 0;
    int16_t sy = 0;
    int16_t width = 0;
    int16_t height = 0;
    if (bitmap_id < 0) { /* the slot gives the graphic and the source rectangle */
        int16_t slot = -1 - bitmap_id;
        Box slot_rect = tables_get_layout_bitmap(slot);
        graphic = view.slot_graphics[slot];
        flip ^= view.slot_flip[slot] ? FLIP_H : 0;
        sx = slot_rect.x;
        sy = slot_rect.y;
        width = slot_rect.w;
        height = slot_rect.h;
    }
    const Bitmap *bm = gfx_get_bitmap(graphic);
    Box b;
    if (bm && layout_get_zone(zone, bm->w, bm->h, &b, &width, &height))
        draw_blit(bm, width + sx, height + sy, &view.viewport, &b, mode == BLIT_OPAQUE ? -1 : TRANSPARENT_COLOR, flip);
}

/* Draws bm into zone of dst. Returns true when it drew; drawn (NULL: not wanted) gets the drawn box.
   ZONE_SHIFTED or ZONE_SIZED pass the zone relative position to layout_get_zone: a shift, or the item size. */
static bool blit_in_zone(const Bitmap *bm, Bitmap *dst, const ZoneBlit *blit, Box *drawn)
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

#define VIEW_CELL_BACK_LEFT     0
#define VIEW_CELL_BACK_RIGHT    1
#define VIEW_CELL_FRONT_RIGHT   2
#define VIEW_CELL_FRONT_LEFT    3
#define VIEW_CELL_CENTER        4
#define FACING_AWAY             0 /* creature direction relative to the party */
#define FACING_LEFT             1
#define FACING_TOWARD           2
#define FACING_RIGHT            3
#define DEPTH_D0                0
#define DEPTH_D1                1
#define DEPTH_D2                2
#define DEPTH_D3                3
#define DEPTH_D4                4
/* scales are in 1/32 */
#define SCALE_D3                14
#define SCALE_D2                21
#define SCALE_NATIVE            32
#define SCALE_EXPLOSION_MIN     4
#define SCALE_SHIFT             5
#define DOOR_PASS_BACK          1 /* door squares draw things behind, then in front of the door */
#define DOOR_PASS_FRONT         2
#define CELL_ORDER_DOOR_PASS    8
#define CELL_ORDER_BITS         4
#define CELL_ORDER_MASK         0xf
#define CLICK_BOX_MIN_W         14 /* smallest clickable object box */
#define CLICK_BOX_MIN_H         11
#define CLICK_BOX_CENTER_H      15
#define CLICK_BOX_CENTER_OFFSET 7

/* The aspect offsets of the active group (group.h) index the shift sets: x, then y. */

/* Creature aspect record (6 bytes): graphic word, coordinate set and transparent color, replacement color sets. */
#define CREATURE_ASPECT_GRAPHIC     0
#define CREATURE_ASPECT_COORD       4
#define CREATURE_ASPECT_REPLACEMENT 5
#define CREATURE_ASPECT_TRANSPARENT 0xf
#define CREATURE_ASPECT_COORD_SHIFT 4

/* Object aspect record (4 bytes): graphic, flags, alcove coordinate set. */
#define OBJECT_ASPECT_GRAPHIC      0
#define OBJECT_ASPECT_FLAGS        2
#define OBJECT_ASPECT_ALCOVE_COORD 3
#define OBJECT_FLIP_ON_RIGHT       0x1
#define OBJECT_HAS_ALCOVE_GRAPHIC  0x10

/* Projectile aspect record (4 bytes): graphic, scale flags, pose flags. */
#define PROJECTILE_ASPECT_GRAPHIC      0
#define PROJECTILE_ASPECT_SCALE_FLAGS  2
#define PROJECTILE_ASPECT_POSE_FLAGS   3
#define PROJECTILE_SCALE_WITH_ENERGY   0x1
#define PROJECTILE_FLIP_ON_LEFT        0x10
#define PROJECTILE_KIND                0x3
#define PROJECTILE_BACK_AND_ROTATION   0
#define PROJECTILE_BACK                1
#define PROJECTILE_NO_BACK             2
#define PROJECTILE_NO_BACK_NO_ROTATION 3
#define POSE_FRONT                     0 /* graphic offset from the projectile aspect graphic */
#define POSE_BACK                      1
#define POSE_SIDE                      2
#define PROJECTILE_ASPECT_LIGHTNING    4
#define PROJECTILE_ASPECT_FIREBALL     11
#define PROJECTILE_ASPECT_SPELL        12
#define PROJECTILE_ASPECT_SLIME        13
#define PROJECTILE_ASPECT_POISON       14
#define WEAPON_PROJECTILE_ASPECT_SHIFT 8 /* 0: the object aspect */
#define WEAPON_PROJECTILE_ASPECT_MASK  0x1f
#define PROJECTILE_ENERGY_MIN          96
#define PROJECTILE_SCALE_MIN           2
#define PROJECTILE_ENERGY_SCALE_SHIFT  8

/* Explosion graphics. */
#define EXPLOSION_ASPECT_FIRE       0
#define EXPLOSION_ASPECT_SPELL      1
#define EXPLOSION_ASPECT_POISON     2
#define EXPLOSION_ASPECT_SMOKE      3
#define EXPLOSION_ATTACK_MIN        48
#define EXPLOSION_STRENGTH_SHIFT    5
#define EXPLOSION_PATTERN_STRENGTHS 3

/* Field aspect bytes and the masked fill arguments. */
#define FIELD_PATTERN                   0
#define FIELD_LAST_UNIT                 1
#define FIELD_TRANSPARENT               2
#define FIELD_MASK                      3
#define FIELD_WIDTH                     4
#define FIELD_HEIGHT                    5
#define FIELD_ASPECT_SIZE               8
#define FIELD_NO_MASK                   0xff
#define FIELD_MASK_FLIPPED              0x80
#define FIELD_FIRST_UNIT_RANGE          0x1f
#define EXPLOSION_FILL_LAST_UNIT        0x57
#define EXPLOSION_FILL_FIRST_UNIT_RANGE 0x3f
#define EXPLOSION_FILL_TRANSPARENT      (FILL_NO_MASK | TRANSPARENT_COLOR)

/* The bitmap of graphic g at scale/32 with palette changes; the native bitmap at full scale without
   changes. Mirroring happens at blit time. Free out->px after use. */
static const Bitmap *get_shrunk_bitmap(int16_t g, int16_t scale, const uint8_t *pal, Bitmap *out)
{
    const Bitmap *bm = gfx_get_bitmap(g);
    *out = (Bitmap){0};
    if (!bm || (scale == SCALE_NATIVE && !pal))
        return bm;
    /* 16-bit sizes as in the original */
    if (draw_shrink(bm, (int16_t)((scale >> 1) + bm->w * scale) >> SCALE_SHIFT,
                    (int16_t)((scale >> 1) + bm->h * scale) >> SCALE_SHIFT, pal, out))
        return NULL;
    return out;
}

/* Palette changes of objects and projectiles: none near, then the D2 and D3 tables. */
static const uint8_t *get_object_palette_changes(int16_t distance)
{
    return distance == 1   ? tables_get_palette_changes(TABLES_CHANGES_OBJECT_D2)
           : distance == 2 ? tables_get_palette_changes(TABLES_CHANGES_OBJECT_D3)
                           : NULL;
}

static int16_t get_object_aspect(uint16_t thing)
{
    return tables_get_object_info(inventory_get_object_info_index(thing))->aspect_index;
}

/* The object aspect of a flying thing, or minus its projectile aspect. */
static int16_t get_projectile_aspect(uint16_t thing)
{
    if (THING_TYPE(thing) == THING_EXPLOSION) {
        switch (thing - EXPLOSION_THING_BASE) {
        case EXPLOSION_FIREBALL:
            return -PROJECTILE_ASPECT_FIREBALL;
        case EXPLOSION_SLIME:
            return -PROJECTILE_ASPECT_SLIME;
        case EXPLOSION_LIGHTNING_BOLT:
            return -PROJECTILE_ASPECT_LIGHTNING;
        case EXPLOSION_POISON_BOLT:
        case EXPLOSION_POISON_CLOUD:
            return -PROJECTILE_ASPECT_POISON;
        default:
            return -PROJECTILE_ASPECT_SPELL;
        }
    }
    if (THING_TYPE(thing) == THING_WEAPON) {
        int16_t aspect = tables_get_weapon_info(rec_weapon_type(thing))->attributes >> WEAPON_PROJECTILE_ASPECT_SHIFT &
                         WEAPON_PROJECTILE_ASPECT_MASK;
        if (aspect)
            return -aspect;
    }
    return get_object_aspect(thing);
}

/* The pile box of a near object grows by the drawn box; a single object gets at least
   CLICK_BOX_MIN_W x CLICK_BOX_MIN_H pixels around its center. The object becomes the pile top and joins the pile list. */
static void add_clickable_object(int16_t index, uint16_t thing, const Box *drawn)
{
    Box *box = &g_dungeon_view_clickable_boxes[index];
    if (box->x == -1) {
        *box = *drawn;
        if (box->h < CLICK_BOX_CENTER_H) {
            if (box->h < CLICK_BOX_MIN_H)
                box->h = CLICK_BOX_MIN_H;
            box->y += (drawn->h >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
        if (box->w < CLICK_BOX_MIN_W) {
            box->w = CLICK_BOX_MIN_W;
            box->x += (drawn->w >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
    } else {
        int16_t right = max16(box->x + box->w, drawn->x + drawn->w);
        int16_t bottom = max16(box->y + box->h, drawn->y + drawn->h);
        box->x = min16(box->x, drawn->x);
        box->y = min16(box->y, drawn->y);
        box->w = right - box->x;
        box->h = bottom - box->y;
    }
    g_pile_top_objects[index] = thing;
    if (g_pile_object_counts[index] < VIEW_PILE_OBJECT_MAX)
        g_pile_objects[index][g_pile_object_counts[index]++] = thing;
}

/* One object on the floor or in an alcove. pile counts the objects of the cell for their shift;
   NULL draws an object in flight at zone without pile shift, moved by place, and drawn gets its box. */
static void draw_object(uint16_t thing, int16_t aspect_index, int16_t cell, int16_t lane, int16_t depth,
                        bool alcove_front, int16_t zone, int16_t *pile, int16_t coord, bool alcove, Placement place,
                        Box *drawn)
{
    const uint8_t *aspect = tables_get_object_aspect(aspect_index);
    bool alcove_graphic = alcove_front && (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_HAS_ALCOVE_GRAPHIC);
    int16_t graphic = GFX_OBJECT_FIRST + aspect[OBJECT_ASPECT_GRAPHIC] + alcove_graphic;
    int16_t distance = 0;
    int flip = (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_FLIP_ON_RIGHT) && !alcove_graphic &&
               (lane >= 1 || (lane == 0 && (cell == VIEW_CELL_BACK_RIGHT || cell == VIEW_CELL_FRONT_RIGHT)));
    Bitmap own;
    const Bitmap *bitmap;
    if (depth == DEPTH_D0)
        bitmap = get_shrunk_bitmap(graphic, SCALE_NATIVE, NULL, &own);
    else {
        int16_t row =
            depth * 2 - 1 - (cell == VIEW_CELL_CENTER ? 1 : cell >> 1); /* front cells count as one row nearer */
        distance = (row + 1) >> 1;
        bitmap = get_shrunk_bitmap(graphic, tables_get_object_scale(row), get_object_palette_changes(distance), &own);
    }
    if (pile) {
        zone = ZONE_SHIFTED |
               (alcove ? ZONE_ALCOVE_OBJECT_FIRST + coord + aspect[OBJECT_ASPECT_ALCOVE_COORD] * ZONE_ALCOVE_VIEW_SETS
                       : ZONE_OBJECT_FIRST + cell + coord * ZONE_OBJECT_CELLS);
        place.zone_dx = tables_get_creature_shift(distance, tables_get_pile_shift(*pile, TABLES_AXIS_X));
        place.zone_dy = tables_get_creature_shift(distance, tables_get_pile_shift(*pile, TABLES_AXIS_Y));
        ++*pile;
        if (!alcove)
            *pile &= PILE_FLOOR_SHIFTS - 1;
        else if (*pile > PILE_ALCOVE_LAST)
            *pile = PILE_ALCOVE_FIRST;
    }
    Box box;
    bool drew = blit_in_zone(bitmap, &view.viewport, &(ZoneBlit){zone, flip, TRANSPARENT_COLOR, place}, &box);
    if (drew && drawn)
        *drawn = box;
    if (drew && pile && distance == 0 && lane == 0)
        add_clickable_object(alcove ? VIEW_CLICK_BOX_ALCOVE : cell, thing, &box);
    free(own.px);
}

/* A creature group as the view sees it: on the party map the active group, otherwise the group record
   with one direction for all creatures and the creature aspects of a group that was never active. */
typedef struct {
    uint16_t thing;
    const CreatureInfo *info;
    const uint8_t *aspect;
    uint8_t cells, dirs, look[GROUP_MAX_CREATURES];
    int16_t size;
} ViewGroup;

/* The creature type, cells, directions, and poses of a group for the view: from its active group on the party map,
   else from the group record. */
static void load_group_aspect(uint16_t thing, ViewGroup *s)
{
    uint8_t index = rec_group_cells(thing);
    s->thing = thing;
    s->info = group_info(thing);
    s->aspect = tables_get_creature_aspect(s->info->aspect_index);
    s->size = creature_size(s->info);
    const ActiveGroup *a = view.map == g_party_map_index ? group_get_active(thing) : NULL;
    if (a && a->group_index == THING_INDEX(thing)) {
        s->cells = a->cells;
        s->dirs = a->directions;
        memcpy(s->look, a->aspect, sizeof s->look);
    } else {
        s->cells = index;
        s->dirs = tables_get_group_directions(rec_group_direction(thing));
        memset(s->look, 0, sizeof s->look);
    }
}

static uint16_t get_creature_cell(const ViewGroup *group, int16_t creature)
{
    return group_cell(group->cells, creature);
}
static uint16_t get_creature_direction(const ViewGroup *group, int16_t creature)
{
    return group_cell(group->dirs, creature);
}

/* The index of the creature that stands in a map cell, or -1. A half-square creature fills two cells. */
static int16_t get_creature_in_cell(const ViewGroup *s, uint16_t cell)
{
    if (s->cells == CELL_CENTERED)
        return 0;
    int16_t i = rec_group_last_creature(s->thing);
    if (s->size == CREATURE_SIZE_HALF) {
        if ((cell & 1) == (s->dirs & 1))
            cell = direction_left(cell);
        for (; i >= 0; i--)
            if (cell == get_creature_cell(s, i) || direction_right(cell) == get_creature_cell(s, i))
                return i;
    } else
        for (; i >= 0; i--)
            if (cell == get_creature_cell(s, i))
                return i;
    return -1;
}

/* One creature: front, side, back, or attack graphic for its facing, scaled and recolored by depth,
   at position pos (view cell, or 4 centered) of the creature coordinate set coord. */
static void draw_creature(const ViewGroup *group, uint8_t look, uint16_t facing, int16_t pos, int16_t coord,
                          int16_t depth)
{
    uint16_t info = group->info->graphic_info;
    int16_t graphic = GFX_CREATURE_FIRST + be16(group->aspect + CREATURE_ASPECT_GRAPHIC);
    int16_t distance = 0;
    bool side = (info & GRAPHIC_HAS_SIDE) && (facing & 1);
    bool back = false;
    bool attack = false;
    bool front_flip = false;
    if (side)
        graphic++;
    else {
        back = (info & GRAPHIC_HAS_BACK) && facing == FACING_AWAY;
        attack = !back && (look & ASPECT_ATTACKING) && (info & GRAPHIC_HAS_ATTACK);
        if (attack)
            graphic += 1 + !!(info & GRAPHIC_HAS_SIDE) + !!(info & GRAPHIC_HAS_BACK);
        else if (back)
            graphic += 1 + !!(info & GRAPHIC_HAS_SIDE);
        else
            front_flip = (info & GRAPHIC_FLIP_NON_ATTACK) && (look & ASPECT_FLIPPED);
    }
    bool special_d2 = depth == DEPTH_D2 && (info & GRAPHIC_SPECIAL_D2_FRONT) && !side && !back && !attack;
    bool flip = (side && facing == FACING_LEFT) || (attack && (look & ASPECT_FLIPPED)) ||
                (special_d2 && (info & GRAPHIC_D2_FRONT_FLIPPED)) || front_flip;
    int16_t transparent = group->aspect[CREATURE_ASPECT_COORD] & CREATURE_ASPECT_TRANSPARENT;
    Bitmap own;
    const Bitmap *bitmap;
    if (depth < DEPTH_D2)
        bitmap = get_shrunk_bitmap(graphic, SCALE_NATIVE, NULL, &own);
    else {
        const uint8_t *pal = depth == DEPTH_D2 ? view.creature_palette_changes_d2 : view.creature_palette_changes_d3;
        distance = depth == DEPTH_D2 ? 1 : 2;
        transparent = pal[transparent];
        bitmap = get_shrunk_bitmap(graphic, depth == DEPTH_D2 ? SCALE_D2 : SCALE_D3, pal, &own);
    }
    Placement place = {
        .zone_dx = tables_get_creature_shift(distance, look & ASPECT_OFFSET_MASK),
        .zone_dy = tables_get_creature_shift(distance, look >> ASPECT_VERTICAL_OFFSET_SHIFT & ASPECT_OFFSET_MASK),
    };
    int16_t set = group->aspect[CREATURE_ASPECT_COORD] >> CREATURE_ASPECT_COORD_SHIFT;
    int16_t zone =
        ZONE_SHIFTED | (ZONE_CREATURE_FIRST + set * ZONE_CREATURE_SET + coord * ZONE_CREATURE_POSITIONS + pos);
    blit_in_zone(bitmap, &view.viewport, &(ZoneBlit){zone, flip, transparent, place}, NULL);
    free(own.px);
}

/* The creatures of the group in view cell cell (map cell mcell). rest holds the cells still to draw,
   front tells that the cell is a front cell of the pass. Returns true when the group is complete. */
static bool draw_creatures_in_cell(const ViewGroup *group, int16_t dir, int16_t view_square, int16_t depth,
                                   int16_t cell, uint16_t mcell, uint16_t rest, int16_t pass, bool front)
{
    int16_t coord = tables_get_view_square(view_square).creature_coord;
    int16_t index;
    int16_t second = 0;
    int16_t pos = cell;
    bool done = false;
    if (coord < 0)
        return true;
    uint16_t creature_index = 0;
    index = get_creature_in_cell(group, mcell);
    if (index < 0) {
        if (group->size != CREATURE_SIZE_HALF)
            return false;
    } else
        creature_index = index;
    uint16_t relative = dir - get_creature_direction(group, creature_index);
    uint16_t facing = NORMALIZE(relative);
    int sideways = relative & 1;
    if (group->cells == CELL_CENTERED) { /* one creature in the middle, drawn after the last cell */
        if (rest || pass == DOOR_PASS_BACK)
            return false;
        done = true;
        pos = group->size == CREATURE_SIZE_HALF && sideways ? VIEW_CELL_FRONT_LEFT : VIEW_CELL_CENTER;
    } else if (group->size == CREATURE_SIZE_HALF && (front || !rest || index < 0)) {
        if (!front || pass == DOOR_PASS_FRONT) {
            if (pass == DOOR_PASS_BACK || rest)
                return false;
            if (!sideways) { /* facing the party or away: both creatures side by side */
                done = true;
                if (index < 0)
                    index = 0;
                second = rec_group_last_creature(group->thing);
                uint16_t c = get_creature_cell(group, creature_index);
                pos = c == dir || direction_left(dir) == c ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT;
            } else {
                if (index < 0)
                    return false;
                pos = VIEW_CELL_CENTER;
            }
        } else if (index >= 0 && sideways)
            pos = VIEW_CELL_FRONT_RIGHT;
        else
            return false;
    } else if (group->size != CREATURE_SIZE_QUARTER)
        return false;
    if (group->size == CREATURE_SIZE_QUARTER && ((view_square == VIEW_SQUARE_D0L && pos != VIEW_CELL_FRONT_RIGHT) ||
                                                 (view_square == VIEW_SQUARE_D0R && pos != VIEW_CELL_FRONT_LEFT)))
        return done;
    draw_creature(group, group->look[index], facing, pos, coord, depth);
    if (second) /* the other creature of a half-size pair, beside it */
        draw_creature(group, group->look[index == 0], facing,
                      pos == VIEW_CELL_BACK_RIGHT ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT, coord, depth);
    return done;
}

/* A tick view starts new tracks; the old tracks stay when the party still looks from the same place. */
static void start_tracks(int16_t dir, int16_t x, int16_t y)
{
    int32_t place = ((((int32_t)view.map << MAP_COORD_SHIFT | x) << MAP_COORD_SHIFT | y) << DIRECTION_SHIFT) | dir;
    flight.last_track_count = place == flight.track_view ? flight.track_count : 0;
    memcpy(flight.last_tracks, flight.tracks, sizeof flight.tracks[0] * (size_t)flight.last_track_count);
    flight.track_count = 0;
    flight.track_view = place;
}

/* The track of a projectile; the cell bits of the thing change while it flies. */
static const ProjectileTrack *find_track(const ProjectileTrack *list, int count, uint16_t thing)
{
    for (int i = 0; i < count; i++)
        if (THING_STRIP_CELL(list[i].thing) == THING_STRIP_CELL(thing))
            return &list[i];
    return NULL;
}

/* Between ticks: the offset from the new box of the projectile toward its old box. */
static Placement begin_projectile(uint16_t thing)
{
    Placement place = {0};
    const ProjectileTrack *t = flight.motion_steps ? find_track(flight.tracks, flight.track_count, thing) : NULL;
    if (!t)
        return place;
    int16_t rest = flight.motion_steps - flight.motion_step;
    place.shift_x = (t->from.x - t->to.x) * rest / flight.motion_steps;
    place.shift_y = (t->from.y - t->to.y) * rest / flight.motion_steps;
    return place;
}

/* At a tick: remembers where the projectile was drawn (box), and where it was at the tick before. */
static void end_projectile(uint16_t thing, bool drawn, const Box *box)
{
    if (flight.motion_steps || !drawn || flight.track_count == PROJECTILE_TRACK_COUNT)
        return;
    const ProjectileTrack *last = find_track(flight.last_tracks, flight.last_track_count, thing);
    flight.tracks[flight.track_count++] = (ProjectileTrack){thing, last ? last->to : *box, *box};
}

/* True when a projectile of the last view moved between the last two ticks. */
bool view_has_moving_projectiles(void)
{
    for (int i = 0; i < flight.track_count; i++)
        if (flight.tracks[i].from.x != flight.tracks[i].to.x || flight.tracks[i].from.y != flight.tracks[i].to.y)
            return true;
    return false;
}

void view_set_motion(int16_t step, int16_t steps)
{
    flight.motion_step = step;
    flight.motion_steps = steps;
}

/* The pose of a projectile graphic by its direction of flight, and its mirror flags in flip_out. */
static int16_t get_projectile_pose(const uint8_t *aspect, uint16_t thing, int16_t dir, int16_t x, int16_t y,
                                   int16_t cell, int16_t lane, int16_t *flip_out)
{
    int16_t kind = aspect[PROJECTILE_ASPECT_POSE_FLAGS] & PROJECTILE_KIND;
    int16_t pose;
    int16_t flip = 0;
    bool rotates = kind == PROJECTILE_BACK_AND_ROTATION;
    bool odd_square = rotates && ((x + y) & 1);
    if (kind == PROJECTILE_NO_BACK_NO_ROTATION)
        pose = POSE_FRONT;
    else {
        int16_t flight_dir = projectile_get_flight_direction(thing);
        if ((dir & 1) == (flight_dir & 1)) { /* flying away from or toward the party: front or back pose */
            pose = kind < PROJECTILE_NO_BACK && (kind != PROJECTILE_BACK || flight_dir == dir) && !odd_square
                       ? POSE_BACK
                       : POSE_FRONT;
            if (rotates && cell < VIEW_CELL_FRONT_RIGHT)
                flip = FLIP_V;
            if ((aspect[PROJECTILE_ASPECT_POSE_FLAGS] & PROJECTILE_FLIP_ON_LEFT) && lane < 1 &&
                (lane != 0 || (cell != VIEW_CELL_BACK_RIGHT && cell != VIEW_CELL_FRONT_RIGHT)))
                flip |= FLIP_H;
        } else { /* flying across: side pose */
            pose = kind == PROJECTILE_NO_BACK ? POSE_BACK
                                              : POSE_SIDE; /* without a back graphic the side graphic is second */
            if (!rotates)
                flip = direction_right(dir) == flight_dir;
            else {
                flip = cell == VIEW_CELL_BACK_LEFT || cell == VIEW_CELL_FRONT_LEFT;
                if (!odd_square)
                    flip ^= FLIP_H;
                else
                    flip |= FLIP_V;
            }
        }
    }
    *flip_out = flip;
    return pose;
}

/* A projectile in view cell cell: an object in flight, or a projectile graphic posed and mirrored by
   its direction of flight and scaled by depth (and kinetic energy). */
static void draw_projectile(uint16_t thing, int16_t dir, int16_t x, int16_t y, int16_t cell, int16_t lane,
                            int16_t depth, int16_t coord)
{
    int16_t zone = ZONE_PROJECTILE_FIRST + coord * ZONE_OBJECT_CELLS + cell;
    int16_t aspect_index = get_projectile_aspect(rec_projectile_slot(thing));
    if (aspect_index >= 0) {
        Box drawn = {0};
        draw_object(thing, aspect_index, cell, lane, depth, false, zone, NULL, 0, false, begin_projectile(thing),
                    &drawn);
        end_projectile(thing, drawn.w > 0, &drawn);
        return;
    }
    const uint8_t *aspect = tables_get_projectile_aspect(-aspect_index);
    int16_t by_energy = aspect[PROJECTILE_ASPECT_SCALE_FLAGS] & PROJECTILE_SCALE_WITH_ENERGY;
    int16_t scale = SCALE_NATIVE;
    int16_t row = 0;
    int16_t flip;
    uint8_t energy = rec_projectile_energy(thing);
    if ((by_energy && energy != KINETIC_ENERGY_MAXIMUM) || depth) {
        row = depth * 2 - (cell >> 1);
        scale = tables_get_projectile_scale(row);
        if (by_energy) /* 16-bit product and signed byte as in the original */
            scale = max16(PROJECTILE_SCALE_MIN, (int8_t)((uint16_t)(max16(PROJECTILE_ENERGY_MIN, energy + 1) * scale) >>
                                                         PROJECTILE_ENERGY_SCALE_SHIFT));
    }
    int16_t pose = get_projectile_pose(aspect, thing, dir, x, y, cell, lane, &flip);
    int16_t distance = row >> 1 == 2 ? 1 : row >> 1 == 3 ? 2 : 0; /* D2 or D3 palette changes */
    Bitmap own;
    const Bitmap *bm = get_shrunk_bitmap(GFX_PROJECTILE_FIRST + aspect[PROJECTILE_ASPECT_GRAPHIC] + pose, scale,
                                         scale == SCALE_NATIVE ? NULL : get_object_palette_changes(distance), &own);
    Box drawn;
    bool drew =
        blit_in_zone(bm, &view.viewport, &(ZoneBlit){zone, flip, TRANSPARENT_COLOR, begin_projectile(thing)}, &drawn);
    end_projectile(thing, drew, &drawn);
    free(own.px);
}

/* A teleporter field or fluxcage: the pattern graphic through the mask graphic at zone, with a random
   start in the pattern so that it sparkles. */
static void draw_field(const uint8_t *aspect, int16_t zone)
{
    int16_t width = aspect[FIELD_WIDTH];
    int16_t height = aspect[FIELD_HEIGHT];
    int16_t sx = 0;
    int16_t flip = 0;
    Box box;
    if (!layout_get_zone(zone, 0, 0, &box, &width, &height))
        return;
    const Bitmap *mask = NULL;
    const Bitmap *units = gfx_get_bitmap(GFX_FIELD_PATTERN_FIRST + aspect[FIELD_PATTERN]);
    if (aspect[FIELD_MASK] != FIELD_NO_MASK) {
        mask = gfx_get_bitmap(GFX_FIELD_MASK_FIRST + (aspect[FIELD_MASK] & ~FIELD_MASK_FLIPPED));
        flip = !!(aspect[FIELD_MASK] & FIELD_MASK_FLIPPED);
        if (flip && mask && mask->w % PIXELS_PER_WORD)
            sx = PIXELS_PER_WORD - mask->w % PIXELS_PER_WORD;
    }
    int16_t first = random16() & FIELD_FIRST_UNIT_RANGE;
    int16_t last = aspect[FIELD_LAST_UNIT] + random_bit();
    if (units)
        draw_masked_fill(units, mask, flip, &view.viewport, &box, last, first, aspect[FIELD_TRANSPARENT], sx, 0);
}

/* An explosion of aspect at scale, recolored for smoke. */
static const Bitmap *get_explosion_bitmap(int16_t aspect, int16_t scale, Bitmap *own)
{
    scale = bounded_value(SCALE_EXPLOSION_MIN, scale & ~1, SCALE_NATIVE);
    return get_shrunk_bitmap(GFX_EXPLOSION_FIRST + min16(aspect, EXPLOSION_ASPECT_POISON), scale,
                             aspect == EXPLOSION_ASPECT_SMOKE ? tables_get_palette_changes(TABLES_CHANGES_SMOKE) : NULL,
                             own);
}

/* An explosion on the party square fills the whole viewport with its pattern. */
static void draw_explosion_on_party(uint16_t thing, int16_t aspect)
{
    bool smoke = aspect == EXPLOSION_ASPECT_SMOKE;
    int16_t strength = rec_explosion_attack(thing) >> EXPLOSION_STRENGTH_SHIFT;
    const Bitmap *units = gfx_get_bitmap(GFX_EXPLOSION_PATTERN_FIRST + (aspect - smoke) * EXPLOSION_PATTERN_STRENGTHS +
                                         (strength != 0) + (strength > 3));
    Bitmap own = {0};
    if (units && smoke && (own.px = malloc((size_t)units->w * units->h))) {
        const uint8_t *changes = tables_get_palette_changes(TABLES_CHANGES_SMOKE);
        own.w = units->w;
        own.h = units->h;
        for (int i = 0; i < own.w * own.h; i++)
            own.px[i] = changes[units->px[i]] & COLOR_INDEX_MASK;
        units = &own;
    }
    int16_t first = random16() & EXPLOSION_FILL_FIRST_UNIT_RANGE;
    int16_t last = random_2bits() + EXPLOSION_FILL_LAST_UNIT;
    int16_t width = 0;
    int16_t height = 0;
    Box box;
    if (units && layout_get_zone(ZONE_VIEWPORT_BOX, view.viewport.w, view.viewport.h, &box, &width, &height))
        draw_masked_fill(units, NULL, 0, &view.viewport, &box, last, first, EXPLOSION_FILL_TRANSPARENT, 0, 0);
    free(own.px);
}

/* The explosion graphic of an explosion type: fire, poison, smoke, or spell. */
static int16_t get_explosion_aspect(int16_t type)
{
    switch (type) {
    case EXPLOSION_FIREBALL:
    case EXPLOSION_LIGHTNING_BOLT:
    case EXPLOSION_REBIRTH_STEP_2:
        return EXPLOSION_ASPECT_FIRE;
    case EXPLOSION_POISON_BOLT:
    case EXPLOSION_POISON_CLOUD:
        return EXPLOSION_ASPECT_POISON;
    case EXPLOSION_SMOKE:
        return EXPLOSION_ASPECT_SMOKE;
    default:
        return EXPLOSION_ASPECT_SPELL;
    }
}

/* The first step of a vi altar rebirth: the back pose of the lightning projectile. */
static void draw_rebirth_step_1(int16_t coord, int16_t flip)
{
    Bitmap own = {0};
    int16_t scale = tables_get_rebirth_scale(TABLES_REBIRTH_STEP_1, coord);
    int16_t g = GFX_PROJECTILE_FIRST +
                tables_get_projectile_aspect(PROJECTILE_ASPECT_LIGHTNING)[PROJECTILE_ASPECT_GRAPHIC] + POSE_BACK;
    blit_in_zone(get_shrunk_bitmap(g, scale, NULL, &own), &view.viewport,
                 &(ZoneBlit){ZONE_REBIRTH_STEP1_FIRST + coord, flip, TRANSPARENT_COLOR, {0}}, NULL);
    free(own.px);
}

/* A fluxcage: the field of the square with the pattern after the teleporter pattern. */
static void draw_fluxcage(int16_t field_index)
{
    uint8_t aspect[FIELD_ASPECT_SIZE];
    memcpy(aspect, tables_get_field_aspect(field_index), FIELD_ASPECT_SIZE);
    aspect[FIELD_PATTERN]++; /* the fluxcage pattern follows the teleporter pattern */
    draw_field(aspect, ZONE_FIELD_FIRST + field_index);
}

/* The explosions of a square, each mirrored at random, then a fluxcage as a field. */
static void draw_explosions(uint16_t thing, int16_t dir, int16_t view_square, int16_t depth, int16_t coord,
                            uint16_t mcell, int16_t pass)
{
    ViewSquare square = tables_get_view_square(view_square);
    int16_t explosion_coord = square.explosion_coord;
    int16_t field_index = square.field_aspect;
    bool fluxcage = false;
    if (explosion_coord < 0 && field_index < 0)
        return;
    for (; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        if (THING_TYPE(thing) != THING_EXPLOSION)
            continue;
        uint16_t cell = THING_CELL(thing);
        int16_t type = rec_explosion_type(thing);
        bool rebirth = type >= EXPLOSION_REBIRTH_STEP_1;
        int16_t zone;
        int16_t scale;
        if (rebirth && !(coord >= 0 && cell == mcell))
            continue;
        int16_t flip = random_2bits();
        if (type == EXPLOSION_REBIRTH_STEP_1) {
            draw_rebirth_step_1(coord, flip);
            continue;
        }
        if (type == EXPLOSION_FLUXCAGE) {
            fluxcage |= depth < DEPTH_D4;
            continue;
        }
        int16_t aspect = get_explosion_aspect(type);
        Bitmap own = {0};
        if (explosion_coord < 0)
            continue;
        if (view_square == VIEW_SQUARE_D0C) {
            draw_explosion_on_party(thing, aspect);
            continue;
        }
        if (rebirth) {
            zone = ZONE_REBIRTH_STEP2_FIRST + coord;
            scale = tables_get_rebirth_scale(TABLES_REBIRTH_STEP_2, coord);
        } else {
            bool far_side = !(cell == dir || direction_left(dir) == cell);
            zone = rec_explosion_is_centered(thing)
                       ? ZONE_EXPLOSION_CENTERED_FIRST + explosion_coord
                       : ZONE_EXPLOSION_FIRST + explosion_coord * ZONE_EXPLOSION_SIDES + far_side;
            scale = (int16_t)(max16(EXPLOSION_ATTACK_MIN, rec_explosion_attack(thing) + 1) *
                              tables_get_explosion_scale(depth)) >>
                    8; /* 16-bit product as in the original */
        }
        blit_in_zone(get_explosion_bitmap(aspect, scale, &own), &view.viewport,
                     &(ZoneBlit){zone, flip, TRANSPARENT_COLOR, {0}}, NULL);
        free(own.px);
    }
    if (fluxcage && field_index >= 0 && pass != DOOR_PASS_BACK && !g_do_not_draw_fluxcages)
        draw_fluxcage(field_index);
}

/* D3 squares show only their front cells, D0 squares only their back cells. */
static bool is_cell_in_view(int16_t depth, int16_t cell)
{
    return (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) && (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT);
}

/* The objects of map cell mcell, drawn in view cell cell; pile counts the objects drawn on the pile. */
static void draw_cell_objects(uint16_t first, int16_t cell, uint16_t mcell, int16_t lane, int16_t depth, int16_t coord,
                              bool alcove, int16_t *pile)
{
    if (coord < 0 || !is_cell_in_view(depth, cell))
        return;
    for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        int16_t type = THING_TYPE(t);
        if (type >= THING_WEAPON && type <= THING_JUNK && THING_CELL(t) == mcell)
            draw_object(t, get_object_aspect(t), cell, lane, depth, alcove && lane == 0, 0, pile, coord, alcove,
                        (Placement){0}, NULL);
    }
}

/* The last group of a square, and whether the square has projectiles and explosions. */
static void find_square_things(uint16_t first, uint16_t *group_thing, bool *has_projectiles, bool *has_explosions)
{
    for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        int16_t type = THING_TYPE(t);
        if (type == THING_GROUP)
            *group_thing = t;
        else if (type == THING_PROJECTILE)
            *has_projectiles = true;
        else if (type == THING_EXPLOSION)
            *has_explosions = true;
    }
}

/* The things of one view square: per cell in cell order (nibbles of cell + 1, low first; 0 = the
   alcove; bit 3 = door pass, bit 0 the front pass) the objects, creatures, and projectiles, then the
   explosions of the square. */
static void draw_square_things(uint16_t first, int16_t dir, int16_t x, int16_t y, int16_t view_square, uint16_t order)
{
    if (first == THING_ENDOFLIST)
        return;
    int16_t pass = 0;
    if (order & CELL_ORDER_DOOR_PASS) {
        pass = (order & 1) ? DOOR_PASS_FRONT : DOOR_PASS_BACK;
        order >>= CELL_ORDER_BITS;
    }
    uint16_t rest = order;
    uint16_t mcell = 0;
    uint16_t group_thing = THING_NONE;
    bool alcove = rest == 0;
    bool has_projectiles = false;
    bool has_explosions = false;
    bool group_done = false;
    bool loaded = false;
    ViewSquare square = tables_get_view_square(view_square);
    int16_t lane = square.lane;
    int16_t depth = square.depth;
    int16_t coord = alcove ? square.alcove_coord : square.object_coord;
    int16_t count = 0;
    ViewGroup group;
    find_square_things(first, &group_thing, &has_projectiles, &has_explosions);
    do {
        int16_t cell, pile;
        if (!alcove) {
            cell = (rest & CELL_ORDER_MASK) - 1;
            rest >>= CELL_ORDER_BITS;
            count++;
            mcell = NORMALIZE(cell + dir);
            pile = 0;
        } else {
            cell = VIEW_CELL_CENTER;
            mcell = direction_back(dir); /* the alcove side of the wall faces the party */
            pile = PILE_ALCOVE_FIRST;
        }
        pile += (mcell & 1) * PILE_ODD_CELL; /* odd map cells start in the second half of the shifts */
        draw_cell_objects(first, cell, mcell, lane, depth, coord, alcove, &pile);
        if (alcove || depth >= DEPTH_D4)
            break;
        bool front = (cell < VIEW_CELL_FRONT_RIGHT || count == 1) &&
                     (rest == 0 || (rest & CELL_ORDER_MASK) - 1 >= VIEW_CELL_FRONT_RIGHT);
        if (group_thing != THING_NONE && !group_done) {
            if (!loaded) {
                load_group_aspect(group_thing, &group);
                loaded = true;
            }
            group_done = draw_creatures_in_cell(&group, dir, view_square, depth, cell, mcell, rest, pass, front);
        }
        if (has_projectiles && square.object_coord >= 0 && is_cell_in_view(depth, cell))
            for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t))
                if (THING_TYPE(t) == THING_PROJECTILE && THING_CELL(t) == mcell)
                    draw_projectile(t, dir, x, y, cell, lane, depth, square.object_coord);
    } while (rest);
    if (has_explosions)
        draw_explosions(first, dir, view_square, depth, coord, mcell, pass);
}

/* The field of a teleporter square. */
static void draw_teleporter_field(int16_t view_square, int16_t zone)
{
    draw_field(tables_get_field_aspect(tables_get_view_square(view_square).field_aspect), zone);
}

/* The shrink of get_scaled_flipped_bitmap: scale/32 per axis. out->px is NULL on failure. */
static void get_scaled_bitmap(int16_t g, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes, Bitmap *out)
{
    const Bitmap *bm = gfx_get_bitmap(g);
    *out = (Bitmap){0};
    if (bm) /* 16-bit sizes as in the original */
        draw_shrink(bm, (int16_t)((scale_x >> 1) + bm->w * scale_x) >> SCALE_SHIFT,
                    (int16_t)((scale_y >> 1) + bm->h * scale_y) >> SCALE_SHIFT, palette_changes, out);
}

/* The inscription of the D1C wall: the blank wall, then the glyphs of each text line. */
static void draw_wall_text(const uint8_t *text)
{
    const Bitmap *font = gfx_get_bitmap(GFX_WALL_TEXT_FONT);
    int16_t glyph_w = tables_get_glyph_width();
    int16_t glyph_h = tables_get_glyph_height();
    draw_wall_bitmap(LAYOUT_INSCRIPTION_WALL, ZONE_INSCRIPTION_WALL_D1C, BLIT_TRANSPARENT);
    for (int16_t line = 0; font; line++) {
        int16_t n = 0;
        while (text[n] < WALL_TEXT_NEW_LINE)
            n++;
        Box box;
        int16_t width = glyph_w * n;
        int16_t height = glyph_h;
        if (n && layout_get_zone(ZONE_WALL_TEXT_LINE_FIRST + line, 0, 0, &box, &width, &height))
            for (box.w = glyph_w; n; n--, text++, box.x += glyph_w)
                draw_blit(font, *text * glyph_w, 0, &view.viewport, &box, TRANSPARENT_COLOR, 0);
        text += n;
        if (*text++ == WALL_TEXT_END)
            break;
    }
}

/* The champion portrait of a portrait sensor on the D1C wall. */
static void draw_wall_portrait(int16_t champion)
{
    const Bitmap *portraits = gfx_get_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t pw = tables_get_portrait_width();
    int16_t ph = tables_get_portrait_height();
    int16_t w = pw;
    int16_t h = ph;
    Box b;
    if (portraits && layout_get_zone(ZONE_WALL_PORTRAIT, 0, 0, &b, &w, &h))
        draw_blit(portraits, champion % PORTRAITS_PER_ROW * pw, champion / PORTRAITS_PER_ROW * ph, &view.viewport, &b,
                  PORTRAIT_TRANSPARENT, 0);
}

/* The ornament or inscription on a view wall, and on the D1C wall the champion
   portrait and the facing flags. Returns true for an alcove. */
static bool draw_wall_ornament(int16_t i, int16_t wall_view, const Aspect *aspect)
{
    if (i < 0)
        return false;
    int16_t zone = ZONE_WALL_ORNAMENT_FIRST + ZONE_WALL_ORNAMENT_VIEWS * view.wall_ornaments[i].coord + wall_view;
    bool alcove = is_alcove(i);
    bool text = i == view.inscription;
    bool mirrored = wall_view == VIEW_WALL_D3R2_LEFT || wall_view == VIEW_WALL_D3R_LEFT ||
                    wall_view == VIEW_WALL_D2R_LEFT || wall_view == VIEW_WALL_D1R_LEFT;
    bool side = mirrored || wall_view == VIEW_WALL_D3L2_RIGHT || wall_view == VIEW_WALL_D3L_RIGHT ||
                wall_view == VIEW_WALL_D2L_RIGHT || wall_view == VIEW_WALL_D1L_RIGHT;
    int16_t graphic = view.wall_ornaments[i].graphic + (side ? 0 : WALL_ORNAMENT_FRONT);
    uint8_t wall_text[WALL_TEXT_SIZE] = {WALL_TEXT_END};
    if (text)
        dungeon_decode_text((char *)wall_text, sizeof wall_text, aspect->text, 0);
    if (wall_view == VIEW_WALL_D1C_FRONT) {
        if (text) {
            draw_wall_text(wall_text);
            return alcove;
        }
        g_facing_alcove = alcove;
        g_facing_vi_altar = i == view.vi_altar;
        g_facing_fountain = i == view.fountain;
    }
    Bitmap own = {0};
    const Bitmap *bitmap;
    if (wall_view < VIEW_WALL_D1L_RIGHT) { /* D3 and D2: scaled and recolored */
        if (wall_view <= VIEW_WALL_D3R2_LEFT)
            get_scaled_bitmap(graphic, SCALE_D3_FAR_SIDE_W, SCALE_D3, tables_get_palette_changes(TABLES_CHANGES_D3),
                              &own);
        else if (wall_view <= VIEW_WALL_D3R_FRONT)
            get_scaled_bitmap(graphic, SCALE_D3, SCALE_D3, tables_get_palette_changes(TABLES_CHANGES_D3), &own);
        else
            get_scaled_bitmap(graphic, SCALE_D2, SCALE_D2, tables_get_palette_changes(TABLES_CHANGES_D2), &own);
        if (!own.px)
            return alcove;
        bitmap = &own;
    } else
        bitmap = gfx_get_bitmap(graphic);
    Placement place = {0};
    if (text) { /* the plaque shows only as many lines as the text has */
        int16_t lines = 1;
        for (const uint8_t *p = wall_text; *p != WALL_TEXT_END; p++)
            lines += *p == WALL_TEXT_NEW_LINE;
        if (lines <= TABLES_PLAQUE_LINES && bitmap) {
            zone |= ZONE_SIZED;
            place.zone_dx = bitmap->w;
            place.zone_dy = tables_get_plaque_height(lines, tables_get_wall_ornament_depth(wall_view));
        }
    }
    Box drawn;
    if (blit_in_zone(bitmap, &view.viewport, &(ZoneBlit){zone, mirrored ? FLIP_H : 0, TRANSPARENT_COLOR, place},
                     &drawn) &&
        wall_view == VIEW_WALL_D1C_FRONT)
        g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn;
    if (wall_view == VIEW_WALL_D1C_FRONT && aspect->portrait >= 0)
        draw_wall_portrait(aspect->portrait);
    free(own.px);
    return alcove;
}

/* The floor ornament i on a floor view. */
static void draw_floor_ornament(int16_t i, int16_t floor_view)
{
    if (i < 0)
        return;
    bool right = floor_view == VIEW_D3R2 || floor_view == VIEW_D3R || floor_view == VIEW_D2R || floor_view == VIEW_D1R;
    bool center = floor_view == VIEW_D3C || floor_view == VIEW_D2C || floor_view == VIEW_D1C;
    bool flip = right || (i == FLOOR_ORNAMENT_FOOTPRINTS && view.use_flipped_walls && center);
    int16_t zone = ZONE_FLOOR_ORNAMENT_FIRST + ZONE_FLOOR_ORNAMENT_VIEWS * view.floor_ornaments[i].coord + floor_view;
    blit_in_zone(gfx_get_bitmap(view.floor_ornaments[i].graphic + tables_get_floor_ornament_offset(floor_view)),
                 &view.viewport, &(ZoneBlit){zone, flip, TRANSPARENT_COLOR, {0}}, NULL);
}

/* An ornament or mask on the door bitmap, scaled for D3 and D2. */
static void draw_door_ornament(int16_t i, int16_t depth, Bitmap *door)
{
    if (i < 0)
        return;
    const Ornament *o = &view.door_ornaments[i];
    int16_t zone = ZONE_DOOR_ORNAMENT_FIRST + ZONE_DOOR_ORNAMENT_DEPTHS * o->coord + depth;
    const ZoneBlit blit = {zone, 0, DOOR_ORNAMENT_TRANSPARENT, {0}};
    Bitmap own = {0};
    if (depth == DOOR_DEPTH_D1)
        blit_in_zone(gfx_get_bitmap(o->graphic), door, &blit, NULL);
    else {
        int16_t scale = depth == DOOR_DEPTH_D2 ? SCALE_D2 : SCALE_D3;
        get_scaled_bitmap(o->graphic, scale, scale,
                          tables_get_palette_changes(depth == DOOR_DEPTH_D2 ? TABLES_CHANGES_DOOR_ORNAMENT_D2
                                                                            : TABLES_CHANGES_DOOR_ORNAMENT_D3),
                          &own);
        if (own.px)
            blit_in_zone(&own, door, &blit, NULL);
    }
    free(own.px);
}

/* The button beside a door. The D1C button is clickable. */
static void draw_door_button(int16_t button)
{
    Bitmap own = {0};
    const Bitmap *bm = gfx_get_bitmap(GFX_DOOR_BUTTON);
    if (button != BUTTON_D1C) {
        int16_t scale = button == BUTTON_D2C ? SCALE_D2 : SCALE_D3;
        get_scaled_bitmap(GFX_DOOR_BUTTON, scale, scale,
                          tables_get_palette_changes(button == BUTTON_D2C ? TABLES_CHANGES_D2 : TABLES_CHANGES_D3),
                          &own);
        bm = &own;
    }
    int16_t zone = ZONE_DOOR_BUTTON_FIRST + tables_get_door_button_coords() + button;
    Box drawn;
    if (bm && bm->px && blit_in_zone(bm, &view.viewport, &(ZoneBlit){zone, 0, TRANSPARENT_COLOR, {0}}, &drawn) &&
        button == BUTTON_D1C)
        g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn;
    free(own.px);
}

/* A copy of the door graphic with its ornaments, placed by the door state. A door that does not
   move vertically opens as two halves that slide apart. */
static void draw_door(uint16_t door_thing, int16_t state, int16_t door_view)
{
    if (state == DOOR_OPEN)
        return;
    int16_t depth = door_view < VIEW_D2L ? DOOR_DEPTH_D3 : door_view < VIEW_D1L ? DOOR_DEPTH_D2 : DOOR_DEPTH_D1;
    int16_t zone = ZONE_DOOR_FIRST + ZONE_DOOR_VIEW_ZONES * door_view;
    int type = rec_door_type(door_thing);
    const Bitmap *graphic = gfx_get_bitmap(view.door_graphics[depth][type]);
    if (!graphic)
        return;
    Bitmap door_copy = {graphic->w, graphic->h, malloc((size_t)graphic->w * graphic->h)};
    if (!door_copy.px)
        return;
    memcpy(door_copy.px, graphic->px, (size_t)graphic->w * graphic->h);
    draw_door_ornament(rec_door_ornament(door_thing), depth, &door_copy);
    if (view.door_type_attributes[type] & DOOR_ANIMATED)
        draw_flip(&door_copy, random_2bits());
    Placement place = {0};
    if (state == DOOR_DESTROYED)
        draw_door_ornament(DOOR_ORNAMENT_DESTROYED, depth, &door_copy);
    else if (state != DOOR_CLOSED) { /* partly open: the zone moves with the state */
        zone += state;
        if (!rec_door_is_vertical(door_thing)) {
            place.zone_dx = door_copy.w >> 1;
            int16_t w = place.zone_dx;
            int16_t h = place.zone_dy;
            Box b;
            if (layout_get_zone(zone + ZONE_DOOR_RIGHT_HALF, door_copy.w, door_copy.h, &b, &w, &h))
                draw_blit(&door_copy, place.zone_dx + w, h, &view.viewport, &b, TRANSPARENT_COLOR, 0);
            zone = (zone + ZONE_DOOR_LEFT_HALF) | ZONE_SIZED;
        }
    }
    blit_in_zone(&door_copy, &view.viewport, &(ZoneBlit){zone, 0, TRANSPARENT_COLOR, place}, NULL);
    free(door_copy.px);
}

/* The hole of an open pit on the map above. */
static void draw_ceiling_pit(int16_t graphic, int16_t zone, int16_t x, int16_t y, int mode)
{
    Position above = map_get_level_change_location((Position){view.map, x, y}, LEVEL_UP);
    if (above.map >= 0 && rec_square_type(rec_square_byte(above.map, above.x, above.y)) == SQUARE_PIT &&
        rec_square_byte(above.map, above.x, above.y) & PIT_OPEN)
        draw_wall_bitmap(graphic, zone, mode);
}

/* The front of a staircase: the up or down graphic at the up or down zone. */
static void draw_stairs_front(bool up, int16_t graphic, int16_t up_zone, int mode)
{
    draw_wall_bitmap(view.stairs + graphic + (up ? 0 : STAIRS_DOWN), up ? up_zone : up_zone + ZONE_STAIRS_DOWN_DELTA,
                     mode);
}

#define DRAWN_SQUARE_COUNT 16 /* the squares that the view draws, and the D4 squares with objects */
#define D4_SQUARE_COUNT    3

/* How draw_square draws a square: all elements, only a wall or a teleporter field, a D0 side square, or the
   square of the party. */
#define SHAPE_FULL          0
#define SHAPE_WALL_OR_FIELD 1
#define SHAPE_D0_SIDE       2
#define SHAPE_D0C           3
#define SIDE_FACE_NONE      0 /* the wall side that a side square shows */
#define SIDE_FACE_RIGHT     1
#define SIDE_FACE_LEFT      2
#define DOOR_FRAMES_MAX     3 /* door frame bitmaps of a door front */

/* Square view flags. */
#define SQ_CEILING_IN_ALCOVE  0x1 /* an alcove draws the ceiling pit before its things */
#define SQ_CEILING_AFTER_DOOR 0x2 /* a door front draws the ceiling pit after the door */
#define SQ_STAIRS_LAYOUT_IDS  0x4 /* the stairs front graphics are layout ids, not stairs graphics */
#define SQ_FRONT              0x8 /* the square ahead: sets g_square_ahead_element and the facing flags */

/* One wall set bitmap: wall_set_ids index, zone, and BLIT_* mode. */
typedef struct {
    int16_t ws, zone;
    uint8_t mode;
} WallPart;

/* The position of a drawn square from the party and the graphics and zones of each element on it. */
typedef struct {
    int8_t forward, right;            /* position from the party */
    uint8_t shape;                    /* SHAPE_* */
    uint8_t mirror;                   /* BLIT_TRANSPARENT or BLIT_FLIPPED: pit, stairs, and ceiling pit */
    int16_t view, view_square;        /* VIEW_D* of the floor ornament and the door; VIEW_SQUARE_D* of things */
    WallPart wall;                    /* the wall; its zone is also the zone of a teleporter field */
    uint8_t side_face;                /* SIDE_FACE_* */
    int16_t side_ornament_view;       /* VIEW_WALL_* of the side face */
    int16_t front_ornament_view;      /* VIEW_WALL_* of the front face, or -1 */
    int16_t pit, invisible_pit;       /* pit graphics; invisible_pit -1: an invisible pit is not drawn */
    int16_t pit_zone;                 /* zone of the pit */
    uint16_t cells, side_cells;       /* cell orders; side_cells 0: a door or stairs side draws nothing */
    int16_t stairs_side;              /* stairs side graphic (D1: the up graphic), or -1 */
    int16_t stairs_side_zone;         /* zone of stairs_side */
    int16_t stairs_side_down;         /* D1: the down stairs side graphic, or -1 */
    int16_t stairs_side_down_zone;    /* zone of stairs_side_down */
    int16_t stairs_front;             /* STAIRS_FRONT_*, or the up layout id with SQ_STAIRS_LAYOUT_IDS */
    int16_t stairs_front_down;        /* the down layout id with SQ_STAIRS_LAYOUT_IDS */
    int16_t stairs_front_zone;        /* zone of the up stairs front */
    uint16_t door_back, door_front;   /* cell orders behind and in front of a door */
    WallPart frames[DOOR_FRAMES_MAX]; /* door frames of a door front */
    uint8_t frame_count;
    int16_t button;                /* BUTTON_* of a door button, or -1 */
    int16_t ceiling, ceiling_zone; /* GFX_CEILING_PIT_* and its zone, or -1 */
    uint8_t flags;                 /* SQ_* */
} SquareView;

/* The 16 squares that the view draws, back to front. Right squares mirror the graphics of the left squares. */
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
     .stairs_side_down = -1,
     .stairs_front = LAYOUT_STAIRS_UP_FRONT_D3L2,
     .stairs_front_down = LAYOUT_STAIRS_DOWN_FRONT_D3L2,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3L2,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .button = -1,
     .ceiling = -1,
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
     .stairs_side_down = -1,
     .stairs_front = LAYOUT_STAIRS_UP_FRONT_D3R2,
     .stairs_front_down = LAYOUT_STAIRS_DOWN_FRONT_D3R2,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3R2,
     .door_back = DOOR_BACK_BR_BL,
     .door_front = DOOR_FRONT_FR_FL,
     .button = -1,
     .ceiling = -1,
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
     .stairs_side_down = -1,
     .stairs_front = STAIRS_FRONT_D3L,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3L,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_LEFT_D3L, ZONE_DOOR_FRAME_LEFT_D3L, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_RIGHT_D3L, ZONE_DOOR_FRAME_RIGHT_D3L, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = -1,
     .ceiling = -1},
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
     .stairs_side_down = -1,
     .stairs_front = STAIRS_FRONT_D3L,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3R,
     .door_back = DOOR_BACK_BR_BL,
     .door_front = DOOR_FRONT_FR_FL,
     .frames = {{WS_DOOR_FRAME_LEFT_D3R, ZONE_DOOR_FRAME_LEFT_D3R, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_LEFT_D3L, ZONE_DOOR_FRAME_RIGHT_D3R, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = BUTTON_D3R,
     .ceiling = -1},
    {.forward = 3,
     .right = 0,
     .shape = SHAPE_FULL,
     .mirror = BLIT_TRANSPARENT,
     .view = VIEW_D3C,
     .view_square = VIEW_SQUARE_D3C,
     .wall = {WS_WALL_D3C, ZONE_WALL_D3C, BLIT_OPAQUE},
     .side_face = SIDE_FACE_NONE,
     .front_ornament_view = VIEW_WALL_D3C_FRONT,
     .pit = GFX_PIT_D3C,
     .invisible_pit = -1,
     .pit_zone = ZONE_PIT_D3C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_down = -1,
     .stairs_front = STAIRS_FRONT_D3C,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D3C,
     .door_back = DOOR_BACK_BL_BR,
     .door_front = DOOR_FRONT_FL_FR,
     .frames = {{WS_DOOR_FRAME_D3C, ZONE_DOOR_FRAME_LEFT_D3C, BLIT_TRANSPARENT},
                {WS_DOOR_FRAME_D3C, ZONE_DOOR_FRAME_RIGHT_D3C, BLIT_FLIPPED}},
     .frame_count = 2,
     .button = BUTTON_D3C,
     .ceiling = -1},
    {.forward = 2,
     .right = -2,
     .shape = SHAPE_WALL_OR_FIELD,
     .view_square = VIEW_SQUARE_D2L2,
     .wall = {WS_WALL_D2L2, ZONE_WALL_D2L2, BLIT_TRANSPARENT}},
    {.forward = 2,
     .right = 2,
     .shape = SHAPE_WALL_OR_FIELD,
     .view_square = VIEW_SQUARE_D2R2,
     .wall = {WS_WALL_D2R2, ZONE_WALL_D2R2, BLIT_TRANSPARENT}},
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
     .stairs_front = STAIRS_FRONT_D2L,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2L,
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
     .stairs_front = STAIRS_FRONT_D2L,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2R,
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
     .front_ornament_view = VIEW_WALL_D2C_FRONT,
     .pit = GFX_PIT_D2C,
     .invisible_pit = GFX_INVISIBLE_PIT_D2C,
     .pit_zone = ZONE_PIT_D2C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_down = -1,
     .stairs_front = STAIRS_FRONT_D2C,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D2C,
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
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1L,
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
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1R,
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
     .front_ornament_view = VIEW_WALL_D1C_FRONT,
     .pit = GFX_PIT_D1C,
     .invisible_pit = GFX_INVISIBLE_PIT_D1C,
     .pit_zone = ZONE_PIT_D1C,
     .cells = CELLS_BL_BR_FL_FR,
     .side_cells = 0,
     .stairs_side = -1,
     .stairs_side_down = -1,
     .stairs_front = STAIRS_FRONT_D1C,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D1C,
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
     .view_square = VIEW_SQUARE_D0L,
     .wall = {WS_WALL_D0L, ZONE_WALL_D0L, BLIT_TRANSPARENT},
     .pit = GFX_PIT_D0L,
     .invisible_pit = GFX_INVISIBLE_PIT_D0L,
     .pit_zone = ZONE_PIT_D0L,
     .cells = CELLS_BR,
     .stairs_side = STAIRS_SIDE_D0L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D0L,
     .stairs_side_down = -1,
     .ceiling = GFX_CEILING_PIT_D0L,
     .ceiling_zone = ZONE_CEILING_PIT_D0L},
    {.forward = 0,
     .right = 1,
     .shape = SHAPE_D0_SIDE,
     .mirror = BLIT_FLIPPED,
     .view_square = VIEW_SQUARE_D0R,
     .wall = {WS_WALL_D0R, ZONE_WALL_D0R, BLIT_TRANSPARENT},
     .pit = GFX_PIT_D0L,
     .invisible_pit = GFX_INVISIBLE_PIT_D0L,
     .pit_zone = ZONE_PIT_D0R,
     .cells = CELLS_BL,
     .stairs_side = STAIRS_SIDE_D0L,
     .stairs_side_zone = ZONE_STAIRS_SIDE_D0R,
     .stairs_side_down = -1,
     .ceiling = GFX_CEILING_PIT_D0L,
     .ceiling_zone = ZONE_CEILING_PIT_D0R},
    {.forward = 0,
     .right = 0,
     .shape = SHAPE_D0C,
     .mirror = BLIT_TRANSPARENT,
     .view_square = VIEW_SQUARE_D0C,
     .wall = {.zone = ZONE_WALL_D0C},
     .pit = GFX_PIT_D0C,
     .invisible_pit = GFX_INVISIBLE_PIT_D0C,
     .pit_zone = ZONE_PIT_D0C,
     .cells = CELLS_BL_BR,
     .stairs_front = STAIRS_FRONT_D0C,
     .stairs_front_zone = ZONE_STAIRS_UP_FRONT_D0C_LEFT,
     .frames = {{WS_DOOR_FRAME_D0C, ZONE_DOOR_FRAME_D0C, BLIT_TRANSPARENT}},
     .frame_count = 1,
     .ceiling = GFX_CEILING_PIT_D0C,
     .ceiling_zone = ZONE_CEILING_PIT_D0C},
};

/* A wall set bitmap of a square. */
static void draw_wall_part(const WallPart *part)
{
    draw_wall_bitmap(view.wall_set_ids[part->ws], part->zone, part->mode);
}

/* The pit of a square, open or invisible. */
static void draw_square_pit(const SquareView *s, bool invisible)
{
    if (s->invisible_pit >= 0 || !invisible)
        draw_wall_bitmap(invisible ? s->invisible_pit : s->pit, s->pit_zone, s->mirror);
}

/* The side of a staircase beside the party's line: on D1 the up or the down graphic. */
static void draw_square_stairs_side(const SquareView *s, bool up)
{
    if (s->stairs_side_down >= 0 && !up)
        draw_wall_bitmap(view.stairs + s->stairs_side_down, s->stairs_side_down_zone, s->mirror);
    else if (s->stairs_side >= 0)
        draw_wall_bitmap(view.stairs + s->stairs_side, s->stairs_side_zone, s->mirror);
}

/* The hole of an open pit above the square, if the square shows one. */
static void draw_square_ceiling_pit(const SquareView *s, int16_t x, int16_t y)
{
    if (s->ceiling >= 0)
        draw_ceiling_pit(s->ceiling, s->ceiling_zone, x, y, s->mirror);
}

/* A wall square: the wall, the ornaments of its visible faces, and the things in an alcove. */
static void draw_square_wall(const SquareView *s, const Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    if (s->flags & SQ_FRONT) { /* ponytail: no thieves eye hole yet, needs the party spell state (step 9) */
        g_facing_alcove = false;
        g_facing_vi_altar = false;
        g_facing_fountain = false;
    }
    draw_wall_part(&s->wall);
    if (s->side_face != SIDE_FACE_NONE)
        draw_wall_ornament(s->side_face == SIDE_FACE_RIGHT ? a->side[SIDE_RIGHT] : a->side[SIDE_LEFT],
                           s->side_ornament_view, a);
    if (s->front_ornament_view < 0 || !draw_wall_ornament(a->side[SIDE_FRONT], s->front_ornament_view, a))
        return;
    if (s->flags & SQ_CEILING_IN_ALCOVE)
        draw_square_ceiling_pit(s, x, y);
    draw_square_things(a->first_thing, dir, x, y, s->view_square, CELLS_ALCOVE);
}

/* A door seen from the front: the things behind the door, the frames and the button, the door, and the things
   in front of it. */
static void draw_square_door_front(const SquareView *s, const Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    draw_floor_ornament(a->floor_ornament, s->view);
    draw_square_things(a->first_thing, dir, x, y, s->view_square, s->door_back);
    for (int i = 0; i < s->frame_count; i++)
        draw_wall_part(&s->frames[i]);
    if (s->button >= 0 && rec_door_has_button(a->door))
        draw_door_button(s->button);
    draw_door(a->door, a->state, s->view);
    if (s->flags & SQ_CEILING_AFTER_DOOR)
        draw_square_ceiling_pit(s, x, y);
    draw_square_things(a->first_thing, dir, x, y, s->view_square, s->door_front);
}

/* The front of a staircase, up or down. */
static void draw_square_stairs_front(const SquareView *s, bool up)
{
    if (s->flags & SQ_STAIRS_LAYOUT_IDS)
        draw_wall_bitmap(up ? s->stairs_front : s->stairs_front_down,
                         up ? s->stairs_front_zone : s->stairs_front_zone + ZONE_STAIRS_DOWN_DELTA, s->mirror);
    else
        draw_stairs_front(up, s->stairs_front, s->stairs_front_zone, s->mirror);
}

/* A D0 side square: the wall or the stairs side, else the pit, the ceiling pit, and the things. */
static void draw_d0_side_square(const SquareView *s, const Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    switch (a->element) {
    case SQUARE_WALL:
        draw_wall_part(&s->wall);
        return;
    case ELEMENT_STAIRS_SIDE:
        draw_square_stairs_side(s, a->state);
        return;
    case SQUARE_PIT:
        draw_square_pit(s, a->state); /* fall through */
        __attribute__((fallthrough));
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        draw_square_ceiling_pit(s, x, y);
        draw_square_things(a->first_thing, dir, x, y, s->view_square, s->cells);
    }
    if (a->element == SQUARE_TELEPORTER)
        draw_teleporter_field(s->view_square, s->wall.zone);
}

/* The square of the party: a door frame, the pit, or the stairs on both sides, then the ceiling pit and the
   things. */
static void draw_d0c_square(const SquareView *s, const Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    if (a->element == ELEMENT_DOOR_SIDE)
        draw_wall_part(&s->frames[0]);
    else if (a->element == SQUARE_PIT)
        draw_square_pit(s, a->state);
    else if (a->element == ELEMENT_STAIRS_FRONT) {
        draw_stairs_front(a->state, s->stairs_front, s->stairs_front_zone, BLIT_TRANSPARENT);
        draw_stairs_front(a->state, s->stairs_front, ZONE_STAIRS_UP_FRONT_D0C_RIGHT, BLIT_FLIPPED);
    }
    draw_square_ceiling_pit(s, x, y);
    draw_square_things(a->first_thing, dir, x, y, s->view_square, s->cells);
    if (a->element == SQUARE_TELEPORTER)
        draw_teleporter_field(s->view_square, s->wall.zone);
}

/* Draws one view square: its wall, pit, stairs, or door, then the floor ornament, the things, and a teleporter
   field. */
static void draw_square(const SquareView *s, int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    get_square_aspect(&a, dir, x, y);
    if (s->shape == SHAPE_WALL_OR_FIELD) {
        if (a.element == SQUARE_WALL)
            draw_wall_part(&s->wall);
        else if (a.element == SQUARE_TELEPORTER)
            draw_teleporter_field(s->view_square, s->wall.zone);
        return;
    }
    if (s->shape == SHAPE_D0_SIDE) {
        draw_d0_side_square(s, &a, dir, x, y);
        return;
    }
    if (s->shape == SHAPE_D0C) {
        draw_d0c_square(s, &a, dir, x, y);
        return;
    }
    if (s->flags & SQ_FRONT)
        g_square_ahead_element = a.element;
    uint16_t cells = s->cells;
    switch (a.element) {
    case SQUARE_WALL:
        draw_square_wall(s, &a, dir, x, y);
        return;
    case SQUARE_PIT:
        draw_square_pit(s, a.state);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        if (!s->side_cells)
            return;
        if (a.element == ELEMENT_STAIRS_SIDE)
            draw_square_stairs_side(s, a.state);
        cells = s->side_cells;
        break;
    case ELEMENT_DOOR_FRONT:
        draw_square_door_front(s, &a, dir, x, y);
        return;
    case ELEMENT_STAIRS_FRONT:
        draw_square_stairs_front(s, a.state);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    draw_floor_ornament(a.floor_ornament, s->view);
    draw_square_ceiling_pit(s, x, y);
    draw_square_things(a.first_thing, dir, x, y, s->view_square, cells);
    if (a.element == SQUARE_TELEPORTER)
        draw_teleporter_field(s->view_square, s->wall.zone);
}

/* Color 9 or 10 of the 6 light palettes and of the creature
   palette changes from replacement set n. */
#define COLOR_CREATURE_1      9
#define COLOR_CREATURE_2      10
#define REPLACEMENT_DEFAULT_1 8
#define REPLACEMENT_DEFAULT_2 12
#define REPLACEMENT_SET_MASK  0xf /* creature aspect byte: set of color 9, then of color 10 */
#define REPLACEMENT_SET_SHIFT 4

/* Puts replacement color set n of a creature type into color of the dungeon palettes and the D2 and D3 changes. */
static void set_creature_replacement_colors(int16_t color, int16_t n)
{
    for (int l = 0; l < LIGHT_LEVELS; l++) /* a color per light level, then the D2 and D3 color */
        view.dungeon_palettes[l][color] = tables_get_replacement_color(n, l);
    view.creature_palette_changes_d2[color] = tables_get_replacement_change(n, DEPTH_D2);
    view.creature_palette_changes_d3[color] = tables_get_replacement_change(n, DEPTH_D3);
}

static void reset_wall_set_ids(void)
{
    for (int i = 0; i < WS_COUNT; i++)
        view.wall_set_ids[i] = tables_get_wall_layout_id(i);
}

static int16_t get_wall_set_slot(int entry)
{
    return -1 - view.wall_set_ids[entry];
}

static void set_slot(int entry, int16_t graphic, bool flip)
{
    view.slot_graphics[get_wall_set_slot(entry)] = graphic;
    view.slot_flip[get_wall_set_slot(entry)] = flip;
}

/* load_floor_set and load_wall_set: the graphic of each layout bitmap slot, and the slots that hold a
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
    view.slot_graphics[-1 - LAYOUT_INSCRIPTION_WALL] = view.slot_graphics[get_wall_set_slot(WS_WALL_D1C)];
    for (int i = 0; i < WALL_COUNT; i++)
        set_slot(WS_MIRRORED_WALL_FIRST + i, view.slot_graphics[get_wall_set_slot(mirrored_walls[i])], true);
    view.stairs = first + WALLSET_STAIRS_FIRST;
    view.slot_graphics[-1 - LAYOUT_STAIRS_UP_FRONT_D3R2] = view.stairs + STAIRS_FRONT_D3L;
    view.slot_graphics[-1 - LAYOUT_STAIRS_UP_FRONT_D3L2] = view.slot_graphics[-1 - LAYOUT_STAIRS_UP_FRONT_D3R2];
    view.slot_graphics[-1 - LAYOUT_STAIRS_DOWN_FRONT_D3R2] = view.stairs + STAIRS_FRONT_D3L + STAIRS_DOWN;
    view.slot_graphics[-1 - LAYOUT_STAIRS_DOWN_FRONT_D3L2] = view.slot_graphics[-1 - LAYOUT_STAIRS_DOWN_FRONT_D3R2];
}

/* The door graphics and door info of the two door types of map m. */
static void load_doors(int16_t m)
{
    for (int type = 0; type < DOOR_TYPES; type++) {
        int16_t set = rec_map_door_set(m, type);
        view.door_type_attributes[type] = tables_get_door_attributes(set);
        for (int depth = 0; depth < DOOR_DEPTHS; depth++)
            view.door_graphics[depth][type] = GFX_DOOR_FIRST + DOOR_DEPTHS * set + depth;
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
    view.alcoves[0] = -1;
    view.alcoves[1] = -1;
    view.alcoves[2] = -1;
    view.vi_altar = -1;
    view.fountain = -1;
    view.inscription = walls; /* after the map ornaments: the inscription plaque, ornament 0 */
    for (int16_t i = 0; i <= walls; i++) {
        uint8_t ornament = i < walls ? ornaments[i] : 0;
        view.wall_ornaments[i] = (Ornament){GFX_WALL_ORNAMENT_FIRST + WALL_ORNAMENT_GRAPHICS * ornament,
                                            tables_get_wall_ornament_coords(ornament)};
        for (int k = 0; k < ALCOVE_ORNAMENT_COUNT; k++)
            if (tables_get_alcove_ornament(k) == ornament) {
                view.alcoves[alcove_count++] = i;
                if (ornament == GFX_ORNAMENT_WALL_VI_ALTAR)
                    view.vi_altar = i;
            }
        if (tables_get_fountain_ornament() == ornament)
            view.fountain = i;
    }
    for (int16_t i = 0; i < floors; i++) {
        uint8_t ornament = ornaments[walls + i];
        view.floor_ornaments[i] = (Ornament){GFX_FLOOR_ORNAMENT_FIRST + FLOOR_ORNAMENT_GRAPHICS * ornament,
                                             tables_get_floor_ornament_coords(ornament)};
    }
    view.floor_ornaments[FLOOR_ORNAMENT_FOOTPRINTS] = (Ornament){GFX_FOOTPRINTS, 0};
    for (int16_t i = 0; i < doors; i++) {
        uint8_t ornament = ornaments[walls + floors + i];
        view.door_ornaments[i] =
            (Ornament){GFX_DOOR_ORNAMENT_FIRST + ornament, tables_get_door_ornament_coords(ornament)};
    }
    view.door_ornaments[DOOR_ORNAMENT_DESTROYED] = (Ornament){GFX_DOOR_MASK_DESTROYED, DOOR_MASK_COORD_SET};
    view.door_ornaments[DOOR_ORNAMENT_THIEVES_EYE] = (Ornament){GFX_DOOR_MASK_THIEVES_EYE, DOOR_MASK_COORD_SET};
}

/* The light palettes and the creature palette changes with the replacement colors of the creature types of
   the map. */
static void load_creature_colors(const uint8_t *creature_types, int16_t count)
{
    for (int l = 0; l < LIGHT_LEVELS; l++)
        tables_get_palette(TABLES_PALETTE_DUNGEON + l, view.dungeon_palettes[l]);
    memcpy(view.creature_palette_changes_d3, tables_get_palette_changes(TABLES_CHANGES_CREATURE_D3),
           sizeof view.creature_palette_changes_d3);
    memcpy(view.creature_palette_changes_d2, tables_get_palette_changes(TABLES_CHANGES_CREATURE_D2),
           sizeof view.creature_palette_changes_d2);
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
    view.map = m;
    view.map_width = dungeon_map_width(m);
    view.map_height = dungeon_map_height(m);
    const uint8_t *creature_types = rec_map_type_lists(m); /* after the squares: creature types, then ornaments */
    int16_t creature_type_count = rec_map_creature_types(m);
    load_wall_set(rec_map_floor_set(m), rec_map_wall_set(m));
    load_doors(m);
    load_ornaments(m, creature_types + creature_type_count);
    load_creature_colors(creature_types, creature_type_count);
}

/* The square forward and right of (x, y) in direction dir. */
static void get_relative_square(int16_t dir, int16_t forward, int16_t right, int16_t *x, int16_t *y)
{
    *x += forward * tables_get_step_east(dir) + right * tables_get_step_east(direction_right(dir));
    *y += forward * tables_get_step_north(dir) + right * tables_get_step_north(direction_right(dir));
}

int16_t view_get_wall_ornament(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (view.map != g_party_map_index)
        return VIEW_ORNAMENT_NONE;
    get_square_aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || a.side[SIDE_FAR] < 0)
        return VIEW_ORNAMENT_NONE;
    int16_t i = a.side[SIDE_FAR];
    if (i == view.inscription)
        return VIEW_ORNAMENT_TEXT;
    if (i == view.vi_altar)
        return VIEW_ORNAMENT_VI_ALTAR;
    if (is_alcove(i))
        return VIEW_ORNAMENT_ALCOVE;
    if (i == view.fountain)
        return VIEW_ORNAMENT_FOUNTAIN;
    return VIEW_ORNAMENT_OTHER;
}

int16_t view_get_wall_face_ornament_index(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (view.map != g_party_map_index)
        return -1;
    get_square_aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || a.side[SIDE_FAR] < 0)
        return -1;
    return view_get_wall_ornament_index(a.side[SIDE_FAR]);
}

int16_t view_get_wall_ornament_index(int16_t ornament)
{
    if (view.map != g_party_map_index || ornament < 0 || ornament >= rec_map_wall_ornaments(view.map))
        return -1;
    return (view.wall_ornaments[ornament].graphic - GFX_WALL_ORNAMENT_FIRST) / WALL_ORNAMENT_GRAPHICS;
}

bool view_is_pressure_pad(int16_t x, int16_t y)
{
    int16_t ornament = view_get_floor_ornament(x, y);
    return ornament == GFX_ORNAMENT_FLOOR_PRESSURE_PAD || ornament == GFX_ORNAMENT_FLOOR_ROUND_PRESSURE_PAD ||
           ornament == GFX_ORNAMENT_FLOOR_TINY_PRESSURE_PAD;
}

int16_t view_get_floor_ornament(int16_t x, int16_t y)
{
    Aspect a;
    if (view.map != g_party_map_index)
        return -1;
    get_square_aspect(&a, DIRECTION_NORTH, x, y);
    if (a.element != SQUARE_CORRIDOR && a.element != SQUARE_PIT && a.element != SQUARE_TELEPORTER)
        return -1;
    int16_t i = a.floor_ornament;
    return i < 0 ? -1 : (view.floor_ornaments[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS;
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
static void draw_map_tile(Bitmap *dst, int16_t tile, int16_t px, int16_t py)
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
    switch (get_projectile_aspect(slot)) {
    case -PROJECTILE_ASPECT_FIREBALL:
        return MAP_TILE_FIREBALL_FIRST;
    case -PROJECTILE_ASPECT_SPELL:
        return MAP_TILE_SPELL_FIRST;
    case -PROJECTILE_ASPECT_POISON:
        return MAP_TILE_POISON_FIRST;
    case -PROJECTILE_ASPECT_SLIME:
        return MAP_TILE_SLIME_FIRST;
    case -PROJECTILE_ASPECT_LIGHTNING: {
        return MAP_TILE_LIGHTNING_FIRST +
               ((projectile_get_flight_direction(projectile) & 1) != (dir & 1) ? MAP_TILE_LIGHTNING_ACROSS : 0);
    }
    }
    return get_projectile_aspect(slot) >= 0 && object_get_type(slot) == OBJECT_TYPE_ZOKATHRA ? MAP_TILE_ZOKATHRA_FIRST
                                                                                             : -1;
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

/* The things of a square that the spells show at (px, py): with DAIN the zokathra objects, the projectiles,
   and the explosions, with KU the creatures. blink: the square is a fake wall that blinks. */
static void draw_map_square_things(Bitmap *dst, const uint8_t *spells, const Aspect *aspect, bool blink, int16_t dir,
                                   int16_t px, int16_t py)
{
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
            draw_map_tile(dst,
                          MAP_TILE_ZOKATHRA_FIRST + NORMALIZE(THING_CELL(thing) - dir) +
                              (aspect->element == SQUARE_WALL && !blink ? MAP_TILE_IN_WALL : 0),
                          px, py);
    }
    if (group != THING_ENDOFLIST && !(map_get_creature_attributes(group) & CREATURE_NOT_ON_MAP))
        draw_map_tile(dst, MAP_TILE_CREATURE_FIRST + (g_game_time & 1), px, py);
    for (uint16_t thing = projectile; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        int16_t tile = THING_TYPE(thing) == THING_PROJECTILE ? get_map_projectile_tile(thing, dir) : -1;
        if (tile >= 0)
            draw_map_tile(dst, tile + NORMALIZE(THING_CELL(thing) - dir), px, py);
    }
    for (uint16_t thing = explosion; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        int16_t tile = THING_TYPE(thing) == THING_EXPLOSION ? get_map_explosion_tile(thing) : -1;
        if (tile >= 0)
            draw_map_tile(dst, tile, px, py);
    }
}

/* A wall on the magic map: blinking when ROS shows a fake wall, else the wall with its vi altar, alcove, and
   fountain sides. */
static void draw_map_wall(Bitmap *dst, const uint8_t *spells, const Aspect *aspect, bool blink, int16_t px, int16_t py)
{
    if (spells[MAGIC_MAP_ROS] && blink && (g_game_time & MAP_TIME_BLINK_WALL)) {
        draw_map_tile(dst, MAP_TILE_FAKE_WALL_BLINK, px, py);
        return;
    }
    draw_map_tile(dst, MAP_TILE_WALL, px, py);
    for (int16_t k = 0; k < WALL_SIDES; k++) {
        int16_t i = aspect->side[k];
        int16_t first = i < 0                ? -1
                        : i == view.vi_altar ? MAP_TILE_VI_ALTAR_FIRST
                        : is_alcove(i)       ? MAP_TILE_ALCOVE_FIRST
                        : i == view.fountain ? MAP_TILE_FOUNTAIN_FIRST
                                             : -1;
        if (first >= 0)
            draw_map_tile(dst, first + k, px, py);
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

/* The tiles of square (x, y) seen in direction dir at (px, py): the square, then with DAIN the footprints, the
   zokathra objects, the projectiles, and the explosions, with ROS the fake walls and invisible pits, with KU the
   creatures. */
static void draw_map_square(Bitmap *dst, const uint8_t *spells, int16_t x, int16_t y, int16_t dir, int16_t px,
                            int16_t py)
{
    Aspect aspect;
    get_square_aspect(&aspect, dir, x, y);
    bool blink = rec_square_type(aspect.square) == SQUARE_FAKE_WALL && (aspect.square & FAKE_WALL_IMAGINARY);
    int16_t color = x == g_party_map_x && y == g_party_map_y ? MAP_COLOR_PARTY
                    : spells[MAGIC_MAP_DAIN] && aspect.element != SQUARE_WALL && has_map_footprints(&aspect, x, y)
                        ? MAP_COLOR_FOOTPRINTS
                        : -1;
    if (color >= 0) {
        Box b = {px - MAP_GAP_X, py - MAP_GAP_Y, MAP_TILE_W + 2 * MAP_GAP_X, MAP_TILE_H + 2 * MAP_GAP_Y};
        draw_fill(dst, &b, (uint8_t)color);
    }
    switch (aspect.element) {
    case SQUARE_WALL:
        draw_map_wall(dst, spells, &aspect, blink, px, py);
        break;
    case SQUARE_CORRIDOR: {
        draw_map_tile(dst, MAP_TILE_FLOOR, px, py);
        int16_t i = aspect.floor_ornament;
        if (i >= 0)
            draw_map_tile(dst,
                          MAP_TILE_FLOOR_ORNAMENT_FIRST +
                              (view.floor_ornaments[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS,
                          px, py);
        break;
    }
    case SQUARE_TELEPORTER:
        draw_map_tile(dst, MAP_TILE_TELEPORTER_FIRST + random_2bits(), px, py);
        break;
    case ELEMENT_DOOR_FRONT:
    case ELEMENT_DOOR_SIDE:
        draw_map_tile(dst, get_map_door_tile(&aspect), px, py);
        break;
    case ELEMENT_STAIRS_SIDE:
    case ELEMENT_STAIRS_FRONT: {
        int16_t side = NORMALIZE(map_get_stairs_exit_direction(x, y) - dir);
        if (aspect.state)
            side = direction_back(side);
        draw_map_tile(dst, MAP_TILE_STAIRS_FIRST + side, px, py);
        break;
    }
    case SQUARE_PIT:
        draw_map_tile(dst,
                      !aspect.state                                                 ? MAP_TILE_PIT
                      : spells[MAGIC_MAP_ROS] && (g_game_time & MAP_TIME_BLINK_PIT) ? MAP_TILE_PIT_BLINK
                                                                                    : MAP_TILE_PIT_INVISIBLE,
                      px, py);
        break;
    }
    if (spells[MAGIC_MAP_KU] || spells[MAGIC_MAP_DAIN])
        draw_map_square_things(dst, spells, &aspect, blink, dir, px, py);
}

/* The magic map of a champion at (px, py) of dst: 7 x 7 squares around the party, or around the square that
   NETA keeps. The party square is drawn last. The ornaments are those of the party map, as in the original. */
void view_draw_magic_map(Bitmap *dst, const uint8_t *spells, int16_t px, int16_t py)
{
    int16_t map = g_party_map_index;
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    int16_t dir = g_party_direction;
    int16_t saved_map = g_current_map_index;
    int16_t party_px = -1;
    int16_t party_py = 0;
    if (view.map != g_party_map_index)
        view_load_map(g_party_map_index);
    int16_t saved[3] = {view.map, view.map_width, view.map_height};
    if (spells[MAGIC_MAP_NETA]) {
        Position kept = magic_get_map_position(spells);
        map = kept.map;
        x = kept.x;
        y = kept.y;
        dir = spells[MAGIC_MAP_NETA] - 1;
    }
    map_set_current_map(map);
    view.map = map;
    view.map_width = dungeon_map_width(map);
    view.map_height = dungeon_map_height(map);
    get_relative_square(dir, MAP_HALF, -MAP_HALF, &x, &y);
    for (int16_t row = 0; row < MAP_SQUARES; row++, py += MAP_TILE_H + MAP_GAP_Y) {
        int16_t sx = x;
        int16_t sy = y;
        int16_t cx = px;
        for (int16_t col = 0; col < MAP_SQUARES; col++, cx += MAP_TILE_W + MAP_GAP_X) {
            if (map == g_party_map_index && sx == g_party_map_x && sy == g_party_map_y) {
                party_px = cx;
                party_py = py;
            } else {
                draw_map_square(dst, spells, sx, sy, dir, cx, py);
            }
            get_relative_square(dir, 0, 1, &sx, &sy);
        }
        get_relative_square(dir, -1, 0, &x, &y);
    }
    if (party_px >= 0)
        draw_map_square(dst, spells, g_party_map_x, g_party_map_y, dir, party_px, party_py);
    view.map = saved[0];
    view.map_width = saved[1];
    view.map_height = saved[2];
    if (saved_map >= 0)
        map_set_current_map(saved_map);
}

void view_draw(int16_t dir, int16_t x, int16_t y)
{
    static const int8_t d4_right[D4_SQUARE_COUNT] = {-1, 1, 0};
    static const int16_t d4_squares[D4_SQUARE_COUNT] = {VIEW_SQUARE_D4L, VIEW_SQUARE_D4R, VIEW_SQUARE_D4C};
    static uint8_t px[VIEWPORT_MAX_H * VIEWPORT_MAX_W];
    view.viewport = (Bitmap){tables_get_viewport_width(), tables_get_viewport_height(), px};
    if (view.map < 0 || (size_t)view.viewport.w * view.viewport.h > sizeof px)
        return;
    memset(px, 0, sizeof px);
    if (!flight.motion_steps)
        start_tracks(dir, x, y);
    for (int i = 0; i < VIEW_CLICK_BOX_COUNT; i++)
        g_dungeon_view_clickable_boxes[i].x = -1;
    memset(g_pile_object_counts, 0, sizeof g_pile_object_counts);
    reset_wall_set_ids();
    view.use_flipped_walls = (dir + x + y) & 1;
    if (view.use_flipped_walls) { /* the mirrored-walls memory option is on, as on a 1 MB Amiga */
        memcpy(&view.wall_set_ids[WS_WALL_FIRST], &view.wall_set_ids[WS_MIRRORED_WALL_FIRST],
               WALL_COUNT * sizeof view.wall_set_ids[0]);
        draw_wall_bitmap(view.wall_set_ids[WS_CEILING], ZONE_CEILING, BLIT_OPAQUE);
        draw_wall_bitmap(view.wall_set_ids[WS_FLOOR], ZONE_FLOOR, BLIT_FLIPPED);
    } else {
        draw_wall_bitmap(view.wall_set_ids[WS_CEILING], ZONE_CEILING, BLIT_FLIPPED);
        draw_wall_bitmap(view.wall_set_ids[WS_FLOOR], ZONE_FLOOR, BLIT_OPAQUE);
    }
    for (int i = 0; i < D4_SQUARE_COUNT; i++) { /* objects only on D4L, D4R, D4C */
        int16_t sx = x;
        int16_t sy = y;
        get_relative_square(dir, DEPTH_D4, d4_right[i], &sx, &sy);
        draw_square_things(map_get_first_thing_at((Position){view.map, sx, sy}), dir, sx, sy, d4_squares[i], CELLS_BL);
    }
    for (int i = 0; i < DRAWN_SQUARE_COUNT; i++) {
        int16_t sx = x;
        int16_t sy = y;
        get_relative_square(dir, square_views[i].forward, square_views[i].right, &sx, &sy);
        draw_square(&square_views[i], dir, sx, sy);
    }
    Bitmap screen = {SCREEN_W, SCREEN_H, &fb[0][0]};
    int16_t width = view.viewport.w;
    int16_t height = view.viewport.h;
    Box box;
    if (layout_get_zone(ZONE_VIEWPORT, view.viewport.w, view.viewport.h, &box, &width, &height))
        draw_blit(&view.viewport, width, height, &screen, &box, -1, 0);
}

bool view_render(int16_t m, int16_t x, int16_t y, int16_t dir)
{
    if (!g_dungeon_maps || m < 0 || m >= dungeon_map_count())
        return false;
    if (m != view.map)
        view_load_map(m);
    int16_t light = g_session.cheat_light ? 0 : g_party_light_level;
    memcpy(palette, view.dungeon_palettes[light], sizeof view.dungeon_palettes[light]);
    view_draw(NORMALIZE(dir), x, y);
    return true;
}
