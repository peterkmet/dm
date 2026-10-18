/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gameloop.h"
#include "adventure.h"
#include "file.h"
#include "graphics.h"
#include "ids.h"
#include "settings.h"
#include "tables.h"
#include "vdeo.h"
#include "champion.h"
#include "dungeon.h"
#include "event.h"
#include "explored.h"
#include "game.h"
#include "group.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "party.h"
#include "projectile.h"
#include "random.h"
#include "record.h"
#include "savegame.h"
#include "sensor.h"
#include "sound.h"
#include "advanced.h"
#include "audio.h"
#include "automap.h"
#include "command.h"
#include "dialog.h"
#include "layout.h"
#include "menus.h"
#include "music.h"
#include "optmenu.h"
#include "panel.h"
#include "startend.h"
#include "status.h"
#include "text.h"
#include "ui.h"
#include "video.h"
#include "view.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Mouse pointer zones and the pointer shadow. */
#define POINTER_ZONE_COUNT   16
#define POINTER_SHADOW_COLOR 12 /* black: the shadow, swapped with the transparent color 0 */

/* PointerZone.champion: champion index + 1 in bits 0-2; 8 and more is the name zone of the champion */
#define ZONE_CHAMPION_INDEX_MASK 0x07
#define ZONE_CHAMPION_NAME       0x08

/* Mouse pointer shapes. */
#define POINTER_ARROW  0
#define POINTER_HAND   1
#define POINTER_OBJECT 2

bool g_stop_waiting_for_input;
bool g_game_time_ticking;
int16_t g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
bool g_game_frozen;
bool g_game_screen_shown;

static int16_t frames_waiting;
static uint16_t shown_hand_object = THING_NONE;
static int16_t shown_hand_icon; /* a waterskin or a torch changes its icon and its name */
static char data_dir[PATH_SIZE];

bool gameloop_load_data(const char *directory)
{
    const char *failed;
    snprintf(data_dir, sizeof data_dir, "%s", directory);
    if (!adventure_load_files(directory, &failed))
        return false;
    startend_set_save_dir(directory);
    char path[PATH_SIZE];
    if (snprintf(path, sizeof path, "%s/%s", directory, VDEO_FILE) < (int)sizeof path)
        vdeo_load(path); /* without the video driver the palette changes without a fade */
    g_endgame_sensor_hook = startend_run_endgame_sensor;
    return layout_init() && text_init() && ui_init();
}

const char *gameloop_get_data_dir(void)
{
    return data_dir;
}

/* Sets the darkness of the dungeon view and the creature rules from the party light. */
static void set_party_light(void)
{
    party_set_light_level(g_party_map_index, party_get_light());
}

void gameloop_draw_view(void)
{
    set_party_light();
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    ui_set_palette(NULL);
}

/* The automap remembers the squares of the current view. */
static void mark_seen_squares(void)
{
    explored_mark_visible((Position){g_party_map_index, g_party_map_x, g_party_map_y}, g_party_direction);
}

/* Redraws what commands and rules changed: flagged champion states, hand and inventory slots,
   the name of the leader hand object, the message area. */
static void refresh_interface(void)
{
    Bitmap screen = video_get_screen();
    status_draw_all(0);
    menus_refresh_spell_area(NULL);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        if (!g_champions[i].current_health)
            continue;
        bool inventory = i == g_session.inventory_champion_index && !g_game_frozen; /* the frozen screen covers it */
        for (uint16_t slot = 0; slot < (inventory ? SLOT_COUNT : SLOT_HAND_ACTION + 1); slot++)
            status_draw_slot(i, slot);
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
            ui_draw_hand_object_name(shown_hand_object);
    }
    ui_draw_message_area();
}

/* The volumes, the music and the filter of a game start: the settings file in the advanced mode, else full
   volumes, no music and no filter; --filter keeps its filter. */
static void apply_settings(void)
{
    Settings settings = {SETTINGS_VOLUME_FULL, SETTINGS_VOLUME_FULL, SETTINGS_MUSIC_ORIGINAL, SETTINGS_FILTER_NONE};
    if (g_session.mode == GAME_MODE_ADVANCED)
        settings = settings_load(startend_save_dir());
    g_audio_volume = settings.sound_volume;
    g_music_volume = settings.music_volume;
    music_select(settings.music);
    if (!g_filter_fixed)
        optmenu_set_filter(settings.filter);
}

/* The start of a game, the part for both a new and a loaded game: interface state and input. */
static void start_game(void)
{
    game_reset_session();
    g_do_not_draw_fluxcages = false;
    g_advanced_screen = ADVANCED_SCREEN_NONE;
    apply_settings();
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
    g_game_screen_shown = true;
    memset(g_screen.px, 0, sizeof g_screen.px);
    ui_set_palette(NULL);
    ui_draw_interface();
    gameloop_draw_view();
    g_game_time_ticking = true;
    frames_waiting = 0;
    command_discard_all_input();
}

