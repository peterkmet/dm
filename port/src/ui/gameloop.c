#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "gameloop.h"
#include "advanced.h"
#include "adventure.h"
#include "automap.h"
#include "champion.h"
#include "command.h"
#include "dialog.h"
#include "group.h"
#include "projectile.h"
#include "random.h"
#include "savegame.h"
#include "sensor.h"
#include "startend.h"
#include "dungeon.h"
#include "explored.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "music.h"
#include "sound.h"
#include "tables.h"
#include "text.h"
#include "timeline_process.h"
#include "ui.h"
#include "video.h"
#include "view.h"

#define TORCH_BURN_INTERVAL_MASK   0x1ff /* torches lose one charge every 512 ticks */
#define TIME_EFFECTS_MASK          0x3f  /* champion time effects every 64 ticks */
#define TIME_EFFECTS_SLEEPING_MASK 0xf   /* every 16 ticks while the party sleeps */
#define LIGHT_SOURCE_COUNT         (CHAMPION_MAX_COUNT * 2)
#define LIGHT_SOURCES_USED         5 /* the brightest torches that add light */
#define LIGHT_FIRST_SHIFT          6
#define POINTER_ZONE_COUNT         16
#define POINTER_SHADOW_COLOR       12 /* black: the shadow, swapped with the transparent color 0 */

/* PointerZone.champion: champion index + 1 in bits 0-2; 8 and more is the name zone of the champion */
enum { ZONE_CHAMPION_INDEX_MASK = 7, ZONE_CHAMPION_NAME = 8 };

enum { POINTER_ARROW, POINTER_HAND, POINTER_OBJECT };

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
    g_endgame_sensor_hook = endgame_sensor;
    return gfx_open(path) >= 0 && !layout_init() && !text_init() && !ui_init();
}

const char *gameloop_data_dir(void)
{
    return data_dir;
}

/* The entrance graphic, the view behind the doors (or nothing for NULL), and each door moved apart by
   opened pixels. Returns 1 while a part of a door shows. */
static int draw_entrance_doors(const Bitmap *view, int16_t opened)
{
    Bitmap screen = ui_screen();
    Box doors, work, door;
    int16_t w = 0, h = 0, shown = 0;
    ui_blit_graphic(&screen, GFX_ENTRANCE, ZONE_SCREEN, COLOR_NONE);
    if (!ui_zone_box(ZONE_ENTRANCE_DOORS, &doors))
        return 0;
    if (!layout_get_zone(ZONE_ENTRANCE_WORK, doors.w, doors.h, &work, &w, &h))
        return 0;
    w = h = 0;
    if (view && layout_get_zone(ZONE_ENTRANCE_VIEWPORT, view->w, view->h, &door, &w, &h)) {
        door.x += work.x;
        door.y += work.y;
        draw_blit(view, w, h, &screen, &door, COLOR_NONE, 0);
    }
    for (int16_t side = 0; side < 2; side++) {
        const Bitmap *bm = gfx_bitmap(GFX_ENTRANCE_LEFT_DOOR + side);
        w = h = 0;
        if (!bm || !layout_get_zone(ZONE_ENTRANCE_LEFT_DOOR + side, bm->w, bm->h, &door, &w, &h))
            continue;
        door.x += work.x;
        door.y += work.y;
        door.w = max16(0, door.w - opened);
        if (side == 0)
            w += opened; /* the left door slides left, the right door slides right */
        else
            door.x += opened;
        if (door.w)
            shown = 1;
        draw_blit(bm, w, h, &screen, &door, COLOR_NONE, 0);
    }
    return shown;
}

