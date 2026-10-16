#include "sensor.h"
#include "adventure.h"
#include "champion.h"
#include "dungeon.h"
#include "game.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "sound.h"
#include "tables.h"
#include "timeline_process.h"

#define SENSOR_EXPERIENCE_SKILL   8 /* steal */
#define SENSOR_EXPERIENCE         300
#define VERSION_CHECKER_LIMIT     37 /* the version checker passes for data below this value */
#define CSB_VERSION_CHECKER_LIMIT 32

int16_t g_sensor_rotation_effect = -1, g_sensor_rotation_effect_map_x, g_sensor_rotation_effect_map_y,
        g_sensor_rotation_effect_cell;

/* Adds a square event on the current map. Clear events come first at equal time, then toggle, then set.
   CSB gives each sensor event priority 0. */
void sensor_add_event(int16_t event_type, int16_t x, int16_t y, int16_t cell, int16_t effect, uint32_t time)
{
    static const uint8_t priority[3] = {[EFFECT_SET] = 1, [EFFECT_CLEAR] = 3, [EFFECT_TOGGLE] = 2};
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, time),
        .type = event_type,
        .priority = g_adventure == ADVENTURE_CSB                      ? 0
                    : effect >= EFFECT_SET && effect <= EFFECT_TOGGLE ? priority[effect]
                                                                      : 0,
        .b = event_square(x, y),
        .c = event_cell_effect(cell, effect),
    };
    timeline_add_event(&event);
}

/* All living champions share the experience, or the leader gets all of it. */
static void sensor_add_skill_experience(uint16_t skill, uint16_t experience, bool leader_only)
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

/* Experience for the party, or a rotation of the cell sensors after the sensor pass. */
void sensor_trigger_local_effect(int16_t local_effect, int16_t x, int16_t y, int16_t cell)
{
    if (local_effect == LOCAL_EFFECT_EXPERIENCE) {
        sensor_add_skill_experience(SENSOR_EXPERIENCE_SKILL, SENSOR_EXPERIENCE, cell != ANY_CELL);
        return;
    }
    g_sensor_rotation_effect = local_effect;
    g_sensor_rotation_effect_map_x = x;
    g_sensor_rotation_effect_map_y = y;
    g_sensor_rotation_effect_cell = cell;
}

static bool is_rotation_sensor(uint16_t thing)
{
    return THING_TYPE(thing) == THING_SENSOR &&
           (g_sensor_rotation_effect_cell == ANY_CELL || THING_CELL(thing) == g_sensor_rotation_effect_cell);
}

/* Moves the first sensor of the cell behind the last sensor of the cell. */
void sensor_process_rotation_effect(void)
{
    if (g_sensor_rotation_effect == -1)
        return;
    if (g_sensor_rotation_effect == LOCAL_EFFECT_ROTATE_SENSORS_1 ||
        g_sensor_rotation_effect == LOCAL_EFFECT_ROTATE_SENSORS) {
        int16_t x = g_sensor_rotation_effect_map_x;
        int16_t y = g_sensor_rotation_effect_map_y;
        uint16_t first = map_get_square_first_thing(x, y);
        while (first != THING_ENDOFLIST && !is_rotation_sensor(first))
            first = map_get_next_thing(first);
        uint16_t second = first == THING_ENDOFLIST ? THING_ENDOFLIST : map_get_next_thing(first);
        while (second != THING_ENDOFLIST && !is_rotation_sensor(second))
            second = map_get_next_thing(second);
        if (second != THING_ENDOFLIST) {
            map_unlink_thing_from_list(first, 0, x, y);
            uint16_t last = second;
            for (uint16_t t = map_get_next_thing(second); t != THING_ENDOFLIST && THING_TYPE(t) == THING_SENSOR;
                 t = map_get_next_thing(t))
                if (is_rotation_sensor(t))
                    last = t;
            thing_set_next(first, map_get_next_thing(last));
            thing_set_next(last, first);
        }
    }
    g_sensor_rotation_effect = -1;
}

