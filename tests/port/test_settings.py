# SPDX-License-Identifier: GPL-3.0-or-later
"""The settings file SETTINGS.TXT: the parser and its defaults."""
import ctypes

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib

FULL, ORIGINAL, SUPER_NES, FM_TOWNS = C.SETTINGS_VOLUME_FULL, C.SETTINGS_MUSIC_ORIGINAL, C.SETTINGS_MUSIC_SUPER_NES, C.SETTINGS_MUSIC_FM_TOWNS
NONE, SCALE2X, SCALE3X = C.SETTINGS_FILTER_NONE, C.SETTINGS_FILTER_SCALE2X, C.SETTINGS_FILTER_SCALE3X
DEFAULTS = (FULL, FULL, ORIGINAL, NONE)


class Settings(ctypes.Structure):
    _fields_ = [("sound_volume", ctypes.c_int16), ("music_volume", ctypes.c_int16), ("music", ctypes.c_int16),
                ("filter", ctypes.c_int16)]


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    lib.settings_parse.restype = Settings
    lib.settings_parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    return lib


def parse(lib, text, adventure=C.ADVENTURE_DM):
    ctypes.c_int16.in_dll(lib, "g_adventure").value = adventure
    s = lib.settings_parse(text, len(text))
    ctypes.c_int16.in_dll(lib, "g_adventure").value = C.ADVENTURE_DM
    return s.sound_volume, s.music_volume, s.music, s.filter


@pytest.mark.parametrize("text, values", [
    (b"sound_volume=3\nmusic_volume=0\nmusic=fm-towns\n", (3, 0, FM_TOWNS, NONE)),
    (b"music=super-nes\r\nsound_volume=5\r\n", (5, FULL, SUPER_NES, NONE)),  # Windows line ends, a missing line
    (b"music_volume=2", (FULL, 2, ORIGINAL, NONE)),  # no final newline
    (b"", DEFAULTS),
    (b"sound_volume=9\nmusic=loud\n", DEFAULTS),  # out of range, unknown music
    (b"sound_volume=-1\nmusic_volume=33\nsound_volume=\nmusic_volume=7x\n", DEFAULTS),
    (b"volume=1\nsound_volume\n=4\nmusic=original\nsound_volume=1\n", (1, FULL, ORIGINAL, NONE)),  # unknown key, no value
    (b"filter=scale2x\n", (FULL, FULL, ORIGINAL, SCALE2X)),
    (b"filter=scale3x\nfilter=none\n", DEFAULTS),  # the last line wins
    (b"filter=scale4x\nfilter=SCALE3X\nfilter=2\n", DEFAULTS),  # unknown filters
    (b"music=" + b"x" * 100 + b"\nmusic_volume=4\n", (FULL, 4, ORIGINAL, NONE)),  # a long line
])
def test_parse(lib, text, values):
    assert parse(lib, text) == values


def test_a_file_above_the_size_limit_gives_the_defaults(lib):
    text = b"sound_volume=1\n"
    assert parse(lib, text.ljust(C.SETTINGS_MAX_SIZE, b"\n")) == (1, FULL, ORIGINAL, NONE)
    assert parse(lib, text.ljust(C.SETTINGS_MAX_SIZE + 1, b"\n")) == DEFAULTS


def test_super_nes_gives_the_original_music_in_csb(lib):
    assert parse(lib, b"music=super-nes\n", C.ADVENTURE_CSB) == DEFAULTS
    assert parse(lib, b"music=fm-towns\n", C.ADVENTURE_CSB) == (FULL, FULL, FM_TOWNS, NONE)
