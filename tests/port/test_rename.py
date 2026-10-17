# SPDX-License-Identifier: GPL-3.0-or-later
"""The name entry of a reincarnated champion: fields, limits, backspace, OK and the duplicate refusal."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.game import champions
from dmtest.lib import load_lib

NAME_LENGTH, TITLE_LENGTH = C.CHAMPION_NAME_SIZE - 1, C.CHAMPION_TITLE_SIZE - 1
RENAMED = 1  # the reincarnated champion; champion 0 stays in the party


class RenameInput(ctypes.Structure):
    _fields_ = [("in_title", ctypes.c_bool), ("position", ctypes.c_int16), ("cursor_visible", ctypes.c_bool)]


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    lib.champion_get_rename_input.restype = RenameInput
    lib.champion_rename_ok.restype = ctypes.c_bool
    lib.champion_rename_type.argtypes = [ctypes.c_char]
    return lib


@pytest.fixture
def entry(lib):
    party = champions(lib)
    party[0].name, party[0].title = b"ZED", b"OLD ONE"
    party[RENAMED].name, party[RENAMED].title = b"HALK", b"THE BARBARIAN"
    ctypes.c_int16.in_dll(lib, "g_party_champion_count").value = 2
    lib.champion_start_rename(RENAMED)
    return lib


def type_text(lib, text):
    for character in text.encode():
        lib.champion_rename_type(bytes([character]))


def texts(lib):
    champion = champions(lib)[RENAMED]
    return champion.name.decode(), champion.title.decode()


def where(lib):
    state = lib.champion_get_rename_input()
    return state.in_title, state.position, state.cursor_visible


def test_start_clears_name_and_title(entry):
    assert texts(entry) == ("", "")
    assert where(entry) == (False, 0, True)


def test_seventh_name_character_moves_to_title(entry):
    type_text(entry, "ABCDEF")
    assert where(entry) == (False, 6, True)
    type_text(entry, "G")
    assert where(entry) == (True, 0, True)
    assert texts(entry) == ("ABCDEFG", "")


def test_title_takes_nineteen_characters_then_hides_cursor(entry):
    type_text(entry, "A")
    entry.champion_rename_next_field()
    type_text(entry, "T" * TITLE_LENGTH)
    assert where(entry) == (True, TITLE_LENGTH, False)
    type_text(entry, "X")
    assert texts(entry) == ("A", "T" * TITLE_LENGTH)


def test_first_space_of_each_field_refused(entry):
    type_text(entry, " A ")
    entry.champion_rename_next_field()
    type_text(entry, " B")
    assert texts(entry) == ("A ", "B")


def test_original_punctuation_accepted(entry):
    type_text(entry, "A,.;:")
    assert texts(entry) == ("A,.;:", "")


def test_next_field_needs_a_name(entry):
    entry.champion_rename_next_field()
    assert where(entry) == (False, 0, True)
    type_text(entry, "A")
    entry.champion_rename_next_field()
    assert where(entry) == (True, 0, True)
    entry.champion_rename_next_field()
    type_text(entry, "B")
    assert where(entry) == (True, 1, True)


def test_backspace_on_empty_name_does_nothing(entry):
    entry.champion_rename_backspace()
    assert where(entry) == (False, 0, True)
    assert texts(entry) == ("", "")


def test_backspace_deletes_previous_character(entry):
    type_text(entry, "AB")
    entry.champion_rename_next_field()
    type_text(entry, "XY")
    entry.champion_rename_backspace()
    assert texts(entry) == ("AB", "X")
    assert where(entry) == (True, 1, True)


def test_backspace_at_title_start_deletes_last_name_character(entry):
    type_text(entry, "ABCDEFG")
    entry.champion_rename_backspace()
    assert texts(entry) == ("ABCDEF", "")
    assert where(entry) == (False, 6, True)


def test_ok_ignored_with_empty_name(entry):
    assert not entry.champion_rename_ok()
    assert where(entry) == (False, 0, True)


def test_ok_with_empty_title_and_trimmed_name(entry):
    type_text(entry, "AB  ")
    assert entry.champion_rename_ok()
    assert texts(entry) == ("AB", "")


def test_ok_in_title(entry):
    type_text(entry, "BO")
    entry.champion_rename_next_field()
    type_text(entry, "THE NEW")
    assert entry.champion_rename_ok()
    assert texts(entry) == ("BO", "THE NEW")


def test_duplicate_name_refused(entry):
    type_text(entry, "ZED ")
    entry.champion_rename_next_field()
    type_text(entry, "THE NEW")
    assert not entry.champion_rename_ok()
    assert texts(entry) == ("ZED ", "")
    assert where(entry) == (False, len("ZED "), True)
    entry.champion_rename_backspace()
    type_text(entry, "D")
    assert entry.champion_rename_ok()
    assert texts(entry) == ("ZEDD", "")


def test_full_name_after_refusal_takes_only_backspace_and_next_field(entry):
    champions(entry)[0].name = b"ABCDEFG"
    type_text(entry, "ABCDEFG")
    assert not entry.champion_rename_ok()
    assert where(entry) == (False, NAME_LENGTH, False)
    type_text(entry, "H")
    assert texts(entry) == ("ABCDEFG", "")
    entry.champion_rename_next_field()
    assert where(entry) == (True, 0, True)
    entry.champion_rename_backspace()
    entry.champion_rename_backspace()
    assert texts(entry) == ("ABCDE", "")
    assert where(entry) == (False, 5, True)
