#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "gameloop.h"
#include "advanced.h"
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
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "layout.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "sound.h"
#include "tables.h"
#include "text.h"
#include "timeline_process.h"
#include "ui.h"
#include "video.h"
#include "view.h"

/* KAOS data addresses. */
#define ENTRANCE_PALETTE 0x100a66     /* g_entrance_palette[16] */
#define SPRITE_PALETTE 0x100190       /* pointer colors (the second half of g_initial_colormap) */
#define ARROW_POINTER 0x1002b0        /* g_arrow_pointer_bitmap: 4 bit planes, size in the 4 bytes before */
#define HAND_POINTER 0x1003d4         /* g_hand_pointer_bitmap */
#define POINTER_ZONES 0x1004f8        /* g_mouse_pointer_zones[16]: x, y, width, height, shape, champion */
#define POINTER_HOTSPOTS 0x1005c8     /* g_mouse_pointer_hotspots[4]: x, y, width, height */
#define POINTER_SHADOW_OBJECT 0x1005fe /* g_pointer_shadow_colors_object[16] */
#define POINTER_SHADOW_OFFSET 0x102952 /* g_pointer_shadow_dx, g_pointer_shadow_dy */

#define TORCH_BURN_INTERVAL_MASK 511  /* torches lose one charge every 512 ticks */
#define TIME_EFFECTS_MASK 63          /* champion time effects every 64 ticks */
#define TIME_EFFECTS_SLEEPING_MASK 15 /* every 16 ticks while the party sleeps */
#define LIGHT_SOURCE_COUNT (CHAMPION_MAX_COUNT * 2)
#define LIGHT_SOURCES_USED 5          /* the brightest torches that add light */
#define LIGHT_FIRST_SHIFT 6
#define POINTER_ZONE_COUNT 16
#define POINTER_ZONE_BYTES 12
#define POINTER_HOTSPOT_BYTES 8
#define POINTER_SHADOW_COLOR 12       /* black: the shadow, swapped with the transparent color 0 */
#define BITPLANE_COUNT 4
#define PIXELS_PER_WORD 16
#define PATH_SIZE 1024
#define POINTER_WIDTH_BEFORE 4        /* the bitmap width word comes 4 bytes before the planes */
#define POINTER_HEIGHT_BEFORE 2

/* Byte offsets of the words of a pointer zone and of a pointer hotspot. */
enum { POINTER_ZONE_X = 0, POINTER_ZONE_Y = 2, POINTER_ZONE_W = 4, POINTER_ZONE_H = 6, POINTER_ZONE_SHAPE = 8 };
enum { HOTSPOT_X = 0, HOTSPOT_Y = 2 };

enum { POINTER_ARROW, POINTER_HAND, POINTER_OBJECT };

static int16_t frames_waiting;
static uint16_t shown_hand_object = THING_NONE;

int gameloop_load_data(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/KAOS.FTL", dir);
    if (kaos_load(path)) return 0;
    snprintf(path, sizeof path, "%s/Graphics.DAT", dir);
    startend_set_save_dir(dir);
    return gfx_open(path) >= 0 && !layout_init() && !text_init() && !tables_init() && !ui_init();
}

/* The entrance graphic with both doors closed, in the entrance palette on the whole screen.
   ponytail: no dungeon behind the doors, no door animation, no credits. */
void entrance_draw(void)
{
    Bitmap screen = ui_screen();
    Box doors, work, door;
    memset(fb, 0, sizeof fb);
    for (int i = 0; i < PALETTE_COLORS; i++) palette[i] = kaos_u16(ENTRANCE_PALETTE + 2 * i);
    palette_split_top = 0;
    palette_split_bottom = SCREEN_H;
    ui_blit_graphic(&screen, GFX_ENTRANCE, ZONE_SCREEN, COLOR_NONE);
    if (!ui_zone_box(ZONE_ENTRANCE_DOORS, &doors)) return;
    int16_t w = 0, h = 0;
    if (!layout_get_zone(ZONE_ENTRANCE_WORK, doors.w, doors.h, &work, &w, &h)) return;
    for (int16_t side = 0; side < 2; side++) {
        const Bitmap *bm = gfx_bitmap(GFX_ENTRANCE_LEFT_DOOR + side);
        w = h = 0;
        if (!bm || !layout_get_zone((int16_t)(ZONE_ENTRANCE_LEFT_DOOR + side), bm->w, bm->h, &door, &w, &h)) continue;
        door.x += work.x;
        door.y += work.y;
        draw_blit(bm, w, h, &screen, &door, COLOR_NONE, 0);
    }
}

/* The party light: magical light plus the burning torches in the hands, the brightest first
   with a falling weight (set_dungeon_view_palette). */
