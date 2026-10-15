#include <stdbool.h>
#include <string.h>
#include "command.h"
#include "action.h"
#include "advanced.h"
#include "dialog.h"
#include "startend.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "group.h"
#include "ids.h"
#include "inventory.h"
#include "magic.h"
#include "spells.h"
#include "map.h"
#include "move.h"
#include "sensor.h"
#include "sound.h"
#include "timeline_process.h"
#include "ui.h"
#include "video.h"
#include "view.h"

#define QUEUE_SIZE               9
#define QUEUE_CLICK_LIMIT        5 /* a click or a key waits only when fewer commands wait */
#define QUEUE_RELEASE_LIMIT      7
#define NO_TABLE                 (-1)
#define INVENTORY_CLOSE          4 /* command_toggle_inventory argument: close the open inventory */
#define INVENTORY_REDRAW         5 /* command_toggle_inventory argument: draw the open inventory again */
#define WAIT_FOR_INPUT_FRAMES    12
#define FOUNTAIN_WATER           2048
#define FILLED_WATERSKIN_CHARGES 3
enum { MOUTH_BLINK_COUNT = 4, MOUTH_BLINK_FRAMES = 8 };  /* the mouth blinks when it eats an object up */
#define VI_ALTAR_REBIRTH_EFFECT 2                        /* event 13 effect byte */
#define HAND_SLOT_BOX_COUNT     (2 * CHAMPION_MAX_COUNT) /* slot boxes 0-7: the hands in the status boxes */

enum { VIEW_CELL_FRONT_FIRST = 2 }; /* view cells 0-1 on the party square, 2-3 ahead */

typedef struct {
    int16_t x, y, command;
} QueuedCommand;

bool g_stop_waiting_for_input;
bool g_game_time_ticking;
int16_t g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
bool g_game_frozen;
int16_t g_new_game = ENTRANCE_WAITING;
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
static int16_t dragged_icon_cell_plus_1;
static Box highlight;
static bool highlight_enabled;

void command_set_entrance_input(void)
{
    primary_mouse = UI_INPUT_ENTRANCE;
    secondary_mouse = NO_TABLE;
    primary_keys = TABLES_KEYS_ENTRANCE;
    secondary_keys = TABLES_KEYS_NONE;
}

static InputTables current_input(void)
{
    return (InputTables){primary_mouse, secondary_mouse, primary_keys, secondary_keys};
}

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
    dialog_input = current_input();
}
void command_restore_input(void)
{
    set_input(dialog_input);
}

void command_set_game_input(void)
{
    primary_mouse = UI_INPUT_INTERFACE;
    secondary_mouse = UI_INPUT_MOVEMENT;
    primary_keys = TABLES_KEYS_INTERFACE;
    secondary_keys = TABLES_KEYS_MOVEMENT;
}

static void queue_put(int16_t command, int16_t x, int16_t y)
{
    queue[(queue_first + queue_count) % QUEUE_SIZE] = (QueuedCommand){x, y, command};
    queue_count++;
}

/* A mouse button event: a press becomes the command of the box under the pointer in the active
   tables, a left release becomes COMMAND_LEFT_RELEASE. */
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

/* A key press becomes the command of the key in the active tables. With the advanced features,
   Escape closes an open inventory instead of freezing the game, and Tab closes it too. */
void command_process_key(uint16_t key)
{
    if (advanced_process_key(key))
        return;
    g_input_received = true;
    g_last_key = key;
    if (queue_count >= QUEUE_CLICK_LIMIT)
        return;
    int16_t command = tables_key_command(primary_keys, key);
    if (!command)
        command = tables_key_command(secondary_keys, key);
    if (g_advanced_features && ((key == KEY_ESCAPE && command == COMMAND_FREEZE_GAME) || key == KEY_TAB) &&
        g_inventory_champion_ordinal && !g_candidate_champion_ordinal)
        command = COMMAND_CLOSE_INVENTORY;
    if (command)
        queue_put(command, 0, 0);
}

/* Drops all waiting commands except the button releases. */
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