static void set_entrance_palette(void)
{
    tables_palette(TABLES_PALETTE_ENTRANCE, palette);
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

/* process_entrance: ENTER or RESUME shows its pressed button, plays the switch sound, and waits 20 frames. */
void entrance_press_button(int resume)
{
    enum { BUTTON_HEIGHT = 13, BUTTON_FRAMES = 20 };
    static const uint8_t volumes[2] = {SOUND_MAX_VOLUME, SOUND_MAX_VOLUME};
    const Bitmap *bm = gfx_bitmap(GFX_ENTRANCE_BUTTONS);
    Bitmap screen = ui_screen();
    Box box;
    int16_t sx = 0, sy = 0;
    if (!g_frame_hook)
        return;
    if (bm &&
        layout_get_zone(resume ? ZONE_ENTRANCE_RESUME : ZONE_ENTRANCE_ENTER, bm->w, BUTTON_HEIGHT, &box, &sx, &sy))
        draw_blit(bm, sx, sy + (resume ? BUTTON_HEIGHT : 0), &screen, &box, COLOR_NONE, 0);
    sound_play_sample(SOUND_SWITCH, volumes);
    wait_frames(BUTTON_FRAMES);
}

/* entrance_open_doors: after ENTER the doors slide apart, 2 pixels each 2 frames, over the dungeon view
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
        !load_dungeon(path) || !ui_zone_box(ZONE_VIEWPORT, &box))
        return;
    view_set_light(g_party_map_index, 0);
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    Bitmap view = {box.w, box.h, view_px};
    for (int16_t y = 0; y < box.h; y++)
        memcpy(view_px + y * box.w, &fb[box.y + y][box.x], (size_t)box.w);
    set_entrance_palette();
    for (int16_t opened = 0, frames = 0; draw_entrance_doors(&view, opened) && !g_quit_requested; opened += DOOR_STEP) {
        if (frames % SOUND_FRAMES == 0)
            sound_play_sample(SOUND_ENTRANCE_DOOR, volumes);
        for (int16_t f = 0; f < STEP_FRAMES && wait_frame(); f++, frames++)
            ;
    }
}

/* The party light: magical light plus the burning torches in the hands, the brightest first
   with a falling weight (set_dungeon_view_palette). */
int16_t gameloop_party_light(void)
{
    int16_t power[LIGHT_SOURCE_COUNT] = {0}, n = 0, total = 0, shift = LIGHT_FIRST_SHIFT;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        for (int hand = SLOT_HAND_ACTION; hand >= SLOT_HAND_READY; hand--, n++) {
            uint16_t thing = g_champions[i].slots[hand];
            int16_t type = object_get_type(thing);
            if (type >= ICON_TORCH_UNLIT && type <= ICON_TORCH_LAST)
                power[n] = weapon_charges(thing);
        }
    /* the original sorts only the 4 brightest: the fifth light is any of the rest */
    for (int i = 0; i < LIGHT_SOURCES_USED - 1; i++)
        for (int k = i + 1; k < LIGHT_SOURCE_COUNT; k++)
            if (power[i] < power[k]) {
                int16_t t = power[i];
                power[i] = power[k];
                power[k] = t;
            }
    for (int i = 0; i < LIGHT_SOURCES_USED; i++)
        if (power[i]) {
            /* the 16-bit cast keeps the overflow of the original word arithmetic */
            total += (int16_t)(*tables_light_power_to_light_amount(power[i]) << shift) >> LIGHT_FIRST_SHIFT;
            shift = max16(0, shift - 1);
        }
    return g_party.magical_light_amount + total;
}

/* Burning torches in the hands lose one charge; a burnt out torch may be discarded later. */
static void decrease_torches_light_power(void)
{
    uint16_t count = g_party_champion_count - (g_candidate_champion_ordinal ? 1 : 0);
    for (uint16_t i = 0; i < count; i++)
        for (int hand = SLOT_HAND_ACTION; hand >= SLOT_HAND_READY; hand--) {
            uint16_t thing = g_champions[i].slots[hand];
            int16_t icon = object_get_icon_index(thing);
            if (icon < ICON_TORCH_UNLIT || icon > ICON_TORCH_LAST || !weapon_charges(thing))
                continue;
            weapon_set_charges(thing, weapon_charges(thing) - 1);
            if (!weapon_charges(thing))
                rec_object_clear_important(thing);
        }
}

int16_t gameloop_set_light(void)
{
    int16_t light = gameloop_party_light();
    view_set_light(g_party_map_index, light);
    return light;
}

/* The dungeon view of the party in the light of the party. */
void gameloop_draw_view(void)
{
    gameloop_set_light();
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
    Bitmap screen = ui_screen();
    ui_draw_all_champion_states(0);
    menus_refresh_spell_area();
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

/* start_game, the part for both a new and a loaded game: interface state and input. */
static void start_game(void)
{
    g_candidate_champion_ordinal = 0;
    g_party_is_sleeping = 0;
    g_game_won = g_do_not_draw_fluxcages = 0;
    g_automap_open = g_cheat_light = g_cheat_immortal = g_cheat_food = 0;
    music_select(MUSIC_ORIGINAL);
    automap_scroll_reset();
    memset(g_champion_skill_level_up, 0, sizeof g_champion_skill_level_up);
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
    g_game_screen_inactive = 0;
    memset(fb, 0, sizeof fb);
    ui_set_palette(NULL);
    ui_draw_interface();
    gameloop_draw_view();
    g_game_time_ticking = 1;
    frames_waiting = 0;
    command_discard_all_input();
}

bool gameloop_new_game(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/Dungeon.DAT", dir);
    if (!load_dungeon(path))
        return false;
    game_new();
    g_saved_game_exists = 0;
    g_leader_empty_handed = 1;
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
    int16_t leader = g_leader_index, caster = g_magic_caster_champion_index;
    uint16_t hand = g_leader_hand_object;
    g_leader_empty_handed = 1;
    g_leader_hand_object = THING_NONE;
    g_leader_index = -1;
    inventory_put_object_in_leader_hand(hand);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        g_champions[i].attributes &= CHAMPION_ATTR_KEEP_MASK;
    ui_draw_all_champion_states(CHAMPION_ATTR_ICON | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ACTION_HAND);
    if (leader != -1)
        champion_set_leader(leader);
    g_magic_caster_champion_index = -1;
    if (caster != -1)
        menus_set_magic_caster(caster);
}

/* start_game for a loaded game; the "GAME LOADED" dialog shows when confirm is 1. */
static void start_loaded_game(int confirm)
{
    start_game();
    map_reset();
    g_new_party_map_index = -1;
    group_remove_all_active_groups();
    set_current_map_and_party_map(g_party_map_index);
    group_add_all_active_groups();
    if (confirm) {
        g_game_screen_inactive = 1;
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
    if (!g_fuse_sequence_requested)
        return;
    g_fuse_sequence_requested = 0;
    fuse_sequence();
}

/* Advanced feature: between two ticks the view shows the projectiles part of the way to their new
   place. The view draws fields and explosions with the game random numbers: the seed stays. */
static void draw_projectile_motion(void)
{
    if (!g_advanced_features || g_game_screen_inactive || g_game_frozen || g_party_is_sleeping ||
        g_inventory_champion_ordinal || !view_projectiles_moving())
        return;
    uint32_t seed = g_random_seed;
    view_set_motion(frames_waiting, g_wait_for_input_max_frames + 1);
    gameloop_draw_view();
    view_set_motion(0, 0);
    g_random_seed = seed;
}

int gameloop_frame(void)
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
        g_stop_waiting_for_input = 1;
    bool tick = g_stop_waiting_for_input && g_game_time_ticking;
    if (!tick)
        draw_projectile_motion();
    return tick;
}

/* Runs all waiting commands and draws what they changed, as gameloop_frame does (control socket). */
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
    g_game_time++;
    if (!(g_game_time & TORCH_BURN_INTERVAL_MASK))
        decrease_torches_light_power();
    if (g_party.freeze_life_ticks)
        g_party.freeze_life_ticks--;
    menus_refresh_action_area();
    if (!(g_game_time & (g_party_is_sleeping ? TIME_EFFECTS_SLEEPING_MASK : TIME_EFFECTS_MASK)))
        champion_apply_time_effects();
    if (g_disabled_movement_ticks)
        g_disabled_movement_ticks--;
    if (g_projectile_disabled_movement_ticks)
        g_projectile_disabled_movement_ticks--;
}

/* The champions that died in the tick let go of the inventory, the presses, and their icons.
   leader is the leader before the deaths. */
static void release_dead_champions(int16_t leader)
{
    for (uint16_t i = 0; i < CHAMPION_MAX_COUNT; i++)
        if (g_dead_champions & 1 << i)
            command_release_dead_champion(i, leader);
    g_dead_champions = 0;
}

/* A party that died in one tick: the spell area as the death before the last one drew it, while the
   last champion, now the caster, still lived with its runes (before holds the champions before the damage). */
static void draw_spell_area_before_last_death(const Champion *before)
{
    int16_t caster = g_magic_caster_champion_index;
    if (caster == -1)
        return;
    Champion dead = g_champions[caster];
    g_champions[caster] = before[caster];
    menus_refresh_spell_area();
    g_champions[caster] = dead;
}

/* game_loop: one tick of the game. A dead party ends in the endgame. */
void gameloop_tick(void)
{
    frames_waiting = 0;
    g_stop_waiting_for_input = 0;
    do {
        if (g_new_party_map_index != -1) {
            move_process_new_party_map();
            command_discard_all_input();
        }
        gameloop_set_light(); /* the creature rules see the light also while the view is not drawn */
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
        endgame(1);
        return;
    }
    advance_game_time();
    message_clear_expired_rows(g_game_time);
    refresh_interface();
    run_requested_sequence();
}

/* Reads the 4 bit plane bitmap of the arrow or the hand pointer into one color per pixel. */
static void planar_pointer(bool hand, uint8_t *px, int *w, int *h)
{
    int16_t pw, ph;
    const uint8_t *planes = tables_pointer_planes(hand, &pw, &ph);
    *w = (uint16_t)pw; /* the size words are unsigned */
    *h = (uint16_t)ph;
    int row_bytes = (*w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD * 2, plane = row_bytes * *h;
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
static void object_pointer(uint8_t *px, int *w, int *h)
{
    uint8_t icon_px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = {UI_ICON_SIZE, UI_ICON_SIZE, icon_px};
    int16_t dx = tables_pointer_shadow_dx(), dy = tables_pointer_shadow_dy();
    const uint8_t *shadows = tables_palette_changes(TABLES_CHANGES_POINTER_SHADOW);
    ui_extract_icon(object_get_icon_index(g_leader_hand_object), &icon);
    *w = UI_ICON_SIZE + dx;
    *h = UI_ICON_SIZE + dy;
    memset(px, POINTER_SHADOW_COLOR, (size_t)*w * *h);
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++) {
            uint8_t shadow = shadows[icon_px[y * UI_ICON_SIZE + x]];
            if (shadow != POINTER_SHADOW_COLOR)
                px[(y + dy) * *w + x + dx] = shadow;
        }
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++)
            if (icon_px[y * UI_ICON_SIZE + x] != POINTER_SHADOW_COLOR)
                px[y * *w + x] = icon_px[y * UI_ICON_SIZE + x];
    for (int i = 0; i < *w * *h; i++) /* the transparent color becomes 0 and the shadow black */
        px[i] = px[i] == POINTER_SHADOW_COLOR ? 0 : px[i] == 0 ? POINTER_SHADOW_COLOR : px[i];
}

/* The pointer shape of a screen position: the hand or the object over the zones that use it
   (viewport, hands of the status boxes) when there is a leader, else the arrow. */
static int pointer_shape(int16_t x, int16_t y, bool in_game)
{
    if (!in_game || g_leader_index == -1)
        return POINTER_ARROW;
    for (int i = 0; i < POINTER_ZONE_COUNT; i++) {
        PointerZone z = tables_pointer_zone(i);
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

bool gameloop_pointer(int16_t x, int16_t y, bool in_game, uint8_t *px, int *w, int *h, int *hot_x, int *hot_y)
{
    if (in_game && g_mouse_pointer_hidden)
        return false;
    int shape = pointer_shape(x, y, in_game);
    if (shape == POINTER_OBJECT)
        object_pointer(px, w, h);
    else
        planar_pointer(shape == POINTER_HAND, px, w, h);
    *hot_x = tables_pointer_hotspot_x(shape);
    *hot_y = tables_pointer_hotspot_y(shape);
    return true;
}

const uint16_t *gameloop_pointer_palette(void)
{
    static uint16_t colors[PALETTE_COLORS];
    tables_palette(TABLES_PALETTE_POINTER, colors);
    return colors;
}
