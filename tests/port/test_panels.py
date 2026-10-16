# SPDX-License-Identifier: GPL-3.0-or-later
"""Inventory panel: object description text against the original, chest open/close/swap, PNG renders."""

import ctypes
import struct

import pytest

from dmtest.game import Bitmap, Box, Champion, recruit_from_mirrors, render, reset, setup_ui, start_ui
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

import dmtools.emu as emu_module
from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

u16, c16 = ctypes.c_uint16, ctypes.c_int16
# Original addresses (docs/reference/kaos-symbols.csv).
DRAW_PANEL_OBJECT, PANEL_PRINT_LINE, PRINT_ZONE = 0x10F28A, 0x10EC88, 0x109A04
STUBS = (0x1072C6, 0x10EC40, 0x10EFAC)  # blit_graphic_to_viewport_zone, ui_draw_icon_to_viewport, draw_panel_arrow_or_eye
THING_DATA, STRINGS, STRING_COUNT, OBJECT_NAMES = 0x101CEA, 0x101E58, 0x101E52, 0x100266
CHAMPIONS, INVENTORY_ORDINAL, COUNT, DIRECTION = 0x101F54, 0x101EA8, 0x1001E8, 0x1001EA
RTS = b"\x4e\x75"
THING_BUF, TEXT_BUF = 0x300000, 0x340000
OBJECT_TYPES = (5, 6, 8, 10)  # weapon, armour, potion, junk
RECORD_SIZE = {5: 4, 6: 4, 7: 4, 8: 4, 9: 8, 10: 4}
SCROLL, CHEST, NONE, END = 7, 9, 0xFFFF, 0xFFFE
SLOT_BOX_CHEST_FIRST, CHEST_SLOTS, VIEWPORT_Y = 38, 8, 33
PRIEST = 2


@pytest.fixture(scope="module")
def lib():
    return setup_ui(load_lib())


@pytest.fixture(scope="module")
def game(lib):
    return start_ui(lib)


def setup(lib):
    lib.panel_draw_object_text.argtypes = [u16, ctypes.c_char_p, ctypes.c_char_p]
    lib.panel_draw_object_text.restype = c16
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.rec_write.argtypes = [u16]
    lib.rec_thing_next.restype = u16
    lib.rec_thing_next.argtypes = [u16]
    lib.gfx_get_string.restype = lib.gfx_get_object_name.restype = ctypes.c_char_p
    lib.gfx_get_string.argtypes = lib.gfx_get_object_name.argtypes = [c16]
    lib.object_get_icon_index.argtypes = [u16]
    lib.object_get_icon_index.restype = c16
    lib.champion_pack.argtypes = [ctypes.POINTER(Champion), ctypes.c_char_p]
    lib.rec_find_unused_thing.restype = u16
    lib.rec_find_unused_thing.argtypes = [u16]
    lib.inventory_get_object_removed_from_slot.argtypes = [u16, u16]
    lib.panel_click_in_chest.argtypes = [c16, c16]
    lib.ui_get_slot_box_zone.argtypes = [c16, ctypes.c_int, ctypes.POINTER(Box)]
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool


def things_of(lib, t):
    """Every used record of thing type t in the dungeon."""
    header = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_dungeon_header"), ctypes.POINTER(ctypes.c_uint8))
    count = header[12 + 2 * t] << 8 | header[13 + 2 * t]
    return [t << 10 | i for i in range(count) if bytes(lib.rec_write(t << 10 | i)[:2]) != b"\xff\xff"], count


def chest_content(lib, chest):
    rec = lib.rec_write(chest)
    out, t = [], rec[2] << 8 | rec[3]
    while t != END:
        out.append(t)
        t = lib.rec_thing_next(t)
    return out


def port_text(lib, thing):
    name, lines = ctypes.create_string_buffer(40), ctypes.create_string_buffer(3 * 40)
    n = lib.panel_draw_object_text(thing, name, lines)
    return name.value, tuple(lines.raw[40 * i:40 * (i + 1)].split(b"\0")[0] for i in range(n))