/* Inverts the highlighted zone back. */
void command_highlight_disable(void)
{
    if (!highlight_enabled)
        return;
    for (int y = highlight.y; y < highlight.y + highlight.h; y++)
        for (int x = highlight.x; x < highlight.x + highlight.w; x++)
            if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
                fb[y][x] ^= COLOR_CYAN;
    highlight_enabled = false;
}

/* The square in front of the party. */
static void front_square(int16_t *x, int16_t *y)
{
    *x = g_party_map_x;
    *y = g_party_map_y;
    update_map_coordinates_after_relative_movement(g_party_direction, 1, 0, x, y);
}

/* Shows a text in the empty viewport: the sleep and freeze screens. */
static void draw_viewport_message(int16_t text)
{
    Bitmap *vp = ui_viewport();
    memset(vp->px, COLOR_BLACK, (size_t)vp->w * vp->h);
    ui_print_zone(vp, ZONE_VIEWPORT_MESSAGE, COLOR_CYAN, COLOR_BLACK, gfx_string(text));
    ui_draw_viewport();
}

/* Dims the movement arrows (or the closed inventory panel), the spell area, and the action area. */
void command_freeze_game(void)
{
    Bitmap screen = ui_screen();
    if (g_game_frozen)
        return;
    g_game_frozen = true;
    command_highlight_disable();
    if (!g_inventory_champion_ordinal)
        ui_fill_zone_pattern(&screen, ZONE_MOVEMENT_ARROWS, COLOR_BLACK);
    else
        close_chest();
    ui_fill_zone_pattern(&screen, ZONE_SPELL_AREA, COLOR_BLACK);
    ui_fill_zone_pattern(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
}

/* Draws the spell area, action area, and the view or the inventory again. A sleeping party stays
   frozen on the sleep screen. */
void command_unfreeze_game(void)
{
    if (!g_game_frozen)
        return;
    if (g_party_is_sleeping) {
        draw_viewport_message(TEXT_WAKE_UP);
        return;
    }
    g_game_frozen = false;
    int16_t caster = g_magic_caster_champion_index;
    g_magic_caster_champion_index = -1;
    menus_set_magic_caster(caster);
    if (!g_acting_champion_ordinal)
        g_action_area_contains_icons = true;
    menus_draw_action_area();
    if (!g_inventory_champion_ordinal) {
        menus_draw_movement_arrows();
    } else {
        int16_t open = g_inventory_champion_ordinal;
        g_inventory_champion_ordinal = 0;
        command_toggle_inventory(open - 1);
    }
}

/* Opens the inventory of a champion, closes it (INVENTORY_CLOSE or the open champion again), or
   draws it again (INVENTORY_REDRAW). The inventory has its own mouse table. */
void command_toggle_inventory(int16_t champion_index)
{
    int16_t open = g_inventory_champion_ordinal;
    if (pressing_fake_wall)
        return;
    if (champion_index < CHAMPION_MAX_COUNT && !g_champions[champion_index].current_health)
        return;
    if (champion_index != INVENTORY_REDRAW && (pressing_eye || pressing_mouth))
        return;
    g_stop_waiting_for_input = true;
    if (open == champion_index + 1)
        champion_index = INVENTORY_CLOSE;
    if (open && champion_index != INVENTORY_REDRAW) {
        ui_draw_inventory_close(open - 1);
        if (g_party_is_sleeping)
            return;
        if (champion_index == INVENTORY_CLOSE) {
            secondary_mouse = UI_INPUT_MOVEMENT;
            secondary_keys = TABLES_KEYS_MOVEMENT;
            command_discard_all_input();
            return;
        }
    }
    if (champion_index == INVENTORY_CLOSE)
        return;
    if (champion_index == INVENTORY_REDRAW)
        champion_index = g_inventory_champion_ordinal - 1;
    ui_draw_inventory_open(champion_index);
    secondary_mouse = UI_INPUT_INVENTORY;
    secondary_keys = TABLES_KEYS_NONE;
    command_discard_all_input();
}

/* The inventory champion eats or drinks the leader hand object. The mouth blinks when the object is
   used up, then the champion swallows and its state is drawn. */
void command_click_on_mouth(void)
{
    int16_t result = process_click_on_mouth();
    if (result == MOUTH_REFUSED)
        return;
    uint16_t champion_index = g_inventory_champion_ordinal - 1;
    if (result == MOUTH_USED_UP)
        for (int16_t blink = MOUTH_BLINK_COUNT; blink > 0; blink--) {
            ui_draw_icon_to_viewport(ICON_MOUTH + !(blink & 1), ZONE_MOUTH);
            ui_draw_viewport();
            wait_frames(MOUTH_BLINK_FRAMES);
        }
    sound_request_play(SOUND_SWALLOW, g_party_map_x, g_party_map_y, SOUND_MODE_NOW);
    if (g_panel_content == PANEL_CONTENT_FOOD_WATER)
        g_champions[champion_index].attributes |= CHAMPION_ATTR_PANEL;
    ui_draw_champion_state(champion_index);
}

/* A champion dies: its open inventory closes, also during a press on the eye or the mouth; a dead
   leader ends a press on an imaginary wall; its icon is cleared and is no longer dragged. */
void command_release_dead_champion(uint16_t champion_index, int16_t leader_index)
{
    Bitmap screen = ui_screen();
    int16_t icon_cell = NORMALIZE(g_champions[champion_index].cell - g_party_direction);
    if (champion_index + 1 == g_inventory_champion_ordinal) {
        pressing_eye = false;
        pressing_mouth = false;
        g_mouse_pointer_hidden = false;
        command_toggle_inventory(INVENTORY_CLOSE);
    }
    if (champion_index == leader_index && pressing_fake_wall) {
        pressing_fake_wall = false;
        g_mouse_pointer_hidden = false;
    }
    if (icon_cell + 1 == dragged_icon_cell_plus_1)
        dragged_icon_cell_plus_1 = 0;
    ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + icon_cell, COLOR_BLACK);
}

