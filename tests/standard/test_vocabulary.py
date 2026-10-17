# SPDX-License-Identifier: GPL-3.0-or-later
"""Every name of the protocol tables in the C code appears in the Protocol vocabulary section of the docs."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLE = re.compile(r"static const char \*const (\w+)\[[^\]]*\] = \{(.*?)\};", re.S)
ACTION = re.compile(r'\{"(\w+)",\s*\w+\s*\}')


def section():
    text = (ROOT / "docs/llm/llm-mode.md").read_text()
    start = text.index("## Protocol vocabulary")
    end = text.find("\n## ", start + 1)
    return text[start : end if end > 0 else None]


def protocol_names():
    names = set()
    state = (ROOT / "src/control/state_json.c").read_text()
    for _, body in TABLE.findall(state):
        names |= set(re.findall(r'"([^"]+)"', body))
    names |= set(ACTION.findall((ROOT / "src/control/control_actions.c").read_text()))
    return names


def test_every_protocol_name_is_documented():
    documented = section()
    missing = sorted(name for name in protocol_names() if f"`{name}`" not in documented)
    assert not missing, f"names missing in docs/llm/llm-mode.md, Protocol vocabulary: {missing}"
