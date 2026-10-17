/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/command.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/magic.h"
#include "engine/map.h"
#include "engine/move.h"
#include "engine/party.h"
#include "engine/record.h"
#include "engine/sensor.h"
#include "engine/sound.h"
#include "ui/advanced.h"
#include "ui/dialog.h"
#include "ui/entrance.h"
#include "ui/frame.h"
#include "ui/gameloop.h"
#include "ui/menus.h"
#include "ui/panel.h"
#include "ui/startend.h"
#include "ui/status.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include <stdbool.h>
#include <string.h>

/* The command queue, the input tables and the mouth blink. */
#define QUEUE_SIZE          9
#define QUEUE_CLICK_LIMIT   5 /* a click or a key waits only when fewer commands wait */
#define QUEUE_RELEASE_LIMIT 7
#define NO_TABLE            (-1)
#define MOUTH_BLINK_COUNT   4 /* the mouth blinks when it eats an object up */
#define MOUTH_BLINK_FRAMES  8

/* View cells 0-1 are on the party square, 2-3 ahead. */
#define VIEW_CELL_FRONT_FIRST 2

typedef struct {
    int16_t x, y, command;
} QueuedCommand;

bool g_mouse_pointer_hidden;
bool g_input_received;
uint16_t g_last_key;

static QueuedCommand queue[QUEUE_SIZE];
static int16_t queue_first, queue_count;
static int16_t primary_mouse = NO_TABLE;
static int16_t secondary_mouse = NO_TABLE;
static int16_t primary_keys = TABLES_KEYS_NONE;
static int16_t secondary_keys = TABLES_KEYS_NONE;
typedef struct {
    int16_t primary_mouse, secondary_mouse;
    int16_t primary_keys, secondary_keys;
} InputTables;

static InputTables frozen_input = {.primary_keys = TABLES_KEYS_NONE, .secondary_keys = TABLES_KEYS_NONE};
static InputTables dialog_input = {.primary_keys = TABLES_KEYS_NONE, .secondary_keys = TABLES_KEYS_NONE};
static bool left_button_down, pressing_eye, pressing_mouth, pressing_fake_wall;
static int16_t dragged_icon_cell = -1; /* the champion icon cell picked for a move, or -1 */
static Box highlight;
static bool highlight_enabled;

void command_set_entrance_input(void)
{
    primary_mouse = TABLES_MOUSE_ENTRANCE;
    secondary_mouse = NO_TABLE;
    primary_keys = TABLES_KEYS_ENTRANCE;
    secondary_keys = TABLES_KEYS_NONE;
}

/* The input tables of the game screen. */
static void set_game_input(void)
{
    primary_mouse = TABLES_MOUSE_INTERFACE;
    secondary_mouse = TABLES_MOUSE_MOVEMENT;
    primary_keys = TABLES_KEYS_INTERFACE;
    secondary_keys = TABLES_KEYS_MOVEMENT;
}

void command_reset(void)
{
    left_button_down = false;
    pressing_eye = false;
    pressing_mouth = false;
    pressing_fake_wall = false;
    dragged_icon_cell = -1;
    highlight_enabled = false;
    g_mouse_pointer_hidden = false;
    g_stop_waiting_for_input = false;
    g_game_frozen = false;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    queue_first = 0;
    queue_count = 0;
    g_input_received = false;
    g_last_key = 0;
    set_game_input();
}

static InputTables get_current_input(void)
{
    return (InputTables){primary_mouse, secondary_mouse, primary_keys, secondary_keys};
}

/* Makes the tables t the active input tables. */
static void set_input(InputTables t)
{
    primary_mouse = t.primary_mouse;
    secondary_mouse = t.secondary_mouse;
    primary_keys = t.primary_keys;
    secondary_keys = t.secondary_keys;
}

void command_set_only_mouse_input(int16_t table)
{
    set_input((InputTables){table, NO_TABLE, TABLES_KEYS_NONE, TABLES_KEYS_NONE});
}

void command_save_input(void)
{
    dialog_input = get_current_input();
}
void command_restore_input(void)
{
    set_input(dialog_input);
}

