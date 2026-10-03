#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "dungeon.h"
#include "ftl.h"
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
#define WALL_ORN_COORDS 0x102bc4    /* g_wall_ornament_coord_sets */
#define FLOOR_ORN_COORDS 0x102c00   /* g_floor_ornament_coord_sets */
#define DOOR_ORN_COORDS 0x102c0a    /* g_door_ornament_coord_sets */
#define DOOR_BUTTON_COORDS 0x102c16 /* g_door_button_coord_sets */
#define PAL_D3 0x102c18             /* g_palette_changes_d3 */
#define PAL_D2 0x102c28             /* g_palette_changes_d2 */
#define PAL_DOOR_D3 0x102c38        /* g_palette_changes_door_ornament_d3 */
#define PAL_DOOR_D2 0x102c48        /* g_palette_changes_door_ornament_d2 */
#define TEXT_HEIGHTS 0x102c57       /* visible plaque height by line count and depth */

#define VIEWPORT_ZONE 7
#define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
#define LID(a) ids[((a) - WALL_SET_IDS) / 2] /* layout id stored at an original address */

enum { NORMAL, OPAQUE, FLIPPED };
enum { EL, SQ, THING, ST, RT, FR, LT }; /* set_square_aspect words */
enum { WALL, CORRIDOR, PIT, STAIRS, DOOR, TELEPORTER, FAKEWALL };
enum { DOOR_SIDE = 0x10, DOOR_FRONT, STAIRS_SIDE, STAIRS_FRONT };

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

static uint16_t next_thing(uint16_t t) { return BE16(get_thing_data(t)); }
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
    for (; t != 0xfffe && (t >> 10 & 15) <= 3; t = next_thing(t)) {
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

/* 7b hook: draw_objects_creatures_projectiles_explosions(thing, dir, x, y, view_square, cell_order). */
static void things(uint16_t thing, int16_t dir, int16_t x, int16_t y, int16_t view_square, uint16_t cells)
{
    (void)thing, (void)dir, (void)x, (void)y, (void)view_square, (void)cells;
}

/* 7b hook: draw_field with g_field_aspects[g_view_square_field_aspect_index[view_square]]. */
static void field(int16_t view_square, int16_t zone)
{
    (void)view_square, (void)zone;
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

/* draw_bitmap_in_zone. Bit 14 of zone starts from g_zone_relative_x/y. */
static void in_zone(const Bitmap *bm, Bitmap *dst, int16_t zone, int flip, int16_t transparent)
{
    int16_t w = 0, h = 0;
    Box b;
    if (!bm || zone == -1) return;
    if (zone & 0xc000) {
        w = rel_x;
        h = rel_y;
        zone &= (int16_t)0xbfff;
    }
    if (layout_get_zone(zone, bm->w, bm->h, &b, &w, &h)) draw_blit(bm, w, h, dst, &b, transparent, flip);
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
    in_zone(bm, &vp, zone, flip, 10);
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
    if (bm && bm->px) in_zone(bm, &vp, view + kaos_u8(DOOR_BUTTON_COORDS) + 1950, 0, 10);
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
    switch (a.v[EL]) {
    case WALL: /* ponytail: no thieves eye hole yet, needs the party spell state (step 9) */
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

/* set_current_map_and_party_map and load_current_map_graphics: the ornaments, wall set, floor set,
   stairs, and doors of the map. Graphics decode on first use. */
void view_load_map(int16_t m)
{
    map = m;
    mw = dungeon_map_width(m);
    mh = dungeon_map_height(m);
    desc = g_dungeon_maps + 16 * m;
    int16_t walls = desc[0xb] & 15, floors = desc[10] & 15, doors = desc[0xd] & 15;
    const uint8_t *meta = g_dungeon_map_data[m][mw - 1] + mh + (desc[0xd] >> 4);
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
    inscription = walls;
    for (int16_t i = 0; i <= walls; i++) {
        uint8_t o = i < walls ? meta[i] : 0;
        wall_orn[i][0] = 0x103 + 2 * o;
        wall_orn[i][1] = kaos_u8(WALL_ORN_COORDS + o);
        for (int k = 0; k < 3; k++)
            if (kaos_u8(ALCOVES + k) == o) alcoves[n_alcove++] = i;
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
}

/* draw_dungeon_view: ceiling, floor, then the 16 squares back to front, then draw_viewport. */
void view_draw(int16_t dir, int16_t x, int16_t y)
{
    static void (*const squares[16])(int16_t, int16_t, int16_t) = { d3l2, d3r2, d3l, d3r, d3c, d2l2, d2r2, d2l, d2r, d2c, d1l, d1r, d1c, d0l, d0r, d0c };
    static const int8_t forward[16] = { 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 0, 0, 0 }, right[16] = { -2, 2, -1, 1, 0, -2, 2, -1, 1, 0, -1, 1, 0, -1, 1, 0 };
    static uint8_t px[136 * 224];
    vp = (Bitmap){ (uint16_t)kaos_s16(VIEWPORT_SIZE), (uint16_t)kaos_s16(VIEWPORT_SIZE + 2), px };
    if (map < 0 || (size_t)vp.w * vp.h > sizeof px) return;
    memset(px, 0, sizeof px);
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
        things(dungeon_square_first_thing(map, sx, sy), dir, sx, sy, (int16_t[]){ 17, 18, 16 }[i], 1);
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

/* Draws the view of (map, x, y, dir) into fb with the full light palette. Returns 1 on success. */
int view_render(int16_t m, int16_t x, int16_t y, int16_t dir)
{
    if (!g_dungeon_maps || m < 0 || m >= dungeon_map_count()) return 0;
    if (m != map) view_load_map(m);
    for (int i = 0; i < 16; i++) palette[i] = kaos_u16(DUNGEON_PALETTE + 2 * i);
    view_draw(dir & 3, x, y);
    return 1;
}
