#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "group.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "projectile.h"
#include "tables.h"
#include "things.h"
#include "graphics.h"
#include "layout.h"
#include "random.h"
#include "video.h"
#include "view.h"

/* KAOS globals (original addresses). */
#define DUNGEON_PALETTE 0x100aa6    /* g_dungeon_palettes[0], full light */
#define STEP_EAST 0x101038          /* g_direction_to_step_east[4] */
#define STEP_NORTH 0x101040         /* g_direction_to_step_north[4] */
#define DOOR_INFO 0x1016e2          /* g_door_info[], high byte attributes */
#define LAYOUT_BITMAPS 0x1025f2     /* g_layout_bitmaps[], 14 bytes, index -1 - id */
#define VIEWPORT_SIZE 0x102948      /* g_viewport_width, g_viewport_height */
#define PORTRAIT_SIZE 0x102958      /* g_portrait_width, g_portrait_height */
#define GLYPH_SIZE 0x10296e         /* wall text glyph width, height */
#define WALL_SET_IDS 0x102a22       /* g_wall_set_layout_ids[46] */
#define WALL_SET_FLIPPED 0x102a60   /* g_wall_set_flipped_layout_ids[15] */
#define WALL_ORN_DEPTH 0x102ba6     /* depth of each wall ornament view (0-4) */
#define FLOOR_ORN_VIEW 0x102bb4     /* graphic offset of each floor ornament view */
#define ALCOVES 0x102bc0            /* g_alcove_ornament_indices[3] */
#define FOUNTAINS 0x102bc3          /* g_fountain_ornament_indices[1] */
#define ORNAMENT_VI_ALTAR 2         /* the alcove ornament that is a vi altar */
#define WALL_ORN_COORDS 0x102bc4    /* g_wall_ornament_coord_sets */
#define FLOOR_ORN_COORDS 0x102c00   /* g_floor_ornament_coord_sets */
#define DOOR_ORN_COORDS 0x102c0a    /* g_door_ornament_coord_sets */
#define DOOR_BUTTON_COORDS 0x102c16 /* g_door_button_coord_sets */
#define PAL_D3 0x102c18             /* g_palette_changes_d3 */
#define PAL_D2 0x102c28             /* g_palette_changes_d2 */
#define PAL_DOOR_D3 0x102c38        /* g_palette_changes_door_ornament_d3 */
#define PAL_DOOR_D2 0x102c48        /* g_palette_changes_door_ornament_d2 */
#define TEXT_HEIGHTS 0x102c57       /* visible plaque height by line count and depth */
#define LIGHT_THRESHOLDS 0x100cd4   /* g_palette_light_thresholds[6] */
#define GROUP_DIRS 0x10192a         /* g_group_directions_packed[4] */
#define VS_LANE 0x102a7e            /* g_view_square_lane[24] */
#define VS_DEPTH 0x102a96           /* g_view_square_depth[24] */
#define VS_OBJECT 0x102aae          /* g_view_square_object_coord_index[24] */
#define VS_ALCOVE 0x102ac6          /* g_view_square_alcove_object_coord_index[24] */
#define VS_CREATURE 0x102ade        /* g_view_square_creature_coord_index[24] */
#define VS_EXPLOSION 0x102af6       /* g_view_square_explosion_coord_index[24] */
#define VS_FIELD 0x102b0e           /* g_view_square_field_aspect_index[24] */
#define FIELD_ASPECTS 0x102b26      /* g_field_aspects[12], 8 bytes */
#define OBJECT_ASPECTS 0x102c68     /* g_object_aspects[85], 4 bytes */
#define PROJECTILE_ASPECTS 0x102db8 /* g_projectile_aspects[], 4 bytes, index 1-14 */
#define PAL_SMOKE 0x102df4          /* g_palette_changes_smoke */
#define PAL_OBJECT_D3 0x102e04      /* object and projectile palette changes, D3 */
#define PAL_OBJECT_D2 0x102e14      /* object and projectile palette changes, D2 */
#define OBJECT_SCALES 0x102e24      /* object scale by depth and row */
#define PROJECTILE_SCALES 0x102e2a  /* projectile scale by depth and row */
#define EXPLOSION_SCALES 0x102e32   /* explosion scale by depth */
#define PILE_SHIFTS 0x102e38        /* shift index pairs of stacked objects */
#define CREATURE_ASPECTS 0x102e58   /* g_creature_aspects[27], 6 bytes */
#define REPLACEMENT_COLORS 0x102efa /* g_creature_replacement_color_sets[13], 14 bytes */
#define REPLACEMENT_SET_SIZE 14
#define PAL_CREATURE_D3 0x102fb0    /* g_palette_changes_creature_d3 */
#define PAL_CREATURE_D2 0x102fc0    /* g_palette_changes_creature_d2 */
#define SHIFT_SETS 0x102fd0         /* g_creature_shift_sets[3][8] */
#define SHIFT_SET_SIZE 8
enum { PILE_FLOOR_SHIFTS = 16, PILE_ODD_CELL = 8, PILE_ALCOVE_FIRST = 2, PILE_ALCOVE_LAST = 13 }; /* PILE_SHIFTS pair indices */
#define REBIRTH_SCALES 0x102fe8     /* rebirth step 2, then step 1 scale by object coordinate set */
#define REBIRTH_STEP_1_SCALES 8

#define VIEWPORT_ZONE 7
#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
#define LID(a) ids[((a) - WALL_SET_IDS) / 2] /* layout id stored at an original address */

enum { MAP_DIFFICULTY = 12, MAP_COUNTS = 13 }; /* map descriptor bytes, high nibble: difficulty, creature type count */
enum { NORMAL, OPAQUE, FLIPPED };
enum { EL, SQ, THING, ST, RT, FR, LT }; /* set_square_aspect words */
enum { WALL, CORRIDOR, PIT, STAIRS, DOOR, TELEPORTER, FAKEWALL };
enum { DOOR_SIDE = 0x10, DOOR_FRONT, STAIRS_SIDE, STAIRS_FRONT };

enum { LIGHT_LEVELS = 6, LIGHT_DARKEST = LIGHT_LEVELS - 1, LIGHT_FULL = 99 }; /* party light amount 99: brightest */

typedef struct {
    uint16_t v[7];
    uint16_t text;    /* g_inscription_thing */
    uint16_t portrait; /* g_champion_portrait_ordinal */
} Aspect;

static Bitmap vp;                             /* g_viewport_bitmap */
static int16_t map = -1, mw, mh;              /* current map and its size */
static const uint8_t *desc;                   /* map descriptor, 16 bytes */
static int16_t ids[46];                       /* g_wall_set_layout_ids, swapped for mirrored views */
static int16_t slot_gfx[64];                  /* graphic of each layout bitmap slot */
static uint8_t slot_flip[64];                 /* slot holds the mirrored graphic */
static int16_t wall_orn[16][2], floor_orn[16][2], door_orn[17][2]; /* graphic, coordinate set */
static int16_t alcoves[3], inscription, stairs, door_gfx[3][2];
static uint16_t door_attr[2];
static int16_t parity, rel_x, rel_y; /* g_use_flipped_wall_bitmaps, g_zone_relative_x/y */
static uint16_t pals[LIGHT_LEVELS][16];                  /* g_dungeon_palettes with the creature replacement colors */
static uint8_t creature_d3[16], creature_d2[16]; /* g_palette_changes_creature_d3/d2 of the map */
static int16_t light = LIGHT_FULL;                  /* party light amount */
static Box drawn_box;                        /* g_drawn_box: the box of the last in_zone */
static int16_t vi_altar, fountain;           /* wall ornament index of the vi altar and the fountain, or -1 */
int16_t g_do_not_draw_fluxcages;
Box g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_COUNT];
uint16_t g_pile_top_objects[VIEW_CLICK_BOX_WALL];
int16_t g_square_ahead_element;
int16_t g_facing_alcove, g_facing_fountain, g_facing_vi_altar;

/* Square byte of the current map; outside, a wall with the "ornament allowed" bit of get_square. */
static uint8_t square(int16_t x, int16_t y)
{
    int inx = x >= 0 && x < mw, iny = y >= 0 && y < mh;
    if (inx && iny) return dungeon_square(map, x, y);
    if (inx) {
        if (y == -1 && (dungeon_square(map, x, 0) >> 5 == 1 || dungeon_square(map, x, 0) >> 5 == 2)) return 2;
        if (y == mh && (dungeon_square(map, x, mh - 1) >> 5 == 1 || dungeon_square(map, x, mh - 1) >> 5 == 2)) return 8;
    } else if (iny) {
        if (x == -1 && (dungeon_square(map, 0, y) >> 5 == 1 || dungeon_square(map, 0, y) >> 5 == 2)) return 4;
        if (x == mw && (dungeon_square(map, mw - 1, y) >> 5 == 1 || dungeon_square(map, mw - 1, y) >> 5 == 2)) return 1;
    }
    return 0;
}

static int16_t is_alcove(int16_t i) { return i >= 0 && (i == alcoves[0] || i == alcoves[1] || i == alcoves[2]); }

/* get_random_ornament_ordinal: a pseudo-random ornament from the square position. */
static uint16_t random_ordinal(int allowed, int16_t count, int16_t x, int16_t y, uint16_t modulo)
{
    if (!allowed) return 0;
    uint16_t a = (uint16_t)(y + x * 32 + 2000), b = (uint16_t)(mh + mw + map * 64 + 3000);
    uint16_t i = (uint16_t)((uint16_t)(BE16(g_dungeon_header) + ((uint16_t)(a * 31417u) >> 1) + (uint16_t)(b * 11u)) >> 2) % modulo;
    return (int16_t)i < count ? i + 1 : 0;
}

