"""The DM entrance: the doors open after ENTER (KAOS entrance_open_doors 0x126c4a)."""

import ctypes
import shutil

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib
from dmtest.markers import GAME


HOOK = ctypes.CFUNCTYPE(None)

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm with the DM data."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    assert lib.gameloop_load_data(str(GAME).encode())
    return lib


def screen(lib):
    return bytes((ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, "fb"))


def test_doors_open_in_64_steps_of_2_frames(lib):
    lib.entrance_draw()
    closed, shots = screen(lib), []
    hook = HOOK(lambda: shots.append(screen(lib)))
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    try:
        lib.entrance_open_doors()
    finally:
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None
    assert len(shots) == 128  # the right door is 128 pixels wide
    assert shots[0] == shots[1] == closed  # the first step shows the closed doors for 2 frames
    assert shots[2] != closed
    assert len({shots[i] for i in range(0, 128, 2)}) == 64


def test_no_frames_no_doors(lib):
    lib.entrance_draw()
    closed = screen(lib)
    lib.entrance_open_doors()
    assert screen(lib) == closed


@pytest.mark.parametrize("resume", [0, 1])
def test_pressed_button_shows_for_20_frames(lib, resume):
    lib.entrance_draw()
    closed, shots = screen(lib), []
    hook = HOOK(lambda: shots.append(screen(lib)))
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    try:
        lib.entrance_press_button(resume)
    finally:
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None
    assert len(shots) == 20 and len(set(shots)) == 1
    changed = [i for i in range(C.SCREEN_W * C.SCREEN_H) if shots[0][i] != closed[i]]
    assert changed and all(240 <= i % C.SCREEN_W < 260 for i in changed)  # only the button column changes


SWSH = GAME / "SWSH.FTL"
WHITE, GRAY, DARK, YELLOW = 0xFFF, 0xAAA, 0x444, 0xFF0


@pytest.mark.skipif(not SWSH.exists(), reason="game/SWSH.FTL missing")
def test_ftl_logo_lasts_as_its_sound(lib):
    """SWSH.FTL: the logo in a palette script, shown until the sound (9078 bytes at period 334) ends."""
    pal = (ctypes.c_uint16 * 32).in_dll(lib, "palette")
    shots = []
    hook = HOOK(lambda: shots.append((screen(lib), list(pal[:16]))))
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    lib.logo_play.argtypes = [ctypes.c_char_p]
    try:
        assert lib.logo_play(str(SWSH).encode()) == 0
    finally:
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None
    assert len(shots) == 43  # 9078 * 334 / 3546895 seconds at 50 Hz
    assert shots[0][1][1] == WHITE and shots[0][1][2] == 0  # the first script step: color 1 is white
    assert shots[-1][1] == [0, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, 0, WHITE, GRAY, WHITE, GRAY, DARK, YELLOW, YELLOW]
    assert len(set(shots[-1][0])) > 4
    lib.sound_stop_all()
