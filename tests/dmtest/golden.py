# SPDX-License-Identifier: GPL-3.0-or-later
"""The setup of the golden rule and screen snapshots: a fixed new game and the recruit steps of the scenarios."""

import ctypes

from dmtest import constants as C
from dmtest.game import Box, ChampionRuntime, Game, Location, recruit_and_hold_object
from dmtest.lib import load_lib
from dmtest.markers import GAME

c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32
SEED = 0x1234ABCD


def golden_game():
    """A new game with a fixed seed in a private copy of libdm."""
    # a private copy: an xdist worker shares one image per path, so earlier modules would leave their state
    lib = load_lib()
    # the types that Game, state and recruit_and_hold_object need, as the fixture of tests/port/test_save.py
    lib.gameloop_load_data.restype = lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool
    lib.ui_zone_box.restype = ctypes.c_bool
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.rec_thing_next.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.rec_find_unused_thing.restype = u16
    lib.rec_find_unused_thing.argtypes = [u16]
    lib.kaos_s16.restype = c16
    lib.dungeon_save.restype = ctypes.c_void_p
    lib.free.argtypes = [ctypes.c_void_p]
    lib.inventory_get_object_weight.restype = u16
    lib.group_get_generated.restype = u16
    lib.group_get_generated.argtypes = [u16, c16, u16, c16, Location]
    lib.gfx_get_action_name.restype = ctypes.c_char_p
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    u32.in_dll(lib, "g_random_seed").value = SEED
    return Game(lib)


def ticks(game, n):
    for _ in range(n):
        game.lib.gameloop_tick()


def hand_box(game, champion_index=0):
    box = Box()
    assert game.lib.ui_get_slot_box_zone(C.SLOT_HAND_ACTION, champion_index, ctypes.byref(box))
    return box


def zone_box(game, zone):
    box = Box()
    assert game.lib.ui_zone_box(zone, ctypes.byref(box))
    return box


def recruit(game):
    """One recruited champion; the leader hand is empty again."""
    recruit_and_hold_object(game)
    game.click_box(hand_box(game), viewport=False)  # the object goes back into the action hand
    assert game.value("g_leader_hand_object", u16) == 0xFFFF


def add_second_champion(game):
    """A copy of champion 0 in the next cell, without objects, as the second champion."""
    champions = game.champions
    ctypes.memmove(ctypes.byref(champions[1]), ctypes.byref(champions[0]), ctypes.sizeof(champions[0]))
    champions[1].runtime = ChampionRuntime()  # the copy has no portrait
    champions[1].name = b"SECOND"
    champions[1].cell = (champions[0].cell + 1) % 4
    for s in range(len(champions[1].slots)):
        champions[1].slots[s] = 0xFFFF
    champions[1].load = 0
    u16.in_dll(game.lib, "g_party_champion_count").value = 2
