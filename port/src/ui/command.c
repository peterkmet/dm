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

/* KAOS data addresses: keyboard input tables (command word, key word; command 0 ends a table). */
#define KEYS_INTERFACE 0x103948       /* inventory keys F1-F4, save, freeze */
#define KEYS_MOVEMENT 0x103964        /* movement and turn keys */
#define KEYS_ENTRANCE 0x10393c
#define KEYS_SLEEPING 0x103980
#define KEYS_FROZEN 0x10398c
#define KEY_ENTRY_BYTES 4
#define KEY_ENTRY_KEY 2               /* the key word after the command word */

#define QUEUE_SIZE 9
#define QUEUE_CLICK_LIMIT 5           /* a click or a key waits only when fewer commands wait */
#define QUEUE_RELEASE_LIMIT 7
#define NO_TABLE (-1)
#define INVENTORY_CLOSE 4             /* inventory_toggle argument: close the open inventory */
#define INVENTORY_REDRAW 5            /* inventory_toggle argument: draw the open inventory again */
#define WAIT_FOR_INPUT_FRAMES 12
#define FOUNTAIN_WATER 2048
#define FILLED_WATERSKIN_CHARGES 3
#define VI_ALTAR_REBIRTH_EFFECT 2     /* event 13 effect byte */
#define MANA_MULTIPLIER_SHIFT 3       /* the cost multiplier of the first rune is in eighths */
#define HAND_SLOT_BOX_COUNT (2 * CHAMPION_MAX_COUNT) /* slot boxes 0-7: the hands in the status boxes */
#define ZONE_THROW_LEFT 745           /* + 1 right: the upper view where a thrown object flies */
#define ZONE_THROW_DOOR_LEFT 748      /* + 1 right: the same with a door in front */

enum { VIEW_CELL_FRONT_FIRST = 2, VIEW_CELL_COUNT = 4 }; /* view cells 0-1 on the party square, 2-3 ahead */

typedef struct {
    int16_t x, y, command;
} QueuedCommand;

int16_t g_stop_waiting_for_input;
int16_t g_game_time_ticking;
int16_t g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
int16_t g_game_frozen;
int16_t g_new_game = ENTRANCE_WAITING;
int16_t g_mouse_pointer_hidden;
int16_t g_input_received;
uint16_t g_last_key;

static QueuedCommand queue[QUEUE_SIZE];
static int16_t queue_first, queue_count;
static int16_t primary_mouse = NO_TABLE, secondary_mouse = NO_TABLE;
static uint32_t primary_keys, secondary_keys;
typedef struct {
    int16_t primary_mouse, secondary_mouse;
    uint32_t primary_keys, secondary_keys;
} InputTables;

static InputTables frozen_input, dialog_input;
static int16_t left_button_down, pressing_eye, pressing_mouth, pressing_fake_wall;
static int16_t dragged_icon_cell_plus_1;
static Box highlight;
static int16_t highlight_enabled;

void command_set_entrance_input(void)
{
    primary_mouse = UI_INPUT_ENTRANCE;
    secondary_mouse = NO_TABLE;
    primary_keys = KEYS_ENTRANCE;
    secondary_keys = 0;
}

static InputTables current_input(void)
{
    return (InputTables){ primary_mouse, secondary_mouse, primary_keys, secondary_keys };
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
    set_input((InputTables){ table, NO_TABLE, 0, 0 });
}

void command_save_input(void) { dialog_input = current_input(); }
void command_restore_input(void) { set_input(dialog_input); }

void command_set_game_input(void)
{
    primary_mouse = UI_INPUT_INTERFACE;
    secondary_mouse = UI_INPUT_MOVEMENT;
    primary_keys = KEYS_INTERFACE;
    secondary_keys = KEYS_MOVEMENT;
}

static void queue_put(int16_t command, int16_t x, int16_t y)
{
    queue[(queue_first + queue_count) % QUEUE_SIZE] = (QueuedCommand){ x, y, command };
    queue_count++;
}

/* A mouse button event: a press becomes the command of the box under the pointer in the active
   tables, a left release becomes COMMAND_LEFT_RELEASE. */
void command_process_click(int16_t x, int16_t y, int16_t kind)
{
    if (kind == CLICK_LEFT_PRESS || kind == CLICK_RIGHT_PRESS) g_input_received = 1;
    if (kind == CLICK_LEFT_PRESS) left_button_down = 1;
    if (kind == CLICK_LEFT_RELEASE) left_button_down = 0;
    if (advanced_process_click(x, y, kind)) return;
    if (queue_count >= (kind == CLICK_LEFT_RELEASE ? QUEUE_RELEASE_LIMIT : QUEUE_CLICK_LIMIT)) return;
    int16_t command = COMMAND_LEFT_RELEASE;
    if (kind != CLICK_LEFT_RELEASE) {
        command = ui_command_from_mouse(primary_mouse, x, y, (uint16_t)kind);
        if (!command) command = ui_command_from_mouse(secondary_mouse, x, y, (uint16_t)kind);
    }
    if (command) queue_put(command, x, y);
}

