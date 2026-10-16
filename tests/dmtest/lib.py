# SPDX-License-Identifier: GPL-3.0-or-later
"""The test library: a private copy of build/libdm.dylib per load, so each test module has its own C globals."""

import ctypes
import os
import shutil
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIBRARY = ROOT / "build" / "libdm.dylib"


def load_lib():
    """Loads a new private copy of the library; conftest.py builds the library before the tests start."""
    folder = Path(tempfile.mkdtemp(prefix="libdm-"))
    copy = folder / LIBRARY.name
    shutil.copy2(LIBRARY, copy)
    lib = ctypes.CDLL(str(copy))
    shutil.rmtree(folder)  # the loaded image stays mapped after the file is gone
    return lib


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
