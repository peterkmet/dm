/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sensor.h"
#include "adventure.h"
#include "ids.h"
#include "tables.h"
#include "champion.h"
#include "event.h"
#include "game.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "party.h"
#include "record.h"
#include "sound.h"
#include "timeline.h"

/* Experience of a steal sensor and the limits of the version checker. */
#define SENSOR_EXPERIENCE         300
#define SENSOR_PRIORITY_COUNT     3
#define VERSION_CHECKER_LIMIT     37 /* the version checker passes for data below this value */
#define VERSION_CHECKER_LIMIT_CSB 32

/* The rotation effect that a sensor asked for in this pass. */
static struct {
    int16_t effect; /* -1: none */
    Location at;
    int16_t cell;
} rotation = {.effect = -1};

void sensor_reset(void)
{
    rotation.effect = -1;
    rotation.at = (Location){0, 0};
    rotation.cell = 0;
}

void sensor_add_event(int16_t event_type, Location at, int16_t cell, int16_t effect, uint32_t time)
{
    static const uint8_t priority[SENSOR_PRIORITY_COUNT] = {[EFFECT_SET] = 1, [EFFECT_CLEAR] = 3, [EFFECT_TOGGLE] = 2};
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, time),
        .type = event_type,
        .priority = g_adventure == ADVENTURE_CSB                      ? 0
                    : effect >= EFFECT_SET && effect <= EFFECT_TOGGLE ? priority[effect]
                                                                      : 0,
        .b = event_square(at.x, at.y),
        .c = event_cell_effect(cell, effect),
    };
    timeline_add_event(&event);
}

/* All living champions share the experience, or the leader gets all of it. */
static void add_skill_experience(uint16_t skill, uint16_t experience, bool leader_only)
{
    if (leader_only) {
        if (g_leader_index != -1)
            champion_add_skill_experience(g_leader_index, skill, experience);
        return;
    }
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            champion_add_skill_experience(i, skill, experience / g_party_champion_count);
}

static bool is_rotation_sensor(uint16_t thing)
{
    return THING_TYPE(thing) == THING_SENSOR && (rotation.cell == ANY_CELL || THING_CELL(thing) == rotation.cell);
}

void sensor_process_rotation_effect(void)
{
    if (rotation.effect == -1)
        return;
    if (rotation.effect == LOCAL_EFFECT_ROTATE_SENSORS_1 || rotation.effect == LOCAL_EFFECT_ROTATE_SENSORS) {
        int16_t x = rotation.at.x;
        int16_t y = rotation.at.y;
        uint16_t first = map_get_square_first_thing(x, y);
        while (first != THING_ENDOFLIST && !is_rotation_sensor(first))
            first = rec_thing_next(first);
        uint16_t second = first == THING_ENDOFLIST ? THING_ENDOFLIST : rec_thing_next(first);
        while (second != THING_ENDOFLIST && !is_rotation_sensor(second))
            second = rec_thing_next(second);
        if (second != THING_ENDOFLIST) {
            map_unlink_thing_from_list(first, 0, x, y);
            uint16_t last = second;
            for (uint16_t t = rec_thing_next(second); t != THING_ENDOFLIST && THING_TYPE(t) == THING_SENSOR;
                 t = rec_thing_next(t))
                if (is_rotation_sensor(t))
                    last = t;
            rec_thing_set_next(first, rec_thing_next(last));
            rec_thing_set_next(last, first);
        }
    }
    rotation.effect = -1;
}

/* Experience for the party, or a rotation of the cell sensors after the sensor pass. */
static void trigger_local_effect(int16_t local_effect, int16_t x, int16_t y, int16_t cell)
{
    if (local_effect == LOCAL_EFFECT_EXPERIENCE) {
        add_skill_experience(SKILL_STEAL, SENSOR_EXPERIENCE, cell != ANY_CELL);
        return;
    }
    rotation.effect = local_effect;
    rotation.at = (Location){x, y};
    rotation.cell = cell;
}