static void queue_put(int16_t command, int16_t x, int16_t y)
{
    queue[(queue_first + queue_count) % QUEUE_SIZE] = (QueuedCommand){x, y, command};
    queue_count++;
}

void command_process_click(int16_t x, int16_t y, int16_t kind)
{
    if (kind == CLICK_LEFT_PRESS || kind == CLICK_RIGHT_PRESS)
        g_input_received = true;
    if (kind == CLICK_LEFT_PRESS)
        left_button_down = true;
    if (kind == CLICK_LEFT_RELEASE)
        left_button_down = false;
    if (advanced_process_click(x, y, kind))
        return;
    if (queue_count >= (kind == CLICK_LEFT_RELEASE ? QUEUE_RELEASE_LIMIT : QUEUE_CLICK_LIMIT))
        return;
    int16_t command = COMMAND_LEFT_RELEASE;
    if (kind != CLICK_LEFT_RELEASE) {
        command = ui_command_from_mouse(primary_mouse, x, y, kind);
        if (!command)
            command = ui_command_from_mouse(secondary_mouse, x, y, kind);
    }
    if (command)
        queue_put(command, x, y);
}

void command_process_key(uint16_t key)
{
    if (advanced_process_key(key))
        return;
    g_input_received = true;
    g_last_key = key;
    if (queue_count >= QUEUE_CLICK_LIMIT)
        return;
    int16_t command = tables_get_key_command(primary_keys, key);
    if (!command)
        command = tables_get_key_command(secondary_keys, key);
    if (g_advanced_features && ((key == KEY_ESCAPE && command == COMMAND_FREEZE_GAME) || key == KEY_TAB) &&
        g_session.inventory_champion_index >= 0 && g_session.candidate_champion_index < 0)
        command = COMMAND_CLOSE_INVENTORY;
    if (command)
        queue_put(command, 0, 0);
}

void command_discard_all_input(void)
{
    QueuedCommand kept[QUEUE_SIZE];
    int16_t n = 0;
    g_input_received = false;
    g_last_key = 0;
    for (int16_t i = 0; i < queue_count; i++) {
        QueuedCommand c = queue[(queue_first + i) % QUEUE_SIZE];
        if (c.command == COMMAND_LEFT_RELEASE || c.command == COMMAND_RIGHT_RELEASE)
            kept[n++] = c;
    }
    queue_first = 0;
    queue_count = n;
    memcpy(queue, kept, sizeof kept[0] * n);
}

/* Inverts a zone on the screen until the next tick. */
static void highlight_zone(int16_t zone)
{
    command_highlight_disable();
    if (!ui_zone_box(zone, &highlight))
        return;
    ui_xor_screen_zone(zone);
    highlight_enabled = true;
}

void command_highlight_disable(void)
{
    if (!highlight_enabled)
        return;
    ui_xor_screen_box(&highlight);
    highlight_enabled = false;
}

/* The square in front of the party. */
static void get_front_square(int16_t *x, int16_t *y)
{
    *x = g_party_map_x;
    *y = g_party_map_y;
    map_step(g_party_direction, 1, 0, x, y);
}

/* Shows a text in the empty viewport: the sleep and freeze screens. */
static void show_viewport_message(int16_t text)
{
    Bitmap *viewport = ui_get_viewport();
    memset(viewport->px, COLOR_BLACK, (size_t)viewport->w * viewport->h);
    ui_print_zone(viewport, ZONE_VIEWPORT_MESSAGE, COLOR_CYAN, COLOR_BLACK, gfx_get_string(text));
    ui_draw_viewport();
}

