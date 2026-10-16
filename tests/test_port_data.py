import ctypes
import os
import struct
import subprocess
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse
from test_port import BUILD, ROOT, sdl3_installed

GAME = ROOT / "game"
BASE = 0x100000
EXPAND_GRAPHIC = 0x113CC8
PACKED, BITMAP = 0x400000, 0x500000

pytestmark = pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed")
needs_game = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists() or not (GAME / "Graphics.DAT").exists(), reason="game files missing")


class FtlImage(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_uint32), ("base", ctypes.c_uint32), ("data_base", ctypes.c_uint32), ("code_base", ctypes.c_uint32)]


class Bitmap(ctypes.Structure):
    _fields_ = [("w", ctypes.c_uint16), ("h", ctypes.c_uint16), ("px", ctypes.POINTER(ctypes.c_uint8))]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(BUILD / "libdm.dylib"))
    lib.ftl_load.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint32, ctypes.POINTER(FtlImage)]
    lib.gfx_get_item.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.gfx_expand.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(Bitmap)]
    for f in ("kaos_u8", "kaos_u16", "kaos_s16", "kaos_u32"):
        getattr(lib, f).argtypes = [ctypes.c_uint32]
    lib.kaos_u16.restype = ctypes.c_uint16
    lib.kaos_s16.restype = ctypes.c_int16
    lib.kaos_u32.restype = ctypes.c_uint32
    return lib


def c_image(lib, raw):
    img = FtlImage()
    if lib.ftl_load(raw, len(raw), BASE, ctypes.byref(img)):
        return None
    out = ctypes.string_at(img.data, img.size), img.data_base, img.code_base
    lib.ftl_free(ctypes.byref(img))
    return out


@needs_game
@pytest.mark.parametrize("path", sorted(GAME.glob("*.FTL")), ids=lambda p: p.name)
def test_ftl_image_matches_python(lib, path):
    raw = path.read_bytes()
    img = load_image(parse(raw), BASE)
    assert c_image(lib, raw) == (img.data, img.data_base, img.code_base)


@needs_game
def test_ftl_bad_checksum(lib):
    raw = bytearray((GAME / "KAOS.FTL").read_bytes())
    data_off = section(raw, 0x11)[0]
    for at in (2, data_off + 12):  # header checksum, data byte
        bad = bytearray(raw)
        bad[at] ^= 1
        with pytest.raises(ValueError):
            parse(bytes(bad))
        assert c_image(lib, bytes(bad)) is None


def section(raw, tag):
    count = struct.unpack_from(">H", raw, 0x12)[0]
    return next(struct.unpack_from(">II", raw, 0x14 + i * 12 + 4) for i in range(count) if struct.unpack_from(">H", raw, 0x14 + i * 12)[0] == tag)


@needs_game
def test_ftl_huge_region(lib):
    raw = bytearray((GAME / "VDEO.FTL").read_bytes())
    off, size = section(raw, 0x10)
    struct.pack_into(">I", raw, off, 0xFFF00000)
    struct.pack_into(">H", raw, off + 0x22, 0)
    struct.pack_into(">H", raw, off + 0x22, sum(struct.unpack_from(f">{size // 2}H", raw, off)) & 0xFFFF)
    parse(bytes(raw))  # checksums still valid
    assert c_image(lib, bytes(raw)) is None


@needs_game
def test_graphics_dat_rejects_bad_files(lib, tmp_path):
    dat = (GAME / "Graphics.DAT").read_bytes()
    for name, data in [("truncated", dat[:1000]), ("short", dat[:3]), ("format0", b"\x00\x01" + dat[4:])]:
        (tmp_path / name).write_bytes(data)
        assert lib.gfx_open(str(tmp_path / name).encode()) == -1
        assert lib.gfx_count() == 0


@needs_game
def test_kaos_accessors(lib):
    data = load_image(parse((GAME / "KAOS.FTL").read_bytes()), BASE).data
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    for addr in (0x100AA6, 0x100170, 0x102566, BASE + len(data) - 4):
        p = addr - BASE
        assert lib.kaos_u8(addr) == data[p]
        assert lib.kaos_u16(addr) == struct.unpack_from(">H", data, p)[0]
        assert lib.kaos_s16(addr) == struct.unpack_from(">h", data, p)[0]
        assert lib.kaos_u32(addr) == struct.unpack_from(">I", data, p)[0]


# PLANE_BITS[k][b]: the 8 pixels of byte b as bytes holding bit k (big-endian bit order)
PLANE_BITS = [[bytes(((b >> (7 - i)) & 1) << k for i in range(8)) for b in range(256)] for k in range(4)]


def planar_to_chunky(planes, w, h):
    rb = (w + 15) // 16 * 2
    size = rb * h
    px = 0
    for k in range(4):
        px |= int.from_bytes(b"".join(PLANE_BITS[k][b] for b in planes[k * size : (k + 1) * size]))
    px = px.to_bytes(size * 8)
    return b"".join(px[y * rb * 8 : y * rb * 8 + w] for y in range(h))


def bitmap_items(dat):
    count = struct.unpack_from(">H", dat, 2)[0]
    sizes = struct.unpack_from(f">{count}H", dat, 4)
    off = 4 + count * 8
    for i, size in enumerate(sizes):
        item = dat[off : off + size]
        off += size
        w, h = struct.unpack_from(">HH", item) if size >= 4 else (0, 0)
        if 0 < w <= 320 and 0 < h <= 200 and i != 672:  # 672 is a sound sample with a size field
            yield i, item, w, h


@needs_game
def test_graphics_dat_items(lib):
    dat = (GAME / "Graphics.DAT").read_bytes()
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) == struct.unpack_from(">H", dat, 2)[0] == lib.gfx_count()
    size = ctypes.c_uint16()
    for i, item, _, _ in bitmap_items(dat):
        assert ctypes.string_at(lib.gfx_get_item(i, ctypes.byref(size)), size.value) == item


@needs_game
def test_expand_graphic_matches_original(lib):
    dat = (GAME / "Graphics.DAT").read_bytes()
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), BASE))
    compared = 0
    for i, item, w, h in bitmap_items(dat):
        size = (w + 15) // 16 * 2 * h * 4
        emu.write(BITMAP, bytes(size))
        emu.write(PACKED, item)
        emu.call(EXPAND_GRAPHIC, [(4, PACKED), (4, BITMAP)])
        expected = planar_to_chunky(emu.read(BITMAP, size), w, h)
        bm = Bitmap()
        assert lib.gfx_expand(item, len(item), ctypes.byref(bm)) == 0
        assert (bm.w, bm.h) == (w, h)
        assert ctypes.string_at(bm.px, w * h) == expected, f"graphic {i}"
        lib.gfx_free_bitmap(ctypes.byref(bm))
        compared += 1
    assert compared == 687


@needs_game
def test_show_smoke_run(lib):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    r = subprocess.run([BUILD / "dm", "--data", GAME, "--show", "1", "--frames", "5"], env=env, capture_output=True, timeout=10)
    assert r.returncode == 0, r.stderr
