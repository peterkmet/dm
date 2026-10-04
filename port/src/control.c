#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include "control.h"
#include "advanced.h"
#include "champion.h"
#include "command.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "gameloop.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "savegame.h"
#include "startend.h"
#include "state_json.h"
#include "timeline_process.h"
#include "ui.h"
#include "video.h"
#include "view.h"

/* KAOS data addresses. */
#define VIEWPORT_ORIGIN 0x10293a      /* viewport origin x, y on the screen */

#define INPUT_SIZE 65536              /* longest request line */
#define TURN_MIN_TICKS 2              /* an action shows its result, then the action area returns */
#define TURN_MAX_TICKS 64
#define WAIT_MAX_TICKS 5000
#define FRAMES_PER_TICK_LIMIT 16      /* a frozen game ends a turn after this many frames per tick */
#define TEXT_SIZE 256
#define SPELL_TAB_STEP 5              /* caster tab zones per caster */
#define ACTION_NAME_SIZE 32
#define PATH_MAX_STEPS 1024           /* a 32 x 32 map */

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL       /* a closed client does not raise SIGPIPE (Linux) */
#else
#define SEND_FLAGS 0                  /* SO_NOSIGPIPE does the same (macOS) */
#endif

/* Entry of a mouse input table (ui.c reads the same layout): command, x1, x2, y1, y2, buttons. A
   negative x1 marks a zone entry; x2 is then the zone. */
enum { INPUT_COMMAND, INPUT_X1, INPUT_X2, INPUT_Y1, INPUT_Y2, INPUT_BUTTONS, INPUT_WORDS };

static int listen_fd = -1, client_fd = -1;
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static char input[INPUT_SIZE];
static size_t input_length;

static struct {
    int pending, wait_only;
    uint32_t start_time;
    long frames, min_ticks, max_ticks;
    int has_damage;
    int16_t damage;
    int moving;                       /* the action was a step; map, x, y is the party square before it */
    int16_t map, x, y;
    const char *stop;                 /* wait: the news that ended it early, or NULL */
} turn;

/* A batch: act_many runs a list of actions, goto walks a path; each step is one turn. */
static struct {
    int active, ran;
    char *actions;                    /* act_many: a copy of the request; next is the rest of its actions array */
    const char *next;
    int16_t path[PATH_MAX_STEPS];     /* goto: the directions to walk */
    int path_length, path_done, moving;
    int16_t facing;                   /* goto: the direction to face at the target, or -1 */
    uint32_t start_time;
    const char *stop, *error;
} batch;

/* What the party knew before a turn: act_many, goto, and wait stop on news against it. */
static struct {
    int16_t health[CHAMPION_MAX_COUNT];
    uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX];
    int group_count;
    uint32_t message_expiry;
} seen;

/* The eye panel text of a look action, for the turn result. */
static struct {
    int shown;
    int16_t count;
    char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT + 1][PANEL_LINE_SIZE];
} look_text;

static const char *const direction_names[] = { "north", "east", "south", "west" };

static int load_failed;               /* a damaged save file left no game to play */

/* The party died, or the game is won: the endgame ran and no turn can follow until load or new_game. */
static int game_over(void) { return g_party_dead || g_game_won; }

/* Input as the player gives it: clicks and keys go through the command queue. */

static void run_queue(void)
{
    while (command_process_queue())
        ;
}

static void click(int16_t x, int16_t y)
{
    command_process_click(x, y, CLICK_LEFT_PRESS);
    command_process_click(x, y, CLICK_LEFT_RELEASE);
    run_queue();
}

static void press_key(uint16_t key)
{
    command_process_key(key);
    run_queue();
}

static void viewport_origin(int16_t *x, int16_t *y)
{
    *x = kaos_s16(VIEWPORT_ORIGIN);
    *y = kaos_s16(VIEWPORT_ORIGIN + 2);
}

/* A screen point inside box b (moved by dx, dy) where the table gives command, or 0. */
static int find_in_box(int16_t table, int16_t command, Box b, int16_t dx, int16_t dy, int16_t *x, int16_t *y)
{
    int16_t cx = (int16_t)(b.x + dx + b.w / 2), cy = (int16_t)(b.y + dy + b.h / 2);
    if (ui_command_from_mouse(table, cx, cy, UI_BUTTON_LEFT) == command) {
        *x = cx, *y = cy;
        return 1;
    }
    for (int16_t py = (int16_t)(b.y + dy); py < b.y + dy + b.h; py++)
        for (int16_t px = (int16_t)(b.x + dx); px < b.x + dx + b.w; px++)
            if (ui_command_from_mouse(table, px, py, UI_BUTTON_LEFT) == command) {
                *x = px, *y = py;
                return 1;
            }
    return 0;
}