/* The command of a key word in a keyboard table, or 0. */
static int16_t key_command(uint32_t table, uint16_t key)
{
    if (!table) return COMMAND_NONE;
    for (uint32_t entry = table; kaos_s16(entry); entry += KEY_ENTRY_BYTES)
        if (kaos_u16(entry + KEY_ENTRY_KEY) == key) return kaos_s16(entry);
    return COMMAND_NONE;
}

/* A key press becomes the command of the key in the active tables. With the advanced features,
   Escape closes an open inventory instead of freezing the game, and Tab closes it too. */
void command_process_key(uint16_t key)
{
    if (advanced_process_key(key)) return;
    g_input_received = 1;
    g_last_key = key;
    if (queue_count >= QUEUE_CLICK_LIMIT) return;
    int16_t command = key_command(primary_keys, key);
    if (!command) command = key_command(secondary_keys, key);
    if (g_advanced_features && ((key == KEY_ESCAPE && command == COMMAND_FREEZE_GAME) || key == KEY_TAB) &&
        g_inventory_champion_ordinal && !g_candidate_champion_ordinal)
        command = COMMAND_CLOSE_INVENTORY;
    if (command) queue_put(command, 0, 0);
}

/* Drops all waiting commands except the button releases. */
void command_discard_all_input(void)
{
    QueuedCommand kept[QUEUE_SIZE];
    int16_t n = 0;
    g_input_received = 0;
    g_last_key = 0;
    for (int16_t i = 0; i < queue_count; i++) {
        QueuedCommand c = queue[(queue_first + i) % QUEUE_SIZE];
        if (c.command == COMMAND_LEFT_RELEASE || c.command == COMMAND_RIGHT_RELEASE) kept[n++] = c;
    }
    queue_first = 0;
    queue_count = n;
    memcpy(queue, kept, sizeof kept[0] * n);
}

/* Inverts a zone on the screen until the next tick. */
static void highlight_zone(int16_t zone)
{
    command_highlight_disable();
    if (!ui_zone_box(zone, &highlight)) return;
    ui_xor_screen_zone(zone);
    highlight_enabled = 1;
}

/* Inverts the highlighted zone back. */
void command_highlight_disable(void)
{
    if (!highlight_enabled) return;
    for (int y = highlight.y; y < highlight.y + highlight.h; y++)
        for (int x = highlight.x; x < highlight.x + highlight.w; x++)
            if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y][x] ^= COLOR_CYAN;
    highlight_enabled = 0;
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
void game_freeze(void)
{
    Bitmap screen = ui_screen();
    if (g_game_frozen) return;
    g_game_frozen = 1;
    command_highlight_disable();
    if (!g_inventory_champion_ordinal) ui_fill_zone_pattern(&screen, ZONE_MOVEMENT_ARROWS, COLOR_BLACK);
    else close_chest();
    ui_fill_zone_pattern(&screen, ZONE_SPELL_AREA, COLOR_BLACK);
    ui_fill_zone_pattern(&screen, ZONE_ACTION_AREA, COLOR_BLACK);
}

/* Draws the spell area, action area, and the view or the inventory again. A sleeping party stays
   frozen on the sleep screen. */
void game_unfreeze(void)
{
    if (!g_game_frozen) return;
    if (g_party_is_sleeping) {
        draw_viewport_message(TEXT_WAKE_UP);
        return;
    }
    g_game_frozen = 0;
    int16_t caster = g_magic_caster_champion_index;
    g_magic_caster_champion_index = -1;
    menus_set_magic_caster(caster);
    if (!g_acting_champion_ordinal) g_action_area_contains_icons = 1;
    menus_draw_action_area();
    if (!g_inventory_champion_ordinal) {
        menus_draw_movement_arrows();
    } else {
        int16_t open = g_inventory_champion_ordinal;
        g_inventory_champion_ordinal = 0;
        inventory_toggle(open - 1);
    }
}

/* Opens the inventory of a champion, closes it (INVENTORY_CLOSE or the open champion again), or
   draws it again (INVENTORY_REDRAW). The inventory has its own mouse table. */
