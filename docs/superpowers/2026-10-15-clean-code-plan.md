# Clean C code: plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `port/src/` reads like C that a person wrote, builds on macOS and Windows with 0 warnings, and keeps the game behavior.

**Architecture:** Golden snapshots (save, dungeon view, ui rules) pin the behavior first. Then one `clang-format` commit sets the style, and `tests/test_standard.py` gets the new rules with an allowlist of open violations. Each later task removes its allowlist lines: one constant owner, KAOS tables behind `data/tables.h`, portability, packed-field accessors, types, names, rules out of `ui/`, and the square table of `ui/view.c`.

**Tech stack:** C11, clang 21 (Apple), MinGW-w64 gcc, SDL3, Make, clang-format 23. Python 3.14, uv, pytest, pytest-xdist, ctypes.

**Spec:** `docs/superpowers/2026-10-15-clean-code.md` (the PRD). Inventories with file:line of the tree `main @ b3d1852`: `docs/superpowers/2026-10-15-clean-code-inventory/kaos.md` (image addresses, R2), `names.md` (duplicates and renames, R3, R7), `ui.md` (ui writes to engine globals and the square drawers, R8, R9).

**Line numbers:** every `file:line` in this plan and in the inventories refers to `b3d1852`. Task 3 reformats all files, so after it the lines move. Find a site by its name or text; `git show b3d1852:port/src/<file>` shows the old lines.

## Global Constraints

- Branch `feature/2026-10-15-clean-code`, main checkout. Commits: Conventional Commits, one type per commit, the git identity of the repo (`git config user.email`), no co-author, no push.
- The PRD, this plan and the inventories are committed on the branch before Task 1 (`git add docs/superpowers/2026-10-15-clean-code*` and `docs/superpowers/2026-10-15-clean-code-inventory`, `docs: clean-code PRD, plan and inventories`).
- Commits: a task that creates a file stages it by name (`git add <new files>`) before `git commit -am`.
- Baseline: `uv run pytest -q` gave `606 passed` on 2026-10-15 at `b3d1852` (measured in the planning session). New tests add to this count. No baseline run at the start of the run.
- Build: `make -C port` with 0 warnings after each task. From Task 14 on also `make -C port windows` with 0 warnings: `make -C port windows 2>&1 | grep -c 'warning:'` prints `0`.
- No change of game behavior. The golden tests of Tasks 1, 2 and 31 and the existing suite prove it. If a task needs a behavior change to pass, stop and report.
- Save files and the binary layout of each struct in a save or in a `ctypes.Structure` of `tests/` stay. Field names may change only in Task 27.
- The LLM protocol names in `platform/state_json.c` and `platform/control.c` (JSON keys, command names) stay.
- Each commit builds: a task deletes a define, a function or a header entry only when it migrates its last user; a shared address define that a later task still needs stays where it is until that task (Tasks 8-11 name these).
- Coding standard: `docs/coding-standard.md` for every change in `port/src/`. Each task that adds a rule adds it to the standard in the same commit.
- Comments: keep existing comments; new comments one line, what the code does, never its history. A comment that only names an original symbol: rewrite it in game terms (Task 30), do not delete it.
- A rename updates `docs/` in the same commit: `grep -rn <old name> docs/ tests/ tools/`.
- Formatting: after Task 3 every changed C file is formatted: `clang-format -i <files>` before each commit.
- Tests: one file `uv run pytest -q tests/test_x.py`; the author runs the build and only the tests of the changed code, never the full suite. Full suite (controller): `uv run pytest -q`.
- Write a test only for: a check against a reference (original program, golden snapshot), a parser or data format, a rule with more than one outcome that no reference check or type check covers, a regression of a real bug (one test per bug). No tests for: constants and data tables; wiring, scaffold, CLI arguments, logging; what the compiler, a linter or a standard check catches; UI and layout; what another test already covers. No mutation tests.
- Tests pass in any order and in parallel; each test file owns its state (unique temp paths).
- Test failures: superpowers:systematic-debugging; root cause before a fix.
- Do not download or run third-party code beyond the project lock file, Homebrew `clang-format` and the SDL3 MinGW package that `port/Makefile` already fetches.

### Weakened assertions

- Task 25: `tests/test_spells.py:71-80` asserts `spell_kind`, `spell_type`, `spell_disabled_ticks` (and the other `SpellRecord` functions) of `data/spells.c`, which have no C caller. The task removes these functions and rewrites the assertions to read the same values from the fields of `tables_spell(i)` (`kind`, `type`, `disabled_ticks`, `base_required_skill_level`). Same expected values, other access path.
- Otherwise none. Renamed fields and functions change names in tests (Tasks 27-29, 34), not assertions.

Approval: approved by the user on 2026-10-15.

## Task size exceptions

Some tasks change more than one module, for a reason:
- Tasks 7, 19, 20, 26: mechanical changes that the standard check or one grep lists site by site; a split by file adds commits without a separate review value.
- Tasks 8-11: each adds accessors in `data/tables` and migrates their users in one commit, so no accessor exists without a user.
- Tasks 28, 29: a rename changes its definition, its callers and the tests in one commit, or the build breaks.
- Tasks 32-35: a rule moves from `ui/` to `engine/` in one commit, or the build breaks.

## Review Focus

1. A golden test that passes because it covers nothing: an empty map loop, a CRC over a buffer the code never writes, a snapshot of values that do not depend on the moved code. Expected: the task shows the golden test fails when the code under it changes one value (Task 1, 2, 31 each have a step for this).
2. A changed signedness or width on a value (`int16_t` ↔ `uint16_t`, `uint8_t` cast removed): a comparison, a division, a right shift or a wrap gives another result. Expected: the same value as before; casts that changed a result stay with a comment.
3. A table accessor that reads the cache before `tables_init` (NULL) or after an adventure switch (stale DM values in CSB). Expected: new accessors read the image at call time; fixtures that use a cached accessor call `tables_init`.
4. A rule moved from `ui/` to `engine/` that changes the order of a rule step and a drawing step in the same tick (for example the action-area refresh between the time step and the time effects). Expected: the same order; the golden rule snapshots of Task 31 stay equal.
5. A renamed C symbol that a `ctypes` test binds by name, or that `state_json.c` prints. Expected: the tests follow the rename; the protocol output stays.

## File map

| File | Change |
|---|---|
| `tests/test_golden_save.py`, `tests/golden/save.json` | new, Task 1 |
| `tests/test_golden_view.py`, `tests/golden/view.json` | new, Task 2 |
| `.clang-format`, `.git-blame-ignore-revs` | new, Task 3 |
| `tests/test_standard.py`, `tests/standard_allowlist.txt`, `docs/coding-standard.md` | Tasks 3-5, then each task removes allowlist lines |
| `port/src/data/tables.c`, `port/src/data/tables.h` | Tasks 8-12: all KAOS addresses and accessors |
| `port/src/data/swsh.c`, `port/src/data/swsh.h` | new, Task 12 |
| `port/src/data/file.c`, `port/src/data/file.h` | new, Task 24 |
| `port/src/data/kaos_addresses.h` | new, Task 8: the `KAOS_*` address defines, included only by `data/` files |
| `port/src/data/os.h`, `port/src/data/os.c` | new, Task 13: folder hooks and name compare |
| `tests/test_golden_rules.py`, `tests/golden/rules.json` | new, Task 31 |
| `tests/test_adf.py` | Task 16: unsafe names |

---

### Task 1: Golden save snapshot

Pins R2-R10: the saved state of a fixed game must not change.

**Files:**
- Create: `tests/test_golden_save.py`, `tests/golden/save.json`

**Interfaces:**
- Consumes: `tests/test_save.py:40-56` fixture pattern, `recruit_and_hold_object` (`tests/test_save.py:92`), `decode` (`tests/test_savefile.py:82`), `Game` (`tests/test_gameloop.py`).
- Produces: `tests/golden/save.json` (SHA-256 per save part); env `DM_UPDATE_GOLDEN=1` rewrites a golden file (used again by Tasks 2 and 31).

- [ ] **Step 1: Write the test.**

```python
"""Golden snapshot of a save file: the saved state of a fixed game stays the same through refactors."""

import ctypes
import hashlib
import json
import os
import subprocess

import pytest

from test_gameloop import Game
from test_port import ROOT
from test_save import GAME, SAVE, recruit_and_hold_object
from test_savefile import decode

c16, u16 = ctypes.c_int16, ctypes.c_uint16

GOLDEN = ROOT / "tests" / "golden" / "save.json"
SEED = 0x1234ABCD

needs_game = pytest.mark.skipif(not (GAME / "KAOS.FTL").exists(), reason="game files missing")


def digests(parts):
    return {"header": hashlib.sha256(parts["header"]).hexdigest(),
            "blocks": [hashlib.sha256(b).hexdigest() for b in parts["blocks"]],
            "portraits": hashlib.sha256(parts["portraits"]).hexdigest(),
            "dungeon": hashlib.sha256(parts["dungeon"]).hexdigest()}


def saved_game(tmp_path):
    """A new game with a fixed seed: recruit, hold an object, save; returns the decoded parts."""
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    # the types that Game and recruit_and_hold_object need, as the fixture of tests/test_save.py:43-52
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = u16
    lib.get_next_thing.restype = u16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.savegame_save.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_new_game(str(GAME).encode())
    ctypes.c_uint32.in_dll(lib, "g_random_seed").value = SEED
    game = Game(lib)
    recruit_and_hold_object(game)
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    return decode(lib, (tmp_path / SAVE).read_bytes())


@needs_game
def test_save_matches_the_golden_snapshot(tmp_path):
    got = digests(saved_game(tmp_path))
    if os.environ.get("DM_UPDATE_GOLDEN"):
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(got, indent=1) + "\n")
    assert got == json.loads(GOLDEN.read_text())
```

- [ ] **Step 2: Check the types.** Compare the `restype`/`argtypes` lines in `saved_game` with `tests/test_save.py:43-52` and `tests/test_gameloop.py:36-55`; copy every line that `Game`, `recruit_and_hold_object` and `savegame_save` need. Tasks that rename a function of this list later update both files.
- [ ] **Step 3: Write the golden file.** `DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_save.py`. Expected: PASS, `tests/golden/save.json` exists.
- [ ] **Step 4: Check determinism.** Run `uv run pytest -q tests/test_golden_save.py` 3 times. Expected: PASS each time. If it fails, find the source of the difference (wall-clock time, an unseeded random) and fix it in the test setup, not in the port.
- [ ] **Step 5: Check that the test detects a change.** Change `SEED` to `0x1234ABCE`, run, expect FAIL; restore `SEED`.
- [ ] **Step 6: Commit.** `git add tests/test_golden_save.py tests/golden/save.json && git commit -m "test: golden snapshot of a saved game"`

---

### Task 2: Golden view snapshot

Pins R2, R6, R7 and R9 for drawing: the dungeon view of every square and direction stays the same. Source: `docs/superpowers/2026-10-15-clean-code-inventory/ui.md` section "Tests covering view drawing" (no test compares view pixels now).

**Files:**
- Create: `tests/test_golden_view.py`, `tests/golden/view.json`

**Interfaces:**
- Consumes: the `lib` and `game` fixtures of `tests/test_view.py:29-41, 155-163`; `view_render(map, x, y, dir)` (`port/src/ui/view.h:55`); `dungeon_map_count`, `dungeon_map_width`, `dungeon_map_height` (`port/src/engine/dungeon.h:30-32`).
- Produces: `tests/golden/view.json`: per map, a list of CRC-32 values, one per row y (all x, all 4 directions of that row in one CRC).