/* The candidate inventory shows the resurrect and reincarnate panel; the game is frozen meanwhile. */
static void show_candidate(void)
{
    int16_t index = g_candidate_champion_ordinal - 1;
    g_inventory_champion_ordinal = 0;
    command_toggle_inventory(index);
    command_freeze_game();
}

/* Click in the resurrect/reincarnate panel of the candidate: accept or cancel it. */
static void click_in_candidate_panel(int16_t x, int16_t y)
{
    Bitmap screen = ui_screen();
    int16_t command = ui_command_from_mouse(UI_INPUT_RESURRECT, x, y, CLICK_LEFT_PRESS);
    int16_t index = g_party_champion_count - 1;
    if (!command || !g_leader_empty_handed)
        return;
    uint8_t cell = g_champions[index].cell;
    champion_click_in_resurrect_reincarnate_panel(command);
    g_inventory_champion_ordinal = index + 1; /* the rule clears it; the panel closes here */
    command_toggle_inventory(INVENTORY_CLOSE);
    if (command == PANEL_CANCEL) {
        ui_fill_zone(&screen, ZONE_STATUS_BOX_FIRST + index, COLOR_BLACK);
        ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + NORMALIZE(cell - g_party_direction), COLOR_BLACK);
        if (!g_party_champion_count)
            menus_set_magic_caster(-1);
    } else {
        int16_t caster = g_magic_caster_champion_index;
        g_magic_caster_champion_index = -1;
        menus_set_magic_caster(caster);
        ui_draw_all_champion_states(CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_NAME_TITLE | CHAMPION_ATTR_STATISTICS |
                                    CHAMPION_ATTR_ICON | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS);
    }
    g_redraw |= REDRAW_ACTION_AREA;
}

/* Left click in a champion status box: the name sets the leader, a hand is a slot box. */
static void click_in_status_box(int16_t champion_index, int16_t x, int16_t y)
{
    if (champion_index + 1 == g_inventory_champion_ordinal) {
        champion_set_leader(champion_index);
        return;
    }
    int16_t command = ui_command_from_mouse(UI_INPUT_STATUS_BOX, x, y, CLICK_LEFT_PRESS);
    if (command >= COMMAND_SET_LEADER_FIRST && command < COMMAND_SET_LEADER_FIRST + CHAMPION_MAX_COUNT)
        champion_set_leader(command - COMMAND_SET_LEADER_FIRST);
    else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST && command < COMMAND_CLICK_SLOT_BOX_FIRST + HAND_SLOT_BOX_COUNT)
        inventory_click_on_slot_box(command - COMMAND_CLICK_SLOT_BOX_FIRST);
}

