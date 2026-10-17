# SPDX-License-Identifier: GPL-3.0-or-later
"""The command-line parser: values, errors, --help."""

import ctypes

import pytest

from dmtest.lib import load_lib
from dmtest import constants as C


class Options(ctypes.Structure):
    _fields_ = [("frames", ctypes.c_int64), ("graphic", ctypes.c_int16), ("view", ctypes.c_bool),
                ("adventure", ctypes.c_int16), ("mode", ctypes.c_int16), ("data_dir", ctypes.c_char_p),
                ("saves_dir", ctypes.c_char_p), ("control_socket", ctypes.c_char_p), ("filter", ctypes.c_char_p)]


lib = load_lib()
lib.options_parse.restype = ctypes.c_int
lib.options_parse.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(Options)]


def parse(*args):
    argv = (ctypes.c_char_p * (len(args) + 1))(b"dm", *(a.encode() for a in args))
    options = Options()
    return lib.options_parse(len(args) + 1, argv, ctypes.byref(options)), options


def test_no_options():
    result, options = parse()
    assert result == C.OPTIONS_RUN
    assert (options.frames, options.graphic, options.view, options.adventure, options.mode) == (-1, -1, False, -1, -1)
    assert options.data_dir is None and options.filter is None


def test_values():
    result, options = parse("--frames", "100", "--data", "game", "--saves", "s", "--control", "c.sock",
                            "--mode", "advanced", "--adventure", "csb", "--filter", "scale2x", "--show", "7", "--view")
    assert result == C.OPTIONS_RUN
    assert (options.frames, options.graphic, options.view) == (100, 7, True)
    assert (options.mode, options.adventure) == (C.GAME_MODE_ADVANCED, C.ADVENTURE_CSB)
    assert (options.data_dir, options.saves_dir, options.control_socket, options.filter) == (b"game", b"s", b"c.sock", b"scale2x")


@pytest.mark.parametrize("args", [
    ["--frams", "5"],          # unknown option
    ["game"],                  # a value without an option
    ["--frames"],              # a missing value
    ["--frames", "5x"],        # a bad number
    ["--show", "-2"],          # a graphic below 0
    ["--mode", "fast"],        # an unknown mode
    ["--adventure", "x"],      # an unknown adventure
])
def test_errors(args):
    assert parse(*args)[0] == C.OPTIONS_ERROR


def test_help():
    assert parse("--help")[0] == C.OPTIONS_HELP
