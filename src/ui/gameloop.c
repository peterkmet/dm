/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/gameloop.h"
#include "data/adventure.h"
#include "data/ftl.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/explored.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/move.h"
#include "engine/projectile.h"
#include "engine/random.h"
#include "engine/savegame.h"
#include "engine/sensor.h"
#include "engine/sound.h"
#include "engine/timeline_process.h"
#include "ui/advanced.h"
#include "ui/audio.h"
#include "ui/automap.h"
#include "ui/command.h"
#include "ui/dialog.h"
#include "ui/layout.h"
#include "ui/music.h"
#include "ui/startend.h"
#include "ui/text.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define POINTER_ZONE_COUNT   16
#define POINTER_SHADOW_COLOR 12 /* black: the shadow, swapped with the transparent color 0 */

/* PointerZone.champion: champion index + 1 in bits 0-2; 8 and more is the name zone of the champion */
#define ZONE_CHAMPION_INDEX_MASK 7
#define ZONE_CHAMPION_NAME       8

#define POINTER_ARROW  0
#define POINTER_HAND   1
#define POINTER_OBJECT 2

static int16_t frames_waiting;
static uint16_t shown_hand_object = THING_NONE;
static int16_t shown_hand_icon; /* a waterskin or a torch changes its icon and its name */
static char data_dir[PATH_SIZE];

bool gameloop_load_data(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(data_dir, sizeof data_dir, "%s", dir);
    snprintf(path, sizeof path, "%s/%s", dir, adventure_engine_name());
    if (tables_load(path))
        return false;
    snprintf(path, sizeof path, "%s/Graphics.DAT", dir);
    startend_set_save_dir(dir);
    g_endgame_sensor_hook = startend_run_endgame_sensor;
    return gfx_open(path) >= 0 && !layout_init() && !text_init() && !ui_init();
}

const char *gameloop_get_data_dir(void)
{
    return data_dir;
}

/* The entrance graphic, the view behind the doors (or nothing for NULL), and each door moved apart by
   opened pixels. Returns 1 while a part of a door shows. */
static int draw_entrance_doors(const Bitmap *view, int16_t opened)
{
    Bitmap screen = ui_get_screen();
    Box doors, work, door;
    int16_t width = 0;
    int16_t height = 0;
    int16_t shown = 0;
    ui_blit_graphic(&screen, GFX_ENTRANCE, ZONE_SCREEN, COLOR_NONE);
    if (!ui_zone_box(ZONE_ENTRANCE_DOORS, &doors))
        return 0;
    if (!layout_get_zone(ZONE_ENTRANCE_WORK, doors.w, doors.h, &work, &width, &height))
        return 0;
    width = 0;
    height = 0;
    if (view && layout_get_zone(ZONE_ENTRANCE_VIEWPORT, view->w, view->h, &door, &width, &height)) {
        door.x += work.x;
        door.y += work.y;
        draw_blit(view, width, height, &screen, &door, COLOR_NONE, 0);
    }
    for (int16_t side = 0; side < 2; side++) {
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
            shown = 1;
        draw_blit(door_bitmap, width, height, &screen, &door, COLOR_NONE, 0);
    }
    return shown;
}

/* The entrance palette over the whole screen. */
static void set_entrance_palette(void)
{
    tables_get_palette(TABLES_PALETTE_ENTRANCE, palette);
    palette_split_top = 0;
    palette_split_bottom = SCREEN_H;
}

/* The entrance graphic with both doors closed, in the entrance palette on the whole screen.
   ponytail: no credits. */
void entrance_draw(void)
{
    memset(fb, 0, sizeof fb);
    set_entrance_palette();
    draw_entrance_doors(NULL, 0);
}

/* ENTER or RESUME shows its pressed button, plays the switch sound, and waits 20 frames. */
void entrance_press_button(int resume)
{
    enum { BUTTON_HEIGHT = 13, BUTTON_FRAMES = 20 };
    static const uint8_t volumes[2] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    const Bitmap *bm = gfx_get_bitmap(GFX_ENTRANCE_BUTTONS);
    Bitmap screen = ui_get_screen();
    Box box;
    int16_t sx = 0;
    int16_t sy = 0;
    if (!g_frame_hook)
        return;
    if (bm &&
        layout_get_zone(resume ? ZONE_ENTRANCE_RESUME : ZONE_ENTRANCE_ENTER, bm->w, BUTTON_HEIGHT, &box, &sx, &sy))
        draw_blit(bm, sx, sy + (resume ? BUTTON_HEIGHT : 0), &screen, &box, COLOR_NONE, 0);
    audio_play_sample(SOUND_SWITCH, volumes);
    wait_frames(BUTTON_FRAMES);
}

