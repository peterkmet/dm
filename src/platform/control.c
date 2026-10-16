/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/control.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/crc32.h"
#include "data/ids.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/savegame.h"
#include "engine/timeline_process.h"
#include "ui/advanced.h"
#include "ui/command.h"
#include "ui/gameloop.h"
#include "ui/startend.h"
#include "ui/ui.h"
#include "ui/video.h"
#include "ui/view.h"
#include "platform/state_json.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define INPUT_SIZE               65536 /* longest request line */
#define TURN_MIN_TICKS           2     /* an action shows its result, then the action area returns */
#define TURN_MAX_TICKS           64
#define WAIT_MAX_TICKS           5000
#define FRAMES_PER_TICK_LIMIT    16 /* a frozen game ends a turn after this many frames per tick */
#define ACTION_NAME_SIZE         32
#define WITNESSED_EXPLOSIONS_MAX 32
#define WITNESSED_GROUPS_MAX     16
#define KEY_PATTERN_SIZE         64 /* a quoted request key */
#define SLOT_REQUEST_SIZE        64
#define CHOICE_SIZE              16
#define CMD_SIZE                 32

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL /* a closed client does not raise SIGPIPE (Linux) */
#else
#define SEND_FLAGS 0 /* SO_NOSIGPIPE does the same (macOS) */
#endif

static int listen_fd = -1;
static int client_fd = -1;
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static char input[INPUT_SIZE];
static size_t input_length;

static struct {
    bool pending, wait_only;
    uint32_t start_time;
    int64_t frames, min_ticks, max_ticks;
    bool has_damage;
    int16_t damage;
    bool moving; /* the action was a step forward and to the right */
    int16_t step_forward, step_right;
    int16_t rotation;             /* the action was a turn: 1 right, -1 left */
    int16_t map, x, y, direction; /* the party before the action */
    int teleports;                /* g_party_teleport_count before the action */
    const char *stop;             /* wait: the news that ended it early, or NULL */
} turn;

/* A batch: act_many runs a list of actions; each step is one turn. */
#define BATCH_DAMAGE_MAX 64
static struct {
    bool active;
    int ran;
    int16_t damages[BATCH_DAMAGE_MAX]; /* the action damage of each choose_action, in order */
    int damage_count;
    char *actions; /* a copy of the request; next is the rest of its actions array */
    const char *next;
    uint32_t start_time;
    const char *stop, *error;
} batch;

/* What the party knew before a turn: act_many and wait stop on news against it. */
static struct {
    int16_t health[CHAMPION_MAX_COUNT];
    uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX];
    int group_count;
    uint32_t message_expiry;
} seen;

/* What the party saw during a request, also when it was gone at the end: the explosions, and the creature
   groups with their count when first seen and when last seen. */
typedef struct {
    StateJsonSeenThing at;
    uint8_t type;
} WitnessedExplosion;
typedef struct {
    StateJsonSeenThing at;
    uint16_t creature;
    int16_t before, after;
    bool in_view, killed;
} WitnessedGroup;
static struct {
    int explosion_count, group_count;
    WitnessedExplosion explosions[WITNESSED_EXPLOSIONS_MAX];
    WitnessedGroup groups[WITNESSED_GROUPS_MAX];
    int16_t health[CHAMPION_MAX_COUNT]; /* the health of each champion at the start of the request */
    uint16_t champion_count;
} witnessed;

/* The eye panel text of a look action, for the turn result. */
static struct {
    bool shown;
    int16_t count;
    char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT + 1][PANEL_LINE_SIZE];
} look_text;

static bool load_failed; /* a damaged save file left no game to play */

/* The party died, or the game is won: the endgame ran and no turn can follow until load or new_game. */
static bool is_game_over(void)
{
    return g_party_dead || g_game_won;
}

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

/* Requests: a flat search for "key": value; nested args are found the same way. */

static const char *find_value(const char *line, const char *key)
{
    char pattern[KEY_PATTERN_SIZE];
    snprintf(pattern, sizeof pattern, "\"%s\"", key);
    const char *p = strstr(line, pattern);
    if (!p)
        return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':')
        p++;
    return p;
}

static bool get_string(const char *line, const char *key, char *out, size_t size)
{
    const char *p = find_value(line, key);
    size_t n = 0;
    if (!p || *p++ != '"')
        return false;
    while (*p && *p != '"' && n + 1 < size)
        out[n++] = *p++;
    out[n] = 0;
    return true;
}

static int64_t get_int(const char *line, const char *key, int64_t missing)
{
    const char *p = find_value(line, key);
    char *end;
    if (!p)
        return missing;
    int64_t value = strtoll(p, &end, DECIMAL_BASE);
    return end == p ? missing : value;
}

/* Actions. Each returns NULL when it gave its input, or the reason why it cannot. */

typedef const char *(*ActionFn)(const char *request);