/* First click on a champion icon picks the champion; the second click moves it to the clicked
   cell and swaps it with the champion there. */
static void click_on_champion_icon(int16_t cell)
{
    Bitmap screen = ui_screen();
    if (!dragged_icon_cell_plus_1) {
        if (champion_get_index_in_cell(NORMALIZE(cell + g_party_direction)) == -1)
            return;
        dragged_icon_cell_plus_1 = cell + 1;
        return;
    }
    int16_t from = dragged_icon_cell_plus_1 - 1;
    dragged_icon_cell_plus_1 = 0;
    int16_t moved = champion_get_index_in_cell(NORMALIZE(from + g_party_direction));
    if (moved == -1)
        return;
    g_champions[moved].direction = g_party_direction;
    g_champions[moved].attributes |= CHAMPION_ATTR_ICON;
    if (cell != from) {
        int16_t other = champion_get_index_in_cell(NORMALIZE(cell + g_party_direction));
        if (other < 0) {
            ui_fill_zone(&screen, ZONE_CHAMPION_ICON_FIRST + from, COLOR_BLACK);
        } else {
            g_champions[other].cell = NORMALIZE(from + g_party_direction);
            g_champions[other].attributes |= CHAMPION_ATTR_ICON;
            ui_draw_champion_state(other);
        }
        g_champions[moved].cell = NORMALIZE(cell + g_party_direction);
    }
    ui_draw_champion_state(moved);
}

/* The leader takes the top object of a pile in view cell 0-3 or the alcove. A creature in a
   front cell protects the objects under it. */
static void grab_object(int16_t pile)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    if (g_leader_index == -1)
        return;
    if (pile >= VIEW_CELL_FRONT_FIRST) {
        front_square(&x, &y);
        uint16_t group = get_square_group(x, y);
        if (group != THING_ENDOFLIST && !move_is_levitating(group) &&
            group_get_creature_ordinal_in_cell(group, NORMALIZE(pile + g_party_direction)))
            return;
    }
    uint16_t thing = g_pile_top_objects[pile];
    if (object_get_icon_index(thing) != -1) {
        move_get_move_result(thing, x, y, -1, 0);
        inventory_put_object_in_leader_hand(thing);
    }
    g_stop_waiting_for_input = true;
}

/* The leader puts the hand object on the floor of view cell 0-3 or into the alcove ahead. Bones
   on a vi altar start the rebirth of their champion. */
static void drop_object(int16_t pile)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    bool alcove = pile == VIEW_CLICK_BOX_ALCOVE;
    if (g_leader_index == -1)
        return;
    if (alcove)
        pile = VIEW_CELL_FRONT_FIRST;
    if (pile >= VIEW_CELL_FRONT_FIRST)
        front_square(&x, &y);
    int16_t cell = NORMALIZE(pile + g_party_direction);
    uint16_t thing = inventory_get_object_removed_from_leader_hand();
    move_get_move_result(THING_WITH_CELL(thing, cell), -1, 0, x, y);
    if (alcove && g_facing_vi_altar && object_get_icon_index(thing) == ICON_BONES) {
        TimelineEvent rebirth = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + 1),
                                 .type = EVENT_VI_ALTAR_REBIRTH,
                                 .priority = junk_charges(thing),
                                 .b = event_square(x, y),
                                 .c = event_cell_effect(cell, VI_ALTAR_REBIRTH_EFFECT)};
        timeline_add_event(&rebirth);
    }
    g_stop_waiting_for_input = true;
}

/* The leader hand touches the wall ahead: wall sensors and mirrors. */
static void touch_front_wall(void)
{
    int16_t x, y;
    front_square(&x, &y);
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height)
        return;
    int16_t candidate = g_candidate_champion_ordinal;
    /* the wall side that faces the party */
    g_stop_waiting_for_input = sensor_is_triggered_by_click_on_wall(x, y, turn_back(g_party_direction));
    if (!candidate && g_candidate_champion_ordinal)
        show_candidate();
}

