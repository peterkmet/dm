/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_VIEW_INTERNAL_H
#define DM_UI_VIEW_INTERNAL_H
/* Prefixes: view_ */
/* Private declarations shared by view.c, view_walls.c and view_things.c. */
#include "data/graphics.h"
#include "engine/map.h"
#include "ui/video.h"
#include <stdbool.h>
#include <stdint.h>

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

#define BLIT_TRANSPARENT          0 /* wall set bitmap draw modes: with transparent color, opaque, mirrored */
#define BLIT_OPAQUE               1
#define BLIT_FLIPPED              2
#define TRANSPARENT_COLOR         10
#define DOOR_ORNAMENT_TRANSPARENT 9
#define PORTRAIT_TRANSPARENT      1

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

/* Cell orders of the things on a square (objects, creatures, projectiles, explosions): a nibble per view cell + 1,
   low nibble first; 0 is the alcove. Bit 3 marks a door pass, bit 0 then selects the pass behind (0) or in front of
   the door (1), and the next nibbles hold the cells. B back, F front, L left, R right. */
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

/* A wall set bitmap: a graphic, or a layout slot of the wall set. */
typedef struct {
    int16_t graphic; /* graphic id, -1 when slot is set */
    int16_t slot;    /* LAYOUT_* slot, -1 when graphic is set */
} WallBitmap;

/* The aspect of a thing in flight: an object aspect, or a projectile aspect. */
typedef struct {
    bool projectile;
    int16_t index;
} ProjectileAspect;

/* A bitmap at a scale: a cached graphic, or an owned copy. */
typedef struct {
    const Bitmap *bitmap; /* the cached graphic, NULL when it is missing or own holds the scaled copy */
    Bitmap own;           /* own.px owns the scaled pixels, NULL when bitmap is the cached graphic */
} ScaledBitmap;