void sensor_trigger_effect(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    if (rec_sensor_is_once_only(sensor))
        rec_sensor_set_type(sensor, SENSOR_DISABLED);
    if (rec_sensor_is_local(sensor)) {
        trigger_local_effect(rec_sensor_local_effect(sensor), x, y, cell);
        return;
    }
    int16_t target_x = rec_sensor_target_x(sensor);
    int16_t target_y = rec_sensor_target_y(sensor);
    int16_t target_type = rec_square_type(rec_square_byte(g_current_map_index, target_x, target_y));
    int16_t target_cell = target_type == SQUARE_WALL ? rec_sensor_target_cell(sensor) : 0;
    sensor_add_event(tables_get_square_type_event(target_type), (Location){target_x, target_y}, target_cell, effect,
                     g_game_time + rec_sensor_delay(sensor));
}

/* True when thing is an object of object_type, or a closed chest that holds one. */
static bool is_or_contains_object(uint16_t thing, int16_t object_type)
{
    int16_t type = object_get_type(thing);
    if (type == object_type)
        return true;
    if (type == ICON_CHEST_CLOSED)
        for (uint16_t t = rec_container_first_thing(thing); t != THING_ENDOFLIST; t = rec_thing_next(t))
            if (object_get_type(t) == object_type)
                return true;
    return false;
}

static bool holds_object_type(int16_t object_type)
{
    return object_get_type(g_leader_hand_object) == object_type;
}

/* Results of a wall sensor test for one click. */
typedef enum {
    CLICK_RESULT_IGNORED,  /* the sensor does nothing for this click */
    CLICK_RESULT_MATCH,    /* the condition holds: the sensor triggers (a hold sensor sets) */
    CLICK_RESULT_NO_MATCH, /* the condition fails (a hold sensor clears) */
} ClickResult;

typedef struct {
    ClickResult result;
    uint16_t thing; /* the thing where the sensor loop goes on */
} ClickTest;

/* The first object of the type in the cell (or in any cell), or THING_NONE. */
static uint16_t get_object_in_cell(int16_t x, int16_t y, int16_t cell, int16_t object_type)
{
    for (uint16_t thing = map_get_square_first_object(x, y); thing != THING_ENDOFLIST; thing = rec_thing_next(thing))
        if (object_get_type(thing) == object_type && (cell == ANY_CELL || THING_CELL(thing) == cell))
            return thing;
    return THING_NONE;
}

/* Single object exchanger: an empty hand takes the object of the sensor type from the cell, a hand with that type puts
   it there. Returns the click_test result. */
static ClickResult exchange_single_object(uint16_t sensor, int16_t x, int16_t y)
{
    uint16_t sensor_cell = THING_CELL(sensor);
    int16_t data = rec_sensor_data(sensor);
    if (g_leader_empty_handed) {
        uint16_t object = get_object_in_cell(x, y, sensor_cell, data);
        if (object == THING_NONE)
            return CLICK_RESULT_IGNORED;
        map_unlink_thing_from_list(object, 0, x, y);
        inventory_put_hand_object(object);
    } else {
        if (!holds_object_type(data) || get_object_in_cell(x, y, sensor_cell, data) != THING_NONE)
            return CLICK_RESULT_IGNORED;
        uint16_t object = g_leader_hand_object;
        inventory_take_hand_object();
        map_link_thing_to_list(THING_WITH_CELL(object, sensor_cell), 0, x, y);
    }
    trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
    return rec_sensor_effect(sensor) == EFFECT_HOLD && !g_leader_empty_handed ? CLICK_RESULT_NO_MATCH
                                                                              : CLICK_RESULT_MATCH;
}

/* Object exchanger: the hand object and the first object of the square swap places. */
static ClickResult exchange_object(uint16_t sensor, Location at, int16_t remaining)
{
    if (remaining)
        return CLICK_RESULT_IGNORED;
    int16_t x = at.x, y = at.y;
    uint16_t object = map_get_square_first_object(x, y);
    if (!holds_object_type(rec_sensor_data(sensor)) || object == THING_ENDOFLIST)
        return CLICK_RESULT_IGNORED;
    map_unlink_thing_from_list(object, 0, x, y);
    uint16_t hand = g_leader_hand_object;
    inventory_take_hand_object();
    map_link_thing_to_list(THING_WITH_CELL(hand, THING_CELL(sensor)), 0, x, y);
    inventory_put_hand_object(object);
    return CLICK_RESULT_MATCH;
}

