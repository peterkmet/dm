/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/state_json.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/ids.h"
#include "data/spells.h"
#include "data/tables.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/explored.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/record.h"
#include "ui/automap.h"
#include "ui/command.h"
#include "ui/panel.h"
#include "ui/ui.h"
#include "ui/view.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_TEXT_SIZE 512
#define MAP_MAX_SIZE    63   /* the widest map row (a map is at most 32 squares wide) */
#define MAP_GRID_PREFIX 3    /* "yy " in front of a grid row */
#define JSON_START_SIZE 1024 /* the first buffer of a JSON text */
#define INT64_TEXT_SIZE 24   /* an int64_t in decimal digits with its sign */

static const char *const direction_names[4] = {"north", "east", "south", "west"};
static const char *const statistic_names[STAT_COUNT] = {"luck",     "strength",  "dexterity", "wisdom",
                                                        "vitality", "antimagic", "antifire"};
static const char *const skill_names[SKILL_BASE_COUNT] = {"fighter", "ninja", "priest", "wizard"};
static const char *const wound_names[UI_BODY_SLOT_COUNT] = {"ready_hand", "action_hand", "head",
                                                            "torso",      "legs",        "feet"};
/* Relative view cells: on the party square the far cells, on the square ahead the near cells are piles 0-3. */
static const char *const cell_names[4] = {"far_left", "far_right", "near_right", "near_left"};
static const char *const side_names[3] = {"left", "center", "right"};
static const char *const party_cell_names[4] = {"front_left", "front_right", "back_right", "back_left"};
static const char *const slot_names[SLOT_COUNT] = {
    "ready_hand",  "action_hand", "head",        "torso",       "legs",        "feet",
    "pouch_2",     "quiver_2",    "quiver_3",    "quiver_4",    "neck",        "pouch_1",
    "quiver_1",    "backpack_1",  "backpack_2",  "backpack_3",  "backpack_4",  "backpack_5",
    "backpack_6",  "backpack_7",  "backpack_8",  "backpack_9",  "backpack_10", "backpack_11",
    "backpack_12", "backpack_13", "backpack_14", "backpack_15", "backpack_16", "backpack_17",
};
/* Creature names by creature type. The original shows no creature names; these are the common ones. */
static const char *const creature_names[CREATURE_TYPE_COUNT] = {
    "Giant Scorpion", "Swamp Slime",  "Giggler",     "Wizard Eye", "Pain Rat",        "Ruster",       "Screamer",
    "Rockpile",       "Ghost",        "Stone Golem", "Mummy",      "Black Flame",     "Skeleton",     "Couatl",
    "Vexirk",         "Magenta Worm", "Trolin",      "Giant Wasp", "Animated Armour", "Materializer", "Water Elemental",
    "Oitu",           "Demon",        "Lord Chaos",  "Red Dragon", "Lord Order",      "Grey Lord",
};

/* JSON writer. */

static void put(Json *j, const char *text, size_t n)
{
    if (j->len + n + 1 > j->cap) {
        size_t cap = j->cap ? j->cap : JSON_START_SIZE;
        while (cap < j->len + n + 1)
            cap *= 2;
        char *s = realloc(j->s, cap);
        if (!s)
            abort();
        j->s = s;
        j->cap = cap;
    }
    memcpy(j->s + j->len, text, n);
    j->len += n;
    j->s[j->len] = 0;
}

/* Puts a comma before the next value unless it opens an object, a list, or follows a key. */
static void separate(Json *j)
{
    if (j->len && !strchr("{[:", j->s[j->len - 1]))
        put(j, ",", 1);
}

void json_raw(Json *j, const char *text)
{
    put(j, text, strlen(text));
}
void json_open(Json *j, char bracket)
{
    separate(j);
    put(j, &bracket, 1);
}
void json_close(Json *j, char bracket)
{
    put(j, &bracket, 1);
}
void json_null(Json *j)
{
    separate(j);
    json_raw(j, "null");
}
void json_bool(Json *j, int value)
{
    separate(j);
    json_raw(j, value ? "true" : "false");
}

void json_int(Json *j, int64_t value)
{
    char buf[INT64_TEXT_SIZE];
    separate(j);
    snprintf(buf, sizeof buf, "%" PRId64, value);
    json_raw(j, buf);
}