void command_freeze_game(void)
{
    Bitmap screen = video_get_screen();
    if (g_game_frozen)
        return;
    g_game_frozen = true;
    command_highlight_disable();
    if (g_session.inventory_champion_index < 0)
        ui_fill_zone_pattern(&screen, ZONE_MOVEMENT_ARROWS, COLOR_BLACK);
    else
        inventory_close_chest();
    ui_fill_zone_pattern(&screen, ZONE_SPELL_AREA, COLOR_BLACK);
    ui_fill_zone_pattern(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
}

void command_unfreeze_game(void)
{
    if (!g_game_frozen)
        return;
    if (g_session.party_sleeping) {
        show_viewport_message(TEXT_WAKE_UP);
        return;
    }
    g_game_frozen = false;
    menus_draw_spell_area();
    if (g_session.acting_champion_index < 0)
        g_action_area_contains_icons = true;
    menus_draw_action_area();
    if (g_session.inventory_champion_index < 0) {
        menus_draw_movement_arrows();
    } else {
        int16_t open = g_session.inventory_champion_index;
        inventory_set_open_champion(-1);
        command_toggle_inventory(open);
    }
}

/* Closes the open inventory panel; the movement input returns unless the party sleeps. */
static void close_open_inventory(int16_t open)
{
    panel_close(open);
    if (g_session.party_sleeping)
        return;
    secondary_mouse = TABLES_MOUSE_MOVEMENT;
    secondary_keys = TABLES_KEYS_MOVEMENT;
    command_discard_all_input();
}

void command_close_inventory(void)
{
    int16_t open = g_session.inventory_champion_index;
    if (pressing_fake_wall || pressing_eye || pressing_mouth)
        return;
    g_stop_waiting_for_input = true;
    if (open >= 0)
        close_open_inventory(open);
}

void command_toggle_inventory(int16_t champion_index)
{
    int16_t open = g_session.inventory_champion_index;
    if (pressing_fake_wall || !g_champions[champion_index].current_health || pressing_eye || pressing_mouth)
        return;
    g_stop_waiting_for_input = true;
    if (open == champion_index) {
        close_open_inventory(open);
        return;
    }
    if (open >= 0) {
        panel_close(open);
        if (g_session.party_sleeping)
            return;
    }
    panel_open(champion_index);
    secondary_mouse = TABLES_MOUSE_INVENTORY;
    secondary_keys = TABLES_KEYS_NONE;
    command_discard_all_input();
}

void command_click_on_mouth(void)
{
    int16_t result = inventory_process_click_on_mouth();
    if (result == MOUTH_REFUSED)
        return;
    uint16_t champion_index = g_session.inventory_champion_index;
    if (result == MOUTH_USED_UP)
        for (int16_t blink = MOUTH_BLINK_COUNT; blink > 0; blink--) {
            ui_draw_icon_to_viewport(ICON_MOUTH + !(blink & 1), ZONE_MOUTH);
            ui_draw_viewport();
            frame_wait_count(MOUTH_BLINK_FRAMES);
        }
    sound_request_play(SOUND_SWALLOW, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_NOW, 0);
    if (g_panel_content == PANEL_CONTENT_FOOD_WATER)
        g_champions[champion_index].attributes |= CHAMPION_ATTR_PANEL;
    status_draw_champion(champion_index);
}

void command_release_dead_champion(int16_t champion_index, int16_t leader_index)
{
    Bitmap screen = video_get_screen();
    int16_t icon_cell = NORMALIZE(g_champions[champion_index].cell - g_party_direction);
    if (champion_index == g_session.inventory_champion_index) {
        pressing_eye = false;
        pressing_mouth = false;
        g_mouse_pointer_hidden = false;
        command_close_inventory();
    }
    if (champion_index == leader_index && pressing_fake_wall) {
        pressing_fake_wall = false;
        g_mouse_pointer_hidden = false;
    }
    if (icon_cell == dragged_icon_cell)
        dragged_icon_cell = -1;
    ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + icon_cell, COLOR_BLACK);
}

/* The candidate inventory shows the resurrect and reincarnate panel; the game is frozen meanwhile. */
static void show_candidate(void)
{
    int16_t index = g_session.candidate_champion_index;
    inventory_set_open_champion(-1);
    command_toggle_inventory(index);
    command_freeze_game();
}