- [ ] **Step 1: Write the test.**

```python
"""Golden snapshot of the dungeon view: every square of every map in all 4 directions draws the same pixels."""

import ctypes
import json
import os
import zlib

import pytest

from test_port import ROOT
from test_view import game, lib, needs_game  # noqa: F401 (fixtures)

GOLDEN = ROOT / "tests" / "golden" / "view.json"
SEED = 0x1234ABCD
MAP_COUNT = 14  # Dungeon.DAT of Dungeon Master


def map_rows(lib, m):
    """CRC-32 of the frame buffer of all views of each row of map m."""
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    rows = []
    for y in range(lib.dungeon_map_height(m)):
        crc = 0
        for x in range(lib.dungeon_map_width(m)):
            for d in range(4):
                ctypes.c_uint32.in_dll(lib, "g_random_seed").value = SEED
                ctypes.memset(ctypes.addressof(fb), 0, 320 * 200)
                lib.view_render(m, x, y, d)
                crc = zlib.crc32(bytes(fb), crc)
        rows.append(crc)
    return rows


@needs_game
@pytest.mark.parametrize("m", range(MAP_COUNT))
def test_view_matches_the_golden_snapshot(game, m):
    lib, _ = game
    for name in ("dungeon_map_count", "dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = ctypes.c_int16
    assert lib.dungeon_map_count() == MAP_COUNT
    got = map_rows(lib, m)
    golden = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        golden[str(m)] = got
        GOLDEN.parent.mkdir(exist_ok=True)
        GOLDEN.write_text(json.dumps(golden, indent=None, sort_keys=True) + "\n")
    assert got == golden[str(m)]
```

- [ ] **Step 2: Check the names.** Check that `fb`, `g_random_seed` and `view_render` exist in `libdm.dylib` under these names and that `view_render` returns without the party on that map (`tests/test_view.py:176` renders other maps). Check `MAP_COUNT` with `lib.dungeon_map_count()`.
- [ ] **Step 3: Write the golden file.** Run `DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_view.py -n 0` (one process, so the 14 writes do not race). Expected: PASS.
- [ ] **Step 4: Measure.** `uv run pytest -q tests/test_golden_view.py --durations=3`. If one map takes more than 20 s, render only directions 0 and 2 for squares whose element (`dungeon_square(m, x, y) >> 5`) is a wall, regenerate, and record the choice in the test docstring.
- [ ] **Step 5: Check determinism and detection.** Run the test 3 times: PASS. Then in `port/src/ui/view.c` change the pit zone of `d2c` (`ZONE_PIT_D2C`) to `ZONE_PIT_D2L`, build, run: expect FAIL on the maps with a pit; restore.
- [ ] **Step 6: Remove the stale scratch path.** `tests/test_view.py:14` `SCRATCH` points at a scratchpad of an old session; remove it and both uses: the PNG write in `test_render_view` (`:181-183`) and in `test_render_things` (`:301-302`).
- [ ] **Step 7: Commit.** `git add tests/test_golden_view.py tests/golden/view.json tests/test_view.py && git commit -m "test: golden snapshot of the dungeon view of every square"`

---

### Task 3: clang-format and the format check

PRD R5.

**Files:**
- Create: `.clang-format`, `.git-blame-ignore-revs`
- Modify: `tests/test_standard.py`, `README.md` (Prerequisites, Tests), `docs/coding-standard.md`, all `port/src/**/*.[ch]` except `ui/dr_mp3.h`

**Interfaces:**
- Produces: `test_sources_are_formatted` in `tests/test_standard.py`; `CLANG_FORMAT_MAJOR = 23`.

- [ ] **Step 1: Install.** `brew install clang-format`; `clang-format --version` prints version 23.
- [ ] **Step 2: Write `.clang-format`.**

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
BreakBeforeBraces: Linux
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: false
AlignTrailingComments: true
AlignConsecutiveMacros: AcrossComments
IndentCaseLabels: false
PointerAlignment: Right
SortIncludes: Never
ReflowComments: false
```

- [ ] **Step 3: Try it on 3 files.** `clang-format -i port/src/engine/group.c port/src/ui/view.c port/src/data/csbimage.c`; read `git diff --stat` and 100 lines of each diff. Tune only these options when the result is less readable: `AlignConsecutiveMacros`, `AlignArrayOfStructures: Left` for the tables of `data/csbimage.c`, `data/csbtext.c`, `ui/musicmap.c`, `BinPackArguments`. Use `// clang-format off` / `// clang-format on` only around a data table whose columns the tool breaks, with a one-line reason. `git checkout port/src` before Step 4.
- [ ] **Step 4: Add the check.** In `tests/test_standard.py` add `import shutil, subprocess` and:

```python
CLANG_FORMAT_MAJOR = 23


def test_sources_are_formatted():
    exe = shutil.which("clang-format")
    assert exe, "clang-format is missing: brew install clang-format"
    version = subprocess.run([exe, "--version"], capture_output=True, text=True).stdout
    assert re.search(rf"version {CLANG_FORMAT_MAJOR}\.", version), f"clang-format {CLANG_FORMAT_MAJOR} needed: {version.strip()}"
    result = subprocess.run([exe, "--dry-run", "--Werror", *map(str, sources())], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr[-4000:]
```

- [ ] **Step 5: Run it.** `uv run pytest -q tests/test_standard.py`. Expected: FAIL in `test_sources_are_formatted` (the tree is not formatted yet).
- [ ] **Step 6: Docs.** `README.md` Prerequisites: `brew install clang-format` (version 23, for the tests); Tests: `git config blame.ignoreRevsFile .git-blame-ignore-revs`. `docs/coding-standard.md`: a rule 11 "Format: `.clang-format` in the repo root; `clang-format -i` before each commit." and the check in the check list.
- [ ] **Step 7: Format and commit.** `clang-format -i $(find port/src -name '*.[ch]' ! -name dr_mp3.h)`; `make -C port` (0 warnings); `uv run pytest -q tests/test_standard.py tests/test_golden_view.py tests/test_golden_save.py`. The format splits one-line statements, so `test_functions_are_short` can now report functions over 60 lines: add a `long <file>:<function> open: module cleanup` line for each (Task 26 splits them, Task 36 the drawers of `ui/view.c`; Task 37 checks that none is left). Then PASS. One commit, because the check fails without the formatted tree: `git add -A port/src .clang-format tests/test_standard.py README.md docs/coding-standard.md && git commit -m "style: format port/src with clang-format and check the format"`.
- [ ] **Step 8: Blame ignore file.** `git rev-parse HEAD > .git-blame-ignore-revs`; `git add .git-blame-ignore-revs && git commit -m "build: ignore the format commit in git blame"`.

---

### Task 4: Checks for notation, packed fields, byte order and names

PRD R1.1, R1.2, R1.3, R1.6. The checks start with all current violations in the allowlist, reason `open: module cleanup`.

**Files:**
- Modify: `tests/test_standard.py`, `tests/standard_allowlist.txt`, `docs/coding-standard.md`

**Interfaces:**
- Produces: allowlist keys `notation <NAME>`, `packed <file>`, `byteorder <file>`, `numbered <name>`; removed by Tasks 7, 18, 19, 27.

- [ ] **Step 1: Add the notation check.**

```python
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
```

- [ ] **Step 2: Add the packed-field and byte-order checks** (from `../dm2/tests/test_standard.py`).

```python
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
```

- [ ] **Step 3: Replace the numbered-name check.** Replace `NUMBERED` and `test_no_event_numbers_in_function_names` with a check over all identifiers (functions, fields, constants):

```python
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
```

The byte-offset part asks for at least one digit (`H_BYTE_12A` matches, `CREATURE_AT_ASPECT_INDEX` does not), as PRD R1.6 says.

- [ ] **Step 4: Fill the allowlist.** Run `uv run pytest -q tests/test_standard.py`; for each violation key add one line `<check> <target> open: module cleanup` to `tests/standard_allowlist.txt`, sorted by check. Exceptions with a lasting reason (not `open:`): `notation` for a name whose kind the parts misread (for example `MAX_DUNGEON_SIZE` is a size: fix it in Task 7, not here), none expected.
- [ ] **Step 5: Run.** `uv run pytest -q tests/test_standard.py`: PASS.
- [ ] **Step 6: Docs.** `docs/coding-standard.md`: rule 1 gets the notation sentence of `../dm2/docs/coding-standard.md` rule 1 ("Notation: hex for bit masks and flags ... Decimal for counts ..."), adapted (KAOS addresses, Amiga file formats); rule 3 gets "No casts that only repeat the 68000 operand size"; rule 7 gets "No event numbers or byte offsets in names of functions, struct fields and constants"; new rule 12 (dm2 rule 10, packed fields); rule 9 names `data/bytes.h`. The check list gets the four checks.
- [ ] **Step 7: Commit.** `git commit -am "test: check number notation, packed fields, byte order and numbered names"`

---

### Task 5: Checks for KAOS addresses, predicates, platform code and ui writes

PRD R1.4, R1.5, R1.7, R1.8.

**Files:**
- Modify: `tests/test_standard.py`, `tests/standard_allowlist.txt`, `docs/coding-standard.md`

**Interfaces:**
- Produces: allowlist keys `kaos <file>`, `address <value>`, `bool <file>`, `platform <file>`, `engine-write <file>`; removed by Tasks 8-12, 13, 21-22, 32-35.

- [ ] **Step 1: KAOS checks.**

```python
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
```

- [ ] **Step 2: Predicates in `.c` files.** In `test_predicates_return_bool` remove the condition `path.suffix == ".h"`, so static predicates in `.c` files are checked too. Allowlist key stays `bool <file>`.
- [ ] **Step 3: Platform code.**

```python
PLATFORM_CODE = re.compile(r"^\s*#\s*if.*\b_WIN32\b|^\s*#\s*include\s*<(?:unistd|dirent|strings|execinfo|fcntl)\.h>|^\s*#\s*include\s*<sys/", re.M)


def test_platform_code_only_in_platform():
    ok = allowed()
    report("platform", [f"platform {rel(path)}  | {m.group().strip()}" for path in sources()
                        if path.parent.name != "platform" and ("platform", rel(path)) not in ok
                        for m in PLATFORM_CODE.finditer(path.read_text(errors="replace"))])
```

- [ ] **Step 4: ui writes to engine variables.**

```python
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
```

Redraw-flag writes (`attributes |= CHAMPION_ATTR_*`, `g_redraw |= ...`) stay allowed, as PRD R1.7 says (source: `docs/superpowers/2026-10-15-clean-code-inventory/ui.md` "Flag writes").

- [ ] **Step 5: Fill the allowlist** as in Task 4 Step 4 and run `uv run pytest -q tests/test_standard.py`: PASS.
- [ ] **Step 6: Docs.** `docs/coding-standard.md`: rule 1 "Each KAOS image address has one name with the prefix `KAOS_` in `data/tables.c`; other modules call `tables_*`"; rule 3 predicates also static; rule 8 "Platform code (`_WIN32`, POSIX headers) only in `platform/`" and "ui does not write engine variables, except redraw flags and hooks". Check list updated.
- [ ] **Step 7: Commit.** `git commit -am "test: check KAOS addresses, predicates, platform code and ui writes to engine variables"`

---