static bool parse_champion_arg(const char *request, int16_t *champion)
{
    int64_t c = get_int(request, "champion", -1);
    if (c < 0 || c >= g_party_champion_count || !g_champions[c].current_health)
        return false;
    *champion = c;
    return true;
}

static const char *need_movement_input(void)
{
    if (g_inventory_champion_ordinal)
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
static const char *step(uint16_t key, int16_t forward, int16_t right)
{
    turn.moving = true;
    turn.step_forward = forward;
    turn.step_right = right;
    return parse_movement(key);
}

static const char *rotate(uint16_t key, int16_t rotation)
{
    turn.rotation = rotation;
    return parse_movement(key);
}

/* The step of the turn left the party on its square: a wall, a closed door, or a creature is in the way. */
static bool turn_bumped(void)
{
    return turn.moving && g_party_map_index == turn.map && g_party_map_x == turn.x && g_party_map_y == turn.y &&
           g_party_teleport_count == turn.teleports;
}

/* The party was turned or moved by something else than its action: a spinner, a teleporter, a pit, stairs. */
static const char *turn_displaced(void)
{
    int16_t x = turn.x;
    int16_t y = turn.y;
    if (turn.moving)
        game_update_map_coordinates_after_relative_movement(turn.direction, turn.step_forward, turn.step_right, &x, &y);
    bool stayed = g_party_map_x == turn.x && g_party_map_y == turn.y;
    if (g_party_map_index != turn.map || (!stayed && (g_party_map_x != x || g_party_map_y != y)))
        return "moved";
    if (stayed && g_party_teleport_count != turn.teleports)
        return "moved"; /* a teleporter sent the party back */
    if (g_party_direction != turn.direction && g_party_direction != NORMALIZE(turn.direction + turn.rotation))
        return "turned";
    return NULL;
}

static const char *move_forward(const char *args)
{
    (void)args;
    return step(KEY_UP, 1, 0);
}
static const char *move_back(const char *args)
{
    (void)args;
    return step(KEY_DOWN, -1, 0);
}
static const char *move_left(const char *args)
{
    (void)args;
    return step(KEY_LEFT, 0, -1);
}
static const char *move_right(const char *args)
{
    (void)args;
    return step(KEY_RIGHT, 0, 1);
}
static const char *turn_party_left(const char *args)
{
    (void)args;
    return rotate(KEY_DEL, -1);
}
static const char *turn_party_right(const char *args)
{
    (void)args;
    return rotate(KEY_HELP, 1);
}

static const char *click_wall_ahead(const char *args)
{
    (void)args;
    const Box *b = &g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL];
    if (g_inventory_champion_ordinal)
        return "close the inventory first (close_inventory)";
    if (b->x == -1)
        return "nothing to click on the wall ahead";
    click_view_box(b);
    return NULL;
}

static const char *take_object(const char *args)
{
    int64_t cell = get_int(args, "cell", -1);
    if (g_inventory_champion_ordinal)
        return "close the inventory first (close_inventory)";
    if (cell < 0 || cell > VIEW_CLICK_BOX_ALCOVE)
        return "cell must be 0-4 (see get_view piles)";
    if (!g_leader_empty_handed)
        return "the leader hand is not empty: drop or store the object first";
    if (g_dungeon_view_clickable_boxes[cell].x == -1)
        return "no object at this cell";
    click_view_box(&g_dungeon_view_clickable_boxes[cell]);
    return NULL;
}

static const char *drop_object(const char *args)
{
    int64_t cell = get_int(args, "cell", 0);
    Box b;
    if (g_inventory_champion_ordinal)
        return "close the inventory first (close_inventory)";
    if (g_leader_empty_handed)
        return "the leader hand is empty";
    if (cell == VIEW_CLICK_BOX_ALCOVE) {
        if (g_square_ahead_element != VIEW_ELEMENT_WALL || !g_facing_alcove)
            return "no alcove ahead";
        click_view_box(&g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL]);
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
    if (g_inventory_champion_ordinal != champion_index + 1)
        press_key(command_champion_key(champion_index));
    return NULL;
}

static const char *close_inventory(const char *args)
{
    (void)args;
    if (g_inventory_champion_ordinal)
        press_key(command_champion_key(g_inventory_champion_ordinal - 1));
    return NULL;
}

/* A slot box click swaps the leader hand object with the slot: put and take are the same click.
   The slots of the open chest are boxes of the chest panel. */
static const char *click_slot(const char *args)
{
    int16_t champion_index;
    int64_t slot = get_int(args, "slot", -1);
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_leader_index == -1)
        return "no leader";
    if (g_inventory_champion_ordinal == champion_index + 1) {
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
    command_process_click(x, y, CLICK_LEFT_PRESS);
    gameloop_run_commands();
    look_text.shown = true;
    look_text.count = panel_draw_object_text(g_leader_hand_object, look_text.name, look_text.lines);
    if (g_advanced_features && panel_draw_object_meter(g_leader_hand_object, &label, &value))
        snprintf(look_text.lines[look_text.count++], PANEL_LINE_SIZE, "%s %u", label, value);
    command_process_click(x, y, CLICK_LEFT_RELEASE);
    gameloop_run_commands();
}

