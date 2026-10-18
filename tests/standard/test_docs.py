# SPDX-License-Identifier: GPL-3.0-or-later
"""Repository checks of the docs: every relative Markdown link in the docs resolves."""

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINK = re.compile(r"\]\(([^)#\s]+)(?:#[^)]*)?\)")


def markdown_files():
    docs = sorted((ROOT / "docs").rglob("*.md"))
    return [ROOT / "README.md", ROOT / "CHANGELOG.md", ROOT / "CLAUDE.md", *docs]


def tracked_paths():
    files = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    return set(files) | {str(Path(f).parent) for f in files if f}


def test_doc_links_resolve():
    tracked = tracked_paths()
    broken = [f"{path.relative_to(ROOT)}: {target}" for path in markdown_files() if path.exists()
              for target in LINK.findall(path.read_text())
              if "://" not in target and not target.startswith("mailto:") and (path.parent / target).resolve().relative_to(ROOT).as_posix() not in tracked]
    assert not broken, "broken links:\n" + "\n".join(broken)