/* The test of one wall sensor for a click with the leader hand. remaining counts the sensors after this one in its
   cell. Exchangers and storages move the objects here. A removed sensor gives the thing before it as the loop cursor. */
static ClickTest click_test(uint16_t thing, uint16_t previous, Location at, int16_t remaining)
{
    uint16_t sensor = thing;
    int16_t x = at.x, y = at.y;
    uint16_t sensor_cell = THING_CELL(sensor);
    uint16_t type = rec_sensor_type(sensor);
    uint16_t effect = rec_sensor_effect(sensor);
    int16_t data = rec_sensor_data(sensor);
    bool skip;
    switch (type) {
    case SENSOR_WALL_ORNAMENT_CLICK:
        return (ClickTest){effect == EFFECT_HOLD ? CLICK_RESULT_IGNORED : CLICK_RESULT_MATCH, thing};
    case SENSOR_WALL_ANY_OBJECT:
        return (ClickTest){rec_sensor_is_reverted(sensor) != g_leader_empty_handed ? CLICK_RESULT_NO_MATCH
                                                                                   : CLICK_RESULT_MATCH,
                           thing};
    case SENSOR_WALL_SINGLE_OBJECT_STORAGE:
    case SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED:
        if (remaining)
            return (ClickTest){CLICK_RESULT_IGNORED, thing};
        /* fall through */
        __attribute__((fallthrough));
    case SENSOR_WALL_OBJECT:
    case SENSOR_WALL_OBJECT_REMOVED:
        skip = rec_sensor_is_reverted(sensor) == holds_object_type(data);
        if (!skip && type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED && sensor != previous) {
            rec_thing_set_next(previous, rec_thing_next(sensor));
            rec_thing_set_free(sensor);
            thing = previous;
        }
        if (!skip && type == SENSOR_WALL_SINGLE_OBJECT_STORAGE)
            trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
        return (ClickTest){skip ? CLICK_RESULT_NO_MATCH : CLICK_RESULT_MATCH, thing};
    case SENSOR_WALL_OBJECT_GENERATOR:
        if (remaining)
            return (ClickTest){CLICK_RESULT_IGNORED, thing};
        skip = !g_leader_empty_handed;
        if (!skip)
            trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
        return (ClickTest){skip ? CLICK_RESULT_NO_MATCH : CLICK_RESULT_MATCH, thing};
    case SENSOR_WALL_SINGLE_OBJECT_EXCHANGER:
        return (ClickTest){exchange_single_object(sensor, x, y), thing};
    case SENSOR_WALL_OBJECT_EXCHANGER:
        return (ClickTest){exchange_object(sensor, at, remaining), thing};
    case SENSOR_WALL_CHAMPION_PORTRAIT:
        champion_add_candidate_to_party(data);
        return (ClickTest){CLICK_RESULT_IGNORED, thing};
    }
    return (ClickTest){CLICK_RESULT_IGNORED, thing};
}

