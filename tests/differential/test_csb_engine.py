"""The CSB engine image: APPB.FTL tables at their KAOS addresses (docs/csb-engine-diff.md "Data tables")."""

import ctypes
import os
import shutil
import struct
import subprocess

import pytest

from dmtest.lib import load_lib
from dmtest.markers import GAME, GAME_CSB, ROOT, needs_sdl

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
DM_SPELLS, CSB_SPELLS, CSB_RESTART_BOXES = 0x103C84, 0x104160, 0x104248
DIFFERENT = {CREDITS_PALETTE, ENTRANCE_PALETTE, ENTRANCE_MOUSE, ENDGAME_MOUSE, ENTRANCE_KEYS, EXPERIENCE_GAIN, VERSION_TEXT, CSB_SPELLS, CSB_RESTART_BOXES}
ZONE_RESTART_BUTTON, ZONE_QUIT_BUTTON = 437, 438

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME_CSB / f).exists() for f in ("APPB.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game-csb files missing"),
]
needs_dm = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists(), reason="game files missing")


class CsbCopy(ctypes.Structure):
    _fields_ = [("kaos", ctypes.c_uint32), ("appb", ctypes.c_uint32), ("size", ctypes.c_uint16)]


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


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
    lib.kaos_ptr.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.spell_find.restype = c16
    lib.tables_get_spell.restype = ctypes.c_void_p
    yield lib
    set_adventure(lib, DM)


def set_adventure(lib, adventure):
    c16.in_dll(lib, "g_adventure").value = adventure


def load(lib, adventure, path):
    set_adventure(lib, adventure)
    assert lib.kaos_load(str(path).encode()) == 0
    assert lib.tables_init() == 0
    return lambda addr, n: bytes(lib.kaos_ptr(addr)[:n])


def copies(lib):
    count = ctypes.c_int.in_dll(lib, "csb_copy_count").value
    return list((CsbCopy * count).in_dll(lib, "csb_copies"))


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
    assert csb(CSB_SPELLS, 25 * 8) == kaos(DM_SPELLS, 25 * 8)


@needs_dm
def test_documented_differences(lib, kaos):
    csb = load(lib, CSB, GAME_CSB / "APPB.FTL")
    gain = {i: (a, b) for i, (a, b) in enumerate(zip(kaos(EXPERIENCE_GAIN, 44), csb(EXPERIENCE_GAIN, 44))) if a != b}
    assert gain == {4: (1, 0), 36: (5, 0), 37: (1, 0), 41: (3, 0)}
    assert csb(VERSION_TEXT, 5) == b"V3.1\0"
    entrance, dm_entrance = struct.unpack(">16H", csb(ENTRANCE_PALETTE, 32)), struct.unpack(">16H", kaos(ENTRANCE_PALETTE, 32))
    assert [i for i in range(16) if entrance[i] != dm_entrance[i]] == [5] and entrance[5] == 0x422
    assert csb(ENTRANCE_KEYS, 8) == bytes.fromhex("00c8440000000000")
    assert [struct.unpack_from(">H", csb(ENTRANCE_MOUSE + 12 * i, 2))[0] for i in range(5)] == [0xC8, 0xC9, 0xCA, 0xCB, 0]
    assert [struct.unpack_from(">H", csb(ENDGAME_MOUSE + 12 * i, 2))[0] for i in range(2)] == [0xD7, 0]


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
    assert lib.spell_count() == 29
    assert [lib.spell_find(spell_runes(appb, i)) for i in range(25, 29)] == [25, 26, 27, 28]
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    assert lib.tables_get_spell(28) and not lib.tables_get_spell(29)


@needs_dm
def test_dm_spells(lib, appb):
    load(lib, DM, GAME / "KAOS.FTL")
    assert lib.spell_count() == 25
    assert lib.spell_find(spell_runes(appb, 25)) == -1


@needs_dm
def test_layout_zones_equal_but_endgame_buttons(lib):
    def boxes(folder):
        assert lib.gfx_open(str(folder / "Graphics.DAT").encode()) >= 0 and lib.layout_init() == 0
        result = {}
        for zone in range(1, 2000):
            b, w, h = Box(), c16(), c16()
            if lib.layout_get_zone(zone, 16, 16, ctypes.byref(b), ctypes.byref(w), ctypes.byref(h)):
                result[zone] = (b.x, b.y, b.w, b.h, w.value, h.value)
        return result

    dm, csb = boxes(GAME), boxes(GAME_CSB)
    assert set(dm) - set(csb) == {ZONE_RESTART_BUTTON, ZONE_QUIT_BUTTON}
    assert all(csb[z] == dm[z] for z in csb)


def test_prison_new_game_runs_and_renders(lib):
    set_adventure(lib, CSB)
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    assert lib.gameloop_start_new_game(str(GAME_CSB).encode())
    position = [c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")]
    assert position == [0, 9, 0, 2]
    for _ in range(50):
        if lib.gameloop_run_frame():
            lib.gameloop_tick()
    assert lib.view_render(*position)


def run_dm(*args):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    env.pop("DM_AUTO_ENTER", None)
    return subprocess.run([str(ROOT / "build/dm"), *args, "--frames", "5"], cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)


@needs_sdl
def test_view_shows_prison():
    result = run_dm("--adventure", "csb", "--view")
    assert result.returncode == 0, result.stderr


@needs_sdl
def test_view_loads_csb_from_data_folder(tmp_path):
    for name in ("APPB.FTL", "Graphics.DAT", "Dungeon.DAT", "disk-b"):
        (tmp_path / name).symlink_to(GAME_CSB / name)
    assert run_dm("--adventure", "csb", "--view", "--data", str(tmp_path)).returncode == 0
    (tmp_path / "APPB.FTL").unlink()
    (tmp_path / "APPB.FTL").write_bytes(b"")
    result = run_dm("--adventure", "csb", "--view", "--data", str(tmp_path))
    assert result.returncode == 1 and f"{tmp_path}/APPB.FTL" in result.stderr