### Task 6: One owner for graphic, zone and layout ids

PRD R3. Source: `names.md` section 1 (rows for `ui/dialog.c`, `ui/startend.c`, `ui/command.c`, `ui/gameloop.c`, `ZONE_SPELL_TAB_FIRST`/`ZONE_SPELL_CASTER_TAB_FIRST`).

**Files:**
- Modify: `port/src/data/ids.h`, `port/src/ui/dialog.c`, `port/src/ui/startend.c`, `port/src/ui/command.c`, `port/src/ui/gameloop.c`, other ui files that `names.md` lists for `GFX_`/`ZONE_`/`LAYOUT_`/`ICON_`

- [ ] **Step 1:** For each `GFX_*`, `ZONE_*`, `LAYOUT_*`, `ICON_*` defined in a `.c` file (list: `grep -nE '^\s*(#define|enum).*\b(GFX|ZONE|LAYOUT|ICON)_' port/src/*/*.c`), move the definition into `data/ids.h` in the group of its kind (same value, same name). When `ids.h` has the value under another name for the same thing, keep the `ids.h` name and rename the call sites.
- [ ] **Step 2:** Merge `ZONE_SPELL_TAB_FIRST` and `ZONE_SPELL_CASTER_TAB_FIRST` (`data/ids.h:68`, `:226`) into the name with more call sites; `grep -rn` both in `port/src tests docs`.
- [ ] **Step 3:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_ui.py tests/test_golden_view.py tests/test_select.py`. Expected: PASS.
- [ ] **Step 4:** `clang-format -i` the changed files; `git commit -am "refactor: graphic, zone and layout ids live in data/ids.h"`

---

### Task 7: One name for each engine constant; number notation

PRD R3, R1.1. Source: `names.md` section 1 (engine rows), Task 4 allowlist `notation` lines.

**Files:**
- Modify: `port/src/engine/*.h`, `port/src/engine/*.c`, `port/src/data/tables.h`, `port/src/data/ids.h`, `port/src/data/*.c|h`, `tests/standard_allowlist.txt`, tests that bind a renamed constant by value only (none expected; constants are not in `libdm` symbols)

- [ ] **Step 1: Merge duplicates.** At least: `SLOT_ACTION_HAND` → `SLOT_HAND_ACTION` (`engine/game.h:23`); `CELLS_CENTERED` and `CELL_CENTERED` → one name in `engine/group.h`; `SKILL_PARRY_DEFENSE` → the skill enum of `data/tables.h`; group.c `ICON_*` → `data/ids.h`; `POTION_*` → `engine/inventory.h`; `WEAPON_*` of `projectile.c` → its owner header; `GROUP_RECORD_*_BYTE` (`group.h:57-58`) and `GROUP_*_BYTE` (`map.h:210`) → `map.h`; `HIT_PROBABILITY_NON_MATERIAL`/`_ALWAYS_HITS`; `PROJECTILE_DISABLED_MOVEMENT_TICKS`/`SHOT_DISABLED_MOVEMENT_TICKS`; `MIN_ACTIVE_GROUP_COUNT`/`ACTIVE_GROUP_MAX_COUNT`; `RECRUIT_STAT_MINIMUM`/`NEW_ADVENTURE_STAT_MINIMUM`; `MOVE_CHAIN_LIMIT`/`SQUARE_CHAIN_MAXIMUM`; `SPELL_EXPERIENCE_*`/`CAST_EXPERIENCE_*`; `SAMPLE_HEADER`/`SOUND_SAMPLE_HEADER`; the kinetic-energy maximum (three names) and full volume (four names); the creature `graphic_info` bits of `group.c` and `ui/view.c` → one set in `data/tables.h`. Read the "checked, not duplicates" note of `names.md` first; do not merge those.
- [ ] **Step 2: Square bits.** `SQUARE_RANDOM_ORNAMENT`, `PIT_INVISIBLE` (`ui/view.c:120-121`) → `engine/map.h`.
- [ ] **Step 3: Notation.** Rewrite each constant of the `notation` allowlist lines in the right base (mask → hex, count/size → decimal). `MAX_DUNGEON_SIZE 0x100000` (`engine/dungeon.c:24`) → `(1024 * 1024)` or `1048576`. Remove the `notation` lines. Constants in `ui/` and `platform/` too (one pass for the whole tree).
- [ ] **Step 4:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_golden_save.py tests/test_golden_view.py tests/test_creatures.py tests/test_actions.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: one name and one owner for each constant; hex for masks, decimal for counts"`

---

### Task 8: KAOS addresses and accessors for engine tables

PRD R2. Source: `kaos.md` section 3 "New functions — engine" and "Reuse existing (cached) functions", section 4 (caching).

**Files:**
- Create: `port/src/data/kaos_addresses.h` (no prefix line: defines only)
- Modify: `port/src/data/tables.c`, `port/src/data/tables.h`, `port/src/data/spells.h` (address defines), `port/src/ui/gameloop.c` and `port/src/platform/main.c` (`kaos_load` → `tables_load`), `port/src/engine/game.c`, `group.c`, `map.c`, `move.c`, `recruit.c`, `sensor.c`, `sound.c`, `timeline_process.c`, `champion.c`, `champion.h`, `dungeon.c`, `inventory.c`, `tests/standard_allowlist.txt`, test fixtures that need `tables_init`

**Interfaces:**
- Produces (exact signatures, `data/tables.h`):

```c
int16_t tables_step_east(int16_t direction);
int16_t tables_step_north(int16_t direction);
int16_t tables_arrow_steps_forward(int16_t arrow);
int16_t tables_arrow_steps_right(int16_t arrow);
uint8_t tables_ordered_cell_to_attack(int16_t row, int16_t i);
uint8_t tables_wound_probability_mask(int16_t index);
uint8_t tables_slot_to_steal_from(int16_t index);
uint8_t tables_torch_icon_offset(int16_t charges);
uint16_t tables_slot_drop_order(int16_t i);
uint8_t tables_champion_color(int16_t champion_index);
uint8_t tables_square_type_event(int16_t square_type);
int16_t tables_sound_graphic(int16_t sound);
uint8_t tables_sound_rate(int16_t sound);
uint8_t tables_sound_priority(int16_t sound);
uint8_t tables_sound_volume(int16_t ahead, int16_t side);
uint8_t tables_new_game_extra_things(int16_t thing_type);
uint16_t tables_door_attributes(int16_t door_set); /* the door info word, attributes << 8 | defense, read at call time */
int tables_load(const char *path); /* kaos_load, then tables_init; 0 on success, as kaos_load */
const char *tables_message_escape(int16_t code);
const char *tables_inscription_escape(int16_t code);
const char *tables_escape_replacement(int16_t code);
int16_t tables_portrait_width(void);
int16_t tables_portrait_height(void);
```

Each reads the image at call time through `kaos_*` inside `tables.c` (the same read as the old call site, so the same value and the same abort on a bad address). Parameter and return types follow the type of the value at the old call site; when the old site used another type, keep the old type (Review Focus 2).

- [ ] **Step 0: Where the addresses live.** Every `KAOS_*` address define goes into `data/kaos_addresses.h`. Only `data/` files include it (`tables.c`, `csbimage.c`, `csbtext.c`, `ftl.c` if it needs `KAOS_BASE`); the Task 5 check flags an include outside `data/`. One definition per name, so `test_each_constant_has_one_definition` passes.
- [ ] **Step 0b: Loader.** Add `tables_load(path)` = `kaos_load(path)` then `tables_init()`. `ui/gameloop.c:78-82` calls `tables_load` and drops its own `tables_init`; `platform/main.c:41` calls `tables_load` (an extra `tables_init` only reads the image). No `kaos_load` call stays outside `data/`. Tests keep calling `kaos_load` through ctypes (tests are not in `port/src`).
- [ ] **Step 1:** In `data/kaos_addresses.h` define the existing address defines of `data/tables.c` as `KAOS_<NAME>` (`CREATURE_INFO` → `KAOS_CREATURE_INFO` and so on, `kaos.md` section 3 table "Reuse existing"); add `KAOS_DOOR_INFO` and the `G_SYMBOL_*`/`G_SPELLS` addresses of `data/spells.h:6-8` as `KAOS_*`. Shared defines that later tasks migrate stay where they are in this task: `DOOR_INFO` (`tables.h:9`, used by `ui/view.c:1549` until Task 9), `CHAMPION_COLORS` (`champion.h:14`, used by `ui/ui_champion.c:84` until Task 10); each later task removes the old define when it migrates the last user. No address name has the part `TICKS` or `SIZE`: `ACTION_DISABLED_TICKS` → `KAOS_ACTION_DISABLED_TICKS_TABLE` is still `TICKS`; use `KAOS_ACTIONS_DISABLED` (the table of disabled times) and so on.
- [ ] **Step 2:** Add the functions above with their `KAOS_*` defines in `kaos_addresses.h` (addresses from `kaos.md` Table A and section 3), one-line comment each.
- [ ] **Step 3:** Replace each engine call site of `kaos.md` Table B (engine/*) with the accessor; remove the local address defines. `engine/map.c:261` uses `tables_creature_info(type)->attributes`; `engine/map.c:44` the door word: add `uint16_t tables_door_attributes(int16_t door_set)` that reads the word at call time (keeps `attributes << 8 | defense` without a cache); `engine/timeline_process.c:334` uses `*tables_light_power_to_light_amount(power)`.
- [ ] **Step 4: Cached accessors.** For each site that now calls a cached accessor (`tables_creature_info`, `tables_light_power_to_light_amount`), find the tests that reach it without `tables_init` (`kaos.md` section 4: `test_dungeon.py:43`, `test_game.py:88`, `test_recruit.py:53`, `test_port_data.py:102`, `test_csb_engine.py:71`, `test_sound.py:41`, `test_spells.py:46`, `test_view.py:160`; `test_view.py` matters because `test_golden_view.py` imports its `game` fixture). Add `lib.tables_init()` after `kaos_load` in each such fixture (harness adaptation, not a weakened assertion). In `test_csb_engine.py`, call it again after each adventure switch.
- [ ] **Step 5:** Remove the `kaos engine/*` and `address` allowlist lines that this task fixed.
- [ ] **Step 6:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_tables.py tests/test_game.py tests/test_dungeon.py tests/test_creatures.py tests/test_move.py tests/test_sound.py tests/test_recruit.py tests/test_csb_engine.py tests/test_golden_save.py`. PASS.
- [ ] **Step 7:** Update the renamed names in `docs/` (`docs/tables.md`, `docs/csb-*.md`; `grep -rn` each old name). `clang-format -i`; `git commit -am "refactor: engine reads KAOS tables through data/tables accessors"`

---

### Task 9: View tables through accessors

PRD R2. Source: `kaos.md` section 3 "Creature colors and aspects", "View squares and the dungeon view", "Palette changes" (view.c sites), Table B `ui/view.c`.

**Files:**
- Modify: `port/src/data/tables.c`, `port/src/data/tables.h`, `port/src/ui/view.c`, `port/src/ui/view.h`, `port/src/ui/layout.h`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces (`data/tables.h`): `tables_replacement_color`, `tables_replacement_change`, `tables_creature_aspect`, `tables_object_aspect`, `tables_projectile_aspect`, `tables_field_aspect`, `tables_creature_shift`, `tables_pile_shift`, `tables_object_scale`, `tables_projectile_scale`, `tables_explosion_scale`, `tables_rebirth_scale`, `tables_group_directions`, `ViewSquare tables_view_square(int16_t view_square)` with `typedef struct { int8_t lane, depth, object_coord, alcove_coord, creature_coord, explosion_coord, field_aspect; } ViewSquare;`, `tables_wall_set_layout_id`, `tables_wall_ornament_depth`, `tables_plaque_height`, `tables_floor_ornament_view_offset`, `tables_alcove_ornament`, `tables_fountain_ornament`, `tables_wall_ornament_coord_set`, `tables_floor_ornament_coord_set`, `tables_door_ornament_coord_set`, `tables_door_button_coord_set`, `tables_glyph_width`, `tables_glyph_height`, `tables_viewport_x`, `tables_viewport_y`, `tables_viewport_width`, `tables_viewport_height`, `typedef struct { int16_t x, y, w, h; } TablesRect;` and `TablesRect tables_layout_bitmap(int16_t slot)`, `const uint8_t *tables_palette_changes(int16_t changes)` with all 12 `TABLES_CHANGES_*` values of `kaos.md` section 3 (Task 10 uses `INVISIBILITY`, `ACTION_ICON`, `POINTER_SHADOW`). Signatures as in `kaos.md` section 3, with `int16_t` parameters for game values.

- [ ] **Step 1:** Add the functions and `KAOS_*` defines; aspect functions return `const uint8_t *` and `ui/view.c` keeps its byte-offset enums.
- [ ] **Step 2:** Replace the call sites in `ui/view.c`; remove the `S8` macro (`view.c:71`), the address defines of `view.c`, and `DOOR_INFO` of `tables.h:9`. `scaled()` (`view.c:906`) takes `const uint8_t *` instead of an address. `view.h:8-11` and `layout.h:7` stay until their last user moves: `LAYOUT_BITMAPS` (`dialog.c:64`), `PORTRAIT_SIZE` (`startend.c:277`, `ui_champion.c:202`) and `DUNGEON_PALETTE` (`platform/main.c:53`) in Task 10; `VIEWPORT_SIZE` (`ui.c:63`) and `VIEWPORT_ORIGIN` (`command.c:785`, `ui.c:319`, `platform/control.c:140`) in Task 11.
- [ ] **Step 3:** `view.c:1629` light thresholds: keep the index bound below 6 exactly as now (`kaos.md` section 3 note).
- [ ] **Step 4:** `TEXT_HEIGHTS 0x102c57` is the plaque heights table minus one (`kaos.md` section 5); name it `KAOS_PLAQUE_HEIGHTS` with the same value and a comment why the index starts at 1.
- [ ] **Step 5:** Remove the fixed allowlist lines; `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_view.py tests/test_golden_view.py tests/test_control.py`. PASS.
- [ ] **Step 6:** Docs renames; `clang-format -i`; `git commit -am "refactor: the dungeon view reads KAOS tables through data/tables accessors"`

---

### Task 10: Interface tables through accessors

PRD R2. Source: `kaos.md` section 3 "Palettes", "Interface, champion panel, pointer", Table B for `ui/gameloop.c`, `ui/ui.c`, `ui/ui_champion.c`, `ui/ui_menus.c`, `ui/dialog.c`, `ui/startend.c`, `platform/main.c`.

**Files:**
- Modify: `port/src/data/tables.c|h`, `port/src/ui/gameloop.c`, `ui.c`, `ui.h`, `ui_champion.c`, `ui_menus.c`, `dialog.c`, `startend.c`, `platform/main.c`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `void tables_palette(int16_t palette, uint16_t colors[16])` with `TABLES_PALETTE_INTERFACE, TABLES_PALETTE_POINTER, TABLES_PALETTE_CREDITS, TABLES_PALETTE_ENTRANCE, TABLES_PALETTE_DUNGEON` (+ light level 0-5); `uint16_t tables_palette_color(int16_t palette, int16_t color)` for the loops that fill part of a larger array (`ui.c:156`, `:166`); `tables_icon_sheet_first`, `tables_slot_box_zone`, `tables_slot_box_border`, `tables_champion_icon_width/height`, `tables_bar_shadow`, `tables_damage_splat_width/height(bool large)`, `tables_skill_level_text`, `tables_base_skill_text`, `tables_statistic_text`, `tables_direction_text`, `const char *tables_text(int16_t text)` with `TABLES_TEXT_*`, `tables_message_line_pad`, `tables_message_color`, `tables_button_border`, `tables_button_inner_border`, `TablesRect tables_csb_restart_box(bool inner)`, `PointerZone tables_pointer_zone(int16_t i)`, `tables_pointer_hotspot_x/y`, `tables_pointer_shadow_dx/dy`, `const uint8_t *tables_pointer_planes(bool hand, int16_t *w, int16_t *h)`.

- [ ] **Step 1:** Add the functions; one `load_palette` loop replaces the 5 copies (`gameloop.c:120, 544`, `ui.c:147, 156`, `startend.c:368`) through `tables_palette`.
- [ ] **Step 2:** `startend.c:266` duplicates `ui_skill_rank_name` (`ui_champion.c:401`): call it.
- [ ] **Step 3:** Replace the call sites of the listed files; remove their address defines; `ui.h:15-16` `SKILL_LEVEL_STRINGS`/`BASE_SKILL_STRINGS` go into `kaos_addresses.h`. Remove `CHAMPION_COLORS` (`engine/champion.h:14`), `LAYOUT_BITMAPS` (`layout.h:7`), `PORTRAIT_SIZE` and `DUNGEON_PALETTE` (`view.h`) here, after their last users moved.
- [ ] **Step 4:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_ui.py tests/test_gameloop.py tests/test_panels.py tests/test_entrance.py tests/test_select.py tests/test_csb_ending.py tests/test_golden_view.py`. PASS.
- [ ] **Step 5:** Docs renames; `clang-format -i`; `git commit -am "refactor: the interface reads KAOS tables through data/tables accessors"`

---

### Task 11: Input tables through accessors

PRD R2. Source: `kaos.md` section 3 "Input tables", Table B `ui/command.c`, `ui/ui.c:298-320`, `ui/dialog.c:144-146`, `platform/control.c:140-174`, "Indirect address users".

**Files:**
- Modify: `port/src/data/tables.c|h`, `port/src/ui/ui.c`, `ui.h`, `command.c`, `dialog.c`, `startend.c:235-237`, `port/src/platform/control.c`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `typedef struct { int16_t command, x1, x2, y1, y2; uint16_t buttons; } MouseInput;`, `bool tables_mouse_input(int16_t table, int16_t entry, MouseInput *out)` (false at the terminator), `void tables_set_mouse_input_box(int16_t table, int16_t entry, int16_t x1, int16_t x2, int16_t y1, int16_t y2)` (writes the image big-endian, as `startend.c:235-237` today), `int16_t tables_key_command(int16_t key_table, uint16_t key)` with `TABLES_KEYS_NONE = -1, TABLES_KEYS_ENTRANCE, TABLES_KEYS_INTERFACE, TABLES_KEYS_MOVEMENT, TABLES_KEYS_SLEEPING, TABLES_KEYS_FROZEN`; the `UI_INPUT_*` table ids move from `ui/ui.h` to `data/tables.h`.

- [ ] **Step 1:** Add the functions. Dialog tables (id >= `UI_INPUT_DIALOG_FIRST`) read through `KAOS_DIALOG_MOUSE_INPUTS` (pointer table, `kaos.md`).
- [ ] **Step 2:** `ui_mouse_table_address` (`ui.h:118`) goes; its callers use `tables_mouse_input`.
- [ ] **Step 3:** `command.c`: `primary_keys`/`secondary_keys` change from `uint32_t` address to the `int16_t` key table id; the `NO_TABLE`/`0` checks become `TABLES_KEYS_NONE`.
- [ ] **Step 4:** `platform/control.c:140-174`: viewport origin and mouse tables through the accessors. Remove `VIEWPORT_SIZE` and `VIEWPORT_ORIGIN` (`view.h`) after the last users (`ui.c:63`, `ui.c:319`, `command.c:785`, `control.c:140`) moved.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_ui.py tests/test_control.py tests/test_select.py tests/test_csb_ending.py tests/test_modes.py`. PASS.
- [ ] **Step 6:** Docs; `clang-format -i`; `git commit -am "refactor: mouse and key tables through data/tables accessors"`

---

### Task 12: SWSH addresses and the copy tables

PRD R2. Source: `kaos.md` "Not KAOS — ui/logo.c", section 5, Table B `ui/gameloop.c:469` (`planar_pointer`), `ui/anim.c:35,40,285` (comments).

**Files:**
- Create: `port/src/data/swsh.c`, `port/src/data/swsh.h` (prefix `swsh_`)
- Modify: `port/src/ui/logo.c`, `port/src/ui/anim.c`, `port/src/data/csbimage.c|h`, `port/src/data/csbtext.c`, `port/src/data/tables.h`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `typedef struct { const uint8_t *palette_script, *palette_script_end, *logo; const uint8_t *sample; uint32_t sample_length; uint16_t period; } SwshLogo; bool swsh_load(const char *path, SwshLogo *out); void swsh_free(SwshLogo *logo);` — the fields follow what `ui/logo.c:14-20` reads from `SWSH.FTL`; adapt names and types to the reads there.

- [ ] **Step 1:** Move the `SWSH.FTL` loading and the address defines of `ui/logo.c:14-20` into `data/swsh.c` with the prefix `SWSH_`; `logo.c` calls `swsh_load`.
- [ ] **Step 2:** `ui/anim.c` comments with `ANIM.FTL` addresses: rewrite them in game terms (the check strips comments, but the code must not need the address).
- [ ] **Step 3:** `data/csbimage.c` and `csbtext.c` include `kaos_addresses.h`; the KAOS column of the copy tables uses its `KAOS_*` names (add the missing ones, for example `KAOS_DIALOG_TABLES`). `KAOS_ACTION_SETS` and `KAOS_DIALOG_MOUSE_INPUTS` of `csbimage.c:10,12` become the `tables.c` names. `CSB_SPELLS`, `CSB_RESTART_BOXES` (`csbimage.h:8,10`) keep the `CSB_` prefix (APPB addresses).
- [ ] **Step 4:** The `address` check: every address name starts with `KAOS_`, `CSB_` or `SWSH_` and each value has one name per prefix. Remove all `kaos` and `address` allowlist lines; none stays.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_csb_engine.py tests/test_csb_text.py tests/test_csb_tools.py tests/test_tables.py tests/test_entrance.py`. PASS. `test_kaos_image_only_in_data` passes with no `kaos` allowlist line (it reads code without comments).
- [ ] **Step 6:** Docs (`docs/csb-*.md`, `docs/tables.md`); `clang-format -i`; `git commit -am "refactor: SWSH logo data in data/swsh; one KAOS name per address"`

---

### Task 13: Platform services through data hooks

PRD R4.1. Sources: `data/adf.c:6-13`, `ui/anim.c:1,5,101-102`, `engine/map.c:5-7,171-175`, `platform/main.c:484` (SDL enumeration; its callback at `:352`).

**Files:**
- Create: `port/src/data/os.h`, `port/src/data/os.c` (prefix `os_`)
- Modify: `port/src/data/adf.c`, `port/src/ui/anim.c`, `port/src/engine/map.c`, `port/src/platform/main.c` (sets the hooks), `port/src/platform/platform.c` if the hooks belong there, `tests/conftest.py` or the tests that call ADF extraction or the anim folder list without `main.c` (set a test hook), `tests/standard_allowlist.txt`

**Interfaces:**
- Produces:

```c
/* Prefixes: os_ */
typedef bool (*OsMakeFolder)(const char *path);
typedef void (*OsListFolder)(const char *path, void (*found)(const char *name, void *data), void *data);
extern OsMakeFolder g_os_make_folder_hook;   /* platform/ sets it: SDL_CreateDirectory */
extern OsListFolder g_os_list_folder_hook;   /* platform/ sets it: SDL_EnumerateDirectory */
bool os_make_folder(const char *path);       /* true when the folder exists afterwards (also when it existed before); false when no hook is set */
void os_list_folder(const char *path, void (*found)(const char *name, void *data), void *data);
int os_compare_names(const char *a, const char *b); /* ASCII compare without case, as strcasecmp */
```

- [ ] **Step 1:** Write `os.c`: `os_compare_names` in plain C (`tolower` per byte, `unsigned char`); the two wrappers call the hooks.
- [ ] **Step 2:** `platform/main.c` (or `platform.c`, where SDL starts) sets the two hooks with `SDL_CreateDirectory` and `SDL_EnumerateDirectory`.
- [ ] **Step 3:** `data/adf.c`: `make_folder` → `os_make_folder` (`adf.c:108,111` test `errno != EEXIST` today; `os_make_folder` returns true for an existing folder, so the test becomes `!os_make_folder(path)`); `strcasecmp` → `os_compare_names`; remove `<strings.h>`, `<sys/stat.h>`, `<direct.h>`, the `#ifdef _WIN32` and `ADF_DIR_MODE`.
- [ ] **Step 4:** `ui/anim.c`: `opendir`/`readdir` → `os_list_folder` with a callback; `strcasecmp` → `os_compare_names`.
- [ ] **Step 5:** `engine/map.c`: remove `execinfo.h`, the `#ifndef _WIN32` blocks and the backtrace; `report_free_record` keeps its log line.
- [ ] **Step 6: Tests without SDL.** The test library has no `main.c`: find tests that extract an ADF (`tests/test_adf.py`, `tests/test_extract.py`) or list the anim folder (`tests/test_music.py`, `test_ui.py`); give them a hook through ctypes (a Python callback with `os.makedirs` / `os.listdir`) in their fixture, or set default hooks in `os.c` that use C11 only where possible. Prefer: a ctypes `CFUNCTYPE` hook in the fixture, kept alive in the fixture object.
- [ ] **Step 7:** Remove the `platform` allowlist lines of `data/adf.c`, `ui/anim.c`, `engine/map.c`. `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_adf.py tests/test_extract.py tests/test_music.py`. PASS.
- [ ] **Step 8:** `clang-format -i`; `git commit -am "refactor: folder services through platform hooks; no platform code outside platform/"`

