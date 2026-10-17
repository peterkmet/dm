# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes

import pytest

from dmtest import constants as C
from dmtest.game import Box, global_type, session
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

DM, CSB = 0, 1
VANILLA, ADVANCED, TURNBASED = 0, 1, 2
UP, DOWN, ENTER, ESCAPE = 0, 1, 2, 3
CSB_FILES = ["APPB.FTL", "Graphics.DAT", "Dungeon.DAT", "disk-b/MINI.DAT"]
c16 = ctypes.c_int16



class Adventure(ctypes.Structure):
    _fields_ = [(n, ctypes.c_char_p) for n in ("name", "dir", "title", "engine", "save", "backup")]


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.adventure_is_csb_present.restype = ctypes.c_bool
    lib.adventure_get.restype = ctypes.POINTER(Adventure)
    lib.adventure_find.restype = c16
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.select_restart.restype = ctypes.c_bool
    lib.select_item_box.restype = ctypes.c_bool
    lib.text_init.restype = ctypes.c_bool
    lib.select_screen.restype = c16
    lib.select_csb_choice.restype = c16
    lib.savegame_save.restype = ctypes.c_bool
    yield lib
    set_value(lib, "g_adventure", DM)


def value(lib, name):
    return global_type(name, c16).in_dll(lib, name).value


def set_value(lib, name, v):
    global_type(name, c16).in_dll(lib, name).value = v


def start(lib, adventure=-1, mode=-1, csb_present=1):
    set_value(lib, "g_quit_requested", 0)
    lib.select_start(adventure, mode, csb_present, True)


def keys(lib, *ks):
    for k in ks:
        lib.select_key(k)


def item_box(lib, item):
    box = Box()
    assert lib.select_item_box(item, ctypes.byref(box))
    return box


def click(lib, item):
    box = item_box(lib, item)
    lib.select_click(box.x + box.w // 2, box.y + box.h // 2)


def csb_folder(root, missing=()):
    for name in CSB_FILES:
        if name not in missing:
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).touch()
    return root


@pytest.mark.parametrize(
    "missing, present",
    [((), 1), (CSB_FILES, 0)] + [((name,), 0) for name in CSB_FILES],
    ids=["full", "empty"] + list(CSB_FILES),
)
def test_csb_present_needs_every_file(lib, tmp_path, missing, present):
    assert lib.adventure_is_csb_present(str(csb_folder(tmp_path, missing)).encode()) == present


@needs_dm
def test_save_writes_csb_name(lib, tmp_path):
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    set_value(lib, "g_adventure", CSB)
    try:
        assert lib.savegame_save(str(tmp_path).encode()) is True
        assert sorted(p.name for p in tmp_path.iterdir()) == ["CSBGAME.DAT"]
        assert lib.savegame_save(str(tmp_path).encode()) is True
        assert sorted(p.name for p in tmp_path.iterdir()) == ["CSBGAME.BAK", "CSBGAME.DAT"]
    finally:
        set_value(lib, "g_adventure", DM)


def test_start_no_flags(lib):
    start(lib)
    assert lib.select_screen() == C.SELECT_SCREEN_ADVENTURE


def test_start_both_flags(lib):
    start(lib, CSB, TURNBASED)
    assert lib.select_screen() == C.SELECT_SCREEN_CSB
    start(lib, DM, TURNBASED)
    assert lib.select_screen() == C.SELECT_SCREEN_DONE
    assert (value(lib, "g_adventure"), session(lib).mode, value(lib, "g_advanced_features")) == (DM, TURNBASED, 1)


def test_start_adventure_only(lib):
    start(lib, DM)
    assert lib.select_screen() == C.SELECT_SCREEN_MODE


def test_start_mode_only_means_dm(lib):
    set_value(lib, "g_adventure", CSB)
    start(lib, -1, ADVANCED)
    assert lib.select_screen() == C.SELECT_SCREEN_DONE
    assert value(lib, "g_adventure") == DM


def test_return_selects_dm(lib):
    start(lib)
    keys(lib, ENTER)
    assert value(lib, "g_adventure") == DM
    assert lib.select_screen() == C.SELECT_SCREEN_MODE


