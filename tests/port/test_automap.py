# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import os
import struct
import zlib
from pathlib import Path

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

PNG_DIR = os.environ.get("DM_TEST_PNG_DIR")  # optional: a folder for PNG renders of the screen

pytestmark = [needs_dm]
c16 = ctypes.c_int16
NORTH, EAST, SOUTH, WEST = range(4)
STEP = {NORTH: (0, -1), EAST: (1, 0), SOUTH: (0, 1), WEST: (-1, 0)}
FORWARD, TURN_LEFT = 3, 1
CELL, BLACK, YELLOW, FILLER, DARKEST_GRAY, LIGHT_GRAY = 7, 0, 11, 0xEE, 12, 13
RULER_W, RULER_H = 12, 7
WALL, DOOR, FAKE_WALL = 0, 4, 6


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


class Bitmap(ctypes.Structure):
    _fields_ = [("w", ctypes.c_uint16), ("h", ctypes.c_uint16), ("px", ctypes.POINTER(ctypes.c_uint8))]


VIEWPORT = Box(0, 33, 224, 136)


@pytest.fixture
def game():
    lib = load_lib()
    lib.rec_square_byte.restype = ctypes.c_uint8
    lib.dungeon_load.restype = ctypes.c_bool
    for name in ("dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = c16
    lib.explored_save.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    lib.explored_load.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    lib.explored_is_seen.restype = ctypes.c_bool
    lib.automap_draw.argtypes = [ctypes.POINTER(Bitmap), Box, c16]
    assert lib.tables_load(str(GAME / "KAOS.FTL").encode())
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    lib.game_start_new()
    lib.move_party_enter_dungeon()
    lib.explored_reset()
    return lib


def party(lib):
    return tuple(c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"))


def on_map(lib, m, x, y):
    return 0 <= x < lib.dungeon_map_width(m) and 0 <= y < lib.dungeon_map_height(m)


def blocks(lib, m, x, y):
    if not on_map(lib, m, x, y):
        return True
    square = lib.rec_square_byte(m, x, y)
    kind = square >> 5
    return kind == WALL or (kind == FAKE_WALL and not square & C.FAKE_WALL_OPEN) or (kind == DOOR and square & 7 == C.DOOR_CLOSED)


def ahead(x, y, d, depth, side=0):
    (fx, fy), (rx, ry) = STEP[d], STEP[(d + 1) % 4]
    return x + depth * fx + side * rx, y + depth * fy + side * ry


def mark_here(lib):
    m, x, y, d = party(lib)
    lib.automap_mark_seen(m, x, y, d)


def seen_squares(lib, m):
    return {(x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m)) if lib.explored_is_seen(m, x, y)}


def test_marks_start_and_squares_ahead(game):
    m, x, y, d = party(game)
    assert not game.explored_is_seen(m, x, y)
    mark_here(game)
    assert game.explored_is_seen(m, x, y)
    assert game.explored_is_seen(m, *ahead(x, y, d, 1))
    for side in (-1, 1):
        if on_map(game, m, *ahead(x, y, d, 0, side)):
            assert game.explored_is_seen(m, *ahead(x, y, d, 0, side))


def test_walls_stop_visibility(game):
    m, x, y, _ = party(game)
    stopped = 0
    for d in range(4):
        game.explored_reset()
        game.automap_mark_seen(m, x, y, d)
        expected = set()
        for depth in range(4):
            expected |= {ahead(x, y, d, depth, side) for side in (-1, 0, 1) if on_map(game, m, *ahead(x, y, d, depth, side))
                         and (side == 0 or depth == 0 or not blocks(game, m, *ahead(x, y, d, depth))
                              or not blocks(game, m, *ahead(x, y, d, depth - 1, side)))}
            if depth and blocks(game, m, *ahead(x, y, d, depth)):
                stopped += depth < 3
                break
        assert seen_squares(game, m) == expected
    assert stopped


def test_side_squares_behind_walls_stay_unseen(game):
    game.automap_mark_seen(1, 11, 10, WEST)
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


def draw(lib, w, h, area):
    px = (ctypes.c_uint8 * (w * h))(*([FILLER] * (w * h)))
    lib.automap_draw(ctypes.byref(Bitmap(w, h, px)), area, party(lib)[0])
    return px


def test_draw_stays_in_area_with_party_arrow_at_center(game):
    for d in range(4):
        c16.in_dll(game, "g_party_direction").value = d
        mark_here(game)
        px = draw(game, 320, 200, VIEWPORT)
        for y in range(200):
            for x in range(320):
                inside = VIEWPORT.x <= x < VIEWPORT.x + VIEWPORT.w and VIEWPORT.y <= y < VIEWPORT.y + VIEWPORT.h
                assert (px[y * 320 + x] != FILLER) == inside
        left, top = VIEWPORT.x + RULER_W + (VIEWPORT.w - RULER_W - CELL) // 2, VIEWPORT.y + RULER_H + (VIEWPORT.h - RULER_H - CELL) // 2
        assert px[(top + CELL // 2) * 320 + left + CELL // 2] == YELLOW


def test_rulers_number_the_squares_and_a_frame_ends_the_map(game):
    white = 15
    assert game.gfx_open(str(GAME / "Graphics.DAT").encode()) and game.text_init() == 0  # the font of the numbers
    mark_here(game)
    game.automap_scroll(party(game)[0], -64, -64)  # the top-left corner of the map at the center
    try:
        px = draw(game, VIEWPORT.w, VIEWPORT.h, Box(0, 0, VIEWPORT.w, VIEWPORT.h))
    finally:
        game.automap_scroll_reset()
    rows = [px[y * VIEWPORT.w:(y + 1) * VIEWPORT.w] for y in range(VIEWPORT.h)]
    assert LIGHT_GRAY in b"".join(bytes(r[RULER_W:]) for r in rows[:RULER_H])  # column numbers
    assert LIGHT_GRAY in b"".join(bytes(r[:RULER_W]) for r in rows[RULER_H:])  # row numbers
    left, top = RULER_W + (VIEWPORT.w - RULER_W - CELL) // 2, RULER_H + (VIEWPORT.h - RULER_H - CELL) // 2
    assert rows[top - 1][left + CELL] == white and rows[top + CELL][left - 1] == white
    assert rows[top - 2][left + CELL] == BLACK  # outside the map


def test_floors_are_light_and_walls_dark(game):
    m, x, y, d = party(game)
    mark_here(game)
    px = draw(game, VIEWPORT.w, VIEWPORT.h, Box(0, 0, VIEWPORT.w, VIEWPORT.h))
    left, top = RULER_W + (VIEWPORT.w - RULER_W - CELL) // 2 - x * CELL, RULER_H + (VIEWPORT.h - RULER_H - CELL) // 2 - y * CELL
    colors = {}
    for sx, sy in seen_squares(game, m) - {(x, y)}:
        kind = game.rec_square_type(game.rec_square_byte(m, sx, sy))
        if kind in (WALL, 1):  # wall, floor
            colors.setdefault(kind, set()).add(px[(top + sy * CELL) * VIEWPORT.w + left + sx * CELL])
    assert colors == {WALL: {DARKEST_GRAY}, 1: {LIGHT_GRAY}}


def test_a_pressure_pad_is_a_gray_square_in_the_floor(game):
    gray = 2
    for name, value in (("g_party_map_index", 1), ("g_party_map_x", 14), ("g_party_map_y", 1), ("g_party_direction", 1)):
        c16.in_dll(game, name).value = value
    game.map_set_current_map_and_party_map(1)
    game.view_load_map(1)
    mark_here(game)  # facing east into the room of nine pads of map 1
    px = draw(game, VIEWPORT.w, VIEWPORT.h, Box(0, 0, VIEWPORT.w, VIEWPORT.h))
    left, top = RULER_W + (VIEWPORT.w - RULER_W - CELL) // 2 - 14 * CELL, RULER_H + (VIEWPORT.h - RULER_H - CELL) // 2 - 1 * CELL
    pixel = lambda x, y, dx, dy: px[(top + y * CELL + dy) * VIEWPORT.w + left + x * CELL + dx]
    assert pixel(15, 1, CELL // 2, CELL // 2) == gray and pixel(15, 1, 0, 0) == LIGHT_GRAY
    assert pixel(14, 2, CELL // 2, CELL // 2) != gray  # a plain floor


def png(path, rgb_rows):
    h, w = len(rgb_rows), len(rgb_rows[0]) // 3
    raw = b"".join(b"\0" + bytes(row) for row in rgb_rows)
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def test_png_after_walking(game):
    mark_here(game)
    for _ in range(20):
        before = party(game)
        game.move_party(FORWARD)
        if party(game) == before:
            game.move_turn_party(TURN_LEFT)
        mark_here(game)
    w, h, scale = VIEWPORT.w, VIEWPORT.h, 3
    px = draw(game, w, h, Box(0, 0, w, h))
    assert len(set(px)) > 3
    game.ui_set_screen_palette(None)
    pal = (ctypes.c_uint16 * 16).in_dll(game, "palette")
    rows = [[(pal[px[y * w + x]] >> s & 15) * 17 for x in range(w) for _ in range(scale) for s in (8, 4, 0)] for y in range(h)]
    if PNG_DIR:
        png(Path(PNG_DIR) / "automap.png", [row for row in rows for _ in range(scale)])


def test_darkness_shortens_the_visible_depth(game):
    lib = game
    lib.automap_get_visible_depths.restype = c16
    palette = c16.in_dll(lib, "g_party_light_level")
    best = None
    for y in range(lib.dungeon_map_height(0)):  # a position with a long free view ahead
        for x in range(lib.dungeon_map_width(0)):
            for d in range(4):
                palette.value = 0
                if lib.automap_get_visible_depths(0, x, y, d) == 4:
                    best = (x, y, d)
    assert best
    for darkness, depths in ((0, 4), (3, 4), (4, 3), (5, 2)):
        palette.value = darkness
        assert lib.automap_get_visible_depths(0, *best) == depths
    palette.value = 0
