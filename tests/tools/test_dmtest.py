# SPDX-License-Identifier: GPL-3.0-or-later
"""The library loader: two loads in one process have separate C globals."""

import ctypes

from dmtest.lib import load_lib


def test_each_load_has_its_own_globals():
    a, b = load_lib(), load_lib()
    seed_a = ctypes.c_uint32.in_dll(a, "g_random_seed")
    seed_b = ctypes.c_uint32.in_dll(b, "g_random_seed")
    seed_a.value, seed_b.value = 1, 2
    assert (seed_a.value, seed_b.value) == (1, 2)
