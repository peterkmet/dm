import os
import subprocess
import tempfile

import pytest

from dmtest.markers import ROOT, sdl3_installed

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")


def pytest_configure(config):
    """Builds the library, and the game when SDL3 is there, once in the controller before the workers start."""
    if hasattr(config, "workerinput"):
        return
    result = subprocess.run(["make", "all" if sdl3_installed() else "lib"], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise pytest.UsageError("make failed:\n" + result.stderr[-4000:])
