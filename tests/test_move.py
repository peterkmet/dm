import ctypes
import shutil
import struct
import subprocess
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse
from test_game import Champion

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
GET_SQUARE, LEVEL_CHANGE, STAIRS_EXIT = 0x10A8AC, 0x10AA0C, 0x10AAD2
FIRST_THING, IS_SQUARE_OPEN = 0x10AC4E, 0x10BCE6
G_FIRST_THING_INDEX, G_MAP_DATA, G_MAP_WIDTH, G_MAP_HEIGHT = 0x101CC0, 0x101CC4, 0x101CC8, 0x101CCA
G_MAPS, G_HEADER, G_SQUARE_FIRST_THINGS, G_PARTY_MAP = 0x101CD0, 0x101CD4, 0x101CE6, 0x1001F0
STEP_EAST, STEP_NORTH, ARROW_FORWARD, ARROW_RIGHT = 0x101038, 0x101040, 0x103C56, 0x103C5E
BUF = 0x200000
FORWARD, RIGHT, BACKWARD, LEFT, TURN_LEFT, TURN_RIGHT = 3, 4, 5, 6, 1, 2

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")
c16, u8p = ctypes.c_int16, ctypes.POINTER(ctypes.c_uint8)


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    for name in ("get_square",):
        getattr(lib, name).restype = ctypes.c_uint8
    for name in ("get_square_first_thing", "dungeon_square_first_thing"):
        getattr(lib, name).restype = ctypes.c_uint16
    for name in ("get_location_after_level_change", "get_stairs_exit_direction", "is_square_open",
                 "dungeon_map_width", "dungeon_map_height", "dungeon_map_count", "move_get_move_result"):
        getattr(lib, name).restype = c16
    lib.get_location_after_level_change.argtypes = [c16, c16, ctypes.POINTER(c16), ctypes.POINTER(c16)]
    lib.dungeon_square.restype = ctypes.c_uint8
    lib.dungeon_get_thing_data.restype = u8p
    return lib


@pytest.fixture
def game(lib):
    if not (GAME / "KAOS.FTL").exists() or not (GAME / "Dungeon.DAT").exists():
        pytest.skip("missing game files")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode()) == 1
    assert lib.tables_init() == 0
    lib.game_new()
    lib.move_party_enter_dungeon()
    return lib


def var(lib, name):
    return c16.in_dll(lib, name)


