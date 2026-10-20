# SPDX-License-Identifier: GPL-3.0-or-later
"""The low-pass filter of the Amiga output: a one-pole filter at the cutoff of the LED filter."""
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


def test_the_filter_is_a_one_pole_low_pass_at_the_cutoff():
    lib = load_lib()
    alpha = 1 / (1 + C.AUDIO_OUTPUT_RATE / (2 * math.pi * C.FILTER_CUTOFF_HZ))
    left, right = filtered(lib, STEP, FRAMES)
    assert left == right
    assert left[0] == round(alpha * STEP)  # the step response starts at alpha
    assert left[-1] == STEP  # and settles at the input: unity gain
    assert all(a <= b for a, b in zip(left, left[1:]))  # rises to the input without an overshoot
    assert filtered(lib, 0, FRAMES)[0][-1] == 0  # the output decays back to silence