/* An empty hand on the square ahead: a thud, or the hand goes into an imaginary wall while the
   button is down. */
static void touch_front_square_empty_handed(int16_t imaginary_wall, int16_t x, int16_t y)
{
    if (!g_party_champion_count)
        return;
    if (!imaginary_wall)
        sound_request_play(SOUND_WOODEN_THUD, x, y, SOUND_MODE_NOW);
    else if (left_button_down) {
        pressing_fake_wall = true;
        g_mouse_pointer_hidden = true;
    }
    g_stop_waiting_for_input = true;
}

/* A flask or a waterskin in the hand fills at a fountain. */
static void fill_at_fountain(void)
{
    uint16_t thing = g_leader_hand_object;
    int16_t icon = object_get_icon_index(thing);
    uint16_t weight = inventory_get_object_weight(thing);
    if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL)
        junk_set_charges(thing, FILLED_WATERSKIN_CHARGES);
    else if (icon == ICON_EMPTY_FLASK)
        potion_set_type(thing, POTION_WATER);
    else
        return;
    g_champions[g_leader_index].load += inventory_get_object_weight(thing) - weight;
    g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
}

/* A point in a clickable box of the view; x -1 marks an unused box. */
static bool in_box(const Box *b, int16_t x, int16_t y)
{
    return b->x != -1 && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}

/* The leader throws the hand object when the click is in the left or right throw zone. */
static bool is_leader_hand_object_thrown(int16_t x, int16_t y)
{
    int16_t zone = g_square_ahead_element == VIEW_ELEMENT_DOOR_FRONT ? ZONE_THROW_DOOR_LEFT : ZONE_THROW_LEFT;
    int16_t thrown = 0;
    if (ui_point_in_zone(zone, x, y))
        thrown = action_is_leader_hand_object_thrown(0);
    else if (ui_point_in_zone(zone + 1, x, y))
        thrown = action_is_leader_hand_object_thrown(1);
    if (thrown)
        g_stop_waiting_for_input = true;
    return thrown;
}

/* An empty-handed click in the dungeon view: grab an object, drink at a fountain, touch the wall, or touch the
   square ahead. */
static void click_empty_handed(int16_t x, int16_t y, int16_t fx, int16_t fy, int16_t imaginary_wall)
{
    for (int16_t i = 0; i < VIEW_CLICK_BOX_COUNT; i++) {
        if (!in_box(&g_dungeon_view_clickable_boxes[i], x, y))
            continue;
        if (i != VIEW_CLICK_BOX_WALL) {
            grab_object(i);
            return;
        }
        if (g_facing_fountain && g_leader_index != -1) {
            g_champions[g_leader_index].water = FOUNTAIN_WATER;
            /* With the advanced features, every living champion drinks. */
            for (uint16_t i = 0; g_advanced_features && i < g_party_champion_count; i++)
                if (g_champions[i].current_health)
                    g_champions[i].water = FOUNTAIN_WATER;
            sound_request_play(SOUND_SWALLOW, g_party_map_x, g_party_map_y, SOUND_MODE_NOW);
        }
        if (!g_facing_alcove)
            touch_front_wall();
        return;
    }
    if (g_square_ahead_element != VIEW_ELEMENT_WALL)
        return;
    if (ui_point_in_zone(ZONE_THROW_LEFT, x, y) || ui_point_in_zone(ZONE_THROW_LEFT + 1, x, y))
        touch_front_square_empty_handed(imaginary_wall, fx, fy);
}

