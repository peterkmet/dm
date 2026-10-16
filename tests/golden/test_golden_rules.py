"""Golden snapshots of the game rules that run in ui: each scenario gives the same game state and screen."""

import ctypes
import hashlib
import json
import os
from pathlib import Path

import pytest

from dmtest import constants as C
from dmtest.game import Party, scrub_free_groups, state
from dmtest.golden import add_second_champion, golden_game, recruit, ticks, zone_box
from dmtest.markers import GAME

c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

GOLDEN = Path(__file__).with_name("rules.json")
SEED = 0x1234ABCD
VANILLA, ADVANCED = 0, 1
FOUNTAIN = 1, 0, 21, 2       # map, x, y, direction: facing a fountain on level 2
VI_ALTAR = 0, 4, 17, 2       # facing the vi altar of level 1
DOOR_BUTTON = 0, 1, 13, 2    # facing the first door with a button
ATTACKED = 5, 0, 0, 0        # a map that allows skeletons, with a free corridor square east of the party
# (weapon type, charges): weapon 4 has an action that needs charges, once without and once with them
WEAPONS = (C.WEAPON_TORCH, 0), (8, 0), (9, 0), (10, 0), (4, 0), (4, 3)

needs_game = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists(), reason="game files missing")


@pytest.fixture
def game():
    return golden_game()


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
    flask = new_object(game, C.THING_POTION, C.POTION_EMPTY_FLASK, byte=2)
    skin = new_object(game, C.THING_JUNK, C.JUNK_WATERSKIN)
    for thing in (flask, skin):
        lib.inventory_put_object_in_leader_hand(thing)
        game.click_box(game.boxes[5])
        got.append(snapshot(game))
        lib.inventory_get_object_removed_from_leader_hand()
    assert lib.dungeon_get_thing_data(flask)[2] & 0x7F != C.POTION_EMPTY_FLASK
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
    bones = new_object(game, C.THING_JUNK, C.JUNK_BONES)
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
        box = zone_box(game, C.ZONE_SPELL_DELETE_BOX if zone is None else C.ZONE_SPELL_SYMBOL_BOX_FIRST + zone)
        game.click_box(box, viewport=False)
        got.append(snapshot(game))
    check(f"runes {mana}", got)


@needs_game
def test_torch_burns(game):
    lib = game.lib
    recruit(game)
    torch = new_object(game, C.THING_WEAPON, C.WEAPON_TORCH)
    lib.weapon_set_lit(torch, True)
    lib.weapon_set_charges(torch, 3)
    game.champions[0].slots[C.SLOT_HAND_READY] = torch
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
    assert lib.group_get_generated(C.CREATURE_SKELETON, 1, 0, 3, x + 1, y) == 0xFFFF  # a skeleton east of the party
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
        for s in range(C.SKILL_COUNT):
            champion.skills[s].experience = experience
        for weapon, charges in WEAPONS:
            thing = new_object(game, C.THING_WEAPON, weapon)
            lib.weapon_set_charges(thing, charges)
            champion.slots[C.SLOT_HAND_ACTION] = thing
            game.click_box(zone_box(game, C.ZONE_ACTION_ICON_BOX_FIRST), viewport=False)  # + champion index: the action hand icon of the action area
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
END, NONE = 0xFFFE, 0xFFFF
THING_STRIP_CELL = 0x3FFF  # a thing without its cell: a grabbed object keeps the cell of its pile
VIEW_CELL_FRONT_FIRST, CLICK_BOX_ALCOVE, CLICK_BOX_WALL = 2, 4, 5
STEP = ((0, -1), (1, 0), (0, 1), (-1, 0))  # (dx, dy) of each direction: north, east, south, west
MAP_COUNT = 14
VISIBILITY_MAPS = (0, 1, 2, 3)
LIGHTS = (0, 4, 5)  # g_dungeon_view_palette_index: bright, one row less, two rows less


def square_type(lib, x, y):
    return lib.map_get_square(x, y) >> 5


def is_free(lib, x, y):
    """A corridor square with no thing and no group."""
    return square_type(lib, x, y) == CORRIDOR and lib.map_get_square_first_thing(x, y) == END


