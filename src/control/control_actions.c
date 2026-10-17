/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/bytes.h"
#include "data/ids.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/party.h"
#include "ui/advanced.h"
#include "ui/command.h"
#include "ui/gameloop.h"
#include "ui/panel.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include "control/control_internal.h"
#include "control/request.h"
#include "control/turn.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Sizes of the action arguments. */
#define SLOT_REQUEST_SIZE 64
#define CHOICE_SIZE       16

/* Input as the player gives it: clicks and keys go through the command queue. */

static void click(int16_t x, int16_t y)
{
    command_process_click(x, y, CLICK_LEFT_PRESS);
    command_process_click(x, y, CLICK_LEFT_RELEASE);
    gameloop_run_commands();
}

static void press_key(uint16_t key)
{
    command_process_key(key);
    gameloop_run_commands();
}

static void get_viewport_origin(int16_t *x, int16_t *y)
{
    *x = tables_get_viewport_x();
    *y = tables_get_viewport_y();
}

/* A screen point inside box b (moved by dx, dy) where the table gives command, or 0. */
static bool find_in_box(int16_t table, int16_t command, Box b, int16_t dx, int16_t dy, int16_t *x, int16_t *y)
{
    int16_t cx = b.x + dx + b.w / 2;
    int16_t cy = b.y + dy + b.h / 2;
    if (ui_command_from_mouse(table, cx, cy, UI_BUTTON_LEFT) == command) {
        *x = cx;
        *y = cy;
        return true;
    }
    for (int16_t py = b.y + dy; py < b.y + dy + b.h; py++)
        for (int16_t px = b.x + dx; px < b.x + dx + b.w; px++)
            if (ui_command_from_mouse(table, px, py, UI_BUTTON_LEFT) == command) {
                *x = px;
                *y = py;
                return true;
            }
    return false;
}

/* The screen point that a mouse input table maps to command: a fixed box, or a zone on the screen,
   in the viewport, or in the centered viewport. */
static bool find_click(int16_t table, int16_t command, int16_t *x, int16_t *y)
{
    const Bitmap *viewport = ui_get_viewport();
    int16_t ox, oy;
    get_viewport_origin(&ox, &oy);
    const int16_t offsets[3][2] = {
        {0,                            0                           },
        {ox,                           oy                          },
        {(SCREEN_W - viewport->w) / 2, (SCREEN_H - viewport->h) / 2}
    };
    MouseInput mouse_input;
    for (int16_t entry = 0; tables_get_mouse_input(table, entry, &mouse_input); entry++) {
        if (mouse_input.command != command)
            continue;
        int16_t x1 = mouse_input.x1;
        int16_t x2 = mouse_input.x2;
        Box box = {x1, mouse_input.y1, x2 - x1 + 1, 0};
        if (x1 >= 0) {
            box.h = mouse_input.y2 - box.y + 1;
            if (find_in_box(table, command, box, 0, 0, x, y))
                return true;
            continue;
        }
        if (!ui_zone_box(x2, &box))
            continue;
        for (int k = 0; k < 3; k++)
            if (find_in_box(table, command, box, offsets[k][0], offsets[k][1], x, y))
                return true;
    }
    return false;
}

static bool click_command(int16_t table, int16_t command)
{
    int16_t x, y;
    if (!find_click(table, command, &x, &y))
        return false;
    click(x, y);
    return true;
}

/* A click in the middle of a box of the dungeon view (viewport coordinates). */
static void click_view_box(const Box *b)
{
    int16_t ox, oy;
    get_viewport_origin(&ox, &oy);
    click(ox + b->x + b->w / 2, oy + b->y + b->h / 2);
}

/* Actions. Each returns NULL when it gave its input, or the reason why it cannot. */

typedef const char *(*ActionFn)(const char *request);

static bool parse_champion_arg(const char *request, int16_t *champion)
{
    int64_t c = request_get_int(request, "champion", -1);
    if (c < 0 || c >= g_party_champion_count || !g_champions[(int16_t)c].current_health)
        return false;
    *champion = (int16_t)c;
    return true;
}

static const char *need_movement_input(void)
{
    if (g_session.inventory_champion_index >= 0)
        return "close the inventory first (close_inventory)";
    return NULL;
}

static const char *parse_movement(uint16_t key)
{
    const char *error = need_movement_input();
    if (!error)
        press_key(key);
    return error;
}

/* A step remembers where it goes, so that the turn result can tell a bump or a move by the dungeon. */
static const char *give_step(uint16_t key, int16_t forward, int16_t right)
{
    turn_set_step(forward, right);
    return parse_movement(key);
}