/* The screen point that a mouse input table maps to command: a fixed box, or a zone on the screen,
   in the viewport, or in the centered viewport. */
static int find_click(int16_t table, int16_t command, int16_t *x, int16_t *y)
{
    const Bitmap *vp = ui_viewport();
    int16_t ox, oy;
    viewport_origin(&ox, &oy);
    const int16_t offsets[3][2] = { { 0, 0 }, { ox, oy }, { (int16_t)((SCREEN_W - vp->w) / 2), (int16_t)((SCREEN_H - vp->h) / 2) } };
    for (uint32_t a = ui_mouse_table_address(table); kaos_s16(a + 2 * INPUT_COMMAND); a += 2 * INPUT_WORDS) {
        if (kaos_s16(a + 2 * INPUT_COMMAND) != command) continue;
        int16_t x1 = kaos_s16(a + 2 * INPUT_X1), x2 = kaos_s16(a + 2 * INPUT_X2);
        Box b = { x1, kaos_s16(a + 2 * INPUT_Y1), (int16_t)(x2 - x1 + 1), 0 };
        if (x1 >= 0) {
            b.h = (int16_t)(kaos_s16(a + 2 * INPUT_Y2) - b.y + 1);
            if (find_in_box(table, command, b, 0, 0, x, y)) return 1;
            continue;
        }
        if (!ui_zone_box(x2, &b)) continue;
        for (int k = 0; k < 3; k++)
            if (find_in_box(table, command, b, offsets[k][0], offsets[k][1], x, y)) return 1;
    }
    return 0;
}

static int click_command(int16_t table, int16_t command)
{
    int16_t x, y;
    if (!find_click(table, command, &x, &y)) return 0;
    click(x, y);
    return 1;
}

/* A click in the middle of a box of the dungeon view (viewport coordinates). */
static void click_view_box(const Box *b)
{
    int16_t ox, oy;
    viewport_origin(&ox, &oy);
    click((int16_t)(ox + b->x + b->w / 2), (int16_t)(oy + b->y + b->h / 2));
}

/* Requests: a flat search for "key": value; nested args are found the same way. */

static const char *find_value(const char *line, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof pattern, "\"%s\"", key);
    const char *p = strstr(line, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    return p;
}

static int get_string(const char *line, const char *key, char *out, size_t size)
{
    const char *p = find_value(line, key);
    size_t n = 0;
    if (!p || *p++ != '"') return 0;
    while (*p && *p != '"' && n + 1 < size) out[n++] = *p++;
    out[n] = 0;
    return 1;
}

static long get_int(const char *line, const char *key, long missing)
{
    const char *p = find_value(line, key);
    char *end;
    if (!p) return missing;
    long value = strtol(p, &end, 10);
    return end == p ? missing : value;
}

/* Actions. Each returns NULL when it gave its input, or the reason why it cannot. */

typedef const char *(*ActionFn)(const char *request);

static int champion_arg(const char *request, int16_t *champion)
{
    long c = get_int(request, "champion", -1);
    if (c < 0 || c >= (long)g_party_champion_count || !g_champions[c].current_health) return 0;
    *champion = (int16_t)c;
    return 1;
}

static const char *need_movement_input(void)
{
    if (g_inventory_champion_ordinal) return "close the inventory first (close_inventory)";
    return NULL;
}

static const char *movement(uint16_t key)
{
    const char *error = need_movement_input();
    if (!error) press_key(key);
    return error;
}

/* A step remembers the party square, so that the turn result can tell a bump. */
static const char *step(uint16_t key)
{
    turn.moving = 1;
    turn.map = g_party_map_index, turn.x = g_party_map_x, turn.y = g_party_map_y;
    return movement(key);
}

/* The step of the turn left the party on its square: a wall, a closed door, or a creature is in the way. */
static int turn_bumped(void)
{
    return turn.moving && g_party_map_index == turn.map && g_party_map_x == turn.x && g_party_map_y == turn.y;
}

static const char *move_forward(const char *r) { (void)r; return step(KEY_UP); }
static const char *move_back(const char *r) { (void)r; return step(KEY_DOWN); }
static const char *move_left(const char *r) { (void)r; return step(KEY_LEFT); }
static const char *move_right(const char *r) { (void)r; return step(KEY_RIGHT); }
static const char *turn_left(const char *r) { (void)r; return movement(KEY_DEL); }
static const char *turn_right(const char *r) { (void)r; return movement(KEY_HELP); }

static const char *click_wall_ahead(const char *r)
{
    (void)r;
    const Box *b = &g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL];
    if (g_inventory_champion_ordinal) return "close the inventory first (close_inventory)";
    if (b->x == -1) return "nothing to click on the wall ahead";
    click_view_box(b);
    return NULL;
}

