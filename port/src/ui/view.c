#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "advanced.h"
#include "draw.h"
#include "dungeon.h"
#include "game.h"
#include "group.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "projectile.h"
#include "tables.h"
#include "graphics.h"
#include "layout.h"
#include "random.h"
#include "recruit.h"
#include "video.h"
#include "view.h"

#define ORNAMENT_VI_ALTAR 2 /* the alcove ornament that is a vi altar */
/* tables_pile_shift pair indices */
enum { PILE_FLOOR_SHIFTS = 16, PILE_ODD_CELL = 8, PILE_ALCOVE_FIRST = 2, PILE_ALCOVE_LAST = 13 };

enum { LAYOUT_SLOTS = 64 };

enum { WALL_COUNT = 15 };
enum { WALL_SIDES = 4 }; /* the sides of a wall square */

/* Entries of g_wall_set_layout_ids: layout bitmap ids of the floor set, the walls, the door frames, and the
   mirrored walls. The right square of a pair draws the mirrored frame of the left square. */
enum {
    WS_FLOOR,
    WS_CEILING,
    WS_WALL_D0R,
    WS_WALL_D0L,
    WS_WALL_D1R,
    WS_WALL_D1L,
    WS_WALL_D1C,
    WS_WALL_D2R2,
    WS_WALL_D2L2,
    WS_WALL_D2R,
    WS_WALL_D2L,
    WS_WALL_D2C,
    WS_WALL_D3R2,
    WS_WALL_D3L2,
    WS_WALL_D3R,
    WS_WALL_D3L,
    WS_WALL_D3C,
    WS_DOOR_FRAME_TOP_D1R,
    WS_DOOR_FRAME_TOP_D1L,
    WS_DOOR_FRAME_TOP_D1C,
    WS_DOOR_FRAME_TOP_D2R,
    WS_DOOR_FRAME_TOP_D2L,
    WS_DOOR_FRAME_TOP_D2C,
    WS_DOOR_FRAME_D0C,
    WS_DOOR_FRAME_RIGHT_D1C,
    WS_DOOR_FRAME_LEFT_D1C,
    WS_DOOR_FRAME_D2C,
    WS_DOOR_FRAME_D3C,
    WS_DOOR_FRAME_LEFT_D3L,
    WS_DOOR_FRAME_RIGHT_D3L,
    WS_DOOR_FRAME_LEFT_D3R,
    WS_MIRRORED_WALL_FIRST,
    WS_COUNT = WS_MIRRORED_WALL_FIRST + WALL_COUNT,
};
enum { WS_WALL_FIRST = WS_WALL_D0R };

/* Graphics of a wall set, from GFX_WALLSET_FIRST + GFX_WALLSET_SIZE * wall set. */
enum {
    WALLSET_DOOR_FRAME_D0C,
    WALLSET_DOOR_FRAME_D1C,
    WALLSET_DOOR_FRAME_D2C,
    WALLSET_DOOR_FRAME_D3C,
    WALLSET_DOOR_FRAME_D3_SIDE,
    WALLSET_DOOR_FRAME_TOP_D1,
    WALLSET_DOOR_FRAME_TOP_D2,
    WALLSET_WALL_FIRST,
    WALLSET_STAIRS_FIRST = WALLSET_WALL_FIRST + WALL_COUNT,
};

/* Stairs graphics, from WALLSET_STAIRS_FIRST. Right squares mirror the left graphic. */
enum {
    STAIRS_FRONT_D3L,
    STAIRS_FRONT_D3C,
    STAIRS_FRONT_D2L,
    STAIRS_FRONT_D2C,
    STAIRS_FRONT_D1L,
    STAIRS_FRONT_D1C,
    STAIRS_FRONT_D0C,
    STAIRS_DOWN, /* down stairs front graphic = up stairs front graphic + STAIRS_DOWN */
    STAIRS_SIDE_D2L = 14,
    STAIRS_UP_SIDE_D1L,
    STAIRS_DOWN_SIDE_D1L,
    STAIRS_SIDE_D0L,
};

enum { BLIT_TRANSPARENT, BLIT_OPAQUE, BLIT_FLIPPED }; /* draw_wall_set_bitmap variants */
enum { TRANSPARENT_COLOR = 10, DOOR_ORNAMENT_TRANSPARENT = 9, PORTRAIT_TRANSPARENT = 1 };

/* View elements: the square types, and doors and stairs seen from the side or from the front. */
enum {
    ELEMENT_DOOR_SIDE = 0x10,
    ELEMENT_DOOR_FRONT = VIEW_ELEMENT_DOOR_FRONT,
    ELEMENT_STAIRS_SIDE,
    ELEMENT_STAIRS_FRONT
};

/* random ornament allowed on that side: the north face is the bit of the ornament below on the map */
#define WALL_ORNAMENT_FACE(direction) (WALL_ORNAMENT_BELOW >> (direction))
enum { ORNAMENT_RANDOM_MODULO = 30 };
/* Seeds and multipliers of the pseudo-random ornament hash of get_random_ornament_ordinal. */
#define ORNAMENT_SEED_ROW         32
#define ORNAMENT_SEED_SQUARE_BASE 2000
#define ORNAMENT_SEED_MAP_STEP    64
#define ORNAMENT_SEED_MAP_BASE    3000
#define ORNAMENT_HASH_SQUARE      31417u
#define ORNAMENT_HASH_MAP         11u
enum { ORNAMENT_HASH_SHIFT = 2 };

/* View squares: index of the g_view_square_* tables. */
enum {
    VIEW_SQUARE_D0C,
    VIEW_SQUARE_D0L,
    VIEW_SQUARE_D0R,
    VIEW_SQUARE_D1C,
    VIEW_SQUARE_D1L,
    VIEW_SQUARE_D1R,
    VIEW_SQUARE_D2C,
    VIEW_SQUARE_D2L,
    VIEW_SQUARE_D2R,
    VIEW_SQUARE_D2L2,
    VIEW_SQUARE_D2R2,
    VIEW_SQUARE_D3C,
    VIEW_SQUARE_D3L,
    VIEW_SQUARE_D3R,
    VIEW_SQUARE_D3L2,
    VIEW_SQUARE_D3R2,
    VIEW_SQUARE_D4C,
    VIEW_SQUARE_D4L,
    VIEW_SQUARE_D4R,
};

/* Floor views of the floor ornaments, and door views of the door zones. */
enum { VIEW_D3L2, VIEW_D3R2, VIEW_D3L, VIEW_D3C, VIEW_D3R, VIEW_D2L, VIEW_D2C, VIEW_D2R, VIEW_D1L, VIEW_D1C, VIEW_D1R };

/* View walls of draw_wall_ornament: the visible side of each wall square. */
enum {
    VIEW_WALL_D3L2_RIGHT,
    VIEW_WALL_D3R2_LEFT,
    VIEW_WALL_D3L_RIGHT,
    VIEW_WALL_D3R_LEFT,
    VIEW_WALL_D3L_FRONT,
    VIEW_WALL_D3C_FRONT,
    VIEW_WALL_D3R_FRONT,
    VIEW_WALL_D2L_RIGHT,
    VIEW_WALL_D2R_LEFT,
    VIEW_WALL_D2L_FRONT,
    VIEW_WALL_D2C_FRONT,
    VIEW_WALL_D2R_FRONT,
    VIEW_WALL_D1L_RIGHT,
    VIEW_WALL_D1R_LEFT,
    VIEW_WALL_D1C_FRONT,
};

enum { BUTTON_D3R, BUTTON_D3C, BUTTON_D2C, BUTTON_D1C }; /* door button views */
enum { DOOR_DEPTH_D3, DOOR_DEPTH_D2, DOOR_DEPTH_D1, DOOR_DEPTHS };
enum { DOOR_TYPES = 2 };
enum { ALCOVE_ORNAMENT_COUNT = 3 }; /* g_alcove_ornament_indices */
enum { DOOR_ANIMATED = 0x400 };     /* door info attribute: the door mirrors at random on each draw */

/* Ornaments: ordinal = index + 1, 0 = none. */
enum {
    MAP_ORNAMENTS = 16,
    FLOOR_ORNAMENT_FOOTPRINTS = 15,
    DOOR_ORNAMENT_DESTROYED = 16,
    DOOR_ORNAMENT_THIEVES_EYE = 17
};
enum { FLOOR_ORNAMENT_WITH_FOOTPRINTS = 0x8000 }; /* floor ornament ordinal flag */
enum { WALL_ORNAMENT_GRAPHICS = 2, WALL_ORNAMENT_FRONT = 1, FLOOR_ORNAMENT_GRAPHICS = 6, DOOR_MASK_COORD_SET = 1 };
enum { WALL_TEXT_NEW_LINE = 0x80, WALL_TEXT_END = 0x81 }; /* decode_text mode 0 codes */
enum { WALL_TEXT_SIZE = 80 };
enum { SCALE_D3_FAR_SIDE_W = 30 }; /* the D3L2 and D3R2 wall sides are wider than high */

/* Cell orders of draw_objects_creatures_projectiles_explosions: a nibble per view cell + 1, low nibble first;
   0 is the alcove. Bit 3 marks a door pass, bit 0 then selects the pass behind (0) or in front of the door (1),
   and the next nibbles hold the cells. B back, F front, L left, R right. */
enum {
    CELLS_ALCOVE = 0,
    CELLS_BL = 0x1,
    CELLS_BR = 0x2,
    CELLS_BL_BR = 0x21,
    CELLS_BL_FL = 0x41,
    CELLS_BR_FR = 0x32,
    CELLS_BL_BR_FR = 0x321,
    CELLS_BR_BL_FL = 0x412,
    CELLS_BR_FL_FR = 0x342,
    CELLS_BL_FR_FL = 0x431,
    CELLS_BL_BR_FL_FR = 0x3421,
    CELLS_BR_BL_FR_FL = 0x4312,
    DOOR_BACK_BL = 0x18,
    DOOR_BACK_BR = 0x28,
    DOOR_BACK_BL_BR = 0x218,
    DOOR_BACK_BR_BL = 0x128,
    DOOR_FRONT_FL = 0x49,
    DOOR_FRONT_FR = 0x39,
    DOOR_FRONT_FL_FR = 0x349,
    DOOR_FRONT_FR_FL = 0x439,
};

enum { LIGHT_LEVELS = 6, LIGHT_DARKEST = LIGHT_LEVELS - 1 };

/* set_square_aspect: what the view draws of a square. */
typedef struct {
    uint16_t element;     /* square type, or a door or stairs element */
    uint16_t square;      /* square byte */
    uint16_t first_thing; /* first thing after the doors, teleporters, texts, and sensors */
    union {
        uint16_t side[WALL_SIDES]; /* wall: ornament ordinal of each side, clockwise from the far side */
        struct {
            uint16_t far_side, right, front, left;
        };
        struct {
            uint16_t state, door, floor_ornament;
        }; /* door state, pit invisible, stairs up; door thing */
    };
    uint16_t text;     /* g_inscription_thing */
    uint16_t portrait; /* g_champion_portrait_ordinal */
} Aspect;

typedef struct {
    int16_t graphic, coord; /* first graphic, coordinate set */
} Ornament;

static Bitmap vp;                       /* g_viewport_bitmap */
static int16_t map = -1, mw, mh;        /* current map and its size */
static int16_t wall_set_ids[WS_COUNT];  /* g_wall_set_layout_ids, swapped for mirrored views */
static int16_t slot_gfx[LAYOUT_SLOTS];  /* graphic of each layout bitmap slot */
static uint8_t slot_flip[LAYOUT_SLOTS]; /* slot holds the mirrored graphic */
static Ornament wall_orn[MAP_ORNAMENTS], floor_orn[MAP_ORNAMENTS], door_orn[DOOR_ORNAMENT_THIEVES_EYE];
static int16_t alcoves[ALCOVE_ORNAMENT_COUNT], inscription, stairs, door_gfx[DOOR_DEPTHS][DOOR_TYPES];
static uint16_t door_attr[DOOR_TYPES];
static int16_t parity, rel_x, rel_y;                /* g_use_flipped_wall_bitmaps, g_zone_relative_x/y */
static uint16_t pals[LIGHT_LEVELS][PALETTE_COLORS]; /* g_dungeon_palettes with the creature replacement colors */
static uint8_t creature_d3[PALETTE_COLORS],
    creature_d2[PALETTE_COLORS]; /* g_palette_changes_creature_d3/d2 of the map */
static Box drawn_box;            /* g_drawn_box: the box of the last in_zone */

/* Smooth projectile flight (advanced feature, no rule change): the box of each projectile at the
   last tick and at the tick before. Between ticks the view is drawn with each projectile part of the
   way from the old box to the new box. */
#define PROJECTILE_TRACK_COUNT 32
typedef struct {
    uint16_t thing;
    Box from, to;
} ProjectileTrack;
static ProjectileTrack tracks[PROJECTILE_TRACK_COUNT], last_tracks[PROJECTILE_TRACK_COUNT];
static int track_count, last_track_count;
static int32_t track_view = -1;           /* map, position, and direction of the tracks */
static int16_t motion_step, motion_steps; /* the frame between two ticks; 0 steps: a tick view */
static int16_t shift_x, shift_y;          /* in_zone moves the drawing by this offset */
static int16_t vi_altar, fountain;        /* wall ornament index of the vi altar and the fountain, or -1 */
bool g_do_not_draw_fluxcages;
Box g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_COUNT];
uint16_t g_pile_top_objects[VIEW_CLICK_BOX_WALL];
uint16_t g_pile_objects[VIEW_CLICK_BOX_WALL][VIEW_PILE_OBJECT_MAX];
int16_t g_pile_object_counts[VIEW_CLICK_BOX_WALL];
int16_t g_square_ahead_element;
bool g_facing_alcove, g_facing_fountain, g_facing_vi_altar;

static bool is_open_floor(uint8_t square)
{
    return SQUARE_TYPE(square) == SQUARE_CORRIDOR || SQUARE_TYPE(square) == SQUARE_PIT;
}

/* Square byte of the current map. Outside the map: a wall, with a random ornament allowed on the side that
   faces an open floor square at the map edge. */
