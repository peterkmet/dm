import ctypes
import os
import subprocess
import tempfile
from pathlib import Path

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")

PORT_SRC = Path(__file__).resolve().parent.parent / "src"
PORT_INCLUDES = [f"-I{PORT_SRC}", "-isystem", str(PORT_SRC.parent / "third_party")]


def game_sources():
    """The .c files of libdm: all layers without the SDL entry point and window, and without the Windows stubs."""
    return [str(p) for p in sorted(PORT_SRC.rglob("*.c")) if p.name not in ("main.c", "platform.c", "control_win.c")]


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


def pytest_configure(config):
    """Builds the port once before the workers start, so their make calls find it up to date."""
    if not hasattr(config, "workerinput"):
        subprocess.run(["make"], cwd=PORT_SRC.parent, check=True, capture_output=True)
