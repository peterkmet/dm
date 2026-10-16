import ctypes
import itertools
import shutil
import subprocess
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
GET_SPELL_FROM_SYMBOLS, G_SPELLS = 0x122FDC, 0x103C84
BUF = 0x200000

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")


class Spell(ctypes.Structure):
    _fields_ = [("symbols", ctypes.c_uint32), ("base_required_skill_level", ctypes.c_uint8), ("skill_index", ctypes.c_uint8),
                ("kind", ctypes.c_uint8), ("type", ctypes.c_uint8), ("disabled_ticks", ctypes.c_uint8)]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.spell_draw_fails.restype = ctypes.c_bool
    lib.tables_get_spell.restype = ctypes.POINTER(Spell)
    lib.tables_get_spell.argtypes = [ctypes.c_int]
    lib.spell_find.argtypes = [ctypes.c_char_p]
    lib.spell_get_symbol.restype = ctypes.c_uint8
    lib.spell_get_symbol_mana_cost.argtypes = [ctypes.c_uint16, ctypes.c_int16, ctypes.c_uint8]
    lib.spell_get_symbol_mana_cost.restype = ctypes.c_uint16
    lib.spell_draw_fails.argtypes = [ctypes.c_uint8, ctypes.c_uint16]
    lib.spell_get_failure_experience.argtypes = [ctypes.c_uint16] * 3
    lib.spell_get_failure_experience.restype = ctypes.c_uint16
    if not (GAME / "KAOS.FTL").exists():
        pytest.skip("missing game/KAOS.FTL")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.tables_init() == 0
    return lib


def all_symbol_strings():
    steps = [[bytes([0x60 + s * 6 + n]) for n in range(6)] for s in range(4)]
    for length in range(1, 5):
        yield from (b"".join(p) for p in itertools.product(*steps[:length]))


def test_spell_find_matches_original(lib):
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
    found = set()
    for symbols in all_symbol_strings():
        emu.write(BUF, symbols + b"\0")
        ptr = emu.call(GET_SPELL_FROM_SYMBOLS, [(4, BUF)])
        expected = (ptr - G_SPELLS) // 8 if ptr else -1
        assert lib.spell_find(symbols) == expected, symbols
        found.add(expected)
    assert found == set(range(-1, 25))  # every spell is reachable


def test_spell_fields_and_rules(lib):
    s = lib.tables_get_spell(1).contents  # entry 1: 00667073 01 12 4863
    assert (s.symbols, s.base_required_skill_level, s.skill_index) == (0x667073, 1, 0x12)
    assert (s.kind, s.type, s.disabled_ticks) == (3, 6, 18)
    assert lib.spell_get_symbol(2, 3) == 0x60 + 2 * 6 + 3
    assert [lib.spell_get_symbol_mana_cost(0, n, 0) for n in range(6)] == [1, 2, 3, 4, 5, 6]
    assert lib.spell_get_symbol_mana_cost(1, 0, 0x63) == 2 * 0x14 >> 3  # step 1 base 2, multiplier of power 4
    assert lib.spell_get_symbol_mana_cost(3, 5, 0x65) == 7 * 0x1C >> 3
    for wisdom in (0, 50, 100, 200):
        for r in range(256):
            assert lib.spell_draw_fails(wisdom, r) == ((r & 0x7F) > min(wisdom + 15, 115))
    assert [lib.spell_get_failure_experience(1000, req, 1) for req in (2, 3, 17, 66)] == [500, 250, 0, 500]
