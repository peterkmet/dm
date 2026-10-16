"""Repository checks of the docs: every relative Markdown link outside docs/superpowers/ resolves."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINK = re.compile(r"\]\(([^)#\s]+)(?:#[^)]*)?\)")


def markdown_files():
    docs = [p for p in sorted((ROOT / "docs").rglob("*.md")) if "superpowers" not in p.relative_to(ROOT / "docs").parts]
    return [ROOT / "README.md", ROOT / "CHANGELOG.md", ROOT / "CLAUDE.md", *docs]


def test_doc_links_resolve():
    broken = [f"{path.relative_to(ROOT)}: {target}" for path in markdown_files() if path.exists()
              for target in LINK.findall(path.read_text())
              if "://" not in target and not target.startswith("mailto:") and not (path.parent / target).exists()]
    assert not broken, "broken links:\n" + "\n".join(broken)
