/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/timeline_process.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/action.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/move.h"
#include "engine/projectile.h"
#include "engine/savegame.h"
#include "engine/sensor.h"
#include "engine/sound.h"

#define DOOR_CLOSE_DAMAGE       5
#define COUNTER_MAXIMUM         511 /* the counter data has 9 bits */
#define GENERATOR_DELAY_LONG    127 /* a delay above this value counts in units of 64 ticks above 126 */
#define GENERATOR_DELAY_UNIT    64
#define LIGHT_STEP_TICKS        4
#define REBIRTH_EXPLOSION_TICKS 5

bool g_game_won;
void (*g_endgame_sensor_hook)(uint16_t delay);

/* AND/OR gate data: bits 0-3 are the inputs (one per cell), bits 4-7 are the expected inputs. */
#define GATE_INPUTS(data)   ((data) & 15)
#define GATE_EXPECTED(data) ((data) >> 4 & 15)

static bool is_party_on(int16_t x, int16_t y)
{
    return g_current_map_index == g_party_map_index && x == g_party_map_x && y == g_party_map_y;
}

/* A group that a door or a fake wall can hit: it is not non-material. */
static bool is_material_group_on(int16_t x, int16_t y, uint16_t *attributes)
{
    uint16_t group = map_get_square_group(x, y);
    if (group == THING_ENDOFLIST)
        return false;
    *attributes = map_get_creature_attributes(group);
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
    dungeon_decode_text(buffer, sizeof buffer, text, 1);
    message_print(SENSOR_TEXT_COLOR, buffer, g_game_time);
}

void event_process_door_animation(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    int16_t effect = event_effect(event);
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
            int16_t height = door_is_vertical(map_get_square_first_thing(x, y)) ? rec_creature_height(attributes) : 1;
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

void event_process_door_destruction(TimelineEvent *event)
{
    uint8_t *square = &g_current_map_data[event_square_x(event)][event_square_y(event)];
    *square = square_with_door_state(*square, DOOR_DESTROYED);
}

void event_process_door(TimelineEvent *event)
{
    int16_t state = door_state(g_current_map_data[event_square_x(event)][event_square_y(event)]);
    if (state == DOOR_DESTROYED)
        return;
    if (event_effect(event) == EFFECT_TOGGLE)
        event_set_effect(event, state == DOOR_OPEN ? EFFECT_CLEAR : EFFECT_SET);
    else if (event_effect(event) == EFFECT_SET ? state == DOOR_OPEN : state == DOOR_CLOSED)
        return;
    event->type = EVENT_DOOR_ANIMATION;
    timeline_add_event(event);
}

void event_process_fake_wall(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    int16_t effect = event_effect(event);
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
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    uint8_t *square = &g_current_map_data[x][y];
    if (event_effect(event) == EFFECT_TOGGLE)
        event_set_effect(event, *square & TELEPORTER_OPEN ? EFFECT_CLEAR : EFFECT_SET);
    if (event_effect(event) == EFFECT_SET) {
        *square |= TELEPORTER_OPEN;
        event_move_teleporter_or_pit_square_things(x, y);
    } else {
        *square &= ~TELEPORTER_OPEN;
    }
}

void event_process_teleporter(TimelineEvent *event)
{
    open_or_close(event);
}
void event_process_pit(TimelineEvent *event)
{
    open_or_close(event);
}

/* Shows, hides, or toggles a text. Returns true when the text becomes visible. */
static bool change_text_visibility(uint16_t text, int16_t effect)
{
    bool was_visible = rec_text_is_visible(text);
    rec_text_set_visible(text, effect == EFFECT_TOGGLE ? !was_visible : effect == EFFECT_SET);
    return !was_visible && rec_text_is_visible(text);
}

/* A group generator plays its sound, disables itself, and comes back after its regeneration delay.
   It first creates the group. */
static void trigger_group_generator(uint16_t sensor, const TimelineEvent *event, int16_t x, int16_t y)
{
    group_generate_from_sensor(sensor, x, y);
    if (rec_sensor_is_audible(sensor))
        sound_request_play(SOUND_TELEPORT, x, y, SOUND_MODE_AFTER_TICK);
    uint16_t delay = rec_sensor_generator_delay(sensor);
    if (!rec_sensor_is_once_only(sensor) && !delay)
        return;
    rec_sensor_disable(sensor);
    if (rec_sensor_is_once_only(sensor))
        return;
    if (delay > GENERATOR_DELAY_LONG)
        delay = (delay - (GENERATOR_DELAY_LONG - 1)) * GENERATOR_DELAY_UNIT;
    TimelineEvent enable = {.map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + delay),
                            .type = EVENT_ENABLE_GROUP_GENERATOR,
                            .b = event->b};
    timeline_add_event(&enable);
}

