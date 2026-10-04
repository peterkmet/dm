import ctypes
import shutil
import struct
import subprocess
from pathlib import Path

import pytest
from dmcsb import THING_TYPES, open_dungeon
from dmcsb.dungeon import _default_escape

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
FILES = ["Dungeon", "DungeonF", "DungeonG"]
NEW_GAME_EXTRA_THINGS = 0x101058
DECOMPRESS, DECODE_TEXT = 0x12712C, 0x10B312
G_THING_DATA_2, G_DUNGEON_TEXT_DATA = 0x101CF2, 0x101C72

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")
u8p = ctypes.POINTER(ctypes.c_uint8)


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.decompress_dungeon.argtypes = [ctypes.c_char_p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32]
    lib.decode_text.argtypes = [ctypes.c_char_p, ctypes.c_int16, ctypes.c_uint16, ctypes.c_int16]
    lib.dungeon_square_first_thing.restype = ctypes.c_uint16
    lib.dungeon_square.restype = ctypes.c_uint8
    lib.get_thing_data.restype = u8p
    lib.kaos_ptr.restype = ctypes.c_void_p
    lib.kaos_ptr.argtypes = [ctypes.c_uint32]
    return lib


def need(lib, *names):
    missing = [n for n in ("KAOS.FTL", *names) if not (GAME / n).exists()]
    if missing:
        pytest.skip(f"missing game/{missing[0]}")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0


def load(lib, path):
    assert lib.load_dungeon(str(path).encode()) == 1
    return open_dungeon(str(path))


def mem(lib, name, n):
    return bytes(u8p.in_dll(lib, name)[:n])


def decode(lib, thing, mode, size=4096):
    buf = ctypes.create_string_buffer(size)
    assert lib.decode_text(buf, size, thing, mode) == 1
    return buf


def kaos_emu():
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=200_000_000)


@pytest.mark.parametrize("name", FILES)
def test_dungeon_matches_dmcsb(lib, name):
    need(lib, f"{name}.DAT")
    d = load(lib, GAME / f"{name}.DAT")
    extra = bytes(ctypes.cast(lib.kaos_ptr(NEW_GAME_EXTRA_THINGS), u8p)[:16])
    hdr = mem(lib, "g_dungeon_header", 44)
    assert lib.dungeon_map_count() == d.hdr["MapCount"]
    assert struct.unpack(">H", hdr[10:12])[0] == d.hdr["SquareFirstThingCount"] + 300
    maps = mem(lib, "g_dungeon_maps", 16 * len(d.maps))
    for m, info in enumerate(d.maps):
        w, h = info["Width"] + 1, info["Height"] + 1
        assert (lib.dungeon_map_width(m), lib.dungeon_map_height(m)) == (w, h)
        offset, _, _, ox, oy = struct.unpack_from(">HHHBB", maps, 16 * m)
        assert (offset, ox, oy) == (info["RawMapDataByteOffset"], info["OffsetMapX"], info["OffsetMapY"])
        located = {}
        for t in d.located_things(info["Level"]):
            located.setdefault((t["x"], t["y"]), t)
        for x in range(w):
            for y in range(h):
                sq = lib.dungeon_square(m, x, y)
                assert sq == d.tile_data[offset + x * h + y]
                first = lib.dungeon_square_first_thing(m, x, y)
                assert (first != 0xFFFE) == bool(sq & 0x10)
                if (x, y) in located:
                    assert first >> 14 == located[(x, y)]["cell"]
                    assert struct.unpack(">H", bytes(lib.get_thing_data(first)[:2]))[0] == located[(x, y)]["thing"]["Next"]
    for t, (tname, size, _) in enumerate(THING_TYPES):
        n = d.hdr["ThingCount"][t]
        total = min(0x300 if t == 15 else 0x400, n + extra[t])
        assert struct.unpack(">H", hdr[12 + 2 * t : 14 + 2 * t])[0] == total
        if not size:
            continue
        data = bytes((u8p * 16).in_dll(lib, "g_thing_data")[t][: total * size])
        nexts = [struct.unpack_from(">H", data, i * size)[0] for i in range(total)]
        assert nexts[:n] == [r["Next"] for r in d.things.get(tname, [])]
        assert nexts[n:] == [0xFFFF] * (total - n)