static const char *take_object(const char *r)
{
    long cell = get_int(r, "cell", -1);
    if (g_inventory_champion_ordinal) return "close the inventory first (close_inventory)";
    if (cell < 0 || cell > VIEW_CLICK_BOX_ALCOVE) return "cell must be 0-4 (see get_view piles)";
    if (!g_leader_empty_handed) return "the leader hand is not empty: drop or store the object first";
    if (g_dungeon_view_clickable_boxes[cell].x == -1) return "no object at this cell";
    click_view_box(&g_dungeon_view_clickable_boxes[cell]);
    return NULL;
}

static const char *drop_object(const char *r)
{
    long cell = get_int(r, "cell", 0);
    Box b;
    if (g_inventory_champion_ordinal) return "close the inventory first (close_inventory)";
    if (g_leader_empty_handed) return "the leader hand is empty";
    if (cell == VIEW_CLICK_BOX_ALCOVE) {
        if (!g_facing_alcove) return "no alcove ahead";
        click_view_box(&g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL]);
        return NULL;
    }
    if (cell < 0 || cell > VIEW_CLICK_BOX_ALCOVE || !ui_zone_box((int16_t)(ZONE_DROP_CELL_FIRST + cell), &b)) return "cell must be 0-4";
    click_view_box(&b);
    return NULL;
}

static const char *open_inventory(const char *r)
{
    int16_t c;
    if (!champion_arg(r, &c)) return "no living champion with this index";
    if (g_inventory_champion_ordinal != c + 1) press_key((uint16_t)(KEY_F1 + (c << 8)));
    return NULL;
}

static const char *close_inventory(const char *r)
{
    (void)r;
    if (g_inventory_champion_ordinal) press_key((uint16_t)(KEY_F1 + ((g_inventory_champion_ordinal - 1) << 8)));
    return NULL;
}

/* A slot box click swaps the leader hand object with the slot: put and take are the same click.
   The slots of the open chest are boxes of the chest panel. */
static const char *click_slot(const char *r)
{
    int16_t c;
    long slot = get_int(r, "slot", -1);
    if (!champion_arg(r, &c)) return "no living champion with this index";
    if (g_leader_index == -1) return "no leader";
    if (g_inventory_champion_ordinal == c + 1) {
        if (slot < 0 || slot >= SLOT_CHEST_FIRST + (g_open_chest != THING_NONE ? SLOT_CHEST_COUNT : 0)) return "slot out of range";
        int16_t table = slot >= SLOT_CHEST_FIRST ? UI_INPUT_CHEST : UI_INPUT_INVENTORY;
        if (!click_command(table, (int16_t)(COMMAND_CLICK_SLOT_BOX_FIRST + UI_SLOT_BOX_INVENTORY_FIRST + slot))) return "slot not found";
        return NULL;
    }
    if (slot != SLOT_HAND_READY && slot != SLOT_HAND_ACTION) return "open the inventory of this champion first (open_inventory)";
    if (!click_command(UI_INPUT_STATUS_BOX, (int16_t)(COMMAND_CLICK_SLOT_BOX_FIRST + 2 * c + slot))) return "slot not found";
    return NULL;
}

/* Holds the eye down, reads the panel text of the leader hand object, and releases the eye. */
static void press_eye(int16_t x, int16_t y)
{
    const char *label;
    uint16_t value;
    command_process_click(x, y, CLICK_LEFT_PRESS);
    run_queue();
    look_text.shown = 1;
    look_text.count = panel_object_text(g_leader_hand_object, look_text.name, look_text.lines);
    if (g_advanced_features && panel_object_meter(g_leader_hand_object, &label, &value))
        snprintf(look_text.lines[look_text.count++], PANEL_LINE_SIZE, "%s %u", label, value);
    command_process_click(x, y, CLICK_LEFT_RELEASE);
    run_queue();
}

/* The eye on the leader hand object; with a slot, the hand takes the slot object first and puts it back after. */
static const char *look(const char *r)
{
    int16_t x, y;
    long slot = get_int(r, "slot", -1);
    char request[64];
    if (!g_inventory_champion_ordinal) return "open an inventory first (open_inventory): the eye is in the inventory";
    if (!find_click(UI_INPUT_INVENTORY, COMMAND_CLICK_EYE, &x, &y)) return "eye not found";
    if (slot < 0) {
        if (g_leader_empty_handed) return "the leader hand is empty: take an object, or give champion and slot";
        press_eye(x, y);
        return NULL;
    }
    if (!g_leader_empty_handed) return "the leader hand must be empty to look at a slot";
    snprintf(request, sizeof request, "\"champion\":%ld,\"slot\":%ld", get_int(r, "champion", g_inventory_champion_ordinal - 1), slot);
    const char *error = click_slot(request);
    if (error) return error;
    if (g_leader_empty_handed) return "no object in this slot";
    press_eye(x, y);
    click_slot(request);
    return NULL;
}