/* Click in the resurrect/reincarnate panel of the candidate: accept or cancel it. */
static void click_in_candidate_panel(int16_t x, int16_t y)
{
    Bitmap screen = video_get_screen();
    int16_t command = ui_command_from_mouse(TABLES_MOUSE_RESURRECT, x, y, CLICK_LEFT_PRESS);
    int16_t index = g_party_champion_count - 1;
    if (!command || !g_leader_empty_handed)
        return;
    uint8_t cell = g_champions[index].cell;
    champion_click_rebirth_panel(command);
    inventory_set_open_champion(index); /* the rule clears it; the panel closes here */
    command_close_inventory();
    if (command == PANEL_CANCEL) {
        ui_fill_zone(&screen, ZONE_STATUS_BOX_FIRST + index, COLOR_BLACK);
        ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + NORMALIZE(cell - g_party_direction), COLOR_BLACK);
        if (!g_party_champion_count)
            menus_set_magic_caster(-1);
    } else {
        menus_draw_spell_area();
        status_draw_all(CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_NAME_TITLE | CHAMPION_ATTR_STATISTICS |
                        CHAMPION_ATTR_ICON | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS);
    }
    menus_end_damage_hold();
    g_redraw |= REDRAW_ACTION_AREA;
}

/* Left click in a champion status box: the name sets the leader, a hand is a slot box. */
static void click_in_status_box(int16_t champion_index, int16_t x, int16_t y)
{
    if (champion_index == g_session.inventory_champion_index) {
        champion_set_leader(champion_index);
        return;
    }
    int16_t command = ui_command_from_mouse(TABLES_MOUSE_STATUS_BOX, x, y, CLICK_LEFT_PRESS);
    if (command >= COMMAND_SET_LEADER_FIRST && command < COMMAND_SET_LEADER_FIRST + CHAMPION_MAX_COUNT)
        champion_set_leader(command - COMMAND_SET_LEADER_FIRST);
    else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST &&
             command < COMMAND_CLICK_SLOT_BOX_FIRST + UI_SLOT_BOX_INVENTORY_FIRST)
        inventory_click_on_slot_box(command - COMMAND_CLICK_SLOT_BOX_FIRST);
}

/* First click on a champion icon picks the champion; the second click moves it to the clicked
   cell and swaps it with the champion there. */
static void click_on_champion_icon(int16_t cell)
{
    Bitmap screen = video_get_screen();
    if (dragged_icon_cell < 0) {
        if (champion_get_index_in_cell(NORMALIZE(cell + g_party_direction)) == -1)
            return;
        dragged_icon_cell = cell;
        return;
    }
    int16_t from = dragged_icon_cell;
    dragged_icon_cell = -1;
    int16_t moved = champion_get_index_in_cell(NORMALIZE(from + g_party_direction));
    if (moved == -1)
        return;
    int16_t other = champion_move_to_cell(moved, NORMALIZE(cell + g_party_direction));
    if (cell != from) {
        if (other < 0)
            ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + from, COLOR_BLACK);
        else
            status_draw_champion(other);
    }
    status_draw_champion(moved);
}

/* The leader takes the top object of a pile in view cell 0-3 or the alcove. A creature in a
   front cell protects the objects under it. */
static void grab_object(int16_t pile)
{
    Location at = {g_party_map_x, g_party_map_y};
    if (g_leader_index == -1)
        return;
    if (pile >= VIEW_CELL_FRONT_FIRST)
        get_front_square(&at.x, &at.y);
    if (inventory_grab_object(view_get_result()->pile_top_objects[pile], NORMALIZE(pile + g_party_direction), at))
        g_stop_waiting_for_input = true;
}

/* The leader puts the hand object on the floor of view cell 0-3 or into the alcove ahead. Bones
   on a vi altar start the rebirth of their champion. */
static void drop_object(int16_t pile)
{
    Location at = {g_party_map_x, g_party_map_y};
    bool alcove = pile == VIEW_CLICK_BOX_ALCOVE;
    if (g_leader_index == -1)
        return;
    if (alcove)
        pile = VIEW_CELL_FRONT_FIRST;
    if (pile >= VIEW_CELL_FRONT_FIRST)
        get_front_square(&at.x, &at.y);
    inventory_drop_object(NORMALIZE(pile + g_party_direction), at, alcove && view_get_result()->facing_vi_altar);
    g_stop_waiting_for_input = true;
}

