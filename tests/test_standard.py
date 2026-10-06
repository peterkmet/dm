"""Checks port/src against docs/coding-standard.md. Open exceptions: tests/standard_allowlist.txt."""

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "port" / "src"
ALLOWLIST = Path(__file__).with_name("standard_allowlist.txt")
VENDORED = {"dr_mp3.h"}
MAX_FUNCTION_LINES = 60

HEX = re.compile(r"\b0x[0-9a-fA-F]{2,}\b")
DECIMAL = re.compile(r"(?<![\w.])[1-9][0-9]+(?![\w.])")
DECOMPILER = re.compile(r"\b(?:[uisbcp]Var\d+|local_[0-9a-f]+|arg_[0-9a-f]+|param_\d+|in_[AD][0-7]\w*|CONCAT\d+|DAT_[0-9a-f]+|FUN_[0-9a-f]+)\b")
RECORD = re.compile(r"g_current_map\s*\[|get_thing_data\([^()]*\)\s*\[")
CONST_DEF = re.compile(r"\s*(?:static\s+)?const\s+\w+\s+[A-Z][A-Z0-9_]*\s*=")
ENUM = re.compile(r"\benum\b[^{};]*\{([^}]*)\}")
DEFINE = re.compile(r"^\s*#define\s+([A-Z][A-Z0-9_]*)\b", re.M)
DECL = re.compile(r"^((?:(?:static|inline|const|struct|unsigned)\s+)*[A-Za-z_]\w*[\s*]+)([a-z_]\w*)\s*\(", re.M)
PREFIXES = re.compile(r"^/\* Prefixes: ([\w ]+?) \*/$", re.M)
NUMBERED = re.compile(r"events?_?\d|_\d+_to_\d+")
PREDICATE = re.compile(r"(?:^|_)(?:is|has|can)_")
FUNC_START = re.compile(r"^(?:static\s+)?[A-Za-z_][\w *]*?\b([a-z_]\w*)\s*\([^;]*$")


def sources():
    return sorted(p for p in SRC.rglob("*.[ch]") if p.name not in VENDORED)


def rel(path):
    return path.relative_to(SRC).as_posix()


def allowed():
    """The (check, target) pairs of the allowlist. Each line: check target reason."""
    pairs = set()
    for line in ALLOWLIST.read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            check, target, _reason = line.split(maxsplit=2)
            pairs.add((check, target))
    return pairs


def strip(text):
    """The text without comments and string or character literals; line numbers stay."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group().count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r"\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'", '""', text)


def code_lines(path):
    """Numbered lines that are not constant definitions."""
    text = ENUM.sub(lambda m: "\n" * m.group().count("\n"), strip(path.read_text(errors="replace")))
    for number, line in enumerate(text.splitlines(), 1):
        if not line.lstrip().startswith("#") and not CONST_DEF.match(line):
            yield number, line


def report(check, found):
    """Fails with one allowlist key and the evidence for each violation."""
    assert not found, f"{check} violations (allowlist key, evidence):\n" + "\n".join(found)


def line_check(check, pattern, owners=()):
    ok = allowed()
    found = []
    for path in sources():
        if (check, rel(path)) in ok or path.name in owners:
            continue
        found += [f"{check} {rel(path)}  | {n}: {line.strip()}" for n, line in code_lines(path) if pattern.search(line)]
    report(check, found)


def declarations(path):
    """(return type, name) of each function that a header declares or defines inline."""
    text = strip(path.read_text(errors="replace"))
    return [(ret.replace("static", "").replace("inline", "").strip(), name) for ret, name in DECL.findall(text)
            if not ret.lstrip().startswith(("typedef", "return", "extern"))]


def test_no_hex_literals():
    line_check("hex", HEX)


def test_no_decimal_literals():
    line_check("decimal", DECIMAL)


def test_no_decompiler_names():
    line_check("decompiler", DECOMPILER)


def test_raw_record_access_only_in_accessors():
    line_check("record", RECORD, owners={"map.c", "map.h"})


def test_each_constant_has_one_definition():
    where = defaultdict(set)
    for path in sources():
        text = strip(path.read_text(errors="replace"))
        names = set(DEFINE.findall(text))
        for body in ENUM.findall(text):
            names.update(m.group(1) for item in body.split(",") if (m := re.match(r"\s*([A-Z][A-Z0-9_]*)\b", item)))
        for name in names:
            where[name].add(rel(path))
    ok = allowed()
    report("duplicate", [f"duplicate {name}  | {sorted(paths)}" for name, paths in sorted(where.items())
                         if len(paths) > 1 and ("duplicate", name) not in ok])


def test_headers_state_unique_prefixes():
    ok = allowed()
    owner, found = {}, []
    for path in sources():
        if path.suffix != ".h" or ("prefix", rel(path)) in ok:
            continue
        names = [name for _, name in declarations(path)]
        match = PREFIXES.search(path.read_text(errors="replace"))
        if not match:
            if names:
                found.append(f"prefix {rel(path)}  | no Prefixes line")
            continue
        prefixes = match.group(1).split()
        for prefix in prefixes:
            for other, other_path in owner.items():
                if prefix.startswith(other) or other.startswith(prefix):
                    found.append(f"prefix {rel(path)}  | {prefix} collides with {other} of {other_path}")
            owner[prefix] = rel(path)
        found += [f"prefix {rel(path)}  | {name}" for name in names if not name.startswith(tuple(prefixes))]
    report("prefix", found)


def test_no_event_numbers_in_function_names():
    ok = allowed()
    report("numbered", [f"numbered {name}  | {rel(path)}" for path in sources() if path.suffix == ".h"
                        for _, name in declarations(path) if NUMBERED.search(name) and ("numbered", name) not in ok])


def test_predicates_return_bool():
    ok = allowed()
    report("bool", [f"bool {rel(path)}  | {ret} {name}" for path in sources() if path.suffix == ".h"
                    and ("bool", rel(path)) not in ok
                    for ret, name in declarations(path) if PREDICATE.search(name) and ret != "bool"])


def test_functions_are_short():
    ok = allowed()
    found = []
    for path in sources():
        if path.suffix != ".c":
            continue
        name = start = None
        for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            match = FUNC_START.match(line)
            if match and name is None:
                name, start = match.group(1), number
            elif line.startswith("}") and name:
                key = f"{rel(path)}:{name}"
                if number - start + 1 > MAX_FUNCTION_LINES and ("long", key) not in ok:
                    found.append(f"long {key}  | {number - start + 1} lines")
                name = None
    report("long", found)
