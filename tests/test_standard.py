"""Checks port/src against docs/coding-standard.md. Open exceptions: tests/standard_allowlist.txt."""

import re
import shutil
import subprocess
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
RECORD = re.compile(r"g_current_map\s*\[|dungeon_get_thing_data\([^()]*\)\s*\[")
CONST_DEF = re.compile(r"\s*(?:static\s+)?const\s+\w+\s+[A-Z][A-Z0-9_]*\s*=")
ENUM = re.compile(r"\benum\b[^{};]*\{([^}]*)\}")
DEFINE = re.compile(r"^\s*#define\s+([A-Z][A-Z0-9_]*)\b", re.M)
DECL = re.compile(r"^((?:(?:static|inline|const|struct|unsigned)\s+)*[A-Za-z_]\w*[\s*]+)([a-z_]\w*)\s*\(", re.M)
PREFIXES = re.compile(r"^/\* Prefixes: ([\w ]+?) \*/$", re.M)
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


NUMBERED = re.compile(r"(?i:events?_?\d)|_\d+_to_\d+|(?:^|_)(?:BYTE|OFFSET|AT)_(?=[0-9A-Fa-f]*\d)[0-9A-Fa-f]{2,}(?:_|$)")
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")


def test_no_event_numbers_or_offsets_in_names():
    ok = allowed()
    where = defaultdict(set)
    for path in sources():
        for name in IDENTIFIER.findall(strip(path.read_text(errors="replace"))):
            if NUMBERED.search(name) and ("numbered", name) not in ok:
                where[name].add(rel(path))
    report("numbered", [f"numbered {name}  | {sorted(paths)}" for name, paths in sorted(where.items())])