/* set_square_aspect without the footprints bit. */
static void aspect(Aspect *a, int16_t dir, int16_t x, int16_t y)
{
    memset(a, 0, sizeof *a);
    uint16_t t = dungeon_square_first_thing(map, x, y);
    uint8_t s = square(x, y);
    int floor_scan = 0, wall_scan = 0;
    a->v[EL] = s >> 5;
    a->v[SQ] = s;
    switch (s >> 5) {
    case WALL:
        for (int k = 0; k < 4; k++) /* back, right, front, left */
            a->v[ST + k] = random_ordinal(s & 8 >> ((dir + k) & 3), desc[0xb] >> 4, x, (int16_t)((((dir + k) & 3) + 1) * (y + 1)), 30);
        if (x < 0 || x >= mw || y < 0 || y >= mh)
            for (int k = ST; k <= LT; k++)
                if (is_alcove(a->v[k] - 1)) a->v[k] = 0;
        wall_scan = 1;
        break;
    case FAKEWALL:
        if (!(s & 4)) {
            a->v[EL] = WALL;
            wall_scan = 2;
            break;
        }
        a->v[EL] = CORRIDOR;
        s &= 8;
        /* fall through */
    case CORRIDOR:
        a->v[FR] = random_ordinal(s & 8, desc[10] >> 4, x, y, 30);
        floor_scan = 1;
        break;
    case PIT:
        if (!(s & 8)) a->v[EL] = CORRIDOR;
        else a->v[ST] = s & 4;
        floor_scan = 1;
        break;
    case STAIRS:
        a->v[EL] = (dir & 1) == (s & 8) >> 3 ? STAIRS_SIDE : STAIRS_FRONT;
        a->v[ST] = s & 4;
        break;
    case DOOR:
        a->v[EL] = (dir & 1) == (s & 8) >> 3 ? DOOR_SIDE : DOOR_FRONT;
        a->v[ST] = s & 7;
        a->v[RT] = t & 0x3ff;
        break;
    case TELEPORTER:
        if (!(s & 8) || !(s & 4)) a->v[EL] = CORRIDOR;
        floor_scan = 1;
        break;
    }
    for (; t != THING_END && THING_TYPE(t) <= THING_SENSOR; t = get_next_thing(t)) {
        const uint8_t *d = get_thing_data(t);
        if (floor_scan && (t >> 10 & 15) == 3) a->v[FR] = d[4] >> 4;
        if (!wall_scan) continue;
        int side = ST + ((t >> 14) - dir & 3);
        if ((t >> 10 & 15) == 2) {
            if (d[3] & 1) {
                a->v[side] = inscription + 1;
                a->text = t;
            }
        } else {
            a->v[side] = d[4] >> 4;
            if ((BE16(d + 2) & 0x7f) == 0x7f) a->portrait = (BE16(d + 2) >> 7) + 1;
        }
    }
    a->v[THING] = wall_scan == 2 && g_party_map_x != x && g_party_map_y != y ? 0xfffe : t;
}

/* draw_wall_set_bitmap and its _opaque and _flipped variants. id >= 0 is a graphic index. */
static void wall(int16_t id, int16_t zone, int mode)
{
    int16_t g = id, flip = mode == FLIPPED, sx = 0, sy = 0, w = 0, h = 0;
    if (id < 0) {
        uint32_t a = LAYOUT_BITMAPS + 14 * (-1 - id);
        g = slot_gfx[-1 - id];
        flip ^= slot_flip[-1 - id];
        sx = kaos_s16(a + 2);
        sy = kaos_s16(a + 4);
        w = kaos_s16(a + 6);
        h = kaos_s16(a + 8);
    }
    const Bitmap *bm = gfx_bitmap(g);
    Box b;
    if (bm && layout_get_zone(zone, bm->w, bm->h, &b, &w, &h))
        draw_blit(bm, w + sx, h + sy, &vp, &b, mode == OPAQUE ? -1 : 10, flip);
}

/* draw_bitmap_in_zone. Bit 14 of zone starts from g_zone_relative_x/y. Returns 1 and sets
   drawn_box (g_drawn_box) when it drew. */
static int in_zone(const Bitmap *bm, Bitmap *dst, int16_t zone, int flip, int16_t transparent)
{
    int16_t w = 0, h = 0;
    Box b;
    if (!bm || zone == -1) return 0;
    if (zone & 0xc000) {
        w = rel_x;
        h = rel_y;
        zone &= (int16_t)0xbfff;
    }
    if (!layout_get_zone(zone, bm->w, bm->h, &b, &w, &h)) return 0;
    draw_blit(bm, w, h, dst, &b, transparent, flip);
    drawn_box = b;
    return 1;
}

#define S8(a) ((int16_t)(int8_t)kaos_u8(a)) /* signed table byte */
#define THING_INDEX(thing) ((thing) & 0x3ff) /* record index without type and cell */

enum { VIEW_CELL_BACK_LEFT, VIEW_CELL_BACK_RIGHT, VIEW_CELL_FRONT_RIGHT, VIEW_CELL_FRONT_LEFT, VIEW_CELL_CENTER };
enum { VIEW_SQUARE_D0C, VIEW_SQUARE_D0L, VIEW_SQUARE_D0R, VIEW_SQUARE_D4C = 16, VIEW_SQUARE_D4L, VIEW_SQUARE_D4R };
enum { FACING_AWAY, FACING_LEFT, FACING_TOWARD, FACING_RIGHT }; /* creature direction relative to the party */
enum { DEPTH_D0, DEPTH_D1, DEPTH_D2, DEPTH_D3, DEPTH_D4 };
enum { SCALE_D3 = 14, SCALE_D2 = 21, SCALE_NATIVE = 32, SCALE_EXPLOSION_MIN = 4 };
enum { TRANSPARENT_COLOR = 10 };
enum { DOOR_PASS_BACK = 1, DOOR_PASS_FRONT = 2 }; /* door squares draw things behind, then in front of the door */
enum { CELL_ORDER_DOOR_PASS = 8 };
enum { CLICK_BOX_MIN_W = 14, CLICK_BOX_MIN_H = 11, CLICK_BOX_CENTER_H = 15, CLICK_BOX_CENTER_OFFSET = 7 }; /* smallest clickable object box */

/* Creature graphic info bits. */
enum {
    CREATURE_FLIP_NON_ATTACK = 0x4, CREATURE_HAS_SIDE = 0x8, CREATURE_HAS_BACK = 0x10, CREATURE_HAS_ATTACK = 0x20,
    CREATURE_SPECIAL_D2_FRONT = 0x80, CREATURE_D2_FRONT_FLIPPED = 0x100,
};
enum { ASPECT_SHIFT_MASK = 0x7, ASPECT_SHIFT_Y = 3 }; /* active group creature aspect: shift index x, then y */

/* Creature aspect record (6 bytes): graphic word, coordinate set and transparent color, replacement color sets. */
enum { CREATURE_ASPECT_GRAPHIC = 0, CREATURE_ASPECT_COORD = 4, CREATURE_ASPECT_REPLACEMENT = 5, CREATURE_ASPECT_SIZE = 6 };
enum { CREATURE_ASPECT_TRANSPARENT = 0xf, CREATURE_ASPECT_COORD_SHIFT = 4 };

/* Object aspect record (4 bytes): graphic, flags, alcove coordinate set. */
enum { OBJECT_ASPECT_GRAPHIC = 0, OBJECT_ASPECT_FLAGS = 2, OBJECT_ASPECT_ALCOVE_COORD = 3, OBJECT_ASPECT_SIZE = 4 };
enum { OBJECT_FLIP_ON_RIGHT = 0x1, OBJECT_HAS_ALCOVE_GRAPHIC = 0x10 };

/* Projectile aspect record (4 bytes): graphic, scale flags, pose flags. */
enum { PROJECTILE_ASPECT_GRAPHIC = 0, PROJECTILE_ASPECT_SCALE_FLAGS = 2, PROJECTILE_ASPECT_POSE_FLAGS = 3, PROJECTILE_ASPECT_SIZE = 4 };
enum { PROJECTILE_SCALE_WITH_ENERGY = 0x1, PROJECTILE_FLIP_ON_LEFT = 0x10, PROJECTILE_KIND = 0x3 };
enum { PROJECTILE_BACK_AND_ROTATION, PROJECTILE_BACK, PROJECTILE_NO_BACK, PROJECTILE_NO_BACK_NO_ROTATION };
enum { POSE_FRONT, POSE_BACK, POSE_SIDE }; /* graphic offset from the projectile aspect graphic */
enum { PROJECTILE_ASPECT_LIGHTNING = 4, PROJECTILE_ASPECT_FIREBALL = 11, PROJECTILE_ASPECT_SPELL = 12, PROJECTILE_ASPECT_SLIME = 13, PROJECTILE_ASPECT_POISON = 14 };
enum { WEAPON_PROJECTILE_ASPECT_SHIFT = 8, WEAPON_PROJECTILE_ASPECT_MASK = 0x1f }; /* 0: the object aspect */
enum { KINETIC_ENERGY_MAX = 255, PROJECTILE_ENERGY_MIN = 0x60, PROJECTILE_SCALE_MIN = 2 };

/* Explosion graphics. */
enum { EXPLOSION_ASPECT_FIRE, EXPLOSION_ASPECT_SPELL, EXPLOSION_ASPECT_POISON, EXPLOSION_ASPECT_SMOKE };
enum { EXPLOSION_ATTACK_MIN = 0x30, EXPLOSION_STRENGTH_SHIFT = 5, EXPLOSION_PATTERN_STRENGTHS = 3 };

/* Field aspect bytes and the masked fill arguments. */
enum { FIELD_PATTERN, FIELD_LAST_UNIT, FIELD_TRANSPARENT, FIELD_MASK, FIELD_WIDTH, FIELD_HEIGHT, FIELD_ASPECT_SIZE = 8 };
enum { FIELD_NO_MASK = 0xff, FIELD_MASK_FLIPPED = 0x80, FIELD_FIRST_UNIT_RANGE = 0x1f };
enum { EXPLOSION_FILL_LAST_UNIT = 0x57, EXPLOSION_FILL_FIRST_UNIT_RANGE = 0x3f };
#define EXPLOSION_FILL_TRANSPARENT (FILL_NO_MASK | TRANSPARENT_COLOR)

/* The bitmap of graphic g at scale/32 with palette changes; the native bitmap at full scale without
   changes. Mirroring happens at blit time. Free out->px after use. */
static const Bitmap *shrunk(int16_t g, int16_t scale, const uint8_t *pal, Bitmap *out)
{
    const Bitmap *bm = gfx_bitmap(g);
    *out = (Bitmap){ 0 };
    if (!bm || (scale == SCALE_NATIVE && !pal)) return bm;
    if (draw_shrink(bm, (int16_t)((scale >> 1) + bm->w * scale) >> 5, (int16_t)((scale >> 1) + bm->h * scale) >> 5, pal, out)) return NULL;
    return out;
}

