/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "discard.h"
#include "arith.h"
#include "dungeon.h"
#include "group.h"
#include "map.h"
#include "move.h"
#include "party.h"
#include "projectile.h"
#include "record.h"
#include "sound.h"
#include <string.h>

/* Limits of the search for a thing to discard. */
#define NEAR_PARTY_SQUARES 5 /* a thing at most this many squares from the party on both axes stays */
#define SEARCH_MIN_MAPS    2 /* with fewer maps, only the first map of the search is searched */

/* A thing to discard and its square. */
typedef struct {
    uint16_t thing; /* THING_NONE: none */
    Position place;
} Found;

static struct {
    int16_t last_map[THING_TYPE_COUNT]; /* where the last search of each thing type ended */
} discard;

/* Setup */

void discard_reset(void)
{
    memset(&discard, 0, sizeof discard);
}

/* Queries */

int16_t discard_get_last_map(uint16_t thing_type)
{
    return discard.last_map[thing_type];
}

/* Commands */

/* A group without possessions that may be discarded, an object that is not important, or a thing of another type. */
static bool is_discardable(uint16_t thing)
{
    switch (THING_TYPE(thing)) {
    case THING_GROUP:
        return !rec_group_is_no_discard(thing) && rec_group_possessions(thing) == THING_ENDOFLIST;
    case THING_WEAPON:
    case THING_ARMOUR:
    case THING_POTION:
    case THING_JUNK:
        return !rec_object_is_important(thing);
    default:
        return true;
    }
}

/* The first thing of the type on the square that can be discarded. A sensor that is not disabled protects the
   whole square. */
static uint16_t find_on_square(Position place, uint16_t type)
{
    for (uint16_t thing = map_get_first_thing_at(place); thing != THING_ENDOFLIST; thing = rec_thing_next(thing)) {
        if (THING_TYPE(thing) == THING_SENSOR) {
            if (rec_sensor_type(thing) != SENSOR_DISABLED)
                return THING_NONE;
        } else if (THING_TYPE(thing) == type && is_discardable(thing)) {
            return thing;
        }
    }
    return THING_NONE;
}

/* A square of the party map near the party. */
static bool is_near_party(Position place)
{
    return place.map == g_party_map_index && abs16(place.x - g_party_map_x) <= NEAR_PARTY_SQUARES &&
           abs16(place.y - g_party_map_y) <= NEAR_PARTY_SQUARES;
}

/* The first thing of the type on the map that can be discarded, column by column, outside the squares near the
   party. */
static Found find_on_map(int16_t map, uint16_t type)
{
    for (int16_t x = 0; x < dungeon_map_width(map); x++) {
        for (int16_t y = 0; y < dungeon_map_height(map); y++) {
            Position place = {map, x, y};
            if (is_near_party(place))
                continue;
            uint16_t thing = find_on_square(place, type);
            if (thing != THING_NONE)
                return (Found){thing, place};
        }
    }
    return (Found){.thing = THING_NONE};
}

/* The thing leaves its square: a group is deleted, a projectile ends and drops its object, an object leaves the
   map. A thing of another type stays on its square. */
static void remove_thing(uint16_t thing, Position place)
{
    switch (THING_TYPE(thing)) {
    case THING_GROUP:
        map_set_current_map(place.map);
        group_drop_group_possessions(place.x, place.y, thing, SOUND_MODE_NONE);
        group_delete(place.x, place.y);
        break;
    case THING_PROJECTILE:
        map_set_current_map(place.map);
        projectile_delete_event(thing);
        map_unlink_thing_from_list(thing, 0, place.x, place.y);
        projectile_delete(thing, THING_NONE, place.x, place.y);
        break;
    case THING_WEAPON:
    case THING_ARMOUR:
    case THING_POTION:
    case THING_JUNK:
        map_set_current_map(place.map);
        move_get_move_result(&(Move){.thing = thing, .from.x = place.x, .from.y = place.y, .leaves = true}, NULL);
        break;
    default:
        break;
    }
}

/* The map after map in the search order: the maps after it with a wrap, without the party map; after a full round
   the party map. */
static int16_t next_search_map(int16_t map, int16_t first)
{
    do {
        map++;
        if (map >= dungeon_map_count())
            map = 0;
    } while (map == g_party_map_index);
    return map == first ? g_party_map_index : map;
}

/* Removes a thing of the type from a far square and returns it without its cell; THING_NONE when none can go.
   The search starts on the map where the last search of the type ended and searches the party map last. */
static uint16_t get_discarded_thing(uint16_t type)
{
    if (type == THING_EXPLOSION)
        return THING_NONE;
    int16_t current = g_current_map_index;
    int16_t map = discard.last_map[type];
    if (map == g_party_map_index && ++map >= dungeon_map_count())
        map = 0;
    int16_t first = map;
    for (;;) {
        Found found = find_on_map(map, type);
        if (found.thing != THING_NONE) {
            remove_thing(found.thing, found.place);
            map_set_current_map(current);
            discard.last_map[type] = map;
            return THING_STRIP_CELL(found.thing);
        }
        if (map == g_party_map_index || dungeon_map_count() < SEARCH_MIN_MAPS) {
            discard.last_map[type] = map;
            return THING_NONE;
        }
        map = next_search_map(map, first);
    }
}

uint16_t discard_get_unused_thing(uint16_t thing_type)
{
    uint16_t thing = rec_find_unused_thing(thing_type);
    if (thing != THING_NONE)
        return thing;
    thing = get_discarded_thing(thing_type & ~NO_JUNK_RESERVE);
    if (thing != THING_NONE)
        rec_thing_clear(thing);
    return thing;
}