static int16_t party_light_amount(void)
{
    int16_t power[LIGHT_SOURCE_COUNT] = { 0 }, n = 0, total = 0, shift = LIGHT_FIRST_SHIFT;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        for (int hand = SLOT_HAND_ACTION; hand >= SLOT_HAND_READY; hand--, n++) {
            uint16_t thing = g_champions[i].slots[hand];
            int16_t type = object_get_type(thing);
            if (type >= ICON_TORCH_UNLIT && type <= ICON_TORCH_LAST) power[n] = (int16_t)weapon_charges(thing);
        }
    for (int i = 0; i < LIGHT_SOURCE_COUNT; i++)
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
            if (icon < ICON_TORCH_UNLIT || icon > ICON_TORCH_LAST || !weapon_charges(thing)) continue;
            weapon_set_charges(thing, weapon_charges(thing) - 1);
            if (!weapon_charges(thing)) object_clear_important(thing);
        }
}

/* The dungeon view of the party in the light of the party. */
void gameloop_draw_view(void)
{
    int16_t light = party_light_amount();
    view_set_light(light);
    view_render(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
    ui_set_palette(NULL);
    advanced_draw_debug(light);
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
    champion_draw_all_states(0);
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        if (!g_champions[i].current_health) continue;
        int inventory = i + 1 == g_inventory_champion_ordinal && !g_game_frozen;  /* the frozen screen covers it */
        for (uint16_t slot = 0; slot < (inventory ? SLOT_COUNT : SLOT_HAND_ACTION + 1); slot++) champion_draw_slot(i, slot);
        if (inventory) ui_draw_viewport();
    }
    if (g_leader_hand_object != shown_hand_object) {
        shown_hand_object = g_leader_hand_object;
        if (shown_hand_object == THING_NONE) ui_fill_zone(&screen, ZONE_LEADER_HAND_OBJECT_NAME, COLOR_BLACK);
        else draw_leader_hand_object_name(shown_hand_object);
    }
    ui_draw_message_area();
}

/* start_game, the part for both a new and a loaded game: interface state and input. */
static void start_game(void)
{
    g_candidate_champion_ordinal = 0;
    g_party_is_sleeping = 0;
    g_game_won = g_do_not_draw_fluxcages = 0;
    g_automap_open = 0;
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

int gameloop_new_game(const char *dir)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/Dungeon.DAT", dir);
    if (!load_dungeon(path)) return 0;
    game_new();
    g_saved_game_exists = 0;
    g_leader_empty_handed = 1;
    start_game();
    automap_reset();
    move_party_enter_dungeon();
    show_game();
    mark_seen_squares();
    return 1;
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
    champion_put_object_in_leader_hand(hand);
    for (uint16_t i = 0; i < g_party_champion_count; i++) g_champions[i].attributes &= CHAMPION_ATTR_KEEP_MASK;
    champion_draw_all_states(CHAMPION_ATTR_ICON | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ACTION_HAND);
    if (leader != -1) command_set_leader(leader);
    g_magic_caster_champion_index = -1;
    if (caster != -1) menus_set_magic_caster(caster);
}

void gameloop_start_loaded_game(void)
{
    start_game();
    map_reset();
    g_new_party_map_index = -1;
    group_remove_all_active_groups();
    set_current_map_and_party_map(g_party_map_index);
    group_add_all_active_groups();
    g_game_screen_inactive = 1;
    memset(fb, 0, sizeof fb);
    dialog_draw(NULL, ui_string(TEXT_GAME_LOADED), ui_string(TEXT_OK), NULL, NULL, NULL);
    dialog_get_choice(1);
    show_game();
    reset_champions_of_loaded_game();
    refresh_interface();
    mark_seen_squares();
}

/* The fuse action and sensor 18 ask for a sequence that runs outside the commands and events. */
static void run_requested_sequence(void)
{
    if (!g_fuse_sequence_requested) return;
    g_fuse_sequence_requested = 0;
    fuse_sequence();
}

/* Advanced feature: between two ticks the view shows the projectiles part of the way to their new
   place. The view draws fields and explosions with the game random numbers: the seed stays. */
static void draw_projectile_motion(void)
{
    if (!g_advanced_features || g_game_screen_inactive || g_game_frozen || g_party_is_sleeping || g_inventory_champion_ordinal ||
        !view_projectiles_moving())
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
    if (g_automap_open) return 0; /* the game time stops while the automap is open */
    while ((!g_stop_waiting_for_input || !g_game_time_ticking) && command_process_queue()) ran = 1;
    if (ran) refresh_interface();
    run_requested_sequence();
    if (++frames_waiting > g_wait_for_input_max_frames) g_stop_waiting_for_input = 1;
    int tick = g_stop_waiting_for_input && g_game_time_ticking;
    if (!tick) draw_projectile_motion();
    return tick;
}

/* The game time advances one tick: torches burn, freeze life and movement delays count down, and
   the champions get their time effects. */