@pytest.mark.parametrize("name", FILES)
def test_text_matches_dmcsb(lib, name):
    need(lib, f"{name}.DAT")
    d = load(lib, GAME / f"{name}.DAT")
    compared = 0
    for i, _, _ in d.texts():
        # Escape 30 differs except "THE "/"YOU ": dmcsb gives display glyphs, the original font codes (see emu test)
        ref = d.text(i, escape=lambda c, n: _default_escape(c, n) if c == 29 or n in (2, 3) else "\0")
        if "\0" in ref:
            continue
        assert decode(lib, 2 << 10 | i, 2 | -0x8000).value.decode("latin1") == ref
        assert decode(lib, 2 << 10 | i, 1 | -0x8000).value.decode("latin1") == "\n" + ref.replace("\n", " ")
        compared += 1
    assert compared >= 90


@pytest.mark.parametrize("name", FILES)
def test_decode_text_matches_original(lib, name):
    need(lib, f"{name}.DAT")
    assert lib.load_dungeon(str(GAME / f"{name}.DAT").encode()) == 1
    hdr = mem(lib, "g_dungeon_header", 44)
    count, words = struct.unpack(">H", hdr[16:18])[0], struct.unpack(">H", hdr[6:8])[0]
    emu = kaos_emu()
    emu.write(0x200000, bytes((u8p * 16).in_dll(lib, "g_thing_data")[2][: count * 4]))
    emu.write(0x210000, mem(lib, "g_dungeon_text_data", words * 2))
    emu.write(G_THING_DATA_2, struct.pack(">I", 0x200000))
    emu.write(G_DUNGEON_TEXT_DATA, struct.pack(">I", 0x210000))
    for i in range(count):
        for mode in (0, 1, 2, -0x8000, -0x7FFF, -0x7FFE):
            emu.write(0x220000, bytes(4096))
            emu.call(DECODE_TEXT, [(4, 0x220000), (2, 2 << 10 | i), (2, mode)])
            assert decode(lib, 2 << 10 | i, mode).raw == bytes(emu.read(0x220000, 4096)), (i, mode)


def test_decompress_matches_original(lib):
    need(lib, "Dungeon.DAT")
    raw = (GAME / "Dungeon.DAT").read_bytes()
    assert raw[:2] == b"\x81\x04"
    size = struct.unpack(">I", raw[2:6])[0]
    emu = kaos_emu()
    emu.write(0x200000, raw[8:])
    emu.call(DECOMPRESS, [(4, 0x200000), (4, 0x300000), (4, size)])
    out = ctypes.create_string_buffer(size)
    lib.decompress_dungeon(raw[8:], len(raw) - 8, out, size)
    assert out.raw == bytes(emu.read(0x300000, size))


def plain_dungeon(lib):
    """Dungeon.DAT decompressed, without the checksum word."""
    raw = (GAME / "Dungeon.DAT").read_bytes()
    size = struct.unpack(">I", raw[2:6])[0]
    out = ctypes.create_string_buffer(size)
    assert lib.decompress_dungeon(raw[8:], len(raw) - 8, out, size) == 1
    return bytearray(out.raw[:-2])


def test_uncompressed_file(lib, tmp_path):
    need(lib, "Dungeon.DAT")
    plain = plain_dungeon(lib)
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode()) == 1
    ref = mem(lib, "g_dungeon_raw_map_data", struct.unpack(">H", plain[2:4])[0])
    path = tmp_path / "plain.DAT"
    path.write_bytes(plain)
    assert lib.load_dungeon(str(path).encode()) == 1
    assert mem(lib, "g_dungeon_raw_map_data", len(ref)) == ref
    path.write_bytes(plain + struct.pack(">H", (sum(plain) + 1) & 0xFFFF))
    assert lib.load_dungeon(str(path).encode()) == 0  # bad checksum


def bad_count(d):
    d[12:14] = b"\x04\x01"  # 1025 doors


def bad_map_offset(d):
    d[44:46] = b"\xff\xff"


