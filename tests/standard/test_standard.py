# SPDX-License-Identifier: GPL-3.0-or-later
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
DECIMAL = re.compile(r"(?<![\w.])[1-9][0-9]+(?![\w.])|\[\s*[2-9]\s*\]|<=?\s*[2-9]\s*;")
STRTOL_BASE = re.compile(r"\bstrto\w*\s*\([^;]*?,\s*10\s*\)")
DECOMPILER = re.compile(r"\b(?:[uisbcp]Var\d+|local_[0-9a-f]+|arg_[0-9a-f]+|param_\d+|in_[AD][0-7]\w*|CONCAT\d+|DAT_[0-9a-f]+|FUN_[0-9a-f]+)\b")
RECORD = re.compile(r"\bREC\s*\(|\bg_dungeon_map_data\s*\[|\bg_current_map\s*\[|"
                    r"\brec_(?:read|write)\([^()]*\)\s*\[")
CONST_DEF = re.compile(r"\s*(?:static\s+)?const\s+\w+\s+[A-Z][A-Z0-9_]*\s*=")
ENUM = re.compile(r"\benum\b[^{};]*\{([^}]*)\}")
DEFINE = re.compile(r"^\s*#define\s+([A-Z][A-Z0-9_]*)\b", re.M)
DECL = re.compile(r"^((?:(?:static|inline|const|struct|unsigned|_Noreturn)\s+)*[A-Za-z_]\w*[\s*]+)([a-z_]\w*)\s*\(", re.M)
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
    line_check("decimal", DECIMAL, ignore=STRTOL_BASE)


ENUM_ANY = re.compile(r"\b(?:typedef\s+)?enum\b\s*(\w+)?\s*\{[^}]*\}\s*(\w+)?\s*;", re.S)
RETURN_TYPE = re.compile(r"^(?:static\s+)?(?:enum\s+)?([A-Z]\w*)\s+\**\s*[a-z_]\w*\s*\(", re.M)


def test_enum_names_a_result_type():
    returned = {name for path in sources() for name in RETURN_TYPE.findall(strip(path.read_text(errors="replace")))}
    ok = allowed()
    found = []
    for path in sources():
        if ("enum", rel(path)) in ok:
            continue
        text = strip(path.read_text(errors="replace"))
        for m in ENUM_ANY.finditer(text):
            name = m.group(1) or m.group(2)
            if not name or name not in returned:
                found.append(f"enum {rel(path)}  | {text.count(chr(10), 0, m.start()) + 1}: enum {name or '{...}'}")
    report("enum", found)


DEFINE_LINE = re.compile(r"^\s*#\s*define\s+([A-Z][A-Z0-9_]*)(?!\()\s+(.*?)\s*(?:/\*.*\*/)?\s*$")


def constant_blocks(path):
    """(first line, comment line above, [(name, value)]) of each run of adjacent #define lines with a value."""
    lines = path.read_text(errors="replace").splitlines()
    i = 0
    while i < len(lines):
        m = DEFINE_LINE.match(lines[i])
        if not m or not m.group(2):
            i += 1
            continue
        start, items = i, []
        while i < len(lines) and (m := DEFINE_LINE.match(lines[i])) and m.group(2):
            items.append((m.group(1), m.group(2)))
            i += 1
        yield start + 1, start > 0 and lines[start - 1].rstrip().endswith("*/"), items


def test_constant_blocks_have_a_comment():
    ok = allowed()
    report("block", [f"block {rel(path)}  | {line}: {items[0][0]}" for path in sources() if ("block", rel(path)) not in ok
                     for line, commented, items in constant_blocks(path) if not commented])


def test_no_decompiler_names():
    line_check("decompiler", DECOMPILER)


def test_raw_record_access_only_in_accessors():
    line_check("record", RECORD, owners={"record.c", "record.h"})


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


def module_stem(name):
    """The path of a module's public header without .h; <m>_internal.h gives <m>."""
    return name.removesuffix(".h").removesuffix("_internal")


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
                if module_stem(other_path) == module_stem(rel(path)):
                    continue  # rule 1.7: an internal header shares the prefixes of its public header
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


