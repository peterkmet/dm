# SPDX-License-Identifier: GPL-3.0-or-later
"""The save file codec of the library: the SaveFile structure, encode and decode, and the original read path."""

import ctypes

SAVE = "DMGAME.DAT"  # the save file of Dungeon Master in the data folder
# Original addresses (docs/reference/kaos-symbols.csv).
SAVE_BLOCK_XOR, SAVE_READ_CHECKED_BLOCK, SAVE_READ_HEADER = 0x125510, 0x125576, 0x125766
G_LOAD_FROM_MEMORY, G_LOAD_MEMORY_POINTER, G_LOAD_MEMORY_REMAINING = 0x1040FC, 0x1040FE, 0x104102
PORTRAITS = 4 * 464
MAX_GROUPS, EVENTS = 7, 13
SIZES = [0x80, MAX_GROUPS * 16, 0x580, EVENTS * 10, EVENTS * 2]

u8p = ctypes.POINTER(ctypes.c_uint8)


class SaveFile(ctypes.Structure):
    _fields_ = [
        ("data", u8p),
        ("owns_data", ctypes.c_bool),
        ("header", u8p),
        ("block", u8p * 5),
        ("block_size", ctypes.c_uint16 * 5),
        ("portraits", u8p),
        ("dungeon", u8p),
        ("dungeon_size", ctypes.c_uint32),
    ]


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
    p = lib.savefile_encode(ctypes.byref(s), ctypes.byref(size))
    if not p:
        return None
    out = ctypes.string_at(p, size.value)
    lib.free(p)
    return out


def decode(lib, data):
    s = SaveFile()
    if lib.savefile_decode(data, len(data), ctypes.byref(s)) != 1:
        return None
    r = {
        "header": bytes(s.header[:512]),
        "blocks": [bytes(s.block[i][: s.block_size[i]]) for i in range(5)],
        "portraits": bytes(s.portraits[:PORTRAITS]),
        "dungeon": bytes(s.dungeon[: s.dungeon_size]),
    }
    lib.savefile_free(ctypes.byref(s))
    return r
