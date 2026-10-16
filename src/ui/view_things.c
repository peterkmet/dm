/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/bytes.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/random.h"
#include "engine/record.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "ui/video.h"
#include "ui/view.h"
#include "ui/view_internal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* tables_get_pile_shift pair indices */
#define PILE_FLOOR_SHIFTS 16
#define PILE_ODD_CELL     8
#define PILE_ALCOVE_FIRST 2
#define PILE_ALCOVE_LAST  13

/* The map, square and direction of the projectile tracks. */
typedef struct {
    int16_t map, x, y, dir;
} ViewPosition;

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
    ViewPosition track_view;           /* map, position, and direction of the tracks */
    int16_t motion_step, motion_steps; /* the frame between two ticks; 0 steps: a tick view */
} flight = {
    .track_view = {-1, -1, -1, -1}
};

/* View cells of the things on a square. */
#define VIEW_CELL_BACK_LEFT   0
#define VIEW_CELL_BACK_RIGHT  1
#define VIEW_CELL_FRONT_RIGHT 2
#define VIEW_CELL_FRONT_LEFT  3
#define VIEW_CELL_CENTER      4

/* Creature direction relative to the party. */
#define FACING_AWAY   0
#define FACING_LEFT   1
#define FACING_TOWARD 2
#define FACING_RIGHT  3

/* Door squares draw things behind, then in front of the door; the cell order has one cell per nibble. */
#define DOOR_PASS_BACK   1
#define DOOR_PASS_FRONT  2
#define CELL_ORDER_SHIFT 4

/* A cell order nibble: bit 3 marks a door pass. */
#define CELL_ORDER_DOOR_PASS 0x8
#define CELL_ORDER_MASK      0xf

/* The smallest clickable object box. */
#define CLICK_BOX_MIN_W         14
#define CLICK_BOX_MIN_H         11
#define CLICK_BOX_CENTER_H      15
#define CLICK_BOX_CENTER_OFFSET 7

/* Object aspect record (4 bytes): graphic, flags, alcove coordinate set. */
#define OBJECT_ASPECT_GRAPHIC      0
#define OBJECT_ASPECT_FLAG_BYTE    2
#define OBJECT_ASPECT_ALCOVE_COORD 3

/* Object aspect flags. */
#define OBJECT_FLIP_ON_RIGHT      0x01
#define OBJECT_HAS_ALCOVE_GRAPHIC 0x10

/* Projectile aspect record (4 bytes): graphic, scale flags, pose flags. */
#define PROJECTILE_ASPECT_GRAPHIC    0
#define PROJECTILE_ASPECT_SCALE_BYTE 2
#define PROJECTILE_ASPECT_POSE_BYTE  3

/* Projectile aspect flags, and the kind in the pose flags. */
#define PROJECTILE_SCALE_WITH_ENERGY 0x01
#define PROJECTILE_FLIP_ON_LEFT      0x10
#define PROJECTILE_KIND              0x03

/* Projectile kinds: which poses the graphic set has. */
#define PROJECTILE_BACK_AND_ROTATION   0
#define PROJECTILE_BACK                1
#define PROJECTILE_NO_BACK             2
#define PROJECTILE_NO_BACK_NO_ROTATION 3
#define POSE_FRONT                     0 /* graphic offset from the projectile aspect graphic */
#define POSE_BACK                      1
#define POSE_SIDE                      2
#define WEAPON_PROJECTILE_ASPECT_SHIFT 8 /* 0: the object aspect */

/* Projectile aspect of a weapon in its object info. */
#define WEAPON_PROJECTILE_ASPECT_MASK 0x1f

/* Projectile scale by energy. */
#define PROJECTILE_ENERGY_MIN         96
#define PROJECTILE_SCALE_MIN          2
#define PROJECTILE_ENERGY_SCALE_SHIFT 8

/* Explosion graphics. */
#define EXPLOSION_ASPECT_FIRE       0
#define EXPLOSION_ASPECT_SPELL      1
#define EXPLOSION_ASPECT_POISON     2
#define EXPLOSION_ASPECT_SMOKE      3
#define EXPLOSION_ATTACK_MIN        48
#define EXPLOSION_STRENGTH_SHIFT    5
#define EXPLOSION_PATTERN_STRENGTHS 3

