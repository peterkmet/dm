# SPDX-License-Identifier: GPL-3.0-or-later
"""The PNG screenshot: valid chunks, valid zlib data, one RGB row per screen row."""

import ctypes
import struct
import zlib

from dmtest.lib import load_lib
from dmtest import constants as C

lib = load_lib()
lib.png_write.restype = ctypes.c_bool
lib.png_write.argtypes = [ctypes.c_char_p]


def chunks(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    at = 8
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        (crc,) = struct.unpack(">I", data[at + 8 + length:at + 12 + length])
        assert crc == zlib.crc32(kind + body)
        yield kind, body
        at += 12 + length


def test_png_of_the_screen(tmp_path):
    path = tmp_path / "screen.png"
    assert lib.png_write(str(path).encode())
    found = dict(chunks(path.read_bytes()))
    assert list(found) == [b"IHDR", b"IDAT", b"IEND"]
    width, height, depth, color = struct.unpack(">IIBB", found[b"IHDR"][:10])
    assert (width, height, depth, color) == (C.SCREEN_W, C.SCREEN_H, 8, 2)
    raw = zlib.decompress(found[b"IDAT"])
    assert len(raw) == (1 + 3 * C.SCREEN_W) * C.SCREEN_H
    assert all(raw[y * (1 + 3 * C.SCREEN_W)] == 0 for y in range(C.SCREEN_H))  # filter byte: none


def test_png_to_a_missing_folder(tmp_path):
    assert not lib.png_write(str(tmp_path / "missing" / "screen.png").encode())