void event_process_corridor(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    for (uint16_t t = map_get_square_first_thing(x, y); t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
        if (THING_TYPE(t) == THING_TEXT) {
            if (change_text_visibility(t, event_effect(event)) && is_party_on(x, y))
                print_text(t);
        } else if (THING_TYPE(t) == THING_SENSOR && rec_sensor_type(t) == SENSOR_FLOOR_GROUP_GENERATOR) {
            trigger_group_generator(t, event, x, y);
        }
    }
}

void event_enable_group_generator(TimelineEvent *event)
{
    for (uint16_t t = map_get_square_first_thing(event_square_x(event), event_square_y(event));
         t != THING_NONE && t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
        if (THING_TYPE(t) == THING_SENSOR && rec_sensor_type(t) == SENSOR_DISABLED) {
            rec_sensor_set_type(t, SENSOR_FLOOR_GROUP_GENERATOR);
            return;
        }
    }
}

/* A hold sensor sets or clears with the condition (inverted by the revert bit). Other sensors trigger when it is true. */
static void trigger_on_condition(uint16_t sensor, int16_t condition, int16_t x, int16_t y, int16_t cell)
{
    bool clear = rec_sensor_revert(sensor) == condition;
    if (rec_sensor_effect(sensor) == EFFECT_HOLD)
        sensor_trigger_effect(sensor, clear ? EFFECT_CLEAR : EFFECT_SET, x, y, cell);
    else if (!clear)
        sensor_trigger_effect(sensor, rec_sensor_effect(sensor), x, y, cell);
}

/* A counter counts up (set) or down, and triggers at 0. */
static void count(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    uint16_t data = rec_sensor_data(sensor);
    if (!data)
        return;
    if (effect == EFFECT_SET) {
        if (data < COUNTER_MAXIMUM)
            data++;
    } else {
        data--;
    }
    rec_sensor_set_data(sensor, data);
    if (rec_sensor_effect(sensor) == EFFECT_HOLD)
        sensor_trigger_effect(sensor, rec_sensor_revert(sensor) == (data == 0) ? EFFECT_CLEAR : EFFECT_SET, x, y, cell);
    else if (!data)
        sensor_trigger_effect(sensor, rec_sensor_effect(sensor), x, y, cell); /* the revert bit does not apply here */
}

/* An AND/OR gate sets, clears, or toggles the input of the cell, and triggers when the inputs match. */
static void set_gate_input(uint16_t sensor, int16_t effect, int16_t x, int16_t y, int16_t cell)
{
    uint16_t data = rec_sensor_data(sensor);
    uint16_t input = 1 << cell;
    if (effect == EFFECT_TOGGLE ? (data & input) != 0 : effect != EFFECT_SET)
        data &= ~input;
    else
        data |= input;
    rec_sensor_set_data(sensor, data);
    trigger_on_condition(sensor, GATE_EXPECTED(data) == GATE_INPUTS(data), x, y, cell);
}