def find_spot(game, maps, ahead_ok, directions=range(4)):
    """The first party place (map, x, y, direction) in scan order on a free corridor whose square ahead passes ahead_ok."""
    lib = game.lib
    for m in maps:
        lib.map_set_current_map_and_party_map(m)
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        for y in range(height):
            for x in range(width):
                for d in directions:
                    ax, ay = x + STEP[d][0], y + STEP[d][1]
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
    flask = new_object(game, C.THING_POTION, C.POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    got = []
    for cell in (0, VIEW_CELL_FRONT_FIRST):  # a cell of the party square, then a cell of the square ahead
        game.click_zone(C.ZONE_DROP_CELL_FIRST + cell)
        assert game.value("g_leader_hand_object", u16) == NONE
        got.append(snapshot(game))
        game.click_box(game.boxes[cell])
        assert game.value("g_leader_hand_object", u16) & THING_STRIP_CELL == flask & THING_STRIP_CELL
        got.append(snapshot(game))
    check("grab and drop", got)


@needs_game
def test_grab_under_a_creature(game):
    lib = game.lib
    recruit(game)
    m, x, y, d = find_spot(game, [ATTACKED[0]], lambda x, y: is_free(lib, x, y))
    place_party(game, m, x, y, d)
    fx, fy = x + STEP[d][0], y + STEP[d][1]
    piles = {}
    for cell in (VIEW_CELL_FRONT_FIRST, VIEW_CELL_FRONT_FIRST + 1):
        piles[cell] = new_object(game, C.THING_POTION, C.POTION_EMPTY_FLASK, byte=2)
        hold(game, piles[cell])
        game.click_zone(C.ZONE_DROP_CELL_FIRST + cell)
        assert game.value("g_leader_hand_object", u16) == NONE
    lib.map_get_square_group.restype = u16
    lib.group_get_generated(C.CREATURE_SKELETON, 1, 0, 3, fx, fy)
    group = lib.map_get_square_group(fx, fy)
    lib.group_set_cells(group, (VIEW_CELL_FRONT_FIRST + d) % 4, m)  # the skeleton stands in view cell 2 only
    got = []
    # the creature protects its own cell only; a levitating creature protects nothing
    for cell, creature, grabbed in ((VIEW_CELL_FRONT_FIRST, C.CREATURE_SKELETON, False),
                                    (VIEW_CELL_FRONT_FIRST + 1, C.CREATURE_SKELETON, True),
                                    (VIEW_CELL_FRONT_FIRST, C.CREATURE_WIZARD_EYE, True)):  # the wizard eye levitates
        lib.dungeon_get_thing_data(group)[4] = creature
        game.click_box(game.boxes[cell])
        hand = game.value("g_leader_hand_object", u16)
        assert (hand & THING_STRIP_CELL == piles[cell] & THING_STRIP_CELL) if grabbed else hand == NONE
        got.append({"hand": hand, **snapshot(game)})
        lib.inventory_get_object_removed_from_leader_hand()
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
    flask = new_object(game, C.THING_POTION, C.POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    game.click_box(game.boxes[CLICK_BOX_WALL])
    assert game.value("g_leader_hand_object", u16) == NONE
    got = [snapshot(game)]
    game.click_box(game.boxes[CLICK_BOX_ALCOVE])
    assert game.value("g_leader_hand_object", u16) & THING_STRIP_CELL == flask & THING_STRIP_CELL
    got.append(snapshot(game))
    check("alcove", got)


@needs_game
def test_imaginary_wall(game):
    lib = game.lib
    recruit(game)

    def imaginary(x, y):
        square = lib.map_get_square(x, y)
        return square >> 5 == FAKE_WALL and square & C.FAKE_WALL_IMAGINARY and not square & C.FAKE_WALL_OPEN

    place_party(game, *find_spot(game, range(MAP_COUNT), imaginary))
    game.click_zone(C.ZONE_THROW_LEFT)  # an empty hand goes into the imaginary wall
    got = [snapshot(game)]
    hold(game, new_object(game, C.THING_POTION, C.POTION_EMPTY_FLASK, byte=2))
    game.click_zone(C.ZONE_DROP_CELL_FIRST + VIEW_CELL_FRONT_FIRST)  # an object falls through the imaginary wall
    assert game.value("g_leader_hand_object", u16) == NONE
    got.append(snapshot(game))
    check("imaginary wall", got)


@needs_game
def test_movement_blocked(game):
    lib = game.lib
    recruit(game)
    spot = find_spot(game, [0], lambda x, y: is_free(lib, x, y), directions=(1, 3))  # east or west: not direction 0
    blockers = ("g_disabled_movement_ticks", "g_projectile_disabled_movement_ticks")
    got = []
    # (blocker, projectile direction relative to the party, blocked): a projectile blocks only its own direction
    for blocker, turn, blocked in ((blockers[0], 0, True), (blockers[1], 0, True), (blockers[1], 2, False)):
        place_party(game, *spot)
        c16.in_dll(lib, blocker).value = 3
        c16.in_dll(lib, "g_last_projectile_disabled_movement_direction").value = (spot[3] + turn) % 4
        game.key(C.KEY_UP)
        game.run(1)
        assert (game.position()[:2] == list(spot[1:3])) == blocked
        got.append({"position": game.position(), **snapshot(game)})
        assert game.until(lambda: game.position()[:2] != list(spot[1:3]))
        got.append({"position": game.position(), **snapshot(game)})
        for name in blockers:
            c16.in_dll(lib, name).value = 0
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
        champion.attributes |= C.CHAMPION_ATTR_LOAD
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
