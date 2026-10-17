# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes

import pytest

from dmtest.game import Bitmap, Box, Position
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

pytestmark = [needs_dm]
c16 = ctypes.c_int16
NORTH, EAST, SOUTH, WEST = range(4)
STEP = {NORTH: (0, -1), EAST: (1, 0), SOUTH: (0, 1), WEST: (-1, 0)}


@pytest.fixture
def game():
    lib = load_lib()
    lib.rec_square_byte.restype = ctypes.c_uint8
    lib.dungeon_load.restype = ctypes.c_bool
    lib.text_init.restype = ctypes.c_bool
    for name in ("dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = c16
    lib.explored_save.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    lib.explored_load.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    lib.explored_is_seen.restype = ctypes.c_bool
    lib.automap_draw.argtypes = [ctypes.POINTER(Bitmap), ctypes.POINTER(Box), c16]
    lib.explored_mark_visible.argtypes = [Position, c16]
    lib.explored_get_visible_depths.argtypes = [Position, c16]
    lib.explored_get_visible_depths.restype = c16
    assert lib.tables_load(str(GAME / "KAOS.FTL").encode())
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    lib.game_start_new()
    lib.move_party_enter_dungeon()
    lib.explored_reset()
    return lib


def party(lib):
    return tuple(c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"))


def ahead(x, y, d, depth, side=0):
    (fx, fy), (rx, ry) = STEP[d], STEP[(d + 1) % 4]
    return x + depth * fx + side * rx, y + depth * fy + side * ry


def mark_here(lib):
    m, x, y, d = party(lib)
    lib.explored_mark_visible(Position(m, x, y), d)


def seen_squares(lib, m):
    return {(x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m)) if lib.explored_is_seen(m, x, y)}


def test_side_squares_behind_walls_stay_unseen(game):
    game.explored_mark_visible(Position(1, 11, 10), WEST)
    assert not game.explored_is_seen(1, 10, 9)


def save(lib):
    buf, size = ctypes.POINTER(ctypes.c_uint8)(), ctypes.c_size_t()
    assert lib.explored_save(ctypes.byref(buf), ctypes.byref(size)) is True
    data = ctypes.string_at(buf, size.value)
    ctypes.CDLL(None).free(buf)
    return data


def test_save_load_round_trip(game):
    m = party(game)[0]
    mark_here(game)
    before, data = seen_squares(game, m), save(game)
    assert data[:5] == b"AMAP\x01"
    game.explored_reset()
    assert not seen_squares(game, m)
    for bad in (data[:-1], b"XMAP" + data[4:], data[:4] + b"\x02" + data[5:], data[:-1] + b"\x02"):
        assert game.explored_load(bad, len(bad)) is False
    assert game.explored_load(data, len(data)) is True
    assert seen_squares(game, m) == before


def test_darkness_shortens_the_visible_depth(game):
    lib = game
    palette = c16.in_dll(lib, "g_party_light_level")
    best = None
    for y in range(lib.dungeon_map_height(0)):  # a position with a long free view ahead
        for x in range(lib.dungeon_map_width(0)):
            for d in range(4):
                palette.value = 0
                if lib.explored_get_visible_depths(Position(0, x, y), d) == 4:
                    best = (x, y, d)
    assert best
    for darkness, depths in ((0, 4), (3, 4), (4, 3), (5, 2)):
        palette.value = darkness
        assert lib.explored_get_visible_depths(Position(0, *best[:2]), best[2]) == depths
    palette.value = 0
