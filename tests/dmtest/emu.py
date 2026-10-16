# SPDX-License-Identifier: GPL-3.0-or-later
"""The original program in the emulator: the KAOS.FTL image and the small helpers of the differential tests."""

import pytest

from dmtest.markers import GAME

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

KAOS_BASE = 0x100000  # the load address of KAOS.FTL, as in the original


def kaos_emu(max_cycles=10_000_000):
    """An emulator with the KAOS.FTL image at its load address."""
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), KAOS_BASE), max_cycles=max_cycles)


def var(lib, ctype, name):
    return ctype.in_dll(lib, name)


def need(lib, *names):
    missing = [n for n in ("KAOS.FTL", *names) if not (GAME / n).exists()]
    if missing:
        pytest.skip(f"missing game/{missing[0]}")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.tables_init() == 0


def s16(v):
    return (v & 0xFFFF) - ((v & 0x8000) << 1)