static uint8_t square(int16_t x, int16_t y)
{
    bool in_x = x >= 0 && x < mw, in_y = y >= 0 && y < mh;
    if (in_x && in_y)
        return dungeon_square(map, x, y);
    if (in_x) {
        if (y == -1 && is_open_floor(dungeon_square(map, x, 0)))
            return WALL_ORNAMENT_FACE(DIRECTION_SOUTH);
        if (y == mh && is_open_floor(dungeon_square(map, x, mh - 1)))
            return WALL_ORNAMENT_FACE(DIRECTION_NORTH);
    } else if (in_y) {
        if (x == -1 && is_open_floor(dungeon_square(map, 0, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_EAST);
        if (x == mw && is_open_floor(dungeon_square(map, mw - 1, y)))
            return WALL_ORNAMENT_FACE(DIRECTION_WEST);
    }
    return SQUARE_WALL;
}

static bool is_alcove(int16_t i)
{
    return i >= 0 && (i == alcoves[0] || i == alcoves[1] || i == alcoves[2]);
}

/* get_random_ornament_ordinal: a pseudo-random ornament ordinal (0: none) from the square position. */
static uint16_t random_ordinal(int allowed, int16_t count, int16_t x, int16_t y)
{
    if (!allowed)
        return 0;
    uint16_t square_seed = y + x * ORNAMENT_SEED_ROW + ORNAMENT_SEED_SQUARE_BASE;
    uint16_t map_seed = mh + mw + map * ORNAMENT_SEED_MAP_STEP + ORNAMENT_SEED_MAP_BASE;
    /* 16-bit products and sum as in the original */
    uint16_t hash = (uint16_t)(rec_dungeon_ornament_seed() + ((uint16_t)(square_seed * ORNAMENT_HASH_SQUARE) >> 1) +
                               (uint16_t)(map_seed * ORNAMENT_HASH_MAP)) >>
                    ORNAMENT_HASH_SHIFT;
    uint16_t i = hash % ORNAMENT_RANDOM_MODULO;
    return i < count ? i + 1 : 0;
}

enum { SCAN_NONE, SCAN_FLOOR, SCAN_WALL, SCAN_FAKE_WALL }; /* things that set_square_aspect reads */

/* The random ornament of each side of wall square (x, y) seen in direction dir; no alcove on the walls
   around the map. */
static void random_wall_ornaments(Aspect *a, uint8_t s, int16_t dir, int16_t x, int16_t y)
{
    for (int k = 0; k < WALL_SIDES; k++) {
        int16_t face = NORMALIZE(dir + k);
        a->side[k] =
            random_ordinal(s & WALL_ORNAMENT_FACE(face), map_random_wall_ornament_count(map), x, (face + 1) * (y + 1));
    }
    if (x < 0 || x >= mw || y < 0 || y >= mh) /* no random alcove on the walls around the map */
        for (int k = 0; k < WALL_SIDES; k++)
            if (is_alcove(a->side[k] - 1))
                a->side[k] = 0;
}

/* The ornaments that the doors, teleporters, texts, and sensors of a square show: the floor ornament of a
   floor sensor, the wall ornaments, the inscription, and the champion portrait. Returns the first thing after them. */
static uint16_t scan_ornament_things(Aspect *a, uint16_t thing, int16_t dir, int scan)
{
    for (; thing != THING_ENDOFLIST && THING_TYPE(thing) <= THING_SENSOR; thing = get_next_thing(thing)) {
        if (scan == SCAN_FLOOR && THING_TYPE(thing) == THING_SENSOR)
            a->floor_ornament = rec_sensor_ornament_ordinal(thing);
        if (scan != SCAN_WALL && scan != SCAN_FAKE_WALL)
            continue;
        uint16_t *side = &a->side[NORMALIZE(THING_CELL(thing) - dir)];
        if (THING_TYPE(thing) == THING_TEXT) {
            if (rec_text_is_visible(thing)) {
                *side = inscription + 1;
                a->text = thing;
            }
        } else {
            *side = rec_sensor_ornament_ordinal(thing);
            if (rec_sensor_type(thing) == SENSOR_WALL_CHAMPION_PORTRAIT)
                a->portrait = rec_sensor_data(thing) + 1;
        }
    }
    return thing;
}

/* set_square_aspect without the footprints bit: the element, the ornaments, and the first drawn thing of
   square (x, y) seen in direction dir. */
static void aspect(Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    memset(a, 0, sizeof *a);
    uint16_t thing = dungeon_square_first_thing(map, x, y);
    uint8_t s = square(x, y);
    int scan = SCAN_NONE;
    a->element = SQUARE_TYPE(s);
    a->square = s;
    switch (SQUARE_TYPE(s)) {
    case SQUARE_WALL:
        random_wall_ornaments(a, s, dir, x, y);
        scan = SCAN_WALL;
        break;
    case SQUARE_FAKE_WALL:
        if (!(s & FAKE_WALL_OPEN)) {
            a->element = SQUARE_WALL;
            scan = SCAN_FAKE_WALL;
            break;
        }
        a->element = SQUARE_CORRIDOR;
        s &= SQUARE_RANDOM_ORNAMENT;
        /* fall through */
    case SQUARE_CORRIDOR:
        a->floor_ornament = random_ordinal(s & SQUARE_RANDOM_ORNAMENT, map_random_floor_ornament_count(map), x, y);
        scan = SCAN_FLOOR;
        break;
    case SQUARE_PIT:
        if (!(s & PIT_OPEN))
            a->element = SQUARE_CORRIDOR;
        else
            a->state = s & PIT_INVISIBLE;
        scan = SCAN_FLOOR;
        break;
    case SQUARE_STAIRS:
        a->element = (dir & 1) == !!(s & STAIRS_NORTH_SOUTH) ? ELEMENT_STAIRS_SIDE : ELEMENT_STAIRS_FRONT;
        a->state = s & STAIRS_UP;
        break;
    case SQUARE_DOOR:
        a->element = (dir & 1) == !!(s & DOOR_NORTH_SOUTH) ? ELEMENT_DOOR_SIDE : ELEMENT_DOOR_FRONT;
        a->state = door_state(s);
        a->door = THING_INDEX(thing);
        break;
    case SQUARE_TELEPORTER:
        if (!(s & TELEPORTER_OPEN) || !(s & TELEPORTER_VISIBLE))
            a->element = SQUARE_CORRIDOR;
        scan = SCAN_FLOOR;
        break;
    }
    thing = scan_ornament_things(a, thing, dir, scan);
    /* a closed fake wall shows its things only when the party stands in its row or column */
    a->first_thing = scan == SCAN_FAKE_WALL && g_party_map_x != x && g_party_map_y != y ? THING_ENDOFLIST : thing;
}

/* draw_wall_set_bitmap and its _opaque and _flipped variants: a graphic (id >= 0) or a layout bitmap slot
   (id < 0) at zone. */
static void wall(int16_t id, int16_t zone, int mode)
{
    int16_t graphic = id, flip = mode == BLIT_FLIPPED, sx = 0, sy = 0, w = 0, h = 0;
    if (id < 0) { /* the slot gives the graphic and the source rectangle */
        int16_t slot = -1 - id;
        TablesRect r = tables_layout_bitmap(slot);
        graphic = slot_gfx[slot];
        flip ^= slot_flip[slot];
        sx = r.x;
        sy = r.y;
        w = r.w;
        h = r.h;
    }
    const Bitmap *bm = gfx_bitmap(graphic);
    Box b;
    if (bm && layout_get_zone(zone, bm->w, bm->h, &b, &w, &h))
        draw_blit(bm, w + sx, h + sy, &vp, &b, mode == BLIT_OPAQUE ? -1 : TRANSPARENT_COLOR, flip);
}

/* draw_bitmap_in_zone. ZONE_SHIFTED or ZONE_SIZED pass g_zone_relative_x/y to layout_get_zone: a shift, or
   the item size. Returns true and sets drawn_box (g_drawn_box) when it drew. */
static bool in_zone(const Bitmap *bm, Bitmap *dst, int16_t zone, int flip, int16_t transparent)
{
    int16_t w = 0, h = 0;
    Box b;
    if (!bm || zone == -1)
        return false;
    if (zone & (ZONE_SHIFTED | ZONE_SIZED)) {
        w = rel_x;
        h = rel_y;
        zone &= ~ZONE_SIZED;
    }
    if (!layout_get_zone(zone, bm->w, bm->h, &b, &w, &h))
        return false;
    drawn_box = b;
    b.x += shift_x;
    b.y += shift_y;
    draw_blit(bm, w, h, dst, &b, transparent, flip);
    return true;
}

enum { VIEW_CELL_BACK_LEFT, VIEW_CELL_BACK_RIGHT, VIEW_CELL_FRONT_RIGHT, VIEW_CELL_FRONT_LEFT, VIEW_CELL_CENTER };
enum { FACING_AWAY, FACING_LEFT, FACING_TOWARD, FACING_RIGHT }; /* creature direction relative to the party */
enum { DEPTH_D0, DEPTH_D1, DEPTH_D2, DEPTH_D3, DEPTH_D4 };
/* scales are in 1/32 */
enum { SCALE_D3 = 14, SCALE_D2 = 21, SCALE_NATIVE = 32, SCALE_EXPLOSION_MIN = 4, SCALE_SHIFT = 5 };
enum { DOOR_PASS_BACK = 1, DOOR_PASS_FRONT = 2 }; /* door squares draw things behind, then in front of the door */
enum { CELL_ORDER_DOOR_PASS = 8, CELL_ORDER_BITS = 4, CELL_ORDER_MASK = 0xf };
enum {
    CLICK_BOX_MIN_W = 14,
    CLICK_BOX_MIN_H = 11,
    CLICK_BOX_CENTER_H = 15,
    CLICK_BOX_CENTER_OFFSET = 7
}; /* smallest clickable object box */

/* The aspect offsets of the active group (group.h) index the shift sets: x, then y. */

/* Creature aspect record (6 bytes): graphic word, coordinate set and transparent color, replacement color sets. */
enum {
    CREATURE_ASPECT_GRAPHIC = 0,
    CREATURE_ASPECT_COORD = 4,
    CREATURE_ASPECT_REPLACEMENT = 5,
};
enum { CREATURE_ASPECT_TRANSPARENT = 0xf, CREATURE_ASPECT_COORD_SHIFT = 4 };

/* Object aspect record (4 bytes): graphic, flags, alcove coordinate set. */
enum { OBJECT_ASPECT_GRAPHIC = 0, OBJECT_ASPECT_FLAGS = 2, OBJECT_ASPECT_ALCOVE_COORD = 3 };
enum { OBJECT_FLIP_ON_RIGHT = 0x1, OBJECT_HAS_ALCOVE_GRAPHIC = 0x10 };

/* Projectile aspect record (4 bytes): graphic, scale flags, pose flags. */
enum {
    PROJECTILE_ASPECT_GRAPHIC = 0,
    PROJECTILE_ASPECT_SCALE_FLAGS = 2,
    PROJECTILE_ASPECT_POSE_FLAGS = 3,
};
enum { PROJECTILE_SCALE_WITH_ENERGY = 0x1, PROJECTILE_FLIP_ON_LEFT = 0x10, PROJECTILE_KIND = 0x3 };
enum { PROJECTILE_BACK_AND_ROTATION, PROJECTILE_BACK, PROJECTILE_NO_BACK, PROJECTILE_NO_BACK_NO_ROTATION };
enum { POSE_FRONT, POSE_BACK, POSE_SIDE }; /* graphic offset from the projectile aspect graphic */
enum {
    PROJECTILE_ASPECT_LIGHTNING = 4,
    PROJECTILE_ASPECT_FIREBALL = 11,
    PROJECTILE_ASPECT_SPELL = 12,
    PROJECTILE_ASPECT_SLIME = 13,
    PROJECTILE_ASPECT_POISON = 14
};
enum { WEAPON_PROJECTILE_ASPECT_SHIFT = 8, WEAPON_PROJECTILE_ASPECT_MASK = 0x1f }; /* 0: the object aspect */
enum { PROJECTILE_ENERGY_MIN = 96, PROJECTILE_SCALE_MIN = 2, PROJECTILE_ENERGY_SCALE_SHIFT = 8 };

/* Explosion graphics. */
enum { EXPLOSION_ASPECT_FIRE, EXPLOSION_ASPECT_SPELL, EXPLOSION_ASPECT_POISON, EXPLOSION_ASPECT_SMOKE };
enum { EXPLOSION_ATTACK_MIN = 48, EXPLOSION_STRENGTH_SHIFT = 5, EXPLOSION_PATTERN_STRENGTHS = 3 };

/* Field aspect bytes and the masked fill arguments. */
enum {
    FIELD_PATTERN,
    FIELD_LAST_UNIT,
    FIELD_TRANSPARENT,
    FIELD_MASK,
    FIELD_WIDTH,
    FIELD_HEIGHT,
    FIELD_ASPECT_SIZE = 8
};
enum { FIELD_NO_MASK = 0xff, FIELD_MASK_FLIPPED = 0x80, FIELD_FIRST_UNIT_RANGE = 0x1f };
enum { EXPLOSION_FILL_LAST_UNIT = 0x57, EXPLOSION_FILL_FIRST_UNIT_RANGE = 0x3f };
#define EXPLOSION_FILL_TRANSPARENT (FILL_NO_MASK | TRANSPARENT_COLOR)

/* The bitmap of graphic g at scale/32 with palette changes; the native bitmap at full scale without
   changes. Mirroring happens at blit time. Free out->px after use. */
static const Bitmap *shrunk(int16_t g, int16_t scale, const uint8_t *pal, Bitmap *out)
{
    const Bitmap *bm = gfx_bitmap(g);
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
static const uint8_t *object_pal(int16_t distance)
{
    return distance == 1   ? tables_palette_changes(TABLES_CHANGES_OBJECT_D2)
           : distance == 2 ? tables_palette_changes(TABLES_CHANGES_OBJECT_D3)
                           : NULL;
}

static int16_t object_aspect(uint16_t thing)
{
    return tables_object_info(inventory_get_object_info_index(thing))->aspect_index;
}

/* The object aspect of a flying thing, or minus its projectile aspect. */
static int16_t projectile_aspect(uint16_t thing)
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
        int16_t aspect = tables_weapon_info(thing_weapon_type(thing))->attributes >> WEAPON_PROJECTILE_ASPECT_SHIFT &
                         WEAPON_PROJECTILE_ASPECT_MASK;
        if (aspect)
            return -aspect;
    }
    return object_aspect(thing);
}

/* The pile box of a near object grows by the drawn box; a single object gets at least
   CLICK_BOX_MIN_W x CLICK_BOX_MIN_H pixels around its center. The object becomes the pile top and joins the pile list. */
static void add_clickable_object(int16_t index, uint16_t thing)
{
    Box *c = &g_dungeon_view_clickable_boxes[index];
    if (c->x == -1) {
        *c = drawn_box;
        if (c->h < CLICK_BOX_CENTER_H) {
            if (c->h < CLICK_BOX_MIN_H)
                c->h = CLICK_BOX_MIN_H;
            c->y += (drawn_box.h >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
        if (c->w < CLICK_BOX_MIN_W) {
            c->w = CLICK_BOX_MIN_W;
            c->x += (drawn_box.w >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
    } else {
        int16_t right = max16(c->x + c->w, drawn_box.x + drawn_box.w),
                bottom = max16(c->y + c->h, drawn_box.y + drawn_box.h);
        c->x = min16(c->x, drawn_box.x);
        c->y = min16(c->y, drawn_box.y);
        c->w = right - c->x;
        c->h = bottom - c->y;
    }
    g_pile_top_objects[index] = thing;
    if (g_pile_object_counts[index] < VIEW_PILE_OBJECT_MAX)
        g_pile_objects[index][g_pile_object_counts[index]++] = thing;
}

/* One object on the floor or in an alcove. pile counts the objects of the cell for their shift;
   NULL draws an object in flight at zone without shift. */
static void object(uint16_t thing, int16_t aspect_index, int16_t cell, int16_t lane, int16_t depth, int alcove_front,
                   int16_t zone, int16_t *pile, int16_t coord, int alcove)
{
    const uint8_t *aspect = tables_object_aspect(aspect_index);
    bool alcove_graphic = alcove_front && (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_HAS_ALCOVE_GRAPHIC);
    int16_t g = GFX_OBJECT_FIRST + aspect[OBJECT_ASPECT_GRAPHIC] + alcove_graphic, distance = 0;
    int flip = (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_FLIP_ON_RIGHT) && !alcove_graphic &&
               (lane >= 1 || (lane == 0 && (cell == VIEW_CELL_BACK_RIGHT || cell == VIEW_CELL_FRONT_RIGHT)));
    Bitmap own;
    const Bitmap *bm;
    if (depth == DEPTH_D0)
        bm = shrunk(g, SCALE_NATIVE, NULL, &own);
    else {
        int16_t row =
            depth * 2 - 1 - (cell == VIEW_CELL_CENTER ? 1 : cell >> 1); /* front cells count as one row nearer */
        distance = (row + 1) >> 1;
        bm = shrunk(g, tables_object_scale(row), object_pal(distance), &own);
    }
    if (pile) {
        zone = ZONE_SHIFTED |
               (alcove ? ZONE_ALCOVE_OBJECT_FIRST + coord + aspect[OBJECT_ASPECT_ALCOVE_COORD] * ZONE_ALCOVE_VIEW_SETS
                       : ZONE_OBJECT_FIRST + cell + coord * ZONE_OBJECT_CELLS);
        rel_x = tables_creature_shift(distance, tables_pile_shift(*pile, TABLES_AXIS_X));
        rel_y = tables_creature_shift(distance, tables_pile_shift(*pile, TABLES_AXIS_Y));
        ++*pile;
        if (!alcove)
            *pile &= PILE_FLOOR_SHIFTS - 1;
        else if (*pile > PILE_ALCOVE_LAST)
            *pile = PILE_ALCOVE_FIRST;
    }
    if (in_zone(bm, &vp, zone, flip, TRANSPARENT_COLOR) && pile && distance == 0 && lane == 0)
        add_clickable_object(alcove ? VIEW_CLICK_BOX_ALCOVE : cell, thing);
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
} Group;

static void group(uint16_t thing, Group *s)
{
    uint8_t index = rec_group_cells_byte(thing);
    s->thing = thing;
    s->info = group_info(thing);
    s->aspect = tables_creature_aspect(s->info->aspect_index);
    s->size = creature_size(s->info);
    const ActiveGroup *a = g_active_groups && map == g_party_map_index && index < g_max_active_group_count
                               ? &g_active_groups[index]
                               : NULL;
    if (a && a->group_index == THING_INDEX(thing)) {
        s->cells = a->cells;
        s->dirs = a->directions;
        memcpy(s->look, a->aspect, sizeof s->look);
    } else {
        s->cells = index;
        s->dirs = tables_group_directions(rec_group_direction(thing));
        memset(s->look, 0, sizeof s->look);
    }
}

static uint16_t creature_cell(const Group *s, int16_t creature)
{
    return group_cell(s->cells, creature);
}
static uint16_t creature_direction(const Group *s, int16_t creature)
{
    return group_cell(s->dirs, creature);
}

/* The creature number + 1 that stands in a map cell, or 0. A half-square creature fills two cells. */
static int16_t ordinal_in_cell(const Group *s, uint16_t cell)
{
    if (s->cells == CELL_CENTERED)
        return 1;
    int16_t i = rec_group_last_creature_index(s->thing);
    if (s->size == CREATURE_SIZE_HALF) {
        if ((cell & 1) == (s->dirs & 1))
            cell = turn_left(cell);
        for (; i >= 0; i--)
            if (cell == creature_cell(s, i) || turn_right(cell) == creature_cell(s, i))
                return i + 1;
    } else
        for (; i >= 0; i--)
            if (cell == creature_cell(s, i))
                return i + 1;
    return 0;
}

/* One creature: front, side, back, or attack graphic for its facing, scaled and recolored by depth,
   at position pos (view cell, or 4 centered) of the creature coordinate set coord. */
static void creature(const Group *s, uint8_t look, uint16_t facing, int16_t pos, int16_t coord, int16_t depth)
{
    uint16_t info = s->info->graphic_info;
    int16_t g = GFX_CREATURE_FIRST + BE16(s->aspect + CREATURE_ASPECT_GRAPHIC), distance = 0;
    bool side = (info & GRAPHIC_HAS_SIDE) && (facing & 1), back = false, attack = false, front_flip = false;
    if (side)
        g++;
    else {
        back = (info & GRAPHIC_HAS_BACK) && facing == FACING_AWAY;
        attack = !back && (look & ASPECT_ATTACKING) && (info & GRAPHIC_HAS_ATTACK);
        if (attack)
            g += 1 + !!(info & GRAPHIC_HAS_SIDE) + !!(info & GRAPHIC_HAS_BACK);
        else if (back)
            g += 1 + !!(info & GRAPHIC_HAS_SIDE);
        else
            front_flip = (info & GRAPHIC_FLIP_NON_ATTACK) && (look & ASPECT_FLIPPED);
    }
    bool special_d2 = depth == DEPTH_D2 && (info & GRAPHIC_SPECIAL_D2_FRONT) && !side && !back && !attack;
    bool flip = (side && facing == FACING_LEFT) || (attack && (look & ASPECT_FLIPPED)) ||
                (special_d2 && (info & GRAPHIC_D2_FRONT_FLIPPED)) || front_flip;
    int16_t transparent = s->aspect[CREATURE_ASPECT_COORD] & CREATURE_ASPECT_TRANSPARENT;
    Bitmap own;
    const Bitmap *bm;
    if (depth < DEPTH_D2)
        bm = shrunk(g, SCALE_NATIVE, NULL, &own);
    else {
        const uint8_t *pal = depth == DEPTH_D2 ? creature_d2 : creature_d3;
        distance = depth == DEPTH_D2 ? 1 : 2;
        transparent = pal[transparent];
        bm = shrunk(g, depth == DEPTH_D2 ? SCALE_D2 : SCALE_D3, pal, &own);
    }
    rel_x = tables_creature_shift(distance, look & ASPECT_OFFSET_MASK);
    rel_y = tables_creature_shift(distance, look >> ASPECT_VERTICAL_OFFSET_SHIFT & ASPECT_OFFSET_MASK);
    int16_t set = s->aspect[CREATURE_ASPECT_COORD] >> CREATURE_ASPECT_COORD_SHIFT;
    in_zone(bm, &vp,
            ZONE_SHIFTED | (ZONE_CREATURE_FIRST + set * ZONE_CREATURE_SET + coord * ZONE_CREATURE_POSITIONS + pos),
            flip, transparent);
    free(own.px);
}

/* The creatures of the group in view cell cell (map cell mcell). rest holds the cells still to draw,
   front tells that the cell is a front cell of the pass. Returns true when the group is complete. */
static bool creatures(const Group *s, int16_t dir, int16_t vs, int16_t depth, int16_t cell, uint16_t mcell,
                      uint16_t rest, int16_t pass, bool front)
{
    int16_t coord = tables_view_square(vs).creature_coord, ordinal, index, second = 0, pos = cell;
    bool done = false;
    if (coord < 0)
        return true;
    uint16_t creature_index = 0;
    if (!(ordinal = ordinal_in_cell(s, mcell))) {
        if (s->size != CREATURE_SIZE_HALF)
            return false;
        index = -1;
    } else
        index = creature_index = ordinal - 1;
    uint16_t relative = dir - creature_direction(s, creature_index), facing = NORMALIZE(relative);
    int sideways = relative & 1;
    if (s->cells == CELL_CENTERED) { /* one creature in the middle, drawn after the last cell */
        if (rest || pass == DOOR_PASS_BACK)
            return false;
        done = true;
        pos = s->size == CREATURE_SIZE_HALF && sideways ? VIEW_CELL_FRONT_LEFT : VIEW_CELL_CENTER;
    } else if (s->size == CREATURE_SIZE_HALF && (front || !rest || index < 0)) {
        if (!front || pass == DOOR_PASS_FRONT) {
            if (pass == DOOR_PASS_BACK || rest)
                return false;
            if (!sideways) { /* facing the party or away: both creatures side by side */
                done = true;
                if (index < 0)
                    index = 0;
                second = rec_group_last_creature_index(s->thing);
                uint16_t c = creature_cell(s, creature_index);
                pos = c == dir || turn_left(dir) == c ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT;
            } else {
                if (index < 0)
                    return false;
                pos = VIEW_CELL_CENTER;
            }
        } else if (index >= 0 && sideways)
            pos = VIEW_CELL_FRONT_RIGHT;
        else
            return false;
    } else if (s->size != CREATURE_SIZE_QUARTER)
        return false;
    if (s->size == CREATURE_SIZE_QUARTER && ((vs == VIEW_SQUARE_D0L && pos != VIEW_CELL_FRONT_RIGHT) ||
                                             (vs == VIEW_SQUARE_D0R && pos != VIEW_CELL_FRONT_LEFT)))
        return done;
    for (uint8_t look = s->look[index];;) {
        creature(s, look, facing, pos, coord, depth);
        if (!second)
            break;
        second = 0;
        look = s->look[index == 0];
        pos = pos == VIEW_CELL_BACK_RIGHT ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT;
    }
    return done;
}

/* A tick view starts new tracks; the old tracks stay when the party still looks from the same place. */
static void start_tracks(int16_t dir, int16_t x, int16_t y)
{
    int32_t view = ((((int32_t)map << MAP_COORD_BITS | x) << MAP_COORD_BITS | y) << DIRECTION_BITS) | dir;
    last_track_count = view == track_view ? track_count : 0;
    memcpy(last_tracks, tracks, sizeof tracks[0] * (size_t)last_track_count);
    track_count = 0;
    track_view = view;
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
static void begin_projectile(uint16_t thing)
{
    const ProjectileTrack *t = motion_steps ? find_track(tracks, track_count, thing) : NULL;
    if (!t)
        return;
    int16_t rest = motion_steps - motion_step;
    shift_x = (t->from.x - t->to.x) * rest / motion_steps;
    shift_y = (t->from.y - t->to.y) * rest / motion_steps;
}

/* At a tick: remembers where the projectile was drawn, and where it was at the tick before. */
static void end_projectile(uint16_t thing, int drawn)
{
    shift_x = shift_y = 0;
    if (motion_steps || !drawn || track_count == PROJECTILE_TRACK_COUNT)
        return;
    const ProjectileTrack *last = find_track(last_tracks, last_track_count, thing);
    tracks[track_count++] = (ProjectileTrack){thing, last ? last->to : drawn_box, drawn_box};
}

bool view_projectiles_moving(void)
{
    for (int i = 0; i < track_count; i++)
        if (tracks[i].from.x != tracks[i].to.x || tracks[i].from.y != tracks[i].to.y)
            return true;
    return false;
}

void view_set_motion(int16_t step, int16_t steps)
{
    motion_step = step;
    motion_steps = steps;
}

/* A projectile in view cell cell: an object in flight, or a projectile graphic posed and mirrored by
   its direction of flight and scaled by depth (and kinetic energy). */
static void projectile(uint16_t thing, int16_t dir, int16_t x, int16_t y, int16_t cell, int16_t lane, int16_t depth,
                       int16_t coord)
{
    int16_t zone = ZONE_PROJECTILE_FIRST + coord * ZONE_OBJECT_CELLS + cell,
            aspect_index = projectile_aspect(rec_projectile_slot(thing));
    if (aspect_index >= 0) {
        drawn_box.w = 0;
        begin_projectile(thing);
        object(thing, aspect_index, cell, lane, depth, 0, zone, NULL, 0, 0);
        end_projectile(thing, drawn_box.w > 0);
        return;
    }
    const uint8_t *aspect = tables_projectile_aspect(-aspect_index);
    int16_t kind = aspect[PROJECTILE_ASPECT_POSE_FLAGS] & PROJECTILE_KIND,
            by_energy = aspect[PROJECTILE_ASPECT_SCALE_FLAGS] & PROJECTILE_SCALE_WITH_ENERGY, scale = SCALE_NATIVE,
            row = 0, pose, flip = 0;
    uint8_t energy = rec_projectile_kinetic_energy(thing);
    if ((by_energy && energy != KINETIC_ENERGY_MAXIMUM) || depth) {
        row = depth * 2 - (cell >> 1);
        scale = tables_projectile_scale(row);
        if (by_energy) /* 16-bit product and signed byte as in the original */
            scale = max16(PROJECTILE_SCALE_MIN, (int8_t)((uint16_t)(max16(PROJECTILE_ENERGY_MIN, energy + 1) * scale) >>
                                                         PROJECTILE_ENERGY_SCALE_SHIFT));
    }
    bool rotates = kind == PROJECTILE_BACK_AND_ROTATION, odd_square = rotates && ((x + y) & 1);
    if (kind == PROJECTILE_NO_BACK_NO_ROTATION)
        pose = POSE_FRONT;
    else {
        uint16_t flight =
            g_timeline_events ? projectile_event_direction(&g_timeline_events[rec_projectile_event_index(thing)]) : dir;
        if ((dir & 1) == (flight & 1)) { /* flying away from or toward the party: front or back pose */
            pose = kind < PROJECTILE_NO_BACK && (kind != PROJECTILE_BACK || flight == dir) && !odd_square ? POSE_BACK
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
                flip = turn_right(dir) == flight;
            else {
                flip = cell == VIEW_CELL_BACK_LEFT || cell == VIEW_CELL_FRONT_LEFT;
                if (!odd_square)
                    flip ^= FLIP_H;
                else
                    flip |= FLIP_V;
            }
        }
    }
    int16_t distance = row >> 1 == 2 ? 1 : row >> 1 == 3 ? 2 : 0; /* D2 or D3 palette changes */
    Bitmap own;
    const Bitmap *bm = shrunk(GFX_PROJECTILE_FIRST + aspect[PROJECTILE_ASPECT_GRAPHIC] + pose, scale,
                              scale == SCALE_NATIVE ? NULL : object_pal(distance), &own);
    begin_projectile(thing);
    end_projectile(thing, in_zone(bm, &vp, zone, flip, TRANSPARENT_COLOR));
    free(own.px);
}

/* A teleporter field or fluxcage: the pattern graphic through the mask graphic at zone, with a random
   start in the pattern so that it sparkles. */
static void draw_field(const uint8_t *aspect, int16_t zone)
{
    int16_t w = aspect[FIELD_WIDTH], h = aspect[FIELD_HEIGHT], sx = 0, flip = 0;
    Box b;
    if (!layout_get_zone(zone, 0, 0, &b, &w, &h))
        return;
    const Bitmap *mask = NULL, *units = gfx_bitmap(GFX_FIELD_PATTERN_FIRST + aspect[FIELD_PATTERN]);
    if (aspect[FIELD_MASK] != FIELD_NO_MASK) {
        mask = gfx_bitmap(GFX_FIELD_MASK_FIRST + (aspect[FIELD_MASK] & ~FIELD_MASK_FLIPPED));
        flip = !!(aspect[FIELD_MASK] & FIELD_MASK_FLIPPED);
        if (flip && mask && mask->w % PIXELS_PER_WORD)
            sx = PIXELS_PER_WORD - mask->w % PIXELS_PER_WORD;
    }
    int16_t first = random16() & FIELD_FIRST_UNIT_RANGE, last = aspect[FIELD_LAST_UNIT] + random_bit();
    if (units)
        draw_masked_fill(units, mask, flip, &vp, &b, last, first, aspect[FIELD_TRANSPARENT], sx, 0);
}

/* An explosion of aspect at scale, recolored for smoke. */
static const Bitmap *explosion_bitmap(int16_t aspect, int16_t scale, Bitmap *own)
{
    scale = bounded_value(SCALE_EXPLOSION_MIN, scale & ~1, SCALE_NATIVE);
    return shrunk(GFX_EXPLOSION_FIRST + min16(aspect, EXPLOSION_ASPECT_POISON), scale,
                  aspect == EXPLOSION_ASPECT_SMOKE ? tables_palette_changes(TABLES_CHANGES_SMOKE) : NULL, own);
}

/* An explosion on the party square fills the whole viewport with its pattern. */
static void explosion_on_party(uint16_t thing, int16_t aspect)
{
    bool smoke = aspect == EXPLOSION_ASPECT_SMOKE;
    int16_t strength = rec_explosion_attack(thing) >> EXPLOSION_STRENGTH_SHIFT;
    const Bitmap *units = gfx_bitmap(GFX_EXPLOSION_PATTERN_FIRST + (aspect - smoke) * EXPLOSION_PATTERN_STRENGTHS +
                                     (strength != 0) + (strength > 3));
    Bitmap own = {0};
    if (units && smoke && (own.px = malloc((size_t)units->w * units->h))) {
        const uint8_t *changes = tables_palette_changes(TABLES_CHANGES_SMOKE);
        own.w = units->w;
        own.h = units->h;
        for (int i = 0; i < own.w * own.h; i++)
            own.px[i] = changes[units->px[i]] & COLOR_INDEX_MASK;
        units = &own;
    }
    int16_t first = random16() & EXPLOSION_FILL_FIRST_UNIT_RANGE, last = random_2bits() + EXPLOSION_FILL_LAST_UNIT,
            w = 0, h = 0;
    Box b;
    if (units && layout_get_zone(ZONE_VIEWPORT_BOX, vp.w, vp.h, &b, &w, &h))
        draw_masked_fill(units, NULL, 0, &vp, &b, last, first, EXPLOSION_FILL_TRANSPARENT, 0, 0);
    free(own.px);
}

/* The explosions of a square, each mirrored at random, then a fluxcage as a field. */
static void explosions(uint16_t thing, int16_t dir, int16_t vs, int16_t depth, int16_t coord, uint16_t mcell,
                       int16_t pass)
{
    ViewSquare square = tables_view_square(vs);
    int16_t explosion_coord = square.explosion_coord, field_index = square.field_aspect;
    int fluxcage = 0;
    if (explosion_coord < 0 && field_index < 0)
        return;
    for (; thing != THING_END; thing = get_next_thing(thing)) {
        if (THING_TYPE(thing) != THING_EXPLOSION)
            continue;
        uint16_t cell = THING_CELL(thing);
        int16_t g, type = rec_explosion_type(thing), rebirth = type >= EXPLOSION_REBIRTH_STEP_1, aspect, zone, scale;
        if (rebirth && !(coord >= 0 && cell == mcell))
            continue;
        int16_t flip = random_2bits();
        Bitmap own = {0};
        switch (type) {
        case EXPLOSION_FIREBALL:
        case EXPLOSION_LIGHTNING_BOLT:
        case EXPLOSION_REBIRTH_STEP_2:
            aspect = EXPLOSION_ASPECT_FIRE;
            break;
        case EXPLOSION_POISON_BOLT:
        case EXPLOSION_POISON_CLOUD:
            aspect = EXPLOSION_ASPECT_POISON;
            break;
        case EXPLOSION_SMOKE:
            aspect = EXPLOSION_ASPECT_SMOKE;
            break;
        case EXPLOSION_REBIRTH_STEP_1: /* the back pose of the lightning projectile */
            scale = tables_rebirth_scale(TABLES_REBIRTH_STEP_1, coord);
            g = GFX_PROJECTILE_FIRST +
                tables_projectile_aspect(PROJECTILE_ASPECT_LIGHTNING)[PROJECTILE_ASPECT_GRAPHIC] + POSE_BACK;
            in_zone(shrunk(g, scale, NULL, &own), &vp, ZONE_REBIRTH_STEP1_FIRST + coord, flip, TRANSPARENT_COLOR);
            free(own.px);
            continue;
        case EXPLOSION_FLUXCAGE:
            fluxcage |= depth < DEPTH_D4;
            continue;
        default:
            aspect = EXPLOSION_ASPECT_SPELL;
        }
        if (explosion_coord < 0)
            continue;
        if (vs == VIEW_SQUARE_D0C) {
            explosion_on_party(thing, aspect);
            continue;
        }
        if (rebirth) {
            zone = ZONE_REBIRTH_STEP2_FIRST + coord;
            scale = tables_rebirth_scale(TABLES_REBIRTH_STEP_2, coord);
        } else {
            bool far_side = !(cell == dir || turn_left(dir) == cell);
            zone = rec_explosion_is_centered(thing)
                       ? ZONE_EXPLOSION_CENTERED_FIRST + explosion_coord
                       : ZONE_EXPLOSION_FIRST + explosion_coord * ZONE_EXPLOSION_SIDES + far_side;
            scale = (int16_t)(max16(EXPLOSION_ATTACK_MIN, rec_explosion_attack(thing) + 1) *
                              tables_explosion_scale(depth)) >>
                    8; /* 16-bit product as in the original */
        }
        in_zone(explosion_bitmap(aspect, scale, &own), &vp, zone, flip, TRANSPARENT_COLOR);
        free(own.px);
    }
    if (fluxcage && field_index >= 0 && pass != DOOR_PASS_BACK && !g_do_not_draw_fluxcages) {
        uint8_t aspect[FIELD_ASPECT_SIZE];
        memcpy(aspect, tables_field_aspect(field_index), FIELD_ASPECT_SIZE);
        aspect[FIELD_PATTERN]++; /* the fluxcage pattern follows the teleporter pattern */
        draw_field(aspect, ZONE_FIELD_FIRST + field_index);
    }
}

/* The things of one view square: per cell in cell order (nibbles of cell + 1, low first; 0 = the
   alcove; bit 3 = door pass, bit 0 the front pass) the objects, creatures, and projectiles, then the
   explosions of the square. */
static void things(uint16_t first, int16_t dir, int16_t x, int16_t y, int16_t vs, uint16_t order)
{
    if (first == THING_END)
        return;
    int16_t pass = 0;
    if (order & CELL_ORDER_DOOR_PASS) {
        pass = (order & 1) ? DOOR_PASS_FRONT : DOOR_PASS_BACK;
        order >>= CELL_ORDER_BITS;
    }
    uint16_t rest = order, mcell = 0, group_thing = THING_NONE;
    bool alcove = rest == 0, has_projectiles = false, has_explosions = false, group_done = false, loaded = false;
    ViewSquare square = tables_view_square(vs);
    int16_t lane = square.lane, depth = square.depth, coord = alcove ? square.alcove_coord : square.object_coord,
            count = 0;
    Group s;
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
            mcell = turn_back(dir); /* the alcove side of the wall faces the party */
            pile = PILE_ALCOVE_FIRST;
        }
        pile += (mcell & 1) * PILE_ODD_CELL; /* odd map cells start in the second half of the shifts */
        for (uint16_t t = first; t != THING_END; t = get_next_thing(t)) {
            int16_t type = THING_TYPE(t);
            if (type == THING_GROUP)
                group_thing = t;
            else if (type == THING_PROJECTILE)
                has_projectiles = true;
            else if (type == THING_EXPLOSION)
                has_explosions = true;
            else if (type >= THING_WEAPON && type <= THING_JUNK && coord >= 0 && THING_CELL(t) == mcell &&
                     (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) &&
                     (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT))
                object(t, object_aspect(t), cell, lane, depth, alcove && lane == 0, 0, &pile, coord, alcove);
        }
        if (alcove || depth >= DEPTH_D4)
            break;
        bool front = (cell < VIEW_CELL_FRONT_RIGHT || count == 1) &&
                     (rest == 0 || (rest & CELL_ORDER_MASK) - 1 >= VIEW_CELL_FRONT_RIGHT);
        if (group_thing != THING_NONE && !group_done) {
            if (!loaded)
                group(group_thing, &s), loaded = true;
            group_done = creatures(&s, dir, vs, depth, cell, mcell, rest, pass, front);
        }
        int16_t projectile_coord = square.object_coord;
        if (projectile_coord < 0)
            has_projectiles = false;
        else if (has_projectiles && (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) &&
                 (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT))
            for (uint16_t t = first; t != THING_END; t = get_next_thing(t))
                if (THING_TYPE(t) == THING_PROJECTILE && THING_CELL(t) == mcell)
                    projectile(t, dir, x, y, cell, lane, depth, projectile_coord);
    } while (rest);
    if (has_explosions)
        explosions(first, dir, vs, depth, coord, mcell, pass);
}

/* The field of a teleporter square. */
static void field(int16_t view_square, int16_t zone)
{
    draw_field(tables_field_aspect(tables_view_square(view_square).field_aspect), zone);
}

/* The shrink of get_scaled_flipped_bitmap: scale/32 per axis. out->px is NULL on failure. */
static void scaled(int16_t g, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes, Bitmap *out)
{
    const Bitmap *bm = gfx_bitmap(g);
    *out = (Bitmap){0};
    if (bm) /* 16-bit sizes as in the original */
        draw_shrink(bm, (int16_t)((scale_x >> 1) + bm->w * scale_x) >> SCALE_SHIFT,
                    (int16_t)((scale_y >> 1) + bm->h * scale_y) >> SCALE_SHIFT, palette_changes, out);
}

/* The inscription of the D1C wall: the blank wall, then the glyphs of each text line. */
static void wall_text(const uint8_t *text)
{
    const Bitmap *font = gfx_bitmap(GFX_WALL_TEXT_FONT);
    int16_t glyph_w = tables_glyph_width(), glyph_h = tables_glyph_height();
    wall(LAYOUT_INSCRIPTION_WALL, ZONE_INSCRIPTION_WALL_D1C, BLIT_TRANSPARENT);
    for (int16_t line = 0; font; line++) {
        int16_t n = 0;
        while (text[n] < WALL_TEXT_NEW_LINE)
            n++;
        Box b;
        int16_t w = glyph_w * n, h = glyph_h;
        if (n && layout_get_zone(ZONE_WALL_TEXT_LINE_FIRST + line, 0, 0, &b, &w, &h))
            for (b.w = glyph_w; n; n--, text++, b.x += glyph_w)
                draw_blit(font, *text * glyph_w, 0, &vp, &b, TRANSPARENT_COLOR, 0);
        text += n;
        if (*text++ == WALL_TEXT_END)
            break;
    }
}

/* The champion portrait of a portrait sensor on the D1C wall. */
static void wall_portrait(int16_t champion)
{
    const Bitmap *portraits = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t pw = tables_portrait_width(), ph = tables_portrait_height(), w = pw, h = ph;
    Box b;
    if (portraits && layout_get_zone(ZONE_WALL_PORTRAIT, 0, 0, &b, &w, &h))
        draw_blit(portraits, champion % PORTRAITS_PER_ROW * pw, champion / PORTRAITS_PER_ROW * ph, &vp, &b,
                  PORTRAIT_TRANSPARENT, 0);
}

/* draw_wall_ornament: the ornament or inscription on a view wall, and on the D1C wall the champion
   portrait and the facing flags. Returns 1 for an alcove. */
static int wall_ornament(uint16_t ordinal, int16_t view, const Aspect *a)
{
    if (!ordinal)
        return 0;
    int16_t i = ordinal - 1, zone = ZONE_WALL_ORNAMENT_FIRST + ZONE_WALL_ORNAMENT_VIEWS * wall_orn[i].coord + view;
    bool alcove = is_alcove(i), text = i == inscription;
    int mirrored = view == VIEW_WALL_D3R2_LEFT || view == VIEW_WALL_D3R_LEFT || view == VIEW_WALL_D2R_LEFT ||
                   view == VIEW_WALL_D1R_LEFT;
    int side = mirrored || view == VIEW_WALL_D3L2_RIGHT || view == VIEW_WALL_D3L_RIGHT || view == VIEW_WALL_D2L_RIGHT ||
               view == VIEW_WALL_D1L_RIGHT;
    int16_t graphic = wall_orn[i].graphic + (side ? 0 : WALL_ORNAMENT_FRONT);
    uint8_t s[WALL_TEXT_SIZE] = {WALL_TEXT_END};
    if (text)
        decode_text((char *)s, sizeof s, a->text, 0);
    if (view == VIEW_WALL_D1C_FRONT) {
        if (text) {
            wall_text(s);
            return alcove;
        }
        g_facing_alcove = alcove;
        g_facing_vi_altar = i == vi_altar;
        g_facing_fountain = i == fountain;
    }
    Bitmap own = {0};
    const Bitmap *bm;
    if (view < VIEW_WALL_D1L_RIGHT) { /* D3 and D2: scaled and recolored */
        if (view <= VIEW_WALL_D3R2_LEFT)
            scaled(graphic, SCALE_D3_FAR_SIDE_W, SCALE_D3, tables_palette_changes(TABLES_CHANGES_D3), &own);
        else if (view <= VIEW_WALL_D3R_FRONT)
            scaled(graphic, SCALE_D3, SCALE_D3, tables_palette_changes(TABLES_CHANGES_D3), &own);
        else
            scaled(graphic, SCALE_D2, SCALE_D2, tables_palette_changes(TABLES_CHANGES_D2), &own);
        if (!own.px)
            return alcove;
        bm = &own;
    } else
        bm = gfx_bitmap(graphic);
    if (text) { /* the plaque shows only as many lines as the text has */
        int16_t lines = 1;
        for (const uint8_t *p = s; *p != WALL_TEXT_END; p++)
            lines += *p == WALL_TEXT_NEW_LINE;
        if (lines <= TABLES_PLAQUE_LINES && bm) {
            zone |= ZONE_SIZED;
            rel_x = bm->w;
            rel_y = tables_plaque_height(lines, tables_wall_ornament_depth(view));
        }
    }
    if (in_zone(bm, &vp, zone, mirrored, TRANSPARENT_COLOR) && view == VIEW_WALL_D1C_FRONT)
        g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn_box;
    if (view == VIEW_WALL_D1C_FRONT && a->portrait)
        wall_portrait(a->portrait - 1);
    free(own.px);
    return alcove;
}

/* draw_floor_ornament: the floor ornament on a floor view; FLOOR_ORNAMENT_WITH_FOOTPRINTS adds the footprints. */
static void floor_ornament(uint16_t ordinal, int16_t view)
{
    if (!ordinal)
        return;
    int16_t i = (ordinal & ~FLOOR_ORNAMENT_WITH_FOOTPRINTS) - 1;
    if (i >= 0) {
        bool right = view == VIEW_D3R2 || view == VIEW_D3R || view == VIEW_D2R || view == VIEW_D1R;
        bool center = view == VIEW_D3C || view == VIEW_D2C || view == VIEW_D1C;
        bool flip = right || (i == FLOOR_ORNAMENT_FOOTPRINTS && parity && center);
        in_zone(gfx_bitmap(floor_orn[i].graphic + tables_floor_ornament_view_offset(view)), &vp,
                ZONE_FLOOR_ORNAMENT_FIRST + ZONE_FLOOR_ORNAMENT_VIEWS * floor_orn[i].coord + view, flip,
                TRANSPARENT_COLOR);
    }
    if (ordinal & FLOOR_ORNAMENT_WITH_FOOTPRINTS)
        floor_ornament(FLOOR_ORNAMENT_FOOTPRINTS + 1, view);
}

/* draw_door_ornament: an ornament or mask on the door bitmap, scaled for D3 and D2. */
static void door_ornament(uint16_t ordinal, int16_t depth, Bitmap *door)
{
    if (!ordinal)
        return;
    const Ornament *o = &door_orn[ordinal - 1];
    int16_t zone = ZONE_DOOR_ORNAMENT_FIRST + ZONE_DOOR_ORNAMENT_DEPTHS * o->coord + depth;
    Bitmap own = {0};
    if (depth == DOOR_DEPTH_D1)
        in_zone(gfx_bitmap(o->graphic), door, zone, 0, DOOR_ORNAMENT_TRANSPARENT);
    else {
        int16_t scale = depth == DOOR_DEPTH_D2 ? SCALE_D2 : SCALE_D3;
        scaled(o->graphic, scale, scale,
               tables_palette_changes(depth == DOOR_DEPTH_D2 ? TABLES_CHANGES_DOOR_ORNAMENT_D2
                                                             : TABLES_CHANGES_DOOR_ORNAMENT_D3),
               &own);
        if (own.px)
            in_zone(&own, door, zone, 0, DOOR_ORNAMENT_TRANSPARENT);
    }
    free(own.px);
}

/* draw_door_button: the button beside a door. The D1C button is clickable. */
static void door_button(int16_t view)
{
    Bitmap own = {0};
    const Bitmap *bm = gfx_bitmap(GFX_DOOR_BUTTON);
    if (view != BUTTON_D1C) {
        int16_t scale = view == BUTTON_D2C ? SCALE_D2 : SCALE_D3;
        scaled(GFX_DOOR_BUTTON, scale, scale,
               tables_palette_changes(view == BUTTON_D2C ? TABLES_CHANGES_D2 : TABLES_CHANGES_D3), &own);
        bm = &own;
    }
    if (bm && bm->px &&
        in_zone(bm, &vp, ZONE_DOOR_BUTTON_FIRST + tables_door_button_coord_set() + view, 0, TRANSPARENT_COLOR) &&
        view == BUTTON_D1C)
        g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn_box;
    free(own.px);
}

/* draw_door: a copy of the door graphic with its ornaments, placed by the door state. A door that does not
   move vertically opens as two halves that slide apart. */
static void door(uint16_t door_thing, int16_t state, int16_t view)
{
    if (state == DOOR_OPEN)
        return;
    int16_t depth = view < VIEW_D2L ? DOOR_DEPTH_D3 : view < VIEW_D1L ? DOOR_DEPTH_D2 : DOOR_DEPTH_D1;
    int16_t zone = ZONE_DOOR_FIRST + ZONE_DOOR_VIEW_ZONES * view;
    int type = door_type(door_thing);
    const Bitmap *g = gfx_bitmap(door_gfx[depth][type]);
    if (!g)
        return;
    Bitmap t = {g->w, g->h, malloc((size_t)g->w * g->h)};
    if (!t.px)
        return;
    memcpy(t.px, g->px, (size_t)g->w * g->h);
    door_ornament(door_ornament_ordinal(door_thing), depth, &t);
    if (door_attr[type] & DOOR_ANIMATED)
        draw_flip(&t, random_2bits());
    if (state == DOOR_DESTROYED)
        door_ornament(DOOR_ORNAMENT_DESTROYED, depth, &t);
    else if (state != DOOR_CLOSED) { /* partly open: the zone moves with the state */
        zone += state;
        if (!door_is_vertical(door_thing)) {
            int16_t w = rel_x = t.w >> 1, h = rel_y = 0;
            Box b;
            if (layout_get_zone(zone + ZONE_DOOR_RIGHT_HALF, t.w, t.h, &b, &w, &h))
                draw_blit(&t, rel_x + w, h, &vp, &b, TRANSPARENT_COLOR, 0);
            zone = (zone + ZONE_DOOR_LEFT_HALF) | ZONE_SIZED;
        }
    }
    in_zone(&t, &vp, zone, 0, TRANSPARENT_COLOR);
    free(t.px);
}

/* get_location_after_level_change for any map (map.c checks the party map first): the map and square one
   level up or down, or -1. */
static int16_t level_change(int16_t from, int16_t delta, int16_t *x, int16_t *y)
{
    int16_t dungeon_x = *x + map_offset_x(from), dungeon_y = *y + map_offset_y(from);
    for (int16_t m = 0; m < dungeon_map_count(); m++) {
        if (delta + map_level(from) == map_level(m) && map_offset_x(m) <= dungeon_x &&
            dungeon_x <= map_offset_x(m) + map_last_x(m) && map_offset_y(m) <= dungeon_y &&
            dungeon_y <= map_offset_y(m) + map_last_y(m)) {
            *x = dungeon_x - map_offset_x(m);
            *y = dungeon_y - map_offset_y(m);
            return m;
        }
    }
    return -1;
}

/* draw_ceiling_pit: the hole of an open pit on the map above. */
static void ceiling_pit(int16_t graphic, int16_t zone, int16_t x, int16_t y, int mode)
{
    int16_t m = level_change(map, LEVEL_UP, &x, &y);
    if (m >= 0 && SQUARE_TYPE(dungeon_square(m, x, y)) == SQUARE_PIT && dungeon_square(m, x, y) & PIT_OPEN)
        wall(graphic, zone, mode);
}

/* The front of a staircase: the up or down graphic at the up or down zone. */
static void stairs_front(bool up, int16_t graphic, int16_t up_zone, int mode)
{
    wall(stairs + graphic + (up ? 0 : STAIRS_DOWN), up ? up_zone : up_zone + ZONE_STAIRS_DOWN_DELTA, mode);
}

enum {
    DRAWN_SQUARE_COUNT = 16,
    D4_SQUARE_COUNT = 3
}; /* the squares that the view draws, and the D4 squares with objects */

/* The 16 square functions of draw_dungeon_view, back to front. Each draws the wall, pit, stairs, or door of
   its square, then the floor ornament, the things, and a teleporter field. */

static void d3l2(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BL_BR_FL_FR;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D3L2], ZONE_WALL_D3L2, BLIT_TRANSPARENT);
        wall_ornament(a.right, VIEW_WALL_D3L2_RIGHT, &a);
        return;
    case SQUARE_PIT:
        if (!a.state)
            wall(GFX_PIT_D3L2, ZONE_PIT_D3L2, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        cells = CELLS_BL_BR_FR;
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D3L2);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L2, DOOR_BACK_BL_BR);
        door(a.door, a.state, VIEW_D3L2);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L2, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_FRONT:
        if (a.state)
            wall(LAYOUT_STAIRS_UP_FRONT_D3L2, ZONE_STAIRS_UP_FRONT_D3L2, BLIT_TRANSPARENT);
        else
            wall(LAYOUT_STAIRS_DOWN_FRONT_D3L2, ZONE_STAIRS_UP_FRONT_D3L2 + ZONE_STAIRS_DOWN_DELTA, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D3L2);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L2, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D3L2, ZONE_WALL_D3L2);
}

