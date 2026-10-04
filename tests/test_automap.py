import ctypes
import struct
import subprocess
import zlib
from pathlib import Path

import pytest

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
SCRATCH = Path("/private/tmp/claude-501/-Users-lego-github-dm/a035dd1c-2cf2-40ec-9360-f301ae6b451f/scratchpad")
c16 = ctypes.c_int16
NORTH, EAST, SOUTH, WEST = range(4)
STEP = {NORTH: (0, -1), EAST: (1, 0), SOUTH: (0, 1), WEST: (-1, 0)}
FORWARD, TURN_LEFT = 3, 1
CELL, BLACK, YELLOW, FILLER = 7, 0, 11, 0xEE
WALL, DOOR, FAKE_WALL, FAKE_WALL_OPEN, DOOR_CLOSED = 0, 4, 6, 4, 4


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


class Bitmap(ctypes.Structure):
    _fields_ = [("w", ctypes.c_uint16), ("h", ctypes.c_uint16), ("px", ctypes.POINTER(ctypes.c_uint8))]


VIEWPORT = Box(0, 33, 224, 136)


@pytest.fixture
def game():
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Dungeon.DAT")):
        pytest.skip("missing game files")
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.dungeon_square.restype = ctypes.c_uint8
    for name in ("dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = c16
    lib.automap_save.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    lib.automap_load.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    lib.automap_draw.argtypes = [ctypes.POINTER(Bitmap), Box, c16]
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode()) == 1
    assert lib.tables_init() == 0
    lib.game_new()
    lib.move_party_enter_dungeon()
    lib.automap_reset()
    return lib


def party(lib):
    return tuple(c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"))


def on_map(lib, m, x, y):
    return 0 <= x < lib.dungeon_map_width(m) and 0 <= y < lib.dungeon_map_height(m)


def blocks(lib, m, x, y):
    if not on_map(lib, m, x, y):
        return True
    square = lib.dungeon_square(m, x, y)
    kind = square >> 5
    return kind == WALL or (kind == FAKE_WALL and not square & FAKE_WALL_OPEN) or (kind == DOOR and square & 7 == DOOR_CLOSED)


def ahead(x, y, d, depth, side=0):
    (fx, fy), (rx, ry) = STEP[d], STEP[(d + 1) % 4]
    return x + depth * fx + side * rx, y + depth * fy + side * ry


def mark_here(lib):
    m, x, y, d = party(lib)
    lib.automap_mark_seen(m, x, y, d)


def seen_squares(lib, m):
    return {(x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m)) if lib.automap_is_seen(m, x, y)}


def test_marks_start_and_squares_ahead(game):
    m, x, y, d = party(game)
    assert not game.automap_is_seen(m, x, y)
    mark_here(game)
    assert game.automap_is_seen(m, x, y)
    assert game.automap_is_seen(m, *ahead(x, y, d, 1))
    for side in (-1, 1):
        if on_map(game, m, *ahead(x, y, d, 0, side)):
            assert game.automap_is_seen(m, *ahead(x, y, d, 0, side))


def test_walls_stop_visibility(game):
    m, x, y, _ = party(game)
    stopped = 0
    for d in range(4):
        game.automap_reset()
        game.automap_mark_seen(m, x, y, d)
        expected = set()
        for depth in range(4):
            expected |= {ahead(x, y, d, depth, side) for side in (-1, 0, 1) if on_map(game, m, *ahead(x, y, d, depth, side))}
            if depth and blocks(game, m, *ahead(x, y, d, depth)):
                stopped += depth < 3
                break
        assert seen_squares(game, m) == expected
    assert stopped


def save(lib):
    buf, size = ctypes.POINTER(ctypes.c_uint8)(), ctypes.c_size_t()
    assert lib.automap_save(ctypes.byref(buf), ctypes.byref(size)) == 0
    data = ctypes.string_at(buf, size.value)
    ctypes.CDLL(None).free(buf)
    return data


def test_save_load_round_trip(game):
    m = party(game)[0]
    mark_here(game)
    before, data = seen_squares(game, m), save(game)
    assert data[:5] == b"AMAP\x01"
    game.automap_reset()
    assert not seen_squares(game, m)
    for bad in (data[:-1], b"XMAP" + data[4:], data[:4] + b"\x02" + data[5:], data[:-1] + b"\x02"):
        assert game.automap_load(bad, len(bad)) == -1
    assert game.automap_load(data, len(data)) == 0
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
        left, top = VIEWPORT.x + (VIEWPORT.w - CELL) // 2, VIEWPORT.y + (VIEWPORT.h - CELL) // 2
        assert px[(top + CELL // 2) * 320 + left + CELL // 2] == YELLOW


def png(path, rgb_rows):
    h, w = len(rgb_rows), len(rgb_rows[0]) // 3
    raw = b"".join(b"\0" + bytes(row) for row in rgb_rows)
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def test_png_after_walking(game):
    mark_here(game)
    for _ in range(20):
        before = party(game)
        game.command_move_party(FORWARD)
        if party(game) == before:
            game.command_turn_party(TURN_LEFT)
        mark_here(game)
    w, h, scale = VIEWPORT.w, VIEWPORT.h, 3
    px = draw(game, w, h, Box(0, 0, w, h))
    assert len(set(px)) > 3
    game.ui_set_screen_palette(None)
    pal = (ctypes.c_uint16 * 16).in_dll(game, "palette")
    rows = [[(pal[px[y * w + x]] >> s & 15) * 17 for x in range(w) for _ in range(scale) for s in (8, 4, 0)] for y in range(h)]
    if SCRATCH.exists():
        png(SCRATCH / "automap.png", [row for row in rows for _ in range(scale)])


def test_darkness_shortens_the_visible_depth(game):
    lib = game
    lib.automap_visible_depths.restype = c16
    palette = c16.in_dll(lib, "g_dungeon_view_palette_index")
    best = None
    for y in range(lib.dungeon_map_height(0)):  # a position with a long free view ahead
        for x in range(lib.dungeon_map_width(0)):
            for d in range(4):
                palette.value = 0
                if lib.automap_visible_depths(0, x, y, d) == 4:
                    best = (x, y, d)
    assert best
    for darkness, depths in ((0, 4), (3, 4), (4, 3), (5, 2)):
        palette.value = darkness
        assert lib.automap_visible_depths(0, *best) == depths
    palette.value = 0
