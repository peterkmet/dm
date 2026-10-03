import csv
import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / "docs" / "kaos-symbols.csv"
DECOMP = ROOT / "decomp" / "KAOS.c"
CODE_BASE = 0x104E68
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")


def rows():
    with CSV.open(newline="") as f:
        return list(csv.reader(f))


def test_csv_is_valid():
    header, *body = rows()
    assert header == ["address", "name", "signature", "subsystem"]
    addrs = [int(r[0], 16) for r in body]
    names = [r[1] for r in body]
    assert all(len(r) == 4 for r in body)
    assert len(set(addrs)) == len(addrs)
    assert len(set(names)) == len(names)
    assert all(IDENT.match(n) for n in names)


def test_function_rows_are_function_starts():
    if not DECOMP.exists():
        pytest.skip("decomp/KAOS.c not found")
    starts = {int(a, 16) for a in re.findall(r"^/\* (00[0-9a-f]{6}) \*/$", DECOMP.read_text(), re.M)}
    funcs = [int(r[0], 16) for r in rows()[1:] if int(r[0], 16) >= CODE_BASE]
    assert funcs
    assert [hex(a) for a in funcs if a not in starts] == []