/* The eye on the leader hand object; with a slot, the hand takes the slot object first and puts it back after. */
static const char *send_look(const char *args)
{
    int16_t x, y;
    int64_t slot = get_int(args, "slot", -1);
    char request[SLOT_REQUEST_SIZE];
    if (!g_inventory_champion_ordinal)
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
             get_int(args, "champion", g_inventory_champion_ordinal - 1), slot);
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
    int64_t pair[2] = {get_int(args, "a", -1), get_int(args, "b", -1)};
    for (int k = 0; k < 2; k++)
        if (pair[k] < 0 || pair[k] >= g_party_champion_count || !g_champions[pair[k]].current_health)
            return "a and b must be living champions";
    if (pair[0] == pair[1])
        return "a and b must be different champions";
    for (int k = 0; k < 2; k++)
        if (!click_command(TABLES_MOUSE_INTERFACE, COMMAND_CLICK_CHAMPION_ICON_FIRST +
                                                       NORMALIZE(g_champions[pair[k]].cell - g_party_direction)))
            return "champion icon not found";
    return NULL;
}

static const char *use_mouth(const char *args)
{
    (void)args;
    if (!g_inventory_champion_ordinal)
        return "open the inventory of the champion who eats or drinks first";
    if (g_leader_empty_handed)
        return "take the food or drink in the leader hand first";
    click_command(TABLES_MOUSE_INVENTORY, COMMAND_CLICK_MOUTH);
    return NULL;
}

static const char *choose_action(const char *args)
{
    int16_t champion_index;
    int64_t index = get_int(args, "index", -1);
    if (!parse_champion_arg(args, &champion_index))
        return "no living champion with this index";
    if (g_acting_champion_ordinal && g_acting_champion_ordinal != champion_index + 1)
        return "another champion's action menu is open: use pass first";
    if (!g_acting_champion_ordinal) {
        click_command(TABLES_MOUSE_ACTION_ICONS, COMMAND_SELECT_ACTING_CHAMPION_FIRST + champion_index);
        if (g_acting_champion_ordinal != champion_index + 1)
            return "this champion cannot act now (action disabled, or no actions)";
    }
    if (index < 0 || index >= g_action_count)
        return "index out of range of the action menu (see state action_menu)";
    turn.has_damage = true;
    g_action_damage = 0;
    click_command(TABLES_MOUSE_ACTION_NAMES, COMMAND_ACTION_FIRST + index);
    turn.damage = g_action_damage;
    if (batch.active && batch.damage_count < BATCH_DAMAGE_MAX)
        batch.damages[batch.damage_count++] = g_action_damage;
    return NULL;
}

static const char *pass(const char *args)
{
    (void)args;
    if (g_acting_champion_ordinal)
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

static const char *spell_click(int16_t command)
{
    if (g_magic_caster_champion_index == -1)
        return "no spell area";
    click_command(TABLES_MOUSE_SPELL_AREA, command);
    return NULL;
}

static const char *add_rune(const char *args)
{
    int64_t symbol = get_int(args, "symbol", -1);
    if (symbol < 0 || symbol >= SYMBOL_COUNT)
        return "symbol must be 0-5";
    if (g_magic_caster_champion_index == -1)
        return "no spell area";
    size_t before = strlen(g_champions[g_magic_caster_champion_index].symbols);
    spell_click(COMMAND_SPELL_SYMBOL_FIRST + symbol);
    /* the game ignores a rune that costs more mana than the caster has */
    return strlen(g_champions[g_magic_caster_champion_index].symbols) == before
               ? "the rune was not added: not enough mana"
               : NULL;
}

static const char *delete_rune(const char *args)
{
    (void)args;
    return spell_click(COMMAND_DELETE_SYMBOL);
}
static const char *cast(const char *args)
{
    (void)args;
    return spell_click(COMMAND_CAST_SPELL);
}

static const char *recruit(const char *args)
{
    static const char *const choices[3] = {"resurrect", "reincarnate", "cancel"};
    char choice[CHOICE_SIZE];
    if (!g_candidate_champion_ordinal)
        return "no candidate: click a champion mirror first (click_wall_ahead)";
    if (!g_leader_empty_handed)
        return "the leader hand must be empty";
    if (!get_string(args, "choice", choice, sizeof choice))
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
    if (!g_inventory_champion_ordinal)
        press_key(command_champion_key(g_leader_index));
    click_command(TABLES_MOUSE_INVENTORY, COMMAND_SLEEP);
    return NULL;
}

static const char *wake(const char *args)
{
    (void)args;
    if (g_party_is_sleeping)
        click_command(TABLES_MOUSE_SLEEPING, COMMAND_WAKE_UP);
    return NULL;
}

static const struct {
    const char *name;
    ActionFn run;
} actions[] = {
    {"move_forward",     move_forward    },
    {"move_back",        move_back       },
    {"move_left",        move_left       },
    {"move_right",       move_right      },
    {"turn_left",        turn_party_left },
    {"turn_right",       turn_party_right},
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
    {"recruit",          recruit         },
    {"set_leader",       set_leader      },
    {"sleep",            sleep_party     },
    {"wake",             wake            },
    {"swap_champions",   swap_champions  },
    {"look",             send_look       },
};

/* Screenshot: fb as an RGB PNG with stored (uncompressed) deflate blocks. */

#define ZLIB_METHOD        0x78  /* deflate, 32 KB window */
#define ZLIB_FLAGS         0x01  /* no dictionary; header check bits */
#define ADLER32_MODULUS    65521 /* the largest prime below 65536 */
#define ADLER32_HIGH_SHIFT 16
#define PNG_IHDR_SIZE      13

static void png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t head[8];
    WR32(head, n);
    memcpy(head + 4, type, 4);
    uint32_t crc = crc32_update(crc32_update(0, head + 4, 4), data, n);
    fwrite(head, 1, 8, f);
    fwrite(data, 1, n, f);
    WR32(head, crc);
    fwrite(head, 1, 4, f);
}

