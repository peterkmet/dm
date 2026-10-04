#include "sensor.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "sound.h"
#include "timeline_process.h"

/* KAOS table: the event type that a sensor sends to each square type. */
#define KAOS_SQUARE_TYPE_TO_EVENT_TYPE 0x100d4a

#define SENSOR_EXPERIENCE_SKILL 8   /* steal */
#define SENSOR_EXPERIENCE 300
#define VERSION_CHECKER_LIMIT 37   /* the version checker passes for data below this value */
#define TEXT_SIZE 256

int16_t g_sensor_rotation_effect = -1, g_sensor_rotation_effect_map_x, g_sensor_rotation_effect_map_y, g_sensor_rotation_effect_cell;

/* Adds a square event on the current map. Clear events come first at equal time, then toggle, then set. */
void sensor_add_event(int16_t event_type, int16_t x, int16_t y, int16_t cell, int16_t effect, uint32_t time)
{
    static const uint8_t priority[3] = {[EFFECT_SET] = 1, [EFFECT_CLEAR] = 3, [EFFECT_TOGGLE] = 2};
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, time),
        .type = (uint8_t)event_type,
        .priority = effect >= EFFECT_SET && effect <= EFFECT_TOGGLE ? priority[effect] : 0,
        .b = EVENT_SQUARE(x, y),
        .c = EVENT_CELL_EFFECT(cell, effect),
    };
    timeline_add_event(&event);
}

/* All living champions share the experience, or the leader gets all of it. */
static void sensor_add_skill_experience(uint16_t skill, uint16_t experience, int16_t leader_only)
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

static int16_t is_rotation_sensor(uint16_t thing)
{
    return THING_TYPE(thing) == THING_SENSOR &&
           (g_sensor_rotation_effect_cell == ANY_CELL || THING_CELL(thing) == (uint16_t)g_sensor_rotation_effect_cell);
}

/* Moves the first sensor of the cell behind the last sensor of the cell. */
void sensor_process_rotation_effect(void)
{
    if (g_sensor_rotation_effect == -1)
        return;
    if (g_sensor_rotation_effect == LOCAL_EFFECT_ROTATE_SENSORS_1 || g_sensor_rotation_effect == LOCAL_EFFECT_ROTATE_SENSORS) {
        int16_t x = g_sensor_rotation_effect_map_x, y = g_sensor_rotation_effect_map_y;
        uint16_t first = get_square_first_thing(x, y);
        while (first != THING_ENDOFLIST && !is_rotation_sensor(first))
            first = get_next_thing(first);
        uint16_t second = first == THING_ENDOFLIST ? THING_ENDOFLIST : get_next_thing(first);
        while (second != THING_ENDOFLIST && !is_rotation_sensor(second))
            second = get_next_thing(second);
        if (second != THING_ENDOFLIST) {
            unlink_thing_from_list(first, 0, x, y);
            uint16_t last = second;
            for (uint16_t t = get_next_thing(second); t != THING_ENDOFLIST && THING_TYPE(t) == THING_SENSOR; t = get_next_thing(t))
                if (is_rotation_sensor(t))
                    last = t;
            thing_set_next(first, get_next_thing(last));
            thing_set_next(last, first);
        }
    }
    g_sensor_rotation_effect = -1;
}

/* A remote effect becomes a square event on the target after the sensor delay. Only a wall target keeps the target cell. */
void sensor_trigger_effect(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    if (sensor_is_once_only(sensor))
        sensor_disable(sensor);
    if (sensor_is_local(sensor)) {
        sensor_trigger_local_effect(sensor_local_effect(sensor), x, y, cell);
        return;
    }
    int16_t target_x = sensor_target_x(sensor), target_y = sensor_target_y(sensor);
    int16_t target_type = SQUARE_TYPE(g_current_map_data[target_x][target_y]);
    int16_t target_cell = target_type == SQUARE_WALL ? sensor_target_cell(sensor) : 0;
    sensor_add_event(kaos_u8(KAOS_SQUARE_TYPE_TO_EVENT_TYPE + target_type), target_x, target_y, target_cell, effect,
                     g_game_time + sensor_delay(sensor));
}

