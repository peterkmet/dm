# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes

import pytest

from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm, needs_sdl

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

GAME = ROOT / "game"
BASE = 0x100000
TEXT_PRINT_TO_BITMAP = 0x109116
G_FONT_BITMAP = 0x100F26
FONT, TEXT, PLANES = 0x400000, 0x410000, 0x500000
COLS, NO_EXPIRY = 53, 0xFFFFFFFF

pytestmark = [needs_sdl, needs_dm]


class Bitmap(ctypes.Structure):
    _fields_ = [("w", ctypes.c_uint16), ("h", ctypes.c_uint16), ("px", ctypes.POINTER(ctypes.c_uint8))]


class Point(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int16), ("y", ctypes.c_int16)]


class MessageArea(ctypes.Structure):
    _fields_ = [
        ("text", (ctypes.c_char * COLS) * 4),
        ("color", (ctypes.c_uint8 * COLS) * 4),
        ("expiry", ctypes.c_uint32 * 4),
        ("cursor_row", ctypes.c_int16),
        ("cursor_x", ctypes.c_int16),
    ]


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())
    lib.text_init.restype = ctypes.c_bool
    assert lib.text_init()
    lib.text_draw.argtypes = [ctypes.POINTER(Bitmap), Point, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_char_p]
    lib.message_print.argtypes = [ctypes.c_int16, ctypes.c_char_p]
    return lib


def c_draw(lib, w, h, fill, x, y, fg, bg, text):
    buf = (ctypes.c_uint8 * (w * h))(*[fill] * (w * h))
    lib.text_draw(ctypes.byref(Bitmap(w, h, buf)), Point(x, y), fg, bg, text)
    return bytes(buf)


def original_draw(w, h, fill, x, y, fg, bg, text):
    """Runs text_print_to_bitmap on a 4-plane buffer and returns it as chunky pixels."""
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), BASE))
    gfx = (GAME / "Graphics.DAT").read_bytes()
    n = int.from_bytes(gfx[2:4], "big")
    off = 4 + n * 8 + sum(int.from_bytes(gfx[4 + 2 * i : 6 + 2 * i], "big") for i in range(0x2B7))
    emu.write(FONT, gfx[off : off + 0x300])
    emu.mem.w32(G_FONT_BITMAP, FONT)
    emu.write(TEXT, text + b"\0")
    rb, plane = w // 8, w // 8 * h
    emu.write(PLANES, b"".join(bytes([0xFF if fill >> p & 1 else 0]) * plane for p in range(4)))
    emu.call(TEXT_PRINT_TO_BITMAP, [(4, PLANES), (2, w), (2, x), (2, y), (2, fg), (2, bg), (4, TEXT), (2, h)])
    data = emu.read(PLANES, 4 * plane)
    return bytes(
        sum((data[p * plane + py * rb + px // 8] >> (7 - px % 8) & 1) << p for p in range(4))
        for py in range(h)
        for px in range(w)
    )


@pytest.mark.parametrize("x", [0, 3, 11, 13])
def test_text_draw_matches_original(lib, x):
    want = original_draw(48, 8, 5, x, 5, 15, 2, b"HELLO")
    assert want.count(15) > 40 and want.count(2) > 40
    assert c_draw(lib, 48, 8, 5, x, 5, 15, 2, b"HELLO") == want


def test_text_draw_glyph_pixels(lib):
    px = c_draw(lib, 6, 6, 9, 0, 4, 1, 0, b"H")
    rows = ["".join(".#9"[[0, 1, 9].index(px[y * 6 + x])] for x in range(6)) for y in range(6)]
    assert rows == ["#...#9", "#...#9", "#####9", "#...#9", "#...#9", ".....9"]


def test_text_draw_clips(lib):
    full = c_draw(lib, 20, 12, 3, 4, 7, 15, 1, b"AB")
    clipped = c_draw(lib, 10, 4, 3, -2, 1, 15, 1, b"AB")  # same text 6 px left, 6 px up
    assert clipped == bytes(full[(y + 6) * 20 + x + 6] for y in range(4) for x in range(10))


def message(lib):
    return MessageArea.in_dll(lib, "g_message")


def row(m, r):
    return m.text[r].raw.rstrip(b"\0")


def test_message_scroll_and_expiry(lib):
    lib.message_clear()
    m = message(lib)
    assert (m.cursor_row, m.cursor_x, list(m.expiry)) == (3, 0, [NO_EXPIRY] * 4)
    game_time = ctypes.c_uint32.in_dll(lib, "g_game_time")
    game_time.value = 100
    lib.message_print(13, b"\nONE")
    game_time.value = 110
    lib.message_print(14, b"\nTWO")
    assert [row(m, r) for r in range(4)] == [b"", b"", b"ONE", b"TWO"]
    assert list(m.expiry) == [NO_EXPIRY, NO_EXPIRY, 170, 180]
    assert (m.color[2][0], m.color[3][0], m.cursor_x) == (13, 14, 18)
    game_time.value = 169
    lib.message_clear_expired_rows()
    assert row(m, 2) == b"ONE"
    game_time.value = 170
    lib.message_clear_expired_rows()
    assert [row(m, 2), row(m, 3), m.expiry[2]] == [b"", b"TWO", NO_EXPIRY]


def test_message_word_wrap(lib):
    lib.message_clear()
    m = message(lib)
    words = b" ".join([b"WORD%02d" % i for i in range(9)])  # 9 * 7 - 1 = 62 chars
    lib.message_print(15, b"\n" + words)
    assert row(m, 2) == b"WORD00 WORD01 WORD02 WORD03 WORD04 WORD05 WORD06"
    assert row(m, 3) == b"\0\0WORD07 WORD08"  # continuation row starts at x 12
    assert m.cursor_x == 12 + 13 * 6


def test_message_cursor_clamps_large_column(lib):
    lib.message_clear()
    m = message(lib)
    lib.message_move_cursor(5462, 0)  # column * 6 overflows int16 without the clamp
    assert m.cursor_x == 312
    lib.message_print(1, b"HELLO WORLD")
    assert row(m, 0) == b"\0" * 52 + b"H"  # last cell, then the line wraps
    assert row(m, 1) == b"\0\0ELLO WORLD"
