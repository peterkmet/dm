#include "timeline_process.h"
#include "startend.h"
#include "action.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "group.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "projectile.h"
#include "sensor.h"
#include "sound.h"
#include "tables.h"
#include "ui.h"

/* KAOS table: the light amount of each light power. */
#define KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT 0x100cb4

#define DOOR_CLOSE_DAMAGE 5
#define COUNTER_MAXIMUM 511            /* the counter data has 9 bits */
#define GENERATOR_DELAY_LONG 127       /* a delay above this value counts in units of 64 ticks above 126 */
#define GENERATOR_DELAY_UNIT 64
#define LIGHT_STEP_TICKS 4
#define TEXT_SIZE 256
#define PROJECTILE_EVENT_STEP_ENERGY 0x000f
#define REBIRTH_EXPLOSION_TICKS 5

int16_t g_game_won;

/* AND/OR gate data: bits 0-3 are the inputs (one per cell), bits 4-7 are the expected inputs. */
#define GATE_INPUTS(data) ((data) & 15)
#define GATE_EXPECTED(data) ((data) >> 4 & 15)

static int16_t is_party_on(int16_t x, int16_t y)
{
    return g_current_map_index == g_party_map_index && x == g_party_map_x && y == g_party_map_y;
}

/* A group that a door or a fake wall can hit: it is not non-material. */
static int16_t is_material_group_on(int16_t x, int16_t y, uint16_t *attributes)
{
    uint16_t group = group_get_thing(x, y);
    if (group == THING_ENDOFLIST)
        return 0;
    *attributes = get_creature_attributes(group);
    return !(*attributes & CREATURE_NON_MATERIAL);
}

static void add_again_next_tick(TimelineEvent *event)
{
    event->map_time++;
    timeline_add_event(event);
}

static void print_text(uint16_t text)
{
    char buffer[TEXT_SIZE];
    decode_text(buffer, sizeof buffer, text, 1);
    message_print(SENSOR_TEXT_COLOR, buffer, g_game_time);
}

/* One door step per tick: set opens (state down to open), clear closes (state up to closed). A closing door
   that hits the party or a group goes back one step; the group takes damage. */
void timeline_process_event1_door_animation(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event), effect = EVENT_EFFECT(event);
    uint8_t *square = &g_current_map_data[x][y];
    int16_t state = door_state(*square);
    if (state == DOOR_DESTROYED)
        return;
    event->map_time++;
    if (effect == EFFECT_CLEAR) {
        uint16_t attributes;
        if (is_party_on(x, y) && state != DOOR_OPEN) {
            if (g_party_champion_count) {
                *square = square_with_door_state(*square, DOOR_OPEN);
                if (champion_damage_all(DOOR_CLOSE_DAMAGE, WOUND_HEAD, ATTACK_SELF))
                    sound_request_play(SOUND_PARTY_DAMAGED, x, y, SOUND_MODE_AFTER_TICK);
            }
            add_again_next_tick(event);
            return;
        }
        if (is_material_group_on(x, y, &attributes)) {
            int16_t height = door_checks_creature_height(get_square_first_thing(x, y)) ? creature_height(attributes) : 1;
            if (height <= state) {
                group_door_damages_creatures(x, y);
                *square = square_with_door_state(*square, state != DOOR_OPEN ? state - 1 : DOOR_OPEN);
                sound_request_play(SOUND_DOOR_HITS_CREATURE, x, y, SOUND_MODE_AFTER_TICK);
                add_again_next_tick(event);
                return;
            }
        }
    }
    if ((effect != EFFECT_SET || state != DOOR_OPEN) && (effect != EFFECT_CLEAR || state != DOOR_CLOSED)) {
        state += effect == EFFECT_SET ? -1 : 1;
        *square = square_with_door_state(*square, state);
        sound_request_play(SOUND_DOOR, x, y, SOUND_MODE_AFTER_TICK);
        if (effect == EFFECT_SET ? state == DOOR_OPEN : state == DOOR_CLOSED)
            return;
        timeline_add_event(event);
    }
}