class Original:
    """ui_draw_panel_object of KAOS with the dungeon things, strings, and champions of the port; the
    name print and panel_print_line record their text, the graphics calls return at once."""

    def __init__(self, lib):
        self.lib = lib
        self.emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=5_000_000)
        emu, traps = self.emu, emu_module._machine.traps
        self.printed, self.trap_ids = [], [traps.alloc(self.print_line), traps.alloc(self.print_name)]
        for addr in STUBS:
            emu.write(addr, RTS)
        emu.write(PANEL_PRINT_LINE, struct.pack(">H", 0xA000 | self.trap_ids[0]) + RTS)
        emu.write(PRINT_ZONE, struct.pack(">H", 0xA000 | self.trap_ids[1]) + RTS)
        addr = THING_BUF
        for t in RECORD_SIZE:
            _, count = things_of(lib, t)
            emu.write(addr, ctypes.string_at(lib.rec_write(t << 10), count * RECORD_SIZE[t]))
            emu.write(THING_DATA + 4 * t, struct.pack(">I", addr))
            addr += 0x2000
        at = TEXT_BUF
        emu.write(STRING_COUNT, struct.pack(">h", 128))
        for table, get, n in ((STRINGS, lib.gfx_get_string, 128), (OBJECT_NAMES, lib.gfx_get_object_name, 256)):
            emu.write(table, struct.pack(">I", at))
            ptrs, at = at, at + 4 * n
            for i in range(n):
                emu.write(ptrs + 4 * i, struct.pack(">I", at))
                s = get(i) + b"\0"
                emu.write(at, s)
                at += len(s)

    def close(self):
        for i in self.trap_ids:
            emu_module._machine.traps.free(i)

    def string(self, addr):
        out = bytearray()
        while (b := self.emu.mem.r8(addr + len(out))) != 0:
            out.append(b)
        return bytes(out)

    def print_line(self, op, pc):
        self.printed.append(("line", self.string(self.emu.mem.r32(self.emu.reg("A7") + 4))))

    def print_name(self, op, pc):
        self.printed.append(("name", self.string(self.emu.mem.r32(self.emu.reg("A7") + 8))))

    def sync(self, direction):
        """Champions, inventory champion 1, and the party direction of the port."""
        lib, emu = self.lib, self.emu
        champions = (Champion * 4).in_dll(lib, "g_champions")
        for i in range(4):
            buf = ctypes.create_string_buffer(0x140)
            lib.champion_pack(ctypes.byref(champions[i]), buf)
            emu.write(CHAMPIONS + 0x140 * i, buf.raw)
        emu.write(COUNT, struct.pack(">H", 4))
        emu.write(INVENTORY_ORDINAL, struct.pack(">h", 1))
        emu.write(DIRECTION, struct.pack(">h", direction))

    def set_byte(self, thing, offset, value):
        t, i = thing >> 10 & 15, thing & 0x3FF
        self.emu.write(self.emu.mem.r32(THING_DATA + 4 * t) + i * RECORD_SIZE[t] + offset, bytes([value]))

    def text(self, thing):
        self.printed = []
        self.emu.call(DRAW_PANEL_OBJECT, [(2, thing), (2, 1)])
        names = [s for kind, s in self.printed if kind == "name"]
        lines = [s for kind, s in self.printed if kind == "line" and s not in (b"", b"\f")]
        assert len(names) == 1
        return names[0], tuple(lines)


@pytest.fixture
def party(game):
    setup(game)
    reset(game)
    champions = recruit_from_mirrors(game, 4)
    c16.in_dll(game, "g_inventory_champion_ordinal").value = 1
    yield champions
    c16.in_dll(game, "g_inventory_champion_ordinal").value = 0


@needs_dm
def test_object_text_matches_original(game, party):
    original = Original(game)
    try:
        checked = set()
        for priest_experience in (0, 1 << 20):
            for s in (PRIEST, 12, 13, 14, 15):  # the priest skill and its hidden skills
                party[0].skills[s].experience = priest_experience
            for direction in range(4):
                c16.in_dll(game, "g_party_direction").value = direction
                original.sync(direction)
                for t in OBJECT_TYPES:
                    for thing in things_of(game, t)[0]:
                        assert port_text(game, thing) == original.text(thing), hex(thing)
                        checked.add(port_text(game, thing))
        # flags that the dungeon objects rarely have: poisoned, broken, cursed, no charges (burnt out torch)
        for t, flags in ((5, 0x43), (6, 0x21)):
            for thing in things_of(game, t)[0]:
                rec = game.rec_write(thing)
                saved = rec[2]
                for value in (saved | flags, saved | flags & 0x41, saved | 0x01, saved & ~0x3C):
                    rec[2] = value
                    original.set_byte(thing, 2, rec[2])
                    assert port_text(game, thing) == original.text(thing), hex(thing)
                    checked.add(port_text(game, thing))
                rec[2] = saved
                original.set_byte(thing, 2, saved)
    finally:
        original.close()
        c16.in_dll(game, "g_party_direction").value = 0
    lines = {line for _, ls in checked for line in ls}
    for want in (b"(CONSUMABLE)", b"(POISONED, BROKEN AND CURSED)", b"(BROKEN AND CURSED)", b"(CURSED)",
                 b"(BURNT OUT)", b"PARTY FACING NORTH", b"PARTY FACING WEST"):
        assert want in lines, want
    assert any(name[1:2] == b" " and ord("_") <= name[0] <= ord("e") for name, _ in checked)  # potion power rune