bool sensor_is_wall_triggered(int16_t x, int16_t y, int16_t cell)
{
    bool triggered = false;
    int16_t remaining[CELL_COUNT] = {0};
    uint16_t first = map_get_square_first_thing(x, y);
    for (uint16_t t = first; t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        if (THING_TYPE(t) == THING_SENSOR)
            remaining[THING_CELL(t)]++;
        else if (THING_TYPE(t) > THING_SENSOR)
            break;
    }
    uint16_t previous = first;
    for (uint16_t t = first; t != THING_ENDOFLIST; previous = t, t = rec_thing_next(t)) {
        if (THING_TYPE(t) != THING_SENSOR) {
            if (THING_TYPE(t) > THING_SENSOR)
                break;
            continue;
        }
        uint16_t sensor = t;
        uint16_t sensor_cell = THING_CELL(t);
        remaining[sensor_cell]--;
        uint16_t type = rec_sensor_type(sensor);
        uint16_t effect = rec_sensor_effect(sensor);
        if (type == SENSOR_DISABLED || (g_leader_index == -1 && type != SENSOR_WALL_CHAMPION_PORTRAIT) ||
            sensor_cell != cell)
            continue;
        ClickTest test = click_test(t, previous, (Location){x, y}, remaining[sensor_cell]);
        t = test.thing;
        if (test.result == CLICK_RESULT_IGNORED)
            continue;
        if (effect == EFFECT_HOLD)
            effect = test.result == CLICK_RESULT_NO_MATCH ? EFFECT_CLEAR : EFFECT_SET;
        else if (test.result == CLICK_RESULT_NO_MATCH)
            continue;
        triggered = true;
        if (rec_sensor_is_audible(sensor))
            sound_request_play(SOUND_SWITCH, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_AFTER_TICK, 0);
        if (!g_leader_empty_handed &&
            (type == SENSOR_WALL_OBJECT_REMOVED || type == SENSOR_WALL_SINGLE_OBJECT_STORAGE ||
             type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED)) {
            rec_thing_set_free(g_leader_hand_object);
            inventory_take_hand_object();
        }
        sensor_trigger_effect(sensor, effect, x, y, sensor_cell);
    }
    sensor_process_rotation_effect();
    return triggered;
}

/* The revert bit inverts the value. A hold sensor sets or clears with the value; other sensors trigger on a true value. */
static void trigger(uint16_t sensor, bool value, int16_t x, int16_t y)
{
    int16_t effect = rec_sensor_effect(sensor);
    value ^= rec_sensor_is_reverted(sensor);
    if (effect == EFFECT_HOLD)
        effect = value ? EFFECT_SET : EFFECT_CLEAR;
    else if (!value)
        return;
    if (rec_sensor_is_audible(sensor))
        sound_request_play(SOUND_SWITCH, (Location){x, y}, SOUND_MODE_AFTER_TICK, 0);
    sensor_trigger_effect(sensor, effect, x, y, ANY_CELL);
}

/* A thing that enters or leaves a square (THING_NONE: the party), and what else lies there. */
typedef struct {
    int16_t x, y;
    bool add, party, party_on_square;
    int16_t thing_type, object_type;
    uint16_t cell;
    bool group_present, objects_present, same_type_present, other_type_present;
} SquareChange;

/* Finds the groups and the objects on the square: on a wall square only the objects in the cell of the thing. A party
   that enters a floor square reads its texts. */
static void scan_square(SquareChange *s, bool wall)
{
    for (uint16_t t = map_get_square_first_thing(s->x, s->y); t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        int16_t type = THING_TYPE(t);
        bool is_object;
        if (wall) {
            is_object = THING_CELL(t) == s->cell && type > THING_GROUP;
        } else {
            if (type == THING_GROUP)
                s->group_present = true;
            if (type == THING_TEXT && s->party && s->add && !s->party_on_square)
                message_print_dungeon_text(t);
            is_object = type > THING_GROUP && type < THING_PROJECTILE;
        }
        if (is_object) {
            s->objects_present = true;
            s->same_type_present |= object_get_type(t) == s->object_type;
            s->other_type_present |= object_get_type(t) != s->object_type;
        }
    }
}

/* A wall sensor in the cell of an object that enters or leaves an alcove. */
static void process_wall_sensor(uint16_t sensor, const SquareChange *s)
{
    uint16_t type = rec_sensor_type(sensor);
    int16_t data = rec_sensor_data(sensor);
    if ((type == SENSOR_WALL_ORNAMENT_CLICK && !s->objects_present) ||
        (type == SENSOR_WALL_ANY_OBJECT && !s->same_type_present && s->object_type == data) ||
        (type == SENSOR_WALL_OBJECT && !s->other_type_present && s->object_type != data))
        trigger(sensor, s->add, s->x, s->y);
}