void inventory_toggle(int16_t champion_index)
{
    int16_t open = g_inventory_champion_ordinal;
    if (pressing_fake_wall) return;
    if (champion_index < CHAMPION_MAX_COUNT && !g_champions[champion_index].current_health) return;
    if (champion_index != INVENTORY_REDRAW && (pressing_eye || pressing_mouth)) return;
    g_stop_waiting_for_input = 1;
    if (open == champion_index + 1) champion_index = INVENTORY_CLOSE;
    if (open && champion_index != INVENTORY_REDRAW) {
        inventory_draw_close(open - 1);
        if (g_party_is_sleeping) return;
        if (champion_index == INVENTORY_CLOSE) {
            secondary_mouse = UI_INPUT_MOVEMENT;
            secondary_keys = KEYS_MOVEMENT;
            command_discard_all_input();
            return;
        }
    }
    if (champion_index == INVENTORY_CLOSE) return;
    if (champion_index == INVENTORY_REDRAW) champion_index = g_inventory_champion_ordinal - 1;
    inventory_draw_open(champion_index);
    secondary_mouse = UI_INPUT_INVENTORY;
    secondary_keys = 0;
    command_discard_all_input();
}

/* A champion dies: its open inventory closes, also during a press on the eye or the mouth; a dead
   leader ends a press on an imaginary wall; its icon is cleared and is no longer dragged. */
void command_release_dead_champion(uint16_t champion_index, int16_t leader_index)
{
    Bitmap screen = ui_screen();
    int16_t icon_cell = NORMALIZE(g_champions[champion_index].cell - g_party_direction);
    if (champion_index + 1 == g_inventory_champion_ordinal) {
        pressing_eye = pressing_mouth = g_mouse_pointer_hidden = 0;
        inventory_toggle(INVENTORY_CLOSE);
    }
    if (champion_index == leader_index && pressing_fake_wall) pressing_fake_wall = g_mouse_pointer_hidden = 0;
    if (icon_cell + 1 == dragged_icon_cell_plus_1) dragged_icon_cell_plus_1 = 0;
    ui_fill_zone(&screen, (int16_t)(ZONE_CHAMPION_ICON_FIRST + icon_cell), COLOR_BLACK);
}

/* The candidate inventory shows the resurrect and reincarnate panel; the game is frozen meanwhile. */
static void show_candidate(void)
{
    int16_t index = g_candidate_champion_ordinal - 1;
    g_inventory_champion_ordinal = 0;
    inventory_toggle(index);
    game_freeze();
}

/* Click in the resurrect/reincarnate panel of the candidate: accept or cancel it. */
static void click_in_candidate_panel(int16_t x, int16_t y)
{
    Bitmap screen = ui_screen();
    int16_t command = ui_command_from_mouse(UI_INPUT_RESURRECT, x, y, CLICK_LEFT_PRESS);
    int16_t index = g_party_champion_count - 1;
    if (!command || !g_leader_empty_handed) return;
    uint8_t cell = g_champions[index].cell;
    champion_click_in_resurrect_reincarnate_panel(command);
    g_inventory_champion_ordinal = index + 1; /* the rule clears it; the panel closes here */
    inventory_toggle(INVENTORY_CLOSE);
    if (command == PANEL_CANCEL) {
        ui_fill_zone(&screen, (int16_t)(ZONE_STATUS_BOX_FIRST + index), COLOR_BLACK);
        ui_fill_zone(&screen, (int16_t)(ZONE_CHAMPION_ICON_FIRST + NORMALIZE(cell - g_party_direction)), COLOR_BLACK);
        if (!g_party_champion_count) menus_set_magic_caster(-1);
    } else {
        int16_t caster = g_magic_caster_champion_index;
        g_magic_caster_champion_index = -1;
        menus_set_magic_caster(caster);
        champion_draw_all_states(CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_NAME_TITLE | CHAMPION_ATTR_STATISTICS |
                                 CHAMPION_ATTR_ICON | CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_WOUNDS);
    }
    g_redraw |= REDRAW_ACTION_AREA;
}

/* Left click in a champion status box: the name sets the leader, a hand is a slot box. */
static void click_in_status_box(int16_t champion_index, int16_t x, int16_t y)
{
    if (champion_index + 1 == g_inventory_champion_ordinal) {
        command_set_leader(champion_index);
        return;
    }
    int16_t command = ui_command_from_mouse(UI_INPUT_STATUS_BOX, x, y, CLICK_LEFT_PRESS);
    if (command >= COMMAND_SET_LEADER_FIRST && command < COMMAND_SET_LEADER_FIRST + CHAMPION_MAX_COUNT)
        command_set_leader(command - COMMAND_SET_LEADER_FIRST);
    else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST && command < COMMAND_CLICK_SLOT_BOX_FIRST + HAND_SLOT_BOX_COUNT)
        champion_click_on_slot_box((uint16_t)(command - COMMAND_CLICK_SLOT_BOX_FIRST));
}