/* The first object of the type in the cell (or in any cell), or THING_NONE. */
uint16_t sensor_get_object_of_type_in_cell(int16_t x, int16_t y, int16_t cell, int16_t object_type)
{
    for (uint16_t thing = get_square_first_object(x, y); thing != THING_ENDOFLIST; thing = get_next_thing(thing))
        if (object_get_type(thing) == object_type && (cell == ANY_CELL || THING_CELL(thing) == (uint16_t)cell))
            return thing;
    return THING_NONE;
}

static int16_t is_object_or_in_chest(uint16_t thing, int16_t object_type)
{
    int16_t type = object_get_type(thing);
    if (type == object_type)
        return 1;
    if (type == ICON_CHEST_CLOSED)
        for (uint16_t t = container_first_thing(thing); t != THING_ENDOFLIST; t = get_next_thing(t))
            if (object_get_type(t) == object_type)
                return 1;
    return 0;
}

/* The slots of the living champions, then the leader hand. Chests count with their contents. */
int16_t sensor_is_object_in_party_possession(int16_t object_type)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            for (int16_t slot = 0; slot < SLOT_COUNT; slot++)
                if (is_object_or_in_chest(g_champions[i].slots[slot], object_type))
                    return 1;
    return is_object_or_in_chest(g_leader_hand_object, object_type);
}

static int16_t holds_object_type(uint16_t object_type)
{
    return object_get_type(g_leader_hand_object) == (int16_t)object_type;
}

/* Click on the front wall cell with the leader hand. Returns 1 when a sensor triggered. The object
   generator gives no object until get_object_for_launcher_or_generator exists. */