TRUTH_LOCAL = re.compile(r"^\s+(?:const\s+)?(?:int|int8_t|int16_t|int32_t|uint8_t|uint16_t|uint32_t|unsigned|short|char)"
                         r"\s+(\w+)\s*=\s*(?:true|false|[^;?]*(?:==|!=|<=|>=|&&|\|\|)[^;?]*|![\w(][^;?]*);")
RETURNS_TRUTH = re.compile(r"\b[Rr]eturns? (?:1|0|true|false)\b(?!\s+(?:on|for)\b)")


def test_truth_values_are_bool():
    """Predicates, locals set from a comparison or true/false, and functions that return 1 or 0 are bool."""
    ok = allowed()
    found = []
    for path in sources():
        lines = path.read_text(errors="replace").splitlines()
        for f in functions(path):
            key = f"{rel(path)}:{f.name}"
            if f.ret.replace("static", "").replace("inline", "").strip() == "bool" or ("bool", key) in ok:
                continue
            if PREDICATE.search(f.name):
                found.append(f"bool {key}  | predicate returns {f.ret}")
            elif RETURNS_TRUTH.search(comment_above(lines, f.line)):
                found.append(f"bool {key}  | the comment says it returns 1 or 0")
        if ("bool", rel(path)) not in ok:
            found += [f"bool {rel(path)}  | {n}: {line.strip()}" for n, line in code_lines(path) if TRUTH_LOCAL.match(line)]
    report("bool", sorted(set(found)))


def all_prefixes():
    """Every prefix that a header states, longest first."""
    found = {p for h in headers() if (m := PREFIXES.search(h.read_text(errors="replace"))) for p in m.group(1).split()}
    return sorted(found, key=len, reverse=True)


MAX_NAME_CHARS, MAX_NAME_WORDS = 32, 4


def test_function_names_are_short():
    """<prefix><verb>_<object>: at most four words after the prefix and at most 32 characters."""
    prefixes, ok, found, seen = all_prefixes(), allowed(), [], set()
    for path in sources():
        for f in functions(path):
            key = f"{rel(path)}:{f.name}"
            if (rel(path), f.name) in seen or ("name", key) in ok:
                continue
            seen.add((rel(path), f.name))
            prefix = "" if f.static and path.suffix == ".c" else next((p for p in prefixes if f.name.startswith(p)), "")
            words = f.name[len(prefix):].split("_")
            if len(f.name) > MAX_NAME_CHARS or len(words) > MAX_NAME_WORDS:
                found.append(f"name {key}  | {len(f.name)} characters, {len(words)} words after {prefix or 'no prefix'}")
    report("name", found)


def test_static_functions_have_no_module_prefix():
    prefixes, ok = all_prefixes(), allowed()
    report("static-prefix", sorted({f"static-prefix {rel(path)}:{f.name}  | {p}" for path in sources() for f in functions(path)
                                    if f.static and path.suffix == ".c" and (p := next((p for p in prefixes if f.name.startswith(p)), None))
                                    and ("static-prefix", f"{rel(path)}:{f.name}") not in ok}))


EXTERN_VARIABLE = re.compile(r"^[ \t]*extern\b([^;]*);", re.M)
FILE_STATIC_G = re.compile(r"^static\b[^(;=]*?\b(g_\w+)\b", re.M)


def extern_names(declaration):
    """The variable names of one extern declaration, also a function pointer variable."""
    if (pointer := re.search(r"\(\s*\*\s*(\w+)\s*\)", declaration)):
        return [pointer.group(1)]
    if "(" in declaration:
        return []
    return re.findall(r"(\w+)\s*(?:\[[^\]]*\])*\s*(?:,|$)", declaration.strip())


