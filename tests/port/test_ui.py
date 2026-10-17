# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import struct

import pytest

from dmtest.game import Box, setup_ui, start_ui, zone_records
from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm, needs_sdl

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

GAME = ROOT / "game"
GET_ZONE_BOX_CLIPPED, COMMAND_FROM_MOUSE, LAYOUT_ZONES = 0x11686A, 0x120CC6, 0x102990
MOUSE_TABLES = [0x1031F8, 0x1032E8, 0x103354, 0x10351C, 0x103540, 0x103564, 0x1035A0, 0x1035DC, 0x103654, 0x1036F0, 0x10375C, 0x10378C]
DUNGEON_PALETTE = 0x100AA6
ATTR_ALL = 0xFF80
pytestmark = [needs_sdl]


@pytest.fixture(scope="module")
def lib():
    return setup_ui(load_lib())


@pytest.fixture(scope="module")
def game(lib):
    return start_ui(lib)


@pytest.fixture(scope="module")
def emu(game):
    """The original code with the layout zones of Graphics.DAT in memory."""
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=50_000_000)
    size = ctypes.c_uint16()
    game.gfx_get_item.restype = ctypes.POINTER(ctypes.c_uint8)
    raw = ctypes.string_at(game.gfx_get_item(696, ctypes.byref(size)), size.value)
    chunks, at = zone_records(raw), 0x700000
    emu.write(LAYOUT_ZONES, struct.pack(">I", at))
    for i, (first, last, recs) in enumerate(chunks):
        nxt = at + 8 + len(recs) if i + 1 < len(chunks) else 0
        emu.write(at, struct.pack(">Ihh", nxt, first, last) + recs)
        at = nxt
    emu.zones = [z for first, last, _ in chunks for z in range(first, last + 1)]
    return emu


@needs_dm
def test_zone_box_matches_original(game, emu):
    checked = 0
    for zone in emu.zones:
        for wp, hp in ((10000, 10000), (5000, 10000), (2500, 7000)):
            emu.write(0x6F0000, bytes(8))
            found = emu.call(GET_ZONE_BOX_CLIPPED, [(2, zone), (4, 0x6F0000), (2, wp), (2, hp)]) & 0xFFFFFFFF
            box = Box()
            ok = game.layout_get_zone_box(zone, wp, hp, ctypes.byref(box))
            assert bool(ok) == bool(found), zone
            if ok:
                assert (box.x, box.y, box.w, box.h) == struct.unpack(">4h", emu.read(0x6F0000, 8)), (zone, wp, hp)
                checked += 1
    assert checked > 300


def table_points(emu, table):
    """Points around every box of a mouse input table, plus a coarse grid of the screen."""
    pts = {(x, y) for x in range(0, 320, 9) for y in range(0, 200, 7)}
    a = table
    while (entry := struct.unpack(">6h", emu.read(a, 12)))[0]:
        a += 12
        box = Box()
        if entry[1] >= 0:
            x1, x2, y1, y2 = entry[1:5]
        elif emu_zone_box(emu, entry[2], box):
            dx, dy = (0, 33) if entry[1] == -2 else (48, 32) if entry[1] == -3 else (0, 0)
            x1, x2, y1, y2 = box.x + dx, box.x + dx + box.w - 1, box.y + dy, box.y + dy + box.h - 1
        else:
            continue
        pts |= {(x, y) for x in (x1 - 1, x1, x2, x2 + 1) for y in (y1 - 1, y1, (y1 + y2) // 2, y2, y2 + 1)}
    return sorted(p for p in pts if 0 <= p[0] < 320 and 0 <= p[1] < 200)


def emu_zone_box(emu, zone, box):
    emu.write(0x6F0000, bytes(8))
    if not emu.call(GET_ZONE_BOX_CLIPPED, [(2, zone), (4, 0x6F0000), (2, 10000), (2, 10000)]) & 0xFFFFFFFF:
        return False
    box.x, box.y, box.w, box.h = struct.unpack(">4h", emu.read(0x6F0000, 8))
    return True


@needs_dm
@pytest.mark.parametrize("table", range(len(MOUSE_TABLES)))
def test_mouse_commands_match_original(game, emu, table):
    hits = 0
    for x, y in table_points(emu, MOUSE_TABLES[table]):
        for mask in (1, 2):
            want = emu.call(COMMAND_FROM_MOUSE, [(4, MOUSE_TABLES[table]), (2, x), (2, y), (2, mask)]) & 0xFFFF
            assert game.ui_command_from_mouse(table, x, y, mask) == ctypes.c_int16(want).value, (table, x, y, mask)
            hits += want != 0
    assert hits > 0

