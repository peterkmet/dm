# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import struct

import pytest

from dmtest.game import Bitmap, Box, ZoneClip, ZoneItem, setup_view, start_view, zone_records
from dmtest.game import screen as video_screen
from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm, needs_sdl

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

GAME = ROOT / "game"
GET_ZONE, SHRINK, LAYOUT_ZONES = 0x116296, 0x10E898, 0x102990

pytestmark = [needs_sdl]


@pytest.fixture(scope="module")
def lib():
    return setup_view(load_lib())


def bitmap(rows):
    w, h = len(rows[0]), len(rows)
    buf = (ctypes.c_uint8 * (w * h))(*[c for row in rows for c in row])
    bm = Bitmap(w, h, buf)
    bm._buf = buf
    return bm


def pixels(bm):
    return [[bm.px[y * bm.w + x] for x in range(bm.w)] for y in range(bm.h)]


def planar(px, w, h):
    rb = (w + 15) // 16 * 2
    out = bytearray(rb * h * 4)
    for y in range(h):
        for x in range(w):
            for p in range(4):
                if px[y][x] >> p & 1:
                    out[p * rb * h + y * rb + x // 8] |= 0x80 >> x % 8
    return out


def chunky(data, w, h):
    rb = (w + 15) // 16 * 2
    return [[sum((data[p * rb * h + y * rb + x // 8] >> 7 - x % 8 & 1) << p for p in range(4)) for x in range(w)] for y in range(h)]


@pytest.fixture(scope="module")
def emu():
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=50_000_000)


@needs_dm
@pytest.mark.parametrize("w, h, s", [(32, 40, 14), (45, 23, 21), (17, 64, 14), (24, 24, 30)])
def test_shrink_matches_original(lib, emu, w, h, s):
    px = [[(x * 7 + y * 3 + x * y) % 16 for x in range(w)] for y in range(h)]
    pal = bytes([0, 12, 1, 3, 4, 3, 6, 7, 5, 9, 10, 11, 0, 2, 14, 13])
    dw, dh = ((s >> 1) + w * s) >> 5, ((s >> 1) + h * s) >> 5
    emu.write(0x400000, planar(px, w, h))
    emu.write(0x500000, bytes((dw + 15) // 16 * 2 * dh * 4))
    emu.write(0x600000, pal)
    emu.call(SHRINK, [(4, 0x400000), (4, 0x500000), (2, w), (2, h), (2, dw), (2, dh), (4, 0x600000)])
    out = Bitmap()
    assert lib.draw_shrink(bitmap(px), dw, dh, pal, out) is True
    assert pixels(out) == chunky(emu.read(0x500000, (dw + 15) // 16 * 2 * dh * 4), dw, dh)


@needs_dm
def test_zones_match_original(lib, emu):
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())
    assert lib.layout_init() is True
    size = ctypes.c_uint16()
    raw = ctypes.string_at(lib.gfx_get_item(696, ctypes.byref(size)), size.value)
    chunks, at = zone_records(raw), 0x700000
    emu.write(LAYOUT_ZONES, struct.pack(">I", at))
    for i, (first, last, recs) in enumerate(chunks):
        nxt = at + 8 + len(recs) if i + 1 < len(chunks) else 0
        emu.write(at, struct.pack(">Ihh", nxt, first, last) + recs)
        at = nxt
    emu.write(0x6dfffc, struct.pack(">hh", 37, 23))  # bitmap header at -4: width, height
    zones = [z for first, last, _ in chunks for z in range(first, last + 1)]
    checked = 0
    for zone in zones:
        for w, h in ((0, 0), (10, 7), (300, 250)):
            emu.write(0x6f0000, struct.pack(">hhhhhh", 0, 0, 0, 0, w, h))
            found = emu.call(GET_ZONE, [(4, 0x6e0000), (4, 0x6f0000), (2, zone), (4, 0x6f0008), (4, 0x6f000a)]) & 0xFFFFFFFF
            placed = ZoneClip()
            ok = lib.layout_get_zone(zone, ZoneItem(37, 23, w, h), ctypes.byref(placed))
            assert bool(ok) == bool(found), zone
            if ok:
                box = placed.box
                assert (box.x, box.y, box.w, box.h, placed.source_x, placed.source_y) == struct.unpack(">hhhhhh", emu.read(0x6f0000, 12)), (zone, w, h)
                checked += 1
    assert checked > 500


def screen(lib):
    fb = video_screen(lib).px
    return bytes(fb)


@pytest.fixture(scope="module")
def game(lib):
    return start_view(lib)


MASKED_FILL, BLIT_BITMAP = 0x116A4E, 0x11739C


def word_flipped(px, w, h):
    """flip_bitmap_horizontal: mirrors each row over whole 16-pixel words."""
    pw = (w + 15) // 16 * 16
    return [[px[y][pw - 1 - x] if pw - 1 - x < w else 0 for x in range(pw)] for y in range(h)], pw


class MaskedFill(ctypes.Structure):
    _fields_ = [("units", ctypes.POINTER(Bitmap)), ("mask", ctypes.POINTER(Bitmap)), ("mask_flip", ctypes.c_bool),
                ("last_unit", ctypes.c_int16), ("first_unit", ctypes.c_int16), ("transparent", ctypes.c_int16),
                ("sx", ctypes.c_int16), ("sy", ctypes.c_int16)]


@needs_dm
@pytest.mark.parametrize("box, last, first, transparent, sx, mask_kind", [
    ((5, 3, 36, 20), 63, 7, 0x0A, 0, "plain"),
    ((0, 0, 36, 49), 64, 31, 0x0A, 12, "flipped"),
    ((10, 2, 40, 30), 50, 49, 0x8A, 0, None),
    ((3, 4, 17, 9), 9, 0, 0x03, 2, "plain"),
])
def test_masked_fill_matches_original(lib, emu, box, last, first, transparent, sx, mask_kind):
    lib.draw_masked_fill.argtypes = [ctypes.POINTER(Bitmap), ctypes.POINTER(Box), ctypes.POINTER(MaskedFill)]
    uw, uh, dw, dh = 32, 32, 64, 60
    units = [[(x * 5 + y * 3 + x * y) % 16 for x in range(uw)] for y in range(uh)]
    mw, mh = 36, 49
    mask = [[(x + y) % 3 != 0 for x in range(mw)] for y in range(mh)]
    dst = [[(x + 2 * y) % 16 for x in range(dw)] for y in range(dh)]
    words = ((box[2] - 1) >> 4) + 1
    emu.write(0x400000, planar(units, uw, uh))
    mask_addr = 0
    if mask_kind:
        mpx, mpw = word_flipped(mask, mw, mh) if mask_kind == "flipped" else (mask, mw)
        emu.write(0x500000, planar(mpx, mpw, mh))
        mask_addr = 0x500000
    emu.write(0x600000, struct.pack(">hhhh", *box))
    emu.write(0x610000, bytes(words * 2 * box[3] * 4))
    emu.write(0x700000, planar(dst, dw, dh))
    saved = emu.read(BLIT_BITMAP, 2)
    emu.write(BLIT_BITMAP, b"\x4e\x75")  # rts: the test applies the final blit, the blitter is not emulated
    try:
        emu.call(MASKED_FILL, [(4, 0x400000), (4, 0x700000), (4, mask_addr), (4, 0x610000), (4, 0x600000), (2, last), (2, first),
                               (2, dw), (2, transparent), (2, sx), (2, 0), (2, dh), (2, uw // 16 * uh)])
    finally:
        emu.write(BLIT_BITMAP, saved)
    temp = chunky(emu.read(0x610000, words * 2 * box[3] * 4), words * 16, box[3])
    key = transparent & 0x0F
    expected = [row[:] for row in dst]
    bx, by, bw, bh = box
    for y in range(bh):  # blit_bitmap of the temp bitmap with the color key
        for x in range(bw):
            c = temp[y][sx + x]
            if c != key:
                expected[by + y][bx + x] = c
    out = bitmap(dst)
    mask_bitmap = bitmap([[int(m) for m in row] for row in mask]) if mask_kind else None
    units_bitmap = bitmap(units)
    fill = MaskedFill(ctypes.pointer(units_bitmap), ctypes.pointer(mask_bitmap) if mask_bitmap is not None else None,
                      mask_kind == "flipped", last, first, transparent, sx, 0)
    lib.draw_masked_fill(out, Box(*box), fill)
    assert pixels(out) == expected