def test_globals_have_the_g_prefix():
    ok = allowed()
    found = []
    for path in sources():
        if ("global", rel(path)) in ok:
            continue
        text = strip(path.read_text(errors="replace"))
        found += [f"global {rel(path)}  | extern {name}" for decl in EXTERN_VARIABLE.findall(text)
                  for name in extern_names(decl) if not name.startswith("g_")]
        if path.suffix == ".c":
            found += [f"global {rel(path)}  | static {name}" for name in FILE_STATIC_G.findall(text)]
    report("global", found)


ORDINAL = re.compile(r"(?:^|_)ordinal(?:_|$)|_plus_1(?:_|$)", re.I)


def test_no_ordinals_in_names():
    ok = allowed()
    where = defaultdict(set)
    for path in sources():
        for name in IDENTIFIER.findall(strip(path.read_text(errors="replace"))):
            if ORDINAL.search(name) and ("ordinal", name) not in ok:
                where[name].add(rel(path))
    report("ordinal", [f"ordinal {name}  | {sorted(paths)}" for name, paths in sorted(where.items())])


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
LITERAL = re.compile(r"^\(?-?(0x[0-9a-fA-F]+|\d+)[uUlL]*\)?$")
HEX_LAST_PARTS = {"MASK", "MASKS", "FLAG", "FLAGS", "BIT", "BITS"}
ADDRESS_FIRST_PARTS = {"KAOS", "CSB", "SWSH"}
DECIMAL_PARTS = {"COUNT", "SIZE", "BYTES", "TICKS", "MIN", "MAX", "LIMIT", "THRESHOLD", "WIDTH", "HEIGHT", "SHIFT"}


def block_notation_error(items):
    """Why a constant block breaks the notation rule (hex or decimal in every line), or None."""
    literals = [m.group(1) for _, value in items if (m := LITERAL.match(value.strip()))]
    hexes = [v for v in literals if v.lower().startswith("0x")]
    decimals = [v for v in literals if not v.lower().startswith("0x")]
    names = [name.split("_") for name, _ in items]
    hex_kind = any(parts[-1] in HEX_LAST_PARTS or parts[0] in ADDRESS_FIRST_PARTS for parts in names)
    decimal_kind = any(DECIMAL_PARTS & set(parts) for parts in names if parts[0] not in ADDRESS_FIRST_PARTS)
    if hexes and decimals:
        return "mixes hex and decimal"
    if hex_kind and decimal_kind:
        return "mixes flags or addresses with counts or sizes"
    if hex_kind and decimals:
        return "a flag, mask or address block in decimal"
    if decimal_kind and hexes:
        return "a count, size or limit block in hex"
    if len({len(v) for v in hexes}) > 1:
        return "hex values with different digit widths"
    return None


def test_number_notation():
    ok = allowed()
    report("notation", [f"notation {rel(path)}  | {line}: {items[0][0]}: {error}" for path in sources()
                        if ("notation", rel(path)) not in ok
                        for line, _, items in constant_blocks(path) if (error := block_notation_error(items))])


NAMED_SHIFT = re.compile(r"(?:<<|>>)\s*([A-Z][A-Z0-9_]*_SHIFT)\b")
PACKED = re.compile(r"(?:<<|>>)\s*(?:8|16|24)\b|\((?:u?int(?:8|16|32)_t|unsigned|int)\)\s*\((?:u?int(?:8|16|32)_t|unsigned|int)\)")


def test_no_packed_fields_at_call_sites():
    """Also a shift by a *_SHIFT constant of a header in a .c file of another module: the accessor belongs to the owner."""
    ok = allowed()
    owner = {name: module_stem(path.name) for path in sources() if path.suffix == ".h"
             for name in DEFINE.findall(strip(path.read_text(errors="replace"))) if name.endswith("_SHIFT")}
    found = []
    for path in sources():
        if path.suffix == ".c" and ("packed", rel(path)) not in ok:
            module = module_stem(owner_header(path).name) if owner_header(path) else path.stem
            found += [f"packed {rel(path)}  | {n}: {line.strip()}" for n, line in code_lines(path)
                      if PACKED.search(line) or any(owner.get(name, module) != module for name in NAMED_SHIFT.findall(line))]
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