static bool write_png(const char *path)
{
    enum { ROW = 1 + 3 * SCREEN_W, RAW = ROW * SCREEN_H, BLOCK = 65535 };
    enum { BLOCKS = (RAW + BLOCK - 1) / BLOCK, ZLIB = 2 + RAW + 5 * BLOCKS + 4 };
    static uint8_t raw[RAW], compressed[ZLIB];
    for (int y = 0; y < SCREEN_H; y++) {
        uint8_t *pixel = raw + y * ROW;
        *pixel++ = 0; /* filter: none */
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t argb = video_convert_argb(video_get_color(y, fb[y][x]));
            *pixel++ = video_argb_red(argb);
            *pixel++ = video_argb_green(argb);
            *pixel++ = video_argb_blue(argb);
        }
    }
    uint8_t *out = compressed;
    *out++ = ZLIB_METHOD;
    *out++ = ZLIB_FLAGS;
    uint32_t byte_sum = 1;
    uint32_t sum_of_sums = 0;
    for (size_t done = 0; done < RAW;) {
        size_t n = RAW - done < BLOCK ? RAW - done : BLOCK;
        *out++ = done + n == RAW;
        *out++ = (uint8_t)n;
        *out++ = (uint8_t)(n >> BYTE_BITS);
        *out++ = (uint8_t)~n;
        *out++ = (uint8_t)(~n >> BYTE_BITS);
        memcpy(out, raw + done, n);
        for (size_t i = 0; i < n; i++) {
            byte_sum = (byte_sum + raw[done + i]) % ADLER32_MODULUS;
            sum_of_sums = (sum_of_sums + byte_sum) % ADLER32_MODULUS;
        }
        out += n;
        done += n;
    }
    WR32(out, sum_of_sums << ADLER32_HIGH_SHIFT | byte_sum);
    out += 4;
    uint8_t ihdr[PNG_IHDR_SIZE] = {0};
    WR32(ihdr, SCREEN_W);
    WR32(ihdr + 4, SCREEN_H);
    ihdr[8] = 8; /* bits per channel */
    ihdr[9] = 2; /* RGB */
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, file);
    png_chunk(file, "IHDR", ihdr, sizeof ihdr);
    png_chunk(file, "IDAT", compressed, (uint32_t)(out - compressed));
    png_chunk(file, "IEND", NULL, 0);
    return fclose(file) == 0;
}

/* What the party saw during a request. */

/* Adds the explosions and the creature counts that the party sees now to the record of the request. */
static void record_witnessed(void)
{
    StateJsonSeenThing things[WITNESSED_EXPLOSIONS_MAX];
    int count = state_json_write_visible_things(THING_EXPLOSION, things, WITNESSED_EXPLOSIONS_MAX);
    for (int i = 0; i < count; i++) {
        uint8_t type = rec_explosion_type(things[i].thing);
        int known = 0;
        for (int k = 0; k < witnessed.explosion_count; k++)
            known |= witnessed.explosions[k].type == type && witnessed.explosions[k].at.x == things[i].x &&
                     witnessed.explosions[k].at.y == things[i].y;
        if (!known && witnessed.explosion_count < WITNESSED_EXPLOSIONS_MAX)
            witnessed.explosions[witnessed.explosion_count++] = (WitnessedExplosion){things[i], type};
    }
    for (int k = 0; k < witnessed.group_count; k++)
        witnessed.groups[k].in_view = false;
    count = state_json_write_visible_things(THING_GROUP, things, STATE_JSON_VISIBLE_GROUPS_MAX);
    for (int i = 0; i < count; i++) {
        int k = 0;
        while (k < witnessed.group_count && witnessed.groups[k].at.thing != things[i].thing)
            k++;
        if (k == WITNESSED_GROUPS_MAX)
            continue;
        int16_t creatures = rec_group_last_creature_index(things[i].thing) + 1;
        if (k == witnessed.group_count) { /* a group seen first: count it once, not on each tick */
            witnessed.groups[k] =
                (WitnessedGroup){.creature = rec_group_creature_type(things[i].thing), .before = creatures};
            witnessed.group_count++;
        }
        witnessed.groups[k].at = things[i];
        witnessed.groups[k].after = creatures;
        witnessed.groups[k].in_view = true;
    }
    for (int k = 0; k < witnessed.group_count; k++) /* a killed group frees its record */
        witnessed.groups[k].killed |=
            !witnessed.groups[k].in_view && map_get_next_thing(witnessed.groups[k].at.thing) == THING_NONE;
}