/* Click in the dungeon view (x, y in viewport coordinates): door buttons, objects, the front wall. */
static void click_in_dungeon_view(int16_t x, int16_t y)
{
    int16_t fx, fy;
    const Box *wall_box = &g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL];
    front_square(&fx, &fy);
    if (g_square_ahead_element == VIEW_ELEMENT_DOOR_FRONT) {
        if (g_leader_index == -1)
            return;
        if (!g_leader_empty_handed && is_leader_hand_object_thrown(x, y))
            return;
        if (g_leader_empty_handed && door_button_present(get_square_first_thing(fx, fy)) && in_box(wall_box, x, y)) {
            sound_request_play(SOUND_SWITCH, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
            sensor_add_event(EVENT_DOOR, fx, fy, 0, EFFECT_TOGGLE, (uint32_t)(g_game_time + 1));
            g_stop_waiting_for_input = true;
            return;
        }
    }
    uint8_t square = get_square(fx, fy);
    int16_t imaginary_wall =
        SQUARE_TYPE(square) == SQUARE_FAKE_WALL && (square & FAKE_WALL_IMAGINARY) && !(square & FAKE_WALL_OPEN);
    if (g_leader_empty_handed) {
        click_empty_handed(x, y, fx, fy, imaginary_wall);
        return;
    }
    if ((g_square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall) && is_leader_hand_object_thrown(x, y))
        return;
    int16_t cells = g_square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall ? CELL_COUNT : VIEW_CELL_FRONT_FIRST;
    for (int16_t i = 0; i < cells; i++)
        if (ui_point_in_zone(ZONE_DROP_CELL_FIRST + i, x, y)) {
            drop_object(i);
            return;
        }
    if (cells == CELL_COUNT || !in_box(wall_box, x, y))
        return;
    if (g_facing_alcove) {
        drop_object(VIEW_CLICK_BOX_ALCOVE);
        return;
    }
    if (g_facing_fountain)
        fill_at_fountain();
    touch_front_wall();
}

/* The magic caster adds a rune when it has the mana. */
static void add_champion_symbol(int16_t symbol)
{
    Champion *c = &g_champions[g_magic_caster_champion_index];
    uint16_t step = c->symbol_step;
    uint16_t cost = *tables_symbol_base_mana_cost(step, symbol);
    if (step) /* 16-bit product as in the original */
        cost = (uint16_t)(cost * *tables_symbol_mana_cost_multiplier((uint8_t)c->symbols[0] - SPELL_SYMBOL_FIRST)) >>
               SPELL_MANA_MULTIPLIER_SHIFT;
    if (cost > (uint16_t)c->current_mana)
        return; /* unsigned compare as in the original */
    c->current_mana -= cost;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    c->symbols[step] = (char)(SPELL_SYMBOL_FIRST + step * SYMBOL_COUNT + symbol);
    c->symbols[step + 1] = 0;
    c->symbol_step = NORMALIZE(step + 1);
    menus_draw_available_symbols(c->symbol_step);
    menus_draw_champion_symbols(g_magic_caster_champion_index);
    ui_draw_champion_state(g_magic_caster_champion_index);
}

/* The magic caster removes the last rune; the mana stays spent. */
static void delete_champion_symbol(void)
{
    Champion *c = &g_champions[g_magic_caster_champion_index];
    if (!c->symbols[0])
        return;
    c->symbol_step = NORMALIZE(c->symbol_step - 1);
    c->symbols[c->symbol_step] = 0;
    menus_draw_available_symbols(c->symbol_step);
    menus_draw_champion_symbols(g_magic_caster_champion_index);
}

