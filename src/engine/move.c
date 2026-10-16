/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/move.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/projectile.h"
#include "engine/record.h"
#include "engine/sensor.h"
#include "engine/sound.h"
#include "engine/timeline.h"

#define SCENT_NEW_STRENGTH  24
#define MOVE_STAMINA_FACTOR 3 /* stamina cost of a step: load * 3 / maximum load + 1 */

int16_t g_new_party_map_index = -1;
int g_party_teleport_count;

bool move_is_levitating(uint16_t thing)
{
    int16_t type = THING_TYPE(thing);
    if (type == THING_GROUP)
        return map_get_creature_attributes(thing) & CREATURE_LEVITATES;
    return type == THING_PROJECTILE || type == THING_EXPLOSION;
}

uint16_t move_get_teleporter_rotated_projectile_thing(uint16_t teleporter, uint16_t thing, int16_t *direction)
{
    uint16_t rotation = rec_teleporter_rotation(teleporter);
    if (rec_teleporter_is_absolute(teleporter)) {
        *direction = (int16_t)rotation;
        return thing;
    }
    *direction = (int16_t)NORMALIZE(rotation + *direction);
    return THING_WITH_CELL(thing, rotation + THING_CELL(thing));
}

void move_set_party_direction(int16_t direction)
{
    int16_t delta = direction - g_party_direction;
    g_party_direction = direction;
    if (!delta)
        return;
    int16_t n = g_party_champion_count ? g_party_champion_count : 1;
    for (int16_t i = 0; i < n; i++) {
        g_champions[i].cell = NORMALIZE(g_champions[i].cell + delta);
        g_champions[i].direction = NORMALIZE(g_champions[i].direction + delta);
    }
}

/* The party leaves a scent on its new square. The old square gets strength for the time the party stood there. */
static void add_party_scent(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, int16_t map)
{
    uint16_t n = g_party.scent_count;
    for (; n >= PARTY_SCENT_COUNT; n--)
        party_delete_scent(0);
    if (n)
        party_add_scent_strength(src_x, src_y, g_game_time - g_last_party_movement_time); /* 16-bit difference */
    g_last_party_movement_time = g_game_time;
    g_party.scent_count++;
    if (g_party.footprints_count)
        g_party.last_scent_index = g_party.scent_count;
    g_party.scents[n] = party_pack_position(map, dst_x, dst_y);
    g_party.scent_strengths[n] = 0;
    party_add_scent_strength(dst_x, dst_y,
                             (int16_t)(SCENT_KEEP_LARGER | SCENT_NEW_STRENGTH)); /* the flag is the sign bit */
}

/* A teleporter on the destination square moves the thing to its target and turns it. The party hears it and keeps
   the count of teleports. Returns false when the chain stops: a closed teleporter, another scope, or a teleporter
   that points to itself. */
static bool is_teleported(uint16_t *thing, uint8_t square, bool keeps_cell, MoveDestination *d)
{
    bool party = *thing == THING_NONE;
    int16_t type = party ? -1 : THING_TYPE(*thing);
    uint16_t scope = party ? TELEPORTER_SCOPE_PARTY : TELEPORTER_SCOPE_ALL;
    if (!(square & TELEPORTER_OPEN))
        return false;
    uint16_t teleporter = map_get_square_first_thing(d->x, d->y);
    uint16_t teleporter_scope_bits = rec_teleporter_scope(teleporter);
    if (teleporter_scope_bits == TELEPORTER_SCOPE_CREATURES ||
        (scope != TELEPORTER_SCOPE_ALL && !(scope & teleporter_scope_bits)))
        return false;
    int16_t x = rec_teleporter_target_x(teleporter);
    int16_t y = rec_teleporter_target_y(teleporter);
    int16_t rotation = rec_teleporter_rotation(teleporter);
    bool points_to_itself = x == d->x && y == d->y && rec_teleporter_target_map(teleporter) == d->map;
    d->x = x;
    d->y = y;
    d->map = rec_teleporter_target_map(teleporter);
    map_set_current_map(d->map);
    if (party) {
        g_party_map_x = d->x;
        g_party_map_y = d->y;
        g_party_teleport_count += !points_to_itself;
        if (rec_teleporter_is_audible(teleporter))
            sound_request_play(SOUND_TELEPORT, d->x, d->y, SOUND_MODE_NOW);
        move_set_party_direction(rec_teleporter_is_absolute(teleporter) ? rotation
                                                                        : NORMALIZE(g_party_direction + rotation));
    } else if (type == THING_PROJECTILE) {
        *thing = move_get_teleporter_rotated_projectile_thing(teleporter, *thing, &d->direction);
    } else if (!rec_teleporter_is_absolute(teleporter) && !keeps_cell) {
        *thing = THING_WITH_CELL(*thing, rotation + THING_CELL(*thing));
    }
    return !points_to_itself;
}

