"""Golden snapshots of the game rules that run in ui: each scenario gives the same game state and screen."""

import ctypes
import hashlib
import json
import os
import shutil
import subprocess

import pytest

from test_champion import Party
from test_gameloop import Box, Game
from test_golden_save import scrub_free_groups
from test_port import ROOT
from test_save import GAME, recruit_and_hold_object, state

c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

GOLDEN = ROOT / "tests" / "golden" / "rules.json"
SEED = 0x1234ABCD
VANILLA, ADVANCED = 0, 1
THING_WEAPON, THING_POTION, THING_JUNK = 5, 8, 10
WEAPON_TORCH, POTION_EMPTY_FLASK, JUNK_WATERSKIN, JUNK_BONES = 2, 20, 1, 5
SLOT_HAND_READY, SLOT_HAND_ACTION = 0, 1
FOUNTAIN = 1, 0, 21, 2       # map, x, y, direction: facing a fountain on level 2
VI_ALTAR = 0, 4, 17, 2       # facing the vi altar of level 1
DOOR_BUTTON = 0, 1, 13, 2    # facing the first door with a button
ATTACKED = 5, 0, 0, 0        # a map that allows skeletons, with a free corridor square east of the party
ZONE_ACTION_ICON_FIRST = 89  # + champion index: the action hand icon of the action area
ZONE_SPELL_SYMBOL_BOX_FIRST, ZONE_SPELL_DELETE_BOX = 245, 254
CREATURE_SKELETON = 12
SKILL_COUNT = 20
# (weapon type, charges): weapon 4 has an action that needs charges, once without and once with them
WEAPONS = (WEAPON_TORCH, 0), (8, 0), (9, 0), (10, 0), (4, 0), (4, 3)

needs_game = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists(), reason="game files missing")


@pytest.fixture
def game(tmp_path):
    """A new game with a fixed seed in a private copy of libdm."""
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    # a private copy: an xdist worker shares one image per path, so earlier modules would leave their state
    copy = tmp_path / "libdm.dylib"
    shutil.copy(ROOT / "build/libdm.dylib", copy)
    lib = ctypes.CDLL(str(copy))
    # the types that Game, state and recruit_and_hold_object need, as the fixture of tests/test_save.py
    lib.gameloop_load_data.restype = lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool
    lib.ui_zone_box.restype = ctypes.c_bool
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.map_get_next_thing.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.map_get_unused_thing.restype = u16
    lib.map_get_unused_thing.argtypes = [u16]
    lib.kaos_s16.restype = c16
    lib.dungeon_save.restype = ctypes.c_void_p
    lib.free.argtypes = [ctypes.c_void_p]
    lib.inventory_get_object_weight.restype = u16
    lib.group_get_generated.restype = u16
    lib.group_get_generated.argtypes = [u16, c16, u16, c16, c16, c16]
    lib.gfx_action_name.restype = ctypes.c_char_p
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    u32.in_dll(lib, "g_random_seed").value = SEED
    return Game(lib)


def snapshot(game):
    """SHA-256 of the saved game state and of the screen after a scenario."""
    scrub_free_groups(game.lib)
    s = state(game)
    fb = bytes((ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb"))
    return {"state": hashlib.sha256(repr(sorted(s.items())).encode()).hexdigest(), "screen": hashlib.sha256(fb).hexdigest()}


def check(name, got):
    """Compares the snapshots of a scenario with the golden file; DM_UPDATE_GOLDEN writes them."""
    golden = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        golden[name] = got
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(golden, indent=1, sort_keys=True) + "\n")
    assert got == golden[name]


def ticks(game, n):
    for _ in range(n):
        game.lib.gameloop_tick()


def hand_box(game, champion_index=0):
    box = Box()
    assert game.lib.ui_get_slot_box_zone(SLOT_HAND_ACTION, champion_index, ctypes.byref(box))
    return box


def zone_box(game, zone):
    box = Box()
    assert game.lib.ui_zone_box(zone, ctypes.byref(box))
    return box


def recruit(game):
    """One recruited champion; the leader hand is empty again."""
    recruit_and_hold_object(game)
    game.click_box(hand_box(game), viewport=False)  # the object goes back into the action hand
    assert game.value("g_leader_hand_object", u16) == 0xFFFF


def add_second_champion(game):
    """A copy of champion 0 in the next cell, without objects, as the second champion."""
    champions = game.champions
    ctypes.memmove(ctypes.byref(champions[1]), ctypes.byref(champions[0]), ctypes.sizeof(champions[0]))
    champions[1].name = b"SECOND"
    champions[1].cell = (champions[0].cell + 1) % 4
    for s in range(len(champions[1].slots)):
        champions[1].slots[s] = 0xFFFF
    champions[1].load = 0
    u16.in_dll(game.lib, "g_party_champion_count").value = 2


def place_party(game, m, x, y, d):
    game.lib.map_set_current_map_and_party_map(m)
    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), (x, y, d)):
        c16.in_dll(game.lib, name).value = v
    game.lib.gameloop_draw_view()


def new_object(game, thing_type, object_type, byte=3):
    thing = game.lib.map_get_unused_thing(thing_type)
    data = game.lib.dungeon_get_thing_data(thing)
    data[byte] = (data[byte] & 0x80) | object_type
    return thing


def set_mode(game, mode):
    c16.in_dll(game.lib, "g_game_mode").value = mode
    ctypes.c_bool.in_dll(game.lib, "g_advanced_features").value = mode != VANILLA


