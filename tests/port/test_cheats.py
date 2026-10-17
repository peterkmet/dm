# SPDX-License-Identifier: GPL-3.0-or-later
"""The cheats of the advanced mode in the engine: one-hit kill and walk through walls."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.emu import var
from dmtest.game import (STEP_EAST, STEP_NORTH, Location, Position, Spell, Timeline, add_test_champion, new_game, place_party,
                         setup_creatures, session, skeleton_map_and_squares)
from dmtest.lib import load_lib
from dmtest.markers import needs_dm

c16, u16, u8 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint8

pytestmark = [needs_dm]

SKELETON_HEALTH_MULTIPLIER = 15  # far more health than a few hits take


class FallDrops(ctypes.Structure):
    _fields_ = [("count", c16), ("cells", u8 * C.GROUP_MAX_CREATURES)]


@pytest.fixture(scope="module")
def lib():
    lib = setup_creatures(load_lib())
    for name, res, args in [
        ("group_damage_creature", ctypes.c_int, [u16, c16, Location, c16]), ("rec_group_health", u16, [u16, c16]),
        ("group_door_damages_creatures", None, [c16, c16]),
        ("group_is_killed_by_fall", ctypes.c_bool, [u16, Position, Position, ctypes.POINTER(FallDrops)]),
        ("rec_group_set_creature_type", None, [u16, u16]), ("move_party", None, [c16]), ("move_turn_party", None, [c16]),
        ("rec_door_state", c16, [u8]), ("rec_sensor_type", u16, [u16]),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    yield lib
    session(lib).cheat_kill = session(lib).cheat_walls = 0


@pytest.fixture
def skeleton(lib):
    """A skeleton with much health on the square south of the party."""
    new_game(lib)
    add_test_champion(lib)
    m, (x, y), _ = skeleton_map_and_squares(lib)
    place_party(lib, m, x, y, 2)
    at = Location(x, y + 1)
    lib.group_get_generated(C.CREATURE_SKELETON, SKELETON_HEALTH_MULTIPLIER, 0, 0, at)
    group = lib.map_find_thing(at, C.THING_GROUP)
    assert group != C.THING_ENDOFLIST and lib.rec_group_health(group, 0) > 100
    yield group, at, m
    session(lib).cheat_kill = 0


def is_alive(lib, at):
    return lib.map_find_thing(at, C.THING_GROUP) != C.THING_ENDOFLIST


@pytest.mark.parametrize("cheat", [False, True])
def test_one_hit_kill_of_a_party_hit(lib, skeleton, cheat):
    group, at, _ = skeleton
    session(lib).cheat_kill = cheat
    health = lib.rec_group_health(group, 0)
    lib.group_damage_creature(group, 0, at, 0)  # no damage: no kill
    assert is_alive(lib, at) and lib.rec_group_health(group, 0) == health
    outcome = lib.group_damage_creature(group, 0, at, 3)  # a melee hit or a projectile
    assert (outcome == C.OUTCOME_ALL_CREATURES_KILLED) == cheat and is_alive(lib, at) != cheat
    if not cheat:
        assert lib.rec_group_health(group, 0) == health - 3


def test_one_hit_kill_of_an_explosion(lib, skeleton):
    group, at, _ = skeleton
    session(lib).cheat_kill = 1
    assert lib.group_damage_all_creatures(group, at.x, at.y, 8) == C.OUTCOME_ALL_CREATURES_KILLED


def test_one_hit_kill_keeps_the_invulnerable_creature(lib, skeleton):
    group, at, _ = skeleton
    lib.rec_group_set_creature_type(group, C.CREATURE_LORD_CHAOS)
    session(lib).cheat_kill = 1
    lib.group_damage_creature(group, 0, at, 3)
    assert is_alive(lib, at)


def test_one_hit_kill_skips_doors_and_falls(lib, skeleton):
    group, at, m = skeleton
    session(lib).cheat_kill = 1
    health = lib.rec_group_health(group, 0)
    lib.group_door_damages_creatures(at.x, at.y)
    assert is_alive(lib, at) and lib.rec_group_health(group, 0) < health
    drops = FallDrops()
    assert not lib.group_is_killed_by_fall(group, Position(m, at.x, at.y), Position(m, at.x, at.y), ctypes.byref(drops))
    assert is_alive(lib, at)


def test_new_game_turns_the_cheats_off(lib):
    s = session(lib)
    s.cheat_light = s.cheat_immortal = s.cheat_food = s.cheat_kill = s.cheat_walls = s.cheat_map = 1
    new_game(lib)
    assert not any((s.cheat_light, s.cheat_immortal, s.cheat_food, s.cheat_kill, s.cheat_walls, s.cheat_map))


# Walk through walls.


def square_type(lib, x, y):
    return lib.rec_square_type(lib.map_get_square(x, y))


def is_empty_corridor(lib, m, x, y):
    width, height = lib.dungeon_map_width(m), lib.dungeon_map_height(m)
    return (0 <= x < width and 0 <= y < height and lib.rec_square_type(lib.rec_square_byte(m, x, y)) == C.SQUARE_CORRIDOR
            and lib.map_get_first_thing_at(Position(m, x, y)) == C.THING_ENDOFLIST)


def find_place(lib, wanted):
    """The first empty corridor square and direction whose next square fits wanted(m, x, y), with the map."""
    for m in range(lib.dungeon_map_count()):
        lib.map_set_current_map(m)
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                for d in range(4):
                    nx, ny = x + STEP_EAST[d], y + STEP_NORTH[d]
                    if is_empty_corridor(lib, m, x, y) and wanted(m, nx, ny):
                        return m, x, y, d
    raise AssertionError("no such square")


def is_plain_wall(lib, m, x, y):
    return square_type(lib, x, y) == C.SQUARE_WALL and 0 <= x < lib.dungeon_map_width(m) and 0 <= y < lib.dungeon_map_height(m)


def is_solid_fake_wall(lib, m, x, y):
    square = lib.map_get_square(x, y)
    return lib.rec_square_type(square) == C.SQUARE_FAKE_WALL and not square & (C.FAKE_WALL_OPEN | C.FAKE_WALL_IMAGINARY)


def is_closed_door(lib, m, x, y):
    square = lib.map_get_square(x, y)
    return lib.rec_square_type(square) == C.SQUARE_DOOR and lib.rec_door_state(square) == C.DOOR_CLOSED


def is_outside(lib, m, x, y):
    return not (0 <= x < lib.dungeon_map_width(m) and 0 <= y < lib.dungeon_map_height(m))


def has_click_sensor(lib, m, x, y):
    """A wall button in the cell that the party takes on a wall square."""
    if not is_plain_wall(lib, m, x, y):
        return False
    thing = lib.map_get_square_first_thing(x, y)
    while thing != C.THING_ENDOFLIST:
        if thing >> 10 & 15 == C.THING_SENSOR and thing >> 14 == 3 and lib.rec_sensor_type(thing) == C.SENSOR_WALL_ORNAMENT_CLICK:
            return True
        thing = lib.rec_thing_next(thing)
    return False


def walk(lib, place, cheat):
    """The party on the square of place moves forward; returns the new square and the bump damage."""
    m, x, y, d = place
    new_game(lib)
    champion = add_test_champion(lib)
    place_party(lib, m, x, y, d)
    session(lib).cheat_walls = cheat
    lib.move_party(C.COMMAND_MOVE_FORWARD)
    session(lib).cheat_walls = 0
    return (var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value), champion.runtime.pending_damage


@pytest.mark.parametrize("wanted", [is_plain_wall, is_solid_fake_wall])
def test_walk_through_a_wall(lib, wanted):
    new_game(lib)
    place = find_place(lib, lambda m, x, y: wanted(lib, m, x, y))
    m, x, y, d = place
    ahead = (x + STEP_EAST[d], y + STEP_NORTH[d])
    assert walk(lib, place, True) == (ahead, 0)
    stopped, bump = walk(lib, place, False)
    assert stopped == (x, y) and bump > 0


@pytest.mark.parametrize("wanted", [is_closed_door, is_outside])
def test_walk_through_walls_stops_at_doors_and_the_border(lib, wanted):
    new_game(lib)
    place = find_place(lib, lambda m, x, y: wanted(lib, m, x, y))
    assert walk(lib, place, True)[0] == place[1:3]


def test_walk_through_walls_stops_at_a_creature(lib, skeleton):
    _, at, m = skeleton
    x, y = var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value
    session(lib).cheat_walls = 1
    lib.move_party(C.COMMAND_MOVE_FORWARD)
    session(lib).cheat_walls = 0
    assert (var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value) == (x, y)


def test_leave_a_wall_with_the_cheat_off(lib):
    new_game(lib)
    m, x, y, d = find_place(lib, lambda m, x, y: is_plain_wall(lib, m, x, y))
    wall = (x + STEP_EAST[d], y + STEP_NORTH[d])
    back = (d + 2) & 3
    new_game(lib)
    add_test_champion(lib)
    place_party(lib, m, *wall, back)
    lib.move_party(C.COMMAND_MOVE_FORWARD)
    assert (var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value) == (x, y)
    place_party(lib, m, *wall, d)
    ahead = (wall[0] + STEP_EAST[d], wall[1] + STEP_NORTH[d])
    if square_type(lib, *ahead) == C.SQUARE_WALL:
        lib.move_party(C.COMMAND_MOVE_FORWARD)
        assert (var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value) == wall


def test_no_wall_sensor_runs_for_the_party_in_a_wall(lib):
    new_game(lib)
    place = find_place(lib, lambda m, x, y: has_click_sensor(lib, m, x, y))
    timeline = Timeline.in_dll(lib, "g_timeline")
    new_game(lib)
    add_test_champion(lib)
    place_party(lib, place[0], *place[1:3], place[3])
    count = timeline.count
    session(lib).cheat_walls = 1
    lib.move_party(C.COMMAND_MOVE_FORWARD)
    session(lib).cheat_walls = 0
    assert (var(lib, c16, "g_party_map_x").value, var(lib, c16, "g_party_map_y").value) != place[1:3]
    lib.move_turn_party(C.COMMAND_TURN_RIGHT)
    lib.move_turn_party(C.COMMAND_TURN_RIGHT)
    lib.move_party(C.COMMAND_MOVE_FORWARD)
    assert timeline.count == count


# The spell list of the cheat screen.


@pytest.mark.parametrize("symbols, line", [
    (0x00696F00, "FUL IR (X,4,4) FIREBALL"),  # any power
    (0x60696F00, "LO FUL IR (1,4,4) FIREBALL"),  # a fixed power symbol
    (0x00697075, "FUL BRO NETA (X,4,5,4) FIRE SHIELD"),
])
def test_spell_line(lib, symbols, line):
    lib.cheats_get_spell_line.argtypes = [ctypes.POINTER(Spell), ctypes.c_char_p, ctypes.c_size_t]
    text = ctypes.create_string_buffer(64)
    kind, type_ = (C.SPELL_KIND_PROJECTILE, C.EXPLOSION_FIREBALL) if "FIREBALL" in line else (C.SPELL_KIND_OTHER, C.SPELL_OTHER_FIRE_SHIELD)
    lib.cheats_get_spell_line(ctypes.byref(Spell(symbols=symbols, kind=kind, type=type_)), text, len(text))
    assert text.value.decode() == line