/* First click on a champion icon picks the champion; the second click moves it to the clicked
   cell and swaps it with the champion there. */
static void click_on_champion_icon(int16_t cell)
{
    Bitmap screen = ui_screen();
    if (!dragged_icon_cell_plus_1) {
        if (champion_get_index_in_cell(NORMALIZE(cell + g_party_direction)) == -1) return;
        dragged_icon_cell_plus_1 = cell + 1;
        return;
    }
    int16_t from = dragged_icon_cell_plus_1 - 1;
    dragged_icon_cell_plus_1 = 0;
    int16_t moved = champion_get_index_in_cell(NORMALIZE(from + g_party_direction));
    if (moved == -1) return;
    g_champions[moved].direction = (uint8_t)g_party_direction;
    g_champions[moved].attributes |= CHAMPION_ATTR_ICON;
    if (cell != from) {
        int16_t other = champion_get_index_in_cell(NORMALIZE(cell + g_party_direction));
        if (other < 0) {
            ui_fill_zone(&screen, (int16_t)(ZONE_CHAMPION_ICON_FIRST + from), COLOR_BLACK);
        } else {
            g_champions[other].cell = (uint8_t)NORMALIZE(from + g_party_direction);
            g_champions[other].attributes |= CHAMPION_ATTR_ICON;
            champion_draw_state((uint16_t)other);
        }
        g_champions[moved].cell = (uint8_t)NORMALIZE(cell + g_party_direction);
    }
    champion_draw_state((uint16_t)moved);
}

/* The leader takes the top object of a pile in view cell 0-3 or the alcove. A creature in a
   front cell protects the objects under it. */
static void grab_object(int16_t pile)
{
    int16_t x = g_party_map_x, y = g_party_map_y;
    if (g_leader_index == -1) return;
    if (pile >= VIEW_CELL_FRONT_FIRST) {
        front_square(&x, &y);
        uint16_t group = group_get_thing(x, y);
        if (group != THING_ENDOFLIST && !move_is_levitating(group) &&
            group_get_creature_ordinal_in_cell(group, (uint16_t)NORMALIZE(pile + g_party_direction)))
            return;
    }
    uint16_t thing = g_pile_top_objects[pile];
    if (object_get_icon_index(thing) != -1) {
        move_get_move_result(thing, x, y, -1, 0);
        champion_put_object_in_leader_hand(thing);
    }
    g_stop_waiting_for_input = 1;
}

/* The leader puts the hand object on the floor of view cell 0-3 or into the alcove ahead. Bones
   on a vi altar start the rebirth of their champion. */
static void drop_object(int16_t pile)
{
    int16_t x = g_party_map_x, y = g_party_map_y, alcove = pile == VIEW_CLICK_BOX_ALCOVE;
    if (g_leader_index == -1) return;
    if (alcove) pile = VIEW_CELL_FRONT_FIRST;
    if (pile >= VIEW_CELL_FRONT_FIRST) front_square(&x, &y);
    int16_t cell = NORMALIZE(pile + g_party_direction);
    uint16_t thing = champion_get_object_removed_from_leader_hand();
    move_get_move_result(THING_WITH_CELL(thing, cell), -1, 0, x, y);
    if (alcove && g_facing_vi_altar && object_get_icon_index(thing) == ICON_BONES) {
        TimelineEvent rebirth = { .map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + 1), .type = EVENT_VI_ALTAR_REBIRTH,
                                  .priority = (uint8_t)junk_charges(thing), .b = EVENT_SQUARE(x, y),
                                  .c = EVENT_CELL_EFFECT(cell, VI_ALTAR_REBIRTH_EFFECT) };
        timeline_add_event(&rebirth);
    }
    g_stop_waiting_for_input = 1;
}

/* The leader hand touches the wall ahead: wall sensors and mirrors. */
static void touch_front_wall(void)
{
    int16_t x, y;
    front_square(&x, &y);
    if (x < 0 || x >= g_current_map_width || y < 0 || y >= g_current_map_height) return;
    int16_t candidate = g_candidate_champion_ordinal;
    /* the wall side that faces the party */
    g_stop_waiting_for_input = sensor_is_triggered_by_click_on_wall(x, y, (uint16_t)NORMALIZE(g_party_direction + 2));
    if (!candidate && g_candidate_champion_ordinal) show_candidate();
}

/* An empty hand on the square ahead: a thud, or the hand goes into an imaginary wall while the
   button is down. */
static void touch_front_square_empty_handed(int16_t imaginary_wall, int16_t x, int16_t y)
{
    if (!g_party_champion_count) return;
    if (!imaginary_wall) sound_request_play(SOUND_WOODEN_THUD, x, y, SOUND_MODE_NOW);
    else if (left_button_down) pressing_fake_wall = g_mouse_pointer_hidden = 1;
    g_stop_waiting_for_input = 1;
}

