import ctypes
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from dmtest.game import global_type
from dmtest.lib import load_lib
from dmtest.markers import needs_sdl

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
GAME_CSB = ROOT / "game-csb"
DM, CSB = 0, 1
VANILLA, ADVANCED, TURNBASED = 0, 1, 2
SCREEN_ADVENTURE, SCREEN_MODE, SCREEN_CSB, SCREEN_DONE = 0, 1, 2, 3
CSB_NEW_ADVENTURE, CSB_RESUME, CSB_QUIT = 0, 1, 2
UP, DOWN, ENTER, ESCAPE = 0, 1, 2, 3
COLOR_BLACK, COLOR_DARK_GRAY, COLOR_GOLD, COLOR_LIGHT_GRAY = 0, 1, 9, 13
CSB_FILES = ["APPB.FTL", "Graphics.DAT", "Dungeon.DAT", "disk-b/MINI.DAT"]
c16 = ctypes.c_int16
needs_dm = pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing")
needs_csb = pytest.mark.skipif(not GAME_CSB.exists(), reason="game-csb files missing")

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.adventure_csb_present.restype = ctypes.c_bool
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.select_restart.restype = ctypes.c_bool
    lib.select_item_box.restype = ctypes.c_bool
    lib.adventure_default_dir.restype = ctypes.c_char_p
    lib.adventure_save_name.restype = ctypes.c_char_p
    lib.adventure_backup_name.restype = ctypes.c_char_p
    lib.savegame_save.restype = c16
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


def csb_folder(root, missing=None):
    for name in CSB_FILES:
        if name != missing:
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).touch()
    return root


def test_adventure_of(lib):
    assert lib.adventure_of(b"dm") == DM
    assert lib.adventure_of(b"csb") == CSB
    for name in (b"DM", b"", b"x"):
        assert lib.adventure_of(name) == -1


def test_default_dirs(lib):
    assert lib.adventure_default_dir(DM) == b"game"
    assert lib.adventure_default_dir(CSB) == b"game-csb"


def test_save_names(lib):
    set_value(lib, "g_adventure", DM)
    assert (lib.adventure_save_name(), lib.adventure_backup_name()) == (b"DMGAME.DAT", b"DMGAME.BAK")
    set_value(lib, "g_adventure", CSB)
    assert (lib.adventure_save_name(), lib.adventure_backup_name()) == (b"CSBGAME.DAT", b"CSBGAME.BAK")
    set_value(lib, "g_adventure", DM)


def test_csb_present(lib, tmp_path):
    assert lib.adventure_csb_present(str(tmp_path).encode()) == 0
    assert lib.adventure_csb_present(str(csb_folder(tmp_path / "all")).encode()) == 1


@pytest.mark.parametrize("missing", CSB_FILES)
def test_csb_present_needs_every_file(lib, tmp_path, missing):
    assert lib.adventure_csb_present(str(csb_folder(tmp_path, missing)).encode()) == 0


@needs_csb
def test_csb_present_in_game_csb(lib):
    assert lib.adventure_csb_present(str(GAME_CSB).encode()) == 1


@needs_dm
def test_save_writes_csb_name(lib, tmp_path):
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    set_value(lib, "g_adventure", CSB)
    try:
        assert lib.savegame_save(str(tmp_path).encode()) == 1
        assert sorted(p.name for p in tmp_path.iterdir()) == ["CSBGAME.DAT"]
        assert lib.savegame_save(str(tmp_path).encode()) == 1
        assert sorted(p.name for p in tmp_path.iterdir()) == ["CSBGAME.BAK", "CSBGAME.DAT"]
    finally:
        set_value(lib, "g_adventure", DM)


def test_start_no_flags(lib):
    start(lib)
    assert lib.select_screen() == SCREEN_ADVENTURE


def test_start_both_flags(lib):
    start(lib, CSB, TURNBASED)
    assert lib.select_screen() == SCREEN_CSB
    start(lib, DM, TURNBASED)
    assert lib.select_screen() == SCREEN_DONE
    assert (value(lib, "g_adventure"), value(lib, "g_game_mode"), value(lib, "g_advanced_features")) == (DM, TURNBASED, 1)


