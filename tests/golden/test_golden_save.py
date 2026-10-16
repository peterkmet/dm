# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden snapshot of a save file: the saved state of a fixed game stays the same through refactors."""

import ctypes
import hashlib
import json
import os
from pathlib import Path


from dmtest.game import Game, recruit_and_hold_object, scrub_free_groups
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm
from dmtest.savefile import SAVE, decode

c16, u16 = ctypes.c_int16, ctypes.c_uint16

GOLDEN = Path(__file__).with_name("save.json")
SEED = 0x1234ABCD



def digests(parts):
    return {"header": hashlib.sha256(parts["header"]).hexdigest(),
            "blocks": [hashlib.sha256(b).hexdigest() for b in parts["blocks"]],
            "portraits": hashlib.sha256(parts["portraits"]).hexdigest(),
            "dungeon": hashlib.sha256(parts["dungeon"]).hexdigest()}


def saved_game(tmp_path):
    """A new game with a fixed seed: recruit, hold an object, save; returns the decoded parts."""
    # a private copy: an xdist worker shares one image per path, so earlier modules would leave their state
    lib = load_lib()
    # the types that Game and recruit_and_hold_object need, as the fixture of tests/port/test_save.py
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.rec_thing_next.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.inventory_get_object_weight.restype = u16
    lib.savegame_save.restype = c16
    lib.gameloop_load_data.restype = lib.gameloop_start_new_game.restype = ctypes.c_bool
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    ctypes.c_uint32.in_dll(lib, "g_random_seed").value = SEED
    game = Game(lib)
    recruit_and_hold_object(game)
    scrub_free_groups(lib)
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    return decode(lib, (tmp_path / SAVE).read_bytes())


@needs_dm
def test_save_matches_the_golden_snapshot(tmp_path):
    got = digests(saved_game(tmp_path))
    if os.environ.get("DM_UPDATE_GOLDEN"):
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(got, indent=1) + "\n")
    assert got == json.loads(GOLDEN.read_text())
