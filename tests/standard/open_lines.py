# SPDX-License-Identifier: GPL-3.0-or-later
"""Adds one "open: epic N" allowlist line for each violation that tests/standard reports; --prune removes the
open lines that no check reports any more. Run from the repository root: uv run python tests/standard/open_lines.py [-k EXPR] [--prune]"""

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ALLOWLIST = Path(__file__).with_name("allowlist.txt")
NEVER_OPEN = {"format", "allowlist"}
AUDIO = ("engine/sound.", "ui/music", "ui/audio")
CONTROL = ("control/", "platform/transport", "platform/state_json")
ENGINE_EPIC = {"record": 5, "module": 5, "prefix": 5, "header": 5, "include": 5, "comment": 5, "order": 5,
               "global": 7, "dead-api": 7, "engine-write": 7}
LAYERS = ("data", "engine", "ui", "control", "platform")
PATH_IN_EVIDENCE = re.compile(r"\b((?:data|engine|ui|control|platform)/\w+\.[ch])\b")


def epic(check, target, evidence):
    """The epic of the final refactor that removes this violation (the table of package C in the plan)."""
    if check == "tests" or target.startswith("tests/"):
        return 12
    path = target.split(":")[0]
    if "/" not in path:
        found = PATH_IN_EVIDENCE.search(evidence)
        path = found.group(1) if found else path + "/"
    layer = path.split("/")[0]
    if path.startswith(AUDIO):
        return 9
    if path.startswith(CONTROL):
        return 10
    return {"data": 4, "ui": 8, "platform": 11}.get(layer) or (ENGINE_EPIC.get(check, 6) if layer == "engine" else 13)


def violations(pytest_args, ignore_allowlist):
    """(check, target, evidence) of each violation line of the standard check."""
    dump = Path(tempfile.mkdtemp()) / "violations.txt"
    env = {**os.environ, "DM_STANDARD_DUMP": str(dump)}
    if ignore_allowlist:
        env["DM_STANDARD_IGNORE_ALLOWLIST"] = "1"
    subprocess.run(["uv", "run", "pytest", "-q", "-p", "no:cacheprovider", str(Path(__file__).with_name("test_standard.py")),
                    *pytest_args], env=env, cwd=ROOT, capture_output=True)
    result = []
    for line in dump.read_text().splitlines() if dump.exists() else []:
        head, _, evidence = line.partition("  | ")
        check, target = head.split(maxsplit=1)
        result.append((check, target, evidence))
    return result


def main():
    args = [a for a in sys.argv[1:] if a != "--prune"]
    lines = [line for line in ALLOWLIST.read_text().splitlines()]
    if "--prune" in sys.argv:
        live = {(c, t) for c, t, _ in violations(args, ignore_allowlist=True)}
        kept, seen = [], set()
        for line in lines:
            key = tuple(line.split(maxsplit=2)[:2]) if line.strip() and not line.startswith("#") else None
            if line in seen or (key and line.endswith(tuple(f"open: epic {n}" for n in range(1, 14))) and key not in live):
                continue
            seen.add(line)
            kept.append(line)
        ALLOWLIST.write_text("\n".join(kept) + "\n")
        print(len(lines) - len(kept), "lines removed")
        return
    have = {tuple(line.split(maxsplit=2)[:2]) for line in lines if line.strip() and not line.startswith("#")}
    new = sorted({f"{c} {t} open: epic {epic(c, t, e)}" for c, t, e in violations(args, ignore_allowlist=False)
                  if c not in NEVER_OPEN and (c, t) not in have})
    ALLOWLIST.write_text("\n".join(lines + new) + "\n")
    print(len(new), "lines added")


if __name__ == "__main__":
    main()
