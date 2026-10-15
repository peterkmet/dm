#include "move.h"
#include "champion.h"
#include "dungeon.h"
#include "game.h"
#include "group.h"
#include "inventory.h"
#include "map.h"
#include "projectile.h"
#include "sensor.h"
#include "sound.h"
#include "tables.h"
#include "timeline_process.h"

#define SCENT_STRENGTH_MAXIMUM 80
#define SCENT_KEEP_LARGER      0x8000 /* flag: keep the larger strength instead of adding */
#define SCENT_NEW_STRENGTH     24
#define MOVE_STAMINA_FACTOR    3    /* stamina cost of a step: load * 3 / maximum load + 1 */
#define NO_TELEPORTER_ROTATION (-2) /* src_x value: a teleporter does not rotate the object */

int16_t g_new_party_map_index = -1;
int16_t g_move_result_map_x, g_move_result_map_y;
uint16_t g_move_result_map_index, g_move_result_cell, g_move_result_direction;
int16_t g_use_rope_to_climb_down_pit;
int g_party_teleport_count;

/* Levitating groups, projectiles, and explosions do not fall into pits and do not trigger sensors when they leave a square. */
bool move_is_levitating(uint16_t thing)
{
    int16_t type = THING_TYPE(thing);
    if (type == THING_GROUP)
        return get_creature_attributes(thing) & CREATURE_LEVITATES;
    return type == THING_PROJECTILE || type == THING_EXPLOSION;
}

/* A teleporter turns a projectile: its flight direction and its cell. */
uint16_t move_get_teleporter_rotated_projectile_thing(uint16_t teleporter, uint16_t thing)
{
    uint16_t rotation = teleporter_rotation(teleporter);
    if (teleporter_is_rotation_absolute(teleporter)) {
        g_move_result_direction = rotation;
        return thing;
    }
    g_move_result_direction = NORMALIZE(rotation + g_move_result_direction);
    return THING_WITH_CELL(thing, rotation + THING_CELL(thing));
}

/* Turns the party. Each champion keeps its place relative to the party. The loop also runs one time without champions. */
void move_set_party_direction(int16_t direction)
{
    int16_t delta = direction - g_party_direction;
    g_party_direction = direction;
    if (!delta)
        return;
    int16_t n = g_party_champion_count ? g_party_champion_count : 1;
    for (int16_t i = 0; i < n; i++) {
        g_champions[i].cell = (uint8_t)NORMALIZE(g_champions[i].cell + delta);
        g_champions[i].direction = (uint8_t)NORMALIZE(g_champions[i].direction + delta);
    }
}

/* Adds strength to the party scent on a square, or keeps the larger strength. The original compares scent i
   with strength count - 1 - i; the port keeps this. */
void move_add_scent_strength(int16_t x, int16_t y, int16_t strength_and_flags)
{
    if (!g_party.scent_count)
        return;
    int16_t keep_larger = strength_and_flags & SCENT_KEEP_LARGER, found = 0;
    int16_t value = keep_larger ? strength_and_flags & ~SCENT_KEEP_LARGER : strength_and_flags;
    uint16_t scent = position_pack(g_current_map_index, x, y);
    for (uint16_t i = 0; i < g_party.scent_count; i++) {
        uint16_t s = g_party.scent_count - 1 - i;
        if (g_party.scents[i] != scent)
            continue;
        if (!found) {
            found = 1;
            value = keep_larger ? max16(g_party.scent_strengths[s], value)
                                : min16(SCENT_STRENGTH_MAXIMUM, value + g_party.scent_strengths[s]);
        }
        g_party.scent_strengths[s] = (uint8_t)value;
    }
}

/* The party leaves a scent on its new square. The old square gets strength for the time the party stood there. */
static void add_party_scent(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, int16_t map)
{
    uint16_t n = g_party.scent_count;
    for (; n >= PARTY_SCENT_COUNT; n--)
        champion_delete_scent(0);
    if (n)
        move_add_scent_strength(src_x, src_y, g_game_time - g_last_party_movement_time); /* 16-bit difference */
    g_last_party_movement_time = g_game_time;
    g_party.scent_count++;
    if (g_party.event79_count_footprints)
        g_party.last_scent_index = g_party.scent_count;
    g_party.scents[n] = position_pack(map, dst_x, dst_y);
    g_party.scent_strengths[n] = 0;
    move_add_scent_strength(dst_x, dst_y,
                            (int16_t)(SCENT_KEEP_LARGER | SCENT_NEW_STRENGTH)); /* the flag is the sign bit */
}