/* Clicks the icon of champion a, then the icon of champion b: the two champions swap cells. */
static const char *swap_champions(const char *r)
{
    long pair[2] = { get_int(r, "a", -1), get_int(r, "b", -1) };
    for (int k = 0; k < 2; k++)
        if (pair[k] < 0 || pair[k] >= (long)g_party_champion_count || !g_champions[pair[k]].current_health) return "a and b must be living champions";
    if (pair[0] == pair[1]) return "a and b must be different champions";
    for (int k = 0; k < 2; k++)
        if (!click_command(UI_INPUT_INTERFACE, (int16_t)(COMMAND_CLICK_CHAMPION_ICON_FIRST + NORMALIZE(g_champions[pair[k]].cell - g_party_direction))))
            return "champion icon not found";
    return NULL;
}

static const char *use_mouth(const char *r)
{
    (void)r;
    if (!g_inventory_champion_ordinal) return "open the inventory of the champion who eats or drinks first";
    if (g_leader_empty_handed) return "take the food or drink in the leader hand first";
    click_command(UI_INPUT_INVENTORY, COMMAND_CLICK_MOUTH);
    return NULL;
}

static const char *choose_action(const char *r)
{
    int16_t c;
    long index = get_int(r, "index", -1);
    if (!champion_arg(r, &c)) return "no living champion with this index";
    if (g_acting_champion_ordinal && g_acting_champion_ordinal != c + 1) return "another champion's action menu is open: use pass first";
    if (!g_acting_champion_ordinal) {
        click_command(UI_INPUT_ACTION_ICONS, (int16_t)(COMMAND_SELECT_ACTING_CHAMPION_FIRST + c));
        if (g_acting_champion_ordinal != c + 1) return "this champion cannot act now (action disabled, or no actions)";
    }
    if (index < 0 || index >= (long)g_action_count) return "index out of range of the action menu (see state action_menu)";
    turn.has_damage = 1;
    g_action_damage = 0;
    click_command(UI_INPUT_ACTION_NAMES, (int16_t)(COMMAND_ACTION_FIRST + index));
    turn.damage = g_action_damage;
    return NULL;
}

static const char *pass(const char *r)
{
    (void)r;
    if (g_acting_champion_ordinal) click_command(UI_INPUT_ACTION_NAMES, COMMAND_PASS);
    return NULL;
}

static const char *set_caster(const char *r)
{
    int16_t c;
    Box b;
    if (!champion_arg(r, &c)) return "no living champion with this index";
    if (g_magic_caster_champion_index == -1) return "no spell area";
    if (!ui_zone_box((int16_t)(ZONE_SPELL_CASTER_TAB_FIRST + g_magic_caster_champion_index * SPELL_TAB_STEP + c), &b)) return "caster tab not found";
    click((int16_t)(b.x + b.w / 2), (int16_t)(b.y + b.h / 2));
    return NULL;
}

static const char *spell_click(int16_t command)
{
    if (g_magic_caster_champion_index == -1) return "no spell area";
    click_command(UI_INPUT_SPELL_AREA, command);
    return NULL;
}

static const char *add_rune(const char *r)
{
    long symbol = get_int(r, "symbol", -1);
    if (symbol < 0 || symbol >= SYMBOL_COUNT) return "symbol must be 0-5";
    return spell_click((int16_t)(COMMAND_SPELL_SYMBOL_FIRST + symbol));
}

static const char *delete_rune(const char *r) { (void)r; return spell_click(COMMAND_DELETE_SYMBOL); }
static const char *cast(const char *r) { (void)r; return spell_click(COMMAND_CAST_SPELL); }

static const char *recruit(const char *r)
{
    static const char *const choices[3] = { "resurrect", "reincarnate", "cancel" };
    char choice[16];
    if (!g_candidate_champion_ordinal) return "no candidate: click a champion mirror first (click_wall_ahead)";
    if (!g_leader_empty_handed) return "the leader hand must be empty";
    if (!get_string(r, "choice", choice, sizeof choice)) return "choice must be resurrect, reincarnate, or cancel";
    for (int k = 0; k < 3; k++)
        if (!strcmp(choice, choices[k])) {
            click_command(UI_INPUT_RESURRECT, (int16_t)(PANEL_RESURRECT + k));
            return NULL;
        }
    return "choice must be resurrect, reincarnate, or cancel";
}