/* A flask or a waterskin in the hand fills at a fountain. */
static void fill_at_fountain(void)
{
    uint16_t thing = g_leader_hand_object;
    int16_t icon = object_get_icon_index(thing);
    uint16_t weight = get_object_weight(thing);
    if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL) junk_set_charges(thing, FILLED_WATERSKIN_CHARGES);
    else if (icon == ICON_EMPTY_FLASK) potion_set_type(thing, POTION_WATER);
    else return;
    g_champions[g_leader_index].load += get_object_weight(thing) - weight;
    g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
}

/* A point in a clickable box of the view; x -1 marks an unused box. */
static int16_t in_box(const Box *b, int16_t x, int16_t y)
{
    return b->x != -1 && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}

/* The leader throws the hand object when the click is in the left or right throw zone. */
static int16_t is_leader_hand_object_thrown(int16_t x, int16_t y)
{
    int16_t zone = g_square_ahead_element == VIEW_ELEMENT_DOOR_FRONT ? ZONE_THROW_DOOR_LEFT : ZONE_THROW_LEFT;
    int16_t thrown = 0;
    if (ui_point_in_zone(zone, x, y))
        thrown = champion_is_leader_hand_object_thrown(0);
    else if (ui_point_in_zone((int16_t)(zone + 1), x, y))
        thrown = champion_is_leader_hand_object_thrown(1);
    if (thrown)
        g_stop_waiting_for_input = 1;
    return thrown;
}

/* Click in the dungeon view (x, y in viewport coordinates): door buttons, objects, the front wall. */
static void click_in_dungeon_view(int16_t x, int16_t y)
{
    int16_t fx, fy;
    const Box *wall_box = &g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL];
    front_square(&fx, &fy);
    if (g_square_ahead_element == VIEW_ELEMENT_DOOR_FRONT) {
        if (g_leader_index == -1) return;
        if (!g_leader_empty_handed && is_leader_hand_object_thrown(x, y)) return;
        if (g_leader_empty_handed && door_button_present(get_square_first_thing(fx, fy)) && in_box(wall_box, x, y)) {
            sound_request_play(SOUND_SWITCH, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
            sensor_add_event(EVENT_DOOR, fx, fy, 0, EFFECT_TOGGLE, (uint32_t)(g_game_time + 1));
            g_stop_waiting_for_input = 1;
            return;
        }
    }
    uint8_t square = get_square(fx, fy);
    int16_t imaginary_wall = SQUARE_TYPE(square) == SQUARE_FAKE_WALL && (square & FAKE_WALL_IMAGINARY) && !(square & FAKE_WALL_OPEN);
    if (g_leader_empty_handed) {
        for (int16_t i = 0; i < VIEW_CLICK_BOX_COUNT; i++) {
            if (!in_box(&g_dungeon_view_clickable_boxes[i], x, y)) continue;
            if (i != VIEW_CLICK_BOX_WALL) {
                grab_object(i);
                return;
            }
            if (g_facing_fountain && g_leader_index != -1) {
                g_champions[g_leader_index].water = FOUNTAIN_WATER;
                /* With the advanced features, every living champion drinks. */
                for (uint16_t i = 0; g_advanced_features && i < g_party_champion_count; i++)
                    if (g_champions[i].current_health) g_champions[i].water = FOUNTAIN_WATER;
                sound_request_play(SOUND_SWALLOW, g_party_map_x, g_party_map_y, SOUND_MODE_NOW);
            }
            if (!g_facing_alcove) touch_front_wall();
            return;
        }
        if (g_square_ahead_element != VIEW_ELEMENT_WALL) return;
        if (ui_point_in_zone(ZONE_THROW_LEFT, x, y) || ui_point_in_zone(ZONE_THROW_LEFT + 1, x, y))
            touch_front_square_empty_handed(imaginary_wall, fx, fy);
        return;
    }
    if ((g_square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall) && is_leader_hand_object_thrown(x, y)) return;
    int16_t cells = g_square_ahead_element != VIEW_ELEMENT_WALL || imaginary_wall ? VIEW_CELL_COUNT : VIEW_CELL_FRONT_FIRST;
    for (int16_t i = 0; i < cells; i++)
        if (ui_point_in_zone((int16_t)(ZONE_DROP_CELL_FIRST + i), x, y)) {
            drop_object(i);
            return;
        }
    if (cells == VIEW_CELL_COUNT || !in_box(wall_box, x, y)) return;
    if (g_facing_alcove) {
        drop_object(VIEW_CLICK_BOX_ALCOVE);
        return;
    }
    if (g_facing_fountain) fill_at_fountain();
    touch_front_wall();
}

