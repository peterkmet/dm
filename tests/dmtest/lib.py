"""The test library: a private copy of build/libdm.dylib per load, so each test module has its own C globals."""

import ctypes
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