bool gameloop_start_new_game(const char *directory)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/Dungeon.DAT", directory);
    if (!dungeon_load(path))
        return false;
    game_start_new();
    savegame_forget();
    champion_reset_for_new_game(); /* a new game has no hand object to put back */
    start_game();
    explored_reset();
    move_party_enter_dungeon();
    show_game();
    mark_seen_squares();
    return true;
}

/* champion_reset_for_new_game for a loaded game: the hand object, the leader with its load,
   and the magic caster come back; all champion states are drawn. */
static void reset_loaded_champions(void)
{
    int16_t leader = g_leader_index;
    inventory_put_hand_object(champion_reset_for_new_game());
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        g_champions[i].attributes &= CHAMPION_ATTR_KEEP_MASK;
    status_draw_all(CHAMPION_ATTR_ICON | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ACTION_HAND);
    if (leader != -1)
        champion_set_leader(leader);
    menus_draw_spell_area();
}

/* The start of a loaded game; the "GAME LOADED" dialog shows when confirm is true. */
static void start_loaded_game(bool confirm)
{
    start_game();
    map_reset();
    group_remove_all_active_groups();
    map_set_party_map(g_party_map_index);
    group_add_all_active_groups();
    if (confirm) {
        g_game_screen_shown = false;
        memset(g_screen.px, 0, sizeof g_screen.px);
        dialog_draw(&(DialogText){
            {NULL, gfx_get_string(TEXT_GAME_LOADED)},
            {gfx_get_string(TEXT_OK)}
        });
        dialog_get_choice(1);
    }
    show_game();
    reset_loaded_champions();
    refresh_interface();
    mark_seen_squares();
}

void gameloop_start_loaded_game(void)
{
    start_loaded_game(true);
}
void gameloop_continue_loaded_game(void)
{
    start_loaded_game(false);
}

/* The fuse action and the endgame sensor ask for a sequence that runs outside the commands and events. */
static void run_requested_sequence(void)
{
    if (projectile_take_fuse_request())
        startend_run_fuse_sequence();
}

/* Advanced feature: between two ticks the view shows the projectiles part of the way to their new
   place. The view draws fields and explosions with the game random numbers: the seed stays. */
static void show_projectile_motion(void)
{
    if (!g_advanced_features || !g_game_screen_shown || g_game_frozen || g_session.party_sleeping ||
        g_session.inventory_champion_index >= 0 || !view_has_moving_projectiles())
        return;
    uint32_t seed = random_get_seed();
    view_set_motion(frames_waiting, g_wait_for_input_max_frames + 1);
    gameloop_draw_view();
    view_set_motion(0, 0);
    random_set_seed(seed);
}

bool gameloop_run_frame(void)
{
    bool ran = false;
    if (g_advanced_screen != ADVANCED_SCREEN_NONE)
        return false; /* the game time stops while a viewport screen is open */
    while ((!g_stop_waiting_for_input || !g_game_time_ticking) && command_process_queue())
        ran = true;
    if (ran)
        refresh_interface();
    run_requested_sequence();
    if (++frames_waiting > g_wait_for_input_max_frames)
        g_stop_waiting_for_input = true;
    bool tick = g_stop_waiting_for_input && g_game_time_ticking;
    if (!tick)
        show_projectile_motion();
    return tick;
}

void gameloop_run_commands(void)
{
    bool ran = false;
    while (command_process_queue())
        ran = true;
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
    for (int16_t i = 0; i < CHAMPION_MAX_COUNT; i++)
        if (dead & 1 << i)
            command_release_dead_champion(i, leader);
}

/* A party that died in one tick: the spell area as the death before the last one drew it, while the
   last champion, now the caster, still lived with its runes (before holds the champions before the damage). */
static void show_death_spell_area(const Champion *before)
{
    int16_t caster = g_magic_caster_champion_index;
    if (caster != -1)
        menus_refresh_spell_area(&before[caster]);
}

/* The advanced CSB start: the party takes stairs down in the prison, and no candidate waits at a mirror. */
static bool is_prison_stairs_down(void)
{
    return g_session.mode == GAME_MODE_ADVANCED && g_adventure == ADVENTURE_CSB && g_dungeon_id == PRISON_DUNGEON_ID &&
           g_session.new_party_map != -1 &&
           rec_map_level(g_session.new_party_map) == rec_map_level(g_party_map_index) + LEVEL_DOWN &&
           g_session.candidate_champion_index < 0;
}