static const char *set_leader(const char *r)
{
    int16_t c;
    if (!champion_arg(r, &c)) return "no living champion with this index";
    click_command(UI_INPUT_STATUS_BOX, (int16_t)(COMMAND_SET_LEADER_FIRST + c));
    return NULL;
}

static const char *sleep_party(const char *r)
{
    (void)r;
    if (g_leader_index == -1) return "no leader";
    if (!g_inventory_champion_ordinal) press_key((uint16_t)(KEY_F1 + (g_leader_index << 8)));
    click_command(UI_INPUT_INVENTORY, COMMAND_SLEEP);
    return NULL;
}

static const char *wake(const char *r)
{
    (void)r;
    if (g_party_is_sleeping) click_command(UI_INPUT_SLEEPING, COMMAND_WAKE_UP);
    return NULL;
}

static const struct {
    const char *name;
    ActionFn run;
} actions[] = {
    { "move_forward", move_forward }, { "move_back", move_back }, { "move_left", move_left }, { "move_right", move_right },
    { "turn_left", turn_left }, { "turn_right", turn_right }, { "click_wall_ahead", click_wall_ahead },
    { "take_object", take_object }, { "drop_object", drop_object }, { "open_inventory", open_inventory },
    { "close_inventory", close_inventory }, { "put_in_slot", click_slot }, { "take_from_slot", click_slot },
    { "eat", use_mouth }, { "drink", use_mouth }, { "choose_action", choose_action }, { "pass", pass },
    { "set_caster", set_caster }, { "add_rune", add_rune }, { "delete_rune", delete_rune }, { "cast", cast },
    { "recruit", recruit }, { "set_leader", set_leader }, { "sleep", sleep_party }, { "wake", wake },
    { "swap_champions", swap_champions }, { "look", look },
};

/* Screenshot: fb as an RGB PNG with stored (uncompressed) deflate blocks. */

#define CRC32_POLYNOMIAL 0xedb88320u   /* reflected CRC-32 of PNG */
#define ZLIB_METHOD 0x78               /* deflate, 32 KB window */
#define ZLIB_FLAGS 0x01                /* no dictionary; header check bits */
#define BYTE_MASK 0xff

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? CRC32_POLYNOMIAL ^ c >> 1 : c >> 1;
            crc_table[i] = c;
        }
    crc = ~crc;
    while (n--) crc = crc_table[(crc ^ *p++) & BYTE_MASK] ^ crc >> 8;
    return ~crc;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t head[8];
    be32(head, n);
    memcpy(head + 4, type, 4);
    uint32_t crc = crc32_update(crc32_update(0, head + 4, 4), data, n);
    fwrite(head, 1, 8, f);
    fwrite(data, 1, n, f);
    be32(head, crc);
    fwrite(head, 1, 4, f);
}

static int write_png(const char *path)
{
    enum { ROW = 1 + 3 * SCREEN_W, RAW = ROW * SCREEN_H, BLOCK = 65535 };
    enum { BLOCKS = (RAW + BLOCK - 1) / BLOCK, ZLIB = 2 + RAW + 5 * BLOCKS + 4 };
    static uint8_t raw[RAW], z[ZLIB];
    for (int y = 0; y < SCREEN_H; y++) {
        uint8_t *p = raw + y * ROW;
        *p++ = 0; /* filter: none */
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t argb = video_argb(video_color(y, fb[y][x]));
            *p++ = (uint8_t)(argb >> 16), *p++ = (uint8_t)(argb >> 8), *p++ = (uint8_t)argb;
        }
    }
    uint8_t *q = z;
    *q++ = ZLIB_METHOD, *q++ = ZLIB_FLAGS;
    uint32_t a = 1, b = 0;
    for (size_t done = 0; done < RAW;) {
        size_t n = RAW - done < BLOCK ? RAW - done : BLOCK;
        *q++ = done + n == RAW;
        *q++ = (uint8_t)n, *q++ = (uint8_t)(n >> 8), *q++ = (uint8_t)~n, *q++ = (uint8_t)(~n >> 8);
        memcpy(q, raw + done, n);
        for (size_t i = 0; i < n; i++) a = (a + raw[done + i]) % 65521, b = (b + a) % 65521;
        q += n;
        done += n;
    }
    be32(q, b << 16 | a);
    q += 4;
    uint8_t ihdr[13] = { 0 };
    be32(ihdr, SCREEN_W);
    be32(ihdr + 4, SCREEN_H);
    ihdr[8] = 8; /* bits per channel */
    ihdr[9] = 2; /* RGB */
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    png_chunk(f, "IHDR", ihdr, sizeof ihdr);
    png_chunk(f, "IDAT", z, (uint32_t)(q - z));
    png_chunk(f, "IEND", NULL, 0);
    return fclose(f) == 0;
}

