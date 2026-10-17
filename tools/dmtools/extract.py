# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract all files from an Amiga ADF disk image and print a manifest."""

import argparse
import hashlib
from pathlib import Path

from amitools.fs.ADFSVolume import ADFSVolume
from amitools.fs.blkdev.BlkDevFactory import BlkDevFactory


def _walk(node, rel):
    for entry in node.get_entries_sorted_by_name():
        path = rel / entry.get_file_name().get_unicode_name()
        if entry.is_dir():
            yield from _walk(entry, path)
        else:
            yield path, entry.get_file_data()


def extract(adf_path, out_dir):
    """Write every file of the ADF below out_dir; return [(path, size, sha256)]."""
    blkdev = BlkDevFactory().open(str(adf_path), read_only=True)
    vol = ADFSVolume(blkdev)
    vol.open()
    manifest = []
    try:
        for rel, data in _walk(vol.get_root_dir(), Path()):
            target = Path(out_dir) / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            manifest.append((rel.as_posix(), len(data), hashlib.sha256(data).hexdigest()))
    finally:
        vol.close()
        blkdev.close()
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf")
    parser.add_argument("out_dir", nargs="?", default="game")
    args = parser.parse_args()
    for path, size, digest in extract(args.adf, args.out_dir):
        print(f"{digest}  {size:8d}  {path}")
