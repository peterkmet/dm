/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/dungeon.h"
#include "engine/map.h"
#include "engine/random.h"
#include "engine/record.h"
#include "engine/recruit.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "ui/view.h"
#include "ui/view_internal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define WALL_TEXT_NEW_LINE  0x80 /* dungeon_decode_text mode 0 codes */
#define WALL_TEXT_END       0x81
#define WALL_TEXT_SIZE      80
#define SCALE_D3_FAR_SIDE_W 30 /* the D3L2 and D3R2 wall sides are wider than high */

/* The inscription of the D1C wall: the blank wall, then the glyphs of each text line. */
static void show_wall_text(const uint8_t *text)
{
    const Bitmap *font = gfx_get_bitmap(GFX_WALL_TEXT_FONT);
    int16_t glyph_w = tables_get_glyph_width();
    int16_t glyph_h = tables_get_glyph_height();
    view_draw_wall_bitmap(view_get_wall_bitmap(LAYOUT_INSCRIPTION_WALL), ZONE_INSCRIPTION_WALL_D1C, BLIT_TRANSPARENT);
    for (int16_t line = 0; font; line++) {
        int16_t n = 0;
        while (text[n] < WALL_TEXT_NEW_LINE)
            n++;
        Box box;
        int16_t width = glyph_w * n;
        int16_t height = glyph_h;
        if (n && layout_get_zone(ZONE_WALL_TEXT_LINE_FIRST + line, 0, 0, &box, &width, &height))
            for (box.w = glyph_w; n; n--, text++, box.x += glyph_w)
                draw_blit(font, *text * glyph_w, 0, &g_view.viewport, &box, TRANSPARENT_COLOR, 0);
        text += n;
        if (*text++ == WALL_TEXT_END)
            break;
    }
}

/* The champion portrait of a portrait sensor on the D1C wall. */
static void show_wall_portrait(int16_t champion)
{
    const Bitmap *portraits = gfx_get_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t pw = tables_get_portrait_width();
    int16_t ph = tables_get_portrait_height();
    int16_t w = pw;
    int16_t h = ph;
    Box b;
    if (portraits && layout_get_zone(ZONE_WALL_PORTRAIT, 0, 0, &b, &w, &h))
        draw_blit(portraits, champion % PORTRAITS_PER_ROW * pw, champion / PORTRAITS_PER_ROW * ph, &g_view.viewport, &b,
                  PORTRAIT_TRANSPARENT, 0);
}

/* The ornament or inscription on a view wall, and on the D1C wall the champion
   portrait and the facing flags. Returns true for an alcove. */