void timeline_process_event2_door_destruction(TimelineEvent *event)
{
    uint8_t *square = &g_current_map_data[EVENT_X(event)][EVENT_Y(event)];
    *square = square_with_door_state(*square, DOOR_DESTROYED);
}

/* A door event becomes a door animation event at the same time. Toggle closes an open door and opens any other. */
void timeline_process_event10_door(TimelineEvent *event)
{
    int16_t state = door_state(g_current_map_data[EVENT_X(event)][EVENT_Y(event)]);
    if (state == DOOR_DESTROYED)
        return;
    if (EVENT_EFFECT(event) == EFFECT_TOGGLE)
        EVENT_SET_EFFECT(event, state == DOOR_OPEN ? EFFECT_CLEAR : EFFECT_SET);
    else if (EVENT_EFFECT(event) == EFFECT_SET ? state == DOOR_OPEN : state == DOOR_CLOSED)
        return;
    event->type = EVENT_DOOR_ANIMATION;
    timeline_add_event(event);
}

/* Opens or closes a fake wall. Closing waits while the party or a material group is on the square. */
void timeline_process_event7_fake_wall(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event), effect = EVENT_EFFECT(event);
    uint8_t *square = &g_current_map_data[x][y];
    uint16_t attributes;
    if (effect == EFFECT_TOGGLE)
        effect = *square & FAKE_WALL_OPEN ? EFFECT_CLEAR : EFFECT_SET;
    if (effect != EFFECT_CLEAR) {
        *square |= FAKE_WALL_OPEN;
        return;
    }
    if (is_party_on(x, y) || is_material_group_on(x, y, &attributes)) {
        add_again_next_tick(event);
        return;
    }
    *square &= ~FAKE_WALL_OPEN;
}

/* Opens or closes a teleporter or a pit. Opening moves the things on the square. */
static void open_or_close(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event);
    uint8_t *square = &g_current_map_data[x][y];
    if (EVENT_EFFECT(event) == EFFECT_TOGGLE)
        EVENT_SET_EFFECT(event, *square & TELEPORTER_OPEN ? EFFECT_CLEAR : EFFECT_SET);
    if (EVENT_EFFECT(event) == EFFECT_SET) {
        *square |= TELEPORTER_OPEN;
        timeline_move_teleporter_or_pit_square_things(x, y);
    } else {
        *square &= ~TELEPORTER_OPEN;
    }
}

void timeline_process_event8_teleporter(TimelineEvent *event) { open_or_close(event); }
void timeline_process_event9_pit(TimelineEvent *event) { open_or_close(event); }

/* Shows, hides, or toggles a text. Returns 1 when the text becomes visible. */
static int16_t change_text_visibility(uint16_t text, int16_t effect)
{
    int16_t was_visible = text_is_visible(text);
    text_set_visible(text, effect == EFFECT_TOGGLE ? !was_visible : effect == EFFECT_SET);
    return !was_visible && text_is_visible(text);
}

/* A group generator plays its sound, disables itself, and comes back after its regeneration delay.
   It first creates the group. */
static void trigger_group_generator(uint16_t sensor, const TimelineEvent *event, int16_t x, int16_t y)
{
    group_generate_from_sensor(sensor, x, y);
    if (sensor_is_audible(sensor))
        sound_request_play(SOUND_TELEPORT, x, y, SOUND_MODE_AFTER_TICK);
    uint16_t delay = sensor_generator_delay(sensor);
    if (!sensor_is_once_only(sensor) && !delay)
        return;
    sensor_disable(sensor);
    if (sensor_is_once_only(sensor))
        return;
    if (delay > GENERATOR_DELAY_LONG)
        delay = (delay - (GENERATOR_DELAY_LONG - 1)) * GENERATOR_DELAY_UNIT;
    TimelineEvent enable = {.map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + delay), .type = EVENT_ENABLE_GROUP_GENERATOR, .b = event->b};
    timeline_add_event(&enable);
}