/* Field aspect bytes and the masked fill arguments. */
#define FIELD_PATTERN            0
#define FIELD_LAST_UNIT          1
#define FIELD_TRANSPARENT        2
#define FIELD_MASK_BYTE          3
#define FIELD_WIDTH              4
#define FIELD_HEIGHT             5
#define FIELD_ASPECT_SIZE        8
#define EXPLOSION_FILL_LAST_UNIT 87

/* Masked fill arguments: no mask, a flipped mask, the random first unit. */
#define FIELD_NO_MASK                   0xff
#define FIELD_MASK_FLIPPED              0x80
#define FIELD_FIRST_UNIT_RANGE          0x1f
#define EXPLOSION_FILL_FIRST_UNIT_RANGE 0x3f
#define EXPLOSION_FILL_TRANSPARENT      (FILL_NO_MASK | TRANSPARENT_COLOR)

/* Palette changes of objects and projectiles: none near, then the D2 and D3 tables. */
static const uint8_t *get_object_palette_changes(int16_t distance)
{
    return distance == 1   ? tables_get_palette_changes(TABLES_CHANGES_OBJECT_D2)
           : distance == 2 ? tables_get_palette_changes(TABLES_CHANGES_OBJECT_D3)
                           : NULL;
}

/* The aspect of an object by its type. */
static int16_t get_object_aspect(uint16_t thing)
{
    return tables_get_object_info(inventory_get_object_info_index(thing))->aspect_index;
}

ProjectileAspect view_get_projectile_aspect(uint16_t thing)
{
    if (THING_TYPE(thing) == THING_EXPLOSION) {
        switch (thing - EXPLOSION_THING_BASE) {
        case EXPLOSION_FIREBALL:
            return (ProjectileAspect){true, PROJECTILE_ASPECT_FIREBALL};
        case EXPLOSION_SLIME:
            return (ProjectileAspect){true, PROJECTILE_ASPECT_SLIME};
        case EXPLOSION_LIGHTNING_BOLT:
            return (ProjectileAspect){true, PROJECTILE_ASPECT_LIGHTNING};
        case EXPLOSION_POISON_BOLT:
        case EXPLOSION_POISON_CLOUD:
            return (ProjectileAspect){true, PROJECTILE_ASPECT_POISON};
        default:
            return (ProjectileAspect){true, PROJECTILE_ASPECT_SPELL};
        }
    }
    if (THING_TYPE(thing) == THING_WEAPON) {
        int16_t aspect = tables_get_weapon_info(rec_weapon_type(thing))->attributes >> WEAPON_PROJECTILE_ASPECT_SHIFT &
                         WEAPON_PROJECTILE_ASPECT_MASK;
        if (aspect)
            return (ProjectileAspect){true, aspect};
    }
    return (ProjectileAspect){false, get_object_aspect(thing)};
}

/* The pile box of a near object grows by the drawn box; a single object gets at least
   CLICK_BOX_MIN_W x CLICK_BOX_MIN_H pixels around its center. The object becomes the pile top and joins the pile list. */
static void add_clickable_object(int16_t index, uint16_t thing, const Box *drawn)
{
    Box *box = &g_view.result.clickable_boxes[index];
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
    g_view.result.pile_top_objects[index] = thing;
    if (g_view.result.pile_object_counts[index] < VIEW_PILE_OBJECT_MAX)
        g_view.result.pile_objects[index][g_view.result.pile_object_counts[index]++] = thing;
}

/* One view square while its things draw. */
typedef struct {
    int16_t dir;         /* party direction */
    Location at;         /* the map square */
    int16_t view_square; /* VIEW_SQUARE_* */
    int16_t lane, depth; /* of the view square */
    int16_t coord;       /* object or alcove coordinate set, -1 none */
    int16_t pass;        /* DOOR_PASS_* of a door square, else 0 */
    int16_t cell;        /* the view cell that draws, VIEW_CELL_* */
    int16_t map_cell;    /* its map cell */
} SquareDraw;