/* The leader hand touches the wall ahead: wall sensors and mirrors. */
static void touch_front_wall(void)
{
    int16_t x, y;
    get_front_square(&x, &y);
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height)
        return;
    int16_t candidate = g_session.candidate_champion_index;
    /* the wall side that faces the party */
    g_stop_waiting_for_input = sensor_is_wall_triggered(x, y, direction_back(g_party_direction));
    if (candidate < 0 && g_session.candidate_champion_index >= 0)
        show_candidate();
}

/* An empty hand on the square ahead: a thud, or the hand goes into an imaginary wall while the
   button is down. */
static void touch_front_square(bool imaginary_wall, int16_t x, int16_t y)
{
    if (!g_party_champion_count)
        return;
    if (!imaginary_wall)
        sound_request_play(SOUND_WOODEN_THUD, (Location){x, y}, SOUND_MODE_NOW, 0);
    else if (left_button_down) {
        pressing_fake_wall = true;
        g_mouse_pointer_hidden = true;
    }
    g_stop_waiting_for_input = true;
}

/* A point in a clickable box of the view; x -1 marks an unused box. */
static bool is_in_box(const Box *b, int16_t x, int16_t y)
{
    return b->x != -1 && box_contains(b, x, y);
}

/* The leader throws the hand object when the click is in the left or right throw zone. */
static bool is_hand_object_thrown(int16_t x, int16_t y)
{
    int16_t zone =
        view_get_result()->square_ahead_element == VIEW_ELEMENT_DOOR_FRONT ? ZONE_THROW_DOOR_LEFT : ZONE_THROW_LEFT;
    bool thrown = false;
    if (ui_is_point_in_zone(zone, x, y))
        thrown = action_is_hand_object_thrown(0);
    else if (ui_is_point_in_zone(zone + 1, x, y))
        thrown = action_is_hand_object_thrown(1);
    if (thrown)
        g_stop_waiting_for_input = true;
    return thrown;
}

/* An empty-handed click in the dungeon view: grab an object, drink at a fountain, touch the wall, or touch the
   square ahead. */
static void click_empty_handed(int16_t x, int16_t y, int16_t fx, int16_t fy, bool imaginary_wall)
{
    const ViewResult *view = view_get_result();
    for (int16_t i = 0; i < VIEW_CLICK_BOX_COUNT; i++) {
        if (!is_in_box(&view->clickable_boxes[i], x, y))
            continue;
        if (i != VIEW_CLICK_BOX_WALL) {
            grab_object(i);
            return;
        }
        /* With the advanced features, every living champion drinks. */
        if (view->facing_fountain && g_leader_index != -1)
            champion_drink_from_fountain(g_advanced_features);
        if (!view->facing_alcove)
            touch_front_wall();
        return;
    }
    if (view->square_ahead_element != VIEW_ELEMENT_WALL)
        return;
    if (ui_is_point_in_zone(ZONE_THROW_LEFT, x, y) || ui_is_point_in_zone(ZONE_THROW_LEFT + 1, x, y))
        touch_front_square(imaginary_wall, fx, fy);
}

/* Click in the dungeon view (x, y in viewport coordinates): door buttons, objects, the front wall. */
static void click_in_dungeon_view(int16_t x, int16_t y)
{
    int16_t fx, fy;
    const ViewResult *view = view_get_result();
    const Box *wall_box = &view->clickable_boxes[VIEW_CLICK_BOX_WALL];
    get_front_square(&fx, &fy);
    if (view->square_ahead_element == VIEW_ELEMENT_DOOR_FRONT) {
        if (g_leader_index == -1)
            return;
        if (!g_leader_empty_handed && is_hand_object_thrown(x, y))
            return;
        if (g_leader_empty_handed && rec_door_has_button(map_get_square_first_thing(fx, fy)) &&
            is_in_box(wall_box, x, y)) {
            sensor_press_door_button(fx, fy);
            g_stop_waiting_for_input = true;
            return;
        }
    }
    bool imaginary_wall = map_is_imaginary_wall((Location){fx, fy});
    if (g_leader_empty_handed) {
        click_empty_handed(x, y, fx, fy, imaginary_wall);
        return;
    }
    if ((view->square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall) && is_hand_object_thrown(x, y))
        return;
    int16_t cells =
        view->square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall ? CELL_COUNT : VIEW_CELL_FRONT_FIRST;
    for (int16_t i = 0; i < cells; i++)
        if (ui_is_point_in_zone(ZONE_DROP_CELL_FIRST + i, x, y)) {
            drop_object(i);
            return;
        }
    if (cells == CELL_COUNT || !is_in_box(wall_box, x, y))
        return;
    if (view->facing_alcove) {
        drop_object(VIEW_CLICK_BOX_ALCOVE);
        return;
    }
    if (view->facing_fountain)
        inventory_fill_at_fountain();
    touch_front_wall();
}