---

### Task 14: Windows build with no warning switches

PRD R4.2. Sources: the GCC warnings `engine/action.c:433`, `ui/music.c:9-13`, `ui/anim.c:106`, `ui/automap.c:155`, `ui/gameloop.c:160`, `ui/ui_champion.c:548`; `port/Makefile` `WIN_CFLAGS`.

**Files:**
- Modify: `port/Makefile`, `port/src/engine/action.c`, `port/src/ui/music.c`, `port/src/ui/anim.c`, `port/src/ui/automap.c`, `port/src/ui/gameloop.c`, `port/src/ui/ui_champion.c`

- [ ] **Step 1:** `port/Makefile`: `WIN_CFLAGS = $(CFLAGS)`; remove the comment sentence about the gcc-only checks.
- [ ] **Step 2:** `make -C port windows 2>&1 | grep warning:` lists the warnings.
- [ ] **Step 3:** `action.c:433`: GCC accepts a comment that matches `-Wimplicit-fallthrough=3`, for example `/* fall through */` on its own line; keep the reason on the line above.
- [ ] **Step 4:** `music.c:9-13`: put the clang pragmas under `#ifdef __clang__`. Then build for Windows: if `dr_mp3.h` gives GCC warnings, add `#pragma GCC diagnostic ignored "-W<name>"` per warning name that GCC prints, under `#elif defined(__GNUC__)` with push/pop (assumption: GCC does not accept the group names `-Wall`/`-Wextra` in a pragma). Fallback: compile `music.c` for Windows with `-isystem` for the folder of `dr_mp3.h`.
- [ ] **Step 5:** The 4 `snprintf` sites: size the buffer for the largest value of the type (`ui/automap.c:155` `char text[4]` with `%02d` of an `int16_t` needs 7), or check the return value where a path can be long.
- [ ] **Step 6:** `make -C port windows 2>&1 | grep -c 'warning:'` prints `0`; `make -C port` 0 warnings; `uv run pytest -q tests/test_automap.py tests/test_music.py`. PASS.
- [ ] **Step 7:** `clang-format -i`; `git commit -am "build: the Windows build has 0 warnings without warning switches"`