static bool show_wall_ornament(int16_t i, int16_t wall_view, const Aspect *aspect)
{
    if (i < 0)
        return false;
    int16_t zone = ZONE_WALL_ORNAMENT_FIRST + ZONE_WALL_ORNAMENT_VIEWS * g_view.wall_ornaments[i].coord + wall_view;
    bool alcove = view_is_alcove(i);
    bool text = i == g_view.inscription;
    bool mirrored = wall_view == VIEW_WALL_D3R2_LEFT || wall_view == VIEW_WALL_D3R_LEFT ||
                    wall_view == VIEW_WALL_D2R_LEFT || wall_view == VIEW_WALL_D1R_LEFT;
    bool side = mirrored || wall_view == VIEW_WALL_D3L2_RIGHT || wall_view == VIEW_WALL_D3L_RIGHT ||
                wall_view == VIEW_WALL_D2L_RIGHT || wall_view == VIEW_WALL_D1L_RIGHT;
    int16_t graphic = g_view.wall_ornaments[i].graphic + (side ? 0 : WALL_ORNAMENT_FRONT);
    uint8_t wall_text[WALL_TEXT_SIZE] = {WALL_TEXT_END};
    if (text)
        dungeon_decode_text((char *)wall_text, sizeof wall_text, aspect->text, 0);
    if (wall_view == VIEW_WALL_D1C_FRONT) {
        if (text) {
            show_wall_text(wall_text);
            return alcove;
        }
        g_view.result.facing_alcove = alcove;
        g_view.result.facing_vi_altar = i == g_view.vi_altar;
        g_view.result.facing_fountain = i == g_view.fountain;
    }
    ScaledBitmap scaled = {0};
    const Bitmap *bitmap;
    if (wall_view < VIEW_WALL_D1L_RIGHT) { /* D3 and D2: scaled and recolored */
        if (wall_view <= VIEW_WALL_D3R2_LEFT)
            scaled =
                view_get_scaled(graphic, SCALE_D3_FAR_SIDE_W, SCALE_D3, tables_get_palette_changes(TABLES_CHANGES_D3));
        else if (wall_view <= VIEW_WALL_D3R_FRONT)
            scaled = view_get_scaled(graphic, SCALE_D3, SCALE_D3, tables_get_palette_changes(TABLES_CHANGES_D3));
        else
            scaled = view_get_scaled(graphic, SCALE_D2, SCALE_D2, tables_get_palette_changes(TABLES_CHANGES_D2));
        if (!scaled.own.px)
            return alcove;
        bitmap = view_get_scaled_bitmap(&scaled);
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
    if (view_draw_in_zone(bitmap, &g_view.viewport, &(ZoneBlit){zone, mirrored ? FLIP_H : 0, TRANSPARENT_COLOR, place},
                          &drawn) &&
        wall_view == VIEW_WALL_D1C_FRONT)
        g_view.result.clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn;
    if (wall_view == VIEW_WALL_D1C_FRONT && aspect->portrait >= 0)
        show_wall_portrait(aspect->portrait);
    view_free_scaled(&scaled);
    return alcove;
}

void view_draw_floor_ornament(int16_t i, int16_t floor_view)
{
    if (i < 0)
        return;
    bool right = floor_view == VIEW_D3R2 || floor_view == VIEW_D3R || floor_view == VIEW_D2R || floor_view == VIEW_D1R;
    bool center = floor_view == VIEW_D3C || floor_view == VIEW_D2C || floor_view == VIEW_D1C;
    bool flip = right || (i == FLOOR_ORNAMENT_FOOTPRINTS && g_view.use_flipped_walls && center);
    int16_t zone = ZONE_FLOOR_ORNAMENT_FIRST + ZONE_FLOOR_ORNAMENT_VIEWS * g_view.floor_ornaments[i].coord + floor_view;
    view_draw_in_zone(gfx_get_bitmap(g_view.floor_ornaments[i].graphic + tables_get_floor_ornament_offset(floor_view)),
                      &g_view.viewport, &(ZoneBlit){zone, flip ? FLIP_H : 0, TRANSPARENT_COLOR, {0}}, NULL);
}

/* An ornament or mask on the door bitmap, scaled for D3 and D2. */
static void show_door_ornament(int16_t i, int16_t depth, Bitmap *door)
{
    if (i < 0)
        return;
    const Ornament *o = &g_view.door_ornaments[i];
    int16_t zone = ZONE_DOOR_ORNAMENT_FIRST + ZONE_DOOR_ORNAMENT_DEPTHS * o->coord + depth;
    const ZoneBlit blit = {zone, 0, DOOR_ORNAMENT_TRANSPARENT, {0}};
    if (depth == DOOR_DEPTH_D1)
        view_draw_in_zone(gfx_get_bitmap(o->graphic), door, &blit, NULL);
    else {
        int16_t scale = depth == DOOR_DEPTH_D2 ? SCALE_D2 : SCALE_D3;
        ScaledBitmap scaled =
            view_get_scaled(o->graphic, scale, scale,
                            tables_get_palette_changes(depth == DOOR_DEPTH_D2 ? TABLES_CHANGES_DOOR_ORNAMENT_D2
                                                                              : TABLES_CHANGES_DOOR_ORNAMENT_D3));
        if (scaled.own.px)
            view_draw_in_zone(&scaled.own, door, &blit, NULL);
        view_free_scaled(&scaled);
    }
}

/* The button beside a door. The D1C button is clickable. */
static void show_door_button(int16_t button)
{
    ScaledBitmap scaled = {gfx_get_bitmap(GFX_DOOR_BUTTON), {0}};
    if (button != BUTTON_D1C) {
        int16_t scale = button == BUTTON_D2C ? SCALE_D2 : SCALE_D3;
        scaled =
            view_get_scaled(GFX_DOOR_BUTTON, scale, scale,
                            tables_get_palette_changes(button == BUTTON_D2C ? TABLES_CHANGES_D2 : TABLES_CHANGES_D3));
    }
    const Bitmap *bm = view_get_scaled_bitmap(&scaled);
    int16_t zone = ZONE_DOOR_BUTTON_FIRST + tables_get_door_button_coords() + button;
    Box drawn;
    if (bm && bm->px && view_draw_in_zone(bm, &g_view.viewport, &(ZoneBlit){zone, 0, TRANSPARENT_COLOR, {0}}, &drawn) &&
        button == BUTTON_D1C)
        g_view.result.clickable_boxes[VIEW_CLICK_BOX_WALL] = drawn;
    view_free_scaled(&scaled);
}

/* A copy of the door graphic with its ornaments, placed by the door state. A door that does not
   move vertically opens as two halves that slide apart. */
static void show_door(uint16_t door_thing, int16_t state, int16_t door_view)
{
    if (state == DOOR_OPEN)
        return;
    int16_t depth = door_view < VIEW_D2L ? DOOR_DEPTH_D3 : door_view < VIEW_D1L ? DOOR_DEPTH_D2 : DOOR_DEPTH_D1;
    int16_t zone = ZONE_DOOR_FIRST + ZONE_DOOR_VIEW_ZONES * door_view;
    int type = rec_door_type(door_thing);
    const Bitmap *graphic = gfx_get_bitmap(g_view.door_graphics[depth][type]);
    if (!graphic)
        return;
    Bitmap door_copy = {graphic->w, graphic->h, malloc((size_t)graphic->w * graphic->h)};
    if (!door_copy.px)
        return;
    memcpy(door_copy.px, graphic->px, (size_t)graphic->w * graphic->h);
    show_door_ornament(rec_door_ornament(door_thing), depth, &door_copy);
    if (g_view.door_type_attributes[type] & DOOR_ANIMATED)
        draw_flip(&door_copy, random_2bits());
    Placement place = {0};
    if (state == DOOR_DESTROYED)
        show_door_ornament(DOOR_ORNAMENT_DESTROYED, depth, &door_copy);
    else if (state != DOOR_CLOSED) { /* partly open: the zone moves with the state */
        zone += state;
        if (!rec_door_is_vertical(door_thing)) {
            place.zone_dx = door_copy.w >> 1;
            int16_t w = place.zone_dx;
            int16_t h = place.zone_dy;
            Box b;
            if (layout_get_zone(zone + ZONE_DOOR_RIGHT_HALF, door_copy.w, door_copy.h, &b, &w, &h))
                draw_blit(&door_copy, place.zone_dx + w, h, &g_view.viewport, &b, TRANSPARENT_COLOR, 0);
            zone = (zone + ZONE_DOOR_LEFT_HALF) | ZONE_SIZED;
        }
    }
    view_draw_in_zone(&door_copy, &g_view.viewport, &(ZoneBlit){zone, 0, TRANSPARENT_COLOR, place}, NULL);
    free(door_copy.px);
}

/* The hole of an open pit on the map above. */
static void show_ceiling_pit(int16_t graphic, int16_t zone, int16_t x, int16_t y, int mode)
{
    Position above = map_get_level_change_location((Position){g_view.map, x, y}, LEVEL_UP);
    if (above.map >= 0 && rec_square_type(rec_square_byte(above.map, above.x, above.y)) == SQUARE_PIT &&
        rec_square_byte(above.map, above.x, above.y) & PIT_OPEN)
        view_draw_wall_bitmap(view_get_wall_bitmap(graphic), zone, mode);
}

/* The front of a staircase: the up or down graphic at the up or down zone. */
static void show_stairs_front(bool up, int16_t graphic, int16_t up_zone, int mode)
{
    view_draw_wall_bitmap(view_get_wall_bitmap(g_view.stairs + graphic + (up ? 0 : STAIRS_DOWN)),
                          up ? up_zone : up_zone + ZONE_STAIRS_DOWN_DELTA, mode);
}

void view_draw_wall_part(const WallPart *part)
{
    view_draw_wall_bitmap(g_view.wall_set[part->ws], part->zone, part->mode);
}

void view_draw_square_pit(const SquareView *square, bool invisible)
{
    if (square->invisible_pit >= 0 || !invisible)
        view_draw_wall_bitmap(view_get_wall_bitmap(invisible ? square->invisible_pit : square->pit), square->pit_zone,
                              square->mirror);
}

void view_draw_stairs_side(const SquareView *square, bool up)
{
    if (square->stairs_side_down >= 0 && !up)
        view_draw_wall_bitmap(view_get_wall_bitmap(g_view.stairs + square->stairs_side_down),
                              square->stairs_side_down_zone, square->mirror);
    else if (square->stairs_side >= 0)
        view_draw_wall_bitmap(view_get_wall_bitmap(g_view.stairs + square->stairs_side), square->stairs_side_zone,
                              square->mirror);
}

void view_draw_ceiling_pit(const SquareView *square, Location at)
{
    if (square->ceiling >= 0)
        show_ceiling_pit(square->ceiling, square->ceiling_zone, at.x, at.y, square->mirror);
}

void view_draw_square_wall(const SquareView *square, const Aspect *a, int16_t dir, Location at)
{
    if (square->flags & SQ_FRONT) {
        g_view.result.facing_alcove = false;
        g_view.result.facing_vi_altar = false;
        g_view.result.facing_fountain = false;
    }
    view_draw_wall_part(&square->wall);
    if (square->side_face != SIDE_FACE_NONE)
        show_wall_ornament(square->side_face == SIDE_FACE_RIGHT ? a->side[SIDE_RIGHT] : a->side[SIDE_LEFT],
                           square->side_ornament_view, a);
    if (square->front_ornament_view < 0 || !show_wall_ornament(a->side[SIDE_FRONT], square->front_ornament_view, a))
        return;
    if (square->flags & SQ_CEILING_IN_ALCOVE)
        view_draw_ceiling_pit(square, at);
    view_draw_square_things(a->first_thing, dir, at, square->view_square, CELLS_ALCOVE);
}

void view_draw_door_front(const SquareView *square, const Aspect *a, int16_t dir, Location at)
{
    view_draw_floor_ornament(a->floor_ornament, square->view);
    view_draw_square_things(a->first_thing, dir, at, square->view_square, square->door_back);
    for (int i = 0; i < square->frame_count; i++)
        view_draw_wall_part(&square->frames[i]);
    if (square->button >= 0 && rec_door_has_button(a->door))
        show_door_button(square->button);
    show_door(a->door, a->state, square->view);
    if (square->flags & SQ_CEILING_AFTER_DOOR)
        view_draw_ceiling_pit(square, at);
    view_draw_square_things(a->first_thing, dir, at, square->view_square, square->door_front);
}

void view_draw_stairs_front(const SquareView *square, bool up)
{
    if (square->flags & SQ_STAIRS_LAYOUT_IDS)
        view_draw_wall_bitmap(view_get_wall_bitmap(up ? square->stairs_front : square->stairs_front_down),
                              up ? square->stairs_front_zone : square->stairs_front_zone + ZONE_STAIRS_DOWN_DELTA,
                              square->mirror);
    else
        show_stairs_front(up, square->stairs_front, square->stairs_front_zone, square->mirror);
}

void view_draw_d0c_stairs(const SquareView *square, bool up)
{
    show_stairs_front(up, square->stairs_front, square->stairs_front_zone, BLIT_TRANSPARENT);
    show_stairs_front(up, square->stairs_front, square->stairs_front_flipped_zone, BLIT_FLIPPED);
}
