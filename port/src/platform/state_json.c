#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state_json.h"
#include "action.h"
#include "adventure.h"
#include "champion.h"
#include "command.h"
#include "dungeon.h"
#include "game.h"
#include "group.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "projectile.h"
#include "spells.h"
#include "tables.h"
#include "things.h"
#include "ui.h"
#include "view.h"
#include "automap.h"

#define STATE_TEXT_SIZE 512
#define MAP_MAX_SIZE 63               /* the widest map row (a map is at most 32 squares wide) */
#define MAP_GRID_PREFIX 3             /* "yy " in front of a grid row */
#define STAMINA_DISPLAY_UNIT 10       /* the inventory shows stamina in tenths of the stored value */

static const char *const direction_names[4] = { "north", "east", "south", "west" };
static const char *const statistic_names[STAT_COUNT] = { "luck", "strength", "dexterity", "wisdom", "vitality", "antimagic", "antifire" };
static const char *const skill_names[SKILL_BASE_COUNT] = { "fighter", "ninja", "priest", "wizard" };
static const char *const wound_names[UI_BODY_SLOT_COUNT] = { "ready_hand", "action_hand", "head", "torso", "legs", "feet" };
/* Relative view cells: on the party square the far cells, on the square ahead the near cells are piles 0-3. */
static const char *const cell_names[4] = { "far_left", "far_right", "near_right", "near_left" };
static const char *const side_names[3] = { "left", "center", "right" };
static const char *const party_cell_names[4] = { "front_left", "front_right", "back_right", "back_left" };
static const char *const slot_names[SLOT_COUNT] = {
    "ready_hand", "action_hand", "head", "torso", "legs", "feet", "pouch_2", "quiver_2", "quiver_3", "quiver_4",
    "neck", "pouch_1", "quiver_1", "backpack_1", "backpack_2", "backpack_3", "backpack_4", "backpack_5", "backpack_6",
    "backpack_7", "backpack_8", "backpack_9", "backpack_10", "backpack_11", "backpack_12", "backpack_13", "backpack_14",
    "backpack_15", "backpack_16", "backpack_17",
};
/* Creature names by creature type. The original shows no creature names; these are the common ones. */
static const char *const creature_names[CREATURE_TYPE_COUNT] = {
    "Giant Scorpion", "Swamp Slime", "Giggler", "Wizard Eye", "Pain Rat", "Ruster", "Screamer", "Rockpile", "Ghost",
    "Stone Golem", "Mummy", "Black Flame", "Skeleton", "Couatl", "Vexirk", "Magenta Worm", "Trolin", "Giant Wasp",
    "Animated Armour", "Materializer", "Water Elemental", "Oitu", "Demon", "Lord Chaos", "Red Dragon", "Lord Order",
    "Grey Lord",
};

/* JSON writer. */

static void put(Json *j, const char *text, size_t n)
{
    if (j->len + n + 1 > j->cap) {
        size_t cap = j->cap ? j->cap : 1024;
        while (cap < j->len + n + 1) cap *= 2;
        char *s = realloc(j->s, cap);
        if (!s) abort();
        j->s = s;
        j->cap = cap;
    }
    memcpy(j->s + j->len, text, n);
    j->len += n;
    j->s[j->len] = 0;
}

static void separate(Json *j)
{
    if (j->len && !strchr("{[:", j->s[j->len - 1])) put(j, ",", 1);
}

void json_raw(Json *j, const char *text) { put(j, text, strlen(text)); }
void json_open(Json *j, char bracket) { separate(j); put(j, &bracket, 1); }
void json_close(Json *j, char bracket) { put(j, &bracket, 1); }
void json_null(Json *j) { separate(j); json_raw(j, "null"); }
void json_bool(Json *j, int value) { separate(j); json_raw(j, value ? "true" : "false"); }

void json_int(Json *j, long value)
{
    char buf[24];
    separate(j);
    snprintf(buf, sizeof buf, "%ld", value);
    json_raw(j, buf);
}

void json_str(Json *j, const char *text)
{
    char buf[8];
    separate(j);
    put(j, "\"", 1);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') {
            buf[0] = '\\';
            buf[1] = (char)*p;
            put(j, buf, 2);
        } else if (*p == '\n') {
            put(j, "\\n", 2);
        } else if (*p < ' ' || *p > '~') {
            snprintf(buf, sizeof buf, "\\u%04x", *p);
            put(j, buf, 6);
        } else {
            put(j, (const char *)p, 1);
        }
    }
    put(j, "\"", 1);
}