/* Click in the spell area: a caster tab, a rune, delete, or cast. */
static void click_in_spell_area(int16_t x, int16_t y)
{
    int16_t command = ui_command_from_mouse(UI_INPUT_SPELL_AREA, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_CHANGE_MAGIC_CASTER) {
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            if (ui_point_in_zone(ZONE_SPELL_CASTER_TAB_FIRST + g_magic_caster_champion_index * SPELL_TAB_STEP + i, x,
                                 y)) {
                menus_set_magic_caster(i);
                return;
            }
    } else if (command == COMMAND_CAST_SPELL) {
        if (!g_champions[g_magic_caster_champion_index].symbols[0])
            return;
        highlight_zone(ZONE_SPELL_CAST_BOX);
        g_stop_waiting_for_input = magic_get_click_on_spell_cast_result();
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
    if (!g_acting_champion_ordinal) {
        if (!g_action_area_contains_icons)
            return;
        int16_t command = ui_command_from_mouse(UI_INPUT_ACTION_ICONS, x, y, CLICK_LEFT_PRESS);
        int16_t index = command - COMMAND_SELECT_ACTING_CHAMPION_FIRST;
        if (command && index >= 0 && index < g_party_champion_count)
            menus_set_acting_champion(index);
        return;
    }
    int16_t command = ui_command_from_mouse(UI_INPUT_ACTION_NAMES, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_PASS) {
        highlight_zone(ZONE_ACTION_PASS_BOX);
        trigger_action(-1);
    } else if (command > COMMAND_PASS && command - COMMAND_PASS <= g_action_count) {
        highlight_zone(ZONE_ACTION_NAME_BOX_FIRST + command - COMMAND_ACTION_FIRST);
        g_stop_waiting_for_input = trigger_action(command - COMMAND_ACTION_FIRST);
    }
}

/* Draws an object icon in a zone of the viewport. */
static void draw_viewport_icon(int16_t icon, int16_t zone)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = {UI_ICON_SIZE, UI_ICON_SIZE, px};
    ui_extract_icon(icon, &bm);
    ui_blit_zone(ui_viewport(), &bm, zone, COLOR_NONE);
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
    draw_viewport_icon(ICON_EYE_LOOKING, ZONE_EYE);
    ui_draw_panel_eye();
    ui_draw_viewport();
}

/* The left button release ends a press on the eye, the mouth, or an imaginary wall. */
static void left_release(void)
{
    if (pressing_eye) {
        pressing_eye = false;
        draw_viewport_icon(ICON_EYE, ZONE_EYE);
        ui_draw_panel();
        ui_draw_viewport();
    }
    pressing_mouth = false;
    pressing_fake_wall = false;
    g_mouse_pointer_hidden = false;
}

/* The party sleeps: the game runs at full speed and only the wake up input is active. */
static void go_to_sleep(void)
{
    if (g_candidate_champion_ordinal)
        return;
    if (g_inventory_champion_ordinal)
        command_toggle_inventory(INVENTORY_CLOSE);
    command_freeze_game();
    g_party_is_sleeping = true;
    draw_viewport_message(TEXT_WAKE_UP);
    g_wait_for_input_max_frames = 0;
    set_input((InputTables){UI_INPUT_SLEEPING, NO_TABLE, TABLES_KEYS_SLEEPING, TABLES_KEYS_NONE});
    command_discard_all_input();
}

/* The party wakes up by a command or by damage: game speed and input return. */
void command_wake_up(void)
{
    if (primary_mouse != UI_INPUT_SLEEPING)
        return;
    champion_wake_up();
    g_stop_waiting_for_input = true;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    command_set_game_input();
    command_discard_all_input();
}

