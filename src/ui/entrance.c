/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/entrance.h"
#include "data/file.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/dungeon.h"
#include "engine/party.h"
#include "engine/sound.h"
#include "ui/audio.h"
#include "ui/draw.h"
#include "ui/frame.h"
#include "ui/gameloop.h"
#include "ui/layout.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <stdio.h>
#include <string.h>

/* Sizes in pixels, times in frames. */
#define SIDE_COUNT    2 /* the two doors, the two sound channels */
#define BUTTON_HEIGHT 13
#define BUTTON_FRAMES 20
#define DOOR_STEP     2
#define STEP_FRAMES   2
#define SOUND_FRAMES  40

int16_t g_entrance_choice = ENTRANCE_WAITING;

/* The entrance graphic, the view behind the doors (or nothing for NULL), and each door moved apart by
   opened pixels. Returns true while a part of a door shows. */
static bool show_doors(const Bitmap *view, int16_t opened)
{
    Bitmap screen = video_get_screen();
    Box doors, work, door;
    int16_t width = 0;
    int16_t height = 0;
    bool shown = false;
    ui_blit_graphic(&screen, GFX_ENTRANCE, ZONE_SCREEN, COLOR_NONE);
    if (!ui_zone_box(ZONE_ENTRANCE_DOORS, &doors))
        return false;
    if (!layout_get_zone(ZONE_ENTRANCE_WORK, doors.w, doors.h, &work, &width, &height))
        return false;
    width = 0;
    height = 0;
    if (view && layout_get_zone(ZONE_ENTRANCE_VIEWPORT, view->w, view->h, &door, &width, &height)) {
        door.x += work.x;
        door.y += work.y;
        draw_blit(view, width, height, &screen, &door, COLOR_NONE, 0);
    }
    for (int16_t side = 0; side < SIDE_COUNT; side++) {
        const Bitmap *door_bitmap = gfx_get_bitmap(GFX_ENTRANCE_LEFT_DOOR + side);
        width = 0;
        height = 0;
        if (!door_bitmap ||
            !layout_get_zone(ZONE_ENTRANCE_LEFT_DOOR + side, door_bitmap->w, door_bitmap->h, &door, &width, &height))
            continue;
        door.x += work.x;
        door.y += work.y;
        door.w = max16(0, door.w - opened);
        if (side == 0)
            width += opened; /* the left door slides left, the right door slides right */
        else
            door.x += opened;
        if (door.w)
            shown = true;
        draw_blit(door_bitmap, width, height, &screen, &door, COLOR_NONE, 0);
    }
    return shown;
}

/* The entrance palette over the whole screen. */
static void set_entrance_palette(void)
{
    tables_get_palette(TABLES_PALETTE_ENTRANCE, g_screen.palette);
    g_screen.split_top = 0;
    g_screen.split_bottom = SCREEN_H;
}

void entrance_draw(void)
{
    memset(g_screen.px, 0, sizeof g_screen.px);
    set_entrance_palette();
    show_doors(NULL, 0);
}

void entrance_press_button(bool resume)
{
    static const uint8_t volumes[SIDE_COUNT] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    const Bitmap *bm = gfx_get_bitmap(GFX_ENTRANCE_BUTTONS);
    Bitmap screen = video_get_screen();
    Box box;
    int16_t sx = 0;
    int16_t sy = 0;
    if (!g_frame_hook)
        return;
    if (bm &&
        layout_get_zone(resume ? ZONE_ENTRANCE_RESUME : ZONE_ENTRANCE_ENTER, bm->w, BUTTON_HEIGHT, &box, &sx, &sy))
        draw_blit(bm, sx, sy + (resume ? BUTTON_HEIGHT : 0), &screen, &box, COLOR_NONE, 0);
    audio_play_sample(SOUND_SWITCH, volumes);
    frame_wait_count(BUTTON_FRAMES);
}

void entrance_open_doors(void)
{
    static const uint8_t volumes[SIDE_COUNT] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    static uint8_t view_px[SCREEN_W * SCREEN_H];
    char path[PATH_SIZE];
    Box box;
    if (snprintf(path, sizeof path, "%s/Dungeon.DAT", gameloop_get_data_dir()) >= (int)sizeof path || !g_frame_hook ||
        !dungeon_load(path) || !ui_zone_box(ZONE_VIEWPORT, &box))
        return;
    party_set_light_level(g_party_map_index, 0);
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    Bitmap view = {box.w, box.h, view_px};
    for (int16_t y = 0; y < box.h; y++)
        memcpy(view_px + y * box.w, &g_screen.px[box.y + y][box.x], (size_t)box.w);
    set_entrance_palette();
    for (int16_t opened = 0, frames = 0; show_doors(&view, opened) && !g_quit_requested; opened += DOOR_STEP) {
        if (frames % SOUND_FRAMES == 0)
            audio_play_sample(SOUND_ENTRANCE_DOOR, volumes);
        for (int16_t f = 0; f < STEP_FRAMES && frame_wait(); f++, frames++)
            ;
    }
}