/* Palette changes of objects and projectiles: none near, then the D2 and D3 tables. */
static const uint8_t *object_pal(int16_t distance)
{
    return distance == 1 ? kaos_ptr(PAL_OBJECT_D2) : distance == 2 ? kaos_ptr(PAL_OBJECT_D3) : NULL;
}

static int16_t object_aspect(uint16_t thing) { return tables_object_info(get_object_info_index(thing))->aspect_index; }

/* The object aspect of a flying thing, or minus its projectile aspect. */
static int16_t projectile_aspect(uint16_t thing)
{
    if (THING_TYPE(thing) == THING_EXPLOSION) {
        switch (thing - EXPLOSION_THING_BASE) {
        case EXPLOSION_FIREBALL: return -PROJECTILE_ASPECT_FIREBALL;
        case EXPLOSION_SLIME: return -PROJECTILE_ASPECT_SLIME;
        case EXPLOSION_LIGHTNING_BOLT: return -PROJECTILE_ASPECT_LIGHTNING;
        case EXPLOSION_POISON_BOLT: case EXPLOSION_POISON_CLOUD: return -PROJECTILE_ASPECT_POISON;
        default: return -PROJECTILE_ASPECT_SPELL;
        }
    }
    if (THING_TYPE(thing) == THING_WEAPON) {
        int16_t aspect = tables_weapon_info(thing_weapon_type(thing))->attributes >> WEAPON_PROJECTILE_ASPECT_SHIFT & WEAPON_PROJECTILE_ASPECT_MASK;
        if (aspect) return -aspect;
    }
    return object_aspect(thing);
}

/* The pile box of a near object grows by the drawn box; a single object gets at least
   CLICK_BOX_MIN_W x CLICK_BOX_MIN_H pixels around its center. The object becomes the pile top. */