void gameloop_tick(void)
{
    frames_waiting = 0;
    g_stop_waiting_for_input = false;
    do {
        if (is_prison_stairs_down() && startend_take_prison_stairs())
            return; /* the CSB dungeon runs from the next tick */
        if (g_session.new_party_map != -1) {
            move_process_new_party_map();
            command_discard_all_input();
        }
        set_party_light(); /* the creature rules see the light also while the view is not drawn */
        event_process_expired();
    } while (g_session.new_party_map != -1);
    music_update();
    if (!g_session.party_sleeping) {
        command_wake_up();
        if (g_session.candidate_champion_index < 0 && !champion_is_renaming())
            command_unfreeze_game();
        if (g_session.inventory_champion_index < 0)
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
    champion_apply_pending_damage();
    for (int16_t i = 0; i < g_party_champion_count; i++) {
        int16_t damage = champion_take_damage_to_show(i);
        if (damage)
            status_draw_damage(i, damage);
    }
    release_dead_champions(leader);
    if (g_session.party_dead) {
        status_draw_all(0); /* the dead champions show during the death scream */
        show_death_spell_area(before);
        startend_run_endgame(true);
        return;
    }
    advance_game_time();
    message_clear_expired_rows();
    refresh_interface();
    run_requested_sequence();
}

/* Reads the 4 bit plane bitmap of the arrow or the hand pointer into one color per pixel. */
static void decode_planar_pointer(bool hand, PointerImage *image)
{
    PointerBitmap bitmap = tables_get_pointer_bitmap(hand);
    const uint8_t *planes = bitmap.planes;
    image->w = (uint16_t)bitmap.width; /* the size words are unsigned */
    image->h = (uint16_t)bitmap.height;
    int row_bytes = (image->w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD * 2;
    int plane = row_bytes * image->h;
    for (int y = 0; y < image->h; y++)
        for (int x = 0; x < image->w; x++) {
            uint8_t c = 0;
            for (int p = 0; p < BITPLANE_COUNT; p++) {
                uint8_t byte = planes[p * plane + y * row_bytes + x / CHAR_BIT];
                c |= (uint8_t)((byte >> (CHAR_BIT - 1 - x % CHAR_BIT) & 1) << p);
            }
            image->px[y * image->w + x] = c;
        }
}

/* The object icon of the leader hand with a black shadow at the shadow offset. */
static void make_object_pointer(PointerImage *image)
{
    uint8_t *px = image->px;
    uint8_t icon_px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = {UI_ICON_SIZE, UI_ICON_SIZE, icon_px};
    int16_t dx = tables_get_pointer_shadow_dx();
    int16_t dy = tables_get_pointer_shadow_dy();
    const uint8_t *shadows = tables_get_palette_changes(TABLES_CHANGES_POINTER_SHADOW);
    ui_extract_icon(object_get_icon_index(g_leader_hand_object), &icon);
    image->w = UI_ICON_SIZE + dx;
    image->h = UI_ICON_SIZE + dy;
    memset(px, POINTER_SHADOW_COLOR, (size_t)image->w * image->h);
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++) {
            uint8_t shadow = shadows[icon_px[y * UI_ICON_SIZE + x]];
            if (shadow != POINTER_SHADOW_COLOR)
                px[(y + dy) * image->w + x + dx] = shadow;
        }
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++)
            if (icon_px[y * UI_ICON_SIZE + x] != POINTER_SHADOW_COLOR)
                px[y * image->w + x] = icon_px[y * UI_ICON_SIZE + x];
    for (int i = 0; i < image->w * image->h; i++) /* the transparent color becomes 0 and the shadow black */
        px[i] = px[i] == POINTER_SHADOW_COLOR ? 0 : px[i] == 0 ? POINTER_SHADOW_COLOR : px[i];
}

/* The pointer shape of a screen position: the hand or the object over the zones that use it
   (viewport, hands of the status boxes) when there is a leader, else the arrow; the name entry shows the arrow. */
static int get_pointer_shape(int16_t x, int16_t y, bool in_game)
{
    if (!in_game || g_leader_index == -1 || champion_is_renaming())
        return POINTER_ARROW;
    for (int i = 0; i < POINTER_ZONE_COUNT; i++) {
        PointerZone z = tables_get_pointer_zone(i);
        if (!box_contains(&(Box){z.x, z.y, z.w, z.h}, x, y))
            continue;
        int16_t champion = z.champion;
        if (!champion && !z.shape)
            return POINTER_ARROW;
        /* a status box: the arrow over a living champion with its inventory open, or over its name */
        int16_t index = (champion & ZONE_CHAMPION_INDEX_MASK) - 1;
        if (champion && index < g_party_champion_count && g_champions[index].current_health &&
            (index == g_session.inventory_champion_index || champion >= ZONE_CHAMPION_NAME))
            return POINTER_ARROW;
        return g_leader_empty_handed ? POINTER_HAND : POINTER_OBJECT;
    }
    return POINTER_ARROW;
}

bool gameloop_get_pointer(int16_t x, int16_t y, bool in_game, PointerImage *image)
{
    if (in_game && g_mouse_pointer_hidden)
        return false;
    int shape = get_pointer_shape(x, y, in_game);
    if (shape == POINTER_OBJECT)
        make_object_pointer(image);
    else
        decode_planar_pointer(shape == POINTER_HAND, image);
    image->hot_x = tables_get_pointer_hotspot_x(shape);
    image->hot_y = tables_get_pointer_hotspot_y(shape);
    return true;
}

const uint16_t *gameloop_get_pointer_palette(void)
{
    static uint16_t colors[PALETTE_COLORS];
    tables_get_palette(TABLES_PALETTE_POINTER, colors);
    return colors;
}