void event_process_wall(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    int16_t cell = event_cell(event);
    int16_t effect = event_effect(event);
    for (uint16_t t = map_get_square_first_thing(x, y); t != THING_ENDOFLIST; t = map_get_next_thing(t)) {
        if (THING_TYPE(t) == THING_TEXT && THING_CELL(t) == cell)
            change_text_visibility(t, effect);
        else if (THING_TYPE(t) == THING_SENSOR && rec_sensor_type(t) == SENSOR_WALL_COUNTER)
            count(t, effect, x, y, cell);
        else if (THING_TYPE(t) == THING_SENSOR && rec_sensor_type(t) == SENSOR_WALL_AND_OR_GATE)
            set_gate_input(t, effect, x, y, cell);
        else if (THING_TYPE(t) == THING_SENSOR && projectile_is_launcher_sensor(t) && THING_CELL(t) == cell) {
            projectile_trigger_launcher(t, event);
            if (rec_sensor_is_once_only(t))
                rec_sensor_set_type(t, SENSOR_DISABLED);
        } else if (THING_TYPE(t) == THING_SENSOR && rec_sensor_type(t) == SENSOR_WALL_ENDGAME) {
            g_game_won = true;
            if (g_adventure != ADVENTURE_CSB) /* DM: the saved game no longer counts; CSB keeps it */
                savegame_forget();
            if (g_endgame_sensor_hook)
                g_endgame_sensor_hook(rec_sensor_delay(t));
        }
    }
    sensor_process_rotation_effect();
}

/* A moved projectile keeps its step energy and gets its new square, direction, cell, and map. */
static void update_projectile_event(uint16_t projectile)
{
    TimelineEvent *e = &g_timeline_events[rec_projectile_event_index(projectile)];
    e->c = projectile_event_c(g_move_result_map_x, g_move_result_map_y, g_move_result_direction,
                              projectile_event_step_energy(e));
    e->b = THING_WITH_CELL(projectile, g_move_result_cell);
    e->map_time = EVENT_MAP_TIME(g_move_result_map_index, e->map_time & EVENT_TIME_MASK);
}

/* A moved explosion gets its new square, cell, and map in its explosion events. */
static void update_explosion_events(uint16_t explosion)
{
    for (uint16_t i = 0; i < g_event_high_water; i++) {
        TimelineEvent *e = &g_timeline_events[i];
        if (e->type == EVENT_EXPLOSION && e->c == explosion) {
            e->b = event_square(g_move_result_map_x, g_move_result_map_y);
            e->c = THING_WITH_CELL(explosion, g_move_result_cell);
            e->map_time = EVENT_MAP_TIME(g_move_result_map_index, e->map_time & EVENT_TIME_MASK);
        }
    }
}

void event_move_teleporter_or_pit_square_things(int16_t x, int16_t y)
{
    if (is_party_on(x, y))
        move_get_move_result(THING_NONE, x, y, x, y);
    uint16_t group = map_get_square_group(x, y);
    if (group != THING_ENDOFLIST)
        move_get_move_result(group, x, y, x, y);
    uint16_t thing = map_get_square_first_object(x, y);
    int16_t objects = 0;
    for (uint16_t t = thing; t != THING_ENDOFLIST; t = map_get_next_thing(t))
        objects += THING_TYPE(t) > THING_GROUP;
    while (thing != THING_ENDOFLIST && objects) {
        objects--;
        uint16_t next = map_get_next_thing(thing);
        uint16_t type = THING_TYPE(thing);
        if (type > THING_GROUP)
            move_get_move_result(thing, x, y, x, y);
        if (type == THING_PROJECTILE)
            update_projectile_event(thing);
        else if (type == THING_EXPLOSION)
            update_explosion_events(thing);
        thing = next;
    }
}

void event_process_light(TimelineEvent *event)
{
    int16_t power = event->b;
    if (!power)
        return;
    bool negative = power < 0;
    if (negative)
        power = -power;
    int16_t rest = power - 1;
    int16_t delta = tables_get_light_amount(power) - tables_get_light_amount(rest);
    if (negative) {
        delta = -delta;
        rest = -rest;
    }
    g_party.magical_light_amount += delta;
    if (rest) {
        TimelineEvent next = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + LIGHT_STEP_TICKS),
                              .type = EVENT_LIGHT,
                              .b = rest};
        timeline_add_event(&next);
    }
}