/* The magic caster adds a rune when it has the mana; the runes and the caster state are drawn. */
static void add_champion_symbol(int16_t symbol)
{
    if (!magic_add_champion_symbol(symbol))
        return;
    menus_draw_available_symbols(g_champions[g_magic_caster_champion_index].symbol_step);
    menus_draw_champion_symbols(g_magic_caster_champion_index);
    status_draw_champion(g_magic_caster_champion_index);
}

/* The magic caster removes the last rune; the runes are drawn. */
static void delete_champion_symbol(void)
{
    if (!magic_delete_champion_symbol())
        return;
    menus_draw_available_symbols(g_champions[g_magic_caster_champion_index].symbol_step);
    menus_draw_champion_symbols(g_magic_caster_champion_index);
}

/* Click in the spell area: a caster tab, a rune, delete, or cast. */
static void click_in_spell_area(int16_t x, int16_t y)
{
    int16_t command = ui_command_from_mouse(TABLES_MOUSE_SPELL_AREA, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_CHANGE_MAGIC_CASTER) {
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            if (ui_is_point_in_zone(ZONE_SPELL_CASTER_TAB_FIRST + g_magic_caster_champion_index * SPELL_TAB_STEP + i, x,
                                    y)) {
                menus_set_magic_caster(i);
                return;
            }
    } else if (command == COMMAND_CAST_SPELL) {
        if (!g_champions[g_magic_caster_champion_index].symbols[0])
            return;
        highlight_zone(ZONE_SPELL_CAST_BOX);
        g_stop_waiting_for_input = magic_click_cast_spell();
    } else if (command >= COMMAND_SPELL_SYMBOL_FIRST && command < COMMAND_SPELL_SYMBOL_FIRST + SYMBOL_COUNT) {
        highlight_zone(ZONE_SPELL_SYMBOL_BOX_FIRST + command - COMMAND_SPELL_SYMBOL_FIRST);
        add_champion_symbol(command - COMMAND_SPELL_SYMBOL_FIRST);
    } else if (command == COMMAND_DELETE_SYMBOL) {
        highlight_zone(ZONE_SPELL_DELETE_BOX);
        delete_champion_symbol();
    }
}

/* An action of the menu (or pass, -1) ends the menu of the acting champion. Returns true when the
   action took place. */
static bool trigger_action(int16_t list_index)
{
    if (list_index >= 0)
        return action_did_click_trigger_action(list_index);
    menus_clear_acting_champion();
    return false;
}

/* Click in the action area: choose the acting champion, then an action or pass. */
static void click_in_action_area(int16_t x, int16_t y)
{
    if (g_session.acting_champion_index < 0) {
        if (!g_action_area_contains_icons && !menus_click_damage_hold())
            return;
        int16_t command = ui_command_from_mouse(TABLES_MOUSE_ACTION_ICONS, x, y, CLICK_LEFT_PRESS);
        int16_t index = command - COMMAND_SELECT_ACTING_CHAMPION_FIRST;
        if (command && index >= 0 && index < g_party_champion_count)
            menus_set_acting_champion(index);
        return;
    }
    int16_t command = ui_command_from_mouse(TABLES_MOUSE_ACTION_NAMES, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_PASS) {
        highlight_zone(ZONE_ACTION_PASS_BOX);
        trigger_action(-1);
    } else if (command > COMMAND_PASS && command - COMMAND_PASS <= g_action_list.count) {
        highlight_zone(ZONE_ACTION_NAME_BOX_FIRST + command - COMMAND_ACTION_FIRST);
        g_stop_waiting_for_input = trigger_action(command - COMMAND_ACTION_FIRST);
    }
}