def party(lib):
    return tuple(var(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"))


def place(lib, map_index, x, y, direction):
    for name, value in zip(("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"), (map_index, x, y, direction)):
        var(lib, name).value = value
    lib.move_party_enter_dungeon()


def recruit(lib):
    champion = (Champion * 4).in_dll(lib, "g_champions")[0]
    champion.current_health = champion.maximum_health = 100
    champion.current_stamina = champion.maximum_stamina = 1000
    champion.statistics[1][1] = 50
    for slot in range(30):
        champion.slots[slot] = 0xFFFF
    ctypes.c_uint16.in_dll(lib, "g_party_champion_count").value = 1
    var(lib, "g_leader_index").value = 0
    return champion


def tick(lib):
    """One game_loop tick: the timeline, then the game time."""
    lib.event_process_expired()
    ctypes.c_uint32.in_dll(lib, "g_game_time").value += 1


def kaos_words(lib, addr, n):
    lib.kaos_u16.restype = ctypes.c_uint16
    return [c16(lib.kaos_u16(addr + 2 * i)).value for i in range(n)]


def test_start_position(game):
    header = bytes(u8p.in_dll(game, "g_dungeon_header")[:44])
    start = struct.unpack(">H", header[8:10])[0]
    assert party(game) == (0, start & 31, start >> 5 & 31, start >> 10 & 3) == (0, 1, 3, 2)
    assert var(game, "g_current_map_index").value == 0
    assert var(game, "g_new_party_map_index").value == -1


@pytest.mark.parametrize("command", [LEFT, RIGHT])
def test_move_into_wall_stays(game, command):
    game.move_party(command)
    assert party(game) == (0, 1, 3, 2)
    assert var(game, "g_disabled_movement_ticks").value == 0


def test_move_along_corridor(game):
    east, north = kaos_words(game, STEP_EAST, 4), kaos_words(game, STEP_NORTH, 4)
    forward, right = kaos_words(game, ARROW_FORWARD, 4), kaos_words(game, ARROW_RIGHT, 4)
    for command in (FORWARD, FORWARD, BACKWARD, FORWARD, FORWARD):
        _, x, y, d = party(game)
        a = command - FORWARD
        x += forward[a] * east[d] + right[a] * east[(d + 1) & 3]
        y += forward[a] * north[d] + right[a] * north[(d + 1) & 3]
        game.move_party(command)
        assert party(game) == (0, x, y, d)
        assert var(game, "g_disabled_movement_ticks").value == 1
    assert party(game) == (0, 1, 6, 2)


def test_turning(game):
    champion = recruit(game)
    champion.cell = 1
    for command, direction, cell in ((TURN_RIGHT, 3, 2), (TURN_LEFT, 2, 1), (TURN_LEFT, 1, 0), (TURN_LEFT, 0, 3), (TURN_LEFT, 3, 2)):
        game.move_turn_party(command)
        assert party(game)[3] == direction and champion.cell == cell
    assert party(game)[1:3] == (1, 3)


def test_stairs_change_map(game):
    place(game, 0, 4, 15, 3)
    game.move_party(FORWARD)
    assert var(game, "g_new_party_map_index").value == 1
    game.move_process_new_party_map()
    assert party(game) == (1, 3, 1, 0)
    assert var(game, "g_new_party_map_index").value == -1 and var(game, "g_current_map_index").value == 1
    game.move_party(FORWARD)
    assert party(game) == (1, 3, 0, 0)


def test_floor_sensor_opens_door(game):
    recruit(game)
    place(game, 0, 7, 9, 3)
    door = lambda: game.dungeon_square(0, 5, 9) & 7
    assert game.dungeon_square(0, 5, 9) >> 5 == 4 and door() == 4
    game.move_party(FORWARD)
    assert party(game) == (0, 6, 9, 3)
    states = []
    for _ in range(5):
        tick(game)
        states.append(door())
    assert states == [4, 3, 2, 1, 0]


def test_door_event_toggles_door(game):
    door = lambda: game.dungeon_square(0, 5, 9) & 7
    game.sensor_add_event(10, 5, 9, 0, 2, 1)
    for _ in range(6):
        tick(game)
    assert door() == 0
    game.sensor_add_event(10, 5, 9, 0, 2, ctypes.c_uint32.in_dll(game, "g_game_time").value)
    for _ in range(5):
        tick(game)
    assert door() == 4


def test_teleporter_moves_party(game):
    place(game, 1, 29, 3, 1)
    game.move_party(FORWARD)
    assert party(game) == (1, 30, 1, 0)  # teleporter (30, 3): target (30, 1), absolute rotation 0
    assert var(game, "g_new_party_map_index").value == -1


def test_pit_drops_party(game):
    recruit(game)
    place(game, 1, 7, 9, 0)
    x, y = c16(7), c16(8)
    below = game.get_location_after_level_change(1, 1, ctypes.byref(x), ctypes.byref(y))
    assert below == 2
    game.move_party(FORWARD)
    assert var(game, "g_new_party_map_index").value == below
    assert party(game)[1:3] == (x.value, y.value)
    assert 0 < (c16 * 4).in_dll(game, "g_champion_pending_damage")[0] < 100
    game.move_process_new_party_map()
    assert party(game)[:3] == (2, x.value, y.value)


def test_turn_on_stairs_takes_stairs(game):
    place(game, 1, 3, 1, 0)
    game.move_turn_party(TURN_LEFT)
    game.move_process_new_party_map()
    assert party(game)[0] == 0 and game.dungeon_square(0, *party(game)[1:3]) >> 5 == 3


def emu_with_map(lib, m):
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
    header = bytes(u8p.in_dll(lib, "g_dungeon_header")[:44])
    maps_count, raw_size, square_things = header[4], struct.unpack(">H", header[2:4])[0], struct.unpack(">H", header[10:12])[0]
    maps = bytes(u8p.in_dll(lib, "g_dungeon_maps")[:16 * maps_count])
    raw = bytes(u8p.in_dll(lib, "g_dungeon_raw_map_data")[:raw_size])
    columns = c16.in_dll(lib, "g_dungeon_column_count").value
    column_index = bytes(u8p.in_dll(lib, "g_dungeon_column_first_thing_index")[:2 * columns])
    first_things = bytes(u8p.in_dll(lib, "g_square_first_things")[:2 * square_things])
    first_column = ctypes.POINTER(c16).in_dll(lib, "g_dungeon_map_first_column")[m]
    w, h = lib.dungeon_map_width(m), lib.dungeon_map_height(m)
    offset = struct.unpack(">H", maps[16 * m:16 * m + 2])[0]
    header_at, maps_at, raw_at, cols_at, index_at, things_at = BUF, BUF + 0x100, BUF + 0x1000, BUF + 0x20000, BUF + 0x21000, BUF + 0x30000
    emu.write(header_at, header)
    emu.write(maps_at, maps)
    emu.write(raw_at, raw)
    emu.write(cols_at, b"".join(struct.pack(">I", raw_at + offset + x * h) for x in range(w)))
    emu.write(index_at, column_index)
    emu.write(things_at, first_things)
    for addr, fmt, value in ((G_MAPS, ">I", maps_at), (G_HEADER, ">I", header_at), (G_MAP_DATA, ">I", cols_at),
                             (G_MAP_WIDTH, ">H", w), (G_MAP_HEIGHT, ">H", h), (G_FIRST_THING_INDEX, ">I", index_at + 2 * first_column),
                             (G_SQUARE_FIRST_THINGS, ">I", things_at), (G_PARTY_MAP, ">H", 0)):
        emu.write(addr, struct.pack(fmt, value))
    return emu


@pytest.mark.parametrize("m", [0, 1, 2])
def test_square_queries_match_original(game, m):
    emu = emu_with_map(game, m)
    game.set_current_map(m)
    w, h = game.dungeon_map_width(m), game.dungeon_map_height(m)
    for x in range(-2, w + 2):
        for y in range(-2, h + 2):
            args = [(2, x & 0xFFFF), (2, y & 0xFFFF)]
            square = game.get_square(x, y)
            assert square == emu.call(GET_SQUARE, args) & 0xFF, (x, y)
            assert game.is_square_open(x, y) == emu.call(IS_SQUARE_OPEN, args) & 0xFFFF
            if 0 <= x < w and 0 <= y < h:
                assert game.get_square_first_thing(x, y) == emu.call(FIRST_THING, args) & 0xFFFF
            if square >> 5 == 3:
                assert game.get_stairs_exit_direction(x, y) == emu.call(STAIRS_EXIT, args) & 0xFFFF
    for x in range(w):
        for y in range(h):
            for delta in (-1, 1):
                cx, cy = c16(x), c16(y)
                result = game.get_location_after_level_change(m, delta, ctypes.byref(cx), ctypes.byref(cy))
                emu.write(BUF + 0x40000, struct.pack(">hh", x, y))
                expected = c16(emu.call(LEVEL_CHANGE, [(2, m), (2, delta & 0xFFFF), (4, BUF + 0x40000), (4, BUF + 0x40002)]) & 0xFFFF).value
                assert result == expected
                if expected >= 0:
                    assert (cx.value, cy.value) == struct.unpack(">hh", emu.read(BUF + 0x40000, 4))


def test_link_and_unlink_keep_lists(game):
    """An object dropped on an empty square makes a new list. Removing it restores all square first things."""
    game.set_current_map(0)
    count = struct.unpack(">H", bytes(u8p.in_dll(game, "g_dungeon_header")[10:12]))[0]
    before = bytes(u8p.in_dll(game, "g_square_first_things")[:2 * count])
    columns = c16.in_dll(game, "g_dungeon_column_count").value
    index_before = bytes(u8p.in_dll(game, "g_dungeon_column_first_thing_index")[:2 * columns])
    assert game.get_square_first_thing(1, 5) == 0xFFFE
    junk = 10 << 10 | 0  # junk record 0
    saved = bytes(game.dungeon_get_thing_data(junk)[:4])
    game.link_thing_to_list(junk | 2 << 14, 0, 1, 5)
    assert game.get_square_first_thing(1, 5) == junk | 2 << 14
    assert game.dungeon_square(0, 1, 5) & 0x10
    game.unlink_thing_from_list(junk, 0, 1, 5)
    assert game.get_square_first_thing(1, 5) == 0xFFFE and not game.dungeon_square(0, 1, 5) & 0x10
    assert bytes(u8p.in_dll(game, "g_square_first_things")[:2 * count]) == before
    assert bytes(u8p.in_dll(game, "g_dungeon_column_first_thing_index")[:2 * columns]) == index_before
    ctypes.memmove(game.dungeon_get_thing_data(junk), saved, 4)