/* The magic caster adds a rune when it has the mana. */
static void add_champion_symbol(int16_t symbol)
{
    Champion *c = &g_champions[g_magic_caster_champion_index];
    uint16_t step = c->symbol_step, cost = *tables_symbol_base_mana_cost(step, symbol);
    if (step) cost = (uint16_t)(cost * *tables_symbol_mana_cost_multiplier((uint8_t)c->symbols[0] - SPELL_SYMBOL_FIRST)) >> MANA_MULTIPLIER_SHIFT;
    if (cost > (uint16_t)c->current_mana) return;
    c->current_mana -= cost;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    c->symbols[step] = (char)(SPELL_SYMBOL_FIRST + step * SYMBOL_COUNT + symbol);
    c->symbols[step + 1] = 0;
    c->symbol_step = (uint8_t)NORMALIZE(step + 1);
    menus_draw_available_symbols(c->symbol_step);
    menus_draw_champion_symbols((uint16_t)g_magic_caster_champion_index);
    champion_draw_state((uint16_t)g_magic_caster_champion_index);
}

/* The magic caster removes the last rune; the mana stays spent. */
static void delete_champion_symbol(void)
{
    Champion *c = &g_champions[g_magic_caster_champion_index];
    if (!c->symbols[0]) return;
    c->symbol_step = (uint8_t)NORMALIZE(c->symbol_step - 1);
    c->symbols[c->symbol_step] = 0;
    menus_draw_available_symbols(c->symbol_step);
    menus_draw_champion_symbols((uint16_t)g_magic_caster_champion_index);
}

/* Click in the spell area: a caster tab, a rune, delete, or cast. */
static void click_in_spell_area(int16_t x, int16_t y)
{
    int16_t command = ui_command_from_mouse(UI_INPUT_SPELL_AREA, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_CHANGE_MAGIC_CASTER) {
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            if (ui_point_in_zone((int16_t)(ZONE_SPELL_CASTER_TAB_FIRST + g_magic_caster_champion_index * SPELL_TAB_STEP + i), x, y)) {
                menus_set_magic_caster((int16_t)i);
                return;
            }
    } else if (command == COMMAND_CAST_SPELL) {
        if (!g_champions[g_magic_caster_champion_index].symbols[0]) return;
        highlight_zone(ZONE_SPELL_CAST_BOX);
        g_stop_waiting_for_input = menus_get_click_on_spell_cast_result();
    } else if (command >= COMMAND_SPELL_SYMBOL_FIRST && command < COMMAND_SPELL_SYMBOL_FIRST + SYMBOL_COUNT) {
        highlight_zone((int16_t)(ZONE_SPELL_SYMBOL_BOX_FIRST + command - COMMAND_SPELL_SYMBOL_FIRST));
        add_champion_symbol(command - COMMAND_SPELL_SYMBOL_FIRST);
    } else if (command == COMMAND_DELETE_SYMBOL) {
        highlight_zone(ZONE_SPELL_DELETE_BOX);
        delete_champion_symbol();
    }
}

/* An action of the menu (or pass, -1) ends the menu of the acting champion. Returns 1 when the
   action took place. */
static int16_t trigger_action(int16_t list_index)
{
    if (list_index >= 0) return menus_did_click_trigger_action(list_index);
    menus_clear_acting_champion();
    return 0;
}

/* Click in the action area: choose the acting champion, then an action or pass. */
static void click_in_action_area(int16_t x, int16_t y)
{
    if (!g_acting_champion_ordinal) {
        if (!g_action_area_contains_icons) return;
        int16_t command = ui_command_from_mouse(UI_INPUT_ACTION_ICONS, x, y, CLICK_LEFT_PRESS);
        int16_t index = command - COMMAND_SELECT_ACTING_CHAMPION_FIRST;
        if (command && index >= 0 && index < (int16_t)g_party_champion_count) menus_set_acting_champion((uint16_t)index);
        return;
    }
    int16_t command = ui_command_from_mouse(UI_INPUT_ACTION_NAMES, x, y, CLICK_LEFT_PRESS);
    if (command == COMMAND_PASS) {
        highlight_zone(ZONE_ACTION_PASS_BOX);
        trigger_action(-1);
    } else if (command > COMMAND_PASS && command - COMMAND_PASS <= (int16_t)g_action_count) {
        highlight_zone((int16_t)(ZONE_ACTION_NAME_BOX_FIRST + command - COMMAND_ACTION_FIRST));
        g_stop_waiting_for_input = trigger_action(command - COMMAND_ACTION_FIRST);
    }
}