int16_t sensor_is_triggered_by_click_on_wall(int16_t x, int16_t y, uint16_t cell)
{
    int16_t triggered = 0, remaining[4] = {0};
    uint16_t first = get_square_first_thing(x, y);
    for (uint16_t t = first; t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_TYPE(t) == THING_SENSOR)
            remaining[THING_CELL(t)]++;
        else if (THING_TYPE(t) > THING_SENSOR)
            break;
    }
    uint16_t previous = first;
    for (uint16_t t = first; t != THING_ENDOFLIST; previous = t, t = get_next_thing(t)) {
        if (THING_TYPE(t) != THING_SENSOR) {
            if (THING_TYPE(t) > THING_SENSOR)
                break;
            continue;
        }
        uint16_t sensor = t, sensor_cell = THING_CELL(t);
        remaining[sensor_cell]--;
        uint16_t type = sensor_type(sensor), data = sensor_data(sensor), effect = sensor_effect(sensor);
        if (type == SENSOR_DISABLED || (g_leader_index == -1 && type != SENSOR_WALL_CHAMPION_PORTRAIT) || sensor_cell != cell)
            continue;
        int16_t skip;
        switch (type) {
        case SENSOR_WALL_ORNAMENT_CLICK:
            if (effect == EFFECT_HOLD)
                continue;
            skip = 0;
            break;
        case SENSOR_WALL_ANY_OBJECT:
            skip = sensor_revert(sensor) != g_leader_empty_handed;
            break;
        case SENSOR_WALL_SINGLE_OBJECT_STORAGE:
        case SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED:
            if (remaining[sensor_cell])
                continue;
            /* fall through */
        case SENSOR_WALL_OBJECT:
        case SENSOR_WALL_OBJECT_REMOVED:
            skip = sensor_revert(sensor) == holds_object_type(data);
            if (!skip && type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED && t != previous) {
                thing_set_next(previous, get_next_thing(sensor));
                thing_set_free(sensor);
                t = previous;
            }
            if (!skip && type == SENSOR_WALL_SINGLE_OBJECT_STORAGE)
                sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
            break;
        case SENSOR_WALL_OBJECT_GENERATOR:
            if (remaining[sensor_cell])
                continue;
            skip = !g_leader_empty_handed;
            if (!skip)
                sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
            break;
        case SENSOR_WALL_SINGLE_OBJECT_EXCHANGER:
            if (g_leader_empty_handed) {
                uint16_t object = sensor_get_object_of_type_in_cell(x, y, sensor_cell, data);
                if (object == THING_NONE)
                    continue;
                unlink_thing_from_list(object, 0, x, y);
                champion_put_object_in_leader_hand(object);
            } else {
                if (!holds_object_type(data) || sensor_get_object_of_type_in_cell(x, y, sensor_cell, data) != THING_NONE)
                    continue;
                uint16_t object = g_leader_hand_object;
                champion_get_object_removed_from_leader_hand();
                link_thing_to_list(THING_WITH_CELL(object, sensor_cell), 0, x, y);
            }
            sensor_trigger_local_effect(LOCAL_EFFECT_ROTATE_SENSORS, x, y, sensor_cell);
            skip = effect == EFFECT_HOLD && !g_leader_empty_handed;
            break;
        case SENSOR_WALL_OBJECT_EXCHANGER: {
            if (remaining[sensor_cell])
                continue;
            uint16_t object = get_square_first_object(x, y);
            if (!holds_object_type(data) || object == THING_ENDOFLIST)
                continue;
            unlink_thing_from_list(object, 0, x, y);
            uint16_t hand = g_leader_hand_object;
            champion_get_object_removed_from_leader_hand();
            link_thing_to_list(THING_WITH_CELL(hand, sensor_cell), 0, x, y);
            champion_put_object_in_leader_hand(object);
            skip = 0;
            break;
        }
        case SENSOR_WALL_CHAMPION_PORTRAIT:
            champion_add_candidate_to_party((int16_t)data);
            continue;
        default:
            continue;
        }
        if (effect == EFFECT_HOLD) {
            effect = skip ? EFFECT_CLEAR : EFFECT_SET;
            skip = 0;
        }
        if (skip)
            continue;
        triggered = 1;
        if (sensor_is_audible(sensor))
            sound_request_play(SOUND_SWITCH, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
        if (!g_leader_empty_handed && (type == SENSOR_WALL_OBJECT_REMOVED || type == SENSOR_WALL_SINGLE_OBJECT_STORAGE ||
                                       type == SENSOR_WALL_OBJECT_REMOVED_SENSOR_REMOVED)) {
            thing_set_free(g_leader_hand_object);
            champion_get_object_removed_from_leader_hand();
        }
        sensor_trigger_effect(sensor, effect, x, y, sensor_cell);
    }
    sensor_process_rotation_effect();
    return triggered;
}

/* The revert bit inverts the value. A hold sensor sets or clears with the value; other sensors trigger on a true value. */
static void trigger(uint16_t sensor, int16_t value, int16_t x, int16_t y)
{
    int16_t effect = sensor_effect(sensor);
    value ^= sensor_revert(sensor);
    if (effect == EFFECT_HOLD)
        effect = value ? EFFECT_SET : EFFECT_CLEAR;
    else if (!value)
        return;
    if (sensor_is_audible(sensor))
        sound_request_play(SOUND_SWITCH, x, y, SOUND_MODE_AFTER_TICK);
    sensor_trigger_effect(sensor, effect, x, y, ANY_CELL);
}

static void print_text(uint16_t text)
{
    char buffer[TEXT_SIZE];
    decode_text(buffer, sizeof buffer, text, 1);
    message_print(SENSOR_TEXT_COLOR, buffer, g_game_time);
}

/* A wall sensor in the cell of an object that enters or leaves an alcove. */
static void process_wall_sensor(uint16_t sensor, int16_t object_type, int16_t objects_present, int16_t same_type_present,
                                int16_t other_type_present, int16_t add, int16_t x, int16_t y)
{
    uint16_t type = sensor_type(sensor), data = sensor_data(sensor);
    if ((type == SENSOR_WALL_ORNAMENT_CLICK && !objects_present) ||
        (type == SENSOR_WALL_ANY_OBJECT && !same_type_present && object_type == (int16_t)data) ||
        (type == SENSOR_WALL_OBJECT && !other_type_present && object_type != (int16_t)data))
        trigger(sensor, add, x, y);
}