static void d3r2(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BR_BL_FR_FL;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D3R2], ZONE_WALL_D3R2, BLIT_TRANSPARENT);
        wall_ornament(a.left, VIEW_WALL_D3R2_LEFT, &a);
        return;
    case SQUARE_PIT:
        if (!a.state)
            wall(GFX_PIT_D3L2, ZONE_PIT_D3R2, BLIT_FLIPPED);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        cells = CELLS_BR_BL_FL;
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D3R2);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R2, DOOR_BACK_BR_BL);
        door(a.door, a.state, VIEW_D3R2);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R2, DOOR_FRONT_FR_FL);
        return;
    case ELEMENT_STAIRS_FRONT:
        if (a.state)
            wall(LAYOUT_STAIRS_UP_FRONT_D3R2, ZONE_STAIRS_UP_FRONT_D3R2, BLIT_FLIPPED);
        else
            wall(LAYOUT_STAIRS_DOWN_FRONT_D3R2, ZONE_STAIRS_UP_FRONT_D3R2 + ZONE_STAIRS_DOWN_DELTA, BLIT_FLIPPED);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D3R2);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R2, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D3R2, ZONE_WALL_D3R2);
}

static void d3l(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BL_BR_FL_FR;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D3L], ZONE_WALL_D3L, BLIT_TRANSPARENT);
        wall_ornament(a.right, VIEW_WALL_D3L_RIGHT, &a);
        if (wall_ornament(a.front, VIEW_WALL_D3L_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        if (!a.state)
            wall(GFX_PIT_D3L, ZONE_PIT_D3L, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        cells = CELLS_BL_BR_FR;
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D3L);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L, DOOR_BACK_BL_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_LEFT_D3L], ZONE_DOOR_FRAME_LEFT_D3L, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_RIGHT_D3L], ZONE_DOOR_FRAME_RIGHT_D3L, BLIT_FLIPPED);
        door(a.door, a.state, VIEW_D3L);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D3L, ZONE_STAIRS_UP_FRONT_D3L, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D3L);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D3L, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D3L, ZONE_WALL_D3L);
}