/* A remote effect becomes a square event on the target after the sensor delay. Only a wall target keeps the target cell. */
void sensor_trigger_effect(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    if (rec_sensor_is_once_only(sensor))
        rec_sensor_disable(sensor);
    if (rec_sensor_is_local(sensor)) {
        sensor_trigger_local_effect(rec_sensor_local_effect(sensor), x, y, cell);
        return;
    }
    int16_t target_x = rec_sensor_target_x(sensor);
    int16_t target_y = rec_sensor_target_y(sensor);
    int16_t target_type = SQUARE_TYPE(g_current_map_data[target_x][target_y]);
    int16_t target_cell = target_type == SQUARE_WALL ? rec_sensor_target_cell(sensor) : 0;
    sensor_add_event(tables_get_square_type_event(target_type), target_x, target_y, target_cell, effect,
                     g_game_time + rec_sensor_delay(sensor));
}

/* The first object of the type in the cell (or in any cell), or THING_NONE. */
uint16_t sensor_get_object_of_type_in_cell(int16_t x, int16_t y, int16_t cell, int16_t object_type)
{
    for (uint16_t thing = map_get_square_first_object(x, y); thing != THING_ENDOFLIST;
         thing = map_get_next_thing(thing))
        if (object_get_type(thing) == object_type && (cell == ANY_CELL || THING_CELL(thing) == cell))
            return thing;
    return THING_NONE;
}

static bool is_object_or_in_chest(uint16_t thing, int16_t object_type)
{
    int16_t type = object_get_type(thing);
    if (type == object_type)
        return true;
    if (type == ICON_CHEST_CLOSED)
        for (uint16_t t = container_first_thing(thing); t != THING_ENDOFLIST; t = map_get_next_thing(t))
            if (object_get_type(t) == object_type)
                return true;
    return false;
}

/* The slots of the living champions, then the leader hand. Chests count with their contents. */
bool sensor_is_object_in_party_possession(int16_t object_type)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            for (int16_t slot = 0; slot < SLOT_COUNT; slot++)
                if (is_object_or_in_chest(g_champions[i].slots[slot], object_type))
                    return true;
    return is_object_or_in_chest(g_leader_hand_object, object_type);
}

static bool holds_object_type(int16_t object_type)
{
    return object_get_type(g_leader_hand_object) == object_type;
}

/* Result of click_test: the sensor does nothing for this click. */
#define CLICK_IGNORED (-1)

/* Single object exchanger: an empty hand takes the object of the sensor type from the cell, a hand with that type puts
   it there. Returns the click_test result. */
static int16_t exchange_single_object(uint16_t sensor, int16_t x, int16_t y)
{
    uint16_t sensor_cell = THING_CELL(sensor);
    int16_t data = rec_sensor_data(sensor);
    if (g_leader_empty_handed) {
        uint16_t object = sensor_get_object_of_type_in_cell(x, y, sensor_cell, data);
        if (object == THING_NONE)
            return CLICK_IGNORED;
        map_unlink_thing_from_list(object, 0, x, y);
        inventory_put_object_in_leader_hand(object);
    } else {
        if (!holds_object_type(data) || sensor_get_object_of_type_in_cell(x, y, sensor_cell, data) != THING_NONE)
            return CLICK_IGNORED;
        uint16_t object = g_leader_hand_object;
        inventory_get_object_removed_from_leader_hand();
        map_link_thing_to_list(THING_WITH_CELL(object, sensor_cell), 0, x, y);
    }
    sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
    return rec_sensor_effect(sensor) == EFFECT_HOLD && !g_leader_empty_handed;
}

/* The test of one wall sensor for a click with the leader hand: 0 passes, 1 fails (a hold sensor clears), or
   CLICK_IGNORED. remaining counts the sensors after this one in its cell. Exchangers and storages move the objects
   here. A removed sensor sets *t to the thing before it. */
