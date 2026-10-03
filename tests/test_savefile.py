import ctypes
import random
import shutil
import struct
import subprocess
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
KAOS = ROOT / "game" / "KAOS.FTL"
SAVE_BLOCK_XOR, SAVE_READ_CHECKED_BLOCK, SAVE_READ_HEADER = 0x125510, 0x125576, 0x125766
G_LOAD_FROM_MEMORY, G_LOAD_MEMORY_POINTER, G_LOAD_MEMORY_REMAINING = 0x1040FC, 0x1040FE, 0x104102
PORTRAITS = 4 * 464
MAX_GROUPS, EVENTS = 7, 13
SIZES = [0x80, MAX_GROUPS * 16, 0x580, EVENTS * 10, EVENTS * 2]

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")
u8p = ctypes.POINTER(ctypes.c_uint8)


class SaveFile(ctypes.Structure):
    _fields_ = [
        ("data", u8p),
        ("header", u8p),
        ("block", u8p * 5),
        ("block_size", ctypes.c_uint16 * 5),
        ("portraits", u8p),
        ("dungeon", u8p),
        ("dungeon_size", ctypes.c_uint32),
    ]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.save_decode.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(SaveFile)]
    lib.save_encode.argtypes = [ctypes.POINTER(SaveFile), ctypes.POINTER(ctypes.c_uint32)]
    lib.save_encode.restype = ctypes.c_void_p
    lib.save_free.argtypes = [ctypes.POINTER(SaveFile)]
    lib.free.argtypes = [ctypes.c_void_p]
    return lib


def plain_parts(seed=1, platform=5, fmt=3):
    rnd = random.Random(seed)
    header = bytearray(rnd.randbytes(512))
    header[0x12A], header[0x12B] = 1, platform
    struct.pack_into(">HHH", header, 0x176, fmt, 10, 0)
    blocks = [bytearray(rnd.randbytes(n)) for n in SIZES]
    struct.pack_into(">H", blocks[0], 28, EVENTS)
    struct.pack_into(">H", blocks[0], 46, MAX_GROUPS)
    return header, blocks, rnd.randbytes(PORTRAITS), rnd.randbytes(300)


def buf(data):
    return (ctypes.c_uint8 * len(data)).from_buffer_copy(bytes(data))


def encode(lib, header, blocks, portraits, dungeon, sizes=SIZES):
    keep = [buf(header), *map(buf, blocks), buf(portraits), buf(dungeon)]
    s = SaveFile()
    s.header = ctypes.cast(keep[0], u8p)
    for i in range(5):
        s.block[i] = ctypes.cast(keep[1 + i], u8p)
        s.block_size[i] = sizes[i]
    s.portraits, s.dungeon = ctypes.cast(keep[6], u8p), ctypes.cast(keep[7], u8p)
    s.dungeon_size = len(dungeon)
    size = ctypes.c_uint32()
    p = lib.save_encode(ctypes.byref(s), ctypes.byref(size))
    if not p:
        return None
    out = ctypes.string_at(p, size.value)
    lib.free(p)
    return out


def decode(lib, data):
    s = SaveFile()
    if lib.save_decode(data, len(data), ctypes.byref(s)) != 1:
        return None
    r = {
        "header": bytes(s.header[:512]),
        "blocks": [bytes(s.block[i][: s.block_size[i]]) for i in range(5)],
        "portraits": bytes(s.portraits[:PORTRAITS]),
        "dungeon": bytes(s.dungeon[: s.dungeon_size]),
    }
    lib.save_free(ctypes.byref(s))
    return r


def test_round_trip(lib):
    header, blocks, portraits, dungeon = plain_parts()
    file = encode(lib, header, blocks, portraits, dungeon)
    assert len(file) == 512 + sum(SIZES) + PORTRAITS + len(dungeon)
    r = decode(lib, file)
    assert r["blocks"] == [bytes(b) for b in blocks]
    assert r["portraits"] == portraits and r["dungeon"] == dungeon
    # the encoder sets only the last chain word (0x0fe) and the 5 checksums (0x156)
    changed = {i for i in range(512) if r["header"][i] != header[i]}
    assert changed <= {0xFE, 0xFF, *range(0x156, 0x160)}
    # the header is not stored plain
    assert file[0x100:0x200] != r["header"][0x100:0x200]