/* Texts on the corridor change visibility, and group generators run. */
void timeline_process_event5_corridor(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event);
    for (uint16_t t = get_square_first_thing(x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_TYPE(t) == THING_TEXT) {
            if (change_text_visibility(t, EVENT_EFFECT(event)) && is_party_on(x, y))
                print_text(t);
        } else if (THING_TYPE(t) == THING_SENSOR && sensor_type(t) == SENSOR_FLOOR_GROUP_GENERATOR) {
            trigger_group_generator(t, event, x, y);
        }
    }
}

/* The first disabled sensor of the square becomes a group generator again. */
void timeline_process_event65_enable_group_generator(TimelineEvent *event)
{
    for (uint16_t t = get_square_first_thing(EVENT_X(event), EVENT_Y(event)); t != THING_NONE && t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_TYPE(t) == THING_SENSOR && sensor_type(t) == SENSOR_DISABLED) {
            sensor_set_type(t, SENSOR_FLOOR_GROUP_GENERATOR);
            return;
        }
    }
}

/* A hold sensor sets or clears with the condition (inverted by the revert bit). Other sensors trigger when it is true. */
static void trigger_on_condition(uint16_t sensor, int16_t condition, int16_t x, int16_t y, int16_t cell)
{
    int16_t clear = sensor_revert(sensor) == condition;
    if (sensor_effect(sensor) == EFFECT_HOLD)
        sensor_trigger_effect(sensor, clear ? EFFECT_CLEAR : EFFECT_SET, x, y, cell);
    else if (!clear)
        sensor_trigger_effect(sensor, sensor_effect(sensor), x, y, cell);
}

/* A counter counts up (set) or down, and triggers at 0. */
static void count(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    uint16_t data = sensor_data(sensor);
    if (!data)
        return;
    if (effect == EFFECT_SET) {
        if (data < COUNTER_MAXIMUM)
            data++;
    } else {
        data--;
    }
    sensor_set_data(sensor, data);
    if (sensor_effect(sensor) == EFFECT_HOLD)
        sensor_trigger_effect(sensor, sensor_revert(sensor) == (data == 0) ? EFFECT_CLEAR : EFFECT_SET, x, y, cell);
    else if (!data)
        sensor_trigger_effect(sensor, sensor_effect(sensor), x, y, cell);   /* the revert bit does not apply here */
}

/* An AND/OR gate sets, clears, or toggles the input of the cell, and triggers when the inputs match. */
static void gate(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    uint16_t data = sensor_data(sensor), input = 1 << cell;
    if (effect == EFFECT_TOGGLE ? (data & input) != 0 : effect != EFFECT_SET)
        data &= ~input;
    else
        data |= input;
    sensor_set_data(sensor, data);
    trigger_on_condition(sensor, GATE_EXPECTED(data) == GATE_INPUTS(data), x, y, cell);
}

/* Texts of the event cell change visibility. Counters, AND/OR gates, and projectile launchers of the cell run.
   An endgame sensor ends the game. */
void timeline_process_event6_wall(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event), cell = EVENT_CELL(event), effect = EVENT_EFFECT(event);
    for (uint16_t t = get_square_first_thing(x, y); t != THING_ENDOFLIST; t = get_next_thing(t)) {
        if (THING_TYPE(t) == THING_TEXT && THING_CELL(t) == (uint16_t)cell)
            change_text_visibility(t, effect);
        else if (THING_TYPE(t) == THING_SENSOR && sensor_type(t) == SENSOR_WALL_COUNTER)
            count(t, effect, x, y, cell);
        else if (THING_TYPE(t) == THING_SENSOR && sensor_type(t) == SENSOR_WALL_AND_OR_GATE)
            gate(t, effect, x, y, cell);
        else if (THING_TYPE(t) == THING_SENSOR && sensor_is_launcher(t) && THING_CELL(t) == (uint16_t)cell) {
            timeline_trigger_projectile_launcher(t, event);
            if (sensor_is_once_only(t))
                sensor_set_type(t, SENSOR_DISABLED);
        } else if (THING_TYPE(t) == THING_SENSOR && sensor_type(t) == SENSOR_WALL_ENDGAME) {
            endgame_sensor(sensor_delay(t));
        }
    }
    sensor_process_rotation_effect();
}

