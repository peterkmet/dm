import ctypes
import os
import shutil
import subprocess
from collections import deque
from pathlib import Path

import pytest

from test_champion import Champion
from conftest import global_type

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
STEP_EAST, STEP_NORTH = (0, 1, 0, -1), (-1, 0, 1, 0)
KEY_UP, KEY_DEL, KEY_HELP, KEY_F1, KEY_RETURN, KEY_ESCAPE = 0x4C00, 0x4600, 0x5F00, 0x5000, 0x4400, 0x4500
LEFT_PRESS, LEFT_RELEASE = 2, 4
VIEWPORT_ORIGIN = 0x10293A
ZONE_RESURRECT, ZONE_DROP_PARTY_CELL, ZONE_BACKPACK_FIRST_SLOT = 570, 740, 520
SLOT_HAND_ACTION, SLOT_BACKPACK_FIRST = 1, 13
SENSOR, CORRIDOR, CHAMPION_PORTRAIT = 3, 1, 127
ATTACK_RAW = 0
ZONE_SPELL_AREA, SPELL_SYMBOL_FIRST = 13, 0x60
c16, u16 = ctypes.c_int16, ctypes.c_uint16

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


@pytest.fixture(scope="module")
def game():
    subprocess.run(["make"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.gameloop_get_pointer.restype = ctypes.c_bool
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.map_get_next_thing.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    return Game(lib)


class Game:
    def __init__(self, lib):
        self.lib = lib
        self.boxes = (Box * 6).in_dll(lib, "g_dungeon_view_clickable_boxes")
        self.champions = (Champion * 4).in_dll(lib, "g_champions")
        self.origin = lib.kaos_s16(VIEWPORT_ORIGIN), lib.kaos_s16(VIEWPORT_ORIGIN + 2)

    def value(self, name, kind=c16):
        return global_type(name, kind).in_dll(self.lib, name).value

    def position(self):
        return [self.value(n) for n in ("g_party_map_x", "g_party_map_y", "g_party_direction")]

    def run(self, frames=13):
        for _ in range(frames):
            if self.lib.gameloop_run_frame():
                self.lib.gameloop_tick()

    def until(self, condition, limit=400):
        for _ in range(limit):
            if condition():
                return True
            self.run(1)
        return condition()

    def key(self, key):
        self.lib.command_process_key(key)

    def click(self, x, y):
        self.lib.command_process_click(x, y, LEFT_PRESS)
        self.lib.command_process_click(x, y, LEFT_RELEASE)
        self.run()

    def click_box(self, box, viewport=True):
        x, y = box.x + box.w // 2, box.y + box.h // 2
        if viewport:
            x, y = x + self.origin[0], y + self.origin[1]
        self.click(x, y)

    def click_zone(self, zone):
        box = Box()
        assert self.lib.layout_get_zone_box(zone, 10000, 10000, ctypes.byref(box))
        self.click_box(box)

    def face(self, direction):
        while self.position()[2] != direction:
            current = self.position()[2]
            self.key(KEY_HELP if (direction - current) & 3 == 1 else KEY_DEL)
            assert self.until(lambda: self.position()[2] != current)

    def walk(self, path):
        for (x0, y0), (x1, y1) in zip(path, path[1:]):
            self.face(next(d for d in range(4) if (x0 + STEP_EAST[d], y0 + STEP_NORTH[d]) == (x1, y1)))
            assert self.until(lambda: self.position()[:2] == [x0, y0])
            self.key(KEY_UP)
            assert self.until(lambda: self.position()[:2] == [x1, y1])

    def mirror_cell(self, x, y):
        thing = self.lib.map_get_square_first_thing(x, y)
        while thing not in (0xFFFE, 0xFFFF):
            data = self.lib.dungeon_get_thing_data(thing)
            if thing >> 10 & 15 == SENSOR and (data[2] << 8 | data[3]) & 0x7F == CHAMPION_PORTRAIT:
                return thing >> 14
            thing = self.lib.map_get_next_thing(thing)
        return None

    def path_to_mirror(self, skip=()):
        """Shortest corridor path to a square whose wall ahead holds a mirror facing it, and that direction."""
        start = tuple(self.position()[:2])
        previous, queue = {start: None}, deque([start])
        while queue:
            x, y = queue.popleft()
            for d in range(4):
                nx, ny = x + STEP_EAST[d], y + STEP_NORTH[d]
                if (nx, ny) not in skip and self.mirror_cell(nx, ny) == (d + 2) & 3:
                    path, square = [], (x, y)
                    while square:
                        path.append(square)
                        square = previous[square]
                    return path[::-1], d
                if self.lib.map_get_square(nx, ny) >> 5 == CORRIDOR and (nx, ny) not in previous:
                    previous[nx, ny] = (x, y)
                    queue.append((nx, ny))
        raise AssertionError("no mirror reachable")


def test_new_game_recruit_take_and_store(game):
    start = game.position()
    game.run(26)
    game.key(KEY_DEL)
    assert game.until(lambda: game.position()[2] == (start[2] + 3) & 3)
    game.key(KEY_HELP)
    assert game.until(lambda: game.position()[2] == start[2])

    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    wall = game.boxes[5]
    assert wall.x != -1
    game.click_box(wall)
    assert game.value("g_candidate_champion_ordinal") == 1
    assert game.value("g_inventory_champion_ordinal") == 1

    game.click_zone(ZONE_RESURRECT)
    assert game.value("g_candidate_champion_ordinal") == 0
    assert game.value("g_party_champion_count", u16) == 1
    assert game.value("g_leader_index") == 0
    assert game.value("g_inventory_champion_ordinal") == 0

    weapon = game.champions[0].slots[SLOT_HAND_ACTION]
    assert weapon != 0xFFFF
    box = Box()
    assert game.lib.ui_get_slot_box_zone(SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) == weapon
    assert game.champions[0].slots[SLOT_HAND_ACTION] == 0xFFFF

    game.click_zone(ZONE_DROP_PARTY_CELL)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF
    pile = next(i for i in range(4) if game.boxes[i].x != -1)
    game.click_box(game.boxes[pile])
    assert game.value("g_leader_hand_object", u16) & 0x3FFF == weapon & 0x3FFF
    game.click_box(box, viewport=False)  # the status box hand takes the weapon without the inventory
    assert game.champions[0].slots[SLOT_HAND_ACTION] & 0x3FFF == weapon & 0x3FFF
    assert game.value("g_leader_hand_object", u16) == 0xFFFF
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) & 0x3FFF == weapon & 0x3FFF

    game.key(KEY_F1)
    game.run()
    assert game.value("g_inventory_champion_ordinal") == 1
    game.click_zone(ZONE_BACKPACK_FIRST_SLOT)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF
    assert game.champions[0].slots[SLOT_BACKPACK_FIRST] & 0x3FFF == weapon & 0x3FFF


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
    assert game.value("g_party_champion_count", u16) == 2
    game.key(KEY_F1 + 0x100)
    game.run()
    assert game.value("g_inventory_champion_ordinal") == 2

    game.lib.champion_add_pending_damage_and_wounds(1, 10000, 0, ATTACK_RAW)
    game.run()
    assert game.champions[1].current_health == 0
    assert game.value("g_inventory_champion_ordinal") == 0
    game.click_zone(ZONE_BACKPACK_FIRST_SLOT)  # the dungeon view takes the click; no slot of champion -1

    box = Box()
    assert game.lib.ui_get_slot_box_zone(SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) != 0xFFFF
    game.click_zone(ZONE_DROP_PARTY_CELL)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF

    direction = game.position()[2]
    game.key(KEY_DEL)
    assert game.until(lambda: game.position()[2] == (direction + 3) & 3)


def test_game_runs_headless_with_auto_enter():
    subprocess.run(["make"], cwd=ROOT, check=True, capture_output=True)
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", DM_AUTO_ENTER="1")
    result = subprocess.run([str(ROOT / "build/dm"), "--frames", "100"], cwd=ROOT, env=env, capture_output=True, timeout=60)
    assert result.returncode == 0, result.stderr


def viewport_pixels(game):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb")
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
    game.key(KEY_F1)
    game.run()
    assert game.value("g_inventory_champion_ordinal") == 1
    inventory = viewport_pixels(game)

    game.key(KEY_ESCAPE)
    game.run(39)
    assert game.value("g_game_frozen") == 1
    frozen = viewport_pixels(game)
    assert len(set(frozen)) == 2  # black and the GAME FROZEN text, no slot icons on top

    for key in (KEY_ESCAPE, KEY_RETURN):
        game.key(key)
        game.run()
        if not game.value("g_game_frozen"):
            break
    assert game.value("g_game_frozen") == 0
    assert viewport_pixels(game) == inventory


def test_party_light_sets_the_darkness_of_view_and_rules(game):
    from test_champion import Party

    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    lib.automap_get_visible_depths.restype = c16
    party = Party.in_dll(lib, "g_party")
    darkness = c16.in_dll(lib, "g_dungeon_view_palette_index")
    pal = (u16 * 16).in_dll(lib, "palette")
    place = {n: c16.in_dll(lib, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")}
    start = {n: v.value for n, v in place.items()}

    def view(light):
        party.magical_light_amount = light
        lib.gameloop_draw_view()
        return darkness.value, list(pal), lib.automap_get_visible_depths(1, 14, 4, 3)

    try:
        for n, v in zip(place, (1, 14, 4, 3)):  # map 1 has darkness, a long corridor ahead
            place[n].value = v
        bright, dark = view(1000), view(-1000)
        assert bright[0] == 0 and dark[0] == 5
        assert bright[1] != dark[1]
        assert dark[2] < bright[2]
        sleeping = ctypes.c_bool.in_dll(lib, "g_party_is_sleeping")
        sleeping.value = 1  # no view is drawn, the creature rules still see the light of this tick
        party.magical_light_amount = 1000
        lib.gameloop_tick()
        assert darkness.value == 0
        sleeping.value = 0
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
    lib.map_get_unused_thing.restype = u16
    skin = lib.map_get_unused_thing(10)
    data = lib.dungeon_get_thing_data(skin)
    data[3], data[2] = junk, 1 << 6  # one charge
    lib.inventory_put_object_in_leader_hand(skin)
    game.key(KEY_F1)
    game.run()
    box = Box()
    assert lib.ui_zone_box(17, ctypes.byref(box))  # the leader hand object name
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    name = lambda: [fb[(box.y + y) * 320 + box.x + x] for y in range(box.h) for x in range(box.w)]
    before = name()
    game.click_zone(545)  # the mouth
    assert lib.dungeon_get_thing_data(skin)[2] >> 6 == 0
    assert name() != before
    game.key(KEY_F1)
    game.run()


def pointer(lib, x, y):
    """The pointer at (x, y) in the game: width, height, and hot spot (they differ by shape)."""
    px = (ctypes.c_uint8 * (32 * 32))()
    out = [ctypes.c_int() for _ in range(4)]
    assert lib.gameloop_get_pointer(x, y, 1, px, *[ctypes.byref(v) for v in out])
    return tuple(v.value for v in out)


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
    assert lib.ui_get_slot_box_zone(SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)  # the weapon goes into the leader hand
    hand = (box.x + box.w // 2, box.y + box.h // 2)
    viewport_object, arrow = pointer(lib, 100, 100), pointer(lib, 300, 100)
    assert viewport_object != arrow
    assert pointer(lib, *hand) == viewport_object
    game.key(KEY_F1)
    game.run()
    assert pointer(lib, *hand) == arrow
    game.key(KEY_F1)
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
    assert game.value("g_party_champion_count", u16) == 2 and game.value("g_magic_caster_champion_index") == 0
    game.champions[1].symbol_step, game.champions[1].symbols = 1, bytes([SPELL_SYMBOL_FIRST])


def spell_area(game):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb")
    box = Box()
    assert game.lib.ui_zone_box(ZONE_SPELL_AREA, ctypes.byref(box))
    return [fb[(box.y + y) * 320 + box.x + x] for y in range(box.h) for x in range(box.w)]


def test_a_party_that_dies_in_one_tick_shows_the_spell_area_of_the_last_caster(game):
    """The caster dies first: the spell area goes to the last champion as it was before its own death in the same tick."""
    lib = game.lib
    pending = (c16 * 4).in_dll(lib, "g_champion_pending_damage")
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
    assert game.value("g_party_dead") and frames[0] == expected