static int16_t click_test(uint16_t *t, uint16_t previous, int16_t x, int16_t y, int16_t remaining)
{
    uint16_t sensor = *t;
    uint16_t sensor_cell = THING_CELL(sensor);
    uint16_t type = rec_sensor_type(sensor);
    uint16_t effect = rec_sensor_effect(sensor);
    int16_t data = rec_sensor_data(sensor), skip;
    switch (type) {
    case SENSOR_WALL_ORNAMENT_CLICK:
        return effect == EFFECT_HOLD ? CLICK_IGNORED : 0;
    case SENSOR_WALL_ANY_OBJECT:
        return rec_sensor_revert(sensor) != g_leader_empty_handed;
    case SENSOR_WALL_SINGLE_OBJECT_STORAGE:
    case SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED:
        if (remaining)
            return CLICK_IGNORED;
        /* fall through */
    case SENSOR_WALL_OBJECT:
    case SENSOR_WALL_OBJECT_REMOVED:
        skip = rec_sensor_revert(sensor) == holds_object_type(data);
        if (!skip && type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED && sensor != previous) {
            thing_set_next(previous, map_get_next_thing(sensor));
            thing_set_free(sensor);
            *t = previous;
        }
        if (!skip && type == SENSOR_WALL_SINGLE_OBJECT_STORAGE)
            sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
        return skip;
    case SENSOR_WALL_OBJECT_GENERATOR:
        if (remaining)
            return CLICK_IGNORED;
        skip = !g_leader_empty_handed;
        if (!skip)
            sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
        return skip;
    case SENSOR_WALL_SINGLE_OBJECT_EXCHANGER:
        return exchange_single_object(sensor, x, y);
    case SENSOR_WALL_OBJECT_EXCHANGER: {
        if (remaining)
            return CLICK_IGNORED;
        uint16_t object = map_get_square_first_object(x, y);
        if (!holds_object_type(data) || object == THING_ENDOFLIST)
            return CLICK_IGNORED;
        map_unlink_thing_from_list(object, 0, x, y);
        uint16_t hand = g_leader_hand_object;
        inventory_get_object_removed_from_leader_hand();
        map_link_thing_to_list(THING_WITH_CELL(hand, sensor_cell), 0, x, y);
        inventory_put_object_in_leader_hand(object);
        return 0;
    }
    case SENSOR_WALL_CHAMPION_PORTRAIT:
        champion_add_candidate_to_party(data);
        return CLICK_IGNORED;
    }
    return CLICK_IGNORED;
}

/* Click on the front wall cell with the leader hand. Returns true when a sensor triggered. The object
   generator gives no object until projectile_get_object_for_launcher_or_generator exists. */