def test_encode_matches_original(lib):
    if not KAOS.exists():
        pytest.skip("missing game/KAOS.FTL")
    header, blocks, portraits, dungeon = plain_parts(seed=7)
    file = encode(lib, header, blocks, portraits, dungeon)
    plain = decode(lib, file)["header"]
    emu = Emu(load_image(parse(KAOS.read_bytes()), 0x100000))
    scratch, filebuf = 0x300000, 0x400000

    # save_block_xor on the plain bytes gives the file bytes and the stored checksum
    pos = 512
    for i, b in enumerate(blocks):
        key, checksum = struct.unpack_from(">H", plain, 0x136 + 2 * i)[0], struct.unpack_from(">H", plain, 0x156 + 2 * i)[0]
        emu.write(scratch, b)
        assert emu.call(SAVE_BLOCK_XOR, [(4, scratch), (2, key), (2, len(b) // 2)]) & 0xFFFF == checksum
        assert emu.read(scratch, len(b)) == file[pos : pos + len(b)]
        pos += len(b)

    # the original read path accepts the file: header chain check, then each checked block
    emu.write(filebuf, file)
    emu.write(G_LOAD_FROM_MEMORY, struct.pack(">HII", 1, filebuf, len(file)))
    assert emu.call(SAVE_READ_HEADER, [(4, scratch)]) & 0xFFFF
    assert emu.read(scratch, 512) == plain
    for i, b in enumerate(blocks):
        key, checksum = struct.unpack_from(">H", plain, 0x136 + 2 * i)[0], struct.unpack_from(">H", plain, 0x156 + 2 * i)[0]
        assert emu.call(SAVE_READ_CHECKED_BLOCK, [(4, scratch), (2, len(b)), (2, key), (2, checksum)]) & 0xFFFF == 1
        assert emu.read(scratch, len(b)) == bytes(b)


def test_encode_rejects_wrong_sizes(lib):
    parts = plain_parts()
    assert encode(lib, *parts, sizes=[0x80, MAX_GROUPS * 16 + 2, 0x580, EVENTS * 10, EVENTS * 2]) is None


def flip(data, offset, plain=None):
    # the block sum adds plain and cipher words: flip a bit where the key bit is 0, so both change the same way
    key = plain[offset] ^ data[offset] if plain else 0
    b = bytearray(data)
    b[offset] ^= next(1 << i for i in range(8) if not key >> i & 1)
    return bytes(b)


def plain_file(lib, file):
    r = decode(lib, file)
    return r["header"] + b"".join(r["blocks"]) + r["portraits"] + r["dungeon"]


def test_malformed_fails(lib):
    file = encode(lib, *plain_parts())
    dungeon_start = 512 + sum(SIZES) + PORTRAITS
    for n in [0, 1, 511, 512, 600, 512 + 0x80, dungeon_start - 1]:
        assert decode(lib, file[:n]) is None, n
    assert decode(lib, file[:dungeon_start])["dungeon"] == b""
    plain = plain_file(lib, file)
    for offset in [0x00, 0x14, 0x100, 0x1FF, 512, 512 + 0x80, 512 + 0x80 + MAX_GROUPS * 16 + 0x580, dungeon_start - PORTRAITS - 1]:
        assert decode(lib, flip(file, offset, plain)) is None, hex(offset)
    assert decode(lib, encode(lib, *plain_parts(fmt=2))) is None
    assert decode(lib, encode(lib, *plain_parts(platform=1))) is None
    rnd = random.Random(3)
    for n in [512, 4096, 20000]:
        assert decode(lib, rnd.randbytes(n)) is None


def test_huge_counts_fail(lib):
    header, blocks, portraits, dungeon = plain_parts()
    struct.pack_into(">H", blocks[0], 46, 0x0FFF)
    struct.pack_into(">H", blocks[0], 28, 0x0CCC)
    sizes = [0x80, 0xFFF0, 0x580, 0x7FF8, 0x1998]
    blocks = [blocks[0], bytes(sizes[1]), blocks[2], bytes(sizes[3]), bytes(sizes[4])]
    file = encode(lib, header, blocks, portraits, dungeon, sizes)
    assert decode(lib, file) is not None
    assert decode(lib, file[:0x10000]) is None


def test_champion_checksum_is_not_checked(lib):
    # load_game ignores the result of the champions block check
    file = encode(lib, *plain_parts())
    champions = 512 + 0x80 + MAX_GROUPS * 16
    assert decode(lib, flip(file, champions)) is not None