void json_str(Json *j, const char *text)
{
    char buf[8];
    separate(j);
    put(j, "\"", 1);
    for (const unsigned char *character = (const unsigned char *)text; *character; character++) {
        if (*character == '"' || *character == '\\') {
            buf[0] = '\\';
            buf[1] = (char)*character;
            put(j, buf, 2);
        } else if (*character == '\n') {
            put(j, "\\n", 2);
        } else if (*character < ' ' || *character > '~') {
            snprintf(buf, sizeof buf, "\\u%04x", *character);
            put(j, buf, 6);
        } else {
            put(j, (const char *)character, 1);
        }
    }
    put(j, "\"", 1);
}

void json_key(Json *j, const char *key)
{
    json_str(j, key);
    put(j, ":", 1);
}

static void key_int(Json *j, const char *key, int64_t value)
{
    json_key(j, key);
    json_int(j, value);
}
static void key_str(Json *j, const char *key, const char *text)
{
    json_key(j, key);
    json_str(j, text);
}
static void key_bool(Json *j, const char *key, int value)
{
    json_key(j, key);
    json_bool(j, value);
}

/* A weight in tenths of a kilogram as a decimal number, as the load line shows it. */
static void tenths(Json *j, int64_t value)
{
    char buf[INT64_TEXT_SIZE];
    separate(j);
    snprintf(buf, sizeof buf, "%" PRId64 ".%" PRId64, value / WEIGHT_TENTHS, value % WEIGHT_TENTHS);
    json_raw(j, buf);
}

/* A key with a list of two numbers, for example current and maximum health. */
static void pair(Json *j, const char *key, int64_t a, int64_t b)
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
    case THING_WEAPON:
        return "weapon";
    case THING_ARMOUR:
        return "armour";
    case THING_SCROLL:
        return "scroll";
    case THING_POTION:
        return "potion";
    case THING_CONTAINER:
        return "container";
    case THING_JUNK:
        return "junk";
    }
    return "other";
}

static bool is_object(uint16_t thing)
{
    return THING_TYPE(thing) >= THING_WEAPON && THING_TYPE(thing) <= THING_JUNK;
}

static const char *object_name(uint16_t thing)
{
    return gfx_get_object_name(object_get_icon_index(thing));
}

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
        uint16_t text_thing = THING_MAKE(THING_TEXT, rec_scroll_text_index(thing));
        if (dungeon_decode_text(text, sizeof text, text_thing, TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY))
            key_str(j, "text", text);
    }
    json_close(j, '}');
}

/* Champions. */

/* The charges of the object in the action hand (1 when the object has no charges). */
static uint16_t action_hand_charges(const Champion *c)
{
    uint16_t thing = c->slots[SLOT_HAND_ACTION];
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        return rec_weapon_charges(thing);
    case THING_ARMOUR:
        return rec_armour_charges(thing);
    case THING_JUNK:
        return rec_junk_charges(thing);
    }
    return 1;
}

/* The action menu that the action hand of a champion opens, as set_action_list builds it. */
static void action_list_json(Json *j, int16_t index)
{
    const Champion *champion = &g_champions[index];
    int16_t set_index = ACTION_SET_EMPTY_HAND;
    json_open(j, '[');
    if (champion->slots[SLOT_HAND_ACTION] != THING_NONE) {
        const ObjectInfo *info =
            tables_get_object_info(inventory_get_object_info_index(champion->slots[SLOT_HAND_ACTION]));
        set_index = info ? info->action_set_index : 0;
    }
    const ActionSet *set = set_index ? tables_get_action_set(set_index) : NULL;
    if (set) {
        json_str(j, gfx_get_action_name(set->action_indices[0]));
        for (int k = 1; k < ACTION_LIST_COUNT; k++) {
            uint8_t action = set->action_indices[k];
            uint8_t level = set->minimum_skill_levels[k - 1];
            const ActionInfo *info = tables_get_action_info(action);
            if (action == ACTION_NONE || !info ||
                ((level & ACTION_LEVEL_NEEDS_CHARGES) && !action_hand_charges(champion)))
                continue;
            if ((level & ~ACTION_LEVEL_NEEDS_CHARGES) <=
                champion_get_skill_level(index, info->skill_index, false, false))
                json_str(j, gfx_get_action_name(action));
        }
    }
    json_close(j, ']');
}

/* The rune names of a spell in progress. */
static void runes_json(Json *j, const char *symbols, int count)
{
    json_open(j, '[');
    for (int k = 0; k < count && symbols[k]; k++)
        json_str(j, g_rune_names[(uint8_t)symbols[k] - SPELL_SYMBOL_FIRST]);
    json_close(j, ']');
}