/* Draws an object icon in a zone of the viewport. */
static void draw_viewport_icon(int16_t icon, int16_t zone)
{
    uint8_t px[UI_ICON_SIZE * UI_ICON_SIZE];
    Bitmap bm = { UI_ICON_SIZE, UI_ICON_SIZE, px };
    ui_extract_icon(icon, &bm);
    ui_blit_zone(ui_viewport(), &bm, zone, COLOR_NONE);
}

/* While the left button stays down on the eye, the panel shows the skills and statistics, or the
   description of the object in the hand. */
static void click_on_eye(void)
{
    if (!left_button_down) return;
    pressing_eye = g_mouse_pointer_hidden = 1;
    command_discard_all_input();
    draw_viewport_icon(ICON_EYE_LOOKING, ZONE_EYE);
    draw_panel_eye();
    ui_draw_viewport();
}

/* The left button release ends a press on the eye, the mouth, or an imaginary wall. */
static void left_release(void)
{
    if (pressing_eye) {
        pressing_eye = 0;
        draw_viewport_icon(ICON_EYE, ZONE_EYE);
        draw_panel();
        ui_draw_viewport();
    }
    pressing_mouth = pressing_fake_wall = g_mouse_pointer_hidden = 0;
}

/* The party sleeps: the game runs at full speed and only the wake up input is active. */
static void go_to_sleep(void)
{
    if (g_candidate_champion_ordinal) return;
    if (g_inventory_champion_ordinal) inventory_toggle(INVENTORY_CLOSE);
    game_freeze();
    g_party_is_sleeping = 1;
    draw_viewport_message(TEXT_WAKE_UP);
    g_wait_for_input_max_frames = 0;
    set_input((InputTables){ UI_INPUT_SLEEPING, NO_TABLE, KEYS_SLEEPING, 0 });
    command_discard_all_input();
}

/* The party wakes up by a command or by damage: game speed and input return. */
void command_wake_up(void)
{
    if (primary_mouse != UI_INPUT_SLEEPING) return;
    champion_wake_up();
    g_stop_waiting_for_input = 1;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    command_set_game_input();
    command_discard_all_input();
}

/* The game time stops; only the unfreeze input is active. */
static void freeze_game(void)
{
    g_game_time_ticking = 0;
    game_freeze();
    draw_viewport_message(TEXT_GAME_FROZEN);
    frozen_input = current_input();
    set_input((InputTables){ UI_INPUT_FROZEN, NO_TABLE, KEYS_FROZEN, 0 });
    command_discard_all_input();
}

/* The game time runs again with the input of before the freeze. */
static void unfreeze_game(void)
{
    g_game_time_ticking = 1;
    game_unfreeze();
    set_input(frozen_input);
    command_discard_all_input();
}

/* A movement command: the arrow lights up; a step that does not move the party ends the input wait. */
static void move_or_turn(int16_t command)
{
    int16_t x = g_party_map_x, y = g_party_map_y, map = g_party_map_index;
    g_stop_waiting_for_input = 1;
    highlight_zone((int16_t)(ZONE_MOVEMENT_ARROW_BEFORE_FIRST + command));
    if (command <= COMMAND_TURN_RIGHT) {
        command_turn_party(command);
        champion_draw_all_states(CHAMPION_ATTR_ICON);
        return;
    }
    command_move_party(command);
    if (x == g_party_map_x && y == g_party_map_y && map == g_party_map_index && g_new_party_map_index == -1) {
        command_discard_all_input();
        g_stop_waiting_for_input = 0;
    }
}

/* Commands that do not run while the eye or the mouth is pressed. */
static void process_other_command(int16_t command)
{
    if (pressing_eye || pressing_mouth) return;
    switch (command) {
    case COMMAND_SLEEP: go_to_sleep(); break;
    case COMMAND_WAKE_UP: command_wake_up(); break;
    case COMMAND_FREEZE_GAME: freeze_game(); break;
    case COMMAND_UNFREEZE_GAME: unfreeze_game(); break;
    case COMMAND_ENTRANCE_ENTER: case COMMAND_ENTRANCE_ENTER_DUNGEON_B: g_new_game = ENTRANCE_NEW_GAME; break;
    case COMMAND_ENTRANCE_RESUME: g_new_game = ENTRANCE_RESUME; break;
    case COMMAND_SAVE_GAME: save_game(); break;
    case COMMAND_DIALOG_CHOICE_FIRST: case COMMAND_DIALOG_CHOICE_FIRST + 1: case COMMAND_DIALOG_CHOICE_FIRST + 2:
    case COMMAND_DIALOG_CHOICE_FIRST + 3: g_dialog_choice = (int16_t)(command - COMMAND_DIALOG_CHOICE_FIRST + 1); break;
    case COMMAND_RESTART_GAME: g_restart_requested = 1; break;
    case COMMAND_QUIT: g_quit_requested = 1; break;
    default: break; /* ponytail: credits (203) and name entry are not ported yet */
    }
}