static void add_clickable_object(int16_t index, uint16_t thing)
{
    Box *c = &g_dungeon_view_clickable_boxes[index];
    if (c->x == -1) {
        *c = drawn_box;
        if (c->h < CLICK_BOX_CENTER_H) {
            if (c->h < CLICK_BOX_MIN_H) c->h = CLICK_BOX_MIN_H;
            c->y += (drawn_box.h >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
        if (c->w < CLICK_BOX_MIN_W) {
            c->w = CLICK_BOX_MIN_W;
            c->x += (drawn_box.w >> 1) - CLICK_BOX_CENTER_OFFSET;
        }
    } else {
        int16_t right = max16(c->x + c->w, drawn_box.x + drawn_box.w), bottom = max16(c->y + c->h, drawn_box.y + drawn_box.h);
        c->x = min16(c->x, drawn_box.x);
        c->y = min16(c->y, drawn_box.y);
        c->w = right - c->x;
        c->h = bottom - c->y;
    }
    g_pile_top_objects[index] = thing;
}

/* One object on the floor or in an alcove. pile counts the objects of the cell for their shift;
   NULL draws an object in flight at zone without shift. */
static void object(uint16_t thing, int16_t aspect_index, int16_t cell, int16_t lane, int16_t depth, int alcove_front, int16_t zone, int16_t *pile, int16_t coord, int alcove)
{
    const uint8_t *aspect = kaos_ptr(OBJECT_ASPECTS + OBJECT_ASPECT_SIZE * aspect_index);
    int alcove_graphic = alcove_front && (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_HAS_ALCOVE_GRAPHIC);
    int16_t g = GFX_OBJECT_FIRST + aspect[OBJECT_ASPECT_GRAPHIC] + alcove_graphic, distance = 0;
    int flip = (aspect[OBJECT_ASPECT_FLAGS] & OBJECT_FLIP_ON_RIGHT) && !alcove_graphic && (lane >= 1 || (lane == 0 && (cell == VIEW_CELL_BACK_RIGHT || cell == VIEW_CELL_FRONT_RIGHT)));
    Bitmap own;
    const Bitmap *bm;
    if (depth == DEPTH_D0) bm = shrunk(g, SCALE_NATIVE, NULL, &own);
    else {
        int16_t row = depth * 2 - 1 - (cell == VIEW_CELL_CENTER ? 1 : cell >> 1); /* front cells count as one row nearer */
        distance = (row + 1) >> 1;
        bm = shrunk(g, kaos_u8(OBJECT_SCALES + row), object_pal(distance), &own);
    }
    if (pile) {
        zone = ZONE_SHIFTED | (alcove ? ZONE_ALCOVE_OBJECT_FIRST + coord + aspect[OBJECT_ASPECT_ALCOVE_COORD] * ZONE_ALCOVE_VIEW_SETS : ZONE_OBJECT_FIRST + cell + coord * ZONE_OBJECT_CELLS);
        rel_x = S8(SHIFT_SETS + kaos_u8(PILE_SHIFTS + 2 * *pile) + distance * SHIFT_SET_SIZE);
        rel_y = S8(SHIFT_SETS + kaos_u8(PILE_SHIFTS + 2 * *pile + 1) + distance * SHIFT_SET_SIZE);
        ++*pile;
        if (!alcove) *pile &= PILE_FLOOR_SHIFTS - 1;
        else if (*pile > PILE_ALCOVE_LAST) *pile = PILE_ALCOVE_FIRST;
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
    uint8_t index = group_cells_byte(thing);
    s->thing = thing;
    s->info = group_info(thing);
    s->aspect = kaos_ptr(CREATURE_ASPECTS + CREATURE_ASPECT_SIZE * s->info->aspect_index);
    s->size = creature_size(s->info);
    const ActiveGroup *a = g_active_groups && map == g_party_map_index && index < g_max_active_group_count ? &g_active_groups[index] : NULL;
    if (a && a->group_index == THING_INDEX(thing)) {
        s->cells = a->cells;
        s->dirs = a->directions;
        memcpy(s->look, a->aspect, sizeof s->look);
    } else {
        s->cells = index;
        s->dirs = kaos_u8(GROUP_DIRS + group_direction(thing));
        memset(s->look, 0, sizeof s->look);
    }
}

static uint16_t creature_cell(const Group *s, int16_t creature) { return s->cells >> 2 * creature & 3; }
static uint16_t creature_direction(const Group *s, int16_t creature) { return s->dirs >> 2 * creature & 3; }

/* The creature number + 1 that stands in a map cell, or 0. A half-square creature fills two cells. */
static int16_t ordinal_in_cell(const Group *s, uint16_t cell)
{
    if (s->cells == CELLS_CENTERED) return 1;
    int16_t i = group_last_creature_index(s->thing);
    if (s->size == CREATURE_SIZE_HALF) {
        if ((cell & 1) == (s->dirs & 1)) cell = (cell - 1) & 3;
        for (; i >= 0; i--)
            if (cell == creature_cell(s, i) || ((cell + 1) & 3) == creature_cell(s, i)) return i + 1;
    } else
        for (; i >= 0; i--)
            if (cell == creature_cell(s, i)) return i + 1;
    return 0;
}

/* One creature: front, side, back, or attack graphic for its facing, scaled and recolored by depth,
   at position pos (view cell, or 4 centered) of the creature coordinate set coord. */
static void creature(const Group *s, uint8_t look, uint16_t facing, int16_t pos, int16_t coord, int16_t depth)
{
    uint16_t info = s->info->graphic_info;
    int16_t g = GFX_CREATURE_FIRST + BE16(s->aspect + CREATURE_ASPECT_GRAPHIC), distance = 0;
    int side = (info & CREATURE_HAS_SIDE) && (facing & 1), back = 0, attack = 0, front_flip = 0;
    if (side) g++;
    else {
        back = (info & CREATURE_HAS_BACK) && facing == FACING_AWAY;
        attack = !back && (look & ASPECT_ATTACKING) && (info & CREATURE_HAS_ATTACK);
        if (attack) g += 1 + !!(info & CREATURE_HAS_SIDE) + !!(info & CREATURE_HAS_BACK);
        else if (back) g += 1 + !!(info & CREATURE_HAS_SIDE);
        else front_flip = (info & CREATURE_FLIP_NON_ATTACK) && (look & ASPECT_FLIPPED);
    }
    int special_d2 = depth == DEPTH_D2 && (info & CREATURE_SPECIAL_D2_FRONT) && !side && !back && !attack;
    int flip = (side && facing == FACING_LEFT) || (attack && (look & ASPECT_FLIPPED)) || (special_d2 && (info & CREATURE_D2_FRONT_FLIPPED)) || front_flip;
    int16_t transparent = s->aspect[CREATURE_ASPECT_COORD] & CREATURE_ASPECT_TRANSPARENT;
    Bitmap own;
    const Bitmap *bm;
    if (depth < DEPTH_D2) bm = shrunk(g, SCALE_NATIVE, NULL, &own);
    else {
        const uint8_t *pal = depth == DEPTH_D2 ? creature_d2 : creature_d3;
        distance = depth == DEPTH_D2 ? 1 : 2;
        transparent = pal[transparent];
        bm = shrunk(g, depth == DEPTH_D2 ? SCALE_D2 : SCALE_D3, pal, &own);
    }
    rel_x = S8(SHIFT_SETS + (look & ASPECT_SHIFT_MASK) + distance * SHIFT_SET_SIZE);
    rel_y = S8(SHIFT_SETS + (look >> ASPECT_SHIFT_Y & ASPECT_SHIFT_MASK) + distance * SHIFT_SET_SIZE);
    int16_t set = s->aspect[CREATURE_ASPECT_COORD] >> CREATURE_ASPECT_COORD_SHIFT;
    in_zone(bm, &vp, ZONE_SHIFTED | (ZONE_CREATURE_FIRST + set * ZONE_CREATURE_SET + coord * ZONE_CREATURE_POSITIONS + pos), flip, transparent);
    free(own.px);
}

/* The creatures of the group in view cell cell (map cell mcell). rest holds the cells still to draw,
   front tells that the cell is a front cell of the pass. Returns 1 when the group is complete. */
static int creatures(const Group *s, int16_t dir, int16_t vs, int16_t depth, int16_t cell, uint16_t mcell, uint16_t rest, int16_t pass, int front)
{
    int16_t coord = S8(VS_CREATURE + vs), ordinal, index, second = 0, pos = cell, done = 0;
    if (coord < 0) return 1;
    uint16_t creature_index = 0;
    if (!(ordinal = ordinal_in_cell(s, mcell))) {
        if (s->size != CREATURE_SIZE_HALF) return 0;
        index = -1;
    } else index = (int16_t)(creature_index = ordinal - 1);
    uint16_t relative = dir - creature_direction(s, creature_index), facing = relative & 3;
    int sideways = relative & 1;
    if (s->cells == CELLS_CENTERED) { /* one creature in the middle, drawn after the last cell */
        if (rest || pass == DOOR_PASS_BACK) return 0;
        done = 1;
        pos = s->size == CREATURE_SIZE_HALF && sideways ? VIEW_CELL_FRONT_LEFT : VIEW_CELL_CENTER;
    } else if (s->size == CREATURE_SIZE_HALF && (front || !rest || index < 0)) {
        if (!front || pass == DOOR_PASS_FRONT) {
            if (pass == DOOR_PASS_BACK || rest) return 0;
            if (!sideways) { /* facing the party or away: both creatures side by side */
                done = 1;
                if (index < 0) index = 0;
                second = group_last_creature_index(s->thing);
                uint16_t c = creature_cell(s, creature_index);
                pos = c == dir || ((dir - 1) & 3) == c ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT;
            } else {
                if (index < 0) return 0;
                pos = VIEW_CELL_CENTER;
            }
        } else if (index >= 0 && sideways) pos = VIEW_CELL_FRONT_RIGHT;
        else return 0;
    } else if (s->size != CREATURE_SIZE_QUARTER) return 0;
    if (s->size == CREATURE_SIZE_QUARTER && ((vs == VIEW_SQUARE_D0L && pos != VIEW_CELL_FRONT_RIGHT) || (vs == VIEW_SQUARE_D0R && pos != VIEW_CELL_FRONT_LEFT))) return done;
    for (uint8_t look = s->look[index];;) {
        creature(s, look, facing, pos, coord, depth);
        if (!second) break;
        second = 0;
        look = s->look[index == 0];
        pos = pos == VIEW_CELL_BACK_RIGHT ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT;
    }
    return done;
}

/* A projectile in view cell cell: an object in flight, or a projectile graphic posed and mirrored by
   its direction of flight and scaled by depth (and kinetic energy). */
static void projectile(uint16_t thing, int16_t dir, int16_t x, int16_t y, int16_t cell, int16_t lane, int16_t depth, int16_t coord)
{
    int16_t zone = ZONE_PROJECTILE_FIRST + coord * ZONE_OBJECT_CELLS + cell, aspect_index = projectile_aspect(projectile_slot(thing));
    if (aspect_index >= 0) {
        object(thing, aspect_index, cell, lane, depth, 0, zone, NULL, 0, 0);
        return;
    }
    const uint8_t *aspect = kaos_ptr(PROJECTILE_ASPECTS + PROJECTILE_ASPECT_SIZE * -aspect_index);
    int16_t kind = aspect[PROJECTILE_ASPECT_POSE_FLAGS] & PROJECTILE_KIND, by_energy = aspect[PROJECTILE_ASPECT_SCALE_FLAGS] & PROJECTILE_SCALE_WITH_ENERGY, scale = SCALE_NATIVE, row = 0, pose, flip = 0;
    uint8_t energy = projectile_kinetic_energy(thing);
    if ((by_energy && energy != KINETIC_ENERGY_MAX) || depth) {
        row = depth * 2 - (cell >> 1);
        scale = kaos_u8(PROJECTILE_SCALES + row);
        if (by_energy) scale = max16(PROJECTILE_SCALE_MIN, (int8_t)((uint16_t)(max16(PROJECTILE_ENERGY_MIN, energy + 1) * scale) >> 8));
    }
    int rotates = kind == PROJECTILE_BACK_AND_ROTATION, odd_square = rotates && ((x + y) & 1);
    if (kind == PROJECTILE_NO_BACK_NO_ROTATION) pose = POSE_FRONT;
    else {
        uint16_t flight = g_timeline_events ? projectile_event_direction(&g_timeline_events[projectile_event_index(thing)]) : (uint16_t)dir;
        if ((dir & 1) == (flight & 1)) { /* flying away from or toward the party: front or back pose */
            pose = kind < PROJECTILE_NO_BACK && (kind != PROJECTILE_BACK || flight == dir) && !odd_square ? POSE_BACK : POSE_FRONT;
            if (rotates && cell < VIEW_CELL_FRONT_RIGHT) flip = FLIP_V;
            if ((aspect[PROJECTILE_ASPECT_POSE_FLAGS] & PROJECTILE_FLIP_ON_LEFT) && lane < 1 && (lane != 0 || (cell != VIEW_CELL_BACK_RIGHT && cell != VIEW_CELL_FRONT_RIGHT))) flip |= FLIP_H;
        } else { /* flying across: side pose */
            pose = kind == PROJECTILE_NO_BACK ? POSE_BACK : POSE_SIDE; /* without a back graphic the side graphic is second */
            if (!rotates) flip = ((dir + 1) & 3) == flight;
            else {
                flip = cell == VIEW_CELL_BACK_LEFT || cell == VIEW_CELL_FRONT_LEFT;
                if (!odd_square) flip ^= FLIP_H;
                else flip |= FLIP_V;
            }
        }
    }
    int16_t distance = row >> 1 == 2 ? 1 : row >> 1 == 3 ? 2 : 0; /* D2 or D3 palette changes */
    Bitmap own;
    const Bitmap *bm = shrunk(GFX_PROJECTILE_FIRST + aspect[PROJECTILE_ASPECT_GRAPHIC] + pose, scale, scale == SCALE_NATIVE ? NULL : object_pal(distance), &own);
    in_zone(bm, &vp, zone, flip, TRANSPARENT_COLOR);
    free(own.px);
}

/* A teleporter field or fluxcage: the pattern graphic through the mask graphic at zone, with a random
   start in the pattern so that it sparkles. */
static void draw_field(const uint8_t *aspect, int16_t zone)
{
    int16_t w = aspect[FIELD_WIDTH], h = aspect[FIELD_HEIGHT], sx = 0, flip = 0;
    Box b;
    if (!layout_get_zone(zone, 0, 0, &b, &w, &h)) return;
    const Bitmap *mask = NULL, *units = gfx_bitmap(GFX_FIELD_PATTERN_FIRST + aspect[FIELD_PATTERN]);
    if (aspect[FIELD_MASK] != FIELD_NO_MASK) {
        mask = gfx_bitmap(GFX_FIELD_MASK_FIRST + (aspect[FIELD_MASK] & ~FIELD_MASK_FLIPPED));
        flip = !!(aspect[FIELD_MASK] & FIELD_MASK_FLIPPED);
        if (flip && mask && (mask->w & 15)) sx = 16 - (mask->w & 15);
    }
    int16_t first = random16() & FIELD_FIRST_UNIT_RANGE, last = aspect[FIELD_LAST_UNIT] + random_bit();
    if (units) draw_masked_fill(units, mask, flip, &vp, &b, last, first, aspect[FIELD_TRANSPARENT], sx, 0);
}

/* An explosion of aspect at scale, recolored for smoke. */
static const Bitmap *explosion_bitmap(int16_t aspect, int16_t scale, Bitmap *own)
{
    scale = bounded_value(SCALE_EXPLOSION_MIN, scale & ~1, SCALE_NATIVE);
    return shrunk(GFX_EXPLOSION_FIRST + min16(aspect, EXPLOSION_ASPECT_POISON), scale, aspect == EXPLOSION_ASPECT_SMOKE ? kaos_ptr(PAL_SMOKE) : NULL, own);
}

/* An explosion on the party square fills the whole viewport with its pattern. */
static void explosion_on_party(uint16_t thing, int16_t aspect)
{
    int smoke = aspect == EXPLOSION_ASPECT_SMOKE;
    int16_t strength = explosion_attack(thing) >> EXPLOSION_STRENGTH_SHIFT;
    const Bitmap *units = gfx_bitmap(GFX_EXPLOSION_PATTERN_FIRST + (aspect - smoke) * EXPLOSION_PATTERN_STRENGTHS + (strength != 0) + (strength > 3));
    Bitmap own = { 0 };
    if (units && smoke && (own.px = malloc((size_t)units->w * units->h))) {
        own.w = units->w;
        own.h = units->h;
        for (int i = 0; i < own.w * own.h; i++) own.px[i] = kaos_u8(PAL_SMOKE + units->px[i]) & 15;
        units = &own;
    }
    int16_t first = random16() & EXPLOSION_FILL_FIRST_UNIT_RANGE, last = random_2bits() + EXPLOSION_FILL_LAST_UNIT, w = 0, h = 0;
    Box b;
    if (units && layout_get_zone(ZONE_VIEWPORT_BOX, vp.w, vp.h, &b, &w, &h)) draw_masked_fill(units, NULL, 0, &vp, &b, last, first, EXPLOSION_FILL_TRANSPARENT, 0, 0);
    free(own.px);
}

/* The explosions of a square, each mirrored at random, then a fluxcage as a field. */
static void explosions(uint16_t thing, int16_t dir, int16_t vs, int16_t depth, int16_t coord, uint16_t mcell, int16_t pass)
{
    int16_t explosion_coord = S8(VS_EXPLOSION + vs), field_index = S8(VS_FIELD + vs);
    int fluxcage = 0;
    if (explosion_coord < 0 && field_index < 0) return;
    for (; thing != THING_END; thing = get_next_thing(thing)) {
        if (THING_TYPE(thing) != THING_EXPLOSION) continue;
        uint16_t cell = THING_CELL(thing);
        int16_t g, type = explosion_type(thing), rebirth = type >= EXPLOSION_REBIRTH_STEP_1, aspect, zone, scale;
        if (rebirth && !(coord >= 0 && cell == mcell)) continue;
        int16_t flip = random_2bits();
        Bitmap own = { 0 };
        switch (type) {
        case EXPLOSION_FIREBALL: case EXPLOSION_LIGHTNING_BOLT: case EXPLOSION_REBIRTH_STEP_2: aspect = EXPLOSION_ASPECT_FIRE; break;
        case EXPLOSION_POISON_BOLT: case EXPLOSION_POISON_CLOUD: aspect = EXPLOSION_ASPECT_POISON; break;
        case EXPLOSION_SMOKE: aspect = EXPLOSION_ASPECT_SMOKE; break;
        case EXPLOSION_REBIRTH_STEP_1: /* the back pose of the lightning projectile */
            scale = kaos_u8(REBIRTH_SCALES + REBIRTH_STEP_1_SCALES + coord);
            g = GFX_PROJECTILE_FIRST + kaos_u8(PROJECTILE_ASPECTS + PROJECTILE_ASPECT_SIZE * PROJECTILE_ASPECT_LIGHTNING + PROJECTILE_ASPECT_GRAPHIC) + POSE_BACK;
            in_zone(shrunk(g, scale, NULL, &own), &vp, ZONE_REBIRTH_STEP1_FIRST + coord, flip, TRANSPARENT_COLOR);
            free(own.px);
            continue;
        case EXPLOSION_FLUXCAGE:
            fluxcage |= depth < DEPTH_D4;
            continue;
        default: aspect = EXPLOSION_ASPECT_SPELL;
        }
        if (explosion_coord < 0) continue;
        if (vs == VIEW_SQUARE_D0C) {
            explosion_on_party(thing, aspect);
            continue;
        }
        if (rebirth) {
            zone = ZONE_REBIRTH_STEP2_FIRST + coord;
            scale = kaos_u8(REBIRTH_SCALES + coord);
        } else {
            int far_side = !(cell == dir || ((dir - 1) & 3) == cell);
            zone = explosion_is_centered(thing) ? ZONE_EXPLOSION_CENTERED_FIRST + explosion_coord : ZONE_EXPLOSION_FIRST + explosion_coord * ZONE_EXPLOSION_SIDES + far_side;
            scale = (int16_t)(max16(EXPLOSION_ATTACK_MIN, explosion_attack(thing) + 1) * kaos_u8(EXPLOSION_SCALES + depth)) >> 8;
        }
        in_zone(explosion_bitmap(aspect, scale, &own), &vp, zone, flip, TRANSPARENT_COLOR);
        free(own.px);
    }
    if (fluxcage && field_index >= 0 && pass != DOOR_PASS_BACK && !g_do_not_draw_fluxcages) {
        uint8_t aspect[FIELD_ASPECT_SIZE];
        memcpy(aspect, kaos_ptr(FIELD_ASPECTS + FIELD_ASPECT_SIZE * field_index), FIELD_ASPECT_SIZE);
        aspect[FIELD_PATTERN]++; /* the fluxcage pattern follows the teleporter pattern */
        draw_field(aspect, ZONE_FIELD_FIRST + field_index);
    }
}

/* The things of one view square: per cell in cell order (nibbles of cell + 1, low first; 0 = the
   alcove; bit 3 = door pass, bit 0 the front pass) the objects, creatures, and projectiles, then the
   explosions of the square. */
static void things(uint16_t first, int16_t dir, int16_t x, int16_t y, int16_t vs, uint16_t order)
{
    if (first == THING_END) return;
    int16_t pass = 0;
    if (order & CELL_ORDER_DOOR_PASS) {
        pass = (order & 1) ? DOOR_PASS_FRONT : DOOR_PASS_BACK;
        order >>= 4;
    }
    uint16_t rest = order, mcell = 0, group_thing = THING_NONE;
    int alcove = rest == 0, has_projectiles = 0, has_explosions = 0, group_done = 0, loaded = 0;
    int16_t lane = S8(VS_LANE + vs), depth = S8(VS_DEPTH + vs), coord = S8((alcove ? VS_ALCOVE : VS_OBJECT) + vs), count = 0;
    Group s;
    do {
        int16_t cell, pile;
        if (!alcove) {
            cell = (rest & 15) - 1;
            rest >>= 4;
            count++;
            mcell = (cell + dir) & 3;
            pile = 0;
        } else {
            cell = VIEW_CELL_CENTER;
            mcell = (dir + 2) & 3; /* the alcove side of the wall faces the party */
            pile = PILE_ALCOVE_FIRST;
        }
        pile += (mcell & 1) * PILE_ODD_CELL; /* odd map cells start in the second half of the shifts */
        for (uint16_t t = first; t != THING_END; t = get_next_thing(t)) {
            int16_t type = THING_TYPE(t);
            if (type == THING_GROUP) group_thing = t;
            else if (type == THING_PROJECTILE) has_projectiles = 1;
            else if (type == THING_EXPLOSION) has_explosions = 1;
            else if (type >= THING_WEAPON && type <= THING_JUNK && coord >= 0 && THING_CELL(t) == mcell && (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) && (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT))
                object(t, object_aspect(t), cell, lane, depth, alcove && lane == 0, 0, &pile, coord, alcove);
        }
        if (alcove || depth >= DEPTH_D4) break;
        int front = (cell < VIEW_CELL_FRONT_RIGHT || count == 1) && (rest == 0 || (rest & 15) - 1 >= VIEW_CELL_FRONT_RIGHT);
        if (group_thing != THING_NONE && !group_done) {
            if (!loaded) group(group_thing, &s), loaded = 1;
            group_done = creatures(&s, dir, vs, depth, cell, mcell, rest, pass, front);
        }
        int16_t projectile_coord = S8(VS_OBJECT + vs);
        if (projectile_coord < 0) has_projectiles = 0;
        else if (has_projectiles && (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) && (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT))
            for (uint16_t t = first; t != THING_END; t = get_next_thing(t))
                if (THING_TYPE(t) == THING_PROJECTILE && THING_CELL(t) == mcell) projectile(t, dir, x, y, cell, lane, depth, projectile_coord);
    } while (rest);
    if (has_explosions) explosions(first, dir, vs, depth, coord, mcell, pass);
}

/* The field of a teleporter square. */
static void field(int16_t view_square, int16_t zone)
{
    draw_field(kaos_ptr(FIELD_ASPECTS + FIELD_ASPECT_SIZE * S8(VS_FIELD + view_square)), zone);
}

/* The shrink of get_scaled_flipped_bitmap: scale/32 per axis. out->px is NULL on failure. */
static void scaled(int16_t g, int16_t scale_x, int16_t scale_y, uint32_t palette_changes, Bitmap *out)
{
    const Bitmap *bm = gfx_bitmap(g);
    *out = (Bitmap){ 0 };
    if (bm) draw_shrink(bm, (int16_t)((scale_x >> 1) + bm->w * scale_x) >> 5, (int16_t)((scale_y >> 1) + bm->h * scale_y) >> 5, kaos_ptr(palette_changes), out);
}

/* The wall text of draw_wall_ornament at D1C: glyphs of graphic 0x102 in zones 1000 + line. */
static void wall_text(const uint8_t *s)
{
    const Bitmap *font = gfx_bitmap(0x102);
    int16_t gw = kaos_s16(GLYPH_SIZE), gh = kaos_s16(GLYPH_SIZE + 2);
    wall(-0x33, 0x2df, NORMAL);
    for (int16_t line = 0; font; line++) {
        int16_t n = 0;
        while (s[n] < 0x80) n++;
        Box b;
        int16_t w = gw * n, h = gh;
        if (n && layout_get_zone(1000 + line, 0, 0, &b, &w, &h))
            for (b.w = gw; n; n--, s++, b.x += gw) draw_blit(font, *s * gw, 0, &vp, &b, 10, 0);
        s += n;
        if (*s++ == 0x81) break;
    }
}

/* draw_wall_ornament: view walls 0-14, see docs/symbols/range-7-9.md. Returns 1 for an alcove. */
static int wall_ornament(uint16_t ordinal, int16_t view, const Aspect *a)
{
    if (!ordinal) return 0;
    int16_t i = ordinal - 1, g = wall_orn[i][0], flip = 0, zone = view + wall_orn[i][1] * 15 + 1004;
    int alcove = is_alcove(i), text = i == inscription;
    uint8_t s[80] = { 0x81 };
    Bitmap own = { 0 };
    const Bitmap *bm;
    if (text) decode_text((char *)s, sizeof s, a->text, 0);
    if (view < 12) {
        if (view == 8 || view == 3 || view == 1) flip = 1;
        else if (view > 3 && view != 7) g++;
        if (view < 2) scaled(g, 30, 14, PAL_D3, &own);
        else if (view < 7) scaled(g, 14, 14, PAL_D3, &own);
        else scaled(g, 21, 21, PAL_D2, &own);
        bm = &own;
        if (!own.px) return alcove;
    } else {
        if (view == 14) {
            if (text) {
                wall_text(s);
                return alcove;
            }
            g++;
            g_facing_alcove = alcove;
            g_facing_vi_altar = i == vi_altar;
            g_facing_fountain = i == fountain;
        }
        flip = view == 13;
        bm = gfx_bitmap(g);
    }
    if (text) {
        int16_t lines = 0;
        for (const uint8_t *p = s; *p != 0x81; p++) lines += *p == 0x80;
        if (++lines < 4 && bm) {
            zone |= 0x4000;
            rel_x = bm->w;
            rel_y = kaos_u8(TEXT_HEIGHTS + lines + kaos_u8(WALL_ORN_DEPTH + view) * 3);
        }
    }
    if (in_zone(bm, &vp, zone, flip, 10) && view == 14) g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn_box;
    if (view == 14 && a->portrait) {
        const Bitmap *p = gfx_bitmap(0x1a);
        int16_t pw = kaos_s16(PORTRAIT_SIZE), ph = kaos_s16(PORTRAIT_SIZE + 2), n = a->portrait - 1, w = pw, h = ph;
        Box b;
        if (p && layout_get_zone(0x2e1, 0, 0, &b, &w, &h)) draw_blit(p, (n & 7) * pw, (n >> 3) * ph, &vp, &b, 1, 0);
    }
    free(own.px);
    return alcove;
}

/* draw_floor_ornament: view floors 0-10. Bit 15 of the ordinal adds the footprints. */
static void floor_ornament(uint16_t ordinal, int16_t view)
{
    if (!ordinal) return;
    int16_t i = (ordinal & 0x7fff) - 1;
    if (i >= 0) {
        int flip = view == 10 || view == 7 || view == 4 || view == 1 || (i == 15 && parity && (view == 9 || view == 6 || view == 3));
        in_zone(gfx_bitmap(floor_orn[i][0] + kaos_u8(FLOOR_ORN_VIEW + view)), &vp, view + floor_orn[i][1] * 11 + 1500, flip, 10);
    }
    if (ordinal & 0x8000) floor_ornament(16, view);
}

/* draw_door_ornament into the door bitmap. depth 0 D3, 1 D2, 2 D1. */
static void door_ornament(uint16_t ordinal, int16_t depth, Bitmap *door)
{
    if (!ordinal) return;
    int16_t i = ordinal - 1;
    Bitmap own = { 0 };
    if (depth == 2) in_zone(gfx_bitmap(door_orn[i][0]), door, depth + door_orn[i][1] * 3 + 2000, 0, 9);
    else {
        scaled(door_orn[i][0], depth ? 21 : 14, depth ? 21 : 14, depth ? PAL_DOOR_D2 : PAL_DOOR_D3, &own);
        if (own.px) in_zone(&own, door, depth + door_orn[i][1] * 3 + 2000, 0, 9);
    }
    free(own.px);
}

/* draw_door_button: view 0 D3R, 1 D3C, 2 D2C, 3 D1C. */
static void door_button(int16_t view)
{
    Bitmap own = { 0 };
    const Bitmap *bm = gfx_bitmap(0x1c5);
    if (view < 3) {
        scaled(0x1c5, view == 2 ? 21 : 14, view == 2 ? 21 : 14, view == 2 ? PAL_D2 : PAL_D3, &own);
        bm = &own;
    }
    if (bm && bm->px && in_zone(bm, &vp, view + kaos_u8(DOOR_BUTTON_COORDS) + 1950, 0, 10) && view == 3)
        g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn_box;
    free(own.px);
}

static int has_button(uint16_t door) { return get_thing_data(door)[3] & 0x40; }

/* draw_door: a copy of the door graphic with its ornament, placed by the door state. */
static void door(uint16_t index, int16_t state, int16_t depth, int16_t zone)
{
    if (!state) return;
    const uint8_t *d = get_thing_data(index);
    int set = d[3] & 1;
    const Bitmap *g = gfx_bitmap(door_gfx[depth][set]);
    if (!g) return;
    Bitmap t = { g->w, g->h, malloc((size_t)g->w * g->h) };
    if (!t.px) return;
    memcpy(t.px, g->px, (size_t)g->w * g->h);
    door_ornament((d[3] & 0x1e) >> 1, depth, &t);
    if (door_attr[set] & 0x400) draw_flip(&t, random_2bits() & 3); /* animated door */
    if (state == 5) door_ornament(16, depth, &t);
    else if (state != 4) {
        zone += state;
        if (!(d[3] & 0x20)) { /* two halves that slide apart */
            int16_t w = rel_x = t.w >> 1, h = rel_y = 0;
            Box b;
            if (layout_get_zone(zone + 6, t.w, t.h, &b, &w, &h)) draw_blit(&t, rel_x + w, h, &vp, &b, 10, 0);
            zone += 0x4003;
        }
    }
    in_zone(&t, &vp, zone, 0, 10);
    free(t.px);
}

/* get_location_after_level_change: the map and square one level up or down. */
static int16_t level_change(int16_t m, int16_t delta, int16_t *x, int16_t *y)
{
    const uint8_t *from = g_dungeon_maps + 16 * m;
    int16_t ax = *x + from[6], ay = *y + from[7];
    for (int16_t i = 0; i < dungeon_map_count(); i++) {
        const uint8_t *to = g_dungeon_maps + 16 * i;
        if ((uint16_t)(delta + (from[9] & 0x3f)) == (to[9] & 0x3f) && to[6] <= ax && ax <= to[6] + (BE16(to + 8) >> 6 & 0x1f)
            && to[7] <= ay && ay <= to[7] + (to[8] >> 3)) {
            *x = ax - to[6];
            *y = ay - to[7];
            return i;
        }
    }
    return -1;
}

/* draw_ceiling_pit: the hole of an open pit on the map above. */
static void ceiling_pit(int16_t g, int16_t zone, int16_t x, int16_t y, int flip)
{
    int16_t m = level_change(map, -1, &x, &y);
    if (m >= 0 && dungeon_square(m, x, y) >> 5 == PIT && dungeon_square(m, x, y) & 8) wall(g, zone, flip ? FLIPPED : NORMAL);
}

#define ASPECT Aspect a; aspect(&a, dir, x, y)
#define STAIR(i) (stairs + (i))

static void d3l2(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x3421;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a3c), 0x2be, NORMAL); wall_ornament(a.v[RT], 0, &a); return;
    case PIT: if (!a.v[ST]) wall(0x31, 0x352, NORMAL); break;
    case DOOR_SIDE: case STAIRS_SIDE: cells = 0x321; break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 0);
        things(a.v[THING], dir, x, y, 14, 0x218);
        door(a.v[RT], a.v[ST], 0, 0xe74);
        things(a.v[THING], dir, x, y, 14, 0x349);
        return;
    case STAIRS_FRONT: wall(a.v[ST] ? -0x21 : -0x23, a.v[ST] ? 800 : 0x32d, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 0);
    things(a.v[THING], dir, x, y, 14, cells);
    if (a.v[EL] == TELEPORTER) field(14, 0x2be);
}