void json_key(Json *j, const char *key)
{
    json_str(j, key);
    put(j, ":", 1);
}

static void key_int(Json *j, const char *key, long value) { json_key(j, key); json_int(j, value); }
static void key_str(Json *j, const char *key, const char *text) { json_key(j, key); json_str(j, text); }
static void key_bool(Json *j, const char *key, int value) { json_key(j, key); json_bool(j, value); }

/* A value in tenths as a decimal number, as the load line shows it. */
static void tenths(Json *j, long value)
{
    char buf[24];
    separate(j);
    snprintf(buf, sizeof buf, "%ld.%ld", value / 10, value % 10);
    json_raw(j, buf);
}

static void pair(Json *j, const char *key, long a, long b)
{
    json_key(j, key);
    json_open(j, '[');
    json_int(j, a);
    json_int(j, b);
    json_close(j, ']');
}

/* Objects. */

static const char *object_kind(uint16_t thing)
{
    switch (THING_TYPE(thing)) {
    case THING_WEAPON: return "weapon";
    case THING_ARMOUR: return "armour";
    case THING_SCROLL: return "scroll";
    case THING_POTION: return "potion";
    case THING_CONTAINER: return "container";
    case THING_JUNK: return "junk";
    }
    return "other";
}

static int is_object(uint16_t thing) { return THING_TYPE(thing) >= THING_WEAPON && THING_TYPE(thing) <= THING_JUNK; }

static const char *object_name(uint16_t thing) { return ui_object_name(object_get_icon_index(thing)); }

/* An object as {name, kind}; a held scroll also gives its text. */
static void object_json(Json *j, uint16_t thing, int held)
{
    if (thing == THING_NONE) {
        json_null(j);
        return;
    }
    json_open(j, '{');
    key_str(j, "name", object_name(thing));
    key_str(j, "kind", object_kind(thing));
    if (held && THING_TYPE(thing) == THING_SCROLL) {
        char text[STATE_TEXT_SIZE];
        uint16_t text_thing = (uint16_t)(THING_TEXT << 10 | scroll_text_index(thing));
        if (decode_text(text, sizeof text, text_thing, TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY)) key_str(j, "text", text);
    }
    json_close(j, '}');
}

/* Champions. */

/* The charges of the object in the action hand (1 when the object has no charges). */
static uint16_t action_hand_charges(const Champion *c)
{
    uint16_t thing = c->slots[SLOT_HAND_ACTION];
    switch (THING_TYPE(thing)) {
    case THING_WEAPON: return weapon_charges(thing);
    case THING_ARMOUR: return armour_charges(thing);
    case THING_JUNK: return junk_charges(thing);
    }
    return 1;
}

/* The action menu that the action hand of a champion opens, as menus_set_action_list builds it. */
static void action_list_json(Json *j, int16_t index)
{
    const Champion *c = &g_champions[index];
    int16_t set_index = ACTION_SET_EMPTY_HAND;
    json_open(j, '[');
    if (c->slots[SLOT_HAND_ACTION] != THING_NONE) {
        const ObjectInfo *info = tables_object_info(get_object_info_index(c->slots[SLOT_HAND_ACTION]));
        set_index = info ? info->action_set_index : 0;
    }
    const ActionSet *set = set_index ? tables_action_set(set_index) : NULL;
    if (set) {
        json_str(j, ui_action_name(set->action_indices[0]));
        for (int k = 1; k < ACTION_LIST_COUNT; k++) {
            uint8_t action = set->action_indices[k], level = set->minimum_skill_levels[k - 1];
            const ActionInfo *info = tables_action_info(action);
            if (action == ACTION_NONE || !info || ((level & ACTION_LEVEL_NEEDS_CHARGES) && !action_hand_charges(c))) continue;
            if ((level & ~ACTION_LEVEL_NEEDS_CHARGES) <= champion_get_skill_level(index, info->skill_index))
                json_str(j, ui_action_name(action));
        }
    }
    json_close(j, ']');
}

static void runes_json(Json *j, const char *symbols, int count)
{
    json_open(j, '[');
    for (int k = 0; k < count && symbols[k]; k++) json_str(j, g_rune_names[(uint8_t)symbols[k] - SPELL_SYMBOL_FIRST]);
    json_close(j, ']');
}

