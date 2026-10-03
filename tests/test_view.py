import ctypes
import struct
import subprocess
import zlib
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse
from test_port import BUILD, ROOT, sdl3_installed

GAME = ROOT / "game"
SCRATCH = Path("/private/tmp/claude-501/-Users-lego-github-dm/a035dd1c-2cf2-40ec-9360-f301ae6b451f/scratchpad")
GET_ZONE, SHRINK, LAYOUT_ZONES = 0x116296, 0x10E898, 0x102990

pytestmark = pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed")
needs_game = pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing")


class Bitmap(ctypes.Structure):
    _fields_ = [("w", ctypes.c_uint16), ("h", ctypes.c_uint16), ("px", ctypes.POINTER(ctypes.c_uint8))]


class Box(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int16), ("y", ctypes.c_int16), ("w", ctypes.c_int16), ("h", ctypes.c_int16)]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(BUILD / "libdm.dylib"))
    lib.draw_blit.argtypes = [ctypes.POINTER(Bitmap), ctypes.c_int16, ctypes.c_int16, ctypes.POINTER(Bitmap), ctypes.POINTER(Box), ctypes.c_int16, ctypes.c_int]
    lib.draw_shrink.argtypes = [ctypes.POINTER(Bitmap), ctypes.c_int16, ctypes.c_int16, ctypes.c_char_p, ctypes.POINTER(Bitmap)]
    lib.layout_get_zone.argtypes = [ctypes.c_int16, ctypes.c_int16, ctypes.c_int16, ctypes.POINTER(Box), ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_int16)]
    lib.gfx_item.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.gfx_item.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint16)]
    lib.view_render.argtypes = [ctypes.c_int16] * 4
    return lib


def bitmap(rows):
    w, h = len(rows[0]), len(rows)
    buf = (ctypes.c_uint8 * (w * h))(*[c for row in rows for c in row])
    bm = Bitmap(w, h, buf)
    bm._buf = buf
    return bm


def pixels(bm):
    return [[bm.px[y * bm.w + x] for x in range(bm.w)] for y in range(bm.h)]


def test_blit_transparent_and_flip(lib):
    src = bitmap([[1, 2, 3], [4, 5, 0]])
    dst = bitmap([[9] * 4 for _ in range(3)])
    lib.draw_blit(src, 0, 0, dst, Box(1, 1, 3, 2), 0, 0)
    assert pixels(dst) == [[9, 9, 9, 9], [9, 1, 2, 3], [9, 4, 5, 9]]
    lib.draw_blit(src, 1, 0, dst, Box(0, 0, 2, 2), -1, 1)
    assert pixels(dst) == [[2, 1, 9, 9], [5, 4, 2, 3], [9, 4, 5, 9]]
    lib.draw_blit(src, 0, 0, dst, Box(2, 1, 3, 2), 0x12, 3)  # key uses bits 0-3, both flips, clipped right
    assert pixels(dst) == [[2, 1, 9, 9], [5, 4, 0, 5], [9, 4, 3, 9]]


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
    if not (GAME / "KAOS.FTL").exists():
        pytest.skip("game files missing")
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=50_000_000)


@needs_game
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
    assert lib.draw_shrink(bitmap(px), dw, dh, pal, out) == 0
    assert pixels(out) == chunky(emu.read(0x500000, (dw + 15) // 16 * 2 * dh * 4), dw, dh)


def zone_records(raw):
    ranges = struct.unpack_from(">H", raw, 2)[0]
    pairs = [struct.unpack_from(">hh", raw, 4 + 4 * i) for i in range(ranges)]
    off, out = 4 + 4 * ranges, []
    for first, last in pairs:
        n = (last - first + 1) * 8
        out.append((first, last, raw[off:off + n]))
        off += n
    return out


@needs_game
def test_zones_match_original(lib, emu):
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) > 0
    assert lib.layout_init() == 0
    size = ctypes.c_uint16()
    raw = ctypes.string_at(lib.gfx_item(696, ctypes.byref(size)), size.value)
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
            box, cw, ch = Box(), ctypes.c_int16(w), ctypes.c_int16(h)
            ok = lib.layout_get_zone(zone, 37, 23, ctypes.byref(box), ctypes.byref(cw), ctypes.byref(ch))
            assert bool(ok) == bool(found), zone
            if ok:
                assert (box.x, box.y, box.w, box.h, cw.value, ch.value) == struct.unpack(">hhhhhh", emu.read(0x6f0000, 12)), (zone, w, h)
                checked += 1
    assert checked > 500


def png(path, rgb_rows):
    h, w = len(rgb_rows), len(rgb_rows[0]) // 3
    raw = b"".join(b"\0" + bytes(row) for row in rgb_rows)
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def screen(lib):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    pal = (ctypes.c_uint16 * 16).in_dll(lib, "palette")
    rgb = [[(pal[c] >> s & 15) * 17 for c in (fb[y * 320 + x] for x in range(320)) for s in (8, 4, 0)] for y in range(200)]
    return bytes(fb), rgb


@pytest.fixture(scope="module")
def game(lib):
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")):
        pytest.skip("game files missing")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) > 0
    assert lib.layout_init() == 0
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode())
    party = [ctypes.c_int16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")]
    return lib, party


VIEWS = {"start": None, "door": (0, 1, 13, 2), "ornament": (0, 4, 9, 3), "text": (0, 4, 17, 1), "stairs": (0, 4, 15, 3)}  # map, x, y, direction


@needs_game
@pytest.mark.parametrize("name", list(VIEWS))
def test_render_view(game, tmp_path, name):
    lib, party = game
    m, x, y, d = VIEWS[name] or party
    ctypes.memset(ctypes.addressof(ctypes.c_uint8.in_dll(lib, "fb")), 0, 320 * 200)
    assert lib.view_render(m, x, y, d)
    first, rgb = screen(lib)
    assert lib.view_render(m, x, y, d)
    assert screen(lib)[0] == first
    assert len(set(first)) > 4
    for out in (tmp_path, SCRATCH):
        if out.exists():
            png(out / f"view_{name}.png", rgb)