static void d3r2(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x4312;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a3a), 0x2bf, NORMAL); wall_ornament(a.v[LT], 1, &a); return;
    case PIT: if (!a.v[ST]) wall(0x31, 0x353, FLIPPED); break;
    case DOOR_SIDE: case STAIRS_SIDE: cells = 0x412; break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 1);
        things(a.v[THING], dir, x, y, 15, 0x128);
        door(a.v[RT], a.v[ST], 0, 0xe7e);
        things(a.v[THING], dir, x, y, 15, 0x439);
        return;
    case STAIRS_FRONT: wall(a.v[ST] ? -0x22 : -0x24, a.v[ST] ? 0x321 : 0x32e, FLIPPED); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 1);
    things(a.v[THING], dir, x, y, 15, cells);
    if (a.v[EL] == TELEPORTER) field(15, 0x2bf);
}

static void d3l(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x3421;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a40), 0x2c1, NORMAL);
        wall_ornament(a.v[RT], 2, &a);
        if (wall_ornament(a.v[FR], 4, &a)) things(a.v[THING], dir, x, y, 12, 0);
        return;
    case PIT: if (!a.v[ST]) wall(0x32, 0x354, NORMAL); break;
    case DOOR_SIDE: case STAIRS_SIDE: cells = 0x321; break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 2);
        things(a.v[THING], dir, x, y, 12, 0x218);
        wall(LID(0x102a5a), 0x2ce, NORMAL);
        wall(LID(0x102a5c), 0x2cf, FLIPPED);
        door(a.v[RT], a.v[ST], 0, 0xe88);
        things(a.v[THING], dir, x, y, 12, 0x349);
        return;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 0 : 7), a.v[ST] ? 0x322 : 0x32f, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 2);
    things(a.v[THING], dir, x, y, 12, cells);
    if (a.v[EL] == TELEPORTER) field(12, 0x2c1);
}