static void champion_json(Json *j, int16_t index)
{
    const Champion *c = &g_champions[index];
    json_open(j, '{');
    key_int(j, "index", index);
    key_str(j, "name", c->name);
    key_str(j, "title", c->title);
    key_bool(j, "alive", c->current_health > 0);
    pair(j, "health", c->current_health, c->maximum_health);
    pair(j, "stamina", c->current_stamina / STAMINA_DISPLAY_UNIT, c->maximum_stamina / STAMINA_DISPLAY_UNIT);
    pair(j, "mana", c->current_mana, c->maximum_mana);
    key_int(j, "food", c->food);
    key_int(j, "water", c->water);
    json_key(j, "load_kg");
    json_open(j, '[');
    tenths(j, c->load);
    tenths(j, champion_get_maximum_load(c));
    json_close(j, ']');
    key_str(j, "position", party_cell_names[NORMALIZE(c->cell - g_party_direction)]);
    key_bool(j, "poisoned", c->poison_event_count > 0);
    key_bool(j, "action_disabled", (c->attributes & CHAMPION_ATTR_ACTION_DISABLED) != 0);
    json_key(j, "statistics");
    json_open(j, '{');
    for (int s = 0; s < STAT_COUNT; s++) pair(j, statistic_names[s], c->statistics[s][STAT_CURRENT], c->statistics[s][STAT_MAXIMUM]);
    json_close(j, '}');
    json_key(j, "skills");
    json_open(j, '{');
    for (int s = 0; s < SKILL_BASE_COUNT; s++) {
        int16_t level = champion_get_skill_level(index, (uint16_t)s);
        const char *rank = ui_skill_rank_name(level);
        json_key(j, skill_names[s]);
        json_open(j, '{');
        key_int(j, "level", level);
        json_key(j, "rank");
        if (rank) json_str(j, rank);
        else json_null(j);
        json_close(j, '}');
    }
    json_close(j, '}');
    json_key(j, "wounds");
    json_open(j, '[');
    for (int w = 0; w < UI_BODY_SLOT_COUNT; w++)
        if (c->wounds & 1 << w) json_str(j, wound_names[w]);
    json_close(j, ']');
    json_key(j, "slots");
    json_open(j, '[');
    for (int s = 0; s < SLOT_COUNT; s++) {
        if (c->slots[s] == THING_NONE) continue;
        json_open(j, '{');
        key_int(j, "slot", s);
        key_str(j, "where", slot_names[s]);
        json_key(j, "object");
        object_json(j, c->slots[s], 1);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_key(j, "actions");
    action_list_json(j, index);
    json_key(j, "runes");
    runes_json(j, c->symbols, CHAMPION_SYMBOL_COUNT);
    json_close(j, '}');
}

/* The message area rows that hold text, oldest first. */
static void messages_json(Json *j)
{
    json_open(j, '[');
    for (int r = 0; r < MESSAGE_ROWS; r++) {
        char row[MESSAGE_COLS + 1];
        int n = 0;
        for (int k = 0; k < MESSAGE_COLS; k++) {
            row[k] = g_message.text[r][k] ? g_message.text[r][k] : ' ';
            if (row[k] != ' ') n = k + 1;
        }
        row[n] = 0;
        if (n) json_str(j, row);
    }
    json_close(j, ']');
}

/* The spell area: the caster, the chosen runes, and the six runes of the next step with their mana cost. */
static void spell_json(Json *j)
{
    int16_t caster = g_magic_caster_champion_index;
    if (caster < 0) {
        json_null(j);
        return;
    }
    const Champion *c = &g_champions[caster];
    json_open(j, '{');
    key_int(j, "caster", caster);
    json_key(j, "chosen");
    runes_json(j, c->symbols, CHAMPION_SYMBOL_COUNT);
    json_key(j, "available");
    json_open(j, '[');
    for (int s = 0; s < SYMBOL_COUNT; s++) {
        uint16_t cost = *tables_symbol_base_mana_cost(c->symbol_step, s);
        if (c->symbol_step) cost = (uint16_t)(cost * *tables_symbol_mana_cost_multiplier((uint8_t)c->symbols[0] - SPELL_SYMBOL_FIRST)) >> 3;
        json_open(j, '{');
        key_int(j, "symbol", s);
        key_str(j, "name", g_rune_names[c->symbol_step * SYMBOL_COUNT + s]);
        key_int(j, "mana", cost);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_close(j, '}');
}

static const char *panel_name(void)
{
    switch (g_panel_content) {
    case PANEL_CONTENT_FOOD_WATER: return "food_water";
    case PANEL_CONTENT_SCROLL: return "scroll";
    case PANEL_CONTENT_CHEST: return "chest";
    case PANEL_CONTENT_RESURRECT: return "resurrect";
    }
    return "other";
}

/* The inventory panel: whose inventory is open, what the panel shows, the open chest. */
static void inventory_json(Json *j)
{
    if (!g_inventory_champion_ordinal) {
        json_null(j);
        return;
    }
    json_open(j, '{');
    key_int(j, "champion", g_inventory_champion_ordinal - 1);
    key_str(j, "panel", panel_name());
    if (g_open_chest != THING_NONE) {
        json_key(j, "chest");
        json_open(j, '[');
        for (int s = 0; s < SLOT_CHEST_COUNT; s++) {
            if (g_open_chest_slots[s] == THING_NONE) continue;
            json_open(j, '{');
            key_int(j, "slot", SLOT_CHEST_FIRST + s);
            json_key(j, "object");
            object_json(j, g_open_chest_slots[s], 1);
            json_close(j, '}');
        }
        json_close(j, ']');
    }
    json_close(j, '}');
}

void state_json_state(Json *j)
{
    json_open(j, '{');
    key_int(j, "time", (long)g_game_time);
    key_str(j, "adventure", g_adventure == ADVENTURE_CSB ? "csb" : "dm");
    json_key(j, "party");
    json_open(j, '{');
    key_int(j, "map", g_party_map_index);
    key_int(j, "level", map_level(g_party_map_index));
    key_int(j, "x", g_party_map_x);
    key_int(j, "y", g_party_map_y);
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_int(j, "darkness", g_dungeon_view_palette_index);
    key_bool(j, "can_move", !g_disabled_movement_ticks);
    key_bool(j, "sleeping", g_party_is_sleeping);
    key_bool(j, "dead", g_party_dead);
    json_close(j, '}');
    /* A shown candidate is the last champion record, but not yet in the party. */
    int16_t members = (int16_t)g_party_champion_count, candidate = -1;
    if (g_candidate_champion_ordinal && members) candidate = --members;
    json_key(j, "leader");
    if (g_leader_index < 0 || g_leader_index == candidate) json_null(j);
    else json_int(j, g_leader_index);
    json_key(j, "leader_hand");
    object_json(j, g_leader_hand_object, 1);
    json_key(j, "candidate");
    if (candidate >= 0) champion_json(j, candidate);
    else json_null(j);
    json_key(j, "champions");
    json_open(j, '[');
    for (int16_t i = 0; i < members; i++) champion_json(j, i);
    json_close(j, ']');
    json_key(j, "action_menu");
    if (g_acting_champion_ordinal) {
        json_open(j, '{');
        key_int(j, "champion", g_acting_champion_ordinal - 1);
        json_key(j, "actions");
        json_open(j, '[');
        for (int k = 0; k < ACTION_LIST_COUNT && g_action_list_indices[k] != ACTION_NONE; k++) json_str(j, ui_action_name(g_action_list_indices[k]));
        json_close(j, ']');
        json_close(j, '}');
    } else {
        json_null(j);
    }
    json_key(j, "spell");
    spell_json(j);
    json_key(j, "inventory");
    inventory_json(j);
    json_key(j, "messages");
    messages_json(j);
    json_close(j, '}');
}

/* Squares as a player sees them. */

static const char *door_state_name(int16_t state)
{
    if (state == DOOR_OPEN) return "open";
    if (state == DOOR_CLOSED) return "closed";
    if (state == DOOR_DESTROYED) return "destroyed";
    return "partly_open";
}

/* The look of a square: a fake wall looks like a wall until it opens; a closed pit and a teleporter
   without a field look like floor. */
static const char *square_name(uint8_t square)
{
    switch (SQUARE_TYPE(square)) {
    case SQUARE_CORRIDOR: return "floor";
    case SQUARE_PIT: return square & PIT_OPEN ? "pit" : "floor";
    case SQUARE_STAIRS: return square & STAIRS_UP ? "stairs_up" : "stairs_down";
    case SQUARE_DOOR: return "door";
    case SQUARE_TELEPORTER: return (square & TELEPORTER_OPEN) && (square & TELEPORTER_VISIBLE) ? "teleporter" : "floor";
    case SQUARE_FAKE_WALL: return square & FAKE_WALL_OPEN ? "floor" : "wall";
    }
    return "wall";
}

static int in_party_map(int16_t x, int16_t y)
{
    return x >= 0 && y >= 0 && x < dungeon_map_width(g_party_map_index) && y < dungeon_map_height(g_party_map_index);
}

static uint8_t party_map_square(int16_t x, int16_t y) { return in_party_map(x, y) ? dungeon_square(g_party_map_index, x, y) : 0; }

/* The rows of the view the party sees now: up to the first center square that blocks sight. */
static int16_t visible_depths(void) { return automap_visible_depths(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction); }

static void view_square(int16_t depth, int16_t side, int16_t *x, int16_t *y)
{
    *x = g_party_map_x, *y = g_party_map_y;
    update_map_coordinates_after_relative_movement(g_party_direction, depth, side, x, y);
}

/* What a wall face shows: inscription text, a champion mirror, a switch (button or lever), a lock (a keyhole
   or a slot that takes an object), an alcove, a fountain, a vi altar, or another ornament. */
typedef struct {
    uint16_t text;                    /* THING_NONE: no visible text */
    int mirror, switch_, lock, torch_holder, alcove, fountain, vi_altar, ornament;
    int16_t lock_ornament;            /* index in the wall ornament graphics, -1: unknown */
    int16_t other_ornament;           /* the same for ornament */
} WallFace;

/* The wall ornaments of DM and CSB, by index, named from their picture. */
static const char *const wall_ornament_names[] = {
    [1] = "square_alcove", [2] = "vi_altar", [3] = "arched_alcove", [4] = "hook", [5] = "iron_lock", [6] = "plate_ring",
    [7] = "dent", [8] = "tiny_button", [10] = "iron_ring", [11] = "crack", [12] = "slime_outlet", [14] = "green_button",
    [15] = "blue_button", [16] = "grate", [17] = "coin_slot", [19] = "wooden_lock", [20] = "rune_lock", [21] = "framed_lock",
    [22] = "studded_lock", [23] = "cross_lock", [24] = "gold_lock", [25] = "skull_lock", [26] = "ornate_gold_lock",
    [27] = "blue_lock", [28] = "green_lock", [29] = "diamond_lock", [30] = "sun_lock", [31] = "black_lock", [32] = "gem_hole",
    [33] = "creeper", [34] = "manacles", [35] = "fountain", [36] = "iron_ornament", [37] = "demon_face", [38] = "scratches",
    [39] = "tiny_hole", [40] = "round_hole", [41] = "narrow_slot", [43] = "mirror", [44] = "torch_holder",
    [45] = "empty_torch_holder", [46] = "long_holder", [47] = "red_button", [48] = "eye_socket", [49] = "grey_button",
    [50] = "small_crack", [51] = "green_gem", [52] = "blue_gem", [53] = "brown_button", [54] = "wall_slot", [55] = "wide_crack",
    [56] = "red_gem_roots", [57] = "blue_gem_roots", [58] = "rock_face", [59] = "lord_order",
};
enum { WALL_ORNAMENT_NAME_COUNT = sizeof wall_ornament_names / sizeof *wall_ornament_names };

static int sensor_takes_object(uint16_t type)
{
    switch (type) {
    case SENSOR_WALL_ANY_OBJECT: case SENSOR_WALL_OBJECT: case SENSOR_WALL_OBJECT_REMOVED: case SENSOR_WALL_SINGLE_OBJECT_STORAGE:
    case SENSOR_WALL_SINGLE_OBJECT_EXCHANGER: case SENSOR_WALL_OBJECT_EXCHANGER: case SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED:
        return 1;
    }
    return 0;
}

/* The side of wall square (x, y) that faces direction face. Returns 1 when it shows something. */
static int wall_face(int16_t x, int16_t y, int16_t face, WallFace *f)
{
    memset(f, 0, sizeof *f);
    f->text = THING_NONE;
    f->lock_ornament = f->other_ornament = -1;
    for (uint16_t t = dungeon_square_first_thing(g_party_map_index, x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_CELL(t) != face) continue;
        if (THING_TYPE(t) == THING_TEXT && text_is_visible(t)) f->text = t;
        if (THING_TYPE(t) != THING_SENSOR) continue;
        if (sensor_type(t) == SENSOR_WALL_CHAMPION_PORTRAIT) f->mirror = 1;
        else if (sensor_ornament_ordinal(t) && sensor_type(t) == SENSOR_WALL_ORNAMENT_CLICK) f->switch_ = 1;
        else if (sensor_ornament_ordinal(t) && sensor_takes_object(sensor_type(t))) {
            if (sensor_data(t) >= ICON_TORCH_UNLIT && sensor_data(t) <= ICON_TORCH_LAST) f->torch_holder = 1;  /* it wants a torch */
            else {
                f->lock = 1;
                f->lock_ornament = view_wall_ornament_index(sensor_ornament_ordinal(t));
            }
        }
    }
    switch (view_wall_ornament(x, y, face)) {
    case VIEW_ORNAMENT_ALCOVE: f->alcove = 1; break;
    case VIEW_ORNAMENT_FOUNTAIN: f->fountain = 1; break;
    case VIEW_ORNAMENT_VI_ALTAR: f->vi_altar = 1; break;
    case VIEW_ORNAMENT_OTHER:
        f->ornament = !f->mirror && !f->switch_ && !f->lock && !f->torch_holder;
        f->other_ornament = view_wall_face_ornament_index(x, y, face);
        break;
    }
    return f->text != THING_NONE || f->mirror || f->switch_ || f->lock || f->torch_holder || f->alcove || f->fountain || f->vi_altar || f->ornament;
}

/* A wall ornament under key: its picture name, else its index, else true. */
static void ornament_json(Json *j, const char *key, int16_t index)
{
    if (index >= 0 && index < WALL_ORNAMENT_NAME_COUNT && wall_ornament_names[index]) key_str(j, key, wall_ornament_names[index]);
    else if (index >= 0) key_int(j, key, index);
    else key_bool(j, key, 1);
}

/* The wall face of (x, y) that faces direction face as {facing, features}, under key, when it shows something. */
static void wall_face_json(Json *j, const char *key, int16_t x, int16_t y, int16_t face)
{
    WallFace f;
    char text[STATE_TEXT_SIZE];
    if (!wall_face(x, y, face, &f)) return;
    json_key(j, key);
    json_open(j, '{');
    key_str(j, "facing", direction_names[face]);
    /* the message mode starts the text with a new line */
    if (f.text != THING_NONE && decode_text(text, sizeof text, f.text, TEXT_MODE_MESSAGE)) key_str(j, "text", text + (text[0] == '\n'));
    if (f.mirror) key_bool(j, "mirror", 1);
    if (f.switch_) key_bool(j, "switch", 1);
    if (f.lock) ornament_json(j, "lock", f.lock_ornament);
    if (f.torch_holder) key_bool(j, "torch_holder", 1);
    if (f.alcove) key_bool(j, "alcove", 1);
    if (f.fountain) key_bool(j, "fountain", 1);
    if (f.vi_altar) key_bool(j, "vi_altar", 1);
    if (f.ornament) ornament_json(j, "ornament", f.other_ornament);
    json_close(j, '}');
}

/* The wall faces the view draws: the front of a wall ahead, the lane side of a side wall from depth 1, and
   also the front of a side wall from depth 2. The view draws no ornaments on the side walls of depth 0. */
static void wall_faces_json(Json *j, int16_t depth, int16_t side, int16_t x, int16_t y)
{
    if (side && depth >= 1) wall_face_json(j, "side_face", x, y, NORMALIZE(g_party_direction - side));
    if ((!side && depth >= 1) || depth >= 2) wall_face_json(j, "front_face", x, y, NORMALIZE(g_party_direction + 2));
}

/* The floor ornaments of the game graphics, by index. */
static const char *const floor_ornament_names[] = {
    "grate", "pressure_pad", "moss", "round_grate", "round_pressure_pad", "flame_pit", "crack", "tiny_pressure_pad", "puddle"
};
enum { FLOOR_ORNAMENT_COUNT = sizeof floor_ornament_names / sizeof *floor_ornament_names };

/* The floor ornament that the view draws on the square; an unknown ornament gets its index. */
static void floor_ornament_json(Json *j, int16_t x, int16_t y)
{
    int16_t ornament = view_floor_ornament(x, y);
    if (ornament < 0) return;
    if (ornament < FLOOR_ORNAMENT_COUNT) key_str(j, "floor_ornament", floor_ornament_names[ornament]);
    else key_int(j, "floor_ornament", ornament);
}

const char *state_json_creature_name(uint16_t type) { return type < CREATURE_TYPE_COUNT ? creature_names[type] : "unknown"; }

void state_json_explosion_name(Json *j, uint16_t type)
{
    switch (type) {
    case EXPLOSION_FIREBALL: json_str(j, "fireball"); return;
    case EXPLOSION_SLIME: json_str(j, "slime"); return;
    case EXPLOSION_LIGHTNING_BOLT: json_str(j, "lightning"); return;
    case EXPLOSION_HARM_NON_MATERIAL: json_str(j, "harm_non_material"); return;
    case EXPLOSION_OPEN_DOOR: json_str(j, "open_door"); return;
    case EXPLOSION_POISON_BOLT: json_str(j, "poison_bolt"); return;
    case EXPLOSION_POISON_CLOUD: json_str(j, "poison_cloud"); return;
    case EXPLOSION_SMOKE: json_str(j, "smoke"); return;
    case EXPLOSION_FLUXCAGE: json_str(j, "fluxcage"); return;
    case EXPLOSION_REBIRTH_STEP_1:
    case EXPLOSION_REBIRTH_STEP_2: json_str(j, "rebirth"); return;
    }
    json_int(j, type);
}

/* Creature groups, objects per cell, projectiles, and explosions on a square. */
static void square_things_json(Json *j, int16_t x, int16_t y)
{
    Json objects = { 0 }, flying = { 0 };
    json_open(&objects, '[');
    json_open(&flying, '[');
    for (uint16_t t = dungeon_square_first_thing(g_party_map_index, x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        uint16_t type = THING_TYPE(t);
        if (type == THING_GROUP) {
            uint16_t creature = group_creature_type(t);
            json_key(j, "creatures");
            json_open(j, '{');
            key_str(j, "type", state_json_creature_name(creature));
            key_int(j, "count", group_last_creature_index(t) + 1);
            json_close(j, '}');
        } else if (is_object(t)) {
            json_open(&objects, '{');
            key_str(&objects, "cell", cell_names[NORMALIZE(THING_CELL(t) - g_party_direction)]);
            key_str(&objects, "name", object_name(t));
            json_close(&objects, '}');
        } else if (type == THING_PROJECTILE) {
            uint16_t slot = projectile_slot(t);
            json_open(&flying, '{');
            key_str(&flying, "kind", "projectile");
            key_str(&flying, "name", is_object(slot) ? object_name(slot) : "spell");
            json_close(&flying, '}');
        } else if (type == THING_EXPLOSION) {
            json_open(&flying, '{');
            key_str(&flying, "kind", "explosion");
            key_int(&flying, "type", explosion_type(t));
            json_key(&flying, "name");
            state_json_explosion_name(&flying, explosion_type(t));
            json_close(&flying, '}');
        }
    }
    json_close(&objects, ']');
    json_close(&flying, ']');
    json_key(j, "objects");
    json_raw(j, objects.s);
    json_key(j, "flying");
    json_raw(j, flying.s);
    free(objects.s);
    free(flying.s);
}

/* A square of the view that the screen draws: a side square behind a wall at its depth and one row nearer is hidden. */
static int side_seen(int16_t depth, int16_t side)
{
    return automap_side_is_visible(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction, depth, side);
}

void state_json_view(Json *j)
{
    json_open(j, '{');
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_int(j, "darkness", g_dungeon_view_palette_index);
    json_key(j, "squares");
    json_open(j, '[');
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t x, y;
            if (!side_seen(depth, side)) continue;
            view_square(depth, side, &x, &y);
            uint8_t square = party_map_square(x, y);
            const char *name = square_name(square);
            json_open(j, '{');
            key_int(j, "depth", depth);
            key_str(j, "side", side_names[side + 1]);
            key_int(j, "x", x);
            key_int(j, "y", y);
            key_str(j, "element", name);
            if (in_party_map(x, y)) {
                if (SQUARE_TYPE(square) == SQUARE_DOOR) {
                    uint16_t door = dungeon_square_first_thing(g_party_map_index, x, y);
                    key_str(j, "door", door_state_name(door_state(square)));
                    if (THING_TYPE(door) == THING_DOOR && door != THING_ENDOFLIST) key_bool(j, "button", door_button_present(door));
                }
                if (!strcmp(name, "wall")) {
                    wall_faces_json(j, depth, side, x, y);
                } else {
                    if (depth >= 1) floor_ornament_json(j, x, y);
                    square_things_json(j, x, y);
                }
            } else if (!strcmp(name, "wall")) {
                wall_faces_json(j, depth, side, x, y); /* a border wall outside the map can show a random ornament */
            }
            json_close(j, '}');
        }
    json_close(j, ']');
    json_key(j, "piles");
    json_open(j, '[');
    for (int16_t pile = 0; pile < VIEW_CLICK_BOX_WALL; pile++) {
        uint16_t thing = g_pile_top_objects[pile];
        if (g_dungeon_view_clickable_boxes[pile].x == -1 || object_get_icon_index(thing) == -1) continue;
        json_open(j, '{');
        key_int(j, "cell", pile);
        key_str(j, "top", object_name(thing));
        json_close(j, '}');
    }
    json_close(j, ']');
    key_bool(j, "wall_ahead_clickable", g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL].x != -1);
    int wall_ahead = g_square_ahead_element == VIEW_ELEMENT_WALL; /* the view sets the facing flags only for a wall ahead */
    key_bool(j, "alcove", wall_ahead && g_facing_alcove);
    key_bool(j, "fountain", wall_ahead && g_facing_fountain);
    key_bool(j, "vi_altar", wall_ahead && g_facing_vi_altar);
    json_close(j, '}');
}

int state_json_visible_things(uint16_t type, StateJsonSeenThing *out, int max)
{
    int count = 0;
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t x, y;
            if (!side_seen(depth, side)) continue;
            view_square(depth, side, &x, &y);
            if (!in_party_map(x, y) || !strcmp(square_name(party_map_square(x, y)), "wall")) continue;
            for (uint16_t t = dungeon_square_first_thing(g_party_map_index, x, y); t != THING_ENDOFLIST; t = get_next_thing(t))
                if (THING_TYPE(t) == type && count < max)
                    out[count++] = (StateJsonSeenThing){ depth, x, y, side_names[side + 1], THING_STRIP_CELL(t) };
        }
    return count;
}

