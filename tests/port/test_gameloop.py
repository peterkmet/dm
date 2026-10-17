# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes

import pytest

from dmtest import constants as C
from dmtest.game import pending_damage, STEP_EAST, STEP_NORTH, Box, Party, Position, start_game, session, screen
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

ZONE_RESURRECT, ZONE_DROP_PARTY_CELL, ZONE_BACKPACK_FIRST_SLOT = 570, 740, 520
c16, u16 = ctypes.c_int16, ctypes.c_uint16

pytestmark = [needs_dm]


@pytest.fixture(scope="module")
def game():
    return start_game(load_lib())


def test_champion_dies_with_inventory_open(game):
    assert game.lib.gameloop_start_new_game(str(GAME).encode())
    game.run(26)
    mirrors = set()
    for _ in range(2):
        path, direction = game.path_to_mirror(mirrors)
        game.walk(path)
        game.face(direction)
        game.run()
        game.click_box(game.boxes[5])
        game.click_zone(ZONE_RESURRECT)
        x, y = path[-1]
        mirrors.add((x + STEP_EAST[direction], y + STEP_NORTH[direction]))
    assert game.value("g_party_champion_count", c16) == 2
    game.key(C.KEY_F1 + 0x100)
    game.run()
    assert game.session.inventory_champion_index == 1

    game.lib.champion_add_pending_damage(1, 10000, 0, C.ATTACK_RAW)
    game.run()
    assert game.champions[1].current_health == 0
    assert game.session.inventory_champion_index == -1
    game.click_zone(ZONE_BACKPACK_FIRST_SLOT)  # the dungeon view takes the click; no slot of champion -1

    box = Box()
    assert game.lib.ui_get_slot_box_zone(C.SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) != 0xFFFF
    game.click_zone(ZONE_DROP_PARTY_CELL)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF

    direction = game.position()[2]
    game.key(C.KEY_DEL)
    assert game.until(lambda: game.position()[2] == (direction + 3) & 3)


def viewport_pixels(game):
    fb = screen(game.lib).px
    x0, y0 = game.origin
    return [fb[(y0 + y) * 320 + x0 + x] for y in range(136) for x in range(224)]


def test_freeze_covers_open_inventory(game):
    assert game.lib.gameloop_start_new_game(str(GAME).encode())
    game.run(26)
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    game.key(C.KEY_F1)
    game.run()
    assert game.session.inventory_champion_index == 0
    inventory = viewport_pixels(game)

    game.key(C.KEY_ESCAPE)
    game.run(39)
    assert game.value("g_game_frozen") == 1
    frozen = viewport_pixels(game)
    assert len(set(frozen)) == 2  # black and the GAME FROZEN text, no slot icons on top

    for key in (C.KEY_ESCAPE, C.KEY_RETURN):
        game.key(key)
        game.run()
        if not game.value("g_game_frozen"):
            break
    assert game.value("g_game_frozen") == 0
    assert viewport_pixels(game) == inventory