def test_start_adventure_only(lib):
    start(lib, DM)
    assert lib.select_screen() == SCREEN_MODE


def test_start_mode_only_means_dm(lib):
    set_value(lib, "g_adventure", CSB)
    start(lib, -1, ADVANCED)
    assert lib.select_screen() == SCREEN_DONE
    assert value(lib, "g_adventure") == DM


def test_return_selects_dm(lib):
    start(lib)
    keys(lib, ENTER)
    assert value(lib, "g_adventure") == DM
    assert lib.select_screen() == SCREEN_MODE


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
    assert value(lib, "g_game_mode") == VANILLA
    start(lib, DM)
    keys(lib, DOWN, DOWN, DOWN, DOWN, ENTER)
    assert value(lib, "g_game_mode") == TURNBASED


@pytest.mark.parametrize("item, features", [(VANILLA, 0), (ADVANCED, 1), (TURNBASED, 1)])
def test_mode_screen_values(lib, item, features):
    start(lib, DM)
    keys(lib, *[DOWN] * item, ENTER)
    assert (value(lib, "g_game_mode"), value(lib, "g_advanced_features")) == (item, features)
    assert lib.select_screen() == SCREEN_DONE


def test_escape_back(lib):
    start(lib)
    keys(lib, ESCAPE)
    assert lib.select_screen() == SCREEN_ADVENTURE
    keys(lib, ENTER, ESCAPE)
    assert lib.select_screen() == SCREEN_ADVENTURE
    start(lib, DM)
    keys(lib, ESCAPE)
    assert lib.select_screen() == SCREEN_MODE


def test_escape_on_the_csb_screen(lib):
    start(lib, CSB)
    keys(lib, ENTER, ESCAPE)
    assert lib.select_screen() == SCREEN_MODE
    start(lib, CSB, VANILLA)
    keys(lib, ESCAPE)
    assert lib.select_screen() == SCREEN_CSB


def test_csb_disabled(lib):
    start(lib, csb_present=0)
    keys(lib, DOWN)
    click(lib, CSB)
    assert lib.select_screen() == SCREEN_ADVENTURE
    keys(lib, ENTER)
    assert value(lib, "g_adventure") == DM


def test_click_activates(lib):
    start(lib)
    lib.select_click(0, 0)
    assert lib.select_screen() == SCREEN_ADVENTURE
    click(lib, CSB)
    assert value(lib, "g_adventure") == CSB
    assert lib.select_screen() == SCREEN_MODE


@pytest.mark.parametrize("item", [CSB_NEW_ADVENTURE, CSB_RESUME, CSB_QUIT])
def test_csb_screen_choices(lib, item):
    start(lib)
    keys(lib, DOWN, ENTER, ENTER)
    assert lib.select_screen() == SCREEN_CSB
    keys(lib, *[DOWN] * item, ENTER)
    assert lib.select_screen() == SCREEN_DONE
    assert lib.select_csb_choice() == item
    assert value(lib, "g_quit_requested") == 0  # QUIT goes back to the first screen (main.c)


@needs_dm
def test_csb_screen_again_with_a_message(lib):
    load_font(lib, GAME)
    message_line = Box(0, 152, 320, 12)
    start(lib, CSB, VANILLA)
    keys(lib, ENTER)
    lib.select_start_csb(b"NOT AVAILABLE YET")
    assert lib.select_screen() == SCREEN_CSB
    lib.select_draw()
    assert colors_in(lib, message_line) == {COLOR_BLACK, COLOR_LIGHT_GRAY}
    keys(lib, DOWN)
    lib.select_draw()
    assert colors_in(lib, message_line) == {COLOR_BLACK}


def test_done_ignores_keys(lib):
    start(lib, DM, VANILLA)
    keys(lib, ESCAPE, DOWN, ENTER)
    assert lib.select_screen() == SCREEN_DONE
    assert (value(lib, "g_adventure"), value(lib, "g_game_mode")) == (DM, VANILLA)


