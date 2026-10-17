# SPDX-License-Identifier: GPL-3.0-or-later
"""The CSB engine image: APPB.FTL tables at their KAOS addresses (docs/csb/engine-diff.md "Data tables")."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib
from dmtest.markers import GAME, GAME_CSB, needs_csb, needs_dm

from dmtools import ftl

BASE = 0x100000
DM, CSB = 0, 1
c16 = ctypes.c_int16

# KAOS addresses of the tables with CSB values or converted layouts
CREDITS_PALETTE, ENTRANCE_PALETTE = 0x100A46, 0x100A66
ENTRANCE_MOUSE, ENDGAME_MOUSE, ENTRANCE_KEYS = 0x10318C, 0x1031D4, 0x10393C
DIALOG_MOUSE_INPUTS, DIALOG_TABLES = 0x103BD4, 0x103A84
ACTION_SETS, APPB_ACTION_SETS = 0x103D4C, 0x1047AC
EXPERIENCE_GAIN, VERSION_TEXT = 0x103FB4, 0x10402C
DM_SPELLS = 1064068
DIFFERENT = {CREDITS_PALETTE, ENTRANCE_PALETTE, ENTRANCE_MOUSE, ENDGAME_MOUSE, ENTRANCE_KEYS, EXPERIENCE_GAIN, VERSION_TEXT, C.CSB_SPELLS, C.CSB_RESTART_BOXES}

pytestmark = [needs_csb]


class CsbCopy(ctypes.Structure):
    _fields_ = [("kaos", ctypes.c_uint32), ("appb", ctypes.c_uint32), ("size", ctypes.c_uint16)]


def image(path):
    return ftl.load_image(ftl.parse(path.read_bytes()), BASE).data


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.layout_get_zone.restype = ctypes.c_bool
    lib.view_render.restype = ctypes.c_bool
    lib.layout_init.restype = ctypes.c_bool
    lib.kaos_ptr.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.spell_find.restype = c16
    lib.tables_get_spell.restype = ctypes.c_void_p
    yield lib
    set_adventure(lib, DM)


def set_adventure(lib, adventure):
    c16.in_dll(lib, "g_adventure").value = adventure


def load(lib, adventure, path):
    set_adventure(lib, adventure)
    assert lib.tables_load(str(path).encode())
    return lambda addr, n: bytes(lib.kaos_ptr(addr)[:n])


def copies(lib):
    count = ctypes.c_int.in_dll(lib, "g_csb_copy_count").value
    return list((CsbCopy * count).in_dll(lib, "g_csb_copies"))


class FtlImage(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_uint32), ("base", ctypes.c_uint32), ("data_base", ctypes.c_uint32), ("code_base", ctypes.c_uint32)]


# file-local constant of src/data/csb_text.c: the pointers to the load, save, and disk messages
CSB_APPB_DISK_MESSAGES = 0x104D08


def test_failed_text_build_publishes_no_image(lib):
    """A CSB image whose text build fails is freed and not published (bug: the image stayed loaded)."""
    lib.ftl_load.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint32, ctypes.POINTER(FtlImage)]
    lib.csb_image_build.argtypes = [ctypes.POINTER(FtlImage), ctypes.POINTER(FtlImage)]
    lib.ftl_free.argtypes = [ctypes.POINTER(FtlImage)]
    raw = (GAME_CSB / "APPB.FTL").read_bytes()
    appb = FtlImage()
    assert lib.ftl_load(raw, len(raw), BASE, ctypes.byref(appb))
    at = CSB_APPB_DISK_MESSAGES - appb.base
    ctypes.memmove(ctypes.addressof(appb.data.contents) + at, b"\xff\xff\xff\xff", 4)  # a pointer outside the image
    out = FtlImage()
    assert not lib.csb_image_build(ctypes.byref(appb), ctypes.byref(out))
    assert not out.data
    lib.ftl_free(ctypes.byref(appb))


@pytest.fixture(scope="module")
def appb():
    data = image(GAME_CSB / "APPB.FTL")
    return lambda addr, n: data[addr - BASE : addr - BASE + n]


@pytest.fixture(scope="module")
def kaos():
    if not (GAME / "KAOS.FTL").exists():
        return None
    data = image(GAME / "KAOS.FTL")
    return lambda addr, n: data[addr - BASE : addr - BASE + n]


def test_copied_tables_equal_appb(lib, appb):
    csb = load(lib, CSB, GAME_CSB / "APPB.FTL")
    for c in copies(lib):
        assert csb(c.kaos, c.size) == appb(c.appb, c.size), hex(c.kaos)


@needs_dm
def test_equal_tables_match_dm(lib, kaos):
    csb = load(lib, CSB, GAME_CSB / "APPB.FTL")
    for c in copies(lib):
        if c.kaos not in DIFFERENT:
            assert csb(c.kaos, c.size) == kaos(c.kaos, c.size), hex(c.kaos)
    assert csb(C.CSB_SPELLS, 25 * 8) == kaos(DM_SPELLS, 25 * 8)


def test_converted_action_sets(lib, appb):
    csb = load(lib, CSB, GAME_CSB / "APPB.FTL")
    for i in range(44):
        k, a = csb(ACTION_SETS + 8 * i, 8), appb(APPB_ACTION_SETS + 6 * i, 6)
        assert k[:3] + k[4:7] == a and k[3] == k[7] == 0


@needs_dm
def test_converted_dialog_pointers(lib, kaos):
    csb = load(lib, CSB, GAME_CSB / "APPB.FTL")
    assert csb(DIALOG_MOUSE_INPUTS, 32) == kaos(DIALOG_MOUSE_INPUTS, 32)
    assert csb(DIALOG_TABLES, 336) == kaos(DIALOG_TABLES, 336)


def spell_runes(appb, index):
    """The lowest power symbol, then the symbols of CSB spell index."""
    return b"\x60" + appb(0x1046C2 + 8 * index + 1, 3) + b"\0"


def test_csb_spells(lib, appb):
    load(lib, CSB, GAME_CSB / "APPB.FTL")
    assert lib.tables_get_spell(28) and not lib.tables_get_spell(29)
    assert [lib.spell_find(spell_runes(appb, i)) for i in range(25, 29)] == [25, 26, 27, 28]
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    assert lib.tables_get_spell(28) and not lib.tables_get_spell(29)


@needs_dm
def test_dm_spells(lib, appb):
    load(lib, DM, GAME / "KAOS.FTL")
    assert lib.tables_get_spell(24) and not lib.tables_get_spell(25)
    assert lib.spell_find(spell_runes(appb, 25)) == -1