/* Starts the record of what the party sees during a request. */
static void start_witnessing(void)
{
    memset(&witnessed, 0, sizeof witnessed);
    witnessed.champion_count = g_party_champion_count;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        witnessed.health[i] = g_champions[i].current_health;
    record_witnessed();
}

static void seen_place_json(Json *j, const StateJsonSeenThing *at)
{
    json_key(j, "x");
    json_int(j, at->x);
    json_key(j, "y");
    json_int(j, at->y);
    json_key(j, "depth");
    json_int(j, at->depth);
    json_key(j, "side");
    json_str(j, at->side);
}

/* The explosions the party saw and the groups that lost creatures in view or were killed; nothing when none. */
static void seen_json(Json *j)
{
    int changes = 0;
    for (int k = 0; k < witnessed.group_count; k++)
        changes += witnessed.groups[k].killed || witnessed.groups[k].after < witnessed.groups[k].before;
    if (!changes && !witnessed.explosion_count)
        return;
    json_key(j, "seen");
    json_open(j, '{');
    json_key(j, "explosions");
    json_open(j, '[');
    for (int k = 0; k < witnessed.explosion_count; k++) {
        json_open(j, '{');
        json_key(j, "name");
        state_json_explosion_name(j, witnessed.explosions[k].type);
        seen_place_json(j, &witnessed.explosions[k].at);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_key(j, "creature_changes");
    json_open(j, '[');
    for (int k = 0; k < witnessed.group_count; k++) {
        if (!witnessed.groups[k].killed && witnessed.groups[k].after >= witnessed.groups[k].before)
            continue;
        json_open(j, '{');
        json_key(j, "type");
        json_str(j, state_json_creature_name(witnessed.groups[k].creature));
        seen_place_json(j, &witnessed.groups[k].at);
        json_key(j, "before");
        json_int(j, witnessed.groups[k].before);
        json_key(j, "after");
        json_int(j, witnessed.groups[k].killed ? 0 : witnessed.groups[k].after);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_close(j, '}');
}

/* Responses. */

/* The health that each champion lost since the start of the request, only when one lost health. */
static void health_lost_json(Json *j)
{
    int any = 0;
    for (uint16_t i = 0; i < witnessed.champion_count; i++)
        any |= g_champions[i].current_health < witnessed.health[i];
    if (!any)
        return;
    json_key(j, "health_lost");
    json_open(j, '[');
    for (uint16_t i = 0; i < witnessed.champion_count; i++) {
        int lost = witnessed.health[i] - g_champions[i].current_health;
        json_int(j, lost > 0 ? lost : 0);
    }
    json_close(j, ']');
}

static void close_client(void)
{
    if (client_fd >= 0)
        close(client_fd);
    client_fd = -1;
    input_length = 0;
}

static void send_line(Json *j)
{
    json_raw(j, "\n");
    for (size_t sent = 0; client_fd >= 0 && sent < j->len;) {
        ssize_t n = send(client_fd, j->s + sent, j->len - sent, SEND_FLAGS);
        if (n <= 0)
            close_client();
        else
            sent += (size_t)n;
    }
    free(j->s);
}

static void send_error(const char *error)
{
    Json j = {0};
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 0);
    json_key(&j, "error");
    json_str(&j, error);
    json_close(&j, '}');
    send_line(&j);
}

/* The eye panel text of a look action, only when the turn looked. */
static void look_json(Json *j)
{
    if (!look_text.shown)
        return;
    json_key(j, "look");
    json_open(j, '{');
    json_key(j, "name");
    json_str(j, look_text.name);
    json_key(j, "lines");
    json_open(j, '[');
    for (int16_t i = 0; i < look_text.count; i++)
        json_str(j, look_text.lines[i]);
    json_close(j, ']');
    json_close(j, '}');
}

/* The end of a turn: the ticks it took, the action damage, the state, and the view. */
static void send_turn_result(void)
{
    Json j = {0};
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 1);
    json_key(&j, "ticks");
    json_int(&j, g_game_time - (batch.active ? batch.start_time : turn.start_time));
    if (turn.wait_only) {
        json_key(&j, "stopped");
        json_str(&j, turn.stop ? turn.stop : "done");
    }
    if (!turn.wait_only && !batch.active && !is_game_over() && turn_displaced()) {
        json_key(&j, "stopped");
        json_str(&j, turn_displaced());
    }
    if (batch.active) {
        json_key(&j, "ran");
        json_int(&j, batch.ran);
        json_key(&j, "stopped");
        json_str(&j, batch.stop);
        if (batch.error) {
            json_key(&j, "error");
            json_str(&j, batch.error);
        }
    }
    json_key(&j, "game_over");
    json_bool(&j, is_game_over());
    if (turn_bumped()) {
        json_key(&j, "bumped");
        json_bool(&j, 1);
    }
    if (batch.active && batch.damage_count) {
        json_key(&j, "action_damage");
        json_open(&j, '[');
        for (int i = 0; i < batch.damage_count; i++)
            json_int(&j, batch.damages[i]);
        json_close(&j, ']');
    } else if (!batch.active && turn.has_damage) {
        json_key(&j, "action_damage");
        json_int(&j, turn.damage);
    }
    health_lost_json(&j);
    look_json(&j);
    seen_json(&j);
    json_key(&j, "state");
    state_json_write_state(&j);
    json_key(&j, "view");
    state_json_write_view(&j);
    json_close(&j, '}');
    send_line(&j);
}