/* The slots of the living champions, then the leader hand. Chests count with their contents. */
static bool is_object_possessed(int16_t object_type)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            for (int16_t slot = 0; slot < SLOT_COUNT; slot++)
                if (is_or_contains_object(g_champions[i].slots[slot], object_type))
                    return true;
    return is_or_contains_object(g_leader_hand_object, object_type);
}

/* A floor sensor: each type tests the thing and what else lies on the square. */
static void process_floor_sensor(uint16_t sensor, const SquareChange *s, uint8_t square)
{
    int16_t x = s->x;
    int16_t y = s->y;
    int16_t data = rec_sensor_data(sensor);
    bool add = s->add;
    switch (rec_sensor_type(sensor)) {
    case SENSOR_FLOOR_ANY:
        if (!s->party_on_square && !s->objects_present && !s->group_present)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_PARTY_OR_GROUP:
        if (s->thing_type <= THING_GROUP && !s->party_on_square && !s->group_present)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_PARTY:
        if (!s->party || !g_party_champion_count)
            break;
        if (!data) {
            if (!s->party_on_square)
                trigger(sensor, add, x, y);
        } else {
            trigger(sensor, add && g_party_direction + 1 == data, x, y); /* data is the direction + 1 */
        }
        break;
    case SENSOR_FLOOR_OBJECT:
        if (s->object_type == data && !s->same_type_present)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_PARTY_ON_STAIRS:
        if (s->party && rec_square_type(square) == SQUARE_STAIRS)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_GROUP:
        if (!s->party && s->thing_type <= THING_GROUP && !s->group_present)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_PARTY_POSSESSION:
        if (s->party)
            trigger(sensor, is_object_possessed(data), x, y);
        break;
    case SENSOR_FLOOR_VERSION_CHECKER:
        if (s->party && add && !s->party_on_square)
            trigger(sensor, data < (g_adventure == ADVENTURE_CSB ? VERSION_CHECKER_LIMIT_CSB : VERSION_CHECKER_LIMIT),
                    x, y);
        break;
    }
}

void sensor_process_thing_change(int16_t x, int16_t y, uint16_t thing, bool party_on_square, bool add)
{
    bool party = thing == THING_NONE;
    SquareChange s = {.x = x,
                      .y = y,
                      .add = add,
                      .party = party,
                      .party_on_square = party_on_square,
                      .thing_type = party ? -1 : THING_TYPE(thing),
                      .object_type = party ? -1 : object_get_type(thing),
                      .cell = THING_CELL(thing)};
    if (!add && !party)
        map_unlink_thing_from_list(thing, 0, x, y);
    uint8_t square = rec_square_byte(g_current_map_index, x, y);
    bool wall = rec_square_type(square) == SQUARE_WALL;
    if (wall && party)
        return; /* the party in a wall (walk through walls) runs no wall sensor */
    scan_square(&s, wall);
    if (add && !party)
        map_link_thing_to_list(thing, 0, x, y);
    for (uint16_t t = map_get_square_first_thing(x, y); t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        if (THING_TYPE(t) != THING_SENSOR) {
            if (THING_TYPE(t) > THING_SENSOR)
                break;
            continue;
        }
        if (rec_sensor_type(t) == SENSOR_DISABLED)
            continue;
        if (!wall)
            process_floor_sensor(t, &s, square);
        else if (THING_CELL(t) == s.cell)
            process_wall_sensor(t, &s);
    }
    sensor_process_rotation_effect();
}

void sensor_add_rebirth_event(uint16_t bones, int16_t x, int16_t y, int16_t cell)
{
    TimelineEvent rebirth = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + 1),
                             .type = EVENT_VI_ALTAR_REBIRTH,
                             .priority = rec_junk_charges(bones),
                             .b = event_square(x, y),
                             .c = event_cell_effect(cell, VI_ALTAR_REBIRTH_EFFECT)};
    timeline_add_event(&rebirth);
}

void sensor_press_door_button(int16_t x, int16_t y)
{
    sound_request_play(SOUND_SWITCH, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_AFTER_TICK, 0);
    sensor_add_event(EVENT_DOOR, (Location){x, y}, 0, EFFECT_TOGGLE, (uint32_t)(g_game_time + 1));
}