bool sensor_is_triggered_by_click_on_wall(int16_t x, int16_t y, uint16_t cell)
{
    bool triggered = false;
    int16_t remaining[CELL_COUNT] = {0};
    uint16_t first = map_get_square_first_thing(x, y);
    for (uint16_t t = first; t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
        if (THING_TYPE(t) == THING_SENSOR)
            remaining[THING_CELL(t)]++;
        else if (THING_TYPE(t) > THING_SENSOR)
            break;
    }
    uint16_t previous = first;
    for (uint16_t t = first; t != THING_ENDOFLIST; previous = t, t = map_get_next_thing(t)) {
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
        int16_t skip = click_test(&t, previous, x, y, remaining[sensor_cell]);
        if (skip == CLICK_IGNORED)
            continue;
        if (effect == EFFECT_HOLD) {
            effect = skip ? EFFECT_CLEAR : EFFECT_SET;
            skip = 0;
        }
        if (skip)
            continue;
        triggered = true;
        if (rec_sensor_is_audible(sensor))
            sound_request_play(SOUND_SWITCH, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
        if (!g_leader_empty_handed &&
            (type == SENSOR_WALL_OBJECT_REMOVED || type == SENSOR_WALL_SINGLE_OBJECT_STORAGE ||
             type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED)) {
            thing_set_free(g_leader_hand_object);
            inventory_get_object_removed_from_leader_hand();
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
    value ^= rec_sensor_revert(sensor);
    if (effect == EFFECT_HOLD)
        effect = value ? EFFECT_SET : EFFECT_CLEAR;
    else if (!value)
        return;
    if (rec_sensor_is_audible(sensor))
        sound_request_play(SOUND_SWITCH, x, y, SOUND_MODE_AFTER_TICK);
    sensor_trigger_effect(sensor, effect, x, y, ANY_CELL);
}

static void print_text(uint16_t text)
{
    char buffer[TEXT_SIZE];
    dungeon_decode_text(buffer, sizeof buffer, text, 1);
    message_print(SENSOR_TEXT_COLOR, buffer, g_game_time);
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
    for (uint16_t t = map_get_square_first_thing(s->x, s->y); t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
        int16_t type = THING_TYPE(t);
        bool is_object;
        if (wall) {
            is_object = THING_CELL(t) == s->cell && type > THING_GROUP;
        } else {
            if (type == THING_GROUP)
                s->group_present = true;
            if (type == THING_TEXT && s->party && s->add && !s->party_on_square)
                print_text(t);
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
        if (s->party && SQUARE_TYPE(square) == SQUARE_STAIRS)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_GROUP:
        if (!s->party && s->thing_type <= THING_GROUP && !s->group_present)
            trigger(sensor, add, x, y);
        break;
    case SENSOR_FLOOR_PARTY_POSSESSION:
        if (s->party)
            trigger(sensor, sensor_is_object_in_party_possession(data), x, y);
        break;
    case SENSOR_FLOOR_VERSION_CHECKER:
        if (s->party && add && !s->party_on_square)
            trigger(sensor, data < (g_adventure == ADVENTURE_CSB ? CSB_VERSION_CHECKER_LIMIT : VERSION_CHECKER_LIMIT),
                    x, y);
        break;
    }
}

/* thing THING_NONE is the party. A floor square runs its floor sensors. A wall square runs the sensors of the thing cell. */
void sensor_process_thing_addition_or_removal(int16_t x, int16_t y, uint16_t thing, bool party_on_square, bool add)
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
    uint8_t square = g_current_map_data[x][y];
    bool wall = SQUARE_TYPE(square) == SQUARE_WALL;
    scan_square(&s, wall);
    if (add && !party)
        map_link_thing_to_list(thing, 0, x, y);
    for (uint16_t t = map_get_square_first_thing(x, y); t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
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

/* No rotation effect waits at the start of a game. */
void sensor_reset(void)
{
    g_sensor_rotation_effect = -1;
    g_sensor_rotation_effect_map_x = 0;
    g_sensor_rotation_effect_map_y = 0;
    g_sensor_rotation_effect_cell = 0;
}

/* Bones put into a vi altar: the rebirth of their champion starts next tick on the party map. */
void sensor_add_vi_altar_rebirth_event(uint16_t bones, int16_t x, int16_t y, int16_t cell)
{
    TimelineEvent rebirth = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + 1),
                             .type = EVENT_VI_ALTAR_REBIRTH,
                             .priority = junk_charges(bones),
                             .b = event_square(x, y),
                             .c = event_cell_effect(cell, VI_ALTAR_REBIRTH_EFFECT)};
    timeline_add_event(&rebirth);
}

/* A press on the button of the door at x, y: a switch sound and the door toggles next tick. */
void sensor_press_door_button(int16_t x, int16_t y)
{
    sound_request_play(SOUND_SWITCH, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    sensor_add_event(EVENT_DOOR, x, y, 0, EFFECT_TOGGLE, (uint32_t)(g_game_time + 1));
}
