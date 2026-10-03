import ctypes
import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).parent.parent
BUILD = ROOT / "build"


def sdl3_installed():
    if not (shutil.which("clang") and shutil.which("brew")):
        return False
    prefix = subprocess.run(["brew", "--prefix", "sdl3"], capture_output=True, text=True).stdout.strip()
    return Path(prefix, "include/SDL3/SDL.h").exists()


pytestmark = pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed")


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    return ctypes.CDLL(str(BUILD / "libdm.dylib"))


def test_palette(lib):
    lib.video_argb.restype = ctypes.c_uint32
    assert lib.video_argb(0x0F80) == 0xFFFF8800
    assert lib.video_argb(0x0000) == 0xFF000000
    assert lib.video_argb(0x0FFF) == 0xFFFFFFFF


@pytest.mark.parametrize(
    "out, pos, game",
    [
        ((960, 600), (0, 0), (0, 0)),
        ((960, 600), (959, 599), (319, 199)),
        ((960, 600), (300, 150), (100, 50)),
        ((1000, 700), (20, 50), (0, 0)),  # scale 3, offset (20, 50)
        ((1000, 700), (23, 56), (1, 2)),
        ((1000, 700), (5, 10), (0, 0)),  # letterbox, clamped
        ((1000, 700), (999, 699), (319, 199)),
    ],
)
def test_mouse_mapping(lib, out, pos, game):
    gx, gy = ctypes.c_int(), ctypes.c_int()
    lib.video_map_mouse(*out, *pos, ctypes.byref(gx), ctypes.byref(gy))
    assert (gx.value, gy.value) == game


def test_smoke_run(lib):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy")
    r = subprocess.run([BUILD / "dm", "--frames", "5"], env=env, capture_output=True, timeout=10)
    assert r.returncode == 0, r.stderr