int state_json_visible_groups(uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX])
{
    StateJsonSeenThing seen[STATE_JSON_VISIBLE_GROUPS_MAX];
    int count = state_json_visible_things(THING_GROUP, seen, STATE_JSON_VISIBLE_GROUPS_MAX);
    for (int i = 0; i < count; i++) groups[i] = seen[i].thing;
    return count;
}

/* Explored squares: the automap and the squares of the current view, which the automap marks at the next tick. */
static int is_seen(int16_t x, int16_t y)
{
    if (automap_is_seen(g_party_map_index, x, y)) return 1;
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t vx, vy;
            if (!side_seen(depth, side)) continue;
            view_square(depth, side, &vx, &vy);
            if (vx == x && vy == y) return 1;
        }
    return 0;
}

static char map_char(int16_t x, int16_t y)
{
    uint8_t square = dungeon_square(g_party_map_index, x, y);
    const char *name = square_name(square);
    if (!strcmp(name, "wall")) return '#';
    if (!strcmp(name, "pit")) return 'O';
    if (!strcmp(name, "stairs_up")) return '<';
    if (!strcmp(name, "stairs_down")) return '>';
    if (!strcmp(name, "teleporter")) return 'T';
    if (!strcmp(name, "door")) return door_state(square) == DOOR_OPEN || door_state(square) == DOOR_DESTROYED ? 'd' : 'D';
    return view_floor_pressure_pad(x, y) ? '_' : '.';
}