static void send_part(const char *key, void (*part)(Json *))
{
    Json j = {0};
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 1);
    json_key(&j, key);
    part(&j);
    json_close(&j, '}');
    send_line(&j);
}

static void start_turn(int64_t min_ticks, int64_t max_ticks, bool wait_only)
{
    turn.pending = true;
    turn.wait_only = wait_only;
    turn.stop = NULL;
    turn.start_time = g_game_time;
    turn.frames = 0;
    turn.min_ticks = min_ticks;
    turn.max_ticks = max_ticks;
}

/* Gives the input of action name with the args in request; NULL, or the reason why it cannot. */
static const char *run_action(const char *name, const char *request)
{
    for (int k = 0; k < ARRAY_COUNT(actions); k++) {
        if (strcmp(name, actions[k].name))
            continue;
        if (g_candidate_champion_ordinal && strcmp(name, "recruit"))
            return "a candidate is shown: answer with recruit first";
        if (g_party_is_sleeping && strcmp(name, "wake"))
            return "the party sleeps: use wake or wait";
        turn.has_damage = 0;
        look_text.shown = 0;
        turn.moving = 0;
        turn.rotation = 0;
        turn.map = g_party_map_index;
        turn.x = g_party_map_x;
        turn.y = g_party_map_y;
        turn.direction = g_party_direction;
        turn.teleports = g_party_teleport_count;
        return actions[k].run(request);
    }
    return "unknown action";
}

static void handle_act(const char *request)
{
    char name[ACTION_NAME_SIZE];
    if (!get_string(request, "action", name, sizeof name)) {
        send_error("missing action");
        return;
    }
    start_witnessing();
    const char *error = run_action(name, request);
    if (error)
        send_error(error);
    else
        start_turn(TURN_MIN_TICKS, TURN_MAX_TICKS, 0);
}

/* Batches. */

/* The latest expiry time of a message row: it grows when the game prints a message. */
static uint32_t latest_message_expiry(void)
{
    uint32_t latest = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] > latest)
            latest = g_message.expiry[r];
    return latest;
}

/* Remembers what the news checks compare during and after the next turn. */
static void take_snapshot(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        seen.health[i] = g_champions[i].current_health;
    seen.group_count = state_json_write_visible_groups(seen.groups);
    seen.message_expiry = latest_message_expiry();
}

/* A creature group is in view that was not in view at the snapshot. */
static bool new_group_in_view(void)
{
    uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX];
    int count = state_json_write_visible_groups(groups);
    for (int i = 0; i < count; i++) {
        int known = 0;
        for (int k = 0; k < seen.group_count; k++)
            known |= groups[i] == seen.groups[k];
        if (!known)
            return true;
    }
    return false;
}

/* What happened since the snapshot that the player must see, or NULL. */
static const char *send_news(void)
{
    if (is_game_over())
        return "game_over";
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health < seen.health[i])
            return "damage";
    if (new_group_in_view())
        return "creature";
    if (latest_message_expiry() > seen.message_expiry)
        return "message";
    return NULL;
}

/* Why a batch stops after a turn, or NULL to go on. */
static const char *batch_stop_reason(void)
{
    if (is_game_over())
        return send_news();
    if (turn_bumped())
        return "bumped";                      /* before damage: a bump into a wall hurts */
    const char *displaced = turn_displaced(); /* before damage: a pit fall hurts */
    return displaced ? displaced : send_news();
}