/* thing THING_NONE is the party. A floor square runs its floor sensors. A wall square runs the sensors of the thing cell. */
void sensor_process_thing_addition_or_removal(int16_t x, int16_t y, uint16_t thing, int16_t party_on_square, int16_t add)
{
    int16_t party = thing == THING_NONE, thing_type = party ? -1 : THING_TYPE(thing);
    int16_t object_type = party ? -1 : object_get_type(thing);
    if (!add && !party)
        unlink_thing_from_list(thing, 0, x, y);
    uint8_t square = g_current_map_data[x][y];
    int16_t wall = SQUARE_TYPE(square) == SQUARE_WALL;
    uint16_t cell = THING_CELL(thing);
    int16_t group_present = 0, objects_present = 0, same_type_present = 0, other_type_present = 0;
    for (uint16_t t = get_square_first_thing(x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        int16_t type = THING_TYPE(t), is_object;
        if (wall) {
            is_object = THING_CELL(t) == cell && type > THING_GROUP;
        } else {
            if (type == THING_GROUP)
                group_present = 1;
            if (type == THING_TEXT && party && add && !party_on_square)
                print_text(t);
            is_object = type > THING_GROUP && type < THING_PROJECTILE;
        }
        if (is_object) {
            objects_present = 1;
            same_type_present |= object_get_type(t) == object_type;
            other_type_present |= object_get_type(t) != object_type;
        }
    }
    if (add && !party)
        link_thing_to_list(thing, 0, x, y);
    for (uint16_t t = get_square_first_thing(x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_TYPE(t) != THING_SENSOR) {
            if (THING_TYPE(t) > THING_SENSOR)
                break;
            continue;
        }
        uint16_t type = sensor_type(t), data = sensor_data(t);
        if (type == SENSOR_DISABLED)
            continue;
        if (wall) {
            if (THING_CELL(t) == cell)
                process_wall_sensor(t, object_type, objects_present, same_type_present, other_type_present, add, x, y);
            continue;
        }
        switch (type) {
        case SENSOR_FLOOR_ANY:
            if (!party_on_square && !objects_present && !group_present)
                trigger(t, add, x, y);
            break;
        case SENSOR_FLOOR_PARTY_OR_GROUP:
            if (thing_type <= THING_GROUP && !party_on_square && !group_present)
                trigger(t, add, x, y);
            break;
        case SENSOR_FLOOR_PARTY:
            if (!party || !g_party_champion_count)
                break;
            if (!data) {
                if (!party_on_square)
                    trigger(t, add, x, y);
            } else {
                trigger(t, add && g_party_direction + 1 == (int16_t)data, x, y);   /* data is the direction + 1 */
            }
            break;
        case SENSOR_FLOOR_OBJECT:
            if (object_type == (int16_t)data && !same_type_present)
                trigger(t, add, x, y);
            break;
        case SENSOR_FLOOR_PARTY_ON_STAIRS:
            if (party && SQUARE_TYPE(square) == SQUARE_STAIRS)
                trigger(t, add, x, y);
            break;
        case SENSOR_FLOOR_GROUP:
            if (!party && thing_type <= THING_GROUP && !group_present)
                trigger(t, add, x, y);
            break;
        case SENSOR_FLOOR_PARTY_POSSESSION:
            if (party)
                trigger(t, sensor_is_object_in_party_possession(data), x, y);
            break;
        case SENSOR_FLOOR_VERSION_CHECKER:
            if (party && add && !party_on_square)
                trigger(t, data < VERSION_CHECKER_LIMIT, x, y);
            break;
        }
    }
    sensor_process_rotation_effect();
}