static void advance_game_time(void)
{
    g_game_time++;
    if (!(g_game_time & TORCH_BURN_INTERVAL_MASK)) decrease_torches_light_power();
    if (g_party.freeze_life_ticks) g_party.freeze_life_ticks--;
    menus_refresh_action_area();
    if (!(g_game_time & (g_party_is_sleeping ? TIME_EFFECTS_SLEEPING_MASK : TIME_EFFECTS_MASK))) champion_apply_time_effects();
    if (g_disabled_movement_ticks) g_disabled_movement_ticks--;
    if (g_projectile_disabled_movement_ticks) g_projectile_disabled_movement_ticks--;
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
        timeline_process();
    } while (g_new_party_map_index != -1);
    if (!g_party_is_sleeping) {
        command_wake_up();
        if (!g_candidate_champion_ordinal) game_unfreeze();
        if (!g_inventory_champion_ordinal) gameloop_draw_view();
    }
    mark_seen_squares();
    command_highlight_disable();
    sound_play_pending();
    champion_apply_and_draw_pending_damage_and_wounds();
    if (g_party_dead) {
        endgame(1);
        return;
    }
    advance_game_time();
    message_clear_expired_rows(g_game_time);
    refresh_interface();
    run_requested_sequence();
}

/* Reads a 4 bit plane bitmap of the KAOS data into one color per pixel. */
static void planar_pointer(uint32_t address, uint8_t *px, int *w, int *h)
{
    *w = kaos_u16(address - POINTER_WIDTH_BEFORE);
    *h = kaos_u16(address - POINTER_HEIGHT_BEFORE);
    int row_bytes = (*w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD * 2, plane = row_bytes * *h;
    for (int y = 0; y < *h; y++)
        for (int x = 0; x < *w; x++) {
            uint8_t c = 0;
            for (int p = 0; p < BITPLANE_COUNT; p++) {
                uint8_t byte = kaos_u8(address + p * plane + y * row_bytes + x / CHAR_BIT);
                c |= (uint8_t)((byte >> (CHAR_BIT - 1 - x % CHAR_BIT) & 1) << p);
            }
            px[y * *w + x] = c;
        }
}

/* The object icon of the leader hand with a black shadow at the shadow offset (build_mouse_pointer_bitmap). */
static void object_pointer(uint8_t *px, int *w, int *h)
{
    uint8_t icon_px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap icon = { UI_ICON_SIZE, UI_ICON_SIZE, icon_px };
    int16_t dx = kaos_s16(POINTER_SHADOW_OFFSET), dy = kaos_s16(POINTER_SHADOW_OFFSET + 2);
    ui_extract_icon(object_get_icon_index(g_leader_hand_object), &icon);
    *w = UI_ICON_SIZE + dx;
    *h = UI_ICON_SIZE + dy;
    memset(px, POINTER_SHADOW_COLOR, (size_t)*w * *h);
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++) {
            uint8_t shadow = kaos_u8(POINTER_SHADOW_OBJECT + icon_px[y * UI_ICON_SIZE + x]);
            if (shadow != POINTER_SHADOW_COLOR) px[(y + dy) * *w + x + dx] = shadow;
        }
    for (int y = 0; y < UI_ICON_SIZE; y++)
        for (int x = 0; x < UI_ICON_SIZE; x++)
            if (icon_px[y * UI_ICON_SIZE + x] != POINTER_SHADOW_COLOR) px[y * *w + x] = icon_px[y * UI_ICON_SIZE + x];
    for (int i = 0; i < *w * *h; i++) /* the transparent color becomes 0 and the shadow black */
        px[i] = px[i] == POINTER_SHADOW_COLOR ? 0 : px[i] == 0 ? POINTER_SHADOW_COLOR : px[i];
}

/* The pointer shape of a screen position: the hand or the object over the zones that use it
   (viewport, hands of the status boxes) when there is a leader, else the arrow. */
static int pointer_shape(int16_t x, int16_t y, int in_game)
{
    if (!in_game || g_leader_index == -1) return POINTER_ARROW;
    for (int i = 0; i < POINTER_ZONE_COUNT; i++) {
        uint32_t z = POINTER_ZONES + POINTER_ZONE_BYTES * i;
        int16_t zx = kaos_s16(z + POINTER_ZONE_X), zy = kaos_s16(z + POINTER_ZONE_Y);
        int16_t zw = kaos_s16(z + POINTER_ZONE_W), zh = kaos_s16(z + POINTER_ZONE_H);
        if (x < zx || x >= zx + zw || y < zy || y >= zy + zh) continue;
        if (!kaos_s16(z + POINTER_ZONE_SHAPE)) return POINTER_ARROW;
        return g_leader_empty_handed ? POINTER_HAND : POINTER_OBJECT;
    }
    return POINTER_ARROW;
}

int gameloop_pointer(int16_t x, int16_t y, int in_game, uint8_t *px, int *w, int *h, int *hot_x, int *hot_y)
{
    if (in_game && g_mouse_pointer_hidden) return 0;
    int shape = pointer_shape(x, y, in_game);
    if (shape == POINTER_OBJECT) object_pointer(px, w, h);
    else planar_pointer(shape == POINTER_HAND ? HAND_POINTER : ARROW_POINTER, px, w, h);
    *hot_x = kaos_s16(POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + HOTSPOT_X);
    *hot_y = kaos_s16(POINTER_HOTSPOTS + POINTER_HOTSPOT_BYTES * shape + HOTSPOT_Y);
    return 1;
}

const uint16_t *gameloop_pointer_palette(void)
{
    static uint16_t colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++) colors[i] = kaos_u16(SPRITE_PALETTE + 2 * i);
    return colors;
}