/* The destination goes one level down at the same dungeon place, and its map becomes the current map. */
static void fall_one_level(MoveDestination *d)
{
    Position below = map_get_level_change_location((Position){d->map, d->x, d->y}, LEVEL_DOWN);
    d->map = below.map;
    d->x = below.x;
    d->y = below.y;
    map_set_current_map(d->map);
}

/* The destination follows teleporters, pits, and stairs. The party falls into a pit with its damage. An object on
   stairs rolls out of them. Returns the thing with its new cell. */
static uint16_t follow_square_chain(const Move *move, MoveDestination *d, int16_t *start_direction)
{
    uint16_t thing = move->thing;
    bool uses_rope = move->uses_rope;
    bool party = thing == THING_NONE;
    int16_t type = party ? -1 : THING_TYPE(thing);
    bool levitating = party ? false : move_is_levitating(thing);
    for (int16_t n = SQUARE_CHAIN_MAXIMUM; n; n--) {
        uint8_t square = rec_square_byte(g_current_map_index, d->x, d->y);
        int16_t square_type = rec_square_type(square);
        if (square_type == SQUARE_TELEPORTER) {
            if (!is_teleported(&thing, square, move->keeps_cell, d))
                break;
        } else if (square_type == SQUARE_PIT && !levitating && (square & PIT_OPEN) && !(square & PIT_IMAGINARY)) {
            fall_one_level(d);
            if (party) {
                g_party_map_x = d->x;
                g_party_map_y = d->y;
                champion_fall_damage(uses_rope);
                uses_rope = false;
            }
        } else if (square_type == SQUARE_STAIRS && !party && type != THING_PROJECTILE) {
            /* An object on down stairs falls one level. On up stairs it only rolls out. */
            if (!(square & STAIRS_UP))
                fall_one_level(d);
            int16_t exit = map_get_stairs_exit_direction(d->x, d->y);
            map_step(exit, 1, 0, &d->x, &d->y);
            *start_direction = direction_back(exit);
            /* The object takes the cell on the side it came from, next to the stairs. */
            uint16_t cell = direction_front_cell(*start_direction, THING_CELL(thing));
            thing = THING_WITH_CELL(thing, cell);
        } else {
            break;
        }
    }
    return thing;
}

/* The thing arrives on its destination square. The party kills a group there and enters the square sensors, or waits
   for the map change on another map. A projectile only joins the square list. */
static void arrive_on_square(uint16_t thing, int16_t dst_x, int16_t dst_y, int16_t map, int16_t src_map,
                             bool same_square)
{
    map_set_current_map(map);
    if (thing == THING_NONE) {
        uint16_t group = map_find_thing((Location){g_party_map_x, g_party_map_y}, THING_GROUP);
        if (group != THING_ENDOFLIST) {
            group_drop_group_possessions(g_party_map_x, g_party_map_y, group, SOUND_MODE_AFTER_TICK);
            group_delete(g_party_map_x, g_party_map_y);
        }
        if (map == src_map) {
            sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, same_square, true);
        } else {
            map_set_current_map(src_map);
            g_new_party_map_index = map;
        }
        return;
    }
    if (THING_TYPE(thing) == THING_PROJECTILE)
        map_link_thing_to_list(thing, 0, dst_x, dst_y);
    else
        sensor_process_thing_addition_or_removal(dst_x, dst_y, thing, map_is_party_on((Location){dst_x, dst_y}), true);
    map_set_current_map(src_map);
}