static void d3r(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BR_BL_FR_FL;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D3R], ZONE_WALL_D3R, BLIT_TRANSPARENT);
        wall_ornament(a.left, VIEW_WALL_D3R_LEFT, &a);
        if (wall_ornament(a.front, VIEW_WALL_D3R_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        if (!a.state)
            wall(GFX_PIT_D3L, ZONE_PIT_D3R, BLIT_FLIPPED);
        break;
    case ELEMENT_DOOR_SIDE:
    case ELEMENT_STAIRS_SIDE:
        cells = CELLS_BR_BL_FL;
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D3R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R, DOOR_BACK_BR_BL);
        wall(wall_set_ids[WS_DOOR_FRAME_LEFT_D3R], ZONE_DOOR_FRAME_LEFT_D3R, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_LEFT_D3L], ZONE_DOOR_FRAME_RIGHT_D3R, BLIT_FLIPPED);
        if (door_button_present(a.door))
            door_button(BUTTON_D3R);
        door(a.door, a.state, VIEW_D3R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R, DOOR_FRONT_FR_FL);
        return;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D3L, ZONE_STAIRS_UP_FRONT_D3R, BLIT_FLIPPED);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D3R);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D3R, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D3R, ZONE_WALL_D3R);
}

static void d3c(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D3C], ZONE_WALL_D3C, BLIT_OPAQUE);
        if (wall_ornament(a.front, VIEW_WALL_D3C_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D3C, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        if (!a.state)
            wall(GFX_PIT_D3C, ZONE_PIT_D3C, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D3C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3C, DOOR_BACK_BL_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_D3C], ZONE_DOOR_FRAME_LEFT_D3C, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_D3C], ZONE_DOOR_FRAME_RIGHT_D3C, BLIT_FLIPPED);
        if (door_button_present(a.door))
            door_button(BUTTON_D3C);
        door(a.door, a.state, VIEW_D3C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D3C, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D3C, ZONE_STAIRS_UP_FRONT_D3C, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D3C);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D3C, CELLS_BL_BR_FL_FR);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D3C, ZONE_WALL_D3C);
}