def no_maps(d):
    d[4] = 0


def big_raw_size(d):
    d[2:4] = b"\xff\xff"  # raw map data size past the end


@pytest.mark.parametrize("change", [bad_count, bad_map_offset, no_maps, big_raw_size])
def test_bad_header_fails(lib, tmp_path, change):
    need(lib, "Dungeon.DAT")
    d = plain_dungeon(lib)
    change(d)
    path = tmp_path / "bad.DAT"
    path.write_bytes(d)
    assert lib.load_dungeon(str(path).encode()) == 0
    assert lib.dungeon_map_count() == 0
    assert not u8p.in_dll(lib, "g_dungeon_header")


THING_SIZES = [4, 6, 4, 8, 16, 4, 4, 4, 4, 8, 4, 0, 0, 0, 8, 4]


def offsets(d):
    """Byte offsets of the parts of a plain dungeon: columns, square first things, records of each thing type."""
    maps = d[4]
    columns = 44 + 16 * maps
    width = sum((struct.unpack_from(">H", d, 44 + 16 * m + 8)[0] >> 6 & 31) + 1 for m in range(maps))
    first_things = columns + 2 * width
    p = first_things + 2 * struct.unpack_from(">H", d, 10)[0] + 2 * struct.unpack_from(">H", d, 6)[0]
    records = []
    for t in range(16):
        records.append(p)
        p += struct.unpack_from(">H", d, 12 + 2 * t)[0] * THING_SIZES[t]
    return columns, first_things, records


def bad_next_link(d):
    d[offsets(d)[2][0] : offsets(d)[2][0] + 2] = b"\x03\xff"  # door 1023


def bad_next_link_type(d):
    d[offsets(d)[2][0] : offsets(d)[2][0] + 2] = b"\x2c\x00"  # type 11 has no records


def bad_first_thing(d):
    d[offsets(d)[1] : offsets(d)[1] + 2] = b"\x0b\xff"  # teleporter 1023


def bad_column_index(d):
    d[offsets(d)[0] : offsets(d)[0] + 2] = b"\xff\x00"


def bad_teleporter_target(d):
    teleporters = offsets(d)[2][1]
    used = next(i for i in range(0, 6 * 1024, 6) if d[teleporters + i : teleporters + i + 2] != b"\xff\xff")
    d[teleporters + used + 4] = 0xFF  # target map 255


@pytest.mark.parametrize(
    "change", [bad_next_link, bad_next_link_type, bad_first_thing, bad_column_index, bad_teleporter_target]
)
def test_corrupt_links_fail(lib, tmp_path, change):
    need(lib, "Dungeon.DAT")
    d = plain_dungeon(lib)
    path = tmp_path / "bad.DAT"
    path.write_bytes(d)
    assert lib.load_dungeon(str(path).encode()) == 1
    assert lib.dungeon_validate() == 1
    change(d)
    path.write_bytes(d)
    assert lib.load_dungeon(str(path).encode()) == 0
    assert lib.dungeon_map_count() == 0


@pytest.mark.parametrize("cut", [9, 28, 1000, 20000])
def test_truncated_file_fails(lib, tmp_path, cut):
    need(lib, "Dungeon.DAT")
    path = tmp_path / "cut.DAT"
    path.write_bytes((GAME / "Dungeon.DAT").read_bytes()[:cut])
    assert lib.load_dungeon(str(path).encode()) == 0


def test_bad_text_offset_and_small_buffer(lib):
    need(lib, "Dungeon.DAT")
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode()) == 1
    buf = ctypes.create_string_buffer(8)
    assert lib.decode_text(buf, 8, 2 << 10, 2 | -0x8000) == 0  # text 0 is longer than 7 bytes
    assert len(buf.value) == 7
    words = struct.unpack(">H", mem(lib, "g_dungeon_header", 8)[6:8])[0]
    rec = lib.get_thing_data(2 << 10)
    rec[2], rec[3] = (words << 3 | 1) >> 8 & 0xFF, (words << 3 | 1) & 0xFF
    assert lib.decode_text(buf, 8, 2 << 10, 2) == 0