/* After ENTER the doors slide apart, 2 pixels each 2 frames, over the dungeon view
   in the entrance palette. The door sound plays at the start and every 40 frames.
   ponytail: the original draws a made-up corridor; this draws the view of the start square. */
void entrance_open_doors(void)
{
    enum { DOOR_STEP = 2, STEP_FRAMES = 2, SOUND_FRAMES = 40 };
    static const uint8_t volumes[2] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    static uint8_t view_px[SCREEN_W * SCREEN_H];
    char path[PATH_SIZE];
    Box box;
    if (snprintf(path, sizeof path, "%s/Dungeon.DAT", data_dir) >= (int)sizeof path || !g_frame_hook ||
        !dungeon_load(path) || !ui_zone_box(ZONE_VIEWPORT, &box))
        return;
    inventory_set_dungeon_view_palette(g_party_map_index, 0);
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    Bitmap view = {box.w, box.h, view_px};
    for (int16_t y = 0; y < box.h; y++)
        memcpy(view_px + y * box.w, &fb[box.y + y][box.x], (size_t)box.w);
    set_entrance_palette();
    for (int16_t opened = 0, frames = 0; draw_entrance_doors(&view, opened) && !g_quit_requested; opened += DOOR_STEP) {
        if (frames % SOUND_FRAMES == 0)
            audio_play_sample(SOUND_ENTRANCE_DOOR, volumes);
        for (int16_t f = 0; f < STEP_FRAMES && wait_frame(); f++, frames++)
            ;
    }
}

/* Sets the darkness of the dungeon view and the creature rules from the party light. */
static void set_party_light(void)
{
    inventory_set_dungeon_view_palette(g_party_map_index, inventory_get_party_light());
}

/* The dungeon view of the party in the light of the party. */
void gameloop_draw_view(void)
{
    set_party_light();
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    ui_set_palette(NULL);
}

/* The automap remembers the squares of the current view. */
static void mark_seen_squares(void)
{
    automap_mark_seen(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
}

/* Redraws what commands and rules changed: flagged champion states, hand and inventory slots,
   the name of the leader hand object, the message area. */
static void refresh_interface(void)
{
    Bitmap screen = ui_get_screen();
    ui_draw_all_champion_states(0);
    menus_refresh_spell_area(NULL);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        if (!g_champions[i].current_health)
            continue;
        bool inventory = i + 1 == g_inventory_champion_ordinal && !g_game_frozen; /* the frozen screen covers it */
        for (uint16_t slot = 0; slot < (inventory ? SLOT_COUNT : SLOT_HAND_ACTION + 1); slot++)
            ui_draw_champion_slot(i, slot);
        if (inventory)
            ui_draw_viewport();
    }
    int16_t icon = g_leader_hand_object == THING_NONE ? -1 : object_get_icon_index(g_leader_hand_object);
    if (g_leader_hand_object != shown_hand_object || icon != shown_hand_icon) {
        shown_hand_object = g_leader_hand_object;
        shown_hand_icon = icon;
        if (shown_hand_object == THING_NONE)
            ui_fill_zone(&screen, ZONE_LEADER_HAND_OBJECT_NAME, COLOR_BLACK);
        else
            ui_draw_leader_hand_object_name(shown_hand_object);
    }
    ui_draw_message_area();
}

/* The start of a game, the part for both a new and a loaded game: interface state and input. */
static void start_game(void)
{
    game_reset_session();
    g_do_not_draw_fluxcages = false;
    g_automap_open = false;
    music_select(MUSIC_ORIGINAL);
    automap_scroll_reset();
    shown_hand_object = THING_NONE;
    command_reset();
    inventory_reset();
    group_reset();
    menus_reset();
    sensor_reset();
    move_reset();
    projectile_reset();
    message_clear();
    view_load_map(g_party_map_index);
}

/* The interface appears and the game time runs. */
static void show_game(void)
{
    g_game_screen_inactive = false;
    memset(fb, 0, sizeof fb);
    ui_set_palette(NULL);
    ui_draw_interface();
    gameloop_draw_view();
    g_game_time_ticking = true;
    frames_waiting = 0;
    command_discard_all_input();
}

bool gameloop_start_new_game(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/Dungeon.DAT", dir);
    if (!dungeon_load(path))
        return false;
    game_start_new();
    savegame_forget();
    champion_reset_data_to_start_game(); /* a new game has no hand object to put back */
    start_game();
    explored_reset();
    move_party_enter_dungeon();
    show_game();
    mark_seen_squares();
    return true;
}

/* champion_reset_data_to_start_game for a loaded game: the hand object, the leader with its load,
   and the magic caster come back; all champion states are drawn. */
static void reset_champions_of_loaded_game(void)
{
    int16_t leader = g_leader_index;
    inventory_put_object_in_leader_hand(champion_reset_data_to_start_game());
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        g_champions[i].attributes &= CHAMPION_ATTR_KEEP_MASK;
    ui_draw_all_champion_states(CHAMPION_ATTR_ICON | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ACTION_HAND);
    if (leader != -1)
        champion_set_leader(leader);
    menus_draw_spell_area();
}

