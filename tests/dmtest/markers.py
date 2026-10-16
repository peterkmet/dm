"""Skip markers for tests that need game files or programs that a machine may not have."""

import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
GAME, GAME_CSB = ROOT / "game", ROOT / "game-csb"
MINI = GAME_CSB / "disk-b" / "MINI.DAT"
DM_FILES = ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")
CSB_FILES = ("APPB.FTL", "Graphics.DAT", "Dungeon.DAT")


def _brew_path(formula, relative):
    if not shutil.which("brew"):
        return None
    prefix = subprocess.run(["brew", "--prefix", formula], capture_output=True, text=True).stdout.strip()
    path = Path(prefix, relative)
    return path if prefix and path.exists() else None


def sdl3_installed():
    """clang and SDL3 from Homebrew: the game binary needs both; the library does not."""
    return bool(shutil.which("clang")) and _brew_path("sdl3", "include/SDL3/SDL.h") is not None


def needs_file(path, reason=None):
    return pytest.mark.skipif(not Path(path).exists(), reason=reason or f"{Path(path).name} missing")


needs_dm = pytest.mark.skipif(not all((GAME / f).exists() for f in DM_FILES), reason="game files missing")
needs_csb = pytest.mark.skipif(not all((GAME_CSB / f).exists() for f in CSB_FILES), reason="game-csb files missing")
needs_csb_mini = needs_file(MINI, "game-csb/disk-b/MINI.DAT missing")
needs_sdl = pytest.mark.skipif(not _brew_path("sdl3", "include/SDL3/SDL.h"), reason="SDL3 not installed")
needs_ghidra = pytest.mark.skipif(not _brew_path("ghidra", "libexec/support/analyzeHeadless"), reason="Ghidra not installed")