static void batch_finish(const char *stop, const char *error)
{
    batch.stop = stop;
    batch.error = error;
    send_turn_result();
    free(batch.actions);
    memset(&batch, 0, sizeof batch);
}

/* The next act_many element, a "name" or an {"action": "name", "args": {...}} object, copied to out.
   Returns 1, 0 at the end of the list, or -1 for an item that is not an action. */
static int next_element(char *out, size_t size)
{
    const char *cursor = batch.next;
    while (*cursor == ' ' || *cursor == ',')
        cursor++;
    const char *start = cursor;
    if (!*cursor || *cursor == ']')
        return 0;
    if (*cursor == '"') {
        cursor = strchr(cursor + 1, '"');
        if (!cursor)
            return -1;
        cursor++;
    } else if (*cursor == '{') {
        for (int depth = 0; *cursor; cursor++) {
            if (*cursor == '"')
                cursor = strchr(cursor + 1, '"'); /* strings hold no '"' (docs/llm/llm-mode.md) */
            if (!cursor)
                return -1;
            depth += (*cursor == '{') - (*cursor == '}');
            if (!depth)
                break;
        }
        if (!*cursor++)
            return -1;
    } else {
        return -1;
    }
    if ((size_t)(cursor - start) >= size)
        return -1;
    memcpy(out, start, (size_t)(cursor - start));
    out[cursor - start] = 0;
    batch.next = cursor;
    return 1;
}

/* Starts the next step of the batch, or ends the batch when it is done or the step cannot run. */
static void batch_next(void)
{
    const char *error;
    char element[INPUT_SIZE / 4], name[ACTION_NAME_SIZE];
    take_snapshot();
    int found = next_element(element, sizeof element);
    if (found <= 0) {
        batch_finish(found ? "failed" : "done", found ? "an item of actions is not an action name or object" : NULL);
        return;
    }
    if (element[0] == '"')
        snprintf(name, sizeof name, "%.*s", (int)strlen(element) - 2, element + 1);
    else if (!get_string(element, "action", name, sizeof name))
        name[0] = 0;
    error = run_action(name, element);
    if (error)
        batch_finish("failed", error);
    else
        start_turn(TURN_MIN_TICKS, TURN_MAX_TICKS, 0);
}

/* After a turn of the batch: count it, stop on news, else start the next step. */
static void batch_after_turn(void)
{
    batch.ran++;
    const char *stop = batch_stop_reason();
    if (stop)
        batch_finish(stop, NULL);
    else
        batch_next();
}

static void handle_act_many(const char *request)
{
    const char *list = find_value(request, "actions");
    if (!list || *list != '[') {
        send_error("actions must be a list");
        return;
    }
    batch.actions = strdup(list + 1);
    if (!batch.actions)
        abort();
    batch.next = batch.actions;
    batch.active = true;
    batch.damage_count = 0;
    batch.start_time = g_game_time;
    turn.has_damage = false;
    turn.moving = false;
    start_witnessing();
    batch_next();
}

/* Loads the save file of the adventure (DMGAME.DAT or CSBGAME.DAT) from the save directory, as RESUME does, and answers with the state. */
static void handle_load(void)
{
    int16_t result = savegame_load(startend_save_dir());
    if (result == SAVEGAME_MISSING) {
        send_error("no saved game (use save first)");
        return;
    }
    load_failed = result == SAVEGAME_DAMAGED;
    if (load_failed) {
        send_error("the saved game is damaged: use new_game");
        return;
    }
    gameloop_start_loaded_game();
    send_part("state", state_json_write_state);
}

/* Starts a new game on the start square, as ENTER at the entrance does, and answers with the state. */
static void handle_new_game(void)
{
    if (!gameloop_start_new_game(gameloop_get_data_dir())) {
        send_error("cannot start the new game");
        return;
    }
    load_failed = false;
    send_part("state", state_json_write_state);
}

/* Writes the save file of the adventure, as the SAVE button does. */
static void handle_save(void)
{
    const char *reason;
    int saved = startend_save_current_game(&reason);
    if (!saved)
        send_error(reason ? reason : "cannot write the save file");
    else {
        if (saved == SAVED_NEW_ADVENTURE)
            gameloop_start_loaded_game();
        Json j = {0};
        json_raw(&j, "{\"ok\":true}");
        send_line(&j);
    }
}

/* Starts a turn of the given ticks without an action; it ends early on news. */
static void handle_wait(const char *request)
{
    int64_t ticks = get_int(request, "ticks", 1);
    if (ticks < 1 || ticks > WAIT_MAX_TICKS)
        send_error("ticks must be 1-5000");
    else {
        turn.has_damage = false;
        look_text.shown = false;
        turn.moving = false;
        take_snapshot();
        start_witnessing();
        start_turn(ticks, ticks, 1);
    }
}

