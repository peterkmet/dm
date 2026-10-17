# SPDX-License-Identifier: GPL-3.0-or-later
"""CSB text from APPB.FTL and Graphics.DAT item 694 (docs/csb/engine-diff.md "Text")."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib
from dmtest.markers import GAME, GAME_CSB, needs_csb, needs_dm


DM, CSB = 0, 1
TEXT_COUNT, OBJECT_NAME_COUNT = 135, 199
SKILL_LEVEL_STRINGS, BASE_SKILL_STRINGS, STATISTIC_STRINGS, DIRECTION_STRINGS = 0x101E74, 0x102472, 0x101E9A, 0x101E92
c16 = ctypes.c_int16

pytestmark = [needs_csb]

# DM text index -> CSB string, one or more per group of the doc table
EXPECTED = {
    4: "NEOPHYTE", 18: "ARCHMASTER", 19: "WAKE UP", 20: "GAME FROZEN", 21: "PARTY FACING ",
    26: "CONSUMABLE", 27: "POISONED", 28: "BROKEN", 29: "CURSED", 30: " AND ", 31: "(BURNT OUT)",
    32: "(EMPTY)", 33: "(ALMOST EMPTY)", 34: "(ALMOST FULL)", 35: "(FULL)",
    36: "LOAD ", 37: "WEIGHS ", 38: ".", 39: " KG.", 40: " KG", 41: "L", 47: "ANTI-FIRE",
    48: "HEALTH", 49: "STAMINA", 50: "MANA",
    51: "", 52: " NEEDS MORE PRACTICE WITH THIS WIZARD SPELL.", 54: " NEEDS MORE PRACTICE WITH THIS PRIEST SPELL.",
    56: " MUMBLES A MEANINGLESS SPELL.", 58: " NEEDS AN EMPTY FLASK IN HAND FOR POTION.",
    60: " NEEDS A MAGIC MAP IN ACTION HAND FOR THIS SPELL.",
    61: "", 62: " JUST GAINED A FIGHTER LEVEL!", 64: " JUST GAINED A NINJA LEVEL!",
    66: " JUST GAINED A PRIEST LEVEL!", 68: " JUST GAINED A WIZARD LEVEL!",
    69: "IT COMES UP HEADS.", 70: "IT COMES UP TAILS.", 71: "CAN'T REACH", 72: "NEED AMMO",
    73: "", 74: " RESURRECTED.", 76: " REINCARNATED.", 77: "RESTART THIS GAME",
    78: "GAME LOADED, READY TO PLAY.", 80: "CAN'T FIND SAVED GAME!", 81: "UNABLE TO SAVE GAME!",
    86: "THE GAME IS DAMAGED!", 87: "SAVING GAME . . .", 88: "LOADING GAME . . .", 93: "",
    94: "CAN'T MODIFY CHAOS STRIKES BACK DISK!", 95: "THAT'S THE CHAOS STRIKES BACK DISK!",
    96: "PUT THE CHAOS STRIKES BACK DISK IN ~", 99: "", 100: "",
    101: "LOAD SAVED GAME", 102: "SAVE AND PLAY", 103: "SAVE AND QUIT", 104: "FORMAT FLOPPY", 105: "OK", 106: "CANCEL",
    107: "NO CREATURE", 108: "GIANT SCORPION", 109: "SLIME DEVIL", 134: "GREY LORD", 135: "",
}
# DM text indices whose CSB string differs from DM: class and direction order, the disk messages, the save
# dialog, the texts that CSB does not have, the creature names
DIFFERENT = {2, 3, 23, 24, 93, 94, 95, 96, 99, 100, 102, 103, 109, 111, 112, 116, 122, 123, 124, 125, 126, 127, 129, 130}
OBJECT_NAMES = {0: "COMPASS", 23: "STORM", 24: "STORM", 158: "ANTIVENIN", 174: "SHANK", 198: "BONES", 199: ""}


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    for f in ("gfx_get_string", "gfx_get_action_name", "gfx_get_object_name", "panel_get_rank_name"):
        getattr(lib, f).restype = ctypes.c_char_p
    lib.recruit_text_thing.restype = ctypes.c_uint16
    lib.map_get_first_thing_at.restype = ctypes.c_uint16
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    yield lib
    c16.in_dll(lib, "g_adventure").value = DM


def load(lib, adventure, folder):
    c16.in_dll(lib, "g_adventure").value = adventure
    assert lib.gameloop_load_data(str(folder).encode())


def texts(lib, adventure, folder):
    load(lib, adventure, folder)
    return ([lib.gfx_get_string(i).decode() for i in range(TEXT_COUNT)], [lib.gfx_get_action_name(i).decode() for i in range(C.ACTION_COUNT)],
            [lib.gfx_get_object_name(i).decode() for i in range(OBJECT_NAME_COUNT)])


def test_strings(lib):
    load(lib, CSB, GAME_CSB)
    assert {i: lib.gfx_get_string(i).decode() for i in EXPECTED} == EXPECTED


def test_string_index_tables(lib):
    load(lib, CSB, GAME_CSB)

    def table(addr, n):
        return [lib.gfx_get_string(lib.kaos_s16(addr + 2 * k)).decode() for k in range(n)]

    assert table(BASE_SKILL_STRINGS, 4) == ["FIGHTER", "NINJA", "PRIEST", "WIZARD"]
    assert table(DIRECTION_STRINGS, 4) == ["NORTH", "EAST", "SOUTH", "WEST"]
    assert table(STATISTIC_STRINGS, 7) == ["L", "STRENGTH", "DEXTERITY", "WISDOM", "VITALITY", "ANTI-MAGIC", "ANTI-FIRE"]
    levels = table(SKILL_LEVEL_STRINGS, 15)
    assert levels[0] == "NEOPHYTE" and levels[8] == "` MASTER" and levels[14] == "ARCHMASTER"
    assert lib.panel_get_rank_name(16) == b"ARCHMASTER"


def test_action_and_object_names(lib):
    load(lib, CSB, GAME_CSB)
    assert [lib.gfx_get_action_name(i) for i in (0, 4, 27, 43, 44)] == [b"N", b"BLOW HORN", b"INVOKE", b"FUSE", b""]
    assert {i: lib.gfx_get_object_name(i).decode() for i in OBJECT_NAMES} == OBJECT_NAMES


@needs_dm
def test_texts_against_dm(lib):
    dm_strings, dm_actions, dm_objects = texts(lib, DM, GAME)
    csb_strings, csb_actions, csb_objects = texts(lib, CSB, GAME_CSB)
    assert {i for i in range(TEXT_COUNT) if dm_strings[i] != csb_strings[i]} == DIFFERENT
    assert sorted(csb_strings[i] for i in (2, 3, 23, 24)) == sorted(dm_strings[i] for i in (2, 3, 23, 24))
    assert csb_actions == dm_actions
    changed = [i for i in range(OBJECT_NAME_COUNT) if dm_objects[i] != csb_objects[i]]
    assert len(changed) == 33 and {23, 158, 174} <= set(changed)
    assert all(csb_objects)