/* A step waits while the party cannot move, or while a projectile blocks this direction. */
static int movement_blocked(int16_t command)
{
    if (command < COMMAND_MOVE_FORWARD || command > COMMAND_MOVE_LEFT) return 0;
    if (g_disabled_movement_ticks) return 1;
    return g_projectile_disabled_movement_ticks &&
           NORMALIZE(command + g_party_direction - COMMAND_MOVE_FORWARD) == g_last_projectile_disabled_movement_direction;
}

/* Takes the first waiting command and runs it. A movement command waits while the party cannot
   move. Returns 1 when it ran a command. */
int command_process_queue(void)
{
    if (!queue_count) return 0;
    QueuedCommand q = queue[queue_first];
    int16_t command = q.command, x = q.x, y = q.y;
    if (movement_blocked(command)) return 0;
    queue_first = (queue_first + 1) % QUEUE_SIZE;
    queue_count--;
    if (command == COMMAND_LEFT_RELEASE) left_release();
    else if (command >= COMMAND_TURN_LEFT && command <= COMMAND_MOVE_LEFT) move_or_turn(command);
    else if (command >= COMMAND_CLICK_STATUS_BOX_FIRST && command < COMMAND_CLICK_STATUS_BOX_FIRST + CHAMPION_MAX_COUNT) {
        if (command - COMMAND_CLICK_STATUS_BOX_FIRST < (int16_t)g_party_champion_count && !g_candidate_champion_ordinal)
            click_in_status_box(command - COMMAND_CLICK_STATUS_BOX_FIRST, x, y);
    } else if (command >= COMMAND_CLICK_CHAMPION_ICON_FIRST && command <= COMMAND_CLICK_CHAMPION_ICON_LAST) {
        click_on_champion_icon(command - COMMAND_CLICK_CHAMPION_ICON_FIRST);
    } else if (command == COMMAND_RIGHT_RELEASE) {
        dragged_icon_cell_plus_1 = 0;
    } else if (command >= COMMAND_CLICK_SLOT_BOX_FIRST && command <= COMMAND_CLICK_SLOT_BOX_LAST) {
        if (g_leader_index != -1) champion_click_on_slot_box((uint16_t)(command - COMMAND_CLICK_SLOT_BOX_FIRST));
    } else if (command >= COMMAND_TOGGLE_INVENTORY_FIRST && command <= COMMAND_CLOSE_INVENTORY) {
        int16_t index = command - COMMAND_TOGGLE_INVENTORY_FIRST;
        if ((index == INVENTORY_CLOSE || index < (int16_t)g_party_champion_count) && !g_candidate_champion_ordinal) inventory_toggle(index);
    } else if (command == COMMAND_TOGGLE_INVENTORY_LEADER) {
        if (g_leader_index != -1) inventory_toggle(g_leader_index);
    } else if (command == COMMAND_CLICK_SPELL_AREA) {
        if (!g_candidate_champion_ordinal && g_magic_caster_champion_index != -1) click_in_spell_area(x, y);
    } else if (command == COMMAND_CLICK_ACTION_AREA) {
        if (!g_candidate_champion_ordinal) click_in_action_area(x, y);
    } else if (command == COMMAND_CLICK_MOUTH) {
        process_click_on_mouth();
    } else if (command == COMMAND_CLICK_EYE) {
        click_on_eye();
    } else if (command == COMMAND_CLICK_DUNGEON_VIEW) {
        click_in_dungeon_view((int16_t)(x - kaos_s16(VIEWPORT_ORIGIN)), (int16_t)(y - kaos_s16(VIEWPORT_ORIGIN + 2)));
    } else if (command == COMMAND_CLICK_PANEL) {
        if (g_candidate_champion_ordinal) click_in_candidate_panel(x, y);
        else panel_click_in_chest(x, y);
    } else {
        process_other_command(command);
    }
    return 1;
}

/* A new or loaded game starts without a pressed eye, mouth or wall, a highlight, a sleeping input
   speed, or waiting commands; the game input tables are active. */
void command_reset(void)
{
    left_button_down = pressing_eye = pressing_mouth = pressing_fake_wall = 0;
    dragged_icon_cell_plus_1 = highlight_enabled = 0;
    g_mouse_pointer_hidden = g_stop_waiting_for_input = g_game_frozen = 0;
    g_wait_for_input_max_frames = WAIT_FOR_INPUT_FRAMES;
    queue_first = queue_count = 0;
    g_input_received = 0;
    g_last_key = 0;
    command_set_game_input();
}