def test_predicates_return_bool():
    ok = allowed()
    report("bool", [f"bool {rel(path)}  | {ret} {name}" for path in sources() if ("bool", rel(path)) not in ok
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


CONST_VALUE = re.compile(r"^\s*#define\s+([A-Z][A-Z0-9_]*)\s+\(?(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\)?\s*$", re.M)
ENUM_VALUE = re.compile(r"\b([A-Z][A-Z0-9_]*)\s*=\s*\(?(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\)?\s*(?=[,}])")
STATIC_CONST_VALUE = re.compile(r"\bconst\s+\w+\s+([A-Z][A-Z0-9_]*)\s*=\s*(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\s*;")
HEX_LAST_PARTS = {"MASK", "FLAG", "FLAGS", "BIT"}
DECIMAL_PARTS = {"COUNT", "SIZE", "BYTES", "TICKS", "MIN", "MAX", "LIMIT", "THRESHOLD", "WIDTH", "HEIGHT"}


def constant_values(path):
    """(name, literal) of each constant with a plain number value."""
    text = strip(path.read_text(errors="replace"))
    for pattern in (CONST_VALUE, ENUM_VALUE, STATIC_CONST_VALUE):
        yield from pattern.findall(text)


def notation_error(name, literal):
    """Why the literal has the wrong base for the kind of value that the name tells, or None."""
    parts = name.split("_")
    is_hex = literal.lstrip("-").lower().startswith("0x")
    if parts[-1] in HEX_LAST_PARTS:
        return None if is_hex or abs(int(literal, 0)) < 10 else "a mask or flag in decimal"
    if DECIMAL_PARTS & set(parts) and is_hex and parts[0] not in ("KAOS", "CSB", "SWSH"):
        return "a count, size or limit in hex"
    return None


def test_number_notation():
    ok = allowed()
    report("notation", [f"notation {name}  | {rel(path)}: {literal}: {error}" for path in sources()
                        for name, literal in constant_values(path)
                        if (error := notation_error(name, literal)) and ("notation", name) not in ok])


NAMED_SHIFT = re.compile(r"(?:<<|>>)\s*([A-Z][A-Z0-9_]*_SHIFT)\b")
PACKED = re.compile(r"(?:<<|>>)\s*(?:8|16|24)\b|\((?:u?int(?:8|16|32)_t|unsigned|int)\)\s*\((?:u?int(?:8|16|32)_t|unsigned|int)\)")
BYTE_ORDER = ("BE16", "WR16", "BE32", "WR32")


def test_no_packed_fields_at_call_sites():
    """Also a shift by a *_SHIFT constant of a header in a .c file of another module: the accessor belongs to the owner."""
    ok = allowed()
    owner = {name: path.stem for path in sources() if path.suffix == ".h"
             for name in DEFINE.findall(strip(path.read_text(errors="replace"))) if name.endswith("_SHIFT")}
    found = []
    for path in sources():
        if path.suffix == ".c" and ("packed", rel(path)) not in ok:
            found += [f"packed {rel(path)}  | {n}: {line.strip()}" for n, line in code_lines(path)
                      if PACKED.search(line) or any(owner.get(name, path.stem) != path.stem for name in NAMED_SHIFT.findall(line))]
    report("packed", found)


def test_byte_order_helpers_only_in_bytes_header():
    report("byteorder", [f"byteorder {rel(path)}  | {name}" for path in sources() if path.name != "bytes.h"
                         for name in DEFINE.findall(strip(path.read_text(errors="replace"))) if name in BYTE_ORDER])


KAOS_CALL = re.compile(r"\bkaos_\w+\s*\(")
KAOS_HEADER = re.compile(r'^\s*#\s*include\s*"kaos_addresses\.h"', re.M)
IMAGE_ADDRESS = re.compile(r"\b0x10[0-9a-fA-F]{4}\b")
ADDRESS_PREFIXES = ("KAOS_", "CSB_", "SWSH_")


def test_kaos_image_only_in_data():
    ok = allowed()
    found = []
    for path in sources():
        if path.parent.name == "data" or ("kaos", rel(path)) in ok:
            continue
        text = path.read_text(errors="replace")
        lines = [m.group() for m in KAOS_HEADER.finditer(text)]
        lines += [line for line in strip(text).splitlines() if KAOS_CALL.search(line) or IMAGE_ADDRESS.search(line)]
        for line in lines:
            found.append(f"kaos {rel(path)}  | {line.strip()}")
    report("kaos", found)


def test_each_image_address_has_one_name():
    names = defaultdict(set)
    for path in sources():
        for name, literal in CONST_VALUE.findall(strip(path.read_text(errors="replace"))):
            if IMAGE_ADDRESS.fullmatch(literal):
                names[(name.split("_")[0], literal.lower())].add(name)
    ok = allowed()
    report("address", [f"address {literal}  | {sorted(n)}" for (prefix, literal), n in sorted(names.items())
                       if (len(n) > 1 or not all(x.startswith(ADDRESS_PREFIXES) for x in n)) and ("address", literal) not in ok])


PLATFORM_CODE = re.compile(r"^\s*#\s*if.*\b_WIN32\b|^\s*#\s*include\s*<(?:unistd|dirent|strings|execinfo|fcntl)\.h>|^\s*#\s*include\s*<sys/", re.M)


def test_platform_code_only_in_platform():
    ok = allowed()
    report("platform", [f"platform {rel(path)}  | {m.group().strip()}" for path in sources()
                        if path.parent.name != "platform" and ("platform", rel(path)) not in ok
                        for m in PLATFORM_CODE.finditer(path.read_text(errors="replace"))])


EXTERN = re.compile(r"\bextern\b([^;]*);")
REDRAW_FLAG_WRITE = re.compile(r"\battributes\s*[|&]=\s*~?\s*\(?\s*CHAMPION_ATTR_|\bg_redraw\s*[|&]=")


def engine_globals():
    names = set()
    for path in (SRC / "engine").glob("*.h"):
        for decl in EXTERN.findall(strip(path.read_text(errors="replace"))):
            names.update(n for n in re.findall(r"\b(g_\w+)", decl) if not n.endswith("_hook"))
    return names


def test_ui_does_not_write_engine_variables():
    alt = "|".join(sorted(engine_globals(), key=len, reverse=True))
    target = rf"\b(?:{alt})\b(?:\s*\[[^\]]*\]|\s*(?:\.|->)\s*\w+)*"
    write = re.compile(rf"(?:\+\+|--)\s*{target}|{target}\s*(?:\+\+|--|(?:[-+*/|&^]|<<|>>)?=(?!=))")
    ok = allowed()
    report("engine-write", [f"engine-write {rel(path)}  | {n}: {line.strip()}" for path in sorted((SRC / "ui").glob("*.c"))
                            if ("engine-write", rel(path)) not in ok
                            for n, line in code_lines(path) if write.search(line) and not REDRAW_FLAG_WRITE.search(line)])


LAYERS = ["data", "engine", "ui", "platform"]
INCLUDE = re.compile(r'^\s*#include\s+"([\w./]+)"', re.M)


def test_layers():
    ok = allowed()
    home = {p.name: p.parent.name for p in SRC.rglob("*.h")}
    found = []
    for path in sources():
        layer = path.parent.name
        assert layer in LAYERS, f"{rel(path)} is not in a layer folder"
        text = path.read_text(errors="replace")
        for header in INCLUDE.findall(text):
            target = home.get(header)
            key = f"{rel(path)}:{header}"
            if target and LAYERS.index(target) > LAYERS.index(layer) and ("layer", key) not in ok:
                found.append(f"layer {key}  | {layer} includes {target}")
        if layer != "platform" and re.search(r"#include\s+<SDL", text):
            found.append(f"layer {rel(path)}:SDL  | SDL outside platform/")
    report("layer", found)


CLANG_FORMAT_MAJOR = 23


def test_sources_are_formatted():
    exe = shutil.which("clang-format")
    assert exe, "clang-format is missing: brew install clang-format"
    version = subprocess.run([exe, "--version"], capture_output=True, text=True).stdout
    assert re.search(rf"version {CLANG_FORMAT_MAJOR}\.", version), f"clang-format {CLANG_FORMAT_MAJOR} needed: {version.strip()}"
    result = subprocess.run([exe, "--dry-run", "--Werror", *map(str, sources())], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr[-4000:]
