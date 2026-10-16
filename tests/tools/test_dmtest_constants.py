# SPDX-License-Identifier: GPL-3.0-or-later
"""The constant parser of tests/dmtest against small C headers."""

from dmtest.constants import parse

HEADER = """
#define ROWS 5
#define NONE (-1)
#define LAST_ROW (ROWS - 1)
#define MIN_S16 ((int16_t)0x8000)
#define BIT_5 (1 << 5)
#define MASK 0x0fu /* a comment */
#define LETTER 'A'
#define NAME "text"
#define CALL(x) ((x) + 1)
enum MoveResult { MOVE_OK, MOVE_BLOCKED = 3, MOVE_FELL };
"""


def test_values_of_a_header():
    c = parse([HEADER])
    assert (c["ROWS"], c["NONE"], c["LAST_ROW"], c["MIN_S16"], c["BIT_5"], c["MASK"], c["LETTER"]) == (5, -1, 4, -32768, 32, 15, 65)
    assert (c["MOVE_OK"], c["MOVE_BLOCKED"], c["MOVE_FELL"]) == (0, 3, 4)
    assert "NAME" not in c and "CALL" not in c


def test_division_truncates_toward_zero_as_in_c():
    assert parse(["#define A (-7 / 2)\n#define B (-7 % 2)"]) == {"A": -3, "B": -1}


def test_a_constant_may_use_a_later_one():
    assert parse(["#define B (A + 1)", "#define A 2"])["B"] == 3


def test_unsigned_cast_wraps():
    assert parse(["#define ALL ((uint16_t)-1)"])["ALL"] == 0xFFFF