static void d2l2(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    if (a.element == SQUARE_WALL)
        wall(wall_set_ids[WS_WALL_D2L2], ZONE_WALL_D2L2, BLIT_TRANSPARENT);
    else if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D2L2, ZONE_WALL_D2L2);
}

static void d2r2(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    if (a.element == SQUARE_WALL)
        wall(wall_set_ids[WS_WALL_D2R2], ZONE_WALL_D2R2, BLIT_TRANSPARENT);
    else if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D2R2, ZONE_WALL_D2R2);
}

static void d2l(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BL_BR_FL_FR;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D2L], ZONE_WALL_D2L, BLIT_TRANSPARENT);
        wall_ornament(a.right, VIEW_WALL_D2L_RIGHT, &a);
        if (!wall_ornament(a.front, VIEW_WALL_D2L_FRONT, &a))
            return;
        ceiling_pit(GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2L, x, y, BLIT_TRANSPARENT);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2L, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D2L : GFX_PIT_D2L, ZONE_PIT_D2L, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D2L);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2L, DOOR_BACK_BL_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D2L], ZONE_DOOR_FRAME_TOP_D2L, BLIT_TRANSPARENT);
        door(a.door, a.state, VIEW_D2L);
        ceiling_pit(GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2L, x, y, BLIT_TRANSPARENT);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2L, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_SIDE:
        wall(stairs + STAIRS_SIDE_D2L, ZONE_STAIRS_SIDE_D2L, BLIT_TRANSPARENT); /* fall through */
    case ELEMENT_DOOR_SIDE:
        cells = CELLS_BR_FL_FR;
        break;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D2L, ZONE_STAIRS_UP_FRONT_D2L, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D2L);
    ceiling_pit(GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2L, x, y, BLIT_TRANSPARENT);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D2L, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D2L, ZONE_WALL_D2L);
}