static const char *give_rotation(uint16_t key, int16_t rotation)
{
    turn_set_rotation(rotation);
    return parse_movement(key);
}

static const char *step_forward(const char *args)
{
    (void)args;
    return give_step(KEY_UP, 1, 0);
}
static const char *step_back(const char *args)
{
    (void)args;
    return give_step(KEY_DOWN, -1, 0);
}
static const char *step_left(const char *args)
{
    (void)args;
    return give_step(KEY_LEFT, 0, -1);
}
static const char *step_right(const char *args)
{
    (void)args;
    return give_step(KEY_RIGHT, 0, 1);
}
static const char *rotate_left(const char *args)
{
    (void)args;
    return give_rotation(KEY_DEL, -1);
}
static const char *rotate_right(const char *args)
{
    (void)args;
    return give_rotation(KEY_HELP, 1);
}

static const char *click_wall_ahead(const char *args)
{
    (void)args;
    const Box *b = &view_get_result()->clickable_boxes[VIEW_CLICK_BOX_WALL];
    if (g_session.inventory_champion_index >= 0)
        return "close the inventory first (close_inventory)";
    if (b->x == -1)
        return "nothing to click on the wall ahead";
    click_view_box(b);
    return NULL;
}

static const char *take_object(const char *args)
{
    int64_t cell = request_get_int(args, "cell", -1);
    if (g_session.inventory_champion_index >= 0)
        return "close the inventory first (close_inventory)";
    if (cell < 0 || cell > VIEW_CLICK_BOX_ALCOVE)
        return "cell must be 0-4 (see get_view piles)";
    if (!g_leader_empty_handed)
        return "the leader hand is not empty: drop or store the object first";
    const Box *box = &view_get_result()->clickable_boxes[(int16_t)cell];
    if (box->x == -1)
        return "no object at this cell";
    click_view_box(box);
    return NULL;
}

static const char *drop_object(const char *args)
{
    int64_t cell = request_get_int(args, "cell", 0);
    Box b;
    const ViewResult *view = view_get_result();
    if (g_session.inventory_champion_index >= 0)
        return "close the inventory first (close_inventory)";
    if (g_leader_empty_handed)
        return "the leader hand is empty";
    if (cell == VIEW_CLICK_BOX_ALCOVE) {
        if (view->square_ahead_element != VIEW_ELEMENT_WALL || !view->facing_alcove)
            return "no alcove ahead";
        click_view_box(&view->clickable_boxes[VIEW_CLICK_BOX_WALL]);
        return NULL;
    }
    if (cell < 0 || cell > VIEW_CLICK_BOX_ALCOVE || !ui_zone_box(ZONE_DROP_CELL_FIRST + cell, &b))
        return "cell must be 0-4";
    click_view_box(&b);
    return NULL;
}

static const char *open_inventory(const char *args)
{
    int16_t champion_index;
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_session.inventory_champion_index != champion_index)
        press_key(command_champion_key(champion_index));
    return NULL;
}

static const char *close_inventory(const char *args)
{
    (void)args;
    if (g_session.inventory_champion_index >= 0)
        press_key(command_champion_key(g_session.inventory_champion_index));
    return NULL;
}

/* A slot box click swaps the leader hand object with the slot: put and take are the same click.
   The slots of the open chest are boxes of the chest panel. */
static const char *click_slot(const char *args)
{
    int16_t champion_index;
    int64_t slot = request_get_int(args, "slot", -1);
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_leader_index == -1)
        return "no leader";
    if (g_session.inventory_champion_index == champion_index) {
        if (slot < 0 || slot >= SLOT_CHEST_FIRST + (g_open_chest != THING_NONE ? SLOT_CHEST_COUNT : 0))
            return "slot out of range";
        int16_t table = slot >= SLOT_CHEST_FIRST ? TABLES_MOUSE_CHEST : TABLES_MOUSE_INVENTORY;
        if (!click_command(table, COMMAND_CLICK_SLOT_BOX_FIRST + UI_SLOT_BOX_INVENTORY_FIRST + slot))
            return "slot not found";
        return NULL;
    }
    if (slot != SLOT_HAND_READY && slot != SLOT_HAND_ACTION)
        return "open the inventory of this champion first (open_inventory)";
    if (!click_command(TABLES_MOUSE_STATUS_BOX, COMMAND_CLICK_SLOT_BOX_FIRST + 2 * champion_index + slot))
        return "slot not found";
    return NULL;
}