/* The level and the rank name of each base skill of a champion. */
static void skills_json(Json *j, int16_t index)
{
    json_open(j, '{');
    for (int s = 0; s < SKILL_BASE_COUNT; s++) {
        int16_t level = champion_get_skill_level(index, s, false, false);
        const char *rank = panel_get_rank_name(level);
        json_key(j, skill_names[s]);
        json_open(j, '{');
        key_int(j, "level", level);
        json_key(j, "rank");
        if (rank)
            json_str(j, rank);
        else
            json_null(j);
        json_close(j, '}');
    }
    json_close(j, '}');
}

/* One champion: name, bars, statistics, skills, slots, and state. */
static void champion_json(Json *j, int16_t index)
{
    const Champion *champion = &g_champions[index];
    json_open(j, '{');
    key_int(j, "index", index);
    key_str(j, "name", champion->name);
    key_str(j, "title", champion->title);
    key_bool(j, "alive", champion->current_health > 0);
    pair(j, "health", champion->current_health, champion->maximum_health);
    pair(j, "stamina", champion->current_stamina / STAMINA_DISPLAY_UNIT,
         champion->maximum_stamina / STAMINA_DISPLAY_UNIT);
    pair(j, "mana", champion->current_mana, champion->maximum_mana);
    key_int(j, "food", champion->food);
    key_int(j, "water", champion->water);
    json_key(j, "load_kg");
    json_open(j, '[');
    tenths(j, champion->load);
    tenths(j, champion_get_maximum_load(index));
    json_close(j, ']');
    key_str(j, "position", party_cell_names[NORMALIZE(champion->cell - g_party_direction)]);
    key_bool(j, "poisoned", champion->poison_event_count > 0);
    key_bool(j, "action_disabled", (champion->attributes & CHAMPION_ATTR_ACTION_DISABLED) != 0);
    json_key(j, "statistics");
    json_open(j, '{');
    for (int s = 0; s < STAT_COUNT; s++)
        pair(j, statistic_names[s], champion->statistics[s][STAT_CURRENT], champion->statistics[s][STAT_MAXIMUM]);
    json_close(j, '}');
    json_key(j, "skills");
    skills_json(j, index);
    json_key(j, "wounds");
    json_open(j, '[');
    for (int w = 0; w < UI_BODY_SLOT_COUNT; w++)
        if (champion->wounds & 1 << w)
            json_str(j, wound_names[w]);
    json_close(j, ']');
    json_key(j, "slots");
    json_open(j, '[');
    for (int s = 0; s < SLOT_COUNT; s++) {
        if (champion->slots[s] == THING_NONE)
            continue;
        json_open(j, '{');
        key_int(j, "slot", s);
        key_str(j, "where", slot_names[s]);
        json_key(j, "object");
        object_json(j, champion->slots[s], 1);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_key(j, "actions");
    action_list_json(j, index);
    json_key(j, "runes");
    runes_json(j, champion->symbols, CHAMPION_SYMBOL_COUNT);
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
            if (row[k] != ' ')
                n = k + 1;
        }
        row[n] = 0;
        if (n)
            json_str(j, row);
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
    const Champion *champion = &g_champions[caster];
    json_open(j, '{');
    key_int(j, "caster", caster);
    json_key(j, "chosen");
    runes_json(j, champion->symbols, CHAMPION_SYMBOL_COUNT);
    json_key(j, "available");
    json_open(j, '[');
    for (int s = 0; s < SYMBOL_COUNT; s++) {
        uint16_t cost = spell_get_symbol_mana_cost(champion->symbol_step, s, (uint8_t)champion->symbols[0]);
        json_open(j, '{');
        key_int(j, "symbol", s);
        key_str(j, "name", g_rune_names[champion->symbol_step * SYMBOL_COUNT + s]);
        key_int(j, "mana", cost);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_close(j, '}');
}

/* The protocol name of the content of the inventory panel. */
static const char *panel_name(void)
{
    switch (g_panel_content) {
    case PANEL_CONTENT_FOOD_WATER:
        return "food_water";
    case PANEL_CONTENT_SCROLL:
        return "scroll";
    case PANEL_CONTENT_CHEST:
        return "chest";
    case PANEL_CONTENT_RESURRECT:
        return "resurrect";
    }
    return "other";
}

/* The inventory panel: whose inventory is open, what the panel shows, the open chest. */
static void inventory_json(Json *j)
{
    if (g_session.inventory_champion_index < 0) {
        json_null(j);
        return;
    }
    json_open(j, '{');
    key_int(j, "champion", g_session.inventory_champion_index);
    key_str(j, "panel", panel_name());
    if (g_open_chest != THING_NONE) {
        json_key(j, "chest");
        json_open(j, '[');
        for (int s = 0; s < SLOT_CHEST_COUNT; s++) {
            if (g_open_chest_slots[s] == THING_NONE)
                continue;
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

void state_json_write_state(Json *j)
{
    json_open(j, '{');
    key_int(j, "time", g_game_time);
    key_str(j, "adventure", g_adventure == ADVENTURE_CSB ? "csb" : "dm");
    json_key(j, "party");
    json_open(j, '{');
    key_int(j, "map", g_party_map_index);
    key_int(j, "level", rec_map_level(g_party_map_index));
    key_int(j, "x", g_party_map_x);
    key_int(j, "y", g_party_map_y);
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_int(j, "darkness", g_party_light_level);
    key_bool(j, "can_move", !g_disabled_movement_ticks);
    key_bool(j, "sleeping", g_session.party_sleeping);
    key_bool(j, "dead", g_session.party_dead);
    json_close(j, '}');
    /* A shown candidate is the last champion record, but not yet in the party. */
    int16_t members = g_party_champion_count;
    int16_t candidate = -1;
    if (g_session.candidate_champion_index >= 0 && members)
        candidate = --members;
    json_key(j, "leader");
    if (g_leader_index < 0 || g_leader_index == candidate)
        json_null(j);
    else
        json_int(j, g_leader_index);
    json_key(j, "leader_hand");
    object_json(j, g_leader_hand_object, 1);
    json_key(j, "candidate");
    if (candidate >= 0)
        champion_json(j, candidate);
    else
        json_null(j);
    json_key(j, "champions");
    json_open(j, '[');
    for (int16_t i = 0; i < members; i++)
        champion_json(j, i);
    json_close(j, ']');
    json_key(j, "action_menu");
    if (g_session.acting_champion_index >= 0) {
        json_open(j, '{');
        key_int(j, "champion", g_session.acting_champion_index);
        json_key(j, "actions");
        json_open(j, '[');
        for (int k = 0; k < ACTION_LIST_COUNT && g_action_list.actions[k] != ACTION_NONE; k++)
            json_str(j, gfx_get_action_name(g_action_list.actions[k]));
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
    if (state == DOOR_OPEN)
        return "open";
    if (state == DOOR_CLOSED)
        return "closed";
    if (state == DOOR_DESTROYED)
        return "destroyed";
    return "partly_open";
}

/* The look of a square: a fake wall looks like a wall until it opens; a closed pit and a teleporter
   without a field look like floor. */
static const char *square_name(uint8_t square)
{
    switch (rec_square_type(square)) {
    case SQUARE_CORRIDOR:
        return "floor";
    case SQUARE_PIT:
        return square & PIT_OPEN ? "pit" : "floor";
    case SQUARE_STAIRS:
        return square & STAIRS_UP ? "stairs_up" : "stairs_down";
    case SQUARE_DOOR:
        return "door";
    case SQUARE_TELEPORTER:
        return (square & TELEPORTER_OPEN) && (square & TELEPORTER_VISIBLE) ? "teleporter" : "floor";
    case SQUARE_FAKE_WALL:
        return square & FAKE_WALL_OPEN ? "floor" : "wall";
    }
    return "wall";
}

static bool in_party_map(int16_t x, int16_t y)
{
    return x >= 0 && y >= 0 && x < dungeon_map_width(g_party_map_index) && y < dungeon_map_height(g_party_map_index);
}

static uint8_t lookup_party_square(int16_t x, int16_t y)
{
    return in_party_map(x, y) ? rec_square_byte(g_party_map_index, x, y) : 0;
}

/* The rows of the view the party sees now: up to the first center square that blocks sight. */
static int16_t visible_depths(void)
{
    return automap_get_visible_depths(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction);
}

/* The map square depth squares ahead of the party and side squares to the right. */
static void view_square(int16_t depth, int16_t side, int16_t *x, int16_t *y)
{
    *x = g_party_map_x;
    *y = g_party_map_y;
    map_step(g_party_direction, depth, side, x, y);
}

/* What a wall face shows: inscription text, a champion mirror, a switch (button or lever), a lock (a keyhole
   or a slot that takes an object), an alcove, a fountain, a vi altar, or another ornament. */
typedef struct {
    uint16_t text; /* THING_NONE: no visible text */
    int mirror, switch_, lock, torch_holder, alcove, fountain, vi_altar, ornament;
    int16_t lock_ornament;  /* index in the wall ornament graphics, -1: unknown */
    int16_t other_ornament; /* the same for ornament */
} WallFace;

/* The wall ornaments of DM and CSB, by index, named from their picture. */
static const char *const wall_ornament_names[] = {
    NULL,
    "square_alcove",
    "vi_altar",
    "arched_alcove",
    "hook",
    "iron_lock",
    "plate_ring",
    "dent",
    "tiny_button",
    NULL,
    "iron_ring",
    "crack",
    "slime_outlet",
    NULL,
    "green_button",
    "blue_button",
    "grate",
    "coin_slot",
    NULL,
    "wooden_lock",
    "rune_lock",
    "framed_lock",
    "studded_lock",
    "cross_lock",
    "gold_lock",
    "skull_lock",
    "ornate_gold_lock",
    "blue_lock",
    "green_lock",
    "diamond_lock",
    "sun_lock",
    "black_lock",
    "gem_hole",
    "creeper",
    "manacles",
    "fountain",
    "iron_ornament",
    "demon_face",
    "scratches",
    "tiny_hole",
    "round_hole",
    "narrow_slot",
    NULL,
    "mirror",
    "torch_holder",
    "empty_torch_holder",
    "long_holder",
    "red_button",
    "eye_socket",
    "grey_button",
    "small_crack",
    "green_gem",
    "blue_gem",
    "brown_button",
    "wall_slot",
    "wide_crack",
    "red_gem_roots",
    "blue_gem_roots",
    "rock_face",
    "lord_order",
};
#define WALL_ORNAMENT_NAME_COUNT (ARRAY_COUNT(wall_ornament_names))

/* True for a wall sensor type that reacts to an object put on the wall. */
static bool sensor_takes_object(uint16_t type)
{
    switch (type) {
    case SENSOR_WALL_ANY_OBJECT:
    case SENSOR_WALL_OBJECT:
    case SENSOR_WALL_OBJECT_REMOVED:
    case SENSOR_WALL_SINGLE_OBJECT_STORAGE:
    case SENSOR_WALL_SINGLE_OBJECT_EXCHANGER:
    case SENSOR_WALL_OBJECT_EXCHANGER:
    case SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED:
        return true;
    }
    return false;
}

/* The side of wall square (x, y) that faces direction face. Returns true when it shows something. */
static bool wall_face(int16_t x, int16_t y, int16_t face, WallFace *wall)
{
    memset(wall, 0, sizeof *wall);
    wall->text = THING_NONE;
    wall->lock_ornament = -1;
    wall->other_ornament = -1;
    for (uint16_t thing = map_get_first_thing_at((Position){g_party_map_index, x, y}); thing != THING_ENDOFLIST;
         thing = rec_thing_next(thing)) {
        if (THING_CELL(thing) != face)
            continue;
        if (THING_TYPE(thing) == THING_TEXT && rec_text_is_visible(thing))
            wall->text = thing;
        if (THING_TYPE(thing) != THING_SENSOR)
            continue;
        if (rec_sensor_type(thing) == SENSOR_WALL_CHAMPION_PORTRAIT)
            wall->mirror = 1;
        else if (rec_sensor_ornament(thing) >= 0 && rec_sensor_type(thing) == SENSOR_WALL_ORNAMENT_CLICK)
            wall->switch_ = 1;
        else if (rec_sensor_ornament(thing) >= 0 && sensor_takes_object(rec_sensor_type(thing))) {
            if (rec_sensor_data(thing) >= ICON_TORCH_UNLIT && rec_sensor_data(thing) <= ICON_TORCH_LAST)
                wall->torch_holder = 1; /* it wants a torch */
            else {
                wall->lock = 1;
                wall->lock_ornament = view_get_wall_ornament_index(rec_sensor_ornament(thing));
            }
        }
    }
    switch (view_get_wall_ornament(x, y, face)) {
    case VIEW_ORNAMENT_ALCOVE:
        wall->alcove = 1;
        break;
    case VIEW_ORNAMENT_FOUNTAIN:
        wall->fountain = 1;
        break;
    case VIEW_ORNAMENT_VI_ALTAR:
        wall->vi_altar = 1;
        break;
    case VIEW_ORNAMENT_OTHER:
        wall->ornament = !wall->mirror && !wall->switch_ && !wall->lock && !wall->torch_holder;
        wall->other_ornament = view_get_wall_face_ornament_index(x, y, face);
        break;
    }
    return wall->text != THING_NONE || wall->mirror || wall->switch_ || wall->lock || wall->torch_holder ||
           wall->alcove || wall->fountain || wall->vi_altar || wall->ornament;
}

/* A wall ornament under key: its picture name, else its index, else true. */
static void ornament_json(Json *j, const char *key, int16_t index)
{
    if (index >= 0 && index < WALL_ORNAMENT_NAME_COUNT && wall_ornament_names[index])
        key_str(j, key, wall_ornament_names[index]);
    else if (index >= 0)
        key_int(j, key, index);
    else
        key_bool(j, key, 1);
}

/* The wall face of (x, y) that faces direction face as {facing, features}, under key, when it shows something. */
static void wall_face_json(Json *j, const char *key, int16_t x, int16_t y, int16_t face)
{
    WallFace wall;
    char text[STATE_TEXT_SIZE];
    if (!wall_face(x, y, face, &wall))
        return;
    json_key(j, key);
    json_open(j, '{');
    key_str(j, "facing", direction_names[face]);
    /* the message mode starts the text with a new line */
    if (wall.text != THING_NONE && dungeon_decode_text(text, sizeof text, wall.text, TEXT_MODE_MESSAGE))
        key_str(j, "text", text + (text[0] == '\n'));
    if (wall.mirror)
        key_bool(j, "mirror", 1);
    if (wall.switch_)
        key_bool(j, "switch", 1);
    if (wall.lock)
        ornament_json(j, "lock", wall.lock_ornament);
    if (wall.torch_holder)
        key_bool(j, "torch_holder", 1);
    if (wall.alcove)
        key_bool(j, "alcove", 1);
    if (wall.fountain)
        key_bool(j, "fountain", 1);
    if (wall.vi_altar)
        key_bool(j, "vi_altar", 1);
    if (wall.ornament)
        ornament_json(j, "ornament", wall.other_ornament);
    json_close(j, '}');
}

/* The wall faces the view draws: the front of a wall ahead, the lane side of a side wall from depth 1, and
   also the front of a side wall from depth 2. The view draws no ornaments on the side walls of depth 0. */
static void wall_faces_json(Json *j, int16_t depth, int16_t side, int16_t x, int16_t y)
{
    if (side && depth >= 1)
        wall_face_json(j, "side_face", x, y, NORMALIZE(g_party_direction - side));
    if ((!side && depth >= 1) || depth >= 2)
        wall_face_json(j, "front_face", x, y, direction_back(g_party_direction));
}

/* The floor ornaments of the game graphics, by index. */
static const char *const floor_ornament_names[] = {"grate",       "pressure_pad",       "moss",
                                                   "round_grate", "round_pressure_pad", "flame_pit",
                                                   "crack",       "tiny_pressure_pad",  "puddle"};
#define FLOOR_ORNAMENT_COUNT (ARRAY_COUNT(floor_ornament_names))

/* The floor ornament that the view draws on the square; an unknown ornament gets its index. */
static void floor_ornament_json(Json *j, int16_t x, int16_t y)
{
    int16_t ornament = view_get_floor_ornament(x, y);
    if (ornament < 0)
        return;
    if (ornament < FLOOR_ORNAMENT_COUNT)
        key_str(j, "floor_ornament", floor_ornament_names[ornament]);
    else
        key_int(j, "floor_ornament", ornament);
}

const char *state_json_creature_name(uint16_t type)
{
    return type < CREATURE_TYPE_COUNT ? creature_names[type] : "unknown";
}

void state_json_explosion_name(Json *j, uint16_t type)
{
    switch (type) {
    case EXPLOSION_FIREBALL:
        json_str(j, "fireball");
        return;
    case EXPLOSION_SLIME:
        json_str(j, "slime");
        return;
    case EXPLOSION_LIGHTNING_BOLT:
        json_str(j, "lightning");
        return;
    case EXPLOSION_HARM_NON_MATERIAL:
        json_str(j, "harm_non_material");
        return;
    case EXPLOSION_OPEN_DOOR:
        json_str(j, "open_door");
        return;
    case EXPLOSION_POISON_BOLT:
        json_str(j, "poison_bolt");
        return;
    case EXPLOSION_POISON_CLOUD:
        json_str(j, "poison_cloud");
        return;
    case EXPLOSION_SMOKE:
        json_str(j, "smoke");
        return;
    case EXPLOSION_FLUXCAGE:
        json_str(j, "fluxcage");
        return;
    case EXPLOSION_REBIRTH_STEP_1:
    case EXPLOSION_REBIRTH_STEP_2:
        json_str(j, "rebirth");
        return;
    }
    json_int(j, type);
}

/* Creature groups, objects per cell, projectiles, and explosions on a square. */
static void square_things_json(Json *j, int16_t x, int16_t y)
{
    Json objects = {0};
    Json flying = {0};
    json_open(&objects, '[');
    json_open(&flying, '[');
    for (uint16_t thing = map_get_first_thing_at((Position){g_party_map_index, x, y}); thing != THING_ENDOFLIST;
         thing = rec_thing_next(thing)) {
        uint16_t type = THING_TYPE(thing);
        if (type == THING_GROUP) {
            uint16_t creature = rec_group_creature_type(thing);
            json_key(j, "creatures");
            json_open(j, '{');
            key_str(j, "type", state_json_creature_name(creature));
            key_int(j, "count", rec_group_last_creature(thing) + 1);
            json_close(j, '}');
        } else if (is_object(thing)) {
            json_open(&objects, '{');
            key_str(&objects, "cell", cell_names[NORMALIZE(THING_CELL(thing) - g_party_direction)]);
            key_str(&objects, "name", object_name(thing));
            json_close(&objects, '}');
        } else if (type == THING_PROJECTILE) {
            uint16_t slot = rec_projectile_slot(thing);
            json_open(&flying, '{');
            key_str(&flying, "kind", "projectile");
            key_str(&flying, "name", is_object(slot) ? object_name(slot) : "spell");
            json_close(&flying, '}');
        } else if (type == THING_EXPLOSION) {
            json_open(&flying, '{');
            key_str(&flying, "kind", "explosion");
            key_int(&flying, "type", rec_explosion_type(thing));
            json_key(&flying, "name");
            state_json_explosion_name(&flying, rec_explosion_type(thing));
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
static bool side_seen(int16_t depth, int16_t side)
{
    return automap_side_is_visible(g_party_map_index, g_party_map_x, g_party_map_y, g_party_direction, depth, side);
}

/* The piles that take_object reaches, each with its top object and all its objects, top first. */
static void piles_json(Json *j)
{
    json_key(j, "piles");
    json_open(j, '[');
    for (int16_t pile = 0; pile < VIEW_CLICK_BOX_WALL; pile++) {
        uint16_t thing = g_pile_top_objects[pile];
        if (g_dungeon_view_clickable_boxes[pile].x == -1 || object_get_icon_index(thing) == -1)
            continue;
        json_open(j, '{');
        key_int(j, "cell", pile);
        key_str(j, "top", object_name(thing));
        json_key(j, "objects");
        json_open(j, '[');
        for (int16_t i = g_pile_object_counts[pile] - 1; i >= 0; i--) {
            uint16_t object = g_pile_objects[pile][i];
            if (object_get_icon_index(object) != -1)
                json_str(j, object_name(object));
        }
        json_close(j, ']');
        json_close(j, '}');
    }
    json_close(j, ']');
}

void state_json_write_view(Json *j)
{
    json_open(j, '{');
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_int(j, "darkness", g_party_light_level);
    json_key(j, "squares");
    json_open(j, '[');
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t x, y;
            if (!side_seen(depth, side))
                continue;
            view_square(depth, side, &x, &y);
            uint8_t square = lookup_party_square(x, y);
            const char *name = square_name(square);
            json_open(j, '{');
            key_int(j, "depth", depth);
            key_str(j, "side", side_names[side + 1]);
            key_int(j, "x", x);
            key_int(j, "y", y);
            key_str(j, "element", name);
            if (in_party_map(x, y)) {
                if (rec_square_type(square) == SQUARE_DOOR) {
                    uint16_t door = map_get_first_thing_at((Position){g_party_map_index, x, y});
                    key_str(j, "door", door_state_name(rec_door_state(square)));
                    if (THING_TYPE(door) == THING_DOOR && door != THING_ENDOFLIST)
                        key_bool(j, "button", rec_door_has_button(door));
                }
                if (!strcmp(name, "wall")) {
                    wall_faces_json(j, depth, side, x, y);
                } else {
                    if (depth >= 1)
                        floor_ornament_json(j, x, y);
                    square_things_json(j, x, y);
                }
            } else if (!strcmp(name, "wall")) {
                wall_faces_json(j, depth, side, x, y); /* a border wall outside the map can show a random ornament */
            }
            json_close(j, '}');
        }
    json_close(j, ']');
    piles_json(j);
    key_bool(j, "wall_ahead_clickable", g_dungeon_view_clickable_boxes[VIEW_CLICK_BOX_WALL].x != -1);
    int wall_ahead =
        g_square_ahead_element == VIEW_ELEMENT_WALL; /* the view sets the facing flags only for a wall ahead */
    key_bool(j, "alcove", wall_ahead && g_facing_alcove);
    key_bool(j, "fountain", wall_ahead && g_facing_fountain);
    key_bool(j, "vi_altar", wall_ahead && g_facing_vi_altar);
    json_close(j, '}');
}

int state_json_write_visible_things(uint16_t type, StateJsonSeenThing *out, int max)
{
    int count = 0;
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t x, y;
            if (!side_seen(depth, side))
                continue;
            view_square(depth, side, &x, &y);
            if (!in_party_map(x, y) || !strcmp(square_name(lookup_party_square(x, y)), "wall"))
                continue;
            for (uint16_t t = map_get_first_thing_at((Position){g_party_map_index, x, y}); t != THING_ENDOFLIST;
                 t = rec_thing_next(t))
                if (THING_TYPE(t) == type && count < max)
                    out[count++] = (StateJsonSeenThing){depth, x, y, side_names[side + 1], THING_STRIP_CELL(t)};
        }
    return count;
}

int state_json_write_visible_groups(uint16_t groups[STATE_JSON_VISIBLE_GROUPS_MAX])
{
    StateJsonSeenThing seen[STATE_JSON_VISIBLE_GROUPS_MAX];
    int count = state_json_write_visible_things(THING_GROUP, seen, STATE_JSON_VISIBLE_GROUPS_MAX);
    for (int i = 0; i < count; i++)
        groups[i] = seen[i].thing;
    return count;
}

/* Explored squares: the automap and the squares of the current view, which the automap marks at the next tick. */
static bool is_seen(int16_t x, int16_t y)
{
    if (explored_is_seen(g_party_map_index, x, y))
        return true;
    for (int16_t depth = 0; depth < visible_depths(); depth++)
        for (int16_t side = -1; side <= 1; side++) {
            int16_t vx, vy;
            if (!side_seen(depth, side))
                continue;
            view_square(depth, side, &vx, &vy);
            if (vx == x && vy == y)
                return true;
        }
    return false;
}

/* The character of square (x, y) of the party map in the text map. */
static char map_char(int16_t x, int16_t y)
{
    uint8_t square = rec_square_byte(g_party_map_index, x, y);
    const char *name = square_name(square);
    if (!strcmp(name, "wall"))
        return '#';
    if (!strcmp(name, "pit"))
        return 'O';
    if (!strcmp(name, "stairs_up"))
        return '<';
    if (!strcmp(name, "stairs_down"))
        return '>';
    if (!strcmp(name, "teleporter"))
        return 'T';
    if (!strcmp(name, "door"))
        return rec_door_state(square) == DOOR_OPEN || rec_door_state(square) == DOOR_DESTROYED ? 'd' : 'D';
    return view_is_pressure_pad(x, y) ? '_' : '.';
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
    int16_t width = dungeon_map_width(g_party_map_index);
    int16_t height = dungeon_map_height(g_party_map_index);
    char line[MAP_GRID_PREFIX + MAP_MAX_SIZE + 1];
    if (width > MAP_MAX_SIZE)
        width = MAP_MAX_SIZE;
    json_open(j, '{');
    key_int(j, "map", g_party_map_index);
    key_int(j, "level", rec_map_level(g_party_map_index));
    key_int(j, "width", width);
    key_int(j, "height", height);
    pair(j, "party", g_party_map_x, g_party_map_y);
    key_str(j, "facing", direction_names[NORMALIZE(g_party_direction)]);
    key_str(j, "legend",
            "row index = y (north at the top), column = x; @ party, # wall, . floor, D closed door, "
            "d open door, < stairs up, > stairs down, O pit, T teleporter, _ pressure pad, space not explored; "
            "grid: the rows with x mod 10 above and y in front");
    json_key(j, "rows");
    json_open(j, '[');
    for (int16_t y = 0; y < height; y++) {
        map_row(y, width, line);
        json_str(j, line);
    }
    json_close(j, ']');
    json_key(j, "grid");
    json_open(j, '[');
    memset(line, ' ', MAP_GRID_PREFIX);
    for (int16_t x = 0; x < width; x++)
        line[MAP_GRID_PREFIX + x] = (char)('0' + x % DECIMAL_BASE);
    line[MAP_GRID_PREFIX + width] = 0;
    json_str(j, line);
    for (int16_t y = 0; y < height; y++) {
        snprintf(line, sizeof line, "%2d ", y);
        map_row(y, width, line + MAP_GRID_PREFIX);
        json_str(j, line);
    }
    json_close(j, ']');
    json_close(j, '}');
}

char *state_json_get_string(int part)
{
    Json j = {0};
    if (part == STATE_JSON_VIEW)
        state_json_write_view(&j);
    else if (part == STATE_JSON_MAP)
        state_json_map(&j);
    else
        state_json_write_state(&j);
    return j.s;
}