static void d2r(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    uint16_t cells = CELLS_BR_BL_FR_FL;
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D2R], ZONE_WALL_D2R, BLIT_TRANSPARENT);
        wall_ornament(a.left, VIEW_WALL_D2R_LEFT, &a);
        if (wall_ornament(a.front, VIEW_WALL_D2R_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D2R, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D2L : GFX_PIT_D2L, ZONE_PIT_D2R, BLIT_FLIPPED);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D2R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2R, DOOR_BACK_BR_BL);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D2R], ZONE_DOOR_FRAME_TOP_D2R, BLIT_TRANSPARENT);
        door(a.door, a.state, VIEW_D2R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2R, DOOR_FRONT_FR_FL);
        return;
    case ELEMENT_STAIRS_SIDE:
        wall(stairs + STAIRS_SIDE_D2L, ZONE_STAIRS_SIDE_D2R, BLIT_FLIPPED); /* fall through */
    case ELEMENT_DOOR_SIDE:
        cells = CELLS_BL_FR_FL;
        break;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D2L, ZONE_STAIRS_UP_FRONT_D2R, BLIT_FLIPPED);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D2R);
    ceiling_pit(GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2R, x, y, BLIT_FLIPPED);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D2R, cells);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D2R, ZONE_WALL_D2R);
}

static void d2c(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D2C], ZONE_WALL_D2C, BLIT_OPAQUE);
        if (wall_ornament(a.front, VIEW_WALL_D2C_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D2C, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D2C : GFX_PIT_D2C, ZONE_PIT_D2C, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D2C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2C, DOOR_BACK_BL_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D2C], ZONE_DOOR_FRAME_TOP_D2C, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_D2C], ZONE_DOOR_FRAME_LEFT_D2C, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_D2C], ZONE_DOOR_FRAME_RIGHT_D2C, BLIT_FLIPPED);
        if (door_button_present(a.door))
            door_button(BUTTON_D2C);
        door(a.door, a.state, VIEW_D2C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D2C, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D2C, ZONE_STAIRS_UP_FRONT_D2C, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D2C);
    ceiling_pit(GFX_CEILING_PIT_D2C, ZONE_CEILING_PIT_D2C, x, y, BLIT_TRANSPARENT);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D2C, CELLS_BL_BR_FL_FR);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D2C, ZONE_WALL_D2C);
}

static void d1l(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D1L], ZONE_WALL_D1L, BLIT_TRANSPARENT);
        wall_ornament(a.right, VIEW_WALL_D1L_RIGHT, &a);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D1L : GFX_PIT_D1L, ZONE_PIT_D1L, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D1L);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1L, DOOR_BACK_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D1L], ZONE_DOOR_FRAME_TOP_D1L, BLIT_TRANSPARENT);
        door(a.door, a.state, VIEW_D1L);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1L, DOOR_FRONT_FR);
        return;
    case ELEMENT_STAIRS_SIDE:
        if (a.state)
            wall(stairs + STAIRS_UP_SIDE_D1L, ZONE_STAIRS_UP_SIDE_D1L, BLIT_TRANSPARENT);
        else
            wall(stairs + STAIRS_DOWN_SIDE_D1L, ZONE_STAIRS_DOWN_SIDE_D1L, BLIT_TRANSPARENT);
        break;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D1L, ZONE_STAIRS_UP_FRONT_D1L, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D1L);
    ceiling_pit(GFX_CEILING_PIT_D1L, ZONE_CEILING_PIT_D1L, x, y, BLIT_TRANSPARENT);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D1L, CELLS_BR_FR);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D1L, ZONE_WALL_D1L);
}

static void d1r(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D1R], ZONE_WALL_D1R, BLIT_TRANSPARENT);
        wall_ornament(a.left, VIEW_WALL_D1R_LEFT, &a);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D1L : GFX_PIT_D1L, ZONE_PIT_D1R, BLIT_FLIPPED);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D1R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1R, DOOR_BACK_BL);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D1R], ZONE_DOOR_FRAME_TOP_D1R, BLIT_TRANSPARENT);
        door(a.door, a.state, VIEW_D1R);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1R, DOOR_FRONT_FL);
        return;
    case ELEMENT_STAIRS_SIDE:
        if (a.state)
            wall(stairs + STAIRS_UP_SIDE_D1L, ZONE_STAIRS_UP_SIDE_D1R, BLIT_FLIPPED);
        else
            wall(stairs + STAIRS_DOWN_SIDE_D1L, ZONE_STAIRS_DOWN_SIDE_D1R, BLIT_FLIPPED);
        break;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D1L, ZONE_STAIRS_UP_FRONT_D1R, BLIT_FLIPPED);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D1R);
    ceiling_pit(GFX_CEILING_PIT_D1L, ZONE_CEILING_PIT_D1R, x, y, BLIT_FLIPPED);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D1R, CELLS_BL_FL);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D1R, ZONE_WALL_D1R);
}

static void d1c(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    g_square_ahead_element = a.element;
    switch (a.element) {
    case SQUARE_WALL: /* ponytail: no thieves eye hole yet, needs the party spell state (step 9) */
        g_facing_alcove = g_facing_vi_altar = g_facing_fountain = false;
        wall(wall_set_ids[WS_WALL_D1C], ZONE_WALL_D1C, BLIT_OPAQUE);
        if (wall_ornament(a.front, VIEW_WALL_D1C_FRONT, &a))
            things(a.first_thing, dir, x, y, VIEW_SQUARE_D1C, CELLS_ALCOVE);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D1C : GFX_PIT_D1C, ZONE_PIT_D1C, BLIT_TRANSPARENT);
        break;
    case ELEMENT_DOOR_FRONT:
        floor_ornament(a.floor_ornament, VIEW_D1C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1C, DOOR_BACK_BL_BR);
        wall(wall_set_ids[WS_DOOR_FRAME_TOP_D1C], ZONE_DOOR_FRAME_TOP_D1C, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_LEFT_D1C], ZONE_DOOR_FRAME_LEFT_D1C, BLIT_TRANSPARENT);
        wall(wall_set_ids[WS_DOOR_FRAME_RIGHT_D1C], ZONE_DOOR_FRAME_RIGHT_D1C, BLIT_TRANSPARENT);
        if (door_button_present(a.door))
            door_button(BUTTON_D1C);
        door(a.door, a.state, VIEW_D1C);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D1C, DOOR_FRONT_FL_FR);
        return;
    case ELEMENT_STAIRS_FRONT:
        stairs_front(a.state, STAIRS_FRONT_D1C, ZONE_STAIRS_UP_FRONT_D1C, BLIT_TRANSPARENT);
        break;
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
        break;
    default:
        return;
    }
    floor_ornament(a.floor_ornament, VIEW_D1C);
    ceiling_pit(GFX_CEILING_PIT_D1C, ZONE_CEILING_PIT_D1C, x, y, BLIT_TRANSPARENT);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D1C, CELLS_BL_BR_FL_FR);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D1C, ZONE_WALL_D1C);
}

static void d0l(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D0L], ZONE_WALL_D0L, BLIT_TRANSPARENT);
        return;
    case ELEMENT_STAIRS_SIDE:
        wall(stairs + STAIRS_SIDE_D0L, ZONE_STAIRS_SIDE_D0L, BLIT_TRANSPARENT);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D0L : GFX_PIT_D0L, ZONE_PIT_D0L, BLIT_TRANSPARENT); /* fall through */
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        ceiling_pit(GFX_CEILING_PIT_D0L, ZONE_CEILING_PIT_D0L, x, y, BLIT_TRANSPARENT);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D0L, CELLS_BR);
    }
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D0L, ZONE_WALL_D0L);
}

static void d0r(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    switch (a.element) {
    case SQUARE_WALL:
        wall(wall_set_ids[WS_WALL_D0R], ZONE_WALL_D0R, BLIT_TRANSPARENT);
        return;
    case ELEMENT_STAIRS_SIDE:
        wall(stairs + STAIRS_SIDE_D0L, ZONE_STAIRS_SIDE_D0R, BLIT_FLIPPED);
        return;
    case SQUARE_PIT:
        wall(a.state ? GFX_INVISIBLE_PIT_D0L : GFX_PIT_D0L, ZONE_PIT_D0R, BLIT_FLIPPED); /* fall through */
    case SQUARE_CORRIDOR:
    case SQUARE_TELEPORTER:
    case ELEMENT_DOOR_SIDE:
        ceiling_pit(GFX_CEILING_PIT_D0L, ZONE_CEILING_PIT_D0R, x, y, BLIT_FLIPPED);
        things(a.first_thing, dir, x, y, VIEW_SQUARE_D0R, CELLS_BL);
    }
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D0R, ZONE_WALL_D0R);
}

static void d0c(int16_t dir, int16_t x, int16_t y)
{
    Aspect a;
    aspect(&a, dir, x, y);
    if (a.element == ELEMENT_DOOR_SIDE)
        wall(wall_set_ids[WS_DOOR_FRAME_D0C], ZONE_DOOR_FRAME_D0C, BLIT_TRANSPARENT);
    else if (a.element == SQUARE_PIT)
        wall(a.state ? GFX_INVISIBLE_PIT_D0C : GFX_PIT_D0C, ZONE_PIT_D0C, BLIT_TRANSPARENT);
    else if (a.element == ELEMENT_STAIRS_FRONT) {
        stairs_front(a.state, STAIRS_FRONT_D0C, ZONE_STAIRS_UP_FRONT_D0C_LEFT, BLIT_TRANSPARENT);
        stairs_front(a.state, STAIRS_FRONT_D0C, ZONE_STAIRS_UP_FRONT_D0C_RIGHT, BLIT_FLIPPED);
    }
    ceiling_pit(GFX_CEILING_PIT_D0C, ZONE_CEILING_PIT_D0C, x, y, BLIT_TRANSPARENT);
    things(a.first_thing, dir, x, y, VIEW_SQUARE_D0C, CELLS_BL_BR);
    if (a.element == SQUARE_TELEPORTER)
        field(VIEW_SQUARE_D0C, ZONE_WALL_D0C);
}

/* apply_creature_replacement_colors: color 9 or 10 of the 6 light palettes and of the creature
   palette changes from replacement set n. */
enum { COLOR_CREATURE_1 = 9, COLOR_CREATURE_2 = 10, REPLACEMENT_DEFAULT_1 = 8, REPLACEMENT_DEFAULT_2 = 12 };
enum {
    REPLACEMENT_SET_MASK = 0xf,
    REPLACEMENT_SET_SHIFT = 4
}; /* creature aspect byte: set of color 9, then of color 10 */

static void replacement_colors(int16_t color, int16_t n)
{
    for (int l = 0; l < LIGHT_LEVELS; l++) /* a color per light level, then the D2 and D3 color */
        pals[l][color] = tables_replacement_color(n, l);
    creature_d2[color] = tables_replacement_change(n, DEPTH_D2);
    creature_d3[color] = tables_replacement_change(n, DEPTH_D3);
}

static void reset_wall_set_ids(void)
{
    for (int i = 0; i < WS_COUNT; i++)
        wall_set_ids[i] = tables_wall_set_layout_id(i);
}

static int16_t entry_slot(int entry)
{
    return -1 - wall_set_ids[entry];
}

static void set_slot(int entry, int16_t graphic, int flip)
{
    slot_gfx[entry_slot(entry)] = graphic;
    slot_flip[entry_slot(entry)] = flip;
}

/* load_floor_set and load_wall_set: the graphic of each layout bitmap slot, and the slots that hold a
   mirrored graphic. */