/* Map row y in out: one character per square, a space for a square not explored. */
static void map_row(int16_t y, int16_t w, char *out)
{
    for (int16_t x = 0; x < w; x++)
        out[x] = x == g_party_map_x && y == g_party_map_y ? '@' : is_seen(x, y) ? map_char(x, y) : ' ';
    out[w] = 0;
}

void state_json_map(Json *j)
{
    int16_t w = dungeon_map_width(g_party_map_index), h = dungeon_map_height(g_party_map_index);
    char line[MAP_GRID_PREFIX + MAP_MAX_SIZE + 1];
    if (w > MAP_MAX_SIZE) w = MAP_MAX_SIZE;
    json_open(j, '{');
    key_int(j, "map", g_party_map_index);
    key_int(j, "level", map_level(g_party_map_index));
    key_int(j, "width", w);
    key_int(j, "height", h);
    pair(j, "party", g_party_map_x, g_party_map_y);
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_str(j, "legend", "row index = y (north at the top), column = x; @ party, # wall, . floor, D closed door, "
                         "d open door, < stairs up, > stairs down, O pit, T teleporter, _ pressure pad, space not explored; "
                         "grid: the rows with x mod 10 above and y in front");
    json_key(j, "rows");
    json_open(j, '[');
    for (int16_t y = 0; y < h; y++) {
        map_row(y, w, line);
        json_str(j, line);
    }
    json_close(j, ']');
    json_key(j, "grid");
    json_open(j, '[');
    memset(line, ' ', MAP_GRID_PREFIX);
    for (int16_t x = 0; x < w; x++) line[MAP_GRID_PREFIX + x] = (char)('0' + x % 10);
    line[MAP_GRID_PREFIX + w] = 0;
    json_str(j, line);
    for (int16_t y = 0; y < h; y++) {
        snprintf(line, sizeof line, "%2d ", y);
        map_row(y, w, line + MAP_GRID_PREFIX);
        json_str(j, line);
    }
    json_close(j, ']');
    json_close(j, '}');
}

char *state_json_string(int part)
{
    Json j = { 0 };
    if (part == STATE_JSON_VIEW) state_json_view(&j);
    else if (part == STATE_JSON_MAP) state_json_map(&j);
    else state_json_state(&j);
    return j.s;
}
