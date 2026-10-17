# SPDX-License-Identifier: GPL-3.0-or-later
"""The ADF reader of the port against amitools, the game file check, and the disk install."""

import ctypes
from pathlib import Path

import pytest

from dmtest import constants as C
from dmtest.lib import clear_folder_hooks, load_lib, set_folder_hooks
from dmtest.markers import ROOT
from amitools.fs.ADFSVolume import ADFSVolume
from amitools.fs.blkdev.BlkDevFactory import BlkDevFactory
from amitools.fs.FSString import FSString

from dmtools.extract import extract

MEDIA = ROOT / "reference" / "media"
DISKS = {
    "Dungeon Master v3.60.adf": "game",
    "Chaos Strikes Back (FTL) A.adf": "game-csb",
    "Chaos Strikes Back (FTL) B.adf": "game-csb/disk-b",
}
DM, CSB = 0, 1
SMALL, LARGE = b"\x61\x60abc", bytes(range(256)) * 160  # LARGE spans 84 data blocks

needs_disks = pytest.mark.skipif(not all((MEDIA / d).exists() for d in DISKS), reason="ADF images missing")


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    lib.adf_has_file.restype = ctypes.c_bool
    lib.adventure_find_bad_file.restype = ctypes.c_char_p
    lib.adventure_find_bad_file.argtypes = [ctypes.c_int16, ctypes.c_char_p]
    hooks = set_folder_hooks(lib)
    yield lib
    clear_folder_hooks(lib)
    del hooks


def make_adf(path):
    blkdev = BlkDevFactory().create(str(path), options={"size": "880K"})
    vol = ADFSVolume(blkdev)
    vol.create(FSString("Test"))
    root = vol.get_root_dir()
    root.create_file(FSString("KAOS.FTL"), SMALL)
    root.create_file(FSString("Graphics.DAT"), LARGE)
    root.create_file(FSString("empty"), b"")
    sound = root.create_dir(FSString("Sound"))
    sound.create_file(FSString("tele2.amg"), b"\x00\x02\x7f\x80")
    vol.close()
    blkdev.close()


def tree(folder):
    return {p.relative_to(folder).as_posix(): p.read_bytes() for p in sorted(Path(folder).rglob("*")) if p.is_file()}


def test_extract_matches_amitools(lib, tmp_path):
    adf = tmp_path / "test.adf"
    make_adf(adf)

    assert lib.adf_extract(str(adf).encode(), str(tmp_path / "port" / "deep").encode())

    extract(adf, tmp_path / "ref")
    assert tree(tmp_path / "port" / "deep") == tree(tmp_path / "ref")
    assert tree(tmp_path / "ref")["Graphics.DAT"] == LARGE


def test_has_file_root_only_any_case(lib, tmp_path):
    adf = tmp_path / "test.adf"
    make_adf(adf)
    path = str(adf).encode()

    assert lib.adf_has_file(path, b"kaos.ftl")
    assert not lib.adf_has_file(path, b"Sound")  # a folder
    assert not lib.adf_has_file(path, b"tele2.amg")  # not in the root folder
    assert not lib.adf_has_file(str(tmp_path / "none.adf").encode(), b"KAOS.FTL")


def test_extract_rejects_other_files(lib, tmp_path):
    bad = tmp_path / "bad.adf"
    bad.write_bytes(b"DOS\x00" + bytes(1000))

    assert not lib.adf_extract(str(bad).encode(), str(tmp_path / "out").encode())


@pytest.mark.parametrize("name, safe", [("Dungeon.DAT", True), ("a\\b", False), ("a:b", False), ("a*b", False), ("a?b", False),
                                        ('a"b', False), ("a<b", False), ("a>b", False), ("a|b", False), ("a\x01b", False),
                                        ("a/b", False), ("", False), (".", False), ("..", False)])
def test_unsafe_amiga_names_are_rejected(lib, name, safe):
    lib.adf_is_safe_name.restype = ctypes.c_bool
    lib.adf_is_safe_name.argtypes = [ctypes.c_char_p]
    assert lib.adf_is_safe_name(name.encode("latin-1")) == safe


@needs_disks
@pytest.mark.parametrize("disk", DISKS)
def test_game_disk_matches_amitools(lib, tmp_path, disk):
    assert lib.adf_extract(str(MEDIA / disk).encode(), str(tmp_path / "port").encode())

    extract(MEDIA / disk, tmp_path / "ref")
    assert tree(tmp_path / "port") == tree(tmp_path / "ref")


@needs_disks
def test_install_disks_then_files_pass_the_check(lib, tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    folder = ctypes.c_char_p()
    for disk, target in DISKS.items():
        assert lib.adventure_install_disk(str(MEDIA / disk).encode(), ctypes.byref(folder)) == C.ADVENTURE_DISK_INSTALLED
        assert folder.value.decode() == target
    assert lib.adventure_install_disk(str(MEDIA / "Dungeon Master v3.60.adf").encode(), ctypes.byref(folder)) == C.ADVENTURE_DISK_SKIPPED

    assert lib.adventure_find_bad_file(DM, b"game") is None
    assert lib.adventure_find_bad_file(CSB, b"game-csb") is None


@needs_disks
def test_bad_file_names_a_changed_or_missing_file(lib, tmp_path):
    lib.adf_extract(str(MEDIA / "Dungeon Master v3.60.adf").encode(), str(tmp_path).encode())
    dungeon = tmp_path / "Dungeon.DAT"
    dungeon.write_bytes(dungeon.read_bytes() + b"\x00")

    assert lib.adventure_find_bad_file(DM, str(tmp_path).encode()) == b"Dungeon.DAT"
    assert lib.adventure_find_bad_file(CSB, str(tmp_path).encode()) == b"APPB.FTL"  # the DM APPB.FTL is not the CSB file
    assert lib.adventure_find_bad_file(DM, str(tmp_path / "none").encode()) == b"KAOS.FTL"