/* Writes the screen to the PNG file of the request path. */
static void handle_screenshot(const char *request)
{
    char path[PATH_SIZE];
    if (!get_string(request, "path", path, sizeof path))
        send_error("missing path");
    else if (!write_png(path))
        send_error("cannot write the PNG file");
    else {
        Json j = {0};
        json_raw(&j, "{\"ok\":true,\"path\":");
        json_str(&j, path);
        json_close(&j, '}');
        send_line(&j);
    }
}

static void handle_request(const char *request)
{
    char cmd[CMD_SIZE];
    if (!get_string(request, "cmd", cmd, sizeof cmd)) {
        send_error("missing cmd");
        return;
    }
    if (!strcmp(cmd, "load")) {
        handle_load();
        return;
    }
    if (!strcmp(cmd, "new_game")) {
        handle_new_game();
        return;
    }
    if (!g_dungeon_maps || load_failed) {
        send_error("no game is running: use load or new_game");
        return;
    }
    if (!strcmp(cmd, "get_state")) {
        send_part("state", state_json_write_state);
    } else if (!strcmp(cmd, "get_view")) {
        send_part("view", state_json_write_view);
    } else if (!strcmp(cmd, "get_map")) {
        send_part("map", state_json_map);
    } else if (is_game_over() &&
               (!strcmp(cmd, "act") || !strcmp(cmd, "act_many") || !strcmp(cmd, "wait") || !strcmp(cmd, "save"))) {
        send_error("game over");
    } else if (!strcmp(cmd, "save")) {
        handle_save();
    } else if (!strcmp(cmd, "act")) {
        handle_act(request);
    } else if (!strcmp(cmd, "act_many")) {
        handle_act_many(request);
    } else if (!strcmp(cmd, "wait")) {
        handle_wait(request);
    } else if (!strcmp(cmd, "screenshot")) {
        handle_screenshot(request);
    } else {
        send_error("unknown cmd");
    }
}

/* A turn ends when the party can move again after the minimum ticks, or at the maximum. A wait
   ends early on news. A game that does not tick ends the turn after a frame limit. */
static bool turn_finished(void)
{
    int64_t ticks = g_game_time - turn.start_time;
    if (turn.wait_only && (turn.stop = send_news()))
        return true;
    if (is_game_over())
        return true;
    if (ticks >= turn.max_ticks || turn.frames > turn.max_ticks * FRAMES_PER_TICK_LIMIT)
        return true;
    return !turn.wait_only && ticks >= turn.min_ticks && !g_disabled_movement_ticks;
}

bool control_open(const char *path)
{
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    char dir[sizeof socket_path];
    if (strlen(path) >= sizeof address.sun_path)
        return false;
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) {
        *slash = 0;
        mkdir(dir, 0700);
    }
    snprintf(address.sun_path, sizeof address.sun_path, "%s", path);
    snprintf(socket_path, sizeof socket_path, "%s", path);
    unlink(path);
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
        return false;
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof address) || listen(listen_fd, 1) ||
        fcntl(listen_fd, F_SETFL, O_NONBLOCK)) {
        control_close();
        return false;
    }
    return true;
}

bool control_is_available(void)
{
    return true;
}

bool control_is_turn_pending(void)
{
    return turn.pending;
}

void control_poll(void)
{
    if (listen_fd < 0)
        return;
    if (turn.pending) {
        record_witnessed();
        if (!turn_finished()) {
            turn.frames++;
            gameloop_run_commands();
            g_stop_waiting_for_input = true; /* every frame of a turn is a tick */
            return;
        }
        turn.pending = false;
        if (client_fd < 0) {
            free(batch.actions);
            memset(&batch, 0, sizeof batch);
        } else if (batch.active)
            batch_after_turn();
        else
            send_turn_result();
    }
    if (client_fd < 0) {
        client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0)
            return;
        fcntl(client_fd, F_SETFL, 0); /* blocking writes; reads use MSG_DONTWAIT */
#ifdef SO_NOSIGPIPE
        int on = 1;
        setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
    }
    ssize_t n = recv(client_fd, input + input_length, sizeof input - 1 - input_length, MSG_DONTWAIT);
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        close_client();
        return;
    }
    if (n > 0)
        input_length += (size_t)n;
    input[input_length] = 0;
    char *end;
    while (client_fd >= 0 && !turn.pending && (end = strchr(input, '\n'))) {
        *end = 0;
        if (end > input)
            handle_request(input);
        if (client_fd < 0)
            return;
        size_t used = (size_t)(end + 1 - input);
        memmove(input, end + 1, input_length - used + 1);
        input_length -= used;
    }
    if (input_length == sizeof input - 1) {
        send_error("request line too long");
        input_length = 0;
    }
}

void control_close(void)
{
    close_client();
    if (listen_fd >= 0) {
        close(listen_fd);
        unlink(socket_path);
    }
    listen_fd = -1;
}
