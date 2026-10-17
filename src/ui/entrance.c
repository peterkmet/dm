/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "entrance.h"
#include "file.h"
#include "graphics.h"
#include "ids.h"
#include "tables.h"
#include "arith.h"
#include "dungeon.h"
#include "party.h"
#include "sound.h"
#include "audio.h"
#include "command.h"
#include "draw.h"
#include "frame.h"
#include "gameloop.h"
#include "layout.h"
#include "palette.h"
#include "startend.h"
#include "ui.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <string.h>

/* Sizes in pixels, times in frames. */
#define SIDE_COUNT     2 /* the two doors, the two sound channels */
#define BUTTON_HEIGHT  13
#define BUTTON_FRAMES  20
#define DOOR_STEP      2
#define STEP_FRAMES    2
#define SOUND_FRAMES   40
#define CREDITS_FRAMES 1800 /* 36 seconds */

int16_t g_entrance_choice = ENTRANCE_WAITING;

/* The entrance graphic, the view behind the doors (or nothing for NULL), and each door moved apart by
   opened pixels. Returns true while a part of a door shows. */
static bool show_doors(const Bitmap *view, int16_t opened)
{
    Bitmap screen = video_get_screen();
    Box doors;
    ZoneClip work, door;
    bool shown = false;
    ui_blit_graphic(&screen, GFX_ENTRANCE, ZONE_SCREEN, COLOR_NONE);
    if (!ui_zone_box(ZONE_ENTRANCE_DOORS, &doors))
        return false;
    if (!layout_get_zone(ZONE_ENTRANCE_WORK, &(ZoneItem){doors.w, doors.h, 0, 0}, &work))
        return false;
    if (view && layout_get_zone(ZONE_ENTRANCE_VIEWPORT, &(ZoneItem){view->w, view->h, 0, 0}, &door)) {
        door.box.x += work.box.x;
        door.box.y += work.box.y;
        draw_blit(&(BlitSource){view, door.source_x, door.source_y, COLOR_NONE, 0}, &screen, &door.box);
    }
    for (int16_t side = 0; side < SIDE_COUNT; side++) {
        const Bitmap *door_bitmap = gfx_get_bitmap(GFX_ENTRANCE_LEFT_DOOR + side);
        if (!door_bitmap ||
            !layout_get_zone(ZONE_ENTRANCE_LEFT_DOOR + side, &(ZoneItem){door_bitmap->w, door_bitmap->h, 0, 0}, &door))
            continue;
        door.box.x += work.box.x;
        door.box.y += work.box.y;
        door.box.w = max16(0, door.box.w - opened);
        if (side == 0)
            door.source_x += opened; /* the left door slides left, the right door slides right */
        else
            door.box.x += opened;
        if (door.box.w)
            shown = true;
        draw_blit(&(BlitSource){door_bitmap, door.source_x, door.source_y, COLOR_NONE, 0}, &screen, &door.box);
    }
    return shown;
}

/* The top palette over the whole screen. */
static void set_full_screen_split(void)
{
    g_screen.split_top = 0;
    g_screen.split_bottom = SCREEN_H;
}

/* The entrance palette over the whole screen. */
static void set_entrance_palette(void)
{
    tables_get_palette(TABLES_PALETTE_ENTRANCE, g_screen.palette);
    set_full_screen_split();
}

void entrance_draw(void)
{
    uint16_t colors[PALETTE_COLORS];
    palette_fade_to_black();
    memset(g_screen.px, 0, sizeof g_screen.px);
    set_full_screen_split();
    show_doors(NULL, 0);
    tables_get_palette(TABLES_PALETTE_ENTRANCE, colors);
    palette_fade_to(colors);
}

void entrance_press_button(bool resume)
{
    static const uint8_t volumes[SIDE_COUNT] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    const Bitmap *bm = gfx_get_bitmap(GFX_ENTRANCE_BUTTONS);
    Bitmap screen = video_get_screen();
    ZoneClip placed;
    if (!g_frame_hook)
        return;
    if (bm && layout_get_zone(resume ? ZONE_ENTRANCE_RESUME : ZONE_ENTRANCE_ENTER,
                              &(ZoneItem){bm->w, BUTTON_HEIGHT, 0, 0}, &placed))
        draw_blit(&(BlitSource){bm, placed.source_x, placed.source_y + (resume ? BUTTON_HEIGHT : 0), COLOR_NONE, 0},
                  &screen, &placed.box);
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

void entrance_show_credits(void)
{
    uint16_t colors[PALETTE_COLORS];
    palette_fade_to_black();
    startend_draw_credits();
    tables_get_palette(TABLES_PALETTE_CREDITS, colors);
    palette_fade_to(colors);
    while (command_is_left_button_down() && frame_wait()) /* the countdown starts when no button is held */
        ;
    command_discard_all_input(); /* the keys and clicks of the fades do not end the credits */
    for (int16_t frames = 0; frames < CREDITS_FRAMES && !g_input_received && frame_wait(); frames++)
        ;
}