/* A moved projectile keeps its step energy and gets its new square, direction, cell, and map. */
static void update_projectile_event(uint16_t projectile)
{
    TimelineEvent *e = &g_timeline_events[projectile_event_index(projectile)];
    e->c = (uint16_t)((e->c & PROJECTILE_EVENT_STEP_ENERGY) | g_move_result_map_x << 11 | (g_move_result_map_y & 31) << 6 |
                      (g_move_result_direction & 3) << 4);
    e->b = THING_WITH_CELL(projectile, g_move_result_cell);
    e->map_time = EVENT_MAP_TIME(g_move_result_map_index, e->map_time & EVENT_TIME_MASK);
}

/* A moved explosion gets its new square, cell, and map in its explosion events. */
static void update_explosion_events(uint16_t explosion)
{
    for (uint16_t i = 0; i < g_event_high_water; i++) {
        TimelineEvent *e = &g_timeline_events[i];
        if (e->type == EVENT_EXPLOSION && e->c == explosion) {
            e->b = EVENT_SQUARE(g_move_result_map_x, g_move_result_map_y);
            e->c = THING_WITH_CELL(explosion, g_move_result_cell);
            e->map_time = EVENT_MAP_TIME(g_move_result_map_index, e->map_time & EVENT_TIME_MASK);
        }
    }
}

/* The party, the group, and each object on the square move again, for example into an opened pit. */
void timeline_move_teleporter_or_pit_square_things(int16_t x, int16_t y)
{
    if (is_party_on(x, y))
        move_get_move_result(THING_NONE, x, y, x, y);
    uint16_t group = group_get_thing(x, y);
    if (group != THING_ENDOFLIST)
        move_get_move_result(group, x, y, x, y);
    uint16_t thing = get_square_first_object(x, y);
    int16_t objects = 0;
    for (uint16_t t = thing; t != THING_ENDOFLIST; t = get_next_thing(t))
        objects += THING_TYPE(t) > THING_GROUP;
    while (thing != THING_ENDOFLIST && objects) {
        objects--;
        uint16_t next = get_next_thing(thing), type = THING_TYPE(thing);
        if (type > THING_GROUP)
            move_get_move_result(thing, x, y, x, y);
        if (type == THING_PROJECTILE)
            update_projectile_event(thing);
        else if (type == THING_EXPLOSION)
            update_explosion_events(thing);
        thing = next;
    }
}

/* The event holds the signed light power. Each step applies one step of the light table and adds the rest later. */
void timeline_process_event70_light(TimelineEvent *event)
{
    int16_t power = (int16_t)event->b;
    if (!power)
        return;
    int16_t negative = power < 0;
    if (negative)
        power = -power;
    int16_t rest = power - 1;
    int16_t delta = kaos_s16(KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT + power * 2) - kaos_s16(KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT + rest * 2);
    if (negative) {
        delta = -delta;
        rest = -rest;
    }
    g_party.magical_light_amount += delta;
    if (rest) {
        TimelineEvent next = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + LIGHT_STEP_TICKS), .type = EVENT_LIGHT, .b = (uint16_t)rest};
        timeline_add_event(&next);
    }
}

/* A closed door breaks when the attack reaches its defense and the door accepts the attack kind (force or magic).
   ticks 0 breaks it at once, else a door destruction event breaks it later. Returns 1 when the door breaks. */