/* While the left button stays down on the eye, the panel shows the skills and statistics, or the
   description of the object in the hand. */
static void click_on_eye(void)
{
    if (!left_button_down)
        return;
    pressing_eye = true;
    g_mouse_pointer_hidden = true;
    command_discard_all_input();
    ui_draw_icon_to_viewport(ICON_EYE_LOOKING, ZONE_EYE);
    panel_draw_eye();
    ui_draw_viewport();
}

/* The left button release ends a press on the eye, the mouth, or an imaginary wall. */
static void process_left_release(void)
{
    if (pressing_eye) {
        pressing_eye = false;
        ui_draw_icon_to_viewport(ICON_EYE, ZONE_EYE);
        panel_draw();
        ui_draw_viewport();
    }
    pressing_mouth = false;
    pressing_fake_wall = false;
    g_mouse_pointer_hidden = false;
}

/* The party sleeps: the game runs at full speed and only the wake up input is active. */
static void go_to_sleep(void)
{
    if (g_session.candidate_champion_index >= 0)
        return;
    if (g_session.inventory_champion_index >= 0)
        command_close_inventory();
    command_freeze_game();
    champion_go_to_sleep();
    show_viewport_message(TEXT_WAKE_UP);
    g_wait_for_input_max_frames = 0;
    set_input((InputTables){TABLES_MOUSE_SLEEPING, NO_TABLE, TABLES_KEYS_SLEEPING, TABLES_KEYS_NONE});
    command_discard_all_input();
}

void command_wake_up(void)
{
    if (primary_mouse != TABLES_MOUSE_SLEEPING)
        return;
    champion_wake_up();
    g_stop_waiting_for_input = true;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    set_game_input();
    command_discard_all_input();
}

/* The game time stops; only the unfreeze input is active. */
static void freeze_game(void)
{
    g_game_time_ticking = false;
    command_freeze_game();
    show_viewport_message(TEXT_GAME_FROZEN);
    frozen_input = get_current_input();
    set_input((InputTables){TABLES_MOUSE_FROZEN, NO_TABLE, TABLES_KEYS_FROZEN, TABLES_KEYS_NONE});
    command_discard_all_input();
}

/* The game time runs again with the input of before the freeze. */
static void unfreeze_game(void)
{
    g_game_time_ticking = true;
    command_unfreeze_game();
    set_input(frozen_input);
    command_discard_all_input();
}

/* A movement command: the arrow lights up; a step that does not move the party ends the input wait. */
static void step_or_turn(int16_t command)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    int16_t map = g_party_map_index;
    g_stop_waiting_for_input = true;
    highlight_zone(ZONE_MOVEMENT_ARROW_BEFORE_FIRST + command);
    if (command <= COMMAND_TURN_RIGHT) {
        move_turn_party(command);
        status_draw_all(CHAMPION_ATTR_ICON);
        return;
    }
    move_party(command);
    if (x == g_party_map_x && y == g_party_map_y && map == g_party_map_index && g_session.new_party_map == -1) {
        command_discard_all_input();
        g_stop_waiting_for_input = false;
    }
}

