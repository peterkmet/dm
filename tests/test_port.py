import ctypes
import os
import subprocess

import pytest

from dmtest.lib import load_lib
from dmtest.markers import BUILD, sdl3_installed

pytestmark = pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed")


@pytest.fixture(scope="module")
def lib():
    return load_lib()


def test_palette(lib):
    lib.video_convert_argb.restype = ctypes.c_uint32
    assert lib.video_convert_argb(0x0F80) == 0xFFFF8800
    assert lib.video_convert_argb(0x0000) == 0xFF000000
    assert lib.video_convert_argb(0x0FFF) == 0xFFFFFFFF


@pytest.mark.parametrize(
    "out, pos, game",
    [
        ((960, 600), (0, 0), (0, 0)),
        ((960, 600), (959, 599), (319, 199)),
        ((960, 600), (300, 150), (100, 50)),
        ((1000, 700), (10, 41), (3, 1)),  # scale 3.125, offset (0, 37.5)
        ((1000, 700), (999, 662), (319, 199)),
        ((1000, 700), (0, 37), (0, 0)),  # letterbox, clamped
        ((1000, 700), (999, 699), (319, 199)),
    ],
)
def test_mouse_mapping(lib, out, pos, game):
    gx, gy = ctypes.c_int(), ctypes.c_int()
    lib.video_map_mouse(*out, *pos, ctypes.byref(gx), ctypes.byref(gy))
    assert (gx.value, gy.value) == game


def test_smoke_run(lib):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    r = subprocess.run([BUILD / "dm", "--frames", "5"], env=env, capture_output=True, timeout=10)
    assert r.returncode == 0, r.stderr
