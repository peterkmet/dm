import ctypes
import os
import subprocess
import tempfile
from pathlib import Path

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")

PORT_SRC = Path(__file__).resolve().parent.parent / "port" / "src"
PORT_INCLUDES = [f"-I{PORT_SRC / layer}" for layer in ("data", "engine", "ui", "platform")]


def game_sources():
    """The .c files of libdm: all layers without the SDL entry point and window, and without the Windows stubs."""
    return [str(p) for p in sorted(PORT_SRC.rglob("*.c")) if p.name not in ("main.c", "platform.c", "control_win.c")]


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
        subprocess.run(["make", "-C", "port"], cwd=PORT_SRC.parent.parent, check=True, capture_output=True)