def test_party_light_sets_the_darkness_of_view_and_rules(game):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    lib.explored_get_visible_depths.restype = c16
    lib.explored_get_visible_depths.argtypes = [Position, c16]
    party = Party.in_dll(lib, "g_party")
    darkness = c16.in_dll(lib, "g_party_light_level")
    pal = screen(lib).palette
    place = {n: c16.in_dll(lib, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")}
    start = {n: v.value for n, v in place.items()}

    def view(light):
        party.magical_light_amount = light
        lib.gameloop_draw_view()
        return darkness.value, pal[:16], lib.explored_get_visible_depths(Position(1, 14, 4), 3)

    try:
        for n, v in zip(place, (1, 14, 4, 3)):  # map 1 has darkness, a long corridor ahead
            place[n].value = v
        bright, dark = view(1000), view(-1000)
        assert bright[0] == 0 and dark[0] == 5
        assert bright[1] != dark[1]
        assert dark[2] < bright[2]
        session(lib).party_sleeping = 1  # no view is drawn, the creature rules still see the light of this tick
        party.magical_light_amount = 1000
        lib.gameloop_tick()
        assert darkness.value == 0
        session(lib).party_sleeping = 0
        for n, v in start.items():
            place[n].value = v
        assert view(-1000)[0] == 0  # map 0 has no darkness
    finally:
        party.magical_light_amount = 0
        for n, v in start.items():
            place[n].value = v
        lib.gameloop_draw_view()


def test_hand_object_name_follows_the_icon(game):
    """The last drink from a waterskin changes its icon and its name from WATER to WATERSKIN."""
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    game.run(26)
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    lib.tables_get_object_info.restype = ctypes.POINTER(u16 * 3)
    junk = next(j for j in range(53) if lib.tables_get_object_info(0x7F + j).contents[0] == 8)  # waterskin
    lib.rec_find_unused_thing.restype = u16
    skin = lib.rec_find_unused_thing(10)
    data = lib.rec_write(skin)
    data[3], data[2] = junk, 1 << 6  # one charge
    lib.inventory_put_hand_object(skin)
    game.key(C.KEY_F1)
    game.run()
    box = Box()
    assert lib.ui_zone_box(17, ctypes.byref(box))  # the leader hand object name
    fb = screen(lib).px
    name = lambda: [fb[(box.y + y) * 320 + box.x + x] for y in range(box.h) for x in range(box.w)]
    before = name()
    game.click_zone(545)  # the mouth
    assert lib.rec_write(skin)[2] >> 6 == 0
    assert name() != before
    game.key(C.KEY_F1)
    game.run()


class PointerImage(ctypes.Structure):
    _fields_ = [("px", ctypes.c_uint8 * (C.POINTER_SIZE * C.POINTER_SIZE)), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("hot_x", ctypes.c_int), ("hot_y", ctypes.c_int)]


def pointer(lib, x, y):
    """The pointer at (x, y) in the game: width, height, and hot spot (they differ by shape)."""
    image = PointerImage()
    assert lib.gameloop_get_pointer(x, y, 1, ctypes.byref(image))
    return image.w, image.h, image.hot_x, image.hot_y


def test_pointer_over_the_status_box_hands(game):
    """KAOS 0x108530: over the hands of a champion, the pointer shows the hand object, unless that inventory is open."""
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    game.run(26)
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    box = Box()
    assert lib.ui_get_slot_box_zone(C.SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)  # the weapon goes into the leader hand
    hand = (box.x + box.w // 2, box.y + box.h // 2)
    viewport_object, arrow = pointer(lib, 100, 100), pointer(lib, 300, 100)
    assert viewport_object != arrow
    assert pointer(lib, *hand) == viewport_object
    game.key(C.KEY_F1)
    game.run()
    assert pointer(lib, *hand) == arrow
    game.key(C.KEY_F1)
    game.run()


def two_champions_with_runes(game):
    """A new game with two recruited champions; the second has chosen a rune, the first is the caster."""
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    game.run(26)
    mirrors = set()
    for _ in range(2):
        path, direction = game.path_to_mirror(mirrors)
        game.walk(path)
        game.face(direction)
        game.run()
        game.click_box(game.boxes[5])
        game.click_zone(ZONE_RESURRECT)
        mirrors.add((path[-1][0] + STEP_EAST[direction], path[-1][1] + STEP_NORTH[direction]))
    assert game.value("g_party_champion_count", c16) == 2 and game.value("g_magic_caster_champion_index") == 0
    game.champions[1].symbol_step, game.champions[1].symbols = 1, bytes([C.SPELL_SYMBOL_FIRST])


def spell_area(game):
    fb = screen(game.lib).px
    box = Box()
    assert game.lib.ui_zone_box(C.ZONE_SPELL_AREA, ctypes.byref(box))
    return [fb[(box.y + y) * 320 + box.x + x] for y in range(box.h) for x in range(box.w)]


def test_a_party_that_dies_in_one_tick_shows_the_spell_area_of_the_last_caster(game):
    """The caster dies first: the spell area goes to the last champion as it was before its own death in the same tick."""
    lib = game.lib
    pending = pending_damage(lib)
    two_champions_with_runes(game)
    pending[0] = game.champions[0].current_health
    lib.gameloop_tick()
    assert game.champions[1].current_health and game.value("g_magic_caster_champion_index") == 1
    expected = spell_area(game)

    two_champions_with_runes(game)
    frames = []

    @ctypes.CFUNCTYPE(None)
    def first_frame():
        if not frames:
            frames.append(spell_area(game))
        ctypes.c_bool.in_dll(lib, "g_quit_requested").value = 1  # leaves the death scream

    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(first_frame, ctypes.c_void_p).value
    try:
        pending[0], pending[1] = game.champions[0].current_health, game.champions[1].current_health
        lib.gameloop_tick()
    finally:
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None
        ctypes.c_bool.in_dll(lib, "g_quit_requested").value = 0
    assert game.session.party_dead and frames[0] == expected