def load_font(lib, folder):
    assert lib.gfx_open(str(folder / "Graphics.DAT").encode()) > 0
    assert lib.text_init() == 0


def colors_in(lib, box):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    return {fb[y * 320 + x] for y in range(box.y, box.y + box.h) for x in range(box.x, box.x + box.w)}


@needs_dm
def test_draw_items(lib):
    load_font(lib, GAME)
    start(lib)
    lib.select_draw()
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    assert fb[0] == fb[319] == fb[199 * 320] == fb[199 * 320 + 319] == COLOR_BLACK
    assert colors_in(lib, item_box(lib, DM)) == {COLOR_BLACK, COLOR_GOLD}
    assert colors_in(lib, item_box(lib, CSB)) == {COLOR_BLACK, COLOR_LIGHT_GRAY}
    keys(lib, DOWN)
    lib.select_draw()
    assert colors_in(lib, item_box(lib, DM)) == {COLOR_BLACK, COLOR_LIGHT_GRAY}
    assert colors_in(lib, item_box(lib, CSB)) == {COLOR_BLACK, COLOR_GOLD}


@needs_dm
def test_draw_disabled_color(lib):
    load_font(lib, GAME)
    start(lib, csb_present=0)
    lib.select_draw()
    assert colors_in(lib, item_box(lib, CSB)) == {COLOR_BLACK, COLOR_DARK_GRAY}


@needs_csb
def test_font_from_csb_graphics(lib):
    load_font(lib, GAME_CSB)
    start(lib)
    lib.select_draw()
    assert COLOR_GOLD in colors_in(lib, item_box(lib, DM))


def run_dm(*args, auto_enter=False):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    env.pop("DM_AUTO_ENTER", None)
    if auto_enter:
        env["DM_AUTO_ENTER"] = "1"
    return subprocess.run([str(ROOT / "build/dm"), *args, "--frames", "5"], cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)


@needs_sdl
def test_invalid_adventure_exits():
    result = run_dm("--adventure", "x")
    assert result.returncode == 1
    assert "--adventure must be dm or csb" in result.stderr


@needs_sdl
@needs_dm
def test_load_failure_leaves_no_control_socket(tmp_path):
    sock = Path("/tmp") / f"dm-select-{os.getpid()}.sock"
    sock.unlink(missing_ok=True)
    result = run_dm("--mode", "turnbased", "--data", str(tmp_path / "missing"), "--control", str(sock))
    assert result.returncode == 1
    assert "KAOS.FTL is missing or is not the file" in result.stderr
    assert not sock.exists()


@needs_sdl
def test_csb_missing_exits(tmp_path):
    result = run_dm("--adventure", "csb", "--data", str(tmp_path))
    assert result.returncode == 1
    assert f"CSB data is missing in {tmp_path}" in result.stderr


@needs_sdl
@needs_dm
@pytest.mark.parametrize("args, auto_enter", [
    (["--adventure", "dm", "--mode", "vanilla"], False),
    (["--mode", "advanced"], False),
    ([], True),
])
def test_flags_skip_screens(args, auto_enter):
    result = run_dm(*args, auto_enter=auto_enter)
    assert result.returncode == 0, result.stderr
    assert "adventure dm, mode" in result.stderr


@needs_sdl
@needs_dm
@pytest.mark.parametrize("args", [[], ["--adventure", "dm"]])
def test_selection_screen_runs(args):
    result = run_dm(*args)
    assert result.returncode == 0, result.stderr
    assert "adventure dm, mode" not in result.stderr


@needs_sdl
@needs_csb
def test_csb_screen_runs():
    result = run_dm("--adventure", "csb", "--mode", "vanilla")
    assert result.returncode == 0, result.stderr
    assert "adventure dm, mode" not in result.stderr


@pytest.mark.parametrize("adventure, mode, first", [(-1, -1, SCREEN_ADVENTURE), (DM, -1, SCREEN_MODE), (CSB, 0, SCREEN_CSB)])
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