static void d3r(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x4312;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a3e), 0x2c2, NORMAL);
        wall_ornament(a.v[LT], 3, &a);
        if (wall_ornament(a.v[FR], 6, &a)) things(a.v[THING], dir, x, y, 13, 0);
        return;
    case PIT: if (!a.v[ST]) wall(0x32, 0x356, FLIPPED); break;
    case DOOR_SIDE: case STAIRS_SIDE: cells = 0x412; break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 4);
        things(a.v[THING], dir, x, y, 13, 0x128);
        wall(LID(0x102a5e), 0x2d0, NORMAL);
        wall(LID(0x102a5a), 0x2d1, FLIPPED);
        if (has_button(a.v[RT])) door_button(0);
        door(a.v[RT], a.v[ST], 0, 0xe9c);
        things(a.v[THING], dir, x, y, 13, 0x439);
        return;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 0 : 7), a.v[ST] ? 0x324 : 0x331, FLIPPED); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 4);
    things(a.v[THING], dir, x, y, 13, cells);
    if (a.v[EL] == TELEPORTER) field(13, 0x2c2);
}

static void d3c(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a42), 0x2c0, OPAQUE);
        if (wall_ornament(a.v[FR], 5, &a)) things(a.v[THING], dir, x, y, 11, 0);
        return;
    case PIT: if (!a.v[ST]) wall(0x33, 0x355, NORMAL); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 3);
        things(a.v[THING], dir, x, y, 11, 0x218);
        wall(LID(0x102a58), 0x2d2, NORMAL);
        wall(LID(0x102a58), 0x2d3, FLIPPED);
        if (has_button(a.v[RT])) door_button(1);
        door(a.v[RT], a.v[ST], 0, 0xe92);
        things(a.v[THING], dir, x, y, 11, 0x349);
        return;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 1 : 8), a.v[ST] ? 0x323 : 0x330, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 3);
    things(a.v[THING], dir, x, y, 11, 0x3421);
    if (a.v[EL] == TELEPORTER) field(11, 0x2c0);
}

static void d2l2(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    if (a.v[EL] == WALL) wall(LID(0x102a32), 0x2c3, NORMAL);
    else if (a.v[EL] == TELEPORTER) field(9, 0x2c3);
}

static void d2r2(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    if (a.v[EL] == WALL) wall(LID(0x102a30), 0x2c4, NORMAL);
    else if (a.v[EL] == TELEPORTER) field(10, 0x2c4);
}