static bool is_party_on(int16_t x, int16_t y)
{
    return g_current_map_index == g_party_map_index && x == g_party_map_x && y == g_party_map_y;
}

/* A teleporter on the destination square moves the thing to its target and turns it. The party hears it and keeps
   the count of teleports. Returns false when the chain stops: a closed teleporter, another scope, or a teleporter
   that points to itself. */
static bool is_teleported(uint16_t *thing, uint8_t square, int16_t src_x, int16_t *dst_x, int16_t *dst_y, int16_t *map)
{
    int16_t party = *thing == THING_NONE, type = party ? -1 : THING_TYPE(*thing);
    uint16_t scope = party ? TELEPORTER_SCOPE_PARTY : TELEPORTER_SCOPE_ALL;
    if (!(square & TELEPORTER_OPEN))
        return false;
    uint16_t teleporter = get_square_first_thing(*dst_x, *dst_y);
    uint16_t teleporter_scope_bits = teleporter_scope(teleporter);
    if (teleporter_scope_bits == TELEPORTER_SCOPE_CREATURES ||
        (scope != TELEPORTER_SCOPE_ALL && !(scope & teleporter_scope_bits)))
        return false;
    int16_t x = teleporter_target_x(teleporter), y = teleporter_target_y(teleporter),
            rotation = teleporter_rotation(teleporter);
    int16_t points_to_itself = x == *dst_x && y == *dst_y && teleporter_target_map(teleporter) == *map;
    *dst_x = x;
    *dst_y = y;
    *map = teleporter_target_map(teleporter);
    set_current_map(*map);
    if (party) {
        g_party_map_x = *dst_x;
        g_party_map_y = *dst_y;
        g_party_teleport_count += !points_to_itself;
        if (teleporter_is_audible(teleporter))
            sound_request_play(SOUND_TELEPORT, *dst_x, *dst_y, SOUND_MODE_NOW);
        move_set_party_direction(teleporter_is_rotation_absolute(teleporter) ? rotation
                                                                             : NORMALIZE(g_party_direction + rotation));
    } else if (type == THING_PROJECTILE) {
        *thing = move_get_teleporter_rotated_projectile_thing(teleporter, *thing);
    } else if (!teleporter_is_rotation_absolute(teleporter) && src_x != NO_TELEPORTER_ROTATION) {
        *thing = THING_WITH_CELL(*thing, rotation + THING_CELL(*thing));
    }
    return !points_to_itself;
}

/* The destination follows teleporters, pits, and stairs. The party falls into a pit with its damage. An object on
   stairs rolls out of them. Returns the thing with its new cell. */
