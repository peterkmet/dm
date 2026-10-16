# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import random
import struct

import pytest

from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm
from dmtest.savefile import (EVENTS, G_LOAD_FROM_MEMORY, MAX_GROUPS, PORTRAITS, SAVE_BLOCK_XOR, SAVE_READ_CHECKED_BLOCK, SAVE_READ_HEADER,
                             SIZES, SaveFile, decode, encode)

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

KAOS = ROOT / "game" / "KAOS.FTL"

pytestmark = [needs_dm]


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    lib.savefile_decode.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(SaveFile)]
    lib.savefile_decode.restype = ctypes.c_bool
    lib.savefile_encode.argtypes = [ctypes.POINTER(SaveFile), ctypes.POINTER(ctypes.c_uint32)]
    lib.savefile_encode.restype = ctypes.c_void_p
    lib.savefile_free.argtypes = [ctypes.POINTER(SaveFile)]
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
    header, blocks, portraits, dungeon = plain_parts(seed=7)
    file = encode(lib, header, blocks, portraits, dungeon)
    plain = decode(lib, file)["header"]
    emu = Emu(load_image(parse(KAOS.read_bytes()), 0x100000))
    scratch, filebuf = 0x300000, 0x400000

    # savefile_xor_block on the plain bytes gives the file bytes and the stored checksum
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