static void d2l(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x3421;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a36), 0x2c6, NORMAL);
        wall_ornament(a.v[RT], 7, &a);
        if (!wall_ornament(a.v[FR], 9, &a)) return;
        ceiling_pit(0x40, 0x360, x, y, 0);
        things(a.v[THING], dir, x, y, 7, 0);
        return;
    case PIT: wall(a.v[ST] ? 0x3a : 0x34, 0x357, NORMAL); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 5);
        things(a.v[THING], dir, x, y, 7, 0x218);
        wall(LID(0x102a4c), 0x2d9, NORMAL);
        door(a.v[RT], a.v[ST], 1, 0xea6);
        ceiling_pit(0x40, 0x360, x, y, 0);
        things(a.v[THING], dir, x, y, 7, 0x349);
        return;
    case STAIRS_SIDE: wall(STAIR(14), 0x33a, NORMAL); /* fall through */
    case DOOR_SIDE: cells = 0x342; break;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 2 : 9), a.v[ST] ? 0x325 : 0x332, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 5);
    ceiling_pit(0x40, 0x360, x, y, 0);
    things(a.v[THING], dir, x, y, 7, cells);
    if (a.v[EL] == TELEPORTER) field(7, 0x2c6);
}

static void d2r(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    uint16_t cells = 0x4312;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a34), 0x2c7, NORMAL);
        wall_ornament(a.v[LT], 8, &a);
        if (wall_ornament(a.v[FR], 11, &a)) things(a.v[THING], dir, x, y, 8, 0);
        return;
    case PIT: wall(a.v[ST] ? 0x3a : 0x34, 0x359, FLIPPED); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 7);
        things(a.v[THING], dir, x, y, 8, 0x128);
        wall(LID(0x102a4a), 0x2db, NORMAL);
        door(a.v[RT], a.v[ST], 1, 0xeba);
        things(a.v[THING], dir, x, y, 8, 0x439);
        return;
    case STAIRS_SIDE: wall(STAIR(14), 0x33b, FLIPPED); /* fall through */
    case DOOR_SIDE: cells = 0x431; break;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 2 : 9), a.v[ST] ? 0x327 : 0x334, FLIPPED); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 7);
    ceiling_pit(0x40, 0x362, x, y, 1);
    things(a.v[THING], dir, x, y, 8, cells);
    if (a.v[EL] == TELEPORTER) field(8, 0x2c7);
}

static void d2c(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL:
        wall(LID(0x102a38), 0x2c5, OPAQUE);
        if (wall_ornament(a.v[FR], 10, &a)) things(a.v[THING], dir, x, y, 6, 0);
        return;
    case PIT: wall(a.v[ST] ? 0x3b : 0x35, 0x358, NORMAL); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 6);
        things(a.v[THING], dir, x, y, 6, 0x218);
        wall(LID(0x102a4e), 0x2da, NORMAL);
        wall(LID(0x102a56), 0x2d4, NORMAL);
        wall(LID(0x102a56), 0x2d5, FLIPPED);
        if (has_button(a.v[RT])) door_button(2);
        door(a.v[RT], a.v[ST], 1, 0xeb0);
        things(a.v[THING], dir, x, y, 6, 0x349);
        return;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 3 : 10), a.v[ST] ? 0x326 : 0x333, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 6);
    ceiling_pit(0x41, 0x361, x, y, 0);
    things(a.v[THING], dir, x, y, 6, 0x3421);
    if (a.v[EL] == TELEPORTER) field(6, 0x2c5);
}

static void d1l(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a2c), 0x2c9, NORMAL); wall_ornament(a.v[RT], 12, &a); return;
    case PIT: wall(a.v[ST] ? 0x3c : 0x36, 0x35a, NORMAL); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 8);
        things(a.v[THING], dir, x, y, 4, 0x28);
        wall(LID(0x102a46), 0x2dc, NORMAL);
        door(a.v[RT], a.v[ST], 2, 0xec4);
        things(a.v[THING], dir, x, y, 4, 0x39);
        return;
    case STAIRS_SIDE: wall(STAIR(a.v[ST] ? 15 : 16), a.v[ST] ? 0x33c : 0x33e, NORMAL); break;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 4 : 11), a.v[ST] ? 0x328 : 0x335, NORMAL); break;
    case CORRIDOR: case TELEPORTER: case DOOR_SIDE: break;
    default: return;
    }
    floor_ornament(a.v[FR], 8);
    ceiling_pit(0x42, 0x363, x, y, 0);
    things(a.v[THING], dir, x, y, 4, 0x32);
    if (a.v[EL] == TELEPORTER) field(4, 0x2c9);
}

static void d1r(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a2a), 0x2ca, NORMAL); wall_ornament(a.v[LT], 13, &a); return;
    case PIT: wall(a.v[ST] ? 0x3c : 0x36, 0x35c, FLIPPED); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 10);
        things(a.v[THING], dir, x, y, 5, 0x18);
        wall(LID(0x102a44), 0x2de, NORMAL);
        door(a.v[RT], a.v[ST], 2, 0xed8);
        things(a.v[THING], dir, x, y, 5, 0x49);
        return;
    case STAIRS_SIDE: wall(STAIR(a.v[ST] ? 15 : 16), a.v[ST] ? 0x33d : 0x33f, FLIPPED); break;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 4 : 11), a.v[ST] ? 0x32a : 0x337, FLIPPED); break;
    case CORRIDOR: case TELEPORTER: case DOOR_SIDE: break;
    default: return;
    }
    floor_ornament(a.v[FR], 10);
    ceiling_pit(0x42, 0x365, x, y, 1);
    things(a.v[THING], dir, x, y, 5, 0x41);
    if (a.v[EL] == TELEPORTER) field(5, 0x2ca);
}

static void d1c(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    g_square_ahead_element = (int16_t)a.v[EL];
    switch (a.v[EL]) {
    case WALL: /* ponytail: no thieves eye hole yet, needs the party spell state (step 9) */
        g_facing_alcove = g_facing_vi_altar = g_facing_fountain = 0;
        wall(LID(0x102a2e), 0x2c8, OPAQUE);
        if (wall_ornament(a.v[FR], 14, &a)) things(a.v[THING], dir, x, y, 3, 0);
        return;
    case PIT: wall(a.v[ST] ? 0x3d : 0x37, 0x35b, NORMAL); break;
    case DOOR_FRONT:
        floor_ornament(a.v[FR], 9);
        things(a.v[THING], dir, x, y, 3, 0x218);
        wall(LID(0x102a48), 0x2dd, NORMAL);
        wall(LID(0x102a54), 0x2d6, NORMAL);
        wall(LID(0x102a52), 0x2d7, NORMAL);
        if (has_button(a.v[RT])) door_button(3);
        door(a.v[RT], a.v[ST], 2, 0xece);
        things(a.v[THING], dir, x, y, 3, 0x349);
        return;
    case STAIRS_FRONT: wall(STAIR(a.v[ST] ? 5 : 12), a.v[ST] ? 0x329 : 0x336, NORMAL); break;
    case CORRIDOR: case TELEPORTER: break;
    default: return;
    }
    floor_ornament(a.v[FR], 9);
    ceiling_pit(0x43, 0x364, x, y, 0);
    things(a.v[THING], dir, x, y, 3, 0x3421);
    if (a.v[EL] == TELEPORTER) field(3, 0x2c8);
}

static void d0l(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a28), 0x2cc, NORMAL); return;
    case STAIRS_SIDE: wall(STAIR(17), 0x340, NORMAL); return;
    case PIT: wall(a.v[ST] ? 0x3e : 0x38, 0x35d, NORMAL); /* fall through */
    case CORRIDOR: case TELEPORTER: case DOOR_SIDE:
        ceiling_pit(0x44, 0x366, x, y, 0);
        things(a.v[THING], dir, x, y, 1, 2);
    }
    if (a.v[EL] == TELEPORTER) field(1, 0x2cc);
}

static void d0r(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    switch (a.v[EL]) {
    case WALL: wall(LID(0x102a26), 0x2cd, NORMAL); return;
    case STAIRS_SIDE: wall(STAIR(17), 0x341, FLIPPED); return;
    case PIT: wall(a.v[ST] ? 0x3e : 0x38, 0x35f, FLIPPED); /* fall through */
    case CORRIDOR: case TELEPORTER: case DOOR_SIDE:
        ceiling_pit(0x44, 0x368, x, y, 1);
        things(a.v[THING], dir, x, y, 2, 1);
    }
    if (a.v[EL] == TELEPORTER) field(2, 0x2cd);
}

static void d0c(int16_t dir, int16_t x, int16_t y)
{
    ASPECT;
    if (a.v[EL] == DOOR_SIDE) wall(LID(0x102a50), 0x2d8, NORMAL);
    else if (a.v[EL] == PIT) wall(a.v[ST] ? 0x3f : 0x39, 0x35e, NORMAL);
    else if (a.v[EL] == STAIRS_FRONT) {
        wall(STAIR(a.v[ST] ? 6 : 13), a.v[ST] ? 0x32b : 0x338, NORMAL);
        wall(STAIR(a.v[ST] ? 6 : 13), a.v[ST] ? 0x32c : 0x339, FLIPPED);
    }
    ceiling_pit(0x45, 0x367, x, y, 0);
    things(a.v[THING], dir, x, y, 0, 0x21);
    if (a.v[EL] == TELEPORTER) field(0, 0x2cb);
}

/* apply_creature_replacement_colors: color 9 or 10 of the 6 light palettes and of the creature
   palette changes from replacement set n. */
enum { COLOR_CREATURE_1 = 9, COLOR_CREATURE_2 = 10, REPLACEMENT_DEFAULT_1 = 8, REPLACEMENT_DEFAULT_2 = 12 };

static void replacement_colors(int16_t color, int16_t n)
{
    uint32_t set = REPLACEMENT_COLORS + REPLACEMENT_SET_SIZE * n; /* a color per light level, then the D2 and D3 color */
    for (int l = 0; l < LIGHT_LEVELS; l++) pals[l][color] = kaos_u16(set + 2 * l);
    creature_d2[color] = kaos_u8(set + 2 * LIGHT_LEVELS);
    creature_d3[color] = kaos_u8(set + 2 * LIGHT_LEVELS + 1);
}

/* set_current_map_and_party_map and load_current_map_graphics: the ornaments, wall set, floor set,
   stairs, and doors of the map. Graphics decode on first use. */