bool event_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, uint16_t ticks)
{
    uint16_t door = map_get_square_first_thing(x, y);
    if (magic_attack ? !door_breaks_by_magic(door) : !door_breaks_by_force(door))
        return false;
    uint8_t *square = &g_current_map_data[x][y];
    if (attack < door_info_defense(g_current_map_door_info[door_type(door)]) || door_state(*square) != DOOR_CLOSED)
        return false;
    if (!ticks) {
        *square = square_with_door_state(*square, DOOR_DESTROYED);
    } else {
        TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + ticks),
                               .type = EVENT_DOOR_DESTRUCTION,
                               .b = event_square(x, y)};
        timeline_add_event(&event);
    }
    return true;
}

void event_enable_champion_action(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    c->enable_action_event_index = -1;
    c->attributes &= ~CHAMPION_ATTR_ACTION_DISABLED;
    if (c->action_index != ACTION_NONE)
        c->action_defense -= tables_get_action_info(c->action_index)->defense;
    if (c->current_health) {
        if (c->action_index == ACTION_SHOOT && c->slots[SLOT_HAND_READY] == THING_NONE) {
            static const uint16_t quiver_order[] = {SLOT_QUIVER_LINE1_1, SLOT_QUIVER_LINE2_1, SLOT_QUIVER_LINE1_2,
                                                    SLOT_QUIVER_LINE2_2};
            for (int i = 0; i < ARRAY_COUNT(quiver_order); i++) {
                if (action_is_ammunition_compatible_with_weapon(champion_index, SLOT_HAND_ACTION, quiver_order[i])) {
                    inventory_add_object_in_slot(
                        champion_index, inventory_get_object_removed_from_slot(champion_index, quiver_order[i]),
                        SLOT_HAND_READY);
                    break;
                }
            }
        }
        c->attributes |= CHAMPION_ATTR_ACTION_HAND;
    }
    c->action_index = ACTION_NONE;
}

void event_hide_damage_received(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    c->hide_damage_received_event_index = -1;
    if (!c->current_health)
        return;
    if (champion_index + 1 == g_inventory_champion_ordinal)
        c->attributes |= CHAMPION_ATTR_STATUS_BOX;
    else
        c->attributes |= CHAMPION_ATTR_NAME_TITLE;
}

void event_process_vi_altar_rebirth(TimelineEvent *event)
{
    int16_t x = event_square_x(event);
    int16_t y = event_square_y(event);
    int16_t cell = event_cell(event);
    int16_t step = event_effect(event);
    uint16_t champion_index = event->priority;
    if (step == 0) {
        champion_resurrect(champion_index);
        return;
    }
    if (step == 2) {
        explosion_create(EXPLOSION_THING(EXPLOSION_REBIRTH_STEP_1), 0, x, y, cell);
        event->map_time += REBIRTH_EXPLOSION_TICKS;
    } else {
        uint16_t thing = map_get_square_first_thing(x, y);
        while (thing != THING_ENDOFLIST &&
               !(THING_CELL(thing) == cell && THING_TYPE(thing) == THING_JUNK &&
                 object_get_icon_index(thing) == ICON_BONES && junk_charges(thing) == champion_index))
            thing = map_get_next_thing(thing);
        if (thing == THING_ENDOFLIST)
            return;
        map_unlink_thing_from_list(thing, 0, x, y);
        thing_set_free(thing);
        event->map_time++;
    }
    event_set_effect(event, step - 1);
    timeline_add_event(event);
}

