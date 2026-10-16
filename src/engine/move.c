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

/* Strength of a new scent and the stamina factor of a step. */
#define SCENT_NEW_STRENGTH  24
#define MOVE_STAMINA_FACTOR 3 /* stamina cost of a step: load * 3 / maximum load + 1 */

/* Kinds of thing that move along a square chain: the chain rules differ per kind. */
#define MOVER_PARTY      0
#define MOVER_GROUP      1
#define MOVER_OBJECT     2
#define MOVER_PROJECTILE 3

int g_party_teleport_count;

bool move_is_levitating(uint16_t thing)
{
    int16_t type = THING_TYPE(thing);
    if (type == THING_GROUP)
        return map_get_creature_attributes(thing) & CREATURE_LEVITATES;
    return type == THING_PROJECTILE || type == THING_EXPLOSION;
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

/* One move along the square chain: the mover, its thing, the source, and the destination being built. */
typedef struct {
    int16_t mover;           /* MOVER_* */
    uint16_t thing;          /* THING_NONE for the party; teleporters and stairs change its cell */
    Position source;         /* x < 0: the thing enters the map */
    MoveDestination d;       /* map, square, and projectile direction of the destination */
    int16_t start_direction; /* party direction before the chain, or the direction out of the last stairs */
    bool levitating, uses_rope, keeps_cell;
    bool audible;                /* group: the last teleporter plays a sound */
    bool removed_from_party_map; /* group: the group leaves a square of the party map */
    int16_t turned_active;       /* group: active index of the group that the last teleporter turned, else -1 */
    MoveResult result;           /* group: of the last teleporter */
    GroupFallDrops drops;        /* group: cells of creatures that died in a fall */
} Walk;

/* The kind of mover of a thing. */
static int16_t get_mover(uint16_t thing)
{
    if (thing == THING_NONE)
        return MOVER_PARTY;
    if (THING_TYPE(thing) == THING_GROUP)
        return MOVER_GROUP;
    if (THING_TYPE(thing) == THING_PROJECTILE)
        return MOVER_PROJECTILE;
    return MOVER_OBJECT;
}

/* A teleporter takes the party with the party scope, a group with the creatures scope, and an object or a projectile
   with any scope except creatures only. */
static bool is_in_teleporter_scope(int16_t mover, uint16_t scope_bits)
{
    if (mover == MOVER_GROUP)
        return scope_bits & TELEPORTER_SCOPE_CREATURES;
    if (scope_bits == TELEPORTER_SCOPE_CREATURES)
        return false;
    return mover != MOVER_PARTY || (scope_bits & TELEPORTER_SCOPE_PARTY);
}

/* A teleporter turns a projectile: its flight direction and its cell. */
static uint16_t rotate_projectile(uint16_t teleporter, uint16_t thing, int16_t *out_direction)
{
    uint16_t rotation = rec_teleporter_rotation(teleporter);
    if (rec_teleporter_is_absolute(teleporter)) {
        *out_direction = (int16_t)rotation;
        return thing;
    }
    *out_direction = (int16_t)NORMALIZE(rotation + *out_direction);
    return THING_WITH_CELL(thing, rotation + THING_CELL(thing));
}

/* Turns the party. Each champion keeps its place relative to the party. The loop also runs one time without champions. */
static void turn_party(int16_t direction)
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

/* After the jump of a teleporter, the party hears it, keeps the count of teleports and turns; a group hears it later
   and turns; a projectile turns its direction and cell; an object turns its cell. */
static void turn_by_teleporter(Walk *w, uint16_t teleporter, bool points_to_itself)
{
    MoveDestination *d = &w->d;
    int16_t rotation = rec_teleporter_rotation(teleporter);
    switch (w->mover) {
    case MOVER_PARTY:
        g_party_map_x = d->x;
        g_party_map_y = d->y;
        g_party_teleport_count += !points_to_itself;
        if (rec_teleporter_is_audible(teleporter))
            sound_request_play(SOUND_TELEPORT, (Location){d->x, d->y}, SOUND_MODE_NOW, 0);
        turn_party(rec_teleporter_is_absolute(teleporter) ? rotation : NORMALIZE(g_party_direction + rotation));
        break;
    case MOVER_GROUP:
        w->audible = rec_teleporter_is_audible(teleporter);
        if (w->audible)
            sound_request_play(SOUND_BUZZ, (Location){d->x, d->y}, SOUND_MODE_AFTER_TICK, 0);
        w->turned_active = group_turn_by_teleporter(teleporter, w->thing, w->source.map);
        w->result = w->turned_active >= 0 ? MOVE_RESULT_TURNED : MOVE_RESULT_STOPPED;
        break;
    case MOVER_PROJECTILE:
        w->thing = rotate_projectile(teleporter, w->thing, &d->direction);
        break;
    default:
        if (!rec_teleporter_is_absolute(teleporter) && !w->keeps_cell)
            w->thing = THING_WITH_CELL(w->thing, rotation + THING_CELL(w->thing));
        break;
    }
}

/* A teleporter on the destination square moves the thing to its target and turns it. Returns false when the chain
   stops: a closed teleporter, another scope, or a teleporter that points to itself. */
static bool is_teleported(Walk *w, uint8_t square)
{
    MoveDestination *d = &w->d;
    if (!(square & TELEPORTER_OPEN))
        return false;
    uint16_t teleporter = map_get_square_first_thing(d->x, d->y);
    if (!is_in_teleporter_scope(w->mover, rec_teleporter_scope(teleporter)))
        return false;
    int16_t x = rec_teleporter_target_x(teleporter);
    int16_t y = rec_teleporter_target_y(teleporter);
    bool points_to_itself = x == d->x && y == d->y && rec_teleporter_target_map(teleporter) == d->map;
    d->x = x;
    d->y = y;
    d->map = rec_teleporter_target_map(teleporter);
    map_set_current_map(d->map);
    turn_by_teleporter(w, teleporter, points_to_itself);
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

/* The destination follows teleporters, pits, and stairs, at most SQUARE_CHAIN_MAXIMUM squares. The rules per square
   type and mover:
   - teleporter, party: scope party; SOUND_TELEPORT now when audible; turns the party direction; counts the teleport.
   - teleporter, group: scope creatures; SOUND_BUZZ after the tick when audible; turns the creature directions and
     cells.
   - teleporter, object: any scope except creatures only; turns the cell unless keeps_cell.
   - teleporter, projectile: any scope except creatures only; turns the direction and the cell.
   - teleporter, all: a closed teleporter, another scope, or a teleporter that points to itself ends the chain.
   - pit, all: an open, real pit takes the thing one level down; not a levitating thing.
   - pit, party: fall damage, or the stamina cost of the rope for the first pit.
   - pit, group: FALL_DAMAGE on the source map; creatures that died drop their possessions; all dead ends the chain.
   - stairs, object and group: down stairs go one level down; the thing steps out of the stairs and takes the cell
     on the side it came from, next to the stairs.
   - stairs, party and projectile, and any other square: the chain ends.
   Returns true when a fall killed the whole group. */
static bool follow_square_chain(Walk *w)
{
    MoveDestination *d = &w->d;
    for (int16_t n = SQUARE_CHAIN_MAXIMUM; n; n--) {
        uint8_t square = rec_square_byte(g_current_map_index, d->x, d->y);
        int16_t square_type = rec_square_type(square);
        if (square_type == SQUARE_TELEPORTER) {
            if (!is_teleported(w, square))
                break;
        } else if (square_type == SQUARE_PIT && !w->levitating && (square & PIT_OPEN) && !(square & PIT_IMAGINARY)) {
            fall_one_level(d);
            if (w->mover == MOVER_PARTY) {
                g_party_map_x = d->x;
                g_party_map_y = d->y;
                champion_fall_damage(w->uses_rope);
                w->uses_rope = false;
            } else if (w->mover == MOVER_GROUP &&
                       group_is_killed_by_fall(w->thing, w->source, (Position){d->map, d->x, d->y}, &w->drops)) {
                return true;
            }
        } else if (square_type == SQUARE_STAIRS && (w->mover == MOVER_OBJECT || w->mover == MOVER_GROUP)) {
            /* An object on down stairs falls one level. On up stairs it only rolls out. */
            if (!(square & STAIRS_UP))
                fall_one_level(d);
            int16_t exit = map_get_stairs_exit_direction(d->x, d->y);
            map_step(exit, 1, 0, &d->x, &d->y);
            w->start_direction = direction_back(exit);
            /* The object takes the cell on the side it came from, next to the stairs. */
            uint16_t cell = direction_front_cell(w->start_direction, THING_CELL(w->thing));
            w->thing = THING_WITH_CELL(w->thing, cell);
        } else {
            break;
        }
    }
    return false;
}

/* The thing arrives on its destination square. The party kills a group there and enters the square sensors, or waits
   for the map change on another map. A projectile only joins the square list. */
static void arrive_on_square(uint16_t thing, Position dst, int16_t src_map, bool same_square)
{
    int16_t map = dst.map;
    int16_t dst_x = dst.x;
    int16_t dst_y = dst.y;
    map_set_current_map(map);
    if (thing == THING_NONE) {
        uint16_t group = map_find_thing((Location){g_party_map_x, g_party_map_y}, THING_GROUP);
        if (group != THING_ENDOFLIST) {
            group_drop_group_possessions(g_party_map_x, g_party_map_y, group, SOUND_MODE_AFTER_TICK);
            group_delete(g_party_map_x, g_party_map_y);
        }
        if (map == src_map) {
            sensor_process_thing_change(g_party_map_x, g_party_map_y, THING_NONE, same_square, true);
        } else {
            map_set_current_map(src_map);
            g_session.new_party_map = map;
        }
        return;
    }
    if (THING_TYPE(thing) == THING_PROJECTILE)
        map_link_thing_to_list(thing, 0, dst_x, dst_y);
    else
        sensor_process_thing_change(dst_x, dst_y, thing, map_is_party_on((Location){dst_x, dst_y}), true);
    map_set_current_map(src_map);
}

/* The thing leaves its source square: the party and a thing on the floor leave the square sensors, a levitating
   thing only leaves the square list. */
static void leave_source_square(const Walk *w, bool same_square)
{
    int16_t x = w->source.x;
    int16_t y = w->source.y;
    if (w->mover == MOVER_PARTY)
        sensor_process_thing_change(x, y, THING_NONE, same_square, false);
    else if (!w->levitating)
        sensor_process_thing_change(x, y, w->thing, map_is_party_on((Location){x, y}), false);
    else
        map_unlink_thing_from_list(w->thing, 0, x, y);
}

/* The group arrives after the chain: the walk becomes the arrival of the group hook. */
static MoveResult enter_group_square(const Walk *w)
{
    GroupArrival arrival = {
        .group = w->thing,
        .source = w->source,
        .landing = {w->d.map, w->d.x, w->d.y},
        .result = w->result,
        .turned_active = w->turned_active,
        .audible = w->audible,
        .removed_from_party_map = w->removed_from_party_map,
    };
    return group_enter_square(&arrival);
}

/* The walk starts on the destination square: the party stands there, a projectile keeps its flight direction. */
static void start_walk(Walk *w)
{
    w->removed_from_party_map = g_current_map_index == g_party_map_index && w->source.x >= 0;
    if (w->mover == MOVER_PARTY) {
        g_party_map_x = w->d.x;
        g_party_map_y = w->d.y;
        w->start_direction = g_party_direction;
    }
    if (w->mover == MOVER_PROJECTILE)
        w->d.direction = projectile_get_flight_direction(w->thing);
}

MoveResult move_get_move_result(const Move *move, MoveDestination *destination)
{
    int16_t mover = get_mover(move->thing);
    uint16_t src_cell = mover == MOVER_PARTY ? 0 : THING_CELL(move->thing);
    /* A thing that enters comes from (-1, 0); a thing that leaves goes to (-1, 0). */
    int16_t src_x = move->enters ? -1 : move->from.x;
    int16_t src_y = move->enters ? 0 : move->from.y;
    int16_t dst_x = move->leaves ? -1 : move->to.x;
    int16_t dst_y = move->leaves ? 0 : move->to.y;
    int16_t src_map = g_current_map_index;
    Walk w = {
        .mover = mover,
        .thing = move->thing,
        .source = {.map = src_map, .x = src_x, .y = src_y},
        .d = {.map = src_map, .x = dst_x, .y = dst_y},
        .levitating = mover != MOVER_PARTY && move_is_levitating(move->thing),
        .uses_rope = move->uses_rope,
        .keeps_cell = move->keeps_cell,
        .turned_active = -1,
        .result = MOVE_RESULT_DONE,
    };
    bool same_square = false;
    if (src_x >= 0 && (mover == MOVER_PARTY || (mover == MOVER_GROUP && src_map == g_party_map_index)) &&
        projectile_is_killed_by_impact(src_x, src_y, dst_x, dst_y, move->thing))
        return MOVE_RESULT_STOPPED;
    if (dst_x >= 0) {
        start_walk(&w);
        if (follow_square_chain(&w) || (mover == MOVER_GROUP && !group_is_allowed_on_map(w.thing, w.d.map))) {
            group_remove_after_move(w.thing, w.source, (Position){w.d.map, w.d.x, w.d.y}, &w.drops);
            return MOVE_RESULT_STOPPED;
        }
        w.d.cell = THING_CELL(w.thing);
        same_square = w.d.map == src_map && w.d.x == src_x && w.d.y == src_y;
        if (destination)
            *destination = w.d;
        if (!same_square) {
            if (mover == MOVER_PARTY && g_party_champion_count)
                add_party_scent(src_x, src_y, w.d.x, w.d.y, w.d.map);
            map_set_current_map(src_map);
        } else if (mover == MOVER_PARTY) {
            if (g_party_direction == w.start_direction)
                return MOVE_RESULT_DONE;
        } else if (w.d.cell == src_cell && mover != MOVER_PROJECTILE) {
            return MOVE_RESULT_DONE;
        }
    }
    if (src_x >= 0)
        leave_source_square(&w, same_square);
    if (dst_x < 0)
        return MOVE_RESULT_DONE;
    if (mover == MOVER_GROUP)
        return enter_group_square(&w);
    arrive_on_square(w.thing, (Position){w.d.map, w.d.x, w.d.y}, src_map, same_square);
    return MOVE_RESULT_DONE;
}

void move_process_new_party_map(void)
{
    if (g_session.new_party_map == -1)
        return;
    group_remove_all_active_groups();
    map_set_party_map(g_session.new_party_map);
    group_add_all_active_groups();
    move_get_move_result(&(Move){.thing = THING_NONE, .to.x = g_party_map_x, .to.y = g_party_map_y, .enters = true},
                         NULL);
    g_session.new_party_map = -1;
}

void move_party_enter_dungeon(void)
{
    map_reset();
    g_session.new_party_map = -1;
    map_set_party_map(g_party_map_index);
    group_add_all_active_groups();
    move_get_move_result(&(Move){.thing = THING_NONE, .to.x = g_party_map_x, .to.y = g_party_map_y, .enters = true},
                         NULL);
}

/* The party leaves the stairs square and arrives on the other level facing out of the stairs. */
static void take_stairs(bool stairs_go_up)
{
    move_get_move_result(&(Move){.thing = THING_NONE, .from.x = g_party_map_x, .from.y = g_party_map_y, .leaves = true},
                         NULL);
    Position to = map_get_level_change_location((Position){g_party_map_index, g_party_map_x, g_party_map_y},
                                                stairs_go_up ? LEVEL_UP : LEVEL_DOWN);
    g_session.new_party_map = to.map;
    g_party_map_x = to.x;
    g_party_map_y = to.y;
    map_set_current_map(g_session.new_party_map);
    turn_party(map_get_stairs_exit_direction(g_party_map_x, g_party_map_y));
    map_set_current_map(g_party_map_index);
}

void move_turn_party(int16_t command)
{
    uint8_t square = map_get_square(g_party_map_x, g_party_map_y);
    if (rec_square_type(square) == SQUARE_STAIRS) {
        take_stairs(square & STAIRS_UP);
        return;
    }
    sensor_process_thing_change(g_party_map_x, g_party_map_y, THING_NONE, true, false);
    turn_party(NORMALIZE(g_party_direction + (command == COMMAND_TURN_RIGHT ? 1 : -1)));
    sensor_process_thing_change(g_party_map_x, g_party_map_y, THING_NONE, true, true);
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
    int16_t damaged = champion_add_pending_damage(first, 1, WOUND_TORSO | WOUND_LEGS, ATTACK_SELF);
    int16_t second = group_get_target_champion_index(x, y, direction_right(cell));
    if (second != first)
        damaged |= champion_add_pending_damage(second, 1, WOUND_TORSO | WOUND_LEGS, ATTACK_SELF);
    if (damaged)
        sound_request_play(SOUND_PARTY_DAMAGED, (Location){x, y}, SOUND_MODE_NOW, 0);
}

void move_party(int16_t command)
{
    for (int16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            champion_decrement_stamina(i, (uint16_t)(g_champions[i].load * MOVE_STAMINA_FACTOR) /
                                                  champion_get_maximum_load(i) +
                                              1); /* 16-bit wrap */
    int16_t arrow = command - COMMAND_MOVE_FORWARD;
    int16_t x = g_party_map_x;
    int16_t y = g_party_map_y;
    uint8_t square = map_get_square(x, y);
    bool on_stairs = rec_square_type(square) == SQUARE_STAIRS;
    if (on_stairs && command == COMMAND_MOVE_BACKWARD) {
        take_stairs(square & STAIRS_UP);
        return;
    }
    map_step(g_party_direction, tables_get_arrow_steps_forward(arrow), tables_get_arrow_steps_right(arrow), &x, &y);
    square = map_get_square(x, y);
    if (rec_square_type(square) == SQUARE_STAIRS) {
        move_get_move_result(
            &(Move){.thing = THING_NONE, .from.x = g_party_map_x, .from.y = g_party_map_y, .leaves = true}, NULL);
        g_party_map_x = x;
        g_party_map_y = y;
        take_stairs(square & STAIRS_UP);
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
            ticks = max16(ticks, champion_get_movement_ticks(i));
    g_projectile_disabled_movement_ticks = 0;
    g_disabled_movement_ticks = ticks;
}

bool move_is_blocked(int16_t command)
{
    if (command < COMMAND_MOVE_FORWARD || command > COMMAND_MOVE_LEFT)
        return false;
    if (g_disabled_movement_ticks)
        return true;
    return g_projectile_disabled_movement_ticks && NORMALIZE(command + g_party_direction - COMMAND_MOVE_FORWARD) ==
                                                       g_last_projectile_disabled_movement_direction;
}

void move_reset(void)
{
    g_session.new_party_map = -1;
}