/* Where an object of a pile goes. */
typedef struct {
    int16_t zone;      /* the zone of an object in flight */
    int16_t *pile;     /* objects drawn on the pile so far; NULL: an object in flight */
    bool alcove;       /* the pile is in an alcove */
    bool alcove_front; /* the alcove faces the party */
    Placement place;   /* the shift of an object in flight */
    Box *drawn;        /* gets the drawn box of an object in flight; NULL: not wanted */
} ObjectSpot;

/* One object on the floor or in an alcove, in view cell sq->cell. spot->pile counts the objects of the cell for
   their shift; NULL draws an object in flight at spot->zone without pile shift. */
static void show_object(const SquareDraw *sq, uint16_t thing, int16_t aspect_index, const ObjectSpot *spot)
{
    int16_t cell = sq->cell;
    int16_t lane = sq->lane;
    int16_t *pile = spot->pile;
    const uint8_t *aspect = tables_get_object_aspect(aspect_index);
    bool alcove_graphic = spot->alcove_front && (aspect[OBJECT_ASPECT_FLAG_BYTE] & OBJECT_HAS_ALCOVE_GRAPHIC);
    int16_t graphic = GFX_OBJECT_FIRST + aspect[OBJECT_ASPECT_GRAPHIC] + alcove_graphic;
    int16_t distance = 0;
    int flip = (aspect[OBJECT_ASPECT_FLAG_BYTE] & OBJECT_FLIP_ON_RIGHT) && !alcove_graphic &&
               (lane >= 1 || (lane == 0 && (cell == VIEW_CELL_BACK_RIGHT || cell == VIEW_CELL_FRONT_RIGHT)));
    ZoneBlit blit = {spot->zone, flip ? FLIP_H : 0, TRANSPARENT_COLOR, spot->place};
    ScaledBitmap scaled;
    if (sq->depth == DEPTH_D0)
        scaled = view_get_scaled(graphic, SCALE_NATIVE, SCALE_NATIVE, NULL);
    else {
        int16_t row =
            sq->depth * 2 - 1 - (cell == VIEW_CELL_CENTER ? 1 : cell >> 1); /* front cells count as one row nearer */
        distance = (row + 1) >> 1;
        int16_t scale = tables_get_object_scale(row);
        scaled = view_get_scaled(graphic, scale, scale, get_object_palette_changes(distance));
    }
    if (pile) {
        blit.zone = ZONE_SHIFTED | (spot->alcove ? ZONE_ALCOVE_OBJECT_FIRST + sq->coord +
                                                       aspect[OBJECT_ASPECT_ALCOVE_COORD] * ZONE_ALCOVE_VIEW_SETS
                                                 : ZONE_OBJECT_FIRST + cell + sq->coord * ZONE_OBJECT_CELLS);
        blit.place.zone_dx = tables_get_creature_shift(distance, tables_get_pile_shift(*pile, TABLES_AXIS_X));
        blit.place.zone_dy = tables_get_creature_shift(distance, tables_get_pile_shift(*pile, TABLES_AXIS_Y));
        ++*pile;
        if (!spot->alcove)
            *pile &= PILE_FLOOR_SHIFTS - 1;
        else if (*pile > PILE_ALCOVE_LAST)
            *pile = PILE_ALCOVE_FIRST;
    }
    Box box;
    bool drew = view_draw_in_zone(view_get_scaled_bitmap(&scaled), &g_view.viewport, &blit, &box);
    if (drew && spot->drawn)
        *spot->drawn = box;
    if (drew && pile && distance == 0 && lane == 0)
        add_clickable_object(spot->alcove ? VIEW_CLICK_BOX_ALCOVE : cell, thing, &box);
    view_free_scaled(&scaled);
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
    const ActiveGroup *a = g_view.map == g_party_map_index ? group_get_active(thing) : NULL;
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
   at position pos (view cell, or 4 centered) of the creature coordinate set of the view square. */
static void show_creature(const SquareDraw *sq, const ViewGroup *group, uint8_t look, uint16_t facing, int16_t pos)
{
    int16_t coord = tables_get_view_square(sq->view_square).creature_coord;
    int16_t depth = sq->depth;
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
    ScaledBitmap scaled;
    if (depth < DEPTH_D2)
        scaled = view_get_scaled(graphic, SCALE_NATIVE, SCALE_NATIVE, NULL);
    else {
        const uint8_t *pal =
            depth == DEPTH_D2 ? g_view.creature_palette_changes_d2 : g_view.creature_palette_changes_d3;
        distance = depth == DEPTH_D2 ? 1 : 2;
        transparent = pal[transparent];
        int16_t scale = depth == DEPTH_D2 ? SCALE_D2 : SCALE_D3;
        scaled = view_get_scaled(graphic, scale, scale, pal);
    }
    Placement place = {
        .zone_dx = tables_get_creature_shift(distance, group_aspect_x_offset(look)),
        .zone_dy = tables_get_creature_shift(distance, group_aspect_y_offset(look)),
    };
    int16_t set = group->aspect[CREATURE_ASPECT_COORD] >> CREATURE_ASPECT_COORD_SHIFT;
    int16_t zone =
        ZONE_SHIFTED | (ZONE_CREATURE_FIRST + set * ZONE_CREATURE_SET + coord * ZONE_CREATURE_POSITIONS + pos);
    view_draw_in_zone(view_get_scaled_bitmap(&scaled), &g_view.viewport,
                      &(ZoneBlit){zone, flip ? FLIP_H : 0, transparent, place}, NULL);
    view_free_scaled(&scaled);
}

/* The creatures of the group in view cell sq->cell (map cell sq->map_cell). rest holds the cells still to draw,
   front tells that the cell is a front cell of the pass. Returns true when the group is complete. */
static bool show_creatures_in_cell(const SquareDraw *sq, const ViewGroup *group, uint16_t rest, bool front)
{
    int16_t dir = sq->dir;
    int16_t view_square = sq->view_square;
    int16_t pass = sq->pass;
    int16_t coord = tables_get_view_square(view_square).creature_coord;
    int16_t index;
    int16_t second = 0;
    int16_t pos = sq->cell;
    bool done = false;
    if (coord < 0)
        return true;
    uint16_t creature_index = 0;
    index = get_creature_in_cell(group, sq->map_cell);
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
    show_creature(sq, group, group->look[index], facing, pos);
    if (second) /* the other creature of a half-size pair, beside it */
        show_creature(sq, group, group->look[index == 0], facing,
                      pos == VIEW_CELL_BACK_RIGHT ? VIEW_CELL_BACK_LEFT : VIEW_CELL_BACK_RIGHT);
    return done;
}

void view_start_tracks(int16_t dir, Location at)
{
    if (flight.motion_steps)
        return;
    ViewPosition place = {g_view.map, at.x, at.y, dir};
    bool same = place.map == flight.track_view.map && place.x == flight.track_view.x &&
                place.y == flight.track_view.y && place.dir == flight.track_view.dir;
    flight.last_track_count = same ? flight.track_count : 0;
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

void view_set_motion(int16_t step, int16_t steps)
{
    flight.motion_step = step;
    flight.motion_steps = steps;
}

bool view_has_moving_projectiles(void)
{
    for (int i = 0; i < flight.track_count; i++)
        if (flight.tracks[i].from.x != flight.tracks[i].to.x || flight.tracks[i].from.y != flight.tracks[i].to.y)
            return true;
    return false;
}

/* A projectile graphic: the pose and the mirror flag. */
typedef struct {
    int16_t pose;
    int16_t flip;
} ProjectilePose;

/* The pose of a projectile graphic by its direction of flight, and its mirror flags. */
static ProjectilePose get_projectile_pose(const SquareDraw *sq, const uint8_t *aspect, uint16_t thing)
{
    int16_t dir = sq->dir;
    int16_t cell = sq->cell;
    int16_t lane = sq->lane;
    int16_t kind = aspect[PROJECTILE_ASPECT_POSE_BYTE] & PROJECTILE_KIND;
    int16_t pose;
    int16_t flip = 0;
    bool rotates = kind == PROJECTILE_BACK_AND_ROTATION;
    bool odd_square = rotates && ((sq->at.x + sq->at.y) & 1);
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
            if ((aspect[PROJECTILE_ASPECT_POSE_BYTE] & PROJECTILE_FLIP_ON_LEFT) && lane < 1 &&
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
    return (ProjectilePose){pose, flip};
}

/* A projectile in view cell sq->cell: an object in flight, or a projectile graphic posed and mirrored by
   its direction of flight and scaled by depth (and kinetic energy). */
static void show_projectile(const SquareDraw *sq, uint16_t thing)
{
    int16_t cell = sq->cell;
    int16_t depth = sq->depth;
    int16_t zone = ZONE_PROJECTILE_FIRST + sq->coord * ZONE_OBJECT_CELLS + cell;
    ProjectileAspect flight_aspect = view_get_projectile_aspect(rec_projectile_slot(thing));
    if (!flight_aspect.projectile) {
        Box drawn = {0};
        show_object(sq, thing, flight_aspect.index,
                    &(ObjectSpot){.zone = zone, .place = begin_projectile(thing), .drawn = &drawn});
        end_projectile(thing, drawn.w > 0, &drawn);
        return;
    }
    const uint8_t *aspect = tables_get_projectile_aspect(flight_aspect.index);
    int16_t by_energy = aspect[PROJECTILE_ASPECT_SCALE_BYTE] & PROJECTILE_SCALE_WITH_ENERGY;
    int16_t scale = SCALE_NATIVE;
    int16_t row = 0;
    uint8_t energy = rec_projectile_energy(thing);
    if ((by_energy && energy != KINETIC_ENERGY_MAXIMUM) || depth) {
        row = depth * 2 - (cell >> 1);
        scale = tables_get_projectile_scale(row);
        if (by_energy) /* 16-bit product and signed byte as in the original */
            scale = max16(PROJECTILE_SCALE_MIN, (int8_t)((uint16_t)(max16(PROJECTILE_ENERGY_MIN, energy + 1) * scale) >>
                                                         PROJECTILE_ENERGY_SCALE_SHIFT));
    }
    ProjectilePose pose = get_projectile_pose(sq, aspect, thing);
    int16_t distance = row >> 1 == 2 ? 1 : row >> 1 == 3 ? 2 : 0; /* D2 or D3 palette changes */
    ScaledBitmap scaled = view_get_scaled(GFX_PROJECTILE_FIRST + aspect[PROJECTILE_ASPECT_GRAPHIC] + pose.pose, scale,
                                          scale, scale == SCALE_NATIVE ? NULL : get_object_palette_changes(distance));
    Box drawn;
    bool drew = view_draw_in_zone(view_get_scaled_bitmap(&scaled), &g_view.viewport,
                                  &(ZoneBlit){zone, pose.flip, TRANSPARENT_COLOR, begin_projectile(thing)}, &drawn);
    end_projectile(thing, drew, &drawn);
    view_free_scaled(&scaled);
}

/* A teleporter field or fluxcage: the pattern graphic through the mask graphic at zone, with a random
   start in the pattern so that it sparkles. */
static void show_field(const uint8_t *aspect, int16_t zone)
{
    int16_t width = aspect[FIELD_WIDTH];
    int16_t height = aspect[FIELD_HEIGHT];
    int16_t sx = 0;
    bool flip = false;
    Box box;
    if (!layout_get_zone(zone, 0, 0, &box, &width, &height))
        return;
    const Bitmap *mask = NULL;
    const Bitmap *units = gfx_get_bitmap(GFX_FIELD_PATTERN_FIRST + aspect[FIELD_PATTERN]);
    if (aspect[FIELD_MASK_BYTE] != FIELD_NO_MASK) {
        mask = gfx_get_bitmap(GFX_FIELD_MASK_FIRST + (aspect[FIELD_MASK_BYTE] & ~FIELD_MASK_FLIPPED));
        flip = !!(aspect[FIELD_MASK_BYTE] & FIELD_MASK_FLIPPED);
        if (flip && mask && mask->w % PIXELS_PER_WORD)
            sx = PIXELS_PER_WORD - mask->w % PIXELS_PER_WORD;
    }
    int16_t first = random16() & FIELD_FIRST_UNIT_RANGE;
    int16_t last = aspect[FIELD_LAST_UNIT] + random_bit();
    if (units)
        draw_masked_fill(&g_view.viewport, &box,
                         &(MaskedFill){units, mask, flip, last, first, aspect[FIELD_TRANSPARENT], sx, 0});
}

/* An explosion of aspect at scale, recolored for smoke. */
static ScaledBitmap get_explosion_bitmap(int16_t aspect, int16_t scale)
{
    scale = bounded_value(SCALE_EXPLOSION_MIN, scale & ~1, SCALE_NATIVE);
    return view_get_scaled(GFX_EXPLOSION_FIRST + min16(aspect, EXPLOSION_ASPECT_POISON), scale, scale,
                           aspect == EXPLOSION_ASPECT_SMOKE ? tables_get_palette_changes(TABLES_CHANGES_SMOKE) : NULL);
}

/* An explosion on the party square fills the whole viewport with its pattern. */
static void show_explosion_on_party(uint16_t thing, int16_t aspect)
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
    if (units && layout_get_zone(ZONE_VIEWPORT_BOX, g_view.viewport.w, g_view.viewport.h, &box, &width, &height))
        draw_masked_fill(&g_view.viewport, &box,
                         &(MaskedFill){units, NULL, false, last, first, EXPLOSION_FILL_TRANSPARENT, 0, 0});
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
static void show_rebirth_step_1(int16_t coord, int16_t flip)
{
    int16_t scale = tables_get_rebirth_scale(TABLES_REBIRTH_STEP_1, coord);
    int16_t g = GFX_PROJECTILE_FIRST +
                tables_get_projectile_aspect(PROJECTILE_ASPECT_LIGHTNING)[PROJECTILE_ASPECT_GRAPHIC] + POSE_BACK;
    ScaledBitmap scaled = view_get_scaled(g, scale, scale, NULL);
    view_draw_in_zone(view_get_scaled_bitmap(&scaled), &g_view.viewport,
                      &(ZoneBlit){ZONE_REBIRTH_STEP1_FIRST + coord, flip, TRANSPARENT_COLOR, {0}}, NULL);
    view_free_scaled(&scaled);
}

/* A fluxcage: the field of the square with the pattern after the teleporter pattern. */
static void show_fluxcage(int16_t field_index)
{
    uint8_t aspect[FIELD_ASPECT_SIZE];
    memcpy(aspect, tables_get_field_aspect(field_index), FIELD_ASPECT_SIZE);
    aspect[FIELD_PATTERN]++; /* the fluxcage pattern follows the teleporter pattern */
    show_field(aspect, ZONE_FIELD_FIRST + field_index);
}

/* The explosions of a square, each mirrored at random, then a fluxcage as a field. */
static void show_explosions(const SquareDraw *sq, uint16_t first)
{
    int16_t dir = sq->dir;
    int16_t depth = sq->depth;
    int16_t coord = sq->coord;
    ViewSquare square = tables_get_view_square(sq->view_square);
    int16_t explosion_coord = square.explosion_coord;
    int16_t field_index = square.field_aspect;
    bool fluxcage = false;
    if (explosion_coord < 0 && field_index < 0)
        return;
    for (uint16_t thing = first; thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        if (THING_TYPE(thing) != THING_EXPLOSION)
            continue;
        uint16_t cell = THING_CELL(thing);
        int16_t type = rec_explosion_type(thing);
        bool rebirth = type >= EXPLOSION_REBIRTH_STEP_1;
        int16_t zone;
        int16_t scale;
        if (rebirth && !(coord >= 0 && cell == sq->map_cell))
            continue;
        int16_t flip = random_2bits();
        if (type == EXPLOSION_REBIRTH_STEP_1) {
            show_rebirth_step_1(coord, flip);
            continue;
        }
        if (type == EXPLOSION_FLUXCAGE) {
            fluxcage |= depth < DEPTH_D4;
            continue;
        }
        int16_t aspect = get_explosion_aspect(type);
        if (explosion_coord < 0)
            continue;
        if (sq->view_square == VIEW_SQUARE_D0C) {
            show_explosion_on_party(thing, aspect);
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
        ScaledBitmap scaled = get_explosion_bitmap(aspect, scale);
        view_draw_in_zone(view_get_scaled_bitmap(&scaled), &g_view.viewport,
                          &(ZoneBlit){zone, flip, TRANSPARENT_COLOR, {0}}, NULL);
        view_free_scaled(&scaled);
    }
    if (fluxcage && field_index >= 0 && sq->pass != DOOR_PASS_BACK && !g_do_not_draw_fluxcages)
        show_fluxcage(field_index);
}

/* D3 squares show only their front cells, D0 squares only their back cells. */
static bool is_cell_in_view(int16_t depth, int16_t cell)
{
    return (depth != DEPTH_D3 || cell >= VIEW_CELL_FRONT_RIGHT) && (depth != DEPTH_D0 || cell < VIEW_CELL_FRONT_RIGHT);
}

/* The objects of map cell sq->map_cell, drawn in view cell sq->cell; pile counts the objects drawn on the pile. */
static void show_cell_objects(const SquareDraw *sq, uint16_t first, int16_t *pile, bool alcove)
{
    if (sq->coord < 0 || !is_cell_in_view(sq->depth, sq->cell))
        return;
    const ObjectSpot spot = {.pile = pile, .alcove = alcove, .alcove_front = alcove && sq->lane == 0};
    for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        int16_t type = THING_TYPE(t);
        if (type >= THING_WEAPON && type <= THING_JUNK && THING_CELL(t) == sq->map_cell)
            show_object(sq, t, get_object_aspect(t), &spot);
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

void view_draw_square_things(uint16_t first, int16_t dir, Location at, int16_t view_square, uint16_t order)
{
    if (first == THING_ENDOFLIST)
        return;
    ViewSquare square = tables_get_view_square(view_square);
    SquareDraw sq = {.dir = dir, .at = at, .view_square = view_square, .lane = square.lane};
    sq.depth = square.depth;
    if (order & CELL_ORDER_DOOR_PASS) {
        sq.pass = (order & 1) ? DOOR_PASS_FRONT : DOOR_PASS_BACK;
        order >>= CELL_ORDER_SHIFT;
    }
    uint16_t rest = order;
    uint16_t group_thing = THING_NONE;
    bool alcove = rest == 0;
    bool has_projectiles = false;
    bool has_explosions = false;
    bool group_done = false;
    bool loaded = false;
    sq.coord = alcove ? square.alcove_coord : square.object_coord;
    int16_t count = 0;
    ViewGroup group;
    find_square_things(first, &group_thing, &has_projectiles, &has_explosions);
    do {
        int16_t pile;
        if (!alcove) {
            sq.cell = (rest & CELL_ORDER_MASK) - 1;
            rest >>= CELL_ORDER_SHIFT;
            count++;
            sq.map_cell = NORMALIZE(sq.cell + dir);
            pile = 0;
        } else {
            sq.cell = VIEW_CELL_CENTER;
            sq.map_cell = direction_back(dir); /* the alcove side of the wall faces the party */
            pile = PILE_ALCOVE_FIRST;
        }
        pile += (sq.map_cell & 1) * PILE_ODD_CELL; /* odd map cells start in the second half of the shifts */
        show_cell_objects(&sq, first, &pile, alcove);
        if (alcove || sq.depth >= DEPTH_D4)
            break;
        bool front = (sq.cell < VIEW_CELL_FRONT_RIGHT || count == 1) &&
                     (rest == 0 || (rest & CELL_ORDER_MASK) - 1 >= VIEW_CELL_FRONT_RIGHT);
        if (group_thing != THING_NONE && !group_done) {
            if (!loaded) {
                load_group_aspect(group_thing, &group);
                loaded = true;
            }
            group_done = show_creatures_in_cell(&sq, &group, rest, front);
        }
        /* the alcove pass broke out above, so sq.coord is square.object_coord here */
        if (has_projectiles && square.object_coord >= 0 && is_cell_in_view(sq.depth, sq.cell))
            for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t))
                if (THING_TYPE(t) == THING_PROJECTILE && THING_CELL(t) == sq.map_cell)
                    show_projectile(&sq, t);
    } while (rest);
    /* the last drawn map cell decides the rebirth check, as in the original */
    if (has_explosions)
        show_explosions(&sq, first);
}

void view_draw_teleporter_field(int16_t view_square, int16_t zone)
{
    show_field(tables_get_field_aspect(tables_get_view_square(view_square).field_aspect), zone);
}