---

### Task 15: No long for game values

PRD R4.3. Sources: `platform/state_json.c:81,117,122,126,130`, `platform/control.c:60,224-229,239-240,310,321,355,388,410-412,432,439,475,763,820,1040,1112`, `platform/main.c:464,508`.

**Files:**
- Modify: `port/src/platform/state_json.c|h`, `port/src/platform/control.c`, `port/src/platform/main.c`

- [ ] **Step 1:** `json_int(long)` → `json_int(int64_t)` with `"%" PRId64` (`<inttypes.h>`); callers pass `int64_t`.
- [ ] **Step 2:** `(long)g_game_time` → `(uint32_t)` with `PRIu32` or `int64_t` with `PRId64`, as the format needs; `strtol` results go into `int64_t` and are range-checked as today.
- [ ] **Step 3:** No `long` type in code: `grep -nE '\b(unsigned )?long\b' port/src/*/*.[ch] | grep -v dr_mp3 | grep -vE '/\*|"'` prints nothing (comments and strings such as "too long" at `control.c:1189` do not count).
- [ ] **Step 4:** `make -C port`, `make -C port windows` 0 warnings; `uv run pytest -q tests/test_control.py tests/test_modes.py`. PASS (the JSON output stays the same).
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: game time and JSON numbers use fixed-width types"`

---

### Task 16: ADF names safe on Windows

PRD R4.4. Source: `data/adf.c:141`.

**Files:**
- Modify: `port/src/data/adf.c`
- Test: `tests/test_adf.py`

- [ ] **Step 1: Write the failing test** (a parser rule with more than one outcome). Read `tests/test_adf.py` for the helper that builds an ADF image or calls the name check; if `adf.c` has a static name check, make it `bool adf_is_safe_name(const char *name)` in `adf.h`. Test:

```python
@pytest.mark.parametrize("name, safe", [("Dungeon.DAT", True), ("a\\b", False), ("a:b", False), ("a*b", False), ("a?b", False),
                                        ('a"b', False), ("a<b", False), ("a>b", False), ("a|b", False), ("a\x01b", False),
                                        ("a/b", False), ("", False), (".", False), ("..", False)])
def test_unsafe_amiga_names_are_rejected(lib, name, safe):
    lib.adf_is_safe_name.restype = ctypes.c_bool
    lib.adf_is_safe_name.argtypes = [ctypes.c_char_p]
    assert lib.adf_is_safe_name(name.encode("latin-1")) == safe
```

- [ ] **Step 2:** `uv run pytest -q tests/test_adf.py`: FAIL.
- [ ] **Step 3:** Implement: reject `/ \ : * ? " < > |`, bytes below 32, empty, `.` and `..`. `extract_entry` keeps today's behavior for an unsafe name: it returns -1 and the extraction stops with an error (`adf.c:141`); the caller in `platform/` logs the error as today. The test binds `lib.adf_is_safe_name.restype = ctypes.c_bool` and `argtypes = [ctypes.c_char_p]`.
- [ ] **Step 4:** `uv run pytest -q tests/test_adf.py tests/test_extract.py`: PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "fix: skip Amiga file names that are unsafe on Windows"`

---

### Task 17: Turn-based mode not offered on Windows

PRD R4.5. Sources: `ui/select.c:26` (the mode menu: `[SCREEN_MODE] = {"MODE", {"VANILLA","ADVANCED","LLM"}, 3}`), `select_start` (called by `platform/main.c` and `tests/test_select.py`, with a `csb_present` parameter), `platform/main.c:315-325` (`apply_mode`), `platform/control_win.c`.

**Files:**
- Modify: `port/src/ui/select.c`, `port/src/ui/select.h`, `tests/test_select.py`, `port/src/platform/main.c`, `port/src/platform/control.h`, `port/src/platform/control.c`, `port/src/platform/control_win.c`, `port/Makefile`, `README.md` (Windows note)

**Interfaces:**
- Produces: `bool control_is_available(void);` (true in `control.c`, false in `control_win.c`).

- [ ] **Step 1:** Add `control_is_available`. `control_win.c` keeps its stubs; the Makefile already picks the file, so remove the `#ifdef _WIN32` that wraps all of `control_win.c` if the Makefile never compiles it on macOS (check `GAME` in `port/Makefile`: `control_win.c` is in `$(GAME)` today, so keep the guard or filter it out of `GAME`; choose the filter, so `platform/` has no `#ifdef _WIN32` either).
- [ ] **Step 2:** `ui/` cannot include `platform/control.h` (layers): `select_start` gets a `bool llm_mode_available` parameter, like `csb_present`; with false the mode menu has 2 entries (VANILLA, ADVANCED). `platform/main.c` passes `control_is_available()`; `tests/test_select.py` passes `true` (and one test with `false` only if the menu count is a rule with more than one outcome that no test covers: the 2-entry menu is UI, no test). `--mode turnbased` with `!control_is_available()` prints `turn-based mode needs macOS` and exits with an error code.
- [ ] **Step 3:** `make -C port`; `make -C port windows` 0 warnings; `uv run pytest -q tests/test_modes.py tests/test_select.py`. PASS. Manual check item: on Windows (Wine) the menu has no turn-based entry.
- [ ] **Step 4:** `clang-format -i`; `git commit -am "feat: hide the turn-based mode on Windows"`

---

### Task 18: Accessors for timeline event fields

PRD R6.1. Sources: `engine/game.h:94-100` (`TimelineEvent`), `engine/group.c:986,989,1817`, `engine/timeline_process.c:470-471`, `engine/action.c` (`QUIVER_SLOT_SHIFT`), `names.md` 2a (`E_B`/`E_C`).