/* The game time stops; only the unfreeze input is active. */
static void freeze_game(void)
{
    g_game_time_ticking = false;
    command_freeze_game();
    draw_viewport_message(TEXT_GAME_FROZEN);
    frozen_input = current_input();
    set_input((InputTables){UI_INPUT_FROZEN, NO_TABLE, TABLES_KEYS_FROZEN, TABLES_KEYS_NONE});
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
static void move_or_turn(int16_t command)
{
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    int16_t map = g_party_map_index;
    g_stop_waiting_for_input = true;
    highlight_zone(ZONE_MOVEMENT_ARROW_BEFORE_FIRST + command);
    if (command <= COMMAND_TURN_RIGHT) {
        move_turn_party(command);
        ui_draw_all_champion_states(CHAMPION_ATTR_ICON);
        return;
    }
    move_party(command);
    if (x == g_party_map_x && y == g_party_map_y && map == g_party_map_index && g_new_party_map_index == -1) {
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
        g_new_game = ENTRANCE_NEW_GAME;
        break;
    case COMMAND_ENTRANCE_RESUME:
        g_new_game = ENTRANCE_RESUME;
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
        break; /* ponytail: credits (203) and name entry are not ported yet */
    }
}

/* A step waits while the party cannot move, or while a projectile blocks this direction. */
static bool movement_blocked(int16_t command)
{
    if (command < COMMAND_MOVE_FORWARD || command > COMMAND_MOVE_LEFT)
        return false;
    if (g_disabled_movement_ticks)
        return true;
    return g_projectile_disabled_movement_ticks && NORMALIZE(command + g_party_direction - COMMAND_MOVE_FORWARD) ==
                                                       g_last_projectile_disabled_movement_direction;
}

/* Takes the first waiting command and runs it. A movement command waits while the party cannot
   move. Returns true when it ran a command. */
bool command_process_queue(void)
{
    if (!queue_count)
        return false;
    QueuedCommand q = queue[queue_first];
    int16_t command = q.command;
    int16_t x = q.x;
    int16_t y = q.y;
    if (movement_blocked(command))
        return false;
    queue_first = (queue_first + 1) % QUEUE_SIZE;
    queue_count--;
    if (command == COMMAND_LEFT_RELEASE)
        left_release();
    else if (command >= COMMAND_TURN_LEFT && command <= COMMAND_MOVE_LEFT)
        move_or_turn(command);
    else if (command >= COMMAND_CLICK_STATUS_BOX_FIRST &&
             command < COMMAND_CLICK_STATUS_BOX_FIRST + CHAMPION_MAX_COUNT) {
        if (command - COMMAND_CLICK_STATUS_BOX_FIRST < g_party_champion_count && !g_candidate_champion_ordinal)
            click_in_status_box(command - COMMAND_CLICK_STATUS_BOX_FIRST, x, y);
    } else if (command >= COMMAND_CLICK_CHAMPION_ICON_FIRST && command <= COMMAND_CLICK_CHAMPION_ICON_LAST) {
        click_on_champion_icon(command - COMMAND_CLICK_CHAMPION_ICON_FIRST);
    } else if (command == COMMAND_RIGHT_RELEASE) {
        dragged_icon_cell_plus_1 = 0;
    } else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST && command <= COMMAND_CLICK_SLOT_BOX_LAST) {
        if (g_leader_index != -1)
            inventory_click_on_slot_box(command - COMMAND_CLICK_SLOT_BOX_FIRST);
    } else if (command >= COMMAND_TOGGLE_INVENTORY_FIRST && command <= COMMAND_CLOSE_INVENTORY) {
        int16_t index = command - COMMAND_TOGGLE_INVENTORY_FIRST;
        if ((index == INVENTORY_CLOSE || index < g_party_champion_count) && !g_candidate_champion_ordinal)
            command_toggle_inventory(index);
    } else if (command == COMMAND_TOGGLE_INVENTORY_LEADER) {
        if (g_leader_index != -1)
            command_toggle_inventory(g_leader_index);
    } else if (command == COMMAND_CLICK_SPELL_AREA) {
        if (!g_candidate_champion_ordinal && g_magic_caster_champion_index != -1)
            click_in_spell_area(x, y);
    } else if (command == COMMAND_CLICK_ACTION_AREA) {
        if (!g_candidate_champion_ordinal)
            click_in_action_area(x, y);
    } else if (command == COMMAND_CLICK_MOUTH) {
        command_click_on_mouth();
    } else if (command == COMMAND_CLICK_EYE) {
        click_on_eye();
    } else if (command == COMMAND_CLICK_DUNGEON_VIEW) {
        click_in_dungeon_view(x - tables_viewport_x(), y - tables_viewport_y());
    } else if (command == COMMAND_CLICK_PANEL) {
        if (g_candidate_champion_ordinal)
            click_in_candidate_panel(x, y);
        else
            panel_click_in_chest(x, y);
    } else {
        process_other_command(command);
    }
    return true;
}

/* A new or loaded game starts without a pressed eye, mouth or wall, a highlight, a sleeping input
   speed, or waiting commands; the game input tables are active. */
void command_reset(void)
{
    left_button_down = false;
    pressing_eye = false;
    pressing_mouth = false;
    pressing_fake_wall = false;
    dragged_icon_cell_plus_1 = 0;
    highlight_enabled = false;
    g_mouse_pointer_hidden = false;
    g_stop_waiting_for_input = false;
    g_game_frozen = false;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    queue_first = 0;
    queue_count = 0;
    g_input_received = false;
    g_last_key = 0;
    command_set_game_input();
}