MoveResult move_get_move_result(const Move *move, MoveDestination *destination)
{
    uint16_t thing = move->thing;
    bool party = thing == THING_NONE;
    int16_t type = party ? -1 : THING_TYPE(thing);
    uint16_t src_cell = party ? 0 : THING_CELL(thing);
    bool levitating = party ? false : move_is_levitating(thing);
    /* A thing that enters comes from (-1, 0); a thing that leaves goes to (-1, 0). */
    int16_t src_x = move->enters ? -1 : move->from.x;
    int16_t src_y = move->enters ? 0 : move->from.y;
    int16_t dst_x = move->leaves ? -1 : move->to.x;
    int16_t dst_y = move->leaves ? 0 : move->to.y;
    int16_t src_map = g_current_map_index;
    int16_t start_direction = 0;
    bool same_square = false;
    if (type == THING_GROUP)
        return group_get_move_result(move, destination);
    if (party && !move->enters && projectile_is_killed_by_impact(src_x, src_y, dst_x, dst_y, THING_NONE))
        return MOVE_RESULT_STOPPED;
    MoveDestination d = {.map = src_map, .x = dst_x, .y = dst_y};
    if (!move->leaves) {
        if (party) {
            g_party_map_x = dst_x;
            g_party_map_y = dst_y;
            start_direction = g_party_direction;
        }
        if (type == THING_PROJECTILE)
            d.direction = projectile_event_direction(&g_timeline.events[rec_projectile_event_index(thing)]);
        thing = follow_square_chain(move, &d, &start_direction);
        d.cell = THING_CELL(thing);
        same_square = d.map == src_map && d.x == src_x && d.y == src_y;
        if (destination)
            *destination = d;
        if (!same_square) {
            if (party && g_party_champion_count)
                add_party_scent(src_x, src_y, d.x, d.y, d.map);
            map_set_current_map(src_map);
        } else if (party) {
            if (g_party_direction == start_direction)
                return MOVE_RESULT_DONE;
        } else if (d.cell == src_cell && type != THING_PROJECTILE) {
            return MOVE_RESULT_DONE;
        }
    }
    if (!move->enters) {
        if (party)
            sensor_process_thing_addition_or_removal(src_x, src_y, THING_NONE, same_square, false);
        else if (!levitating)
            sensor_process_thing_addition_or_removal(src_x, src_y, thing, map_is_party_on((Location){src_x, src_y}),
                                                     false);
        else
            map_unlink_thing_from_list(thing, 0, src_x, src_y);
    }
    if (!move->leaves)
        arrive_on_square(thing, d.x, d.y, d.map, src_map, same_square);
    return MOVE_RESULT_DONE;
}

void move_process_new_party_map(void)
{
    if (g_new_party_map_index == -1)
        return;
    group_remove_all_active_groups();
    map_set_current_map_and_party_map(g_new_party_map_index);
    group_add_all_active_groups();
    move_get_move_result(&(Move){.thing = THING_NONE, .to.x = g_party_map_x, .to.y = g_party_map_y, .enters = true},
                         NULL);
    g_new_party_map_index = -1;
}

void move_party_enter_dungeon(void)
{
    map_reset();
    g_new_party_map_index = -1;
    map_set_current_map_and_party_map(g_party_map_index);
    group_add_all_active_groups();
    move_get_move_result(&(Move){.thing = THING_NONE, .to.x = g_party_map_x, .to.y = g_party_map_y, .enters = true},
                         NULL);
}

void move_take_stairs(bool stairs_go_up)
{
    move_get_move_result(&(Move){.thing = THING_NONE, .from.x = g_party_map_x, .from.y = g_party_map_y, .leaves = true},
                         NULL);
    Position to = map_get_level_change_location((Position){g_party_map_index, g_party_map_x, g_party_map_y},
                                                stairs_go_up ? LEVEL_UP : LEVEL_DOWN);
    g_new_party_map_index = to.map;
    g_party_map_x = to.x;
    g_party_map_y = to.y;
    map_set_current_map(g_new_party_map_index);
    move_set_party_direction(map_get_stairs_exit_direction(g_party_map_x, g_party_map_y));
    map_set_current_map(g_party_map_index);
}