def module_files(header):
    """The header, its _internal.h and the .c files of its module (same stem or <stem>_*)."""
    base = header.stem.removesuffix("_internal")
    return {p for p in header.parent.iterdir() if p.suffix in (".c", ".h") and (p.stem == base or p.stem.startswith(base + "_"))}


def base_header(path):
    """The public header <m>.h of a file named <m>_<x>, or None."""
    return next((h for h in sorted(path.parent.glob("*.h")) if h != path and not h.stem.endswith("_internal")
                 and path.stem.startswith(h.stem + "_")), None)


def test_one_public_header_per_module():
    ok = allowed()
    found = []
    for path in sources():
        if ("module", rel(path)) in ok:
            continue
        if path.suffix == ".c" and not path.with_suffix(".h").exists() and not base_header(path):
            found.append(f"module {rel(path)}  | no header")
        elif path.suffix == ".h" and path.stem.endswith("_internal"):
            if not path.with_name(path.stem.removesuffix("_internal") + ".h").exists():
                found.append(f"module {rel(path)}  | an internal header without its public header")
        elif path.suffix == ".h" and (base := base_header(path)) and not path.with_suffix(".c").exists() \
                and any(not f.static for f in functions(path)):
            found.append(f"module {rel(path)}  | a second public header of {base.name}")
    report("module", found)


SPDX_C = "/* SPDX-License-Identifier: GPL-3.0-or-later */"


def header_error(path):
    """Why the head of a header is out of order (SPDX, guard, prefix line, purpose paragraph), or None."""
    lines = path.read_text(errors="replace").splitlines()
    guard = f"DM_{path.parent.name.upper()}_{path.stem.upper()}_H"
    if lines[:1] != [SPDX_C]:
        return "line 1 is not the SPDX line"
    if lines[1:3] != [f"#ifndef {guard}", f"#define {guard}"]:
        return f"the guard is not {guard}"
    if len(lines) < 4 or not PREFIXES.match(lines[3]):
        return "line 4 is not the prefix line"
    first = next((line for line in lines[4:] if line.strip()), "")
    if not first.startswith("/*"):
        return "no purpose paragraph after the prefix line"
    return None


def test_header_heads():
    ok = allowed()
    report("header", [f"header {rel(path)}  | {error}" for path in headers()
                      if ("header", rel(path)) not in ok and (error := header_error(path))])


def defined_names(header, cache={}):
    """The names that a header makes known: macros, types, tags, functions, variables, enum members."""
    if header not in cache:
        text = strip(header.read_text(errors="replace"))
        names = set(re.findall(r"^\s*#\s*define\s+(\w+)", text, re.M))
        names |= set(re.findall(r"\}\s*(\w+)\s*;", text)) | set(re.findall(r"\btypedef\b[^;{]*?(\w+)\s*;", text))
        names |= set(re.findall(r"\b(?:struct|enum|union)\s+(\w+)", text))
        names |= {f.name for f in functions(header)}
        names |= {n for decl in EXTERN_VARIABLE.findall(text) for n in extern_names(decl)}
        for body in ENUM.findall(text):
            names |= set(re.findall(r"(?:^|,)\s*([A-Z][A-Z0-9_]*)", body))
        cache[header] = names
    return cache[header]


def test_includes_are_used():
    """A file uses at least one name of each project header it includes; its own header is always allowed."""
    by_path = {rel(h): h for h in headers()}
    ok = allowed()
    found = []
    for path in sources():
        raw = path.read_text(errors="replace")
        used = set(IDENTIFIER.findall(INCLUDE.sub("", strip(raw))))
        own = f"{path.parent.name}/{path.stem}.h"
        for header in INCLUDE.findall(raw):
            key = f"{rel(path)}:{header}"
            if header == own or header not in by_path or ("include", key) in ok:
                continue
            if not defined_names(by_path[header]) & used:
                found.append(f"include {key}  | no name of {header} is used")
    report("include", found)


TRIVIAL_BODY_LINES = 3


