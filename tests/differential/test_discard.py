# SPDX-License-Identifier: GPL-3.0-or-later
"""A thing disappears from a far square when the records of its type run out (src/engine/discard.c), against the
original get_unused_thing and get_discarded_thing with their own removal functions."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.emu import kaos_emu
from dmtest.game import DungeonMirror, Location, Position, ProjectileLaunch, new_game, place_party, setup_creatures, things
from dmtest.lib import load_lib
from dmtest.markers import needs_dm

c16, u16 = ctypes.c_int16, ctypes.c_uint16

# Original addresses (docs/reference/kaos-symbols.csv).
GET_UNUSED_THING = 0x10B1A6  # calls get_discarded_thing (0x10AF2E) when no record is free
LAST_DISCARD_MAP = 0x101C62
STEP_LIMIT = 1000

pytestmark = [needs_dm]


@pytest.fixture(scope="module")
def lib():
    lib = setup_creatures(load_lib())
    for name, res, args in [("discard_get_unused_thing", u16, [u16]), ("discard_get_last_map", c16, [u16]),
                            ("rec_thing_set_next", None, [u16, u16]), ("rec_dungeon_thing_count", u16, [c16])]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    lib.rec_object_set_important.argtypes = lib.rec_group_set_no_discard.argtypes = [u16, ctypes.c_bool]
    return lib


@pytest.fixture
def game(lib):
    new_game(lib)
    lib.move_party_enter_dungeon()
    return lib


def fill_records(lib, thing_type):
    """Every free record of the type becomes used, on no square."""
    for i in range(lib.rec_dungeon_thing_count(thing_type)):
        thing = thing_type << C.THING_TYPE_SHIFT | i
        if lib.rec_thing_next(thing) == C.THING_NONE:
            lib.rec_thing_set_next(thing, C.THING_ENDOFLIST)


def release_things(lib, thing_type):
    """Every other group or object of the type on a square may disappear, as a generated group or a dropped object."""
    found = [t for m in range(lib.dungeon_map_count()) for x in range(lib.dungeon_map_width(m))
             for y in range(lib.dungeon_map_height(m)) for t in things(lib, m, x, y) if t >> C.THING_TYPE_SHIFT & 0xF == thing_type]
    for thing in found[::2]:
        if thing_type == C.THING_GROUP:
            lib.rec_group_set_no_discard(thing, False)
        else:
            lib.rec_object_set_important(thing, False)


def open_square(lib, m):
    """The first corridor square of map m without things."""
    return next((x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m))
                if lib.rec_square_type(lib.rec_square_byte(m, x, y)) == C.SQUARE_CORRIDOR
                and lib.map_get_first_thing_at(Position(m, x, y)) == C.THING_ENDOFLIST)


def mirror(lib):
    m = DungeonMirror(lib, kaos_emu())
    m.emu.write(LAST_DISCARD_MAP, bytes(lib.discard_get_last_map(t) for t in range(C.THING_TYPE_COUNT)))
    return m


def step(lib, m, thing_type):
    """One record request in the port and in the original; both return the same thing and keep the same state."""
    got = lib.discard_get_unused_thing(thing_type)
    assert got == m.emu.call(GET_UNUSED_THING, [(2, thing_type)]) & 0xFFFF
    last = list(m.emu.read(LAST_DISCARD_MAP, C.THING_TYPE_COUNT))
    assert [lib.discard_get_last_map(t) for t in range(C.THING_TYPE_COUNT)] == last
    m.check_state()
    return got


def discard_all(lib, thing_type):
    """Requests until nothing is left to discard, then once more from the party map."""
    fill_records(lib, thing_type)
    m = mirror(lib)
    things = []
    for _ in range(STEP_LIMIT):
        thing = step(lib, m, thing_type)
        if thing == C.THING_NONE:
            break
        things.append(thing)
    assert step(lib, m, thing_type) == C.THING_NONE
    return things


@pytest.mark.parametrize("thing_type", [C.THING_WEAPON, C.THING_ARMOUR, C.THING_POTION, C.THING_JUNK])
def test_objects_disappear_like_original(game, thing_type):
    release_things(game, thing_type)
    discarded = discard_all(game, thing_type)
    assert discarded and len(set(discarded)) == len(discarded)


def test_current_map_comes_back_like_original(game):
    """A request from another map than the party map: the search and the removal end on that map again."""
    release_things(game, C.THING_WEAPON)
    game.map_set_current_map(2)
    assert discard_all(game, C.THING_WEAPON)
    assert c16.in_dll(game, "g_current_map_index").value == 2


@pytest.mark.parametrize("dx, dy, stays", [(5, 0, True), (6, 0, False), (0, 5, True), (0, 6, False), (5, 5, True)])
def test_near_party_stays_like_original(game, dx, dy, stays):
    """On the party map, a thing at most 5 squares away on both axes stays."""
    party_map = c16.in_dll(game, "g_party_map_index").value
    w, h = game.dungeon_map_width(party_map), game.dungeon_map_height(party_map)
    target, x, y = next((t, x, y) for x in range(w) for y in range(h) for t in things(game, party_map, x, y)
                        if t >> C.THING_TYPE_SHIFT & 0xF == C.THING_JUNK
                        and all(s >> C.THING_TYPE_SHIFT & 0xF != C.THING_SENSOR for s in things(game, party_map, x, y)))
    game.rec_object_set_important(target, False)
    px = x + dx if x + dx < w else x - dx
    py = y + dy if y + dy < h else y - dy
    c16.in_dll(game, "g_party_map_x").value, c16.in_dll(game, "g_party_map_y").value = px, py
    assert (target & 0x3FFF in discard_all(game, C.THING_JUNK)) != stays


@pytest.mark.parametrize("party_map", [0, 1])
def test_groups_disappear_like_original(game, party_map):
    """On map 1, the groups of the party map are active: a discarded one leaves the active groups."""
    if party_map:
        place_party(game, party_map, *open_square(game, party_map), 0)
    release_things(game, C.THING_GROUP)
    active = u16.in_dll(game, "g_active_group_count").value
    assert discard_all(game, C.THING_GROUP)
    assert (u16.in_dll(game, "g_active_group_count").value < active) == bool(party_map)


def test_junk_reserve_like_original(game):
    """A plain junk request keeps the last 3 records; NO_JUNK_RESERVE takes them before a thing disappears."""
    release_things(game, C.THING_JUNK)
    fill_records(game, C.THING_JUNK)
    count = game.rec_dungeon_thing_count(C.THING_JUNK)
    for i in range(count - 3, count):
        game.rec_thing_set_next(C.THING_JUNK << C.THING_TYPE_SHIFT | i, C.THING_NONE)
    m = mirror(game)
    assert step(game, m, C.THING_JUNK) >> C.THING_TYPE_SHIFT == C.THING_JUNK
    reserve = [step(game, m, C.THING_JUNK | C.NO_JUNK_RESERVE) for _ in range(3)]
    assert [t & 0x3FF for t in reserve] == list(range(count - 3, count))
    assert step(game, m, C.THING_JUNK | C.NO_JUNK_RESERVE) & 0x3FF < count - 3


def test_projectile_and_explosion_like_original(game):
    """A flying projectile on another map ends and drops its object; an explosion never disappears."""
    weapon = game.rec_find_unused_thing(C.THING_WEAPON)
    square = open_square(game, 1)
    game.map_set_current_map(1)
    game.projectile_create(ctypes.byref(ProjectileLaunch(weapon, Location(*square), 1, 1, 100, 50, 1)))
    game.map_set_current_map(c16.in_dll(game, "g_party_map_index").value)
    fill_records(game, C.THING_PROJECTILE)
    fill_records(game, C.THING_EXPLOSION)
    m = mirror(game)
    assert step(game, m, C.THING_PROJECTILE) >> C.THING_TYPE_SHIFT == C.THING_PROJECTILE
    game.map_set_current_map(1)
    assert game.map_get_square_first_thing(*square) & 0x3FFF == weapon
    game.map_set_current_map(c16.in_dll(game, "g_party_map_index").value)
    assert step(game, m, C.THING_PROJECTILE) == C.THING_NONE
    assert step(game, m, C.THING_EXPLOSION) == C.THING_NONE