/* start_game for a loaded game; the "GAME LOADED" dialog shows when confirm is 1. */
static void start_loaded_game(int confirm)
{
    start_game();
    map_reset();
    group_remove_all_active_groups();
    map_set_current_map_and_party_map(g_party_map_index);
    group_add_all_active_groups();
    if (confirm) {
        g_game_screen_inactive = true;
        memset(fb, 0, sizeof fb);
        dialog_draw(NULL, gfx_string(TEXT_GAME_LOADED), gfx_string(TEXT_OK), NULL, NULL, NULL);
        dialog_get_choice(1);
    }
    show_game();
    reset_champions_of_loaded_game();
    refresh_interface();
    mark_seen_squares();
}

void gameloop_start_loaded_game(void)
{
    start_loaded_game(1);
}
void gameloop_continue_loaded_game(void)
{
    start_loaded_game(0);
}

/* The fuse action and sensor 18 ask for a sequence that runs outside the commands and events. */
static void run_requested_sequence(void)
{
    if (projectile_take_fuse_request())
        startend_run_fuse_sequence();
}

/* Advanced feature: between two ticks the view shows the projectiles part of the way to their new
   place. The view draws fields and explosions with the game random numbers: the seed stays. */
static void draw_projectile_motion(void)
{
    if (!g_advanced_features || g_game_screen_inactive || g_game_frozen || g_party_is_sleeping ||
        g_inventory_champion_ordinal || !view_has_moving_projectiles())
        return;
    uint32_t seed = random_get_seed();
    view_set_motion(frames_waiting, g_wait_for_input_max_frames + 1);
    gameloop_draw_view();
    view_set_motion(0, 0);
    random_set_seed(seed);
}

int gameloop_run_frame(void)
{
    int ran = 0;
    if (g_automap_open)
        return 0; /* the game time stops while the automap is open */
    while ((!g_stop_waiting_for_input || !g_game_time_ticking) && command_process_queue())
        ran = 1;
    if (ran)
        refresh_interface();
    run_requested_sequence();
    if (++frames_waiting > g_wait_for_input_max_frames)
        g_stop_waiting_for_input = true;
    bool tick = g_stop_waiting_for_input && g_game_time_ticking;
    if (!tick)
        draw_projectile_motion();
    return tick;
}

/* Runs all waiting commands and draws what they changed, as gameloop_run_frame does (control socket). */
void gameloop_run_commands(void)
{
    int ran = 0;
    while (command_process_queue())
        ran = 1;
    if (ran)
        refresh_interface();
}

/* The game time advances one tick: torches burn, freeze life and movement delays count down, and
   the champions get their time effects. */
static void advance_game_time(void)
{
    game_advance_time();
    menus_refresh_action_area();
    game_apply_tick_effects();
}

/* The champions that died in the tick let go of the inventory, the presses, and their icons.
   leader is the leader before the deaths. */
static void release_dead_champions(int16_t leader)
{
    uint16_t dead = champion_take_dead_champions();
    for (uint16_t i = 0; i < CHAMPION_MAX_COUNT; i++)
        if (dead & 1 << i)
            command_release_dead_champion(i, leader);
}

/* A party that died in one tick: the spell area as the death before the last one drew it, while the
   last champion, now the caster, still lived with its runes (before holds the champions before the damage). */
static void draw_spell_area_before_last_death(const Champion *before)
{
    int16_t caster = g_magic_caster_champion_index;
    if (caster != -1)
        menus_refresh_spell_area(&before[caster]);
}

/* One tick of the game. A dead party ends in the endgame. */
void gameloop_tick(void)
{
    frames_waiting = 0;
    g_stop_waiting_for_input = false;
    do {
        if (g_new_party_map_index != -1) {
            move_process_new_party_map();
            command_discard_all_input();
        }
        set_party_light(); /* the creature rules see the light also while the view is not drawn */
        event_process_expired();
    } while (g_new_party_map_index != -1);
    music_update();
    if (!g_party_is_sleeping) {
        command_wake_up();
        if (!g_candidate_champion_ordinal)
            command_unfreeze_game();
        if (!g_inventory_champion_ordinal)
            gameloop_draw_view();
        else if (g_adventure == ADVENTURE_CSB)
            panel_redraw_scroll();
    }
    mark_seen_squares();
    command_highlight_disable();
    sound_play_pending();
    int16_t leader = g_leader_index;
    Champion before[CHAMPION_MAX_COUNT];
    memcpy(before, g_champions, sizeof before);
    champion_apply_and_draw_pending_damage_and_wounds();
    release_dead_champions(leader);
    if (g_party_dead) {
        ui_draw_all_champion_states(0); /* the dead champions show during the death scream */
        draw_spell_area_before_last_death(before);
        startend_run_endgame(1);
        return;
    }
    advance_game_time();
    message_clear_expired_rows(g_game_time);
    refresh_interface();
    run_requested_sequence();
}