void move_turn_party(int16_t command)
{
    uint8_t square = map_get_square(g_party_map_x, g_party_map_y);
    if (rec_square_type(square) == SQUARE_STAIRS) {
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, true, false);
    move_set_party_direction(NORMALIZE(g_party_direction + (command == COMMAND_TURN_RIGHT ? 1 : -1)));
    sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, true, true);
}

/* A wall, a door that is not open enough, or a solid fake wall stops the party. */
static bool is_square_blocking(uint8_t square)
{
    int16_t state = rec_door_state(square);
    switch (rec_square_type(square)) {
    case SQUARE_WALL:
        return true;
    case SQUARE_DOOR:
        return state != DOOR_OPEN && state != DOOR_ALMOST_OPEN && state != DOOR_DESTROYED;
    case SQUARE_FAKE_WALL:
        return !(square & FAKE_WALL_OPEN) && !(square & FAKE_WALL_IMAGINARY);
    default:
        return false;
    }
}

/* The two champions on the side of the bump take 1 damage (torso and legs wounds). */
static void bump_into_wall(int16_t x, int16_t y, int16_t arrow)
{
    int16_t cell = NORMALIZE(g_party_direction + arrow + 2);
    int16_t first = group_get_target_champion_index(x, y, cell);
    int16_t damaged = champion_add_pending_damage_and_wounds(first, 1, WOUND_TORSO | WOUND_LEGS, ATTACK_SELF);
    int16_t second = group_get_target_champion_index(x, y, direction_right(cell));
    if (second != first)
        damaged |= champion_add_pending_damage_and_wounds(second, 1, WOUND_TORSO | WOUND_LEGS, ATTACK_SELF);
    if (damaged)
        sound_request_play(SOUND_PARTY_DAMAGED, x, y, SOUND_MODE_NOW);
}

void move_party(int16_t command)
{
    for (int16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            champion_decrement_stamina(i, (uint16_t)(g_champions[i].load * MOVE_STAMINA_FACTOR) /
                                                  champion_get_maximum_load(&g_champions[i]) +
                                              1); /* 16-bit wrap */
    int16_t arrow = command - COMMAND_MOVE_FORWARD;
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    uint8_t square = map_get_square(x, y);
    bool on_stairs = rec_square_type(square) == SQUARE_STAIRS;
    if (on_stairs && command == COMMAND_MOVE_BACKWARD) {
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    map_step(g_party_direction, tables_get_arrow_steps_forward(arrow), tables_get_arrow_steps_right(arrow), &x, &y);
    square = map_get_square(x, y);
    if (rec_square_type(square) == SQUARE_STAIRS) {
        move_get_move_result(
            &(Move){.thing = THING_NONE, .from.x = g_party_map_x, .from.y = g_party_map_y, .leaves = true}, NULL);
        g_party_map_x = x;
        g_party_map_y = y;
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    bool blocked = is_square_blocking(square);
    if (g_party_champion_count) {
        if (blocked) {
            bump_into_wall(x, y, arrow);
        } else {
            blocked = map_find_thing((Location){x, y}, THING_GROUP) != THING_ENDOFLIST;
            if (blocked)
                group_process_reaction_event((Location){x, y}, REACTION_PARTY_IS_ADJACENT);
        }
    }
    if (blocked)
        return;
    if (on_stairs)
        move_get_move_result(&(Move){.thing = THING_NONE, .to.x = x, .to.y = y, .enters = true}, NULL);
    else
        move_get_move_result(
            &(Move){.thing = THING_NONE, .from.x = g_party_map_x, .from.y = g_party_map_y, .to.x = x, .to.y = y}, NULL);
    int16_t ticks = 1;
    for (int16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            ticks = max16(ticks, champion_get_movement_ticks(&g_champions[i]));
    g_projectile_disabled_movement_ticks = 0;
    g_disabled_movement_ticks = ticks;
}

void move_reset(void)
{
    g_new_party_map_index = -1;
}
