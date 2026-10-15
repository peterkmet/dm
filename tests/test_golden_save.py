"""Golden snapshot of a save file: the saved state of a fixed game stays the same through refactors."""

import ctypes
import hashlib
import json
import os
import shutil
import subprocess

import pytest

from test_gameloop import Game
from test_port import ROOT
from test_save import GAME, SAVE, recruit_and_hold_object
from test_savefile import decode

c16, u16 = ctypes.c_int16, ctypes.c_uint16

GOLDEN = ROOT / "tests" / "golden" / "save.json"
SEED = 0x1234ABCD
GROUP_BYTES = 16  # sizeof(ActiveGroup)

needs_game = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists(), reason="game files missing")


def digests(parts):
    return {"header": hashlib.sha256(parts["header"]).hexdigest(),
            "blocks": [hashlib.sha256(b).hexdigest() for b in parts["blocks"]],
            "portraits": hashlib.sha256(parts["portraits"]).hexdigest(),
            "dungeon": hashlib.sha256(parts["dungeon"]).hexdigest()}


def scrub_free_groups(lib):
    """Zero the bytes of free active-group entries: the port leaves them uninitialized, and the save copies them."""
    base = ctypes.c_void_p.in_dll(lib, "g_active_groups").value
    for i in range(ctypes.c_uint16.in_dll(lib, "g_max_active_group_count").value):
        entry = base + i * GROUP_BYTES
        if c16.from_address(entry).value == -1:
            ctypes.memset(entry + 2, 0, GROUP_BYTES - 2)


def saved_game(tmp_path):
    """A new game with a fixed seed: recruit, hold an object, save; returns the decoded parts."""
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    # a private copy: an xdist worker shares one image per path, so earlier modules would leave their state
    copy = tmp_path / "libdm.dylib"
    shutil.copy(ROOT / "build/libdm.dylib", copy)
    lib = ctypes.CDLL(str(copy))
    # the types that Game and recruit_and_hold_object need, as the fixture of tests/test_save.py
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = u16
    lib.get_next_thing.restype = u16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.inventory_get_object_weight.restype = u16
    lib.savegame_save.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_new_game(str(GAME).encode())
    ctypes.c_uint32.in_dll(lib, "g_random_seed").value = SEED
    game = Game(lib)
    recruit_and_hold_object(game)
    scrub_free_groups(lib)
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    return decode(lib, (tmp_path / SAVE).read_bytes())


@needs_game
def test_save_matches_the_golden_snapshot(tmp_path):
    got = digests(saved_game(tmp_path))
    if os.environ.get("DM_UPDATE_GOLDEN"):
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(got, indent=1) + "\n")
    assert got == json.loads(GOLDEN.read_text())