def test_function_comments():
    """A public function has its comment above the prototype, a non-trivial static function above its definition; never both."""
    ok = allowed()
    found = []
    for header in headers():
        hlines = header.read_text(errors="replace").splitlines()
        bodies = {}
        for c in sorted(module_files(header)):
            if c.suffix == ".c":
                clines = c.read_text(errors="replace").splitlines()
                bodies.update({name: (c, clines, start) for name, start, _, static in definitions(c) if not static})
        for f in functions(header):
            if f.static or ("comment", rel(header)) in ok:
                continue
            in_header = comment_above(hlines, f.line)
            body = bodies.get(f.name)
            in_source = body and comment_above(body[1], body[2])
            if in_header and in_source:
                found.append(f"comment {rel(header)}  | {f.name}: a comment above the prototype and the definition in {rel(body[0])}")
            elif not in_header and not f.name.startswith("rec_"):
                found.append(f"comment {rel(header)}  | {f.name}: no comment above the prototype")
    for path in sources():
        if path.suffix != ".c" or ("comment", rel(path)) in ok:
            continue
        lines = path.read_text(errors="replace").splitlines()
        found += [f"comment {rel(path)}  | {name}: no comment above the static definition"
                  for name, start, end, static in definitions(path)
                  if static and end - start - 1 > TRIVIAL_BODY_LINES and not comment_above(lines, start)]
    report("comment", found)


FORWARD_STATIC = re.compile(r"^static\b[^=;{]*?\b(\w+)\s*\([^;{]*\)\s*;", re.M)


def owner_header(path):
    own = path.with_suffix(".h")
    return own if own.exists() else base_header(path)


def test_file_order():
    """Public definitions follow the header order; a static forward declaration names its recursion."""
    ok = allowed()
    found = []
    for path in sources():
        if path.suffix != ".c" or ("order", rel(path)) in ok:
            continue
        header = owner_header(path)
        if header:
            order = [f.name for f in functions(header) if not f.static]
            defined = [name for name, _, _, static in definitions(path) if not static and name in order]
            expected = sorted(defined, key=order.index)
            if defined != expected:
                first = next(a for a, b in zip(defined, expected) if a != b)
                found.append(f"order {rel(path)}  | {first} stands before its place in {header.name}")
        lines = path.read_text(errors="replace").splitlines()
        text = strip("\n".join(lines))
        for m in FORWARD_STATIC.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            if "recursion" not in comment_above(lines, line).lower():
                found.append(f"order {rel(path)}  | {line}: forward declaration of {m.group(1)} without a recursion comment")
    report("order", found)


TYPE_TABLE = {"champion_index": "int16_t", "cell": "int16_t", "direction": "int16_t", "map": "int16_t",
              "slot": "int16_t", "x": "int16_t", "y": "int16_t", "thing": "uint16_t", "ticks": "uint16_t"}


def parameter(text):
    """(type, name) of one parameter; const does not change the type."""
    if (pointer := re.search(r"\(\s*\*\s*(\w+)\s*\)", text)):
        return "function pointer", pointer.group(1)
    m = re.match(r"(.*?)(\w+)\s*((?:\[[^\]]*\])*)$", text)
    if not m:
        return text, ""
    kind = " ".join(m.group(1).replace("const", " ").split()) + ("[]" if m.group(3) else "")
    return kind.replace(" *", "*"), m.group(2)


def test_one_type_per_parameter_name():
    """A parameter name has one type in the headers of a layer, and the type of the table for a game value kind."""
    ok = allowed()
    found = []
    for layer in LAYERS:
        types = defaultdict(set)
        for header in (SRC / layer).glob("*.h"):
            for f in functions(header):
                for text in f.params:
                    kind, name = parameter(text)
                    if name:
                        types[name].add(kind)
        for name, kinds in sorted(types.items()):
            key, expected = f"{layer}:{name}", TYPE_TABLE.get(name)
            if ("type", key) in ok:
                continue
            if expected and kinds != {expected}:
                found.append(f"type {key}  | {sorted(kinds)}; the table says {expected}")
            elif not expected and len(kinds) > 1:
                found.append(f"type {key}  | {sorted(kinds)}")
    report("type", found)