**Files:**
- Modify: `port/src/engine/timeline_process.h` (or `game.h`, the owner of `TimelineEvent`), `engine/group.c`, `engine/timeline_process.c`, `engine/action.c`, other engine files that pack `b`/`c` (find: `grep -nE '\.(b|c)\s*(=|>>|<<|&)' port/src/engine/*.c`), `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: static inline accessors in the owner header, one pair per meaning, for example:

```c
static inline uint16_t event_square(int16_t x, int16_t y) { return (uint16_t)(x << EVENT_X_SHIFT | y); }
static inline int16_t event_square_x(const TimelineEvent *e) { return e->b >> EVENT_X_SHIFT; }
static inline int16_t event_square_y(const TimelineEvent *e) { return e->b & EVENT_Y_MASK; }
static inline uint16_t event_cell_effect(int16_t cell, int16_t effect);
static inline int16_t event_cell(const TimelineEvent *e);
static inline int16_t event_effect(const TimelineEvent *e);
static inline uint16_t event_aspect_delay(uint8_t ticks);         /* group.c:986, 989, 1817 */
static inline int16_t event_quiver_slot(const TimelineEvent *e);  /* timeline_process.c:470: slot + 1 in the high byte, 0: none */
```

The `TimelineEvent` layout stays (`b`, `c` stay `uint16_t`). Names follow the meaning in the code; the header's `Prefixes` line gets `event_`, if no other header owns it (check `grep -n 'Prefixes' port/src/*/*.h`).

- [ ] **Step 1:** List every pack and unpack of `b` and `c` with its event type.
- [ ] **Step 2:** Add the accessors with `*_SHIFT`/`*_MASK` constants in the owner header.
- [ ] **Step 3:** Replace the call sites; `QUIVER_SLOT_SHIFT` moves from `action.c` to the owner header.
- [ ] **Step 4:** Remove the `packed` allowlist lines of the changed files. `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_game.py tests/test_creatures.py tests/test_actions.py tests/test_golden_save.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: named accessors for the fields of timeline events"`

---

### Task 19: Other packed fields

PRD R6.1. Sources: `engine/group.c:653` (`door_info >> 8 & 1`), `engine/projectile.c:59` (`door_attributes`), `engine/group.c:767` (`party_scent_ordinal`) vs `engine/move.c:69`, `engine/game.c:144` (event key), `engine/random.c:22`, `data/csbtext.c:153`, `data/ftl.c:96,99`, `data/graphics.c:89,94`, `platform/main.c:102,103,115`, `platform/control.c:580`; Task 4 `packed` allowlist lines.

**Files:**
- Modify: the files of the remaining `packed` allowlist lines; owner headers (`engine/map.h` door bits, `engine/game.h` positions, `data/bytes.h`)

- [ ] **Step 1:** Door see-through bit: one accessor `bool door_is_see_through(uint16_t door)` in `engine/map.h` that reads `g_current_map_door_info[door_type(door)]` (the map state filled at `map.c:44`, not the KAOS table) with a named shift and bit; `projectile.c:59` (`door_attributes`) and `group.c:653` call it or a `door_attributes(door)` accessor of `map.h`.
- [ ] **Step 2:** One `position_pack(map, x, y)` in `engine/game.h` next to the unpack (`game.h:80`); `group.c:767` and `move.c:69` call it.
- [ ] **Step 3:** Byte splits of a word in `data/` and `platform/` use `WR16`/`BE16` or a named `*_SHIFT` in their own module (`csbtext.c:153` → `WR16`). Bit-stream decoders (`ftl.c`, `graphics.c`) name their shifts (`NIBBLE_BITS`, `BYTE_BITS`) in their own file.
- [ ] **Step 4:** Remove all remaining `packed` and `byteorder` allowlist lines. `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_ftl.py tests/test_csb_text.py tests/test_port_data.py tests/test_golden_save.py tests/test_golden_view.py tests/test_control.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: packed fields through accessors of their owner"`

---

### Task 20: Turn helpers for directions and cells

PRD R6.2. Source: about 60 `& 3` sites (`engine/group.c` 33, `engine/projectile.c` 13, `ui/view.c` 15, others), `NORMALIZE` (`engine/game.h:8`).

**Files:**
- Modify: `port/src/engine/game.h`, `engine/group.c`, `engine/projectile.c`, `engine/*.c` with `& 3` on a direction or cell, `ui/view.c`, `ui/command.c`

**Interfaces:**
- Produces (`engine/game.h`):

```c
static inline int16_t turn_right(int16_t direction) { return NORMALIZE(direction + 1); }
static inline int16_t turn_back(int16_t direction) { return NORMALIZE(direction + 2); }
static inline int16_t turn_left(int16_t direction) { return NORMALIZE(direction + 3); }
```

Add `turn_` to the `Prefixes` line of `game.h`.

- [ ] **Step 1:** `grep -nE '&\s*3\b' port/src/*/*.c`; for each site decide: a direction or cell wrap → `turn_*` (for `+1`, `+2`, `+3`) or `NORMALIZE`; another 2-bit field (`cells >> 2 * i & 3`) → an accessor of its owner (`engine/group.h`, for example `group_cell(cells, i)`); a random mask → a named `*_MASK`.
- [ ] **Step 2:** Replace; keep the operand type of each site (Review Focus 2).
- [ ] **Step 3:** `make -C port`; `uv run pytest -q tests/test_creatures.py tests/test_move.py tests/test_actions.py tests/test_golden_view.py tests/test_golden_save.py`. PASS. `grep -cE '&\s*3\b' port/src/*/*.c` counts only sites that are not directions (list them in the commit message).
- [ ] **Step 4:** `clang-format -i`; `git commit -am "refactor: turn helpers for directions and cells"`

---

### Task 21: bool and casts in engine/

PRD R6.3, R6.4. Sources: Task 5 `bool engine/*` allowlist lines; engine review list: `group.c:427,559`, `champion.c:234`, `inventory.c:256`, `dungeon.c:246 valid_thing`, `savefile.c:82 save_decode`, `group.c:803`; params `notify`, `attacking`, `allow_imaginary`, `new_game`; casts `action.c:123`, `move.c:61,89`, `champion.c:349`, `game.c:93,343`.

**Files:**
- Modify: `port/src/engine/*.c|h`, `tests/standard_allowlist.txt`; tests that bind a changed function signature through ctypes (`restype`/`argtypes` of a function that changes from `int16_t` to `bool`: use `ctypes.c_bool`)

- [ ] **Step 1:** Each truth value (local, parameter, return) in `engine/` → `bool`. A flag stored in a game record or a save keeps its type. A function that tests bind with `restype = c16` and that becomes `bool`: change the test `restype` to `ctypes.c_bool` (harness, not assertion).
- [ ] **Step 2:** Casts: remove a cast that changes nothing (narrowing assignment, same type); keep a cast that wraps, changes sign or truncates in an expression, with a comment when the reason is not obvious. `game.c:93` `(int32_t)((uint32_t)a * b)`: keep if the unsigned multiply avoids signed overflow; write the reason.
- [ ] **Step 3:** Remove the `bool engine/*` allowlist lines. `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_game.py tests/test_champion.py tests/test_creatures.py tests/test_save.py tests/test_savefile.py tests/test_golden_save.py`. PASS.
- [ ] **Step 4:** `clang-format -i`; `git commit -am "refactor: bool for truth values and no casts without effect in engine/"`

---

### Task 22: bool and casts in data/, ui/ and platform/

PRD R6.3, R6.4. Sources: ui review: predicates `command.c:470 in_box`, `view.c:374 in_zone`, `advanced.c:36`; about 29 flag locals (`view.c:589-596`); casts `command.c:377,393`, `dialog.c:101`, `view.c:1506`.

**Files:**
- Modify: `port/src/data/*.c|h`, `port/src/ui/*.c|h`, `port/src/platform/*.c|h`, `tests/standard_allowlist.txt`, ctypes `restype` of changed functions

- [ ] **Step 1:** As Task 21 Steps 1-2 for these layers.
- [ ] **Step 2:** Remove the remaining `bool` allowlist lines; none stays.
- [ ] **Step 3:** `make -C port`, `make -C port windows` 0 warnings; `uv run pytest -q tests/test_standard.py tests/test_ui.py tests/test_view.py tests/test_golden_view.py tests/test_control.py tests/test_select.py`. PASS.
- [ ] **Step 4:** `clang-format -i`; `git commit -am "refactor: bool for truth values and no casts without effect in data, ui and platform"`

---

### Task 23: Small data cleanups

PRD R10.1, R10.3, R10.4. Sources: `data/csbtext.c:81` `COUNT`, `data/tables.c:170` `GET`, `sizeof a / sizeof a[0]` sites; `data/csbimage.c:157-159`; `data/graphics.c:77-78`.

**Files:**
- Modify: `port/src/data/bytes.h`, `data/csbtext.c`, `data/tables.c`, `data/adventure.c`, `data/csbimage.c`, `data/graphics.c`, `platform/main.c`, `platform/control.c`, other files with `sizeof x / sizeof x[0]`

**Interfaces:**
- Produces: `#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))` in `data/bytes.h` (an `int`, as `COUNT` of `csbtext.c:81` today, so `int i < ARRAY_COUNT(a)` raises no `-Wsign-compare`).

- [ ] **Step 1:** `ARRAY_COUNT` replaces `COUNT`, the count part of `GET`, and every `sizeof a / sizeof a[0]` (`grep -rnE 'sizeof\s*\(?\w+\)?\s*/\s*sizeof' port/src`).
- [ ] **Step 2:** `csbimage.c:157-159`: replace `appb->data - KAOS_BASE` with a helper `static uint8_t *appb_at(const Image *img, uint32_t addr)` that subtracts the base from the address, not from the pointer. Same result for every valid address.
- [ ] **Step 3:** `graphics.c`: `NEXT()`/`PUT()` → a small reader/writer struct with `static inline` functions, as `Nibbles` in `data/ftl.c:71`.
- [ ] **Step 4:** Comma operators in `data/` (`adventure.c:89`, `csbimage.c:157`, `csbtext.c:155,170-171`): plain statements.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_port_data.py tests/test_csb_engine.py tests/test_csb_text.py tests/test_ftl.py tests/test_golden_view.py`. PASS.
- [ ] **Step 6:** `clang-format -i`; `git commit -am "refactor: ARRAY_COUNT, a bit reader in graphics.c, no pointer outside the APPB image"`

---

### Task 24: data/file and the adventure struct

PRD R10.5, R10.6. Sources: `data/ftl.c:56-62` `read_file`, `data/adventure.c:60,88` (open-and-close tests), `data/adventure.c` parallel arrays, `platform/main.c:292,338`.

**Files:**
- Create: `port/src/data/file.c`, `port/src/data/file.h`
- Modify: `data/ftl.c|h`, `data/adventure.c|h`, `platform/main.c`, and the callers of `read_file`: `engine/dungeon.c:332`, `engine/savegame.c:409,491,493`, `data/graphics.c:37`, `data/adf.c:58`, `data/adventure.c:73`, `ui/logo.c:30` (or `data/swsh.c` after Task 12), `ui/anim.c:110,159`, `data/ftl.c:227`; tests that bind `read_file` (`grep -rn read_file tests`)

**Interfaces:**
- Produces:

```c
/* Prefixes: file_ */
uint8_t *file_read(const char *path, size_t *len); /* malloc'd contents, NULL when missing or unreadable; same signature as read_file (ftl.c:56) */
bool file_exists(const char *path);
```

and in `data/adventure.h` one `typedef struct { const char *name, *dir, *title; ... } Adventure;` table with the fields that `adventure.c` and `main.c` keep in parallel arrays today (names, dirs, engine, save, backup, titles).

- [ ] **Step 1:** Move `read_file` into `file.c` as `file_read` with early returns (no assignments inside `&&`); add `file_exists`.
- [ ] **Step 2:** Callers and tests that bind `read_file` follow the new name.
- [ ] **Step 3:** One `Adventure` table replaces the parallel arrays of `adventure.c` and `main.c`.
- [ ] **Step 4:** `make -C port`; `uv run pytest -q tests/test_ftl.py tests/test_csb_adventure.py tests/test_extract.py tests/test_modes.py tests/test_select.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: data/file for file reads; one table per adventure"`

---

### Task 25: One spell API

