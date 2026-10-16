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


# Scenarios of the rules that leave ui (PRD R8.3): each one pins a rule before it moves into the engine.
CORRIDOR, FAKE_WALL, WALL = 1, 6, 0
FAKE_WALL_IMAGINARY, FAKE_WALL_OPEN = 1, 4
END, NONE = 0xFFFE, 0xFFFF
THING_INDEX = 0x3FFF  # a thing without its cell: a grabbed object keeps the cell of its pile
VIEW_CELL_FRONT_FIRST, CLICK_BOX_ALCOVE, CLICK_BOX_WALL = 2, 4, 5
ZONE_DROP_CELL_FIRST, ZONE_THROW_LEFT = 740, 745
KEY_UP = 0x4C00
CHAMPION_ATTR_LOAD = 0x0200
STEP = ((0, -1), (1, 0), (0, 1), (-1, 0))  # (dx, dy) of each direction: north, east, south, west
MAP_COUNT = 14
VISIBILITY_MAPS = (0, 1, 2, 3)
LIGHTS = (0, 4, 5)  # g_dungeon_view_palette_index: bright, one row less, two rows less


def square_type(lib, x, y):
    return lib.map_get_square(x, y) >> 5


def is_free(lib, x, y):
    """A corridor square with no thing and no group."""
    return square_type(lib, x, y) == CORRIDOR and lib.map_get_square_first_thing(x, y) == END


def find_spot(game, maps, ahead_ok):
    """The first party place (map, x, y, direction) in scan order on a free corridor whose square ahead passes ahead_ok."""
    lib = game.lib
    for m in maps:
        lib.map_set_current_map_and_party_map(m)
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        for y in range(height):
            for x in range(width):
                for d, (dx, dy) in enumerate(STEP):
                    ax, ay = x + dx, y + dy
                    if is_free(lib, x, y) and 0 <= ax < width and 0 <= ay < height and ahead_ok(ax, ay):
                        return m, x, y, d
    raise AssertionError("no spot")


def hold(game, thing):
    game.lib.inventory_put_object_in_leader_hand(thing)
    assert game.value("g_leader_hand_object", u16) == thing


@needs_game
def test_grab_and_drop(game):
    lib = game.lib
    recruit(game)
    place_party(game, *find_spot(game, [0], lambda x, y: is_free(lib, x, y)))
    flask = new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    got = []
    for cell in (0, VIEW_CELL_FRONT_FIRST):  # a cell of the party square, then a cell of the square ahead
        game.click_zone(ZONE_DROP_CELL_FIRST + cell)
        assert game.value("g_leader_hand_object", u16) == NONE
        got.append(snapshot(game))
        game.click_box(game.boxes[cell])
        assert game.value("g_leader_hand_object", u16) & THING_INDEX == flask & THING_INDEX
        got.append(snapshot(game))
    check("grab and drop", got)


@needs_game
def test_grab_under_a_creature(game):
    lib = game.lib
    recruit(game)
    m, x, y, d = find_spot(game, [ATTACKED[0]], lambda x, y: is_free(lib, x, y))
    place_party(game, m, x, y, d)
    hold(game, new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2))
    game.click_zone(ZONE_DROP_CELL_FIRST + VIEW_CELL_FRONT_FIRST)
    lib.group_get_generated(CREATURE_SKELETON, 1, 0, 3, x + STEP[d][0], y + STEP[d][1])
    got = []
    for cell in (VIEW_CELL_FRONT_FIRST, VIEW_CELL_FRONT_FIRST + 1):  # a creature in the cell protects the pile
        game.click_box(game.boxes[cell])
        got.append({"hand": game.value("g_leader_hand_object", u16), **snapshot(game)})
    check("grab under a creature", got)


@needs_game
def test_alcove(game):
    lib = game.lib
    recruit(game)
    spot = None
    for m in range(MAP_COUNT):  # the only alcove of map 0 is the vi altar
        lib.map_set_current_map_and_party_map(m)
        for x, y, d in [(x, y, d) for y in range(game.value("g_current_map_height"))
                        for x in range(game.value("g_current_map_width")) for d in range(4)]:
            ax, ay = x + STEP[d][0], y + STEP[d][1]
            if not is_free(lib, x, y) or square_type(lib, ax, ay) != WALL:
                continue
            place_party(game, m, x, y, d)
            if game.value("g_facing_alcove") and not game.value("g_facing_vi_altar"):
                spot = m, x, y, d
                break
        if spot:
            break
    assert spot, "no alcove"
    flask = new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    game.click_box(game.boxes[CLICK_BOX_WALL])
    assert game.value("g_leader_hand_object", u16) == NONE
    got = [snapshot(game)]
    game.click_box(game.boxes[CLICK_BOX_ALCOVE])
    assert game.value("g_leader_hand_object", u16) & THING_INDEX == flask & THING_INDEX
    got.append(snapshot(game))
    check("alcove", got)