@needs_game
def test_fill_at_a_fountain(game):
    lib = game.lib
    recruit(game)
    place_party(game, *FOUNTAIN)
    assert game.value("g_facing_fountain")
    got = []
    flask = new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2)
    skin = new_object(game, THING_JUNK, JUNK_WATERSKIN)
    for thing in (flask, skin):
        lib.inventory_put_object_in_leader_hand(thing)
        game.click_box(game.boxes[5])
        got.append(snapshot(game))
        lib.inventory_get_object_removed_from_leader_hand()
    assert lib.dungeon_get_thing_data(flask)[2] & 0x7F != POTION_EMPTY_FLASK
    assert lib.dungeon_get_thing_data(skin)[2] >> 6 == 3
    check("fill", got)


@needs_game
@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_drink_at_a_fountain(game, mode):
    recruit(game)
    add_second_champion(game)
    set_mode(game, mode)
    place_party(game, *FOUNTAIN)
    game.champions[0].water = game.champions[1].water = 100
    game.click_box(game.boxes[5])
    assert (game.champions[0].water, game.champions[1].water) == (2048, 2048 if mode == ADVANCED else 100)
    check(f"drink {mode}", [snapshot(game)])


@needs_game
def test_bones_on_the_vi_altar(game):
    lib = game.lib
    recruit(game)
    add_second_champion(game)
    game.champions[1].current_health = 0
    place_party(game, *VI_ALTAR)
    assert game.value("g_facing_vi_altar") and game.value("g_facing_alcove")
    bones = new_object(game, THING_JUNK, JUNK_BONES)
    lib.dungeon_get_thing_data(bones)[2] = 1 << 6  # charges: champion 1
    lib.inventory_put_object_in_leader_hand(bones)
    game.click_box(game.boxes[5])
    got = [snapshot(game)]
    ticks(game, 30)
    assert game.champions[1].current_health > 0
    got.append(snapshot(game))
    check("vi altar", got)


@needs_game
def test_door_button(game):
    recruit(game)
    place_party(game, *DOOR_BUTTON)
    door = game.lib.map_get_square(DOOR_BUTTON[1], DOOR_BUTTON[2] + 1)
    game.click_box(game.boxes[5])
    got = [snapshot(game)]
    ticks(game, 20)
    assert game.lib.map_get_square(DOOR_BUTTON[1], DOOR_BUTTON[2] + 1) != door
    got.append(snapshot(game))
    check("door button", got)


@needs_game
@pytest.mark.parametrize("mana", [100, 2])
def test_runes(game, mana):
    recruit(game)
    caster = game.champions[game.value("g_magic_caster_champion_index")]
    caster.current_mana = mana
    got = []
    for zone in (0, 1, 2, None, 3):
        box = zone_box(game, ZONE_SPELL_DELETE_BOX if zone is None else ZONE_SPELL_SYMBOL_BOX_FIRST + zone)
        game.click_box(box, viewport=False)
        got.append(snapshot(game))
    check(f"runes {mana}", got)


@needs_game
def test_torch_burns(game):
    lib = game.lib
    recruit(game)
    torch = new_object(game, THING_WEAPON, WEAPON_TORCH)
    lib.weapon_set_lit(torch, True)
    lib.weapon_set_charges(torch, 3)
    game.champions[0].slots[SLOT_HAND_READY] = torch
    got = []
    for _ in range(4):
        ticks(game, 500)
        got.append(snapshot(game))
    assert lib.dungeon_get_thing_data(torch)[2] >> 2 & 15 == 0
    check("torch", got)


@needs_game
def test_turn_to_attacker(game):
    lib = game.lib
    recruit(game)
    add_second_champion(game)  # cell 1: the east side of the party that faces north
    m, x, y, d = ATTACKED
    place_party(game, m, x, y, d)
    assert lib.group_get_generated(CREATURE_SKELETON, 1, 0, 3, x + 1, y) == 0xFFFF  # a skeleton east of the party
    second = game.champions[1]
    assert game.until(lambda: second.direction != d)
    got = [snapshot(game)]
    ticks(game, 20)
    got.append(snapshot(game))
    check("turn to attacker", got)


@needs_game
def test_action_list(game):
    lib = game.lib
    recruit(game)
    champion = game.champions[0]
    count = u16.in_dll(lib, "g_action_count")
    levels = (ctypes.c_uint8 * 3).in_dll(lib, "g_action_list_minimum_skill_levels")
    indices = (ctypes.c_uint8 * 3).in_dll(lib, "g_action_list_indices")
    got = []
    for experience in (0, 1 << 20):
        for s in range(SKILL_COUNT):
            champion.skills[s].experience = experience
        for weapon, charges in WEAPONS:
            thing = new_object(game, THING_WEAPON, weapon)
            lib.weapon_set_charges(thing, charges)
            champion.slots[SLOT_HAND_ACTION] = thing
            game.click_box(zone_box(game, ZONE_ACTION_ICON_FIRST), viewport=False)
            assert game.value("g_acting_champion_ordinal") == 1
            names = [lib.gfx_action_name(indices[k]).decode() if indices[k] != 0xFF else "" for k in range(3)]
            got.append({"count": count.value, "levels": list(levels), "names": names, **snapshot(game)})
            lib.menus_clear_acting_champion()
            game.run()
    check("action list", got)


@needs_game
def test_freeze_life_and_movement_ticks(game):
    lib = game.lib
    recruit(game)
    party = Party.in_dll(lib, "g_party")
    party.freeze_life_ticks = 30
    c16.in_dll(lib, "g_disabled_movement_ticks").value = 40
    c16.in_dll(lib, "g_projectile_disabled_movement_ticks").value = 20
    ticks(game, 10)
    got = [snapshot(game)]
    ticks(game, 40)
    assert party.freeze_life_ticks == 0
    got.append(snapshot(game))
    check("ticks", got)