PRD R10.2. Sources: `data/spells.c|h` (`spell_get`, `SpellRecord`), `data/tables.h` (`tables_spell`, `Spell`), `tests/test_spells.py:29` (`Spell` class mirrors `SpellRecord`), `names.md` section 4.

**Files:**
- Modify: `port/src/data/spells.c|h`, `data/tables.c|h`, `tests/test_spells.py`, callers of the removed API

- [ ] **Step 1:** Count the C callers of each API (`grep -rn 'spell_get\|tables_spell' port/src`). Keep the API with more C callers; if `spell_get` has none (engine review: 0 callers), keep `tables_spell`.
- [ ] **Step 2:** Move each `test_spells.py` test to the kept API: the `ctypes` struct mirrors `Spell` (`data/tables.h:86-93`). The assertions on `spell_kind`, `spell_type`, `spell_disabled_ticks` (`test_spells.py:71-80`) read the same values from `kind`, `type`, `disabled_ticks` of `tables_spell(i)` (approved weakened assertion: same values, other access path). Functions of `spells.c` with C callers (`spell_find`, `spell_symbol`, `spell_symbol_mana_cost`, `spell_count`: check with grep) stay.
- [ ] **Step 3:** Remove `spell_get`, `SpellRecord` and the `SpellRecord` functions without a C caller (`spell_kind`, `spell_type`, `spell_disabled_ticks`, `spell_required_level`, `spell_experience`, `spells.c:27-73`).
- [ ] **Step 4:** `make -C port`; `uv run pytest -q tests/test_spells.py tests/test_tables.py tests/test_csb_magic_map.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: one spell table API"`

---

### Task 26: Comma operators and dense lines in ui/ and platform/

PRD R5 (one statement per line) and ui review item 7. Sources: `platform/main.c:41,43,52,67,69,271,297,325,341` (`return SDL_Log(...), 0`), `platform/control.c:262,568,574,580,583,839`, `engine/champion.c:157,324-326,944`, ui chained assignments `command.c:307,452`, declarations with 3+ initialized variables on one line (`view.c` 16, `command.c` 9).

**Files:**
- Modify: `port/src/platform/main.c`, `platform/control.c`, `engine/champion.c`, `ui/command.c`, `ui/view.c`, other files from `grep -nE '\breturn [^;]*\),\s*[^;]+;' port/src/*/*.c`

- [ ] **Step 1:** Comma operators → statements with braces (`grep -nE ',\s*[a-z_]+\s*(\+\+|--|[-+*/|&]?=)' port/src/*/*.c` and the return form).
- [ ] **Step 2:** Chained assignments → one per line.
- [ ] **Step 3:** A declaration with initializers: one variable with an initializer per line.
- [ ] **Step 4:** Long functions: split each function of an `open: module cleanup` `long` line (Task 3) whose parts have their own game meaning; remove the line. A function that the original keeps in one piece for a reason gets a lasting allowlist reason instead (rule 3 of the standard). Functions in `ui/view.c` that Task 36 replaces stay for Task 36.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_control.py tests/test_modes.py tests/test_champion.py tests/test_golden_view.py tests/test_golden_save.py`. PASS.
- [ ] **Step 6:** `clang-format -i`; `git commit -am "style: one statement per line and short functions"`

---

### Task 27: Game names for fields and constants

PRD R7.2. Source: `names.md` 2a.

**Files:**
- Modify: `port/src/engine/game.h` (`Party`), `engine/action.c`, `engine/magic.c`, `engine/savefile.c` (`H_BYTE_12A`, `HEADER_BYTE_12A`, `HEADER_BYTE_NEW_ADVENTURE`), `data/tables.h` (`ActionSet.byte_6`), `engine/champion.c` (`TIME_BIT_6..8`), `tests/test_tables.py:37`, `tests/standard_allowlist.txt`, `docs/` mentions

- [ ] **Step 1:** `event73_count_thieves_eye` → `thieves_eye_count`, `event79_count_footprints` → `footprints_count`, `event71_count_invisibility` → `invisibility_count` (`tests/test_champion.py:54` `Party` uses its own names; no test change).
- [ ] **Step 2:** `H_BYTE_12A` and its two header constants → `H_SAVE_KIND` with `SAVE_KIND_GAME`, `SAVE_KIND_NEW_ADVENTURE` (read `savefile.c` to confirm the meaning first).
- [ ] **Step 3:** `ActionSet.byte_6` → `flags`; `tests/test_tables.py:37` field name follows.
- [ ] **Step 4:** `TIME_BIT_6..8` → names of what the bit selects in the time test (read `champion.c:98-100` callers).
- [ ] **Step 5:** Remove all `numbered` allowlist lines; any name that stays needs a reason line. `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_tables.py tests/test_savefile.py tests/test_save.py tests/test_champion.py tests/test_golden_save.py`. PASS.
- [ ] **Step 6:** Docs; `clang-format -i`; `git commit -am "refactor: game names for fields and constants with numbers"`

---

### Task 28: Function names in engine/ and data/

PRD R7.1. Source: `names.md` 2b (rows of `engine/` and `data/`); public renames that many tests bind: `gameloop_new_game` (ui, Task 29), `dungeon_square_first_thing` (8 test files), `game_new` (6), `gfx_item`/`gfx_bitmap`, `spell_*`, `tables_*`.

**Files:**
- Modify: `port/src/engine/*`, `port/src/data/*`, `tests/*.py` that bind a renamed function, `docs/` (`docs/symbols/*.md`, `docs/game-state.md`), `tests/standard_allowlist.txt` (`prefix` lines if a prefix changes)

- [ ] **Step 1:** Rename each row of `names.md` 2b for `engine/` and `data/` to its proposed `<prefix><verb>_<object>` name. Where `names.md` proposes no change or "optional", keep the name. Functions without a module prefix (`get_square_group`, `link_thing_to_list`, the generic `get_`/`set_`/`is_` prefixes of `map.h:3`) get the prefix of their header; update the `Prefixes` lines.
- [ ] **Step 2:** `grep -rln '<old>' port/src tests docs tools` for each name; update all.
- [ ] **Step 3:** `make -C port`; `uv run pytest -q tests/test_standard.py` and the test files that the rename touched. PASS.
- [ ] **Step 4:** `clang-format -i`; `git commit -am "refactor: verb-object names for engine and data functions"`

---

### Task 29: Function and variable names in ui/ and platform/

PRD R7.1, R7.4. Source: `names.md` 2b (ui/platform rows, except `d3l2`..`d0c`, which Task 36 removes), 2c (file-static variables: `vp` → `viewport`, `mw`/`mh` → `map_width`/`map_height`, `pals` → `dungeon_palettes`, `win`, `ren`, `tex`, `dat`, `load_ptr`).

**Files:**
- Modify: `port/src/ui/*`, `port/src/platform/*`, tests that bind renamed functions, `docs/`

- [ ] **Step 1:** Rename the functions (`wall` → `draw_wall_bitmap`, `aspect` → `get_square_aspect`, `object`, `group`, `creature`, `things`, `field`, `step` and the others of `names.md`).
- [ ] **Step 2:** Rename the file-static variables of `names.md` 2c; no `g_` for file-static variables.
- [ ] **Step 3:** Short local names: keep only loop indexes and a variable used in a few adjacent lines; `Champion *c` over a long function → `champion`.
- [ ] **Step 4:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_view.py tests/test_golden_view.py tests/test_ui.py tests/test_gameloop.py tests/test_control.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: verb-object function names and descriptive variable names in ui and platform"`

---

### Task 30: Comments in game terms

PRD R7.3, R7.5. Source: `names.md` section 3 (126 comments that are only an original name, 80 of them in `data/csbimage.c`; 52 that open with an original name); `g_slots_to_steal_from` is missing from both symbol CSVs (`data/csbimage.c:41`).

**Files:**
- Modify: the files of `names.md` section 3; `docs/kaos-symbols.csv` (add `g_slots_to_steal_from` with its address `0x100b8a`, `kaos.md` section 5)

- [ ] **Step 1:** Each comment that is only an original name (`/* g_viewport_bitmap */`) → a game-terms comment (`/* the dungeon view bitmap */`). In the copy table of `data/csbimage.c` the `KAOS_*` names of Task 12 already tell the table; the comment says the game meaning in a few words or goes away when it only repeated the name (this is the one case where a comment is replaced by the name in code).
- [ ] **Step 2:** A comment that opens with an original function name (`set_square_aspect: ...`) → the game-terms sentence without the name.
- [ ] **Step 3:** Check each removed original name is in `docs/kaos-symbols.csv` (`names.md`: 106 of 109 are); add the missing one.
- [ ] **Step 4:** Functions without a one-line comment that are not trivial (`names.md`, ui review: `view.c` 33, `gameloop.c` 10, `select.c` 10, `map.c`, `dungeon.c`, `ftl.c sum8`, `adf.c match_name`/`extract_entry`, `state_json.c pair`/`separate`): add one line in game terms.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_standard.py tests/test_symbols.py`. PASS.
- [ ] **Step 6:** `git commit -am "docs: comments in game terms instead of original symbol names"`

---

### Task 31: Golden snapshots of the ui rules

Pins R8 before the move. Source: `ui.md` section 2 (rules with no test: B fill at fountain, C vi altar event creation, D door button, E rune cost and delete, torch burn, H turn to attacker, J action list filter, freeze-life and movement tick decrements). A golden snapshot of the current behavior is a reference check.

**Files:**
- Create: `tests/test_golden_rules.py`, `tests/golden/rules.json`

**Interfaces:**
- Consumes: the `game` fixture of `tests/test_save.py:40-56` (`from test_save import game, recruit_and_hold_object, state`; it sets the ctypes types that `Game`, `state` (`dungeon_save.restype`, `free.argtypes`) and the helpers need), the click helpers of `Game`. Each scenario starts with `gameloop_new_game` and the fixed seed, so the module fixture shares no state between scenarios.
- Produces: one test per rule; each runs a scenario from a new game with a fixed seed and compares the SHA-256 of `state(game)` (all saved globals, champions, party, timeline, dungeon) and of the frame buffer after the scenario with `tests/golden/rules.json`.

- [ ] **Step 1:** Write the helper:

```python
def snapshot(game):
    """SHA-256 of the saved game state and of the screen after a scenario."""
    s = state(game)
    fb = bytes((ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb"))
    return {"state": hashlib.sha256(repr(sorted(s.items())).encode()).hexdigest(), "screen": hashlib.sha256(fb).hexdigest()}
```

- [ ] **Step 2:** Write one scenario per rule (each from `gameloop_new_game`, seed `0x1234ABCD`, one champion recruited with `recruit_and_hold_object` where the rule needs a champion):
  - fill: walk to the first fountain with an empty flask or waterskin in the leader hand, click the fountain;
  - drink: click the fountain with an empty hand (vanilla and advanced);
  - vi altar: drop bones on the vi altar of level 1 (find its square with `dungeon_square`/sensor type), run 30 ticks;
  - door button: click the button of the first door with a button, run 20 ticks;
  - runes: add 3 runes, delete 1, add 1, with mana set to 100 and to 2;
  - torch: hold a lit torch, run 2000 ticks;
  - turn to attacker: put a creature next to the party (as `tests/test_creatures.py` does), run until it attacks, run 20 ticks;
  - action list: hold each of 5 weapons in turn with skill levels set low and high; snapshot `g_action_count` and `g_action_list_minimum_skill_levels` (`ui/ui.h:80`, `ui/ui_menus.c:29`) and the action names per weapon;
  - ticks: set `freeze_life_ticks` and a movement-disabled time, run 50 ticks.
  Use the `DM_UPDATE_GOLDEN` pattern of Task 1.
