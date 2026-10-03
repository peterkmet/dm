import ctypes
import shutil
import subprocess
from pathlib import Path

import pytest
from dmcsb import open_dungeon
from dmcsb.dungeon import decode_champion

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
DECODED_VALUE = 0x1130AC
BUF = 0x200000
STEP_EAST, STEP_NORTH = (0, 1, 0, -1), (-1, 0, 1, 0)

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")
u8p = ctypes.POINTER(ctypes.c_uint8)


class RecruitData(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 8), ("title", ctypes.c_char * 20), ("male", ctypes.c_uint8),
        ("current_health", ctypes.c_int16), ("maximum_health", ctypes.c_int16),
        ("current_stamina", ctypes.c_int16), ("maximum_stamina", ctypes.c_int16),
        ("current_mana", ctypes.c_int16), ("maximum_mana", ctypes.c_int16),
        ("statistics", (ctypes.c_uint8 * 3) * 7), ("experience", ctypes.c_int32 * 20),
    ]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    lib.decode_text.argtypes = [ctypes.c_char_p, ctypes.c_int16, ctypes.c_uint16, ctypes.c_int16]
    lib.dungeon_square_first_thing.restype = ctypes.c_uint16
    lib.get_thing_data.restype = u8p
    lib.recruit_decoded_value.argtypes = [ctypes.c_char_p, ctypes.c_int16]
    lib.recruit_decoded_value.restype = ctypes.c_uint16
    lib.recruit_parse_text.argtypes = [ctypes.c_char_p, ctypes.POINTER(RecruitData)]
    lib.recruit_parse.argtypes = [ctypes.c_uint16, ctypes.POINTER(RecruitData)]
    lib.recruit_text_thing.restype = ctypes.c_uint16
    lib.recruit_portrait_index.argtypes = [ctypes.c_uint16]
    return lib


def need(lib):
    for n in ("KAOS.FTL", "Dungeon.DAT"):
        if not (GAME / n).exists():
            pytest.skip(f"missing game/{n}")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode()) == 1


def things(lib, m, x, y):
    t = lib.dungeon_square_first_thing(m, x, y)
    while t != 0xFFFE:
        yield t
        d = lib.get_thing_data(t)
        t = d[0] << 8 | d[1]


def mirrors(lib):
    """(portrait index, text thing) for each wall sensor of type 127; the text is on the square in front."""
    for m in range(lib.dungeon_map_count()):
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                for t in things(lib, m, x, y):
                    if t >> 10 & 15 == 3 and (p := lib.recruit_portrait_index(t)) >= 0:
                        c = t >> 14
                        yield p, lib.recruit_text_thing(m, x + STEP_EAST[c], y + STEP_NORTH[c])


def test_mirrors_match_dmcsb(lib):
    need(lib)
    d = open_dungeon(str(GAME / "Dungeon.DAT"))
    words = d._text_word_list()
    found = list(mirrors(lib))
    assert sorted(p for p, _ in found) == list(range(24))
    for _, thing in found:
        r = RecruitData()
        assert lib.recruit_parse(thing, ctypes.byref(r)) == 1
        ref = decode_champion(words, d.things["TextString"][thing & 0x3FF]["TextDataWordOffset"])
        assert (r.name.decode(), r.title.decode(), r.male) == (ref["name"], ref["title"], ref["gender"] == "M")
        for cur, mx, key in [("current_health", "maximum_health", "Health"),
                             ("current_stamina", "maximum_stamina", "Stamina"),
                             ("current_mana", "maximum_mana", "Mana")]:
            assert getattr(r, cur) == getattr(r, mx) == ref[key]
        stats = ["Luck", "Strength", "Dexterity", "Wisdom", "Vitality", "AntiMagic", "AntiFire"]
        assert [list(s) for s in r.statistics] == [[max(30, ref[k])] * 2 + [30] for k in stats]
        hidden = [lvl for k in ("Fighter", "Ninja", "Priest", "Wizard") for lvl in ref[k]]
        exp = [125 << lvl if lvl else 0 for lvl in hidden]
        assert list(r.experience) == [sum(exp[i * 4:i * 4 + 4]) for i in range(4)] + exp


def test_decoded_value_matches_original(lib):
    need(lib)
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
    texts = []
    for _, thing in mirrors(lib):
        buf = ctypes.create_string_buffer(128)
        assert lib.decode_text(buf, 128, thing, -0x7FFE) == 1
        texts.append(buf.value)
    texts.append(bytes(range(0x20, 0x80)))  # characters outside A-P carry into the high byte
    for text in texts:
        emu.write(BUF, text + b"\0" * 4)
        for i in range(len(text)):
            for n in (1, 2, 4):
                assert lib.recruit_decoded_value(text[i:] + b"\0" * 4, n) == emu.call(DECODED_VALUE, [(4, BUF + i), (2, n)]) & 0xFFFF


def test_parse_text_edges(lib):
    r = RecruitData()
    assert lib.recruit_parse_text(b"NAME\nT1\nT2\nM\nAAAA", ctypes.byref(r)) == 0  # too short
    text = b"LONGNAMEXX\nABCDEFGHIJKLMNOP\nQRSTUVWXYZ\nF\nAAPPABCDAAAZ\nAAPPBBCCDDEEFF\nAPBCAAAAAAAAAAAA"
    assert lib.recruit_parse_text(text, ctypes.byref(r)) == 1
    assert (r.name, r.title, r.male) == (b"LONGNAM", b"ABCDEFGHIJKLMNOPQRS", 0)
    assert (r.maximum_health, r.maximum_stamina, r.current_mana) == (0xFF, 0x123, 25)
    assert [s[0] for s in r.statistics] == [30, 0xFF, 30, 0x22, 0x33, 0x44, 0x55]  # minimum 30
    assert list(r.experience[4:8]) == [0, 125 << 15, 250, 500]
    assert r.experience[0] == (125 << 15) + 250 + 500


def test_portrait_origin(lib):
    need(lib)
    x, y = ctypes.c_int16(), ctypes.c_int16()
    lib.recruit_portrait_origin(11, ctypes.byref(x), ctypes.byref(y))
    assert (x.value, y.value) == (3 * 32, 1 * 29)