@needs_game
def test_imaginary_wall(game):
    lib = game.lib
    recruit(game)

    def imaginary(x, y):
        square = lib.map_get_square(x, y)
        return square >> 5 == FAKE_WALL and square & FAKE_WALL_IMAGINARY and not square & FAKE_WALL_OPEN

    place_party(game, *find_spot(game, range(MAP_COUNT), imaginary))
    game.click_zone(ZONE_THROW_LEFT)  # an empty hand goes into the imaginary wall
    got = [snapshot(game)]
    hold(game, new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2))
    game.click_zone(ZONE_DROP_CELL_FIRST + VIEW_CELL_FRONT_FIRST)  # an object falls through the imaginary wall
    assert game.value("g_leader_hand_object", u16) == NONE
    got.append(snapshot(game))
    check("imaginary wall", got)


@needs_game
def test_movement_blocked(game):
    lib = game.lib
    recruit(game)
    m, x, y, d = find_spot(game, [0], lambda x, y: is_free(lib, x, y))
    place_party(game, m, x, y, d)
    got = []
    for blocker in ("g_disabled_movement_ticks", "g_projectile_disabled_movement_ticks"):
        start = game.position()
        c16.in_dll(lib, blocker).value = 3
        c16.in_dll(lib, "g_last_projectile_disabled_movement_direction").value = start[2]
        game.key(KEY_UP)
        game.run(1)
        got.append({"position": game.position(), **snapshot(game)})
        assert game.until(lambda: game.position()[:2] != start[:2])
        got.append({"position": game.position(), **snapshot(game)})
        game.face((start[2] + 2) % 4)
    check("movement blocked", got)


@needs_game
def test_fuse_sequence(game):
    lib = game.lib
    recruit(game)
    lib.map_get_square_group.restype = u16
    spot = None
    for level in range(1, MAP_COUNT):
        lib.group_remove_all_active_groups()
        lib.map_set_current_map_and_party_map(level)
        lib.group_add_all_active_groups()
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        groups = {(x, y) for x in range(width) for y in range(height) if lib.map_get_square_group(x, y) != END}
        spot = next(((x - STEP[d][0], y - STEP[d][1], d, (x, y)) for x, y in sorted(groups) for d in range(4)
                     if 0 <= x - STEP[d][0] < width and 0 <= y - STEP[d][1] < height
                     and square_type(lib, x - STEP[d][0], y - STEP[d][1]) == CORRIDOR
                     and (x - STEP[d][0], y - STEP[d][1]) not in groups), None)
        if spot:
            break
    assert spot, "no group next to a corridor"
    px, py, direction, lord_square = spot
    for name, v in (("g_party_map_x", px), ("g_party_map_y", py), ("g_party_direction", direction)):
        c16.in_dll(lib, name).value = v
    lib.projectile_fluxcage_action(*lord_square)  # the sequence removes the fluxcage of the lord square
    lib.startend_run_fuse_sequence()
    assert game.value("g_game_won")
    check("fuse", [snapshot(game)])


@needs_game
def test_load_colors(game):
    lib = game.lib
    recruit(game)
    lib.champion_get_maximum_load.restype = u16
    champion = game.champions[0]
    maximum = lib.champion_get_maximum_load(ctypes.byref(champion))
    lib.command_toggle_inventory(0)
    game.run()
    got = []
    for load in (maximum // 2, maximum * 7 // 8, maximum + 1):  # light gray, yellow, red
        champion.load = load
        champion.attributes |= CHAMPION_ATTR_LOAD
        lib.ui_draw_champion_state(0)
        got.append(snapshot(game))
    check("load", got)


@needs_game
def test_visibility(game):
    lib = game.lib
    for name in ("dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = c16
    lib.dungeon_get_square.restype = ctypes.c_uint8
    lib.explored_save.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    palette = c16.in_dll(lib, "g_dungeon_view_palette_index")
    digest = hashlib.sha256()
    for m in VISIBILITY_MAPS:
        for y in range(lib.dungeon_map_height(m)):
            for x in range(lib.dungeon_map_width(m)):
                if lib.dungeon_get_square(m, x, y) >> 5 != CORRIDOR:
                    continue
                for d in range(4):
                    for light in LIGHTS:
                        palette.value = light
                        lib.explored_reset()
                        lib.automap_mark_seen(m, x, y, d)
                        buf, size = ctypes.c_void_p(), ctypes.c_size_t()
                        assert lib.explored_save(ctypes.byref(buf), ctypes.byref(size)) == 0
                        digest.update(ctypes.string_at(buf, size.value))
                        lib.free(buf)
    check("visibility", [digest.hexdigest()])