/* Responses. */

static void close_client(void)
{
    if (client_fd >= 0) close(client_fd);
    client_fd = -1;
    input_length = 0;
}

static void send_line(Json *j)
{
    json_raw(j, "\n");
    for (size_t sent = 0; client_fd >= 0 && sent < j->len;) {
        ssize_t n = send(client_fd, j->s + sent, j->len - sent, SEND_FLAGS);
        if (n <= 0) close_client();
        else sent += (size_t)n;
    }
    free(j->s);
}

static void send_error(const char *error)
{
    Json j = { 0 };
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 0);
    json_key(&j, "error");
    json_str(&j, error);
    json_close(&j, '}');
    send_line(&j);
}

/* The end of a turn: the ticks it took, the action damage, the state, and the view. */
static void send_turn_result(void)
{
    Json j = { 0 };
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 1);
    json_key(&j, "ticks");
    json_int(&j, (long)(g_game_time - (batch.active ? batch.start_time : turn.start_time)));
    if (turn.wait_only) {
        json_key(&j, "stopped");
        json_str(&j, turn.stop ? turn.stop : "done");
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
    json_bool(&j, game_over());
    if (turn_bumped()) {
        json_key(&j, "bumped");
        json_bool(&j, 1);
    }
    if (turn.has_damage) {
        json_key(&j, "action_damage");
        json_int(&j, turn.damage);
    }
    if (look_text.shown) {
        json_key(&j, "look");
        json_open(&j, '{');
        json_key(&j, "name");
        json_str(&j, look_text.name);
        json_key(&j, "lines");
        json_open(&j, '[');
        for (int16_t i = 0; i < look_text.count; i++) json_str(&j, look_text.lines[i]);
        json_close(&j, ']');
        json_close(&j, '}');
    }
    json_key(&j, "state");
    state_json_state(&j);
    json_key(&j, "view");
    state_json_view(&j);
    json_close(&j, '}');
    send_line(&j);
}

static void send_part(const char *key, void (*part)(Json *))
{
    Json j = { 0 };
    json_open(&j, '{');
    json_key(&j, "ok");
    json_bool(&j, 1);
    json_key(&j, key);
    part(&j);
    json_close(&j, '}');
    send_line(&j);
}

static void start_turn(long min_ticks, long max_ticks, int wait_only)
{
    turn.pending = 1;
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
    for (size_t k = 0; k < sizeof actions / sizeof actions[0]; k++) {
        if (strcmp(name, actions[k].name)) continue;
        if (g_candidate_champion_ordinal && strcmp(name, "recruit")) return "a candidate is shown: answer with recruit first";
        if (g_party_is_sleeping && strcmp(name, "wake")) return "the party sleeps: use wake or wait";
        turn.has_damage = look_text.shown = turn.moving = 0;
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
    const char *error = run_action(name, request);
    if (error) send_error(error);
    else start_turn(TURN_MIN_TICKS, TURN_MAX_TICKS, 0);
}

/* Batches. */

/* The latest expiry time of a message row: it grows when the game prints a message. */
static uint32_t latest_message_expiry(void)
{
    uint32_t latest = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] > latest) latest = g_message.expiry[r];
    return latest;
}

/* Remembers what the news checks compare during and after the next turn. */
static void snapshot(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) seen.health[i] = g_champions[i].current_health;
    seen.group_count = state_json_visible_groups(seen.groups);
    seen.message_expiry = latest_message_expiry();
}

/* A creature group is in view that was not in view at the snapshot. */
static int new_group_in_view(void)
{
    uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX];
    int count = state_json_visible_groups(groups);
    for (int i = 0; i < count; i++) {
        int known = 0;
        for (int k = 0; k < seen.group_count; k++) known |= groups[i] == seen.groups[k];
        if (!known) return 1;
    }
    return 0;
}

/* What happened since the snapshot that the player must see, or NULL. */
static const char *news(void)
{
    if (game_over()) return "game_over";
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health < seen.health[i]) return "damage";
    if (new_group_in_view()) return "creature";
    if (latest_message_expiry() > seen.message_expiry) return "message";
    return NULL;
}

/* Why a batch stops after a turn, or NULL to go on. */
static const char *batch_stop_reason(void)
{
    if (!game_over() && turn_bumped()) return "bumped";  /* before damage: a bump into a wall hurts */
    const char *reason = news();
    if (reason) return reason;
    if (batch.moving) {
        int16_t x = turn.x, y = turn.y;
        update_map_coordinates_after_relative_movement(batch.path[batch.path_done], 1, 0, &x, &y);
        int last = batch.path_done + 1 == batch.path_length;
        if (g_party_map_index != turn.map) return last ? NULL : "moved_elsewhere";
        if (g_party_map_x != x || g_party_map_y != y) return "moved_elsewhere";
    }
    return NULL;
}

