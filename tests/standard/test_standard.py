"""Checks src/ against docs/coding-standard.md. Accepted exceptions: tests/standard/allowlist.txt.

DM_STANDARD_DUMP=<file> appends every violation line to the file; DM_STANDARD_IGNORE_ALLOWLIST=1 reports the
violations that the allowlist accepts too. tests/standard/open_lines.py uses both.
"""

import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict, namedtuple
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
TESTS = ROOT / "tests"
ALLOWLIST = Path(__file__).with_name("allowlist.txt")
LAYERS = ["data", "engine", "ui", "control", "platform"]
MAX_FUNCTION_LINES = 60
EXCEPTION_RULES = {"3.12", "4.8", "5.8"}  # the rules of docs/coding-standard.md that name an exception
OPEN_REASON = re.compile(r"open: epic (?:[1-9]|1[0-3])")

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
    return sorted(SRC.rglob("*.[ch]"))


def headers():
    return [p for p in sources() if p.suffix == ".h"]


def rel(path):
    return path.relative_to(SRC).as_posix()


def allowed():
    """The (check, target) pairs of the allowlist; none when DM_STANDARD_IGNORE_ALLOWLIST is set."""
    if os.environ.get("DM_STANDARD_IGNORE_ALLOWLIST"):
        return set()
    return {tuple(line.split(maxsplit=2)[:2]) for line in ALLOWLIST.read_text().splitlines()
            if line.strip() and not line.startswith("#")}


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
    if found and (dump := os.environ.get("DM_STANDARD_DUMP")):
        with open(dump, "a") as out:
            out.writelines(line + "\n" for line in found)
    assert not found, f"{check} violations (allowlist key, evidence):\n" + "\n".join(found)


def line_check(check, pattern, owners=(), ignore=None):
    """Each code line that matches pattern, outside the owner files; ignore removes allowed text first."""
    ok = allowed()
    found = []
    for path in sources():
        if (check, rel(path)) in ok or path.name in owners:
            continue
        found += [f"{check} {rel(path)}  | {n}: {line.strip()}" for n, line in code_lines(path)
                  if pattern.search(ignore.sub("", line) if ignore else line)]
    report(check, found)


def declarations(path):
    """(return type, name) of each function that a header declares or defines inline."""
    text = strip(path.read_text(errors="replace"))
    return [(ret.replace("static", "").replace("inline", "").strip(), name) for ret, name in DECL.findall(text)
            if not ret.lstrip().startswith(("typedef", "return", "extern"))]


Function = namedtuple("Function", "name ret line params static definition")


def parameter_list(text, start):
    """The text between the opening parenthesis just before start and its partner."""
    depth, i = 1, start
    while depth and i < len(text):
        depth += {"(": 1, ")": -1}.get(text[i], 0)
        i += 1
    return text[start:i - 1]


def split_parameters(params):
    """The parameters of a parameter list, without void."""
    parts, depth, current = [], 0, ""
    for ch in params:
        if ch == "," and depth == 0:
            parts.append(current)
            current = ""
            continue
        depth += {"(": 1, ")": -1, "[": 1, "]": -1}.get(ch, 0)
        current += ch
    parts.append(current)
    return [p.strip() for p in parts if p.strip() and p.strip() != "void"]


def functions(path):
    """Each function that the file declares or defines at column 0, with its parameters."""
    text = strip(path.read_text(errors="replace"))
    found = []
    for m in DECL.finditer(text):
        ret, name = m.group(1), m.group(2)
        if ret.lstrip().startswith(("typedef", "return", "extern", "else")):
            continue
        params = parameter_list(text, m.end())
        after = text[m.end() + len(params) + 1:].lstrip()
        found.append(Function(name, " ".join(ret.split()), text.count("\n", 0, m.start()) + 1,
                              split_parameters(params), "static" in ret.split(), after.startswith("{")))
    return found


def comment_above(lines, line):
    """The comment block that ends on the line above the 1-based line, or ''."""
    i = line - 2
    if i < 0 or not lines[i].rstrip().endswith("*/"):
        return ""
    j = i
    while j > 0 and "/*" not in lines[j]:
        j -= 1
    return "\n".join(lines[j:i + 1]) if lines[j].lstrip().startswith("/*") else ""


def definitions(path):
    """(name, first line, last line, static) of each function body at column 0 of a .c file; 1-based lines."""
    found, name = [], None
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        match = FUNC_START.match(line)
        if match and name is None:
            name, start, static = match.group(1), number, line.startswith("static")
        elif line.startswith("}") and name:
            found.append((name, start, number, static))
            name = None
    return found


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
        for name, start, end, _static in definitions(path):
            key = f"{rel(path)}:{name}"
            if end - start + 1 > MAX_FUNCTION_LINES and ("long", key) not in ok:
                found.append(f"long {key}  | {end - start + 1} lines")
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


