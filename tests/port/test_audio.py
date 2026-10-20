# SPDX-License-Identifier: GPL-3.0-or-later
"""The low-pass filter of the Amiga output: one-pole stages for the fixed RC filter and the LED filter."""
import ctypes
import math

from dmtest import constants as C
from dmtest.lib import load_lib

c16 = ctypes.c_int16
STEP = 10000
FRAMES = 64


def filtered(lib, value, frames):
    out = (c16 * (2 * frames))(*([value] * 2 * frames))
    lib.audio_filter(out, frames)
    return list(out[0::2]), list(out[1::2])


def test_the_filter_stages_are_one_pole_low_passes():
    lib = load_lib()
    alpha = 1
    for cutoff in [C.FILTER_FIXED_CUTOFF_HZ] + [C.FILTER_LED_CUTOFF_HZ] * C.FILTER_LED_POLES:
        alpha *= 1 / (1 + C.AUDIO_OUTPUT_RATE / (2 * math.pi * cutoff))
    left, right = filtered(lib, STEP, FRAMES)
    assert left == right
    assert left[0] == round(alpha * STEP)  # the step response starts at the product of the stage alphas
    assert left[-1] == STEP  # and settles at the input: unity gain
    assert all(a <= b for a, b in zip(left, left[1:]))  # rises to the input without an overshoot
    assert filtered(lib, 0, FRAMES)[0][-1] == 0  # the output decays back to silence