static void batch_finish(const char *stop, const char *error)
{
    batch.stop = stop;
    batch.error = error;
    send_turn_result();
    free(batch.actions);
    memset(&batch, 0, sizeof batch);
}

/* One turn of the party toward direction. */
static const char *turn_toward(int16_t direction)
{
    return run_action(NORMALIZE(direction - g_party_direction) == 1 ? "turn_right" : "turn_left", "");
}

/* The next goto step: turn toward the next square of the path, or move onto it. */
static const char *goto_step(void)
{
    batch.moving = batch.path[batch.path_done] == g_party_direction;
    if (batch.moving) return run_action("move_forward", "");
    return turn_toward(batch.path[batch.path_done]);
}

/* The next act_many element, a "name" or an {"action": "name", "args": {...}} object, copied to out.
   Returns 1, 0 at the end of the list, or -1 for an item that is not an action. */
static int next_element(char *out, size_t size)
{
    const char *p = batch.next;
    while (*p == ' ' || *p == ',') p++;
    const char *start = p;
    if (!*p || *p == ']') return 0;
    if (*p == '"') {
        p = strchr(p + 1, '"');
        if (!p) return -1;
        p++;
    } else if (*p == '{') {
        for (int depth = 0; *p; p++) {
            if (*p == '"') p = strchr(p + 1, '"');  /* strings hold no '"' (docs/llm-mode.md) */
            if (!p) return -1;
            depth += (*p == '{') - (*p == '}');
            if (!depth) break;
        }
        if (!*p++) return -1;
    } else {
        return -1;
    }
    if ((size_t)(p - start) >= size) return -1;
    memcpy(out, start, (size_t)(p - start));
    out[p - start] = 0;
    batch.next = p;
    return 1;
}

/* Starts the next step of the batch, or ends the batch when it is done or the step cannot run. */
static void batch_next(void)
{
    const char *error;
    char element[INPUT_SIZE / 4], name[ACTION_NAME_SIZE];
    snapshot();
    if (batch.actions) {
        int found = next_element(element, sizeof element);
        if (found <= 0) {
            batch_finish(found ? "failed" : "done", found ? "an item of actions is not an action name or object" : NULL);
            return;
        }
        if (element[0] == '"') snprintf(name, sizeof name, "%.*s", (int)strlen(element) - 2, element + 1);
        else if (!get_string(element, "action", name, sizeof name)) name[0] = 0;
        error = run_action(name, element);
    } else {
        int arrived = batch.path_done == batch.path_length;
        if (arrived && (batch.facing < 0 || batch.facing == g_party_direction)) {
            batch_finish("done", NULL);
            return;
        }
        error = arrived ? turn_toward(batch.facing) : goto_step();
    }
    if (error) batch_finish("failed", error);
    else start_turn(TURN_MIN_TICKS, TURN_MAX_TICKS, 0);
}

/* After a turn of the batch: count it, stop on news, else start the next step. */
static void batch_after_turn(void)
{
    batch.ran++;
    const char *stop = batch_stop_reason();
    if (batch.moving && !stop) batch.path_done++;
    batch.moving = 0;
    if (stop) batch_finish(stop, NULL);
    else batch_next();
}

static void handle_act_many(const char *request)
{
    const char *list = find_value(request, "actions");
    if (!list || *list != '[') {
        send_error("actions must be a list");
        return;
    }
    batch.actions = strdup(list + 1);
    if (!batch.actions) abort();
    batch.next = batch.actions;
    batch.active = 1;
    batch.start_time = g_game_time;
    turn.has_damage = turn.moving = 0;
    batch_next();
}

/* The optional facing of goto: a direction name or 0-3; -1 without facing, -2 when it is not a direction. */
static long goto_facing(const char *request)
{
    char name[8];
    if (!find_value(request, "facing")) return -1;
    if (get_string(request, "facing", name, sizeof name)) {
        for (long d = DIRECTION_NORTH; d <= DIRECTION_WEST; d++)
            if (!strcmp(name, direction_names[d])) return d;
        return -2;
    }
    long d = get_int(request, "facing", -2);
    return d >= DIRECTION_NORTH && d <= DIRECTION_WEST ? d : -2;
}

