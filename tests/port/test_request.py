# SPDX-License-Identifier: GPL-3.0-or-later
"""The request reader of the control protocol: the flat key search and the list scanner."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib

lib = load_lib()
lib.request_get_string.restype = ctypes.c_bool
lib.request_get_string.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
lib.request_get_int.restype = ctypes.c_int64
lib.request_get_int.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int64]
lib.request_next_item.restype = ctypes.c_int
lib.request_next_item.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_char_p, ctypes.c_size_t]


def get_string(line, key, size=32):
    out = ctypes.create_string_buffer(size)
    found = lib.request_get_string(line.encode(), key.encode(), out, size)
    return out.value.decode() if found else None


@pytest.mark.parametrize(
    "line, key, expected",
    [
        ('{"cmd": "act", "action": "move_forward"}', "action", "move_forward"),
        ('{"cmd":"act","args":{"choice":"resurrect"}}', "choice", "resurrect"),
        ('{"cmd": "wait", "ticks": 5}', "cmd", "wait"),
        ('{"cmd": "act"}', "action", None),
        ('{"ticks": 5}', "ticks", None),
    ],
)
def test_get_string(line, key, expected):
    assert get_string(line, key) == expected


def test_get_string_cuts_to_size():
    assert get_string('{"path": "/tmp/a-long-file-name.png"}', "path", size=8) == "/tmp/a-"


@pytest.mark.parametrize(
    "line, expected",
    [
        ('{"ticks": 25}', 25),
        ('{"ticks":-3}', -3),
        ('{"ticks": "x"}', -1),
        ('{"cmd": "wait"}', -1),
    ],
)
def test_get_int(line, expected):
    assert lib.request_get_int(line.encode(), b"ticks", -1) == expected


def items(text, size=256):
    """All items of a list body (the text after '['), and the last result."""
    buffer = ctypes.create_string_buffer(text.encode())
    cursor = ctypes.c_char_p(ctypes.addressof(buffer))
    out = ctypes.create_string_buffer(size)
    found = []
    while (result := lib.request_next_item(ctypes.byref(cursor), out, size)) == C.REQUEST_ITEM_FOUND:
        found.append(out.value.decode())
    return found, result


def test_list_of_names_and_objects():
    found, result = items('"move_forward", {"action": "take_object", "args": {"cell": 2}}, "turn_left"]}')
    assert found == ['"move_forward"', '{"action": "take_object", "args": {"cell": 2}}', '"turn_left"']
    assert result == C.REQUEST_ITEM_END


@pytest.mark.parametrize("text", ["5]", '"move_forward', '{"action": "pass"', '{"action": "pass}'])
def test_bad_item(text):
    assert items(text)[1] == C.REQUEST_ITEM_BAD


def test_item_too_long():
    assert items('"move_forward"]', size=4)[1] == C.REQUEST_ITEM_BAD
