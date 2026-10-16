import ctypes
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")

# Globals of libdm that hold a truth value: C bool, one byte.
BOOL_GLOBALS = frozenset({
    "g_party_is_sleeping",
    "g_party_dead",
    "g_cheat_light",
    "g_cheat_immortal",
    "g_cheat_food",
    "g_leader_empty_handed",
    "g_use_rope_to_climb_down_pit",
    "g_create_launcher_projectile",
    "g_fuse_sequence_requested",
    "g_saved_game_exists",
    "g_game_won",
    "g_advanced_features",
    "g_automap_open",
    "g_game_screen_inactive",
    "g_stop_waiting_for_input",
    "g_game_time_ticking",
    "g_game_frozen",
    "g_mouse_pointer_hidden",
    "g_input_received",
    "g_quit_requested",
    "g_restart_requested",
    "g_return_to_entrance",
    "g_action_area_contains_icons",
    "g_do_not_draw_fluxcages",
    "g_facing_alcove",
    "g_facing_fountain",
    "g_facing_vi_altar",
})


def global_type(name, default=ctypes.c_int16):
    """The ctypes type of a libdm global: c_bool for a truth flag, else default."""
    return ctypes.c_bool if name in BOOL_GLOBALS else default


FOUND = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)
MAKE_FOLDER = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_char_p)
LIST_FOLDER = ctypes.CFUNCTYPE(None, ctypes.c_char_p, FOUND, ctypes.c_void_p)


def set_folder_hooks(lib):
    """Sets the folder hooks of data/os.c, which main.c sets with SDL. Keep the result alive while lib uses the hooks."""
    def make_folder(path):
        os.makedirs(path, exist_ok=True)
        return os.path.isdir(path)

    def list_folder(path, found, data):
        for name in os.listdir(path) if os.path.isdir(path) else []:
            found(name, data)

    hooks = MAKE_FOLDER(make_folder), LIST_FOLDER(list_folder)
    ctypes.c_void_p.in_dll(lib, "g_os_make_folder_hook").value = ctypes.cast(hooks[0], ctypes.c_void_p).value
    ctypes.c_void_p.in_dll(lib, "g_os_list_folder_hook").value = ctypes.cast(hooks[1], ctypes.c_void_p).value
    return hooks


def clear_folder_hooks(lib):
    """Removes the hooks of set_folder_hooks before their callbacks are freed."""
    ctypes.c_void_p.in_dll(lib, "g_os_make_folder_hook").value = None
    ctypes.c_void_p.in_dll(lib, "g_os_list_folder_hook").value = None


ROOT = Path(__file__).resolve().parent.parent


def sdl3_installed():
    """SDL3 from Homebrew: the game binary needs it; the library does not."""
    if not shutil.which("brew"):
        return False
    prefix = subprocess.run(["brew", "--prefix", "sdl3"], capture_output=True, text=True).stdout.strip()
    return Path(prefix, "include/SDL3/SDL.h").exists()


def pytest_configure(config):
    """Builds the library, and the game when SDL3 is there, once in the controller before the workers start."""
    if hasattr(config, "workerinput"):
        return
    result = subprocess.run(["make", "all" if sdl3_installed() else "lib"], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise pytest.UsageError("make failed:\n" + result.stderr[-4000:])