/* A poison step, or the end of a spell effect on the party or a champion. */
static void process_effect_event(const TimelineEvent *event)
{
    switch (event->type) {
    case EVENT_INVISIBILITY_END:
        g_party.invisibility_count--;
        break;
    case EVENT_CHAMPION_SHIELD_END:
        g_champions[event->priority].shield_defense -= event->b;
        break;
    case EVENT_THIEVES_EYE_END:
        g_party.thieves_eye_count--;
        break;
    case EVENT_PARTY_SHIELD_END:
        g_party.shield_defense -= event->b;
        break;
    case EVENT_POISON:
        g_champions[event->priority].poison_event_count--;
        champion_poison(event->priority, event->b);
        break;
    case EVENT_SPELL_SHIELD_END:
        g_party.spell_shield_defense -= event->b;
        break;
    case EVENT_FIRE_SHIELD_END:
        g_party.fire_shield_defense -= event->b;
        break;
    case EVENT_FOOTPRINTS_END:
        g_party.footprints_count--;
        break;
    case EVENT_MAGIC_MAP_DAIN_END:
    case EVENT_MAGIC_MAP_ROS_END:
    case EVENT_MAGIC_MAP_KU_END:
        g_champions[event->priority].magic_map[event->type - EVENT_MAGIC_MAP_DAIN_END]--;
        break;
    case EVENT_MAGIC_MAP_NETA_END:
        g_champions[event->priority].magic_map[MAGIC_MAP_NETA] = 0;
        break;
    }
}

void event_process_expired(void)
{
    while (timeline_is_first_event_expired()) {
        TimelineEvent event;
        timeline_extract_first_event(&event);
        map_set_current_map(EVENT_MAP(&event));
        switch (event.type) {
        case EVENT_DOOR_ANIMATION:
            event_process_door_animation(&event);
            break;
        case EVENT_DOOR_DESTRUCTION:
            event_process_door_destruction(&event);
            break;
        case EVENT_CORRIDOR:
            event_process_corridor(&event);
            break;
        case EVENT_WALL:
            event_process_wall(&event);
            break;
        case EVENT_FAKE_WALL:
            event_process_fake_wall(&event);
            break;
        case EVENT_TELEPORTER:
            event_process_teleporter(&event);
            break;
        case EVENT_PIT:
            event_process_pit(&event);
            break;
        case EVENT_DOOR:
            event_process_door(&event);
            break;
        case EVENT_ENABLE_CHAMPION_ACTION:
            event_enable_champion_action(event.priority);
            if (event_quiver_slot(&event) != -1) /* after a throw: the slot that gets the next weapon of the quiver */
                action_move_weapon_from_quiver_to_slot(event.priority, event_quiver_slot(&event));
            break;
        case EVENT_HIDE_DAMAGE_RECEIVED:
            event_hide_damage_received(event.priority);
            break;
        case EVENT_VI_ALTAR_REBIRTH:
            event_process_vi_altar_rebirth(&event);
            break;
        case EVENT_PLAY_SOUND:
            sound_request_play(event.c, event_square_x(&event), event_square_y(&event), SOUND_MODE_AFTER_TICK);
            break;
        case EVENT_REMOVE_FLUXCAGE:
            if (!g_game_won) {
                map_unlink_thing_from_list(event.c, 0, event_square_x(&event), event_square_y(&event));
                thing_set_free(event.c);
            }
            break;
        case EVENT_EXPLOSION:
            explosion_process_step_event(&event);
            break;
        case EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS:
        case EVENT_MOVE_PROJECTILE:
            projectile_process_move_event(&event);
            break;
        case EVENT_WATCHDOG:
            timeline_add_watchdog_event();
            break;
        case EVENT_MOVE_GROUP_SILENT:
        case EVENT_MOVE_GROUP_AUDIBLE:
            group_process_move_event(&event);
            break;
        case EVENT_ENABLE_GROUP_GENERATOR:
            event_enable_group_generator(&event);
            break;
        case EVENT_LIGHT:
            map_set_current_map(g_party_map_index);
            event_process_light(&event);
            break;
        default:
            if (event.type >= EVENT_GROUP_REACTION_DANGER_ON_SQUARE && event.type <= EVENT_UPDATE_BEHAVIOR_CREATURE_3)
                group_process_reaction_event(event_square_x(&event), event_square_y(&event), event.type,
                                             event_group_ticks(&event));
            else
                process_effect_event(&event);
            break;
        }
        map_set_current_map(g_party_map_index);
    }
}