static void load_wall_set(int16_t floor_set, int16_t wall_set)
{
    /* copy_wall_bitmap_and_flip_horizontal: the mirrored entries hold the mirror of these walls */
    static const uint8_t mirrored_walls[WALL_COUNT] = {
        WS_WALL_D0L, WS_WALL_D0R, WS_WALL_D1L,  WS_WALL_D1R,  WS_WALL_D1C, WS_WALL_D2L2, WS_WALL_D2R2, WS_WALL_D2L,
        WS_WALL_D2R, WS_WALL_D2C, WS_WALL_D3L2, WS_WALL_D3R2, WS_WALL_D3L, WS_WALL_D3R,  WS_WALL_D3C,
    };
    int16_t first = GFX_WALLSET_FIRST + GFX_WALLSET_SIZE * wall_set;
    reset_wall_set_ids();
    set_slot(WS_FLOOR, GFX_FLOOR_SET_FIRST + 2 * floor_set, 0);
    set_slot(WS_CEILING, GFX_FLOOR_SET_FIRST + 2 * floor_set + 1, 0);
    for (int i = 0; i < WALL_COUNT; i++)
        set_slot(WS_WALL_FIRST + i, first + WALLSET_WALL_FIRST + i, 0);
    set_slot(WS_DOOR_FRAME_D0C, first + WALLSET_DOOR_FRAME_D0C, 0);
    set_slot(WS_DOOR_FRAME_LEFT_D1C, first + WALLSET_DOOR_FRAME_D1C, 0);
    set_slot(WS_DOOR_FRAME_D2C, first + WALLSET_DOOR_FRAME_D2C, 0);
    set_slot(WS_DOOR_FRAME_D3C, first + WALLSET_DOOR_FRAME_D3C, 0);
    set_slot(WS_DOOR_FRAME_LEFT_D3L, first + WALLSET_DOOR_FRAME_D3_SIDE, 0);
    set_slot(WS_DOOR_FRAME_TOP_D1C, first + WALLSET_DOOR_FRAME_TOP_D1, 0);
    set_slot(WS_DOOR_FRAME_TOP_D2C, first + WALLSET_DOOR_FRAME_TOP_D2, 0);
    set_slot(WS_DOOR_FRAME_RIGHT_D1C, first + WALLSET_DOOR_FRAME_D1C, 1);
    set_slot(WS_DOOR_FRAME_TOP_D2R, first + WALLSET_DOOR_FRAME_TOP_D2, 0); /* layout_share_bitmap */
    set_slot(WS_DOOR_FRAME_TOP_D2L, first + WALLSET_DOOR_FRAME_TOP_D2, 0);
    set_slot(WS_DOOR_FRAME_TOP_D1R, first + WALLSET_DOOR_FRAME_TOP_D1, 0);
    set_slot(WS_DOOR_FRAME_TOP_D1L, first + WALLSET_DOOR_FRAME_TOP_D1, 0);
    set_slot(WS_DOOR_FRAME_RIGHT_D3L, first + WALLSET_DOOR_FRAME_D3_SIDE, 0);
    set_slot(WS_DOOR_FRAME_LEFT_D3R, first + WALLSET_DOOR_FRAME_D3_SIDE, 0);
    slot_gfx[-1 - LAYOUT_INSCRIPTION_WALL] = slot_gfx[entry_slot(WS_WALL_D1C)];
    for (int i = 0; i < WALL_COUNT; i++)
        set_slot(WS_MIRRORED_WALL_FIRST + i, slot_gfx[entry_slot(mirrored_walls[i])], 1);
    stairs = first + WALLSET_STAIRS_FIRST;
    slot_gfx[-1 - LAYOUT_STAIRS_UP_FRONT_D3L2] = slot_gfx[-1 - LAYOUT_STAIRS_UP_FRONT_D3R2] = stairs + STAIRS_FRONT_D3L;
    slot_gfx[-1 - LAYOUT_STAIRS_DOWN_FRONT_D3L2] = slot_gfx[-1 - LAYOUT_STAIRS_DOWN_FRONT_D3R2] =
        stairs + STAIRS_FRONT_D3L + STAIRS_DOWN;
}

/* The door graphics and door info of the two door types of map m. */
static void load_doors(int16_t m)
{
    for (int type = 0; type < DOOR_TYPES; type++) {
        int16_t set = map_door_set(m, type);
        door_attr[type] = tables_door_attributes(set);
        for (int depth = 0; depth < DOOR_DEPTHS; depth++)
            door_gfx[depth][type] = GFX_DOOR_FIRST + DOOR_DEPTHS * set + depth;
    }
}

/* The wall, floor, and door ornaments of map m from its ornament list, and the special ornaments: the
   inscription plaque, the footprints, and the door masks. */
static void load_ornaments(int16_t m, const uint8_t *ornaments)
{
    int16_t walls = map_wall_ornament_count(m), floors = map_floor_ornament_count(m),
            doors = map_door_ornament_count(m);
    int16_t alcove_count = 0;
    alcoves[0] = alcoves[1] = alcoves[2] = -1;
    vi_altar = fountain = -1;
    inscription = walls; /* after the map ornaments: the inscription plaque, ornament 0 */
    for (int16_t i = 0; i <= walls; i++) {
        uint8_t o = i < walls ? ornaments[i] : 0;
        wall_orn[i] =
            (Ornament){GFX_WALL_ORNAMENT_FIRST + WALL_ORNAMENT_GRAPHICS * o, tables_wall_ornament_coord_set(o)};
        for (int k = 0; k < ALCOVE_ORNAMENT_COUNT; k++)
            if (tables_alcove_ornament(k) == o) {
                alcoves[alcove_count++] = i;
                if (o == ORNAMENT_VI_ALTAR)
                    vi_altar = i;
            }
        if (tables_fountain_ornament() == o)
            fountain = i;
    }
    for (int16_t i = 0; i < floors; i++) {
        uint8_t o = ornaments[walls + i];
        floor_orn[i] =
            (Ornament){GFX_FLOOR_ORNAMENT_FIRST + FLOOR_ORNAMENT_GRAPHICS * o, tables_floor_ornament_coord_set(o)};
    }
    floor_orn[FLOOR_ORNAMENT_FOOTPRINTS] = (Ornament){GFX_FOOTPRINTS, 0};
    for (int16_t i = 0; i < doors; i++) {
        uint8_t o = ornaments[walls + floors + i];
        door_orn[i] = (Ornament){GFX_DOOR_ORNAMENT_FIRST + o, tables_door_ornament_coord_set(o)};
    }
    door_orn[DOOR_ORNAMENT_DESTROYED - 1] = (Ornament){GFX_DOOR_MASK_DESTROYED, DOOR_MASK_COORD_SET};
    door_orn[DOOR_ORNAMENT_THIEVES_EYE - 1] = (Ornament){GFX_DOOR_MASK_THIEVES_EYE, DOOR_MASK_COORD_SET};
}

/* The light palettes and the creature palette changes with the replacement colors of the creature types of
   the map. */
static void load_creature_colors(const uint8_t *creature_types, int16_t count)
{
    if (!tables_creature_info(0))
        tables_init();
    for (int l = 0; l < LIGHT_LEVELS; l++)
        tables_palette(TABLES_PALETTE_DUNGEON + l, pals[l]);
    memcpy(creature_d3, tables_palette_changes(TABLES_CHANGES_CREATURE_D3), sizeof creature_d3);
    memcpy(creature_d2, tables_palette_changes(TABLES_CHANGES_CREATURE_D2), sizeof creature_d2);
    replacement_colors(COLOR_CREATURE_1, REPLACEMENT_DEFAULT_1);
    replacement_colors(COLOR_CREATURE_2, REPLACEMENT_DEFAULT_2);
    for (int i = 0; i < count; i++) { /* the original indexes the aspects by creature type */
        uint8_t sets = tables_creature_aspect(creature_types[i])[CREATURE_ASPECT_REPLACEMENT];
        if (sets & REPLACEMENT_SET_MASK)
            replacement_colors(COLOR_CREATURE_1, (sets & REPLACEMENT_SET_MASK) - 1);
        if (sets >> REPLACEMENT_SET_SHIFT)
            replacement_colors(COLOR_CREATURE_2, (sets >> REPLACEMENT_SET_SHIFT) - 1);
    }
}

/* set_current_map_and_party_map and load_current_map_graphics: the wall set, floor set, stairs, doors,
   ornaments, and creature colors of the map. Graphics decode on first use. */
void view_load_map(int16_t m)
{
    map = m;
    mw = dungeon_map_width(m);
    mh = dungeon_map_height(m);
    const uint8_t *creature_types =
        g_dungeon_map_data[m][mw - 1] + mh; /* after the squares: creature types, then ornaments */
    int16_t creature_type_count = map_creature_type_count(m);
    load_wall_set(map_floor_set(m), map_wall_set(m));
    load_doors(m);
    load_ornaments(m, creature_types + creature_type_count);
    load_creature_colors(creature_types, creature_type_count);
}

/* set_dungeon_view_palette: the darkness of the party light amount on map m (a map of difficulty 0 is
   always bright) for the view and the creature rules. */
void view_set_light(int16_t m, int16_t amount)
{
    g_dungeon_view_palette_index = 0;
    if (!map_difficulty(m))
        return;
    if (amount < 1) {
        g_dungeon_view_palette_index = LIGHT_DARKEST;
        return;
    }
    while (g_dungeon_view_palette_index < LIGHT_DARKEST &&
           amount < *tables_palette_light_threshold(g_dungeon_view_palette_index))
        g_dungeon_view_palette_index++;
}

/* update_map_coordinates_after_relative_movement: the square forward and right of (x, y) in direction dir. */
static void step(int16_t dir, int16_t forward, int16_t right, int16_t *x, int16_t *y)
{
    *x += forward * tables_step_east(dir) + right * tables_step_east(turn_right(dir));
    *y += forward * tables_step_north(dir) + right * tables_step_north(turn_right(dir));
}

/* draw_dungeon_view: ceiling, floor, the objects of D4, then the 16 squares back to front, then draw_viewport. */
int16_t view_wall_ornament(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (map != g_party_map_index)
        return VIEW_ORNAMENT_NONE;
    aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || !a.far_side)
        return VIEW_ORNAMENT_NONE;
    int16_t i = a.far_side - 1;
    if (i == inscription)
        return VIEW_ORNAMENT_TEXT;
    if (i == vi_altar)
        return VIEW_ORNAMENT_VI_ALTAR;
    if (is_alcove(i))
        return VIEW_ORNAMENT_ALCOVE;
    if (i == fountain)
        return VIEW_ORNAMENT_FOUNTAIN;
    return VIEW_ORNAMENT_OTHER;
}

int16_t view_wall_face_ornament_index(int16_t x, int16_t y, int16_t face)
{
    Aspect a;
    if (map != g_party_map_index)
        return -1;
    aspect(&a, face, x, y);
    if (a.element != SQUARE_WALL || !a.far_side)
        return -1;
    return view_wall_ornament_index(a.far_side);
}

int16_t view_wall_ornament_index(uint16_t ordinal)
{
    if (map != g_party_map_index || ordinal < 1 || ordinal > map_wall_ornament_count(map))
        return -1;
    return (wall_orn[ordinal - 1].graphic - GFX_WALL_ORNAMENT_FIRST) / WALL_ORNAMENT_GRAPHICS;
}

bool view_floor_pressure_pad(int16_t x, int16_t y)
{
    enum { PRESSURE_PAD = 1, ROUND_PRESSURE_PAD = 4, TINY_PRESSURE_PAD = 7 }; /* floor ornament graphics */
    int16_t ornament = view_floor_ornament(x, y);
    return ornament == PRESSURE_PAD || ornament == ROUND_PRESSURE_PAD || ornament == TINY_PRESSURE_PAD;
}