/* Reads the 4 bit plane bitmap of the arrow or the hand pointer into one color per pixel. */
static void decode_planar_pointer(bool hand, uint8_t *px, int *w, int *h)
{
    int16_t pw, ph;
    const uint8_t *planes = tables_get_pointer_planes(hand, &pw, &ph);
    *w = (uint16_t)pw; /* the size words are unsigned */
    *h = (uint16_t)ph;
    int row_bytes = (*w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD * 2;
    int plane = row_bytes * *h;
    for (int y = 0; y < *h; y++)
        for (int x = 0; x < *w; x++) {
            uint8_t c = 0;
            for (int p = 0; p < BITPLANE_COUNT; p++) {
                uint8_t byte = planes[p * plane + y * row_bytes + x / CHAR_BIT];
                c |= (uint8_t)((byte >> (CHAR_BIT - 1 - x % CHAR_BIT) & 1) << p);
            }
            px[y * *w + x] = c;
        }
}

/* The object icon of the leader hand with a black shadow at the shadow offset (build_mouse_pointer_bitmap). */
static void make_object_pointer(uint8_t *px, int *width, int *height)
{
    uint8_t icon_px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = {UI_ICON_SIZE, UI_ICON_SIZE, icon_px};
    int16_t dx = tables_get_pointer_shadow_dx();
    int16_t dy = tables_get_pointer_shadow_dy();
    const uint8_t *shadows = tables_get_palette_changes(TABLES_CHANGES_POINTER_SHADOW);
    ui_extract_icon(object_get_icon_index(g_leader_hand_object), &icon);
    *width = UI_ICON_SIZE + dx;
    *height = UI_ICON_SIZE + dy;
    memset(px, POINTER_SHADOW_COLOR, (size_t)*width * *height);
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++) {
            uint8_t shadow = shadows[icon_px[y * UI_ICON_SIZE + x]];
            if (shadow != POINTER_SHADOW_COLOR)
                px[(y + dy) * *width + x + dx] = shadow;
        }
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++)
            if (icon_px[y * UI_ICON_SIZE + x] != POINTER_SHADOW_COLOR)
                px[y * *width + x] = icon_px[y * UI_ICON_SIZE + x];
    for (int i = 0; i < *width * *height; i++) /* the transparent color becomes 0 and the shadow black */
        px[i] = px[i] == POINTER_SHADOW_COLOR ? 0 : px[i] == 0 ? POINTER_SHADOW_COLOR : px[i];
}

/* The pointer shape of a screen position: the hand or the object over the zones that use it
   (viewport, hands of the status boxes) when there is a leader, else the arrow. */
static int get_pointer_shape(int16_t x, int16_t y, bool in_game)
{
    if (!in_game || g_leader_index == -1)
        return POINTER_ARROW;
    for (int i = 0; i < POINTER_ZONE_COUNT; i++) {
        PointerZone z = tables_get_pointer_zone(i);
        if (x < z.x || x >= z.x + z.w || y < z.y || y >= z.y + z.h)
            continue;
        int16_t champion = z.champion;
        if (!champion && !z.shape)
            return POINTER_ARROW;
        /* a status box: the arrow over a living champion with its inventory open, or over its name */
        int16_t index = (champion & ZONE_CHAMPION_INDEX_MASK) - 1;
        if (champion && index < g_party_champion_count && g_champions[index].current_health &&
            (index + 1 == g_inventory_champion_ordinal || champion >= ZONE_CHAMPION_NAME))
            return POINTER_ARROW;
        return g_leader_empty_handed ? POINTER_HAND : POINTER_OBJECT;
    }
    return POINTER_ARROW;
}

bool gameloop_get_pointer(int16_t x, int16_t y, bool in_game, uint8_t *px, int *w, int *h, int *hot_x, int *hot_y)
{
    if (in_game && g_mouse_pointer_hidden)
        return false;
    int shape = get_pointer_shape(x, y, in_game);
    if (shape == POINTER_OBJECT)
        make_object_pointer(px, w, h);
    else
        decode_planar_pointer(shape == POINTER_HAND, px, w, h);
    *hot_x = tables_get_pointer_hotspot_x(shape);
    *hot_y = tables_get_pointer_hotspot_y(shape);
    return true;
}

const uint16_t *gameloop_get_pointer_palette(void)
{
    static uint16_t colors[PALETTE_COLORS];
    tables_get_palette(TABLES_PALETTE_POINTER, colors);
    return colors;
}