static uint16_t follow_square_chain(uint16_t thing, int16_t src_x, int16_t *dst_x, int16_t *dst_y, int16_t *map,
                                    int16_t *start_direction)
{
    int16_t party = thing == THING_NONE, type = party ? -1 : THING_TYPE(thing);
    int16_t levitating = party ? 0 : move_is_levitating(thing);
    for (int16_t n = SQUARE_CHAIN_MAXIMUM; n; n--) {
        uint8_t square = g_current_map_data[*dst_x][*dst_y];
        int16_t square_type = SQUARE_TYPE(square);
        if (square_type == SQUARE_TELEPORTER) {
            if (!is_teleported(&thing, square, src_x, dst_x, dst_y, map))
                break;
        } else if (square_type == SQUARE_PIT && !levitating && (square & PIT_OPEN) && !(square & PIT_IMAGINARY)) {
            *map = get_location_after_level_change(*map, LEVEL_DOWN, dst_x, dst_y);
            set_current_map(*map);
            if (party) {
                g_party_map_x = *dst_x;
                g_party_map_y = *dst_y;
                champion_fall_damage(g_use_rope_to_climb_down_pit);
                g_use_rope_to_climb_down_pit = 0;
            }
        } else if (square_type == SQUARE_STAIRS && !party && type != THING_PROJECTILE) {
            /* An object on down stairs falls one level. On up stairs it only rolls out. */
            if (!(square & STAIRS_UP)) {
                *map = get_location_after_level_change(*map, LEVEL_DOWN, dst_x, dst_y);
                set_current_map(*map);
            }
            int16_t exit = get_stairs_exit_direction(*dst_x, *dst_y);
            update_map_coordinates_after_relative_movement(exit, 1, 0, dst_x, dst_y);
            *start_direction = turn_back(exit);
            /* The object takes the cell on the side it came from, next to the stairs. */
            uint16_t cell = *start_direction + ((((THING_CELL(thing) - *start_direction) + 1) & 2) >> 1);
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
                             int16_t same_square)
{
    set_current_map(map);
    if (thing == THING_NONE) {
        uint16_t group = get_square_group(g_party_map_x, g_party_map_y);
        if (group != THING_ENDOFLIST) {
            group_drop_group_possessions(g_party_map_x, g_party_map_y, group, SOUND_MODE_AFTER_TICK);
            group_delete(g_party_map_x, g_party_map_y);
        }
        if (map == src_map) {
            sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, same_square, 1);
        } else {
            set_current_map(src_map);
            g_new_party_map_index = map;
        }
        return;
    }
    if (THING_TYPE(thing) == THING_PROJECTILE)
        link_thing_to_list(thing, 0, dst_x, dst_y);
    else
        sensor_process_thing_addition_or_removal(dst_x, dst_y, thing, is_party_on(dst_x, dst_y), 1);
    set_current_map(src_map);
}

/* thing THING_NONE is the party. src_x < 0: add only. dst_x < 0: remove only. The destination follows teleporters,
   pits, and stairs. Returns 0. A group moves with group_get_move_result (group.c). */
int16_t move_get_move_result(uint16_t thing, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y)
{
    int16_t party = thing == THING_NONE, type = party ? -1 : THING_TYPE(thing);
    uint16_t src_cell = party ? 0 : THING_CELL(thing);
    int16_t levitating = party ? 0 : move_is_levitating(thing);
    int16_t src_map = g_current_map_index, map = src_map, same_square = 0, start_direction = 0;
    if (type == THING_GROUP)
        return group_get_move_result(thing, src_x, src_y, dst_x, dst_y);
    if (party && src_x >= 0 && projectile_is_killed_by_impact(src_x, src_y, dst_x, dst_y, THING_NONE))
        return 1;
    if (dst_x >= 0) {
        if (party) {
            g_party_map_x = dst_x;
            g_party_map_y = dst_y;
            start_direction = g_party_direction;
        }
        if (type == THING_PROJECTILE)
            g_move_result_direction = projectile_event_direction(&g_timeline_events[rec_projectile_event_index(thing)]);
        thing = follow_square_chain(thing, src_x, &dst_x, &dst_y, &map, &start_direction);
        g_move_result_cell = THING_CELL(thing);
        same_square = map == src_map && dst_x == src_x && dst_y == src_y;
        g_move_result_map_x = dst_x;
        g_move_result_map_y = dst_y;
        g_move_result_map_index = map;
        if (!same_square) {
            if (party && g_party_champion_count)
                add_party_scent(src_x, src_y, dst_x, dst_y, map);
            set_current_map(src_map);
        } else if (party) {
            if (g_party_direction == start_direction)
                return 0;
        } else if (g_move_result_cell == src_cell && type != THING_PROJECTILE) {
            return 0;
        }
    }
    if (src_x >= 0) {
        if (party)
            sensor_process_thing_addition_or_removal(src_x, src_y, THING_NONE, same_square, 0);
        else if (!levitating)
            sensor_process_thing_addition_or_removal(src_x, src_y, thing, is_party_on(src_x, src_y), 0);
        else
            unlink_thing_from_list(thing, 0, src_x, src_y);
    }
    if (dst_x >= 0)
        arrive_on_square(thing, dst_x, dst_y, map, src_map, same_square);
    return 0;
}

/* The map change of the game loop: the new map becomes the party map, then the party arrives with its sensors. */
void move_process_new_party_map(void)
{
    if (g_new_party_map_index == -1)
        return;
    group_remove_all_active_groups();
    set_current_map_and_party_map(g_new_party_map_index);
    group_add_all_active_groups();
    move_get_move_result(THING_NONE, -1, 0, g_party_map_x, g_party_map_y);
    g_new_party_map_index = -1;
}

/* New game: the party map becomes current and the party enters the start square. */
void move_party_enter_dungeon(void)
{
    map_reset();
    g_new_party_map_index = -1;
    set_current_map_and_party_map(g_party_map_index);
    group_add_all_active_groups();
    move_get_move_result(THING_NONE, -1, 0, g_party_map_x, g_party_map_y);
}

/* The party leaves the stairs square and arrives on the other level facing out of the stairs. */
void move_take_stairs(int16_t stairs_go_up)
{
    move_get_move_result(THING_NONE, g_party_map_x, g_party_map_y, -1, 0);
    g_new_party_map_index = get_location_after_level_change(g_party_map_index, stairs_go_up ? LEVEL_UP : LEVEL_DOWN,
                                                            &g_party_map_x, &g_party_map_y);
    set_current_map(g_new_party_map_index);
    move_set_party_direction(get_stairs_exit_direction(g_party_map_x, g_party_map_y));
    set_current_map(g_party_map_index);
}

/* On stairs, a turn takes the stairs. The party leaves the square sensors before the turn and enters them after it. */
void move_turn_party(int16_t command)
{
    uint8_t square = get_square(g_party_map_x, g_party_map_y);
    if (SQUARE_TYPE(square) == SQUARE_STAIRS) {
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, 1, 0);
    move_set_party_direction(NORMALIZE(g_party_direction + (command == COMMAND_TURN_RIGHT ? 1 : -1)));
    sensor_process_thing_addition_or_removal(g_party_map_x, g_party_map_y, THING_NONE, 1, 1);
}

/* A wall, a door that is not open enough, or a solid fake wall stops the party. */
static bool is_square_blocking(uint8_t square)
{
    int16_t state = door_state(square);
    switch (SQUARE_TYPE(square)) {
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
    int16_t second = group_get_target_champion_index(x, y, turn_right(cell));
    if (second != first)
        damaged |= champion_add_pending_damage_and_wounds(second, 1, WOUND_TORSO | WOUND_LEGS, ATTACK_SELF);
    if (damaged)
        sound_request_play(SOUND_PARTY_DAMAGED, x, y, SOUND_MODE_NOW);
}

/* One step of the party. With champions, a bump hurts two champions and a group in the way blocks the step and
   reacts to the party. */
void move_party(int16_t command)
{
    for (int16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            champion_decrement_stamina(i, (uint16_t)(g_champions[i].load * MOVE_STAMINA_FACTOR) /
                                                  champion_get_maximum_load(&g_champions[i]) +
                                              1); /* 16-bit wrap */
    int16_t arrow = command - COMMAND_MOVE_FORWARD, x = g_party_map_x, y = g_party_map_y;
    uint8_t square = get_square(x, y);
    int16_t on_stairs = SQUARE_TYPE(square) == SQUARE_STAIRS;
    if (on_stairs && command == COMMAND_MOVE_BACKWARD) {
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    update_map_coordinates_after_relative_movement(g_party_direction, tables_arrow_steps_forward(arrow),
                                                   tables_arrow_steps_right(arrow), &x, &y);
    square = get_square(x, y);
    if (SQUARE_TYPE(square) == SQUARE_STAIRS) {
        move_get_move_result(THING_NONE, g_party_map_x, g_party_map_y, -1, 0);
        g_party_map_x = x;
        g_party_map_y = y;
        move_take_stairs(square & STAIRS_UP);
        return;
    }
    int16_t blocked = is_square_blocking(square);
    if (g_party_champion_count) {
        if (blocked) {
            bump_into_wall(x, y, arrow);
        } else {
            blocked = get_square_group(x, y) != THING_ENDOFLIST;
            if (blocked)
                group_process_reaction_event(x, y, REACTION_PARTY_IS_ADJACENT, 0);
        }
    }
    if (blocked)
        return;
    if (on_stairs)
        move_get_move_result(THING_NONE, -1, 0, x, y);
    else
        move_get_move_result(THING_NONE, g_party_map_x, g_party_map_y, x, y);
    int16_t ticks = 1;
    for (int16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            ticks = max16(ticks, champion_get_movement_ticks(&g_champions[i]));
    g_projectile_disabled_movement_ticks = 0;
    g_disabled_movement_ticks = ticks;
}

/* No map change and no rope climb wait at the start of a game. */
void move_reset(void)
{
    g_new_party_map_index = -1;
    g_use_rope_to_climb_down_pit = 0;
}