/* Holds the eye down, reads the panel text of the leader hand object, and releases the eye. */
static void press_eye(int16_t x, int16_t y)
{
    const char *label;
    uint16_t value;
    char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT + 1][PANEL_LINE_SIZE];
    command_process_click(x, y, CLICK_LEFT_PRESS);
    gameloop_run_commands();
    int16_t count = panel_format_object_text(g_leader_hand_object, name, lines);
    if (g_advanced_features && panel_get_object_meter(g_leader_hand_object, &label, &value))
        snprintf(lines[count++], PANEL_LINE_SIZE, "%s %u", label, value);
    turn_set_look(name, (const char (*)[PANEL_LINE_SIZE])lines, count);
    command_process_click(x, y, CLICK_LEFT_RELEASE);
    gameloop_run_commands();
}

/* The eye on the leader hand object; with a slot, the hand takes the slot object first and puts it back after. */
static const char *look(const char *args)
{
    int16_t x, y;
    int64_t slot = request_get_int(args, "slot", -1);
    char request[SLOT_REQUEST_SIZE];
    if (g_session.inventory_champion_index < 0)
        return "open an inventory first (open_inventory): the eye is in the inventory";
    if (!find_click(TABLES_MOUSE_INVENTORY, COMMAND_CLICK_EYE, &x, &y))
        return "eye not found";
    if (slot < 0) {
        if (g_leader_empty_handed)
            return "the leader hand is empty: take an object, or give champion and slot";
        press_eye(x, y);
        return NULL;
    }
    if (!g_leader_empty_handed)
        return "the leader hand must be empty to look at a slot";
    snprintf(request, sizeof request, "\"champion\":%" PRId64 ",\"slot\":%" PRId64,
             request_get_int(args, "champion", g_session.inventory_champion_index), slot);
    const char *error = click_slot(request);
    if (error)
        return error;
    if (g_leader_empty_handed)
        return "no object in this slot";
    press_eye(x, y);
    click_slot(request);
    return NULL;
}

/* Clicks the icon of champion a, then the icon of champion b: the two champions swap cells. */
static const char *swap_champions(const char *args)
{
    int64_t pair[2] = {request_get_int(args, "a", -1), request_get_int(args, "b", -1)};
    for (int k = 0; k < 2; k++)
        if (pair[k] < 0 || pair[k] >= g_party_champion_count || !g_champions[(int16_t)pair[k]].current_health)
            return "a and b must be living champions";
    if (pair[0] == pair[1])
        return "a and b must be different champions";
    for (int k = 0; k < 2; k++)
        if (!click_command(TABLES_MOUSE_INTERFACE,
                           COMMAND_CLICK_CHAMPION_ICON_FIRST +
                               NORMALIZE(g_champions[(int16_t)pair[k]].cell - g_party_direction)))
            return "champion icon not found";
    return NULL;
}

static const char *use_mouth(const char *args)
{
    (void)args;
    if (g_session.inventory_champion_index < 0)
        return "open the inventory of the champion who eats or drinks first";
    if (g_leader_empty_handed)
        return "take the food or drink in the leader hand first";
    click_command(TABLES_MOUSE_INVENTORY, COMMAND_CLICK_MOUTH);
    return NULL;
}

static const char *choose_action(const char *args)
{
    int16_t champion_index;
    int64_t index = request_get_int(args, "index", -1);
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_session.acting_champion_index >= 0 && g_session.acting_champion_index != champion_index)
        return "another champion's action menu is open: use pass first";
    if (g_session.acting_champion_index < 0) {
        click_command(TABLES_MOUSE_ACTION_ICONS, COMMAND_SELECT_ACTING_CHAMPION_FIRST + champion_index);
        if (g_session.acting_champion_index != champion_index)
            return "this champion cannot act now (action disabled, or no actions)";
    }
    if (index < 0 || index >= g_action_list.count)
        return "index out of range of the action menu (see state action_menu)";
    g_session.action_damage = 0;
    click_command(TABLES_MOUSE_ACTION_NAMES, COMMAND_ACTION_FIRST + index);
    turn_add_damage(g_session.action_damage);
    return NULL;
}

static const char *pass(const char *args)
{
    (void)args;
    if (g_session.acting_champion_index >= 0)
        click_command(TABLES_MOUSE_ACTION_NAMES, COMMAND_PASS);
    return NULL;
}

static const char *set_caster(const char *args)
{
    int16_t champion_index;
    Box b;
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_magic_caster_champion_index == -1)
        return "no spell area";
    if (!ui_zone_box(ZONE_SPELL_CASTER_TAB_FIRST + g_magic_caster_champion_index * SPELL_TAB_STEP + champion_index, &b))
        return "caster tab not found";
    click(b.x + b.w / 2, b.y + b.h / 2);
    return NULL;
}