BYTE_ORDER_DEF = re.compile(r"^\s*#\s*define\s+(be16|wr16|be32|wr32)\b|^(?:static\s+)?(?:inline\s+)?\w[\w\s*]*?\b"
                            r"(be16|wr16|be32|wr32)\s*\([^;]*\)\s*$", re.M | re.I)


def test_byte_order_helpers_only_in_bytes_header():
    report("byteorder", [f"byteorder {rel(path)}  | {m.group().strip()}" for path in sources()
                         if rel(path) != "data/bytes.h"
                         for m in BYTE_ORDER_DEF.finditer(strip(path.read_text(errors="replace")))])


KAOS_CALL = re.compile(r"\bkaos_\w+\s*\(")
KAOS_HEADER = re.compile(r'^\s*#\s*include\s*"(?:data/)?kaos_addresses\.h"', re.M)
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


PLATFORM_CODE = re.compile(r"^\s*#\s*if.*\b_WIN32\b|^\s*#\s*include\s*<(?:unistd|dirent|strings|execinfo|fcntl|poll"
                           r"|windows|direct|io)\.h>|^\s*#\s*include\s*<(?:sys|arpa|netinet)/", re.M)
STDERR_OR_EXIT = re.compile(r"\bfprintf\s*\(\s*stderr\b|\bexit\s*\(")


def test_platform_code_only_in_platform():
    """Operating system headers only in platform/; stderr and exit only in platform/ and data/os.c."""
    ok = allowed()
    found = []
    for path in sources():
        if path.parent.name == "platform" or ("platform", rel(path)) in ok:
            continue
        text = path.read_text(errors="replace")
        found += [f"platform {rel(path)}  | {m.group().strip()}" for m in PLATFORM_CODE.finditer(text)]
        if rel(path) != "data/os.c":
            found += [f"platform {rel(path)}  | {n}: {line.strip()}" for n, line in enumerate(strip(text).splitlines(), 1)
                      if STDERR_OR_EXIT.search(line)]
    report("platform", found)


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


INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def test_layers():
    """The include path names the layer; a file includes only its own layer and the layers before it."""
    ok = allowed()
    found = []
    for path in sources():
        parts = path.relative_to(SRC).parts
        if len(parts) != 2 or parts[0] not in LAYERS:
            found.append(f"layer {rel(path)}  | not in a layer folder")
            continue
        layer = parts[0]
        text = path.read_text(errors="replace")
        for header in INCLUDE.findall(text):
            key = f"{rel(path)}:{header}"
            target = header.split("/")[0] if "/" in header else None
            if ("layer", key) in ok:
                continue
            if target not in LAYERS:
                found.append(f"layer {key}  | the include does not name its layer")
            elif LAYERS.index(target) > LAYERS.index(layer):
                found.append(f"layer {key}  | {layer} includes {target}")
        if layer != "platform" and re.search(r"#\s*include\s+<SDL", text) and ("layer", f"{rel(path)}:SDL") not in ok:
            found.append(f"layer {rel(path)}:SDL  | SDL outside platform/")
    report("layer", found)


CLANG_FORMAT_MAJOR = 23


def clang_format():
    """The clang-format of the virtual environment (dev group), else the one on the PATH."""
    exe = Path(sys.executable).with_name("clang-format")
    return str(exe) if exe.exists() else shutil.which("clang-format")


def test_sources_are_formatted():
    exe = clang_format()
    assert exe, "clang-format is missing: uv sync"
    version = subprocess.run([exe, "--version"], capture_output=True, text=True).stdout
    assert re.search(rf"version {CLANG_FORMAT_MAJOR}\.", version), f"clang-format {CLANG_FORMAT_MAJOR} needed: {version.strip()}"
    result = subprocess.run([exe, "--dry-run", "--Werror", *map(str, sources())], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr[-4000:]


def test_allowlist_names_documented_reasons():
    """Each allowlist line is an open line of the final refactor or an exception rule of the standard."""
    found = []
    for number, line in enumerate(ALLOWLIST.read_text().splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        check, target, reason = line.split(maxsplit=2)
        rule = re.match(r"rule (\d+\.\d+): \S", reason)
        if not OPEN_REASON.fullmatch(reason) and not (rule and rule.group(1) in EXCEPTION_RULES):
            found.append(f"allowlist {check}:{target}  | {number}: {reason}")
    report("allowlist", found)