- [ ] **Step 3:** `DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_rules.py -n 0`; then 3 runs without it: PASS.
- [ ] **Step 4:** Detection: change one constant of each rule once (for example `FOUNTAIN_WATER`), run, expect FAIL for that scenario; restore.
- [ ] **Step 5:** `git add tests/test_golden_rules.py tests/golden/rules.json && git commit -m "test: golden snapshots of the game rules that run in ui"`

---

### Task 32: Champion and inventory rules into engine/

PRD R8. Source: `ui.md` groups A, B, H, I, L, R and the "Consume flags" and "Technical" notes.

**Files:**
- Modify: `port/src/engine/champion.c|h`, `engine/inventory.c|h`, `ui/command.c`, `ui/ui_menus.c`, `ui/ui.c`, `ui/ui_champion.c`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces:

```c
void champion_drink_from_fountain(bool whole_party);           /* A */
bool inventory_fill_leader_hand_at_fountain(void);             /* B: true when the hand object changed */
uint16_t champion_turn_to_last_attackers(void);                /* H: bit i set when champion i turned */
int16_t champion_move_to_cell(int16_t champion_index, int16_t cell); /* I: the other champion index or -1 */
void champion_go_to_sleep(void);                               /* L */
void inventory_set_open_champion(int16_t ordinal);             /* R: 0 closes */
```

- [ ] **Step 1:** Move each rule body from ui into the function; ui calls it at the same place, then draws. H: ui draws the status boxes from the returned mask after the call (each box is separate, `ui.md` H note).
- [ ] **Step 2:** The redraw-flag writes inside a moved rule move with it.
- [ ] **Step 3:** Remove the `engine-write` lines of the files when they have no write left; run `uv run pytest -q tests/test_standard.py tests/test_golden_rules.py tests/test_modes.py tests/test_control.py tests/test_save.py tests/test_ui.py`. PASS.
- [ ] **Step 4:** `clang-format -i`; `git commit -am "refactor: champion and inventory rules move from ui to engine"`

---

### Task 33: Magic and sensor rules into engine/

PRD R8. Source: `ui.md` groups C, D, E, K.

**Files:**
- Modify: `port/src/engine/magic.c|h`, `engine/sensor.c|h`, `ui/command.c`, `ui/ui_menus.c`, `ui/ui.c`, `ui/gameloop.c`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces:

```c
void sensor_add_vi_altar_rebirth_event(uint16_t bones, int16_t x, int16_t y, int16_t cell); /* C */
void sensor_press_door_button(int16_t x, int16_t y);                                       /* D */
bool magic_add_champion_symbol(int16_t symbol);                                            /* E: false when mana is short */
bool magic_delete_champion_symbol(void);                                                   /* E */
bool magic_set_caster(int16_t champion_index);                                             /* K: true when it changed */
```

and a new ui function `void menus_draw_spell_area(void)` (beside `menus_draw_spell_area_controls`, `ui/ui.h:163`) for the four "force redraw" idioms (`command.c:240-242, 340-342`, `ui.c:213-215`, `gameloop.c:323-325`), so they do not write the caster.

- [ ] **Step 1:** Move the rules; `VI_ALTAR_REBIRTH_EFFECT` moves to `engine/sensor.h`.
- [ ] **Step 2:** Remove the fixed `engine-write` lines; `uv run pytest -q tests/test_standard.py tests/test_golden_rules.py tests/test_control.py tests/test_actions.py tests/test_ui.py tests/test_gameloop.py`. PASS.
- [ ] **Step 3:** `clang-format -i`; `git commit -am "refactor: magic and sensor rules move from ui to engine"`

---

### Task 34: Light and time rules into engine/

PRD R8. Source: `ui.md` groups F, G (order note: time++, torches, freeze_life--, ui action-area refresh, time effects, movement ticks--).

**Files:**
- Modify: `port/src/engine/inventory.c|h`, `engine/game.c|h`, `ui/gameloop.c`, `ui/view.c`, `ui/advanced.c`, `ui/startend.c:405`, tests that call `gameloop_set_light`/`view_set_light` (`tests/test_champion.py:515,546,550`, `tests/test_view.py:286-297`), `tests/standard_allowlist.txt`

**Interfaces:**
- Produces:

```c
int16_t inventory_get_party_light(void);                                    /* F */
void inventory_set_dungeon_view_palette(int16_t map_index, int16_t light);  /* F: writes g_dungeon_view_palette_index */
void inventory_decrease_torches_light_power(void);                          /* F */
void game_advance_time(void);       /* G: time++, torches, freeze_life-- */
void game_apply_tick_effects(void); /* G: time effects, movement ticks-- */
```

- [ ] **Step 1:** Move F; `view_set_light` and `gameloop_set_light` callers (also tests) call the engine functions; tests change only the function name they bind.
- [ ] **Step 2:** Move G as two functions; ui calls `game_advance_time()`, refreshes the action area, then calls `game_apply_tick_effects()` (the order stays exactly).
- [ ] **Step 3:** `tests/test_creatures.py:99` `tick()` re-implements the time step; leave it unless it now can call `game_advance_time` with the same effect.
- [ ] **Step 4:** `uv run pytest -q tests/test_standard.py tests/test_golden_rules.py tests/test_champion.py tests/test_gameloop.py tests/test_automap.py tests/test_modes.py tests/test_golden_view.py`. PASS.
- [ ] **Step 5:** `clang-format -i`; `git commit -am "refactor: light and time rules move from ui to engine"`

---

### Task 35: Game, save and action-list rules into engine/

PRD R8. Source: `ui.md` groups J, M, N, O, P, Q, the endgame hook note, "Consume flags", "Technical".

**Files:**
- Modify: `port/src/engine/game.c|h`, `engine/savegame.c|h`, `engine/action.c|h`, `engine/random.c|h`, `engine/timeline_process.c`, `engine/projectile.c|h`, `engine/champion.c|h`, `ui/gameloop.c`, `ui/startend.c`, `ui/ui_menus.c`, `ui/advanced.c`, `ui/select.c`, `ui/ui.h` (`g_action_count`, `g_action_list_minimum_skill_levels` move to `engine/action.h`), `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `void action_set_action_list(int16_t action_set_index); bool action_set_acting_champion(uint16_t champion_index);` (J); `void game_start_fuse_sequence(void);` (M); `void savegame_forget(void);` (N); `int16_t savegame_quick_save(const char *dir); int16_t savegame_quick_load(const char *dir);` that keep `g_saved_game_exists` and the save times (O); `void game_toggle_cheat(int16_t cheat);` with `CHEAT_LIGHT, CHEAT_IMMORTAL, CHEAT_FOOD` (P); `void game_reset_session(void); void game_set_mode(int16_t mode);` (Q); `bool projectile_take_fuse_request(void); uint16_t champion_take_dead_champions(void); int16_t action_take_damage(void);` (consume flags); `uint32_t random_get_seed(void); void random_set_seed(uint32_t seed);` (seed restore, `gameloop.c:371`); the spell-area drawer takes `const Champion *` (removes the swap at `gameloop.c:424-426`).

- [ ] **Step 1:** Endgame hook: the engine sets `g_game_won` and `g_saved_game_exists` before it calls `g_endgame_sensor_hook` (`timeline_process.c:271`); the hook keeps only drawing (`ui.md` Hooks note; DM clears the saved game after the wait, and no game state changes in between).
- [ ] **Step 2:** Move J, M, N, O, P, Q and the consume and technical writes.
- [ ] **Step 3:** The scripted fuse sequence (`startend.c:477-515`) calls engine functions (no writes); it stays in ui.
- [ ] **Step 4:** `tests/standard_allowlist.txt` has no `engine-write` line. `uv run pytest -q tests/test_standard.py tests/test_golden_rules.py tests/test_save.py tests/test_csb_ending.py tests/test_modes.py tests/test_ui.py tests/test_actions.py tests/test_gameloop.py tests/test_select.py`. PASS.
- [ ] **Step 5:** `docs/coding-standard.md` rule 8: name the remaining exceptions (none expected). `clang-format -i`; `git commit -am "refactor: game, save and action-list rules move from ui to engine"`

---

### Task 36: Square drawing from a table

PRD R9. Source: `ui.md` section 3 (per-function table, proposed descriptor, special cases 1-10).

**Files:**
- Modify: `port/src/ui/view.c`

**Interfaces:**
- Produces: `typedef struct { ... } SquareView;` (file-static), `static const SquareView square_views[DRAWN_SQUARE_COUNT]`, `static void draw_square(const SquareView *s, int16_t dir, int16_t x, int16_t y);` `view_draw` loops over the table.

- [ ] **Step 1:** Write the struct as in `ui.md` "Proposed descriptor" (WallPart, shape, mirror, view, view_square, wall, side_face, side/front ornament views, pit, invisible pit, pit zone, cells, side cells, stairs side up/down, stairs front, door back/front, frames[3], button, ceiling, flags).
- [ ] **Step 2:** Fill the 16 rows from the `ui.md` per-function table, back to front (the order of `view_draw` today, not the `VIEW_D*` enum order, special case 10).
- [ ] **Step 3:** Write `draw_square` with the call order of `ui.md` "draw_square call order"; the shapes `SHAPE_FULL`, `SHAPE_WALL_OR_FIELD` (D2L2, D2R2), `SHAPE_D0_SIDE` (D0L, D0R), `SHAPE_D0C` get one branch each (special case 1); flags for D2L ceiling pit in the alcove and after the door (2), absolute stairs layout ids (3), D1C side effects (6), door side as floor (7), D3 invisible pit (8).
- [ ] **Step 4:** Remove `d3l2` … `d0c` and the function-pointer table of `view_draw`.
- [ ] **Step 5:** `make -C port`; `uv run pytest -q tests/test_golden_view.py tests/test_view.py tests/test_control.py tests/test_modes.py tests/test_csb_engine.py`. PASS. The golden view test must pass unchanged; a difference is a bug of this task.
- [ ] **Step 6:** `clang-format -i`; `git commit -am "refactor: draw the dungeon view squares from one table"`

---

### Task 37: Finish

PRD acceptance.

**Files:**
- Modify: `tests/standard_allowlist.txt`, `docs/coding-standard.md`, `README.md`, `docs/superpowers/2026-10-15-clean-code.md` (Status)

- [ ] **Step 1:** `grep -c 'open: module cleanup' tests/standard_allowlist.txt` prints `0` (also no `long` line from Task 3); each remaining line has a lasting reason.
- [ ] **Step 2:** The PRD scenario greps: `grep -rnE 'SLOT_ACTION_HAND|CELLS_CENTERED|KAOS_CREATURE_INFO 0x|event7[0-9]_|H_BYTE_12A|d3l\(|NEXT\(\)|read_file\(' port/src` prints nothing; `test_kaos_image_only_in_data` passes with no allowlist line (code only; comments may name a `kaos_` function).
- [ ] **Step 3:** `make -C port` and `make -C port windows` with 0 warnings; `grep -c -- '-Wno-' port/Makefile` prints `0`.
- [ ] **Step 4:** `docs/coding-standard.md` lists every check of `tests/test_standard.py`; `README.md` names clang-format and the blame config.
- [ ] **Step 5:** `git commit -am "docs: the coding standard after the clean-code pass"`
