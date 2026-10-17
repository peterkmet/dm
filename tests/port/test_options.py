# SPDX-License-Identifier: GPL-3.0-or-later
"""The volumes and the settings of the advanced mode: the mixer scale and the settings at a game start."""
import ctypes

import pytest

from dmtest import constants as C
from dmtest.game import ADVANCED, VANILLA, set_mode, start_game
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

c16 = ctypes.c_int16
SAMPLE, VOLUME, PERIOD = 100, 64, 400
SETTINGS_FILE = "SETTINGS.TXT"


@pytest.fixture(scope="module")
def lib():
    return load_lib()


def mixed(lib, step):
    """The first left output sample of one voice of SAMPLE at VOLUME, at the sound volume step."""
    c16.in_dll(lib, "g_audio_volume").value = step
    data = (ctypes.c_int8 * 64)(*([SAMPLE] * 64))
    lib.audio_stop_all()
    lib.audio_play_data(data, 64, PERIOD, (ctypes.c_uint8 * 2)(VOLUME, VOLUME))
    out = (c16 * 2)()
    lib.audio_mix(out, 1)
    c16.in_dll(lib, "g_audio_volume").value = C.SETTINGS_VOLUME_FULL
    return out[0]


def test_the_sound_volume_scales_the_mix(lib):
    full = SAMPLE * VOLUME * 2  # the mix of one voice without a volume step
    assert mixed(lib, C.SETTINGS_VOLUME_FULL) == full
    assert mixed(lib, 3) == full * 3 // C.SETTINGS_VOLUME_FULL
    assert mixed(lib, C.SETTINGS_VOLUME_OFF) == 0


@needs_dm
def test_a_game_start_takes_the_settings_only_in_the_advanced_mode(tmp_path):
    lib = load_lib()
    lib.startend_save_dir.restype = ctypes.c_char_p
    game = start_game(lib)
    (tmp_path / SETTINGS_FILE).write_text("sound_volume=3\nmusic_volume=2\n")
    data_dir = lib.startend_save_dir()
    lib.startend_set_save_dir(str(tmp_path).encode())
    volumes = lambda: (c16.in_dll(lib, "g_audio_volume").value, c16.in_dll(lib, "g_music_volume").value)
    set_mode(game, ADVANCED)
    assert lib.gameloop_start_new_game(str(GAME).encode())
    assert volumes() == (3, 2)
    set_mode(game, VANILLA)
    assert lib.gameloop_start_new_game(str(GAME).encode())
    assert volumes() == (C.SETTINGS_VOLUME_FULL, C.SETTINGS_VOLUME_FULL)
    lib.startend_set_save_dir(data_dir)