void view_load_map(int16_t m)
{
    map = m;
    mw = dungeon_map_width(m);
    mh = dungeon_map_height(m);
    desc = g_dungeon_maps + 16 * m;
    int16_t walls = desc[0xb] & 15, floors = desc[10] & 15, doors = desc[0xd] & 15;
    const uint8_t *creature_types = g_dungeon_map_data[m][mw - 1] + mh; /* map metadata: creature types, then ornaments */
    int16_t creature_type_count = desc[MAP_COUNTS] >> 4;
    const uint8_t *meta = creature_types + creature_type_count;
    int16_t floor_set = desc[0xf] & 15, wall_set = desc[0xf] >> 4, base = 0x28 * wall_set;
    for (int i = 0; i < 46; i++) ids[i] = kaos_s16(WALL_SET_IDS + 2 * i);
#define SLOT(a, g, f) (slot_gfx[-1 - LID(a)] = (g), slot_flip[-1 - LID(a)] = (f))
    SLOT(0x102a22, 0x4e + 2 * floor_set, 0);
    SLOT(0x102a24, 0x4f + 2 * floor_set, 0);
    for (int i = 0; i < 15; i++) SLOT(WALL_SET_IDS + 4 + 2 * i, 0x5d + base + i, 0);
    static const uint32_t frames[7] = { 0x102a50, 0x102a54, 0x102a56, 0x102a58, 0x102a5a, 0x102a48, 0x102a4e };
    for (int i = 0; i < 7; i++) SLOT(frames[i], 0x56 + base + i, 0);
    SLOT(0x102a52, 0x57 + base, 1);
    SLOT(0x102a4a, 0x5c + base, 0); /* layout_share_bitmap */
    SLOT(0x102a4c, 0x5c + base, 0);
    SLOT(0x102a44, 0x5b + base, 0);
    SLOT(0x102a46, 0x5b + base, 0);
    SLOT(0x102a5c, 0x5a + base, 0);
    SLOT(0x102a5e, 0x5a + base, 0);
    slot_gfx[0x32] = 0x5d + base + 4; /* -0x33 shares the D1C wall */
    /* copy_wall_bitmap_and_flip_horizontal pairs: the mirrored views swap left and right */
    static const uint32_t pairs[15][2] = {
        { 0x102a42, 0x102a7c }, { 0x102a38, 0x102a72 }, { 0x102a2e, 0x102a68 }, { 0x102a28, 0x102a60 }, { 0x102a26, 0x102a62 },
        { 0x102a3e, 0x102a7a }, { 0x102a40, 0x102a78 }, { 0x102a34, 0x102a70 }, { 0x102a36, 0x102a6e }, { 0x102a2a, 0x102a66 },
        { 0x102a2c, 0x102a64 }, { 0x102a3a, 0x102a76 }, { 0x102a3c, 0x102a74 }, { 0x102a30, 0x102a6c }, { 0x102a32, 0x102a6a },
    };
    for (int i = 0; i < 15; i++) SLOT(pairs[i][1], slot_gfx[-1 - LID(pairs[i][0])], 1);
#undef SLOT
    stairs = 0x6c + base;
    slot_gfx[32] = slot_gfx[33] = stairs; /* set_layout_graphic(-0x21/-0x22) */
    slot_gfx[34] = slot_gfx[35] = stairs + 7;
    for (int s = 0; s < 2; s++) {
        int16_t type = desc[0xe] >> 4 * s & 15;
        door_attr[s] = kaos_u16(DOOR_INFO + 2 * type);
        for (int d = 0; d < 3; d++) door_gfx[d][s] = 0xf6 + 3 * type + d;
    }
    int16_t n_alcove = 0;
    alcoves[0] = alcoves[1] = alcoves[2] = -1;
    vi_altar = fountain = -1;
    inscription = walls;
    for (int16_t i = 0; i <= walls; i++) {
        uint8_t o = i < walls ? meta[i] : 0;
        wall_orn[i][0] = 0x103 + 2 * o;
        wall_orn[i][1] = kaos_u8(WALL_ORN_COORDS + o);
        for (int k = 0; k < 3; k++)
            if (kaos_u8(ALCOVES + k) == o) {
                alcoves[n_alcove++] = i;
                if (o == ORNAMENT_VI_ALTAR) vi_altar = i;
            }
        if (kaos_u8(FOUNTAINS) == o) fountain = i;
    }
    for (int16_t i = 0; i < floors; i++) {
        uint8_t o = meta[walls + i];
        floor_orn[i][0] = 0x181 + 6 * o;
        floor_orn[i][1] = kaos_u8(FLOOR_ORN_COORDS + o);
    }
    floor_orn[15][0] = 0x17b; /* footprints */
    floor_orn[15][1] = 0;
    for (int16_t i = 0; i < doors; i++) {
        uint8_t o = meta[walls + floors + i];
        door_orn[i][0] = 0x1b9 + o;
        door_orn[i][1] = kaos_u8(DOOR_ORN_COORDS + o);
    }
    for (int i = 15; i < 17; i++) { /* destroyed door mask, thieves eye */
        door_orn[i][0] = 0x1a8 + i;
        door_orn[i][1] = 1;
    }
    if (!tables_creature_info(0)) tables_init();
    for (int l = 0; l < LIGHT_LEVELS; l++)
        for (int i = 0; i < 16; i++) pals[l][i] = kaos_u16(DUNGEON_PALETTE + sizeof pals[l] * l + 2 * i);
    memcpy(creature_d3, kaos_ptr(PAL_CREATURE_D3), 16);
    memcpy(creature_d2, kaos_ptr(PAL_CREATURE_D2), 16);
    replacement_colors(COLOR_CREATURE_1, REPLACEMENT_DEFAULT_1);
    replacement_colors(COLOR_CREATURE_2, REPLACEMENT_DEFAULT_2);
    for (int i = 0; i < creature_type_count; i++) { /* the original indexes the aspects by creature type */
        uint8_t sets = kaos_u8(CREATURE_ASPECTS + CREATURE_ASPECT_SIZE * creature_types[i] + CREATURE_ASPECT_REPLACEMENT);
        if (sets & 15) replacement_colors(COLOR_CREATURE_1, (sets & 15) - 1);
        if (sets >> 4) replacement_colors(COLOR_CREATURE_2, (sets >> 4) - 1);
    }
}

void view_set_light(int16_t amount) { light = amount; }

/* draw_dungeon_view: ceiling, floor, then the 16 squares back to front, then draw_viewport. */
void view_draw(int16_t dir, int16_t x, int16_t y)
{
    static void (*const squares[16])(int16_t, int16_t, int16_t) = { d3l2, d3r2, d3l, d3r, d3c, d2l2, d2r2, d2l, d2r, d2c, d1l, d1r, d1c, d0l, d0r, d0c };
    static const int8_t forward[16] = { 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 0, 0, 0 }, right[16] = { -2, 2, -1, 1, 0, -2, 2, -1, 1, 0, -1, 1, 0, -1, 1, 0 };
    static uint8_t px[136 * 224];
    vp = (Bitmap){ (uint16_t)kaos_s16(VIEWPORT_SIZE), (uint16_t)kaos_s16(VIEWPORT_SIZE + 2), px };
    if (map < 0 || (size_t)vp.w * vp.h > sizeof px) return;
    memset(px, 0, sizeof px);
    for (int i = 0; i < VIEW_CLICK_BOX_COUNT; i++) g_dungeon_view_clickable_boxes[i].x = -1;
    for (int i = 0; i < 46; i++) ids[i] = kaos_s16(WALL_SET_IDS + 2 * i);
    parity = (dir + x + y) & 1;
    if (parity) { /* the mirrored-walls memory option is on, as on a 1 MB Amiga */
        memcpy(&LID(WALL_SET_IDS + 4), &LID(WALL_SET_FLIPPED), 30);
        wall(LID(0x102a24), 700, OPAQUE);
        wall(LID(0x102a22), 701, FLIPPED);
    } else {
        wall(LID(0x102a24), 700, FLIPPED);
        wall(LID(0x102a22), 701, OPAQUE);
    }
    for (int i = 0; i < 3; i++) { /* objects only on D4L, D4R, D4C */
        int16_t sx = x, sy = y, r = i == 0 ? -1 : i == 1;
        sx += 4 * kaos_s16(STEP_EAST + 2 * dir) + r * kaos_s16(STEP_EAST + 2 * ((dir + 1) & 3));
        sy += 4 * kaos_s16(STEP_NORTH + 2 * dir) + r * kaos_s16(STEP_NORTH + 2 * ((dir + 1) & 3));
        things(dungeon_square_first_thing(map, sx, sy), dir, sx, sy, (int16_t[]){ VIEW_SQUARE_D4L, VIEW_SQUARE_D4R, VIEW_SQUARE_D4C }[i], 1);
    }
    for (int i = 0; i < 16; i++) { /* update_map_coordinates_after_relative_movement */
        int16_t sx = x + forward[i] * kaos_s16(STEP_EAST + 2 * dir) + right[i] * kaos_s16(STEP_EAST + 2 * ((dir + 1) & 3));
        int16_t sy = y + forward[i] * kaos_s16(STEP_NORTH + 2 * dir) + right[i] * kaos_s16(STEP_NORTH + 2 * ((dir + 1) & 3));
        squares[i](dir, sx, sy);
    }
    Bitmap screen = { SCREEN_W, SCREEN_H, &fb[0][0] };
    int16_t w = vp.w, h = vp.h;
    Box b;
    if (layout_get_zone(VIEWPORT_ZONE, vp.w, vp.h, &b, &w, &h)) draw_blit(&vp, w, h, &screen, &b, -1, 0);
}

/* Draws the view of (map, x, y, dir) into fb with the palette of the light amount (set_dungeon_view_palette:
   a map without darkness is always bright). Returns 1 on success. */
int view_render(int16_t m, int16_t x, int16_t y, int16_t dir)
{
    if (!g_dungeon_maps || m < 0 || m >= dungeon_map_count()) return 0;
    if (m != map) view_load_map(m);
    int16_t l = 0;
    if (desc[MAP_DIFFICULTY] >> 4) { /* a map of difficulty 0 is always bright */
        if (light < 1) l = LIGHT_DARKEST;
        else
            while (light < kaos_s16(LIGHT_THRESHOLDS + 2 * l)) l++;
    }
    memcpy(palette, pals[l], sizeof pals[l]);
    view_draw(dir & 3, x, y);
    return 1;
}