/* Commands that do not run while the eye or the mouth is pressed. */
static void process_other_command(int16_t command)
{
    if (pressing_eye || pressing_mouth)
        return;
    switch (command) {
    case COMMAND_SLEEP:
        go_to_sleep();
        break;
    case COMMAND_WAKE_UP:
        command_wake_up();
        break;
    case COMMAND_FREEZE_GAME:
        freeze_game();
        break;
    case COMMAND_UNFREEZE_GAME:
        unfreeze_game();
        break;
    case COMMAND_ENTRANCE_ENTER:
    case COMMAND_ENTRANCE_ENTER_DUNGEON_B:
        g_entrance_choice = ENTRANCE_NEW_GAME;
        break;
    case COMMAND_ENTRANCE_RESUME:
        g_entrance_choice = ENTRANCE_RESUME;
        break;
    case COMMAND_ENTRANCE_CREDITS:
        g_entrance_choice = ENTRANCE_CREDITS;
        break;
    case COMMAND_SAVE_GAME:
        startend_save_game();
        break;
    case COMMAND_DIALOG_CHOICE_FIRST:
    case COMMAND_DIALOG_CHOICE_FIRST + 1:
    case COMMAND_DIALOG_CHOICE_FIRST + 2:
    case COMMAND_DIALOG_CHOICE_FIRST + 3:
        g_dialog_choice = command - COMMAND_DIALOG_CHOICE_FIRST + 1;
        break;
    case COMMAND_RESTART_GAME:
        g_restart_requested = true;
        break;
    case COMMAND_QUIT:
        g_quit_requested = true;
        break;
    default:
        break; /* deviation: no name entry */
    }
}

bool command_process_queue(void)
{
    if (!queue_count)
        return false;
    QueuedCommand q = queue[queue_first];
    int16_t command = q.command;
    int16_t x = q.x;
    int16_t y = q.y;
    if (move_is_blocked(command))
        return false;
    queue_first = (queue_first + 1) % QUEUE_SIZE;
    queue_count--;
    if (command == COMMAND_LEFT_RELEASE)
        process_left_release();
    else if (command >= COMMAND_TURN_LEFT && command <= COMMAND_MOVE_LEFT)
        step_or_turn(command);
    else if (command >= COMMAND_CLICK_STATUS_BOX_FIRST &&
             command < COMMAND_CLICK_STATUS_BOX_FIRST + CHAMPION_MAX_COUNT) {
        if (command - COMMAND_CLICK_STATUS_BOX_FIRST < g_party_champion_count && g_session.candidate_champion_index < 0)
            click_in_status_box(command - COMMAND_CLICK_STATUS_BOX_FIRST, x, y);
    } else if (command >= COMMAND_CLICK_CHAMPION_ICON_FIRST && command <= COMMAND_CLICK_CHAMPION_ICON_LAST) {
        click_on_champion_icon(command - COMMAND_CLICK_CHAMPION_ICON_FIRST);
    } else if (command == COMMAND_RIGHT_RELEASE) {
        dragged_icon_cell = -1;
    } else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST && command <= COMMAND_CLICK_SLOT_BOX_LAST) {
        if (g_leader_index != -1)
            inventory_click_on_slot_box(command - COMMAND_CLICK_SLOT_BOX_FIRST);
    } else if (command >= COMMAND_TOGGLE_INVENTORY_FIRST && command <= COMMAND_CLOSE_INVENTORY) {
        int16_t index = command - COMMAND_TOGGLE_INVENTORY_FIRST;
        if (g_session.candidate_champion_index < 0) {
            if (command == COMMAND_CLOSE_INVENTORY)
                command_close_inventory();
            else if (index < g_party_champion_count)
                command_toggle_inventory(index);
        }
    } else if (command == COMMAND_TOGGLE_INVENTORY_LEADER) {
        if (g_leader_index != -1)
            command_toggle_inventory(g_leader_index);
    } else if (command == COMMAND_CLICK_SPELL_AREA) {
        if (g_session.candidate_champion_index < 0 && g_magic_caster_champion_index != -1)
            click_in_spell_area(x, y);
    } else if (command == COMMAND_CLICK_ACTION_AREA) {
        if (g_session.candidate_champion_index < 0)
            click_in_action_area(x, y);
    } else if (command == COMMAND_CLICK_MOUTH) {
        command_click_on_mouth();
    } else if (command == COMMAND_CLICK_EYE) {
        click_on_eye();
    } else if (command == COMMAND_CLICK_DUNGEON_VIEW) {
        click_in_dungeon_view(x - tables_get_viewport_x(), y - tables_get_viewport_y());
    } else if (command == COMMAND_CLICK_PANEL) {
        if (g_session.candidate_champion_index >= 0)
            click_in_candidate_panel(x, y);
        else
            panel_click_in_chest(x, y);
    } else {
        process_other_command(command);
    }
    return true;
}