def test_arrows_select_csb(lib):
    start(lib)
    keys(lib, DOWN, ENTER)
    assert value(lib, "g_adventure") == CSB
    start(lib)
    keys(lib, DOWN, UP, ENTER)
    assert value(lib, "g_adventure") == DM


def test_cursor_stops_at_the_list_ends(lib):
    start(lib, DM)
    keys(lib, UP, ENTER)
    assert session(lib).mode == VANILLA
    start(lib, DM)
    keys(lib, DOWN, DOWN, DOWN, DOWN, ENTER)
    assert session(lib).mode == TURNBASED


@pytest.mark.parametrize("item, features", [(VANILLA, 0), (ADVANCED, 1), (TURNBASED, 1)])
def test_mode_screen_values(lib, item, features):
    start(lib, DM)
    keys(lib, *[DOWN] * item, ENTER)
    assert (session(lib).mode, value(lib, "g_advanced_features")) == (item, features)
    assert lib.select_screen() == C.SELECT_SCREEN_DONE


def test_escape_back(lib):
    start(lib)
    keys(lib, ESCAPE)
    assert lib.select_screen() == C.SELECT_SCREEN_ADVENTURE
    keys(lib, ENTER, ESCAPE)
    assert lib.select_screen() == C.SELECT_SCREEN_ADVENTURE
    start(lib, DM)
    keys(lib, ESCAPE)
    assert lib.select_screen() == C.SELECT_SCREEN_MODE


def test_escape_on_the_csb_screen(lib):
    start(lib, CSB)
    keys(lib, ENTER, ESCAPE)
    assert lib.select_screen() == C.SELECT_SCREEN_MODE
    start(lib, CSB, VANILLA)
    keys(lib, ESCAPE)
    assert lib.select_screen() == C.SELECT_SCREEN_CSB


def test_csb_disabled(lib):
    start(lib, csb_present=0)
    keys(lib, DOWN)
    click(lib, CSB)
    assert lib.select_screen() == C.SELECT_SCREEN_ADVENTURE
    keys(lib, ENTER)
    assert value(lib, "g_adventure") == DM


def test_click_activates(lib):
    start(lib)
    lib.select_click(0, 0)
    assert lib.select_screen() == C.SELECT_SCREEN_ADVENTURE
    click(lib, CSB)
    assert value(lib, "g_adventure") == CSB
    assert lib.select_screen() == C.SELECT_SCREEN_MODE


@pytest.mark.parametrize("item", [C.SELECT_CSB_NEW_ADVENTURE, C.SELECT_CSB_RESUME, C.SELECT_CSB_QUIT])
def test_csb_screen_choices(lib, item):
    start(lib)
    keys(lib, DOWN, ENTER, ENTER)
    assert lib.select_screen() == C.SELECT_SCREEN_CSB
    keys(lib, *[DOWN] * item, ENTER)
    assert lib.select_screen() == C.SELECT_SCREEN_DONE
    assert lib.select_csb_choice() == item
    assert value(lib, "g_quit_requested") == 0  # QUIT goes back to the first screen (main.c)


def test_done_ignores_keys(lib):
    start(lib, DM, VANILLA)
    keys(lib, ESCAPE, DOWN, ENTER)
    assert lib.select_screen() == C.SELECT_SCREEN_DONE
    assert (value(lib, "g_adventure"), session(lib).mode) == (DM, VANILLA)


@pytest.mark.parametrize("adventure, mode, first", [(-1, -1, C.SELECT_SCREEN_ADVENTURE), (DM, -1, C.SELECT_SCREEN_MODE), (CSB, 0, C.SELECT_SCREEN_CSB)])
def test_restart_goes_back_to_the_first_screen(lib, adventure, mode, first):
    """QUIT on the DM entrance or the CSB screen: the first screen that the flags do not skip."""
    start(lib, adventure, mode)
    keys(lib, *[ENTER] * 3)
    assert lib.select_restart() == 1
    assert lib.select_screen() == first


@pytest.mark.parametrize("adventure", [DM, -1])
def test_restart_with_every_screen_skipped(lib, adventure):
    start(lib, adventure, 0)  # --mode alone means DM
    assert lib.select_restart() == 0