int16_t group_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, int16_t ticks)
{
    uint16_t door = get_square_first_thing(x, y);
    if (magic_attack ? !door_breaks_by_magic(door) : !door_breaks_by_force(door))
        return 0;
    uint8_t *square = &g_current_map_data[x][y];
    if (attack < door_info_defense(g_current_map_door_info[door_type(door)]) || door_state(*square) != DOOR_CLOSED)
        return 0;
    if (!ticks) {
        *square = square_with_door_state(*square, DOOR_DESTROYED);
    } else {
        TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + (uint16_t)ticks), .type = EVENT_DOOR_DESTRUCTION, .b = EVENT_SQUARE(x, y)};
        timeline_add_event(&event);
    }
    return 1;
}

/* The action hand works again: the action defense ends. After a shot the next ammunition of the quiver goes to the
   ready hand (quiver line 1 first, then slots 7-9). */
void timeline_process_event11_enable_champion_action(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    c->enable_action_event_index = -1;
    c->attributes &= ~CHAMPION_ATTR_ACTION_DISABLED;
    if (c->action_index != ACTION_NONE)
        c->action_defense -= tables_action_info(c->action_index)->defense;
    if (c->current_health) {
        if (c->action_index == ACTION_SHOOT && c->slots[SLOT_HAND_READY] == THING_NONE) {
            static const uint16_t quiver_order[] = {SLOT_QUIVER_LINE1_1, SLOT_QUIVER_LINE2_1, SLOT_QUIVER_LINE1_2, SLOT_QUIVER_LINE2_2};
            for (size_t i = 0; i < sizeof quiver_order / sizeof quiver_order[0]; i++) {
                if (champion_is_ammunition_compatible_with_weapon(champion_index, SLOT_HAND_ACTION, quiver_order[i])) {
                    champion_add_object_in_slot(champion_index, champion_get_object_removed_from_slot(champion_index, quiver_order[i]), SLOT_HAND_READY);
                    break;
                }
            }
        }
        c->attributes |= CHAMPION_ATTR_ACTION_HAND;
        champion_draw_state(champion_index);
    }
    c->action_index = ACTION_NONE;
}

/* The damage number on the champion portrait disappears. */
void timeline_process_event12_hide_damage_received(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    c->hide_damage_received_event_index = -1;
    if (!c->current_health)
        return;
    if (champion_index + 1 == g_inventory_champion_ordinal) {
        draw_status_box_portrait((int16_t)champion_index);
    } else {
        c->attributes |= CHAMPION_ATTR_NAME_TITLE;
        champion_draw_state(champion_index);
    }
}

/* Vi altar rebirth in three steps (event cell byte: the step): 2 the rebirth explosion, 1 the bones of the champion
   disappear, 0 the champion lives again. The bones hold the champion index in their charges. */
void timeline_process_event13_vi_altar_rebirth(TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event), cell = EVENT_CELL(event), step = EVENT_EFFECT(event);
    uint16_t champion_index = event->priority;
    if (step == 0) {
        champion_resurrect(champion_index);
        return;
    }
    if (step == 2) {
        explosion_create(EXPLOSION_THING(EXPLOSION_REBIRTH_STEP_1), 0, (uint16_t)x, (uint16_t)y, cell);
        event->map_time += REBIRTH_EXPLOSION_TICKS;
    } else {
        uint16_t thing = get_square_first_thing(x, y);
        while (thing != THING_ENDOFLIST && !(THING_CELL(thing) == cell && THING_TYPE(thing) == THING_JUNK &&
                                             object_get_icon_index(thing) == ICON_BONES && junk_charges(thing) == champion_index))
            thing = get_next_thing(thing);
        if (thing == THING_ENDOFLIST)
            return;
        unlink_thing_from_list(thing, 0, x, y);
        thing_set_free(thing);
        event->map_time++;
    }
    EVENT_SET_EFFECT(event, step - 1);
    timeline_add_event(event);
}

