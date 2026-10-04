"""The DM entrance: the doors open after ENTER (KAOS entrance_open_doors 0x126c4a)."""

import ctypes
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
SCREEN_W, SCREEN_H = 320, 200
HOOK = ctypes.CFUNCTYPE(None)

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


@pytest.fixture(scope="module")
def lib(tmp_path_factory):
    """A private build of libdm with the DM data."""
    out = tmp_path_factory.mktemp("lib") / "libdm.dylib"
    sources = [str(p) for p in sorted((ROOT / "port/src").glob("*.c")) if p.name not in ("main.c", "platform.c")]
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-dynamiclib", "-o", str(out), *sources], check=True)
    lib = ctypes.CDLL(str(out))
    assert lib.gameloop_load_data(str(GAME).encode())
    return lib


def screen(lib):
    return bytes((ctypes.c_uint8 * (SCREEN_W * SCREEN_H)).in_dll(lib, "fb"))


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
    changed = [i for i in range(SCREEN_W * SCREEN_H) if shots[0][i] != closed[i]]
    assert changed and all(240 <= i % SCREEN_W < 260 for i in changed)  # only the button column changes