static const char *click_spell_area(int16_t command)
{
    if (g_magic_caster_champion_index == -1)
        return "no spell area";
    click_command(TABLES_MOUSE_SPELL_AREA, command);
    return NULL;
}

static const char *add_rune(const char *args)
{
    int64_t symbol = request_get_int(args, "symbol", -1);
    if (symbol < 0 || symbol >= SYMBOL_COUNT)
        return "symbol must be 0-5";
    if (g_magic_caster_champion_index == -1)
        return "no spell area";
    size_t before = strlen(g_champions[g_magic_caster_champion_index].symbols);
    click_spell_area(COMMAND_SPELL_SYMBOL_FIRST + symbol);
    /* the game ignores a rune that costs more mana than the caster has */
    return strlen(g_champions[g_magic_caster_champion_index].symbols) == before
               ? "the rune was not added: not enough mana"
               : NULL;
}

static const char *delete_rune(const char *args)
{
    (void)args;
    return click_spell_area(COMMAND_DELETE_SYMBOL);
}
static const char *cast(const char *args)
{
    (void)args;
    return click_spell_area(COMMAND_CAST_SPELL);
}

static const char *answer_mirror(const char *args)
{
    static const char *const choices[3] = {"resurrect", "reincarnate", "cancel"};
    char choice[CHOICE_SIZE];
    if (g_session.candidate_champion_index < 0)
        return "no candidate: click a champion mirror first (click_wall_ahead)";
    if (!g_leader_empty_handed)
        return "the leader hand must be empty";
    if (!request_get_string(args, "choice", choice, sizeof choice))
        return "choice must be resurrect, reincarnate, or cancel";
    for (int k = 0; k < 3; k++)
        if (!strcmp(choice, choices[k])) {
            click_command(TABLES_MOUSE_RESURRECT, PANEL_RESURRECT + k);
            return NULL;
        }
    return "choice must be resurrect, reincarnate, or cancel";
}

static const char *set_leader(const char *args)
{
    int16_t champion_index;
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    click_command(TABLES_MOUSE_STATUS_BOX, COMMAND_SET_LEADER_FIRST + champion_index);
    return NULL;
}

static const char *sleep_party(const char *args)
{
    (void)args;
    if (g_leader_index == -1)
        return "no leader";
    if (g_session.inventory_champion_index < 0)
        press_key(command_champion_key(g_leader_index));
    click_command(TABLES_MOUSE_INVENTORY, COMMAND_SLEEP);
    return NULL;
}

static const char *wake(const char *args)
{
    (void)args;
    if (g_session.party_sleeping)
        click_command(TABLES_MOUSE_SLEEPING, COMMAND_WAKE_UP);
    return NULL;
}

static const struct {
    const char *name;
    ActionFn run;
} actions[] = {
    {"move_forward",     step_forward    },
    {"move_back",        step_back       },
    {"move_left",        step_left       },
    {"move_right",       step_right      },
    {"turn_left",        rotate_left     },
    {"turn_right",       rotate_right    },
    {"click_wall_ahead", click_wall_ahead},
    {"take_object",      take_object     },
    {"drop_object",      drop_object     },
    {"open_inventory",   open_inventory  },
    {"close_inventory",  close_inventory },
    {"put_in_slot",      click_slot      },
    {"take_from_slot",   click_slot      },
    {"eat",              use_mouth       },
    {"drink",            use_mouth       },
    {"choose_action",    choose_action   },
    {"pass",             pass            },
    {"set_caster",       set_caster      },
    {"add_rune",         add_rune        },
    {"delete_rune",      delete_rune     },
    {"cast",             cast            },
    {"recruit",          answer_mirror   },
    {"set_leader",       set_leader      },
    {"sleep",            sleep_party     },
    {"wake",             wake            },
    {"swap_champions",   swap_champions  },
    {"look",             look            },
};

const char *control_run_action(const char *name, const char *request)
{
    for (int k = 0; k < ARRAY_COUNT(actions); k++) {
        if (strcmp(name, actions[k].name))
            continue;
        if (g_session.candidate_champion_index >= 0 && strcmp(name, "recruit"))
            return "a candidate is shown: answer with recruit first";
        if (g_session.party_sleeping && strcmp(name, "wake"))
            return "the party sleeps: use wake or wait";
        turn_begin_action();
        return actions[k].run(request);
    }
    return "unknown action";
}
