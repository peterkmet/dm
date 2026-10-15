"""Golden snapshot of the dungeon view: every square of every map in all 4 directions draws the same pixels."""

import ctypes
import json
import os
import zlib

import pytest

from test_port import ROOT
from test_view import game, lib, needs_game  # noqa: F401 (fixtures)

GOLDEN = ROOT / "tests" / "golden" / "view.json"
SEED = 0x1234ABCD
MAP_COUNT = 14  # Dungeon.DAT of Dungeon Master


def map_rows(lib, m):
    """CRC-32 of the frame buffer of all views of each row of map m."""
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    rows = []
    for y in range(lib.dungeon_map_height(m)):
        crc = 0
        for x in range(lib.dungeon_map_width(m)):
            for d in range(4):
                ctypes.c_uint32.in_dll(lib, "g_random_seed").value = SEED
                ctypes.memset(ctypes.addressof(fb), 0, 320 * 200)
                lib.view_render(m, x, y, d)
                crc = zlib.crc32(bytes(fb), crc)
        rows.append(crc)
    return rows


@needs_game
@pytest.mark.parametrize("m", range(MAP_COUNT))
def test_view_matches_the_golden_snapshot(game, m):
    lib, _ = game
    for name in ("dungeon_map_count", "dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = ctypes.c_int16
    lib.view_render.restype = ctypes.c_bool
    assert lib.dungeon_map_count() == MAP_COUNT
    got = map_rows(lib, m)
    golden = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        golden[str(m)] = got
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(golden, indent=None, sort_keys=True) + "\n")
    assert got == golden[str(m)]
