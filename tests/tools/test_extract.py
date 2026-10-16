# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib

from amitools.fs.ADFSVolume import ADFSVolume
from amitools.fs.blkdev.BlkDevFactory import BlkDevFactory
from amitools.fs.FSString import FSString

from dmtools.extract import extract


def make_adf(path):
    blkdev = BlkDevFactory().create(str(path), options={"size": "880K"})
    vol = ADFSVolume(blkdev)
    vol.create(FSString("Test"))
    root = vol.get_root_dir()
    root.create_file(FSString("KAOS.FTL"), b"\x61\x60abc")
    sub = root.create_dir(FSString("s"))
    sub.create_file(FSString("Startup-sequence"), b"BJELoad_R\n")
    vol.close()
    blkdev.close()


def test_extract_writes_files_and_returns_manifest(tmp_path):
    adf = tmp_path / "test.adf"
    make_adf(adf)
    out = tmp_path / "game"

    manifest = extract(adf, out)

    assert (out / "KAOS.FTL").read_bytes() == b"\x61\x60abc"
    assert (out / "s" / "Startup-sequence").read_bytes() == b"BJELoad_R\n"
    assert manifest == [
        ("KAOS.FTL", 5, hashlib.sha256(b"\x61\x60abc").hexdigest()),
        ("s/Startup-sequence", 10, hashlib.sha256(b"BJELoad_R\n").hexdigest()),
    ]
