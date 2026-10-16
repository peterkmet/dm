"""The CSB save file: header moved by 2 bytes, block 2 checksum, active groups (docs/csb-data.md "Save file")."""

import ctypes
import shutil

import pytest

from dmtest import constants as C
from dmtest.game import Game, global_type, state
from dmtest.lib import load_lib
from dmtest.markers import GAME_CSB, needs_csb, needs_csb_mini
from dmtest.savefile import SaveFile

MINI = GAME_CSB / "disk-b" / "MINI.DAT"
CEDT = GAME_CSB / "disk-b" / "CEDT.FTL"
DM, CSB = 0, 1
LOADED, DAMAGED = 1, -1
NAME = b"CSBGAME.DAT"
CHAMPION_BYTES, MAGIC_MAP = 0x140, 0x118
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

pytestmark = [needs_csb, needs_csb_mini]


@pytest.fixture(scope="module")
def game():
    """A private copy of libdm with the CSB data loaded."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.kaos_s16.restype = c16
    lib.dungeon_save.restype = ctypes.c_void_p
    lib.free.argtypes = [ctypes.c_void_p]
    lib.savegame_save_as.restype = ctypes.c_bool
    lib.savegame_load_from.restype = c16
    lib.dungeon_map_count.restype = c16
    lib.save_decode.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(SaveFile)]
    lib.save_decode.restype = ctypes.c_bool
    lib.save_free.argtypes = [ctypes.POINTER(SaveFile)]
    c16.in_dll(lib, "g_adventure").value = CSB
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    yield Game(lib)
    c16.in_dll(lib, "g_adventure").value = DM


def value(lib, name, kind=c16):
    return global_type(name, kind).in_dll(lib, name).value


def load_mini(game, tmp_path):
    shutil.copy(MINI, tmp_path / "MINI.DAT")
    assert game.lib.savegame_load_from(str(tmp_path).encode(), b"MINI.DAT", None) == LOADED


def save(game, folder):
    folder.mkdir(exist_ok=True)
    assert game.lib.savegame_save_as(str(folder).encode(), NAME, None) == 1
    return (folder / NAME.decode()).read_bytes()


def test_mini_decodes_like_csb_mini(game):
    from dmtools import csb_mini as cm

    raw = MINI.read_bytes()
    py = cm.decode_save(raw)
    s = SaveFile()
    assert game.lib.save_decode(raw, len(raw), ctypes.byref(s)) == 1
    try:
        assert py["layout"] == 2
        assert ctypes.string_at(s.header, 512) == bytes(py["header"])
        for i in range(5):
            assert ctypes.string_at(s.block[i], s.block_size[i]) == bytes(py["blocks"][i]), i
        assert ctypes.string_at(s.portraits, 4 * 464) == py["portraits"]
        assert ctypes.string_at(s.dungeon, s.dungeon_size) == cm.dungeon_dat(py)
    finally:
        game.lib.save_free(ctypes.byref(s))


def test_mini_loads_the_csb_start(game, tmp_path):
    from dmtools import csb_mini as cm

    load_mini(game, tmp_path)
    lib = game.lib
    assert [value(lib, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")] == [4, 22, 18, 2]
    assert lib.dungeon_map_count() == 11
    saved = cm.w(cm.decode_save(MINI.read_bytes())["blocks"][0], C.G_MAX_ACTIVE_GROUP_COUNT)
    assert value(lib, "g_max_active_group_count", u16) == saved < 110


def test_loaded_game_saves_and_loads_back(game, tmp_path):
    from dmtools import csb_mini as cm

    load_mini(game, tmp_path)
    for i in range(4):
        game.champions[i].magic_map[:] = [i + 1, 2, 3, 4, 0x5A, 0xC3]
    before = state(game)
    raw = save(game, tmp_path / "out")
    assert game.lib.savegame_load_from(str(tmp_path / "out").encode(), NAME, None) == LOADED
    assert state(game) == before
    py = cm.decode_save(raw)
    assert py["layout"] == 2 and py["format"] == 3
    for i in range(4):
        assert bytes(py["blocks"][2][i * CHAMPION_BYTES + MAGIC_MAP:][:6]) == bytes([i + 1, 2, 3, 4, 0x5A, 0xC3])


def test_new_prison_game_saves_and_loads_back(game, tmp_path):
    from dmtools import csb_mini as cm

    assert game.lib.gameloop_start_new_game(str(GAME_CSB).encode())
    assert value(game.lib, "g_max_active_group_count", u16) == 60
    before = state(game)
    raw = save(game, tmp_path)
    assert game.lib.savegame_load_from(str(tmp_path).encode(), NAME, None) == LOADED
    assert state(game) == before
    assert cm.w(cm.decode_save(raw)["blocks"][0], C.G_MAX_ACTIVE_GROUP_COUNT) == 60


@pytest.mark.skipif(not CEDT.exists(), reason="CEDT.FTL missing")
def test_original_reader_accepts_the_file(game, tmp_path):
    from dmtest.cedt import check_decode

    load_mini(game, tmp_path)
    raw = save(game, tmp_path / "out")
    check_decode(raw, "port CSBGAME.DAT")


def test_damaged_champion_block_is_rejected(game, tmp_path):
    from dmtools import csb_mini as cm

    load_mini(game, tmp_path)
    good = save(game, tmp_path)
    bad = bytearray(good)
    groups = value(game.lib, "g_max_active_group_count", u16)
    pos = 512 + 128 + groups * 16 + 0x40  # inside block 2: only CSB checks its checksum
    plain = cm.decode_save(good)["blocks"][2][0x41]
    # a bit changes the checksum only when its key bit is 0 and it is not bit 15 of the word: use the low byte
    same = ~(good[pos + 1] ^ plain) & 0xFF
    assert same
    bad[pos + 1] ^= same & -same
    (tmp_path / NAME.decode()).write_bytes(bad)
    assert game.lib.savegame_load_from(str(tmp_path).encode(), NAME, None) == DAMAGED
    (tmp_path / NAME.decode()).write_bytes(good)
    assert game.lib.savegame_load_from(str(tmp_path).encode(), NAME, None) == LOADED