/* Runs every expired event on its map. Event types without a handler here belong to later steps. */
void timeline_process(void)
{
    while (timeline_is_first_event_expired()) {
        TimelineEvent event;
        timeline_extract_first_event(&event);
        set_current_map(EVENT_MAP(&event));
        switch (event.type) {
        case EVENT_DOOR_ANIMATION: timeline_process_event1_door_animation(&event); break;
        case EVENT_DOOR_DESTRUCTION: timeline_process_event2_door_destruction(&event); break;
        case EVENT_CORRIDOR: timeline_process_event5_corridor(&event); break;
        case EVENT_WALL: timeline_process_event6_wall(&event); break;
        case EVENT_FAKE_WALL: timeline_process_event7_fake_wall(&event); break;
        case EVENT_TELEPORTER: timeline_process_event8_teleporter(&event); break;
        case EVENT_PIT: timeline_process_event9_pit(&event); break;
        case EVENT_DOOR: timeline_process_event10_door(&event); break;
        case EVENT_ENABLE_CHAMPION_ACTION:
            timeline_process_event11_enable_champion_action(event.priority);
            if (event.b >> 8)   /* after a throw: the slot + 1 that gets the next weapon of the quiver */
                timeline_move_weapon_from_quiver_to_slot(event.priority, (uint16_t)((int8_t)(event.b >> 8) - 1));
            break;
        case EVENT_HIDE_DAMAGE_RECEIVED: timeline_process_event12_hide_damage_received(event.priority); break;
        case EVENT_VI_ALTAR_REBIRTH: timeline_process_event13_vi_altar_rebirth(&event); break;
        case EVENT_PLAY_SOUND: sound_request_play((int16_t)event.c, EVENT_X(&event), EVENT_Y(&event), SOUND_MODE_AFTER_TICK); break;
        case EVENT_REMOVE_FLUXCAGE:
            if (!g_game_won) {
                unlink_thing_from_list(event.c, 0, EVENT_X(&event), EVENT_Y(&event));
                thing_set_free(event.c);
            }
            break;
        case EVENT_EXPLOSION: explosion_process_event_25(&event); break;
        case EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS:
        case EVENT_MOVE_PROJECTILE: projectile_process_events_48_to_49(&event); break;
        case EVENT_WATCHDOG: timeline_add_watchdog_event(); break;
        case EVENT_MOVE_GROUP_SILENT:
        case EVENT_MOVE_GROUP_AUDIBLE: timeline_process_event60_61_move_group(&event); break;
        case EVENT_ENABLE_GROUP_GENERATOR: timeline_process_event65_enable_group_generator(&event); break;
        case EVENT_LIGHT:
            set_current_map(g_party_map_index);
            timeline_process_event70_light(&event);
            break;
        case EVENT_INVISIBILITY_END: g_party.event71_count_invisibility--; break;
        case EVENT_CHAMPION_SHIELD_END: g_champions[event.priority].shield_defense -= (int16_t)event.b; break;
        case EVENT_THIEVES_EYE_END: g_party.event73_count_thieves_eye--; break;
        case EVENT_PARTY_SHIELD_END: g_party.shield_defense -= (int16_t)event.b; break;
        case EVENT_POISON:
            g_champions[event.priority].poison_event_count--;
            champion_poison(event.priority, event.b);
            break;
        case EVENT_SPELL_SHIELD_END: g_party.spell_shield_defense -= (int16_t)event.b; break;
        case EVENT_FIRE_SHIELD_END: g_party.fire_shield_defense -= (int16_t)event.b; break;
        case EVENT_FOOTPRINTS_END: g_party.event79_count_footprints--; break;
        default:
            if (event.type >= EVENT_GROUP_REACTION_DANGER_ON_SQUARE && event.type <= EVENT_UPDATE_BEHAVIOR_CREATURE_3)
                group_process_events_29_to_41(EVENT_X(&event), EVENT_Y(&event), event.type, (uint16_t)EVENT_CELL(&event));
            break;
        }
        set_current_map(g_party_map_index);
    }
}