/* The state of the dungeon view: the loaded map, its graphics, and the palettes. */
typedef struct {
    Bitmap viewport;                     /* the dungeon view bitmap */
    int16_t map, map_width, map_height;  /* the loaded map, -1 none, and its size */
    WallBitmap wall_set[WS_COUNT];       /* bitmaps of the wall set, swapped for mirrored views */
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

/* Where view_draw_in_zone puts a bitmap: the relative position that a ZONE_SHIFTED or ZONE_SIZED zone passes to
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

#define DEPTH_D0            0
#define DEPTH_D1            1
#define DEPTH_D2            2
#define DEPTH_D3            3
#define DEPTH_D4            4
/* scales are in 1/32 */
#define SCALE_D3            14
#define SCALE_D2            21
#define SCALE_NATIVE        32
#define SCALE_EXPLOSION_MIN 4
#define SCALE_SHIFT         5

/* Creature aspect record (6 bytes): graphic word, coordinate set and transparent color, replacement color sets. */
#define CREATURE_ASPECT_GRAPHIC     0
#define CREATURE_ASPECT_COORD       4
#define CREATURE_ASPECT_REPLACEMENT 5
#define CREATURE_ASPECT_TRANSPARENT 0xf
#define CREATURE_ASPECT_COORD_SHIFT 4

/* Projectile aspects of the spell projectiles. */
#define PROJECTILE_ASPECT_LIGHTNING 4
#define PROJECTILE_ASPECT_FIREBALL  11
#define PROJECTILE_ASPECT_SPELL     12
#define PROJECTILE_ASPECT_SLIME     13
#define PROJECTILE_ASPECT_POISON    14

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

/* One wall set bitmap: wall_set index, zone, and BLIT_* mode. */
typedef struct {
    int16_t ws, zone;
    uint8_t mode;
} WallPart;

/* The position of a drawn square from the party and the graphics and zones of each element on it. */
typedef struct {
    int8_t forward, right;             /* position from the party */
    uint8_t shape;                     /* SHAPE_* */
    uint8_t mirror;                    /* BLIT_TRANSPARENT or BLIT_FLIPPED: pit, stairs, and ceiling pit */
    int16_t view, view_square;         /* VIEW_D* of the floor ornament and the door; VIEW_SQUARE_D* of things */
    WallPart wall;                     /* the wall; its zone is also the zone of a teleporter field */
    uint8_t side_face;                 /* SIDE_FACE_* */
    int16_t side_ornament_view;        /* VIEW_WALL_* of the side face */
    int16_t front_ornament_view;       /* VIEW_WALL_* of the front face, or -1 */
    int16_t pit, invisible_pit;        /* pit graphics; invisible_pit -1: an invisible pit is not drawn */
    int16_t pit_zone;                  /* zone of the pit */
    uint16_t cells, side_cells;        /* cell orders; side_cells 0: a door or stairs side draws nothing */
    int16_t stairs_side;               /* stairs side graphic (D1: the up graphic), or -1 */
    int16_t stairs_side_zone;          /* zone of stairs_side */
    int16_t stairs_side_down;          /* D1: the down stairs side graphic, or -1 */
    int16_t stairs_side_down_zone;     /* zone of stairs_side_down */
    int16_t stairs_front;              /* STAIRS_FRONT_*, or the up layout id with SQ_STAIRS_LAYOUT_IDS */
    int16_t stairs_front_down;         /* the down layout id with SQ_STAIRS_LAYOUT_IDS */
    int16_t stairs_front_zone;         /* zone of the up stairs front */
    int16_t stairs_front_flipped_zone; /* D0C: zone of the mirrored up stairs front, else -1 */
    uint16_t door_back, door_front;    /* cell orders behind and in front of a door */
    WallPart frames[DOOR_FRAMES_MAX];  /* door frames of a door front */
    uint8_t frame_count;
    int16_t button;                /* BUTTON_* of a door button, or -1 */
    int16_t ceiling, ceiling_zone; /* GFX_CEILING_PIT_* and its zone, or -1 */
    uint8_t flags;                 /* SQ_* */
} SquareView;

extern View g_view;

/* view.c: shared drawing primitives */
/* True when wall ornament i of the map is an alcove. */
bool view_is_alcove(int16_t i);
/* The wall set bitmap of a graphic (id >= 0) or of a LAYOUT_* id (id < 0). */
WallBitmap view_get_wall_bitmap(int16_t id);
/* Draws a wall set bitmap, opaque or mirrored as mode says: a graphic or a layout bitmap slot at zone. */
void view_draw_wall_bitmap(WallBitmap bitmap, int16_t zone, int16_t mode);
/* Draws bm into zone of dst. Returns true when it drew; drawn (NULL: not wanted) gets the drawn box.
   ZONE_SHIFTED or ZONE_SIZED pass the zone relative position to layout_get_zone: a shift, or the item size. */
bool view_draw_in_zone(const Bitmap *bm, Bitmap *dst, const ZoneBlit *blit, Box *drawn);
/* The bitmap of graphic g at scale_x/32 by scale_y/32 with palette changes; the native bitmap at full scale
   without changes. Mirroring happens at blit time. */
ScaledBitmap view_get_scaled(int16_t graphic, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes);
/* The bitmap to draw: the scaled copy, else the cached graphic; NULL when the graphic is missing. */
const Bitmap *view_get_scaled_bitmap(const ScaledBitmap *scaled);
/* Frees the scaled copy of scaled, if it owns one. */
void view_free_scaled(ScaledBitmap *scaled);

/* view_walls.c */
/* The floor ornament i on a floor view. */
void view_draw_floor_ornament(int16_t i, int16_t floor_view);
/* A wall set bitmap of a square. */
void view_draw_wall_part(const WallPart *part);
/* The pit of a square, open or invisible. */
void view_draw_square_pit(const SquareView *square, bool invisible);
/* The side of a staircase beside the party's line: on D1 the up or the down graphic. */
void view_draw_stairs_side(const SquareView *square, bool up);
/* The hole of an open pit above the square, if the square shows one. */
void view_draw_ceiling_pit(const SquareView *square, Location at);
/* A wall square: the wall, the ornaments of its visible faces, and the things in an alcove. */
void view_draw_square_wall(const SquareView *square, const Aspect *a, int16_t dir, Location at);
/* A door seen from the front: the things behind the door, the frames and the button, the door, and the things
   in front of it. */
void view_draw_door_front(const SquareView *square, const Aspect *a, int16_t dir, Location at);
/* The front of a staircase, up or down. */
void view_draw_stairs_front(const SquareView *square, bool up);
/* The front of the staircase on the party square: the graphic and its mirror. */
void view_draw_d0c_stairs(const SquareView *square, bool up);

/* view_things.c */
/* The object aspect of a flying thing, or its projectile aspect. */
ProjectileAspect view_get_projectile_aspect(uint16_t thing);
/* The things of one view square: per cell in cell order (nibbles of cell + 1, low first; 0 = the
   alcove; bit 3 = door pass, bit 0 the front pass) the objects, creatures, and projectiles, then the
   explosions of the square. */
void view_draw_square_things(uint16_t first, int16_t dir, Location at, int16_t view_square, uint16_t order);
/* The field of a teleporter square. */
void view_draw_teleporter_field(int16_t view_square, int16_t zone);
/* A tick view starts new tracks; the old tracks stay when the party still looks from the same place. */
void view_start_tracks(int16_t dir, Location at);

#endif