int16_t view_floor_ornament(int16_t x, int16_t y)
{
    Aspect a;
    if (map != g_party_map_index)
        return -1;
    aspect(&a, DIRECTION_NORTH, x, y);
    if (a.element != SQUARE_CORRIDOR && a.element != SQUARE_PIT && a.element != SQUARE_TELEPORTER)
        return -1;
    int16_t i = (a.floor_ornament & ~FLOOR_ORNAMENT_WITH_FOOTPRINTS) - 1;
    return i < 0 ? -1 : (floor_orn[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS;
}

/* CSB magic map: a grid of 7 x 7 squares, each a 7 x 7 pixel tile with a 1 pixel border. */
enum {
    MAP_SQUARES = 7,
    MAP_HALF = 3,
    MAP_TILE_W = 7,
    MAP_TILE_H = 7,
    MAP_GAP_X = 1,
    MAP_GAP_Y = 1,
    MAP_TILES_PER_ROW = 16
};
enum { MAP_COLOR_PARTY = 8, MAP_COLOR_FOOTPRINTS = 4 };
/* Tiles of GFX_MAGIC_MAP_TILES. A tile with "+ cell" or "+ side" has 4 tiles, one per side relative to the map. */
enum {
    MAP_TILE_WALL,
    MAP_TILE_FAKE_WALL_BLINK,
    MAP_TILE_FLOOR,
    MAP_TILE_PIT,
    MAP_TILE_PIT_INVISIBLE,
    MAP_TILE_PIT_BLINK,
    MAP_TILE_TELEPORTER_FIRST = 6,
    MAP_TILE_DOOR_FRONT = 10,
    MAP_TILE_DOOR_SIDE = 11,
    MAP_TILE_DOOR_NO_BUTTON = 2,
    MAP_TILE_DOOR_OPEN = 4,
    MAP_TILE_STAIRS_FIRST = 18,
    MAP_TILE_ALCOVE_FIRST = 22,
    MAP_TILE_VI_ALTAR_FIRST = 26,
    MAP_TILE_FOUNTAIN_FIRST = 30,
    MAP_TILE_ZOKATHRA_FIRST = 34,
    MAP_TILE_IN_WALL = 4,
    MAP_TILE_LIGHTNING_FIRST = 42,
    MAP_TILE_LIGHTNING_ACROSS = 4,
    MAP_TILE_FIREBALL_FIRST = 50,
    MAP_TILE_SPELL_FIRST = 54,
    MAP_TILE_SLIME_FIRST = 58,
    MAP_TILE_POISON_FIRST = 62,
    MAP_TILE_EXPLOSION_FIRE = 66,
    MAP_TILE_EXPLOSION_OTHER = 67,
    MAP_TILE_POISON_CLOUD_FIRST = 68,
    MAP_TILE_FLUXCAGE_FIRST = 72,
    MAP_TILE_CREATURE_FIRST = 76,
    MAP_TILE_FLOOR_ORNAMENT_FIRST = 78,
};
enum { MAP_TIME_BLINK_PIT = 1, MAP_TIME_BLINK_WALL = 2 }; /* game time bits of the blinking tiles */

/* Draws one map tile at (px, py). */
static void map_tile(Bitmap *dst, int16_t tile, int16_t px, int16_t py)
{
    const Bitmap *tiles = gfx_bitmap(GFX_MAGIC_MAP_TILES);
    Box b = {px, py, MAP_TILE_W, MAP_TILE_H};
    if (tiles)
        draw_blit(tiles, tile % MAP_TILES_PER_ROW * MAP_TILE_W, tile / MAP_TILES_PER_ROW * MAP_TILE_H, dst, &b,
                  TRANSPARENT_COLOR, 0);
}

/* Footprints of the footprints spell on a floor square (the footprints bit of set_square_aspect). */
static bool map_footprints(const Aspect *a, int16_t x, int16_t y)
{
    int allowed = a->element == SQUARE_PIT ? a->square & PIT_IMAGINARY
                                           : a->element != SQUARE_WALL && a->element != ELEMENT_STAIRS_SIDE &&
                                                 a->element != ELEMENT_STAIRS_FRONT;
    uint16_t ordinal = allowed ? party_scent_ordinal(x, y) : 0;
    return ordinal && ordinal - 1 >= g_party.first_scent_index && ordinal - 1 < g_party.last_scent_index;
}

/* The tile of a flying thing, or -1: spell projectiles and a flying zokathra. */
static int16_t map_projectile_tile(uint16_t projectile, int16_t dir)
{
    uint16_t slot = rec_projectile_slot(projectile);
    switch (projectile_aspect(slot)) {
    case -PROJECTILE_ASPECT_FIREBALL:
        return MAP_TILE_FIREBALL_FIRST;
    case -PROJECTILE_ASPECT_SPELL:
        return MAP_TILE_SPELL_FIRST;
    case -PROJECTILE_ASPECT_POISON:
        return MAP_TILE_POISON_FIRST;
    case -PROJECTILE_ASPECT_SLIME:
        return MAP_TILE_SLIME_FIRST;
    case -PROJECTILE_ASPECT_LIGHTNING: {
        const TimelineEvent *event = &g_timeline_events[rec_projectile_event_index(projectile)];
        return MAP_TILE_LIGHTNING_FIRST +
               ((projectile_event_direction(event) & 1) != (dir & 1) ? MAP_TILE_LIGHTNING_ACROSS : 0);
    }
    }
    return projectile_aspect(slot) >= 0 && object_get_type(slot) == OBJECT_TYPE_ZOKATHRA ? MAP_TILE_ZOKATHRA_FIRST : -1;
}

/* The tile of an explosion, or -1: smoke and rebirth do not show. */
static int16_t map_explosion_tile(uint16_t explosion)
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
static void map_square_things(Bitmap *dst, const uint8_t *spells, const Aspect *a, int blink, int16_t dir, int16_t px,
                              int16_t py)
{
    uint16_t explosion = THING_ENDOFLIST, projectile = THING_ENDOFLIST, group = THING_ENDOFLIST;
    for (uint16_t t = a->first_thing; t != THING_ENDOFLIST; t = get_next_thing(t)) {
        int16_t type = THING_TYPE(t);
        if (type == THING_GROUP && spells[MAGIC_MAP_KU])
            group = t;
        else if (!spells[MAGIC_MAP_DAIN])
            continue;
        else if (type == THING_PROJECTILE && projectile == THING_ENDOFLIST)
            projectile = t;
        else if (type == THING_EXPLOSION && explosion == THING_ENDOFLIST)
            explosion = t;
        else if (type == THING_JUNK && object_get_type(t) == OBJECT_TYPE_ZOKATHRA)
            map_tile(dst,
                     MAP_TILE_ZOKATHRA_FIRST + NORMALIZE(THING_CELL(t) - dir) +
                         (a->element == SQUARE_WALL && !blink ? MAP_TILE_IN_WALL : 0),
                     px, py);
    }
    if (group != THING_ENDOFLIST && !(get_creature_attributes(group) & CREATURE_NOT_ON_MAP))
        map_tile(dst, MAP_TILE_CREATURE_FIRST + (g_game_time & 1), px, py);
    for (uint16_t t = projectile; t != THING_ENDOFLIST; t = get_next_thing(t)) {
        int16_t tile = THING_TYPE(t) == THING_PROJECTILE ? map_projectile_tile(t, dir) : -1;
        if (tile >= 0)
            map_tile(dst, tile + NORMALIZE(THING_CELL(t) - dir), px, py);
    }
    for (uint16_t t = explosion; t != THING_ENDOFLIST; t = get_next_thing(t)) {
        int16_t tile = THING_TYPE(t) == THING_EXPLOSION ? map_explosion_tile(t) : -1;
        if (tile >= 0)
            map_tile(dst, tile, px, py);
    }
}

/* The tiles of square (x, y) seen in direction dir at (px, py): the square, then with DAIN the footprints, the
   zokathra objects, the projectiles, and the explosions, with ROS the fake walls and invisible pits, with KU the
   creatures. */
static void map_square(Bitmap *dst, const uint8_t *spells, int16_t x, int16_t y, int16_t dir, int16_t px, int16_t py)
{
    Aspect a;
    aspect(&a, dir, x, y);
    bool blink = SQUARE_TYPE(a.square) == SQUARE_FAKE_WALL && (a.square & FAKE_WALL_IMAGINARY);
    int16_t color = x == g_party_map_x && y == g_party_map_y ? MAP_COLOR_PARTY
                    : spells[MAGIC_MAP_DAIN] && a.element != SQUARE_WALL && map_footprints(&a, x, y)
                        ? MAP_COLOR_FOOTPRINTS
                        : -1;
    if (color >= 0) {
        Box b = {px - MAP_GAP_X, py - MAP_GAP_Y, MAP_TILE_W + 2 * MAP_GAP_X, MAP_TILE_H + 2 * MAP_GAP_Y};
        draw_fill(dst, &b, (uint8_t)color);
    }
    switch (a.element) {
    case SQUARE_WALL:
        if (spells[MAGIC_MAP_ROS] && blink && (g_game_time & MAP_TIME_BLINK_WALL)) {
            map_tile(dst, MAP_TILE_FAKE_WALL_BLINK, px, py);
            break;
        }
        map_tile(dst, MAP_TILE_WALL, px, py);
        for (int16_t k = 0; k < WALL_SIDES; k++) {
            int16_t i = a.side[k] - 1;
            int16_t first = !a.side[k]      ? -1
                            : i == vi_altar ? MAP_TILE_VI_ALTAR_FIRST
                            : is_alcove(i)  ? MAP_TILE_ALCOVE_FIRST
                            : i == fountain ? MAP_TILE_FOUNTAIN_FIRST
                                            : -1;
            if (first >= 0)
                map_tile(dst, first + k, px, py);
        }
        break;
    case SQUARE_CORRIDOR: {
        map_tile(dst, MAP_TILE_FLOOR, px, py);
        int16_t i = (a.floor_ornament & ~FLOOR_ORNAMENT_WITH_FOOTPRINTS) - 1;
        if (i >= 0)
            map_tile(dst,
                     MAP_TILE_FLOOR_ORNAMENT_FIRST +
                         (floor_orn[i].graphic - GFX_FLOOR_ORNAMENT_FIRST) / FLOOR_ORNAMENT_GRAPHICS,
                     px, py);
        break;
    }
    case SQUARE_TELEPORTER:
        map_tile(dst, MAP_TILE_TELEPORTER_FIRST + random_2bits(), px, py);
        break;
    case ELEMENT_DOOR_FRONT:
    case ELEMENT_DOOR_SIDE: {
        int16_t tile = a.element == ELEMENT_DOOR_SIDE ? MAP_TILE_DOOR_SIDE : MAP_TILE_DOOR_FRONT;
        if (!door_button_present(THING_MAKE(THING_DOOR, a.door)))
            tile += MAP_TILE_DOOR_NO_BUTTON;
        if (a.state == DOOR_OPEN || a.state == DOOR_DESTROYED)
            tile += MAP_TILE_DOOR_OPEN;
        map_tile(dst, tile, px, py);
        break;
    }
    case ELEMENT_STAIRS_SIDE:
    case ELEMENT_STAIRS_FRONT: {
        int16_t side = NORMALIZE(get_stairs_exit_direction(x, y) - dir);
        if (a.state)
            side = turn_back(side);
        map_tile(dst, MAP_TILE_STAIRS_FIRST + side, px, py);
        break;
    }
    case SQUARE_PIT:
        map_tile(dst,
                 !a.state                                                      ? MAP_TILE_PIT
                 : spells[MAGIC_MAP_ROS] && (g_game_time & MAP_TIME_BLINK_PIT) ? MAP_TILE_PIT_BLINK
                                                                               : MAP_TILE_PIT_INVISIBLE,
                 px, py);
        break;
    }
    if (spells[MAGIC_MAP_KU] || spells[MAGIC_MAP_DAIN])
        map_square_things(dst, spells, &a, blink, dir, px, py);
}

/* The magic map of a champion at (px, py) of dst: 7 x 7 squares around the party, or around the square that
   NETA keeps. The party square is drawn last. The ornaments are those of the party map, as in the original. */
void view_draw_magic_map(Bitmap *dst, const uint8_t *spells, int16_t px, int16_t py)
{
    int16_t m = g_party_map_index, x = g_party_map_x, y = g_party_map_y, dir = g_party_direction,
            saved_map = g_current_map_index;
    int16_t party_px = -1, party_py = 0;
    if (map != g_party_map_index)
        view_load_map(g_party_map_index);
    if (spells[MAGIC_MAP_NETA]) {
        game_get_magic_map_position(spells, &m, &x, &y);
        dir = spells[MAGIC_MAP_NETA] - 1;
    }
    set_current_map(m);
    map = m;
    mw = dungeon_map_width(m);
    mh = dungeon_map_height(m);
    step(dir, MAP_HALF, -MAP_HALF, &x, &y);
    for (int16_t row = 0; row < MAP_SQUARES; row++, py += MAP_TILE_H + MAP_GAP_Y) {
        int16_t sx = x, sy = y, cx = px;
        for (int16_t col = 0; col < MAP_SQUARES; col++, cx += MAP_TILE_W + MAP_GAP_X) {
            if (m == g_party_map_index && sx == g_party_map_x && sy == g_party_map_y) {
                party_px = cx;
                party_py = py;
            } else {
                map_square(dst, spells, sx, sy, dir, cx, py);
            }
            step(dir, 0, 1, &sx, &sy);
        }
        step(dir, -1, 0, &x, &y);
    }
    if (party_px >= 0)
        map_square(dst, spells, g_party_map_x, g_party_map_y, dir, party_px, party_py);
    map = g_party_map_index;
    mw = dungeon_map_width(map);
    mh = dungeon_map_height(map);
    if (saved_map >= 0)
        set_current_map(saved_map);
}

void view_draw(int16_t dir, int16_t x, int16_t y)
{
    static void (*const squares[DRAWN_SQUARE_COUNT])(int16_t, int16_t, int16_t) = {
        d3l2, d3r2, d3l, d3r, d3c, d2l2, d2r2, d2l, d2r, d2c, d1l, d1r, d1c, d0l, d0r, d0c};
    static const int8_t forward[DRAWN_SQUARE_COUNT] = {3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 0, 0, 0},
                        right[DRAWN_SQUARE_COUNT] = {-2, 2, -1, 1, 0, -2, 2, -1, 1, 0, -1, 1, 0, -1, 1, 0};
    static const int8_t d4_right[D4_SQUARE_COUNT] = {-1, 1, 0};
    static const int16_t d4_squares[D4_SQUARE_COUNT] = {VIEW_SQUARE_D4L, VIEW_SQUARE_D4R, VIEW_SQUARE_D4C};
    static uint8_t px[VIEWPORT_MAX_H * VIEWPORT_MAX_W];
    vp = (Bitmap){tables_viewport_width(), tables_viewport_height(), px};
    if (map < 0 || (size_t)vp.w * vp.h > sizeof px)
        return;
    memset(px, 0, sizeof px);
    if (!motion_steps)
        start_tracks(dir, x, y);
    for (int i = 0; i < VIEW_CLICK_BOX_COUNT; i++)
        g_dungeon_view_clickable_boxes[i].x = -1;
    memset(g_pile_object_counts, 0, sizeof g_pile_object_counts);
    reset_wall_set_ids();
    parity = (dir + x + y) & 1;
    if (parity) { /* the mirrored-walls memory option is on, as on a 1 MB Amiga */
        memcpy(&wall_set_ids[WS_WALL_FIRST], &wall_set_ids[WS_MIRRORED_WALL_FIRST],
               WALL_COUNT * sizeof wall_set_ids[0]);
        wall(wall_set_ids[WS_CEILING], ZONE_CEILING, BLIT_OPAQUE);
        wall(wall_set_ids[WS_FLOOR], ZONE_FLOOR, BLIT_FLIPPED);
    } else {
        wall(wall_set_ids[WS_CEILING], ZONE_CEILING, BLIT_FLIPPED);
        wall(wall_set_ids[WS_FLOOR], ZONE_FLOOR, BLIT_OPAQUE);
    }
    for (int i = 0; i < D4_SQUARE_COUNT; i++) { /* objects only on D4L, D4R, D4C */
        int16_t sx = x, sy = y;
        step(dir, DEPTH_D4, d4_right[i], &sx, &sy);
        things(dungeon_square_first_thing(map, sx, sy), dir, sx, sy, d4_squares[i], CELLS_BL);
    }
    for (int i = 0; i < DRAWN_SQUARE_COUNT; i++) {
        int16_t sx = x, sy = y;
        step(dir, forward[i], right[i], &sx, &sy);
        squares[i](dir, sx, sy);
    }
    Bitmap screen = {SCREEN_W, SCREEN_H, &fb[0][0]};
    int16_t w = vp.w, h = vp.h;
    Box b;
    if (layout_get_zone(ZONE_VIEWPORT, vp.w, vp.h, &b, &w, &h))
        draw_blit(&vp, w, h, &screen, &b, -1, 0);
}

/* Draws the view of (map, x, y, dir) into fb with the palette of the darkness that view_set_light set.
   Returns true on success. */
bool view_render(int16_t m, int16_t x, int16_t y, int16_t dir)
{
    if (!g_dungeon_maps || m < 0 || m >= dungeon_map_count())
        return false;
    if (m != map)
        view_load_map(m);
    int16_t light = g_cheat_light ? 0 : g_dungeon_view_palette_index;
    memcpy(palette, pals[light], sizeof pals[light]);
    view_draw(NORMALIZE(dir), x, y);
    return true;
}