RESULT_COMMENT = re.compile(r"\b0 on success\b|-1 (?:on|for) (?:an? )?(?:error|failure)|\b[Rr]eturns? -1\b|\b1/0/-1\b|\b[Rr]eturns? 0 or -1\b")
MAX_PARAMETERS = 5


def test_results():
    ok = allowed()
    found = []
    for path in sources():
        lines = path.read_text(errors="replace").splitlines()
        found += [f"result {rel(path)}:{f.name}  | the comment gives 0 or -1 as a result" for f in functions(path)
                  if ("result", f"{rel(path)}:{f.name}") not in ok and RESULT_COMMENT.search(comment_above(lines, f.line))]
    report("result", sorted(set(found)))


def test_at_most_five_parameters():
    ok = allowed()
    report("params", sorted({f"params {rel(path)}:{f.name}  | {len(f.params)} parameters" for path in sources()
                             for f in functions(path) if len(f.params) > MAX_PARAMETERS
                             and ("params", f"{rel(path)}:{f.name}") not in ok}))


TEST_MARKER = "exported for the tests"


def test_exports_have_users():
    """An exported function or global has a use outside its module, or the test marker on its declaration."""
    used_in = {path: set(IDENTIFIER.findall(strip(path.read_text(errors="replace")))) for path in sources()}
    ok = allowed()
    found = []
    for header in headers():
        if header.stem.endswith("_internal"):
            continue
        own = module_files(header)
        lines = header.read_text(errors="replace").splitlines()
        text = strip("\n".join(lines))
        names = [(f.name, f.line) for f in functions(header) if not f.static]
        names += [(n, text.count("\n", 0, m.start()) + 1) for m in EXTERN_VARIABLE.finditer(text) for n in extern_names(m.group(1))]
        for name, line in names:
            key = f"{rel(header)}:{name}"
            if ("dead-api", key) in ok or any(name in ids for p, ids in used_in.items() if p not in own):
                continue
            if TEST_MARKER not in lines[line - 1] + comment_above(lines, line):
                found.append(f"dead-api {key}  | no use outside {header.stem} and no test marker")
    report("dead-api", found)


COMMENT_TEXT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
EVENT_NUMBER = re.compile(r"(?i)\bevents?\s+#?\d+")
TODO_MARKER = re.compile(r"\b(?:TODO|FIXME|XXX)\b|ponytail:")


def comment_check(check, pattern):
    ok = allowed()
    found = []
    for path in sources():
        if (check, rel(path)) in ok:
            continue
        text = path.read_text(errors="replace")
        for m in COMMENT_TEXT.finditer(text):
            if (hit := pattern.search(m.group())):
                found.append(f"{check} {rel(path)}  | {text.count(chr(10), 0, m.start()) + 1}: {hit.group()}")
    report(check, found)


def test_no_event_numbers_in_comments():
    comment_check("event-comment", EVENT_NUMBER)


def test_no_todo_markers():
    comment_check("todo", TODO_MARKER)


TEST_IMPORT = re.compile(r"^\s*(?:from\s+test_\w+\s+import|import\s+test_\w+)", re.M)
MACHINE_PATH = re.compile("/" + "Users/|/" + "private/tmp/")
CLANG_CALL = re.compile(r"""["']clang["']\s*,""")


def test_tests_follow_the_rules():
    """No test module imports another test module, names a machine path or compiles the library."""
    ok = allowed()
    found = []
    for path in sorted(TESTS.rglob("*.py")):
        target = path.relative_to(ROOT).as_posix()
        if path.resolve() == Path(__file__).resolve() or ("tests", target) in ok:
            continue
        for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            if TEST_IMPORT.match(line) or MACHINE_PATH.search(line) or CLANG_CALL.search(line):
                found.append(f"tests {target}  | {number}: {line.strip()}")
    report("tests", found)