static void handle_goto(const char *request)
{
    long x = get_int(request, "x", -1), y = get_int(request, "y", -1), facing = goto_facing(request);
    if (facing == -2) {
        send_error("facing must be north, east, south, west, or 0-3");
        return;
    }
    int length = x < 0 || y < 0 ? -1 : state_json_path((int16_t)x, (int16_t)y, batch.path, PATH_MAX_STEPS);
    if (length < 0) {
        send_error("no path over explored, passable squares to this square");
        return;
    }
    batch.path_length = length;
    batch.facing = (int16_t)facing;
    batch.active = 1;
    batch.start_time = g_game_time;
    turn.has_damage = turn.moving = 0;
    batch_next();
}

/* Loads DMGAME.DAT from the save directory, as RESUME does, and answers with the state. */
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
    send_part("state", state_json_state);
}

/* Starts a new game on the start square, as ENTER at the entrance does, and answers with the state. */
static void handle_new_game(void)
{
    if (!gameloop_new_game(gameloop_data_dir())) {
        send_error("cannot load Dungeon.DAT");
        return;
    }
    load_failed = 0;
    send_part("state", state_json_state);
}

static void handle_request(const char *request)
{
    char cmd[32], path[1024];
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
        send_part("state", state_json_state);
    } else if (!strcmp(cmd, "get_view")) {
        send_part("view", state_json_view);
    } else if (!strcmp(cmd, "get_map")) {
        send_part("map", state_json_map);
    } else if (game_over() && (!strcmp(cmd, "act") || !strcmp(cmd, "act_many") || !strcmp(cmd, "goto") || !strcmp(cmd, "wait") ||
                               !strcmp(cmd, "save"))) {
        send_error("game over");
    } else if (!strcmp(cmd, "save")) {
        if (!savegame_save(startend_save_dir())) send_error("cannot write the save file");
        else {
            Json j = { 0 };
            json_raw(&j, "{\"ok\":true}");
            send_line(&j);
        }
    } else if (!strcmp(cmd, "act")) {
        handle_act(request);
    } else if (!strcmp(cmd, "act_many")) {
        handle_act_many(request);
    } else if (!strcmp(cmd, "goto")) {
        handle_goto(request);
    } else if (!strcmp(cmd, "wait")) {
        long ticks = get_int(request, "ticks", 1);
        if (ticks < 1 || ticks > WAIT_MAX_TICKS) send_error("ticks must be 1-5000");
        else {
            turn.has_damage = look_text.shown = turn.moving = 0;
            snapshot();
            start_turn(ticks, ticks, 1);
        }
    } else if (!strcmp(cmd, "screenshot")) {
        if (!get_string(request, "path", path, sizeof path)) send_error("missing path");
        else if (!write_png(path)) send_error("cannot write the PNG file");
        else {
            Json j = { 0 };
            json_raw(&j, "{\"ok\":true,\"path\":");
            json_str(&j, path);
            json_close(&j, '}');
            send_line(&j);
        }
    } else {
        send_error("unknown cmd");
    }
}

/* A turn ends when the party can move again after the minimum ticks, or at the maximum. A wait
   ends early on news. A game that does not tick ends the turn after a frame limit. */
static int turn_finished(void)
{
    long ticks = (long)(g_game_time - turn.start_time);
    if (turn.wait_only && (turn.stop = news())) return 1;
    if (game_over()) return 1;
    if (ticks >= turn.max_ticks || turn.frames > turn.max_ticks * FRAMES_PER_TICK_LIMIT) return 1;
    return !turn.wait_only && ticks >= turn.min_ticks && !g_disabled_movement_ticks;
}

int control_open(const char *path)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    char dir[sizeof socket_path];
    if (strlen(path) >= sizeof address.sun_path) return 0;
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
    if (listen_fd < 0) return 0;
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof address) || listen(listen_fd, 1) ||
        fcntl(listen_fd, F_SETFL, O_NONBLOCK)) {
        control_close();
        return 0;
    }
    return 1;
}

int control_is_turn_pending(void) { return turn.pending; }

void control_poll(void)
{
    if (listen_fd < 0) return;
    if (turn.pending) {
        if (!turn_finished()) {
            turn.frames++;
            run_queue();
            g_stop_waiting_for_input = 1; /* every frame of a turn is a tick */
            return;
        }
        turn.pending = 0;
        if (client_fd < 0) {
            free(batch.actions);
            memset(&batch, 0, sizeof batch);
        } else if (batch.active) batch_after_turn();
        else send_turn_result();
    }
    if (client_fd < 0) {
        client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) return;
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
    if (n > 0) input_length += (size_t)n;
    input[input_length] = 0;
    char *end;
    while (client_fd >= 0 && !turn.pending && (end = strchr(input, '\n'))) {
        *end = 0;
        if (end > input) handle_request(input);
        if (client_fd < 0) return;
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