def put_in_action_hand(game, champions, thing):
    champions[0].slots[1] = thing
    champions[0].attributes |= 0x0800  # panel
    game.ui_draw_champion_state(0)


@needs_dm
def test_chest_open_swap_close(game, party):
    chests = [c for c in things_of(game, CHEST)[0] if 2 <= len(chest_content(game, c)) < CHEST_SLOTS]
    assert chests
    chest = chests[0]
    content = chest_content(game, chest)
    open_chest = u16.in_dll(game, "g_open_chest")
    slots = (u16 * CHEST_SLOTS).in_dll(game, "g_open_chest_slots")
    render(game, "base")
    game.ui_draw_inventory_open(0)
    put_in_action_hand(game, party, chest)
    assert c16.in_dll(game, "g_panel_content").value == 6
    assert open_chest.value == chest
    assert list(slots) == content + [NONE] * (CHEST_SLOTS - len(content))

    hand = game.rec_find_unused_thing(5)
    game.inventory_put_object_in_leader_hand(hand)
    box = Box()
    assert game.ui_get_slot_box_zone(SLOT_BOX_CHEST_FIRST, 0, ctypes.byref(box))
    game.panel_click_in_chest(box.x + box.w // 2, box.y + box.h // 2 + VIEWPORT_Y)
    assert u16.in_dll(game, "g_leader_hand_object").value == content[0]
    assert slots[0] == hand
    assert slots[len(content)] == NONE
    assert game.ui_get_slot_box_zone(SLOT_BOX_CHEST_FIRST + len(content), 0, ctypes.byref(box))
    game.panel_click_in_chest(box.x + box.w // 2, box.y + box.h // 2 + VIEWPORT_Y)  # the hand object goes to an empty slot
    assert slots[len(content)] == content[0]
    assert u16.in_dll(game, "g_leader_hand_object").value == NONE

    game.ui_draw_inventory_close(0)
    assert open_chest.value == NONE and list(slots) == [NONE] * CHEST_SLOTS
    assert chest_content(game, chest) == [hand] + content[1:] + [content[0]]

    game.ui_draw_inventory_open(0)
    assert open_chest.value == chest
    game.inventory_get_object_removed_from_slot(0, 1)
    assert open_chest.value == NONE
    assert chest_content(game, chest) == [hand] + content[1:] + [content[0]]
    game.ui_draw_inventory_close(0)


@needs_dm
def test_render_scroll_and_eye(game, party):
    scrolls = things_of(game, SCROLL)[0]
    assert scrolls
    render(game, "base")
    game.ui_draw_inventory_open(0)
    put_in_action_hand(game, party, scrolls[0])
    assert c16.in_dll(game, "g_panel_content").value == 5
    sword = next(t for t in things_of(game, 5)[0] if game.gfx_get_object_name(game.object_get_icon_index(t)) == b"SWORD")
    game.inventory_put_object_in_leader_hand(sword)
    game.ui_draw_panel_eye()
    game.ui_draw_viewport()
    name, lines = port_text(game, sword)
    assert name == b"SWORD" and lines[-1].startswith(b"WEIGHS ")
    game.inventory_get_object_removed_from_leader_hand()
    party[0].slots[1] = NONE
    game.ui_draw_inventory_close(0)


@needs_dm
def test_eat_food_redraws_the_food_bar(game, party):
    game.tables_get_object_info.restype = ctypes.POINTER(u16 * 3)
    junk = next(j for j in range(53) if 0xA8 <= game.tables_get_object_info(0x7F + j).contents[0] < 0xB0)
    thing = game.rec_find_unused_thing(10)
    game.rec_junk_set_type(thing, junk)
    party[0].food = 100
    game.ui_draw_inventory_open(0)
    c16.in_dll(game, "g_panel_content").value = 1  # food and water
    game.ui_draw_panel()
    game.inventory_put_object_in_leader_hand(thing)
    game.ui_get_viewport.restype = ctypes.POINTER(Bitmap)
    viewport = game.ui_get_viewport().contents
    before = viewport.px[: viewport.w * viewport.h]
    game.command_click_on_mouth()
    assert viewport.px[: viewport.w * viewport.h] != before
    assert not party[0].attributes & 0x0900  # statistics and panel drawn
    game.ui_draw_inventory_close(0)
