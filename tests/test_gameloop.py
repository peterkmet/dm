import ctypes
import os
import shutil
import subprocess
from collections import deque
from pathlib import Path

import pytest

from test_champion import Champion

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
c16, u16 = ctypes.c_int16, ctypes.c_uint16

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


@pytest.fixture(scope="module")
def game():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = u16
    lib.get_next_thing.restype = u16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_new_game(str(GAME).encode())
    return Game(lib)


class Game:
    def __init__(self, lib):
        self.lib = lib
        self.boxes = (Box * 6).in_dll(lib, "g_dungeon_view_clickable_boxes")
        self.champions = (Champion * 4).in_dll(lib, "g_champions")
        self.origin = lib.kaos_s16(VIEWPORT_ORIGIN), lib.kaos_s16(VIEWPORT_ORIGIN + 2)

    def value(self, name, kind=c16):
        return kind.in_dll(self.lib, name).value

    def position(self):
        return [self.value(n) for n in ("g_party_map_x", "g_party_map_y", "g_party_direction")]

    def run(self, frames=13):
        for _ in range(frames):
            if self.lib.gameloop_frame():
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
        thing = self.lib.get_square_first_thing(x, y)
        while thing not in (0xFFFE, 0xFFFF):
            data = self.lib.get_thing_data(thing)
            if thing >> 10 & 15 == SENSOR and (data[2] << 8 | data[3]) & 0x7F == CHAMPION_PORTRAIT:
                return thing >> 14
            thing = self.lib.get_next_thing(thing)
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
                if self.lib.get_square(nx, ny) >> 5 == CORRIDOR and (nx, ny) not in previous:
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
    assert game.lib.ui_slot_box_zone(SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) == weapon
    assert game.champions[0].slots[SLOT_HAND_ACTION] == 0xFFFF

    game.click_zone(ZONE_DROP_PARTY_CELL)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF
    pile = next(i for i in range(4) if game.boxes[i].x != -1)
    game.click_box(game.boxes[pile])
    assert game.value("g_leader_hand_object", u16) & 0x3FFF == weapon & 0x3FFF

    game.key(KEY_F1)
    game.run()
    assert game.value("g_inventory_champion_ordinal") == 1
    game.click_zone(ZONE_BACKPACK_FIRST_SLOT)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF
    assert game.champions[0].slots[SLOT_BACKPACK_FIRST] & 0x3FFF == weapon & 0x3FFF


def test_champion_dies_with_inventory_open(game):
    assert game.lib.gameloop_new_game(str(GAME).encode())
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
    assert game.lib.ui_slot_box_zone(SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) != 0xFFFF
    game.click_zone(ZONE_DROP_PARTY_CELL)
    assert game.value("g_leader_hand_object", u16) == 0xFFFF

    direction = game.position()[2]
    game.key(KEY_DEL)
    assert game.until(lambda: game.position()[2] == (direction + 3) & 3)


def test_game_runs_headless_with_auto_enter():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", DM_AUTO_ENTER="1")
    result = subprocess.run([str(ROOT / "build/dm"), "--frames", "100"], cwd=ROOT, env=env, capture_output=True, timeout=60)
    assert result.returncode == 0, result.stderr


def viewport_pixels(game):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb")
    x0, y0 = game.origin
    return [fb[(y0 + y) * 320 + x0 + x] for y in range(136) for x in range(224)]


def test_freeze_covers_open_inventory(game):
    assert game.lib.gameloop_new_game(str(GAME).encode())
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
