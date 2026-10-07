# Coding standard refactor: plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

Approved by the user on 2026-10-06 (design in chat, no PRD). Branch: `feature/2026-10-06-coding-standard-refactor`.

**Goal:** Make `port/src/` follow `docs/coding-standard.md`, with an automated test that keeps it that way.

**Architecture:** Four source layers in subfolders: `data/` ⊂ `engine/` ⊂ `ui/` ⊂ `platform/`. A layer includes only its own headers and the headers of lower layers. The game rules (`engine/`) tell the user interface what to redraw through flags. They do not call drawing functions. `tests/test_standard.py` checks the rules of the standard on each run. `tests/standard_allowlist.txt` holds the open violations. Each task removes its entries, and at the end only permanent exceptions stay.

**Tech stack:** C11, clang, SDL3, Make. Python 3.14, uv, pytest, ctypes.

**Spec:** `docs/coding-standard.md`. Decisions from the chat on 2026-10-06:

- 16-bit types: game values keep `int16_t`/`uint16_t`. One value has one type on its whole path, so a cast is needed only where it changes the result. `int` only for values that are not game values.
- Predicates return `bool`.
- Layers: move the files to folders and remove all engine calls into the user interface in this plan.
- Function prefixes: each header states its prefixes. A prefix belongs to one header only. Rename only collisions and names with event numbers.
- Run the full test suite after the refactor. In this plan: after each task.

## Global constraints

- Baseline: `uv run pytest` gives `577 passed` on 2026-10-06. Each task ends with the full suite green: `uv run pytest -q`. The new tests add to this count.
- No change of game behavior. If a task needs a behavior change to pass, stop and report.
- Keep the binary layout of every struct that is in a save file or that a test declares as a `ctypes.Structure` (`Champion`, `TimelineEvent`, `Bitmap`, and others). Their fields keep their types.
- Keep existing comments (CLAUDE.md section 4). New comments are one line and describe what the code does.
- Module prefixes and names follow rule 7 of the standard. New files: `port/src/data/bytes.h`, `port/src/engine/explored.c`, `port/src/engine/explored.h`, `tests/test_standard.py`, `tests/standard_allowlist.txt`. No other new files unless a task names them.
- Test failure handling (CLAUDE.md section 10): use superpowers:systematic-debugging. Hard stop after 3 failed attempts in one task. When a change breaks existing tests, revert to the last commit of the task before a new approach.
- Commits: Conventional Commits. Author email `55005408+peterkmet@users.noreply.github.com`. No co-author. No push.
- After each task, the `reviewer` agent checks the task. The next task starts only after the user confirms.

## Review focus

The tests do not cover these risks well. The reviewer checks them on each task:

1. A changed signedness (`int16_t` ↔ `uint16_t`) on a value: a comparison, a division, or a right shift of a negative or large value gives another result. Expected: the same value as before.
2. A redraw that moves from an immediate call to the end of the tick: something that the player sees in the same frame changes order, for example a status box after a death.
3. Inventory close on a champion death that moves from immediate to deferred: rule code later in the same tick reads `g_inventory_champion_ordinal`.
4. A test file that compiles its own library from `port/src/*.c` and misses the new subfolders or include paths.
5. A renamed function that a `ctypes` test binds by name, or `state_json.c` and `control.c` that print a name into the LLM protocol.

## File map

The target folder of each existing file:

| Folder | Files |
|---|---|
| `port/src/data/` | `adventure.c/.h`, `csbimage.c/.h`, `csbtext.c/.h`, `ftl.c/.h`, `graphics.c/.h`, `ids.h`, `spells.c/.h`, `tables.c/.h`, `bytes.h` (new) |
| `port/src/engine/` | `action.c/.h`, `champion.c/.h`, `dungeon.c/.h`, `game.c/.h`, `group.c/.h`, `inventory.c/.h`, `magic.c/.h`, `map.c/.h`, `message.c/.h`, `move.c/.h`, `projectile.c/.h`, `random.c/.h`, `recruit.c/.h`, `savefile.c/.h`, `savegame.c/.h`, `sensor.c/.h`, `sound.c/.h`, `things.h`, `timeline_process.c/.h`, `explored.c/.h` (new) |
| `port/src/ui/` | `advanced.c/.h`, `anim.c/.h`, `automap.c/.h`, `command.c/.h`, `dialog.c/.h`, `draw.c/.h`, `gameloop.c/.h`, `layout.c/.h`, `logo.c/.h`, `music.c/.h`, `musicmap.c`, `dr_mp3.h`, `select.c/.h`, `startend.c/.h`, `text.c/.h`, `ui.c/.h`, `ui_champion.c`, `ui_menus.c`, `video.c/.h`, `view.c/.h` |
| `port/src/platform/` | `main.c`, `platform.c/.h`, `scale.c/.h`, `control.c/.h`, `state_json.c/.h` |

`ui/` is the presentation layer: picture, sound output, music, and input. `platform/` holds SDL, the program entry, and the control socket.

---

### Task 1: Door bit 0x20 has two accessors

`port/src/map.h:129` `door_checks_creature_height` and `port/src/map.h:133` `door_is_vertical` both read `REC(thing)[3] & 0x20`.

Hypothesis: this is one door field with two names. The original "Vertical" bit (bit 5 of the door word) selects a door that slides down. Only a door that slides down hits creatures by their height. Then both readings are correct, and the fix is one accessor.

What refutes the hypothesis: the original door animation event (KAOS) tests another bit or a door info attribute for the creature height.

**Files:**
- Modify: `port/src/map.h:129-133`, `port/src/group.c:806`, `port/src/timeline_process.c:87`
- Test: `tests/test_dungeon.py`

- [ ] **Step 1: Find the original code.** Find the door animation event and the creature door damage in `docs/kaos-symbols.csv` (search `door`). Read these functions in `decomp/KAOS.c`. Note the mask that each one tests for the creature height.
- [ ] **Step 2: Decide.**
  - If the mask is bit 5 of the door record (byte 3, `0x20`): the hypothesis holds. Go to Step 3.
  - Else: stop. Report the mask and the function addresses to the user. Do not change code.
- [x] **Step 3: Write the test.** Add to `tests/test_dungeon.py` a test that reads each door thing of `Dungeon.DAT` through `get_thing_data` and asserts that byte 3 & `0x20` equals the `Vertical` field of the `dmcsb` parser. `door_is_vertical` is `static inline`, so `ctypes` cannot call it; the test checks the bit that it reads.
- [ ] **Step 4: Run it.** `uv run pytest tests/test_dungeon.py -q`. Expected: PASS (the accessor exists).
- [ ] **Step 5: Remove the duplicate.** Delete `door_checks_creature_height` from `map.h`. In `group.c:806` and `timeline_process.c:87`, call `door_is_vertical`. Add a one-line comment on `door_is_vertical`: `/* A door that slides down; it hits a creature by its height. */`
- [ ] **Step 6: Run the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 7: Commit.** `git commit -am "refactor: one accessor for the vertical door bit"`

---

### Task 2: Standard test and allowlist

**Files:**
- Create: `tests/test_standard.py`, `tests/standard_allowlist.txt`
- Modify: `docs/coding-standard.md` (rules 7 and 8)

**Interfaces:**
- Produces: allowlist line format `<check> <target> <reason>`. Checks and targets:

| Check | Target | Rule |
|---|---|---|
| `hex` | file path relative to `port/src/` | 1 |
| `decimal` | file path | 1 |
| `duplicate` | constant name | 1 |
| `record` | file path | 2 |
| `decompiler` | file path | 3 |
| `bool` | header path | 3 |
| `long` | `path:function` | 3 |
| `prefix` | header path | 7 |
| `numbered` | function name | 7 |
| `layer` | `path:header` | 8 (added in Task 4) |

- [ ] **Step 1: Update rules 7 and 8 in `docs/coding-standard.md`.** Replace the first bullet of rule 7 with:

  ```markdown
     - Functions: `<prefix><verb>_<object>` in `snake_case`. Each header states its prefixes in one comment line, for example `/* Prefixes: gfx_ */` in `graphics.h`. A prefix belongs to one header only, and no prefix starts with the prefix of another header.
  ```

  Replace rule 8 with:

  ```markdown
  8. The source has four layers in subfolders of `port/src/`: `data/`, `engine/`, `ui/`, and `platform/`. A file includes only headers of its own layer and of the layers before it in this list. Game rules in `engine/` do not call the user interface: they set redraw flags. Only `platform/` includes SDL.
  ```

- [ ] **Step 2: Write `tests/test_standard.py`.**

  ```python
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
  ```

- [ ] **Step 3: Write an empty allowlist and run the test.** Create `tests/standard_allowlist.txt` with this content:

  ```text
  # Open exceptions of tests/test_standard.py. One per line: <check> <target> <reason>.
  # Targets are paths relative to port/src/, constant names, or function names.
  ```

  Run `uv run pytest tests/test_standard.py -q`. Expected: most of the 9 tests FAIL. Each failure line starts with an allowlist key.

  Fixes to the Step 2 code, made during the step (the code above includes them):
  - `record`: rule 2 lets `map.c` and `map.h` index raw record bytes. The first version reported 19 accessor lines of `map.c`. `line_check` takes `owners`, the file names that a check skips.
  - `declarations`: the first version read the function pointer variable `extern void (*g_frame_hook)(void)` in `startend.h` as a function named `void`. It skips `extern` lines now. No header declares a function with `extern`.

- [ ] **Step 4: Fill the allowlist.** For each distinct key in the output, add one line `<key> <reason>`. Use these reasons:
  - `hex map.c`, `hex map.h`: `record layout accessors`.
  - `hex csbimage.c`, `hex csbtext.c`: `addresses of the APPB tables`.
  - Every other key: `open: Task N`. N is the task that fixes the file or name: 3 for `duplicate` and `record`; 7 for `prefix` and `numbered`; for `hex`, `decimal`, `decompiler`, `bool`, and `long`, the cleanup task (8, 9, 10, or 11) that lists the file.

  Keep the paths flat (for example `view.c`). Task 4 changes them.

- [ ] **Step 5: Run the test.** `uv run pytest tests/test_standard.py -q`. Expected: 9 passed.
- [ ] **Step 6: Check the test catches a new violation.** Add `int x = 0x1234;` to the end of `port/src/random.c`. Run the test. Expected: `test_no_hex_literals` FAILS with key `hex random.c`. Remove the line. Run again. Expected: PASS.
- [ ] **Step 7: Run the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 8: Commit.** `git add tests/test_standard.py tests/standard_allowlist.txt docs/coding-standard.md && git commit -m "test: check the C port against the coding standard"`

---

### Task 3: Duplicate constants, byte helpers, raw map byte

**Files:**
- Create: `port/src/bytes.h` (Task 4 moves it to `data/`)
- Modify: `port/src/timeline_process.h`, `port/src/group.h`, `port/src/projectile.h`, `port/src/champion.c`, `port/src/inventory.c`, `port/src/map.h`, `port/src/map.c`, `port/src/savegame.c`, `port/src/control.c`, and every file that the `duplicate` failures name
- Test: `tests/test_standard.py`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `bytes.h` with `BE16(p)`, `WR16(p, v)`, `BE32(p)`, `WR32(p, v)`. It has only macros, so it needs no `Prefixes` line. `timeline_process.h` owns every `EVENT_*` timeline event type. `uint16_t map_difficulty(void)` in `map.h`.

- [ ] **Step 1: Remove the `duplicate` and `record` lines from the allowlist.** Run `uv run pytest tests/test_standard.py -q`. Expected: `test_each_constant_has_one_definition` and `test_raw_record_access_only_in_accessors` FAIL.
- [ ] **Step 2: One header for the event types.** Move the `EVENT_*` timeline event values from `group.h:40-43` and `projectile.h:20` into the enum in `timeline_process.h`. `group.h` and `projectile.h` include `timeline_process.h`. Delete `champion.c:24`. In `inventory.c`, delete `EVENT_CHAMPION_SHIELD` and use `EVENT_CHAMPION_SHIELD_END`. `EVENT_BEHAVIOR_TO_ASPECT` in `group.h` is an offset, not an event type: it stays.
- [ ] **Step 3: Use `EVENT_MAP_TIME`.** Replace `(uint32_t)g_party_map_index << 24 | (...)` at `champion.c:661` and `champion.c:740` with `EVENT_MAP_TIME(g_party_map_index, ...)`. Check the other `<< 24 |` lines in `csbtext.c`, `csbimage.c`, `ftl.c`, `inventory.c`. Change only the lines that build an event map time.
- [ ] **Step 4: One header for the byte helpers.** Create `port/src/bytes.h`:

  ```c
  #ifndef DM_BYTES_H
  #define DM_BYTES_H

  #include <stdint.h>

  /* Big-endian access to the Amiga data: game files, save files, module images. */
  #define BE16(p) ((uint16_t)((p)[0] << 8 | (p)[1]))
  #define WR16(p, v) ((p)[0] = (uint8_t)((uint16_t)(v) >> 8), (p)[1] = (uint8_t)(v))
  #define BE32(p) ((uint32_t)BE16(p) << 16 | BE16((p) + 2))
  #define WR32(p, v) (WR16(p, (uint32_t)(v) >> 16), WR16((p) + 2, v))

  #endif
  ```

  Delete `BE16` and `WR16` from `map.h:9-10`, and include `bytes.h` there. Replace `read32`/`write32` (`champion.c:68-69`), `rd32` (`savegame.c:56`), and `be32` (`control.c:571`) with `BE32`/`WR32`. Check every other `#define` that the `duplicate` failures name the same way: keep one definition in its owner header.
- [ ] **Step 5: Map difficulty accessor.** Add to `map.h`, near the other map accessors, a function `uint16_t map_difficulty(void)` that returns the difficulty of the current map, 0 when no map is loaded. Its body is the expression from `champion.c:456`, with a named constant for the byte offset 12 and the shift 4. Use it in `champion.c:456`.
- [ ] **Step 6: Run the standard test.** Expected: PASS.
- [ ] **Step 7: Run the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 8: Commit.** `git commit -am "refactor: one definition for each constant and byte helper"` (add `port/src/bytes.h`).

Decisions made during the task:
- Byte helpers: the local `rd16`/`wr16`/`rd32`/`wr32` of `savegame.c` and `rd16`/`wr16` of `savefile.c` are copies of the `bytes.h` macros too (rule 9). They are replaced. `WR16` reads its value twice. The first version passed `save_block_xor(...)` and `random16()` to `WR16` in `savefile.c`: the save file became damaged (`test_mcp_tools` failed). These values go into a local variable first now.
- `map_difficulty(int16_t map)` already exists in `things.h` for a map index. The new accessor is `uint16_t map_current_difficulty(void)` in `map.h`. It reads `g_current_map`, because `map_reset` leaves `g_current_map` set. It replaces `current_map_difficulty` of `group.h` (same expression, now with the null check) and the expression in `champion.c`.
- `spells.h` and `tables.h` both defined the type `Spell`, so no file could include both. The `spells.h` type is now `SpellRecord`. `spells.h` owns `DM_SPELL_COUNT`, `SPELL_SYMBOL_FIRST`, and `SPELL_KIND_*` (with `SPELL_KIND_MAGIC_MAP` from `magic.c`). `SYMBOL_FIRST` of `command.c`, `state_json.c`, and `ui_menus.c` is `SPELL_SYMBOL_FIRST`. `magic.c` includes `spells.h` instead of its own declarations.
- Sound numbers: the enums of `map.h` and `group.h` and `SOUND_SWALLOW` moved to `sound.h`. `SOUND_PARTY_DAMAGED 7` of `champion.c` (pit fall) is `SOUND_SCREAM` (7); `map.h` gives `SOUND_PARTY_DAMAGED` the value 15.
- Renames, same name with another meaning: `ICON_VORPAL_BLADE = 43` of `group.c` is `ICON_HARDCLEAVE` (ReDMCSB `C043_ICON_WEAPON_HARDCLEAVE_EXECUTIONER`; icon 40 is the Vorpal Blade of `ids.h`). `SAMPLE_PERIOD 650` of `anim.c` is `ANIMATION_SAMPLE_PERIOD` (`logo.c` keeps the KAOS address). `FRAMES` of `map.c` is `BACKTRACE_FRAMES`. `TEXT_SIZE 512` of `state_json.c` is `STATE_TEXT_SIZE`.
- `SKILL_LEVEL_STRINGS`: `startend.c` had `0x101e70` + 2 * level, `ui_champion.c` `0x101e74` + 2 * (level - 2). Same address; `ui.h` keeps `0x101e74`, and `startend.c` uses `2 * (level - 2)`.
- `SCENT_COUNT` of `move.c` and `savegame.c` is `PARTY_SCENT_COUNT` of `game.h`. `TEXT_SIZE` of `control.c` was not used: deleted.
- Owner headers: `tables.h` (`ACTION_LIST_COUNT`, `ACTION_SET_EMPTY_HAND`, `ACTION_LEVEL_NEEDS_CHARGES`, `DOOR_INFO`, `SPELL_BYTES`, `ACTION_SET_COUNT`), `champion.h` (`CHAMPION_SYMBOL_COUNT`, `CHAMPION_COLORS`, `FOOD_WATER_MINIMUM`, `ACTION_NONE`), `ui.h` (`BASE_SKILL_STRINGS`, `SKILL_LEVEL_STRINGS`, `SPELL_TAB_STEP`, `INPUT_*`, `MOUSE_ENTRY_BYTES` = 2 * `INPUT_WORDS`), `view.h` (`DUNGEON_PALETTE`, `VIEWPORT_ORIGIN`, `VIEWPORT_SIZE`, `PORTRAIT_SIZE`, `VIEW_DEPTH_COUNT`), `layout.h` (`LAYOUT_BITMAPS`), `graphics.h` (`BITPLANE_COUNT`, `PIXELS_PER_WORD`), `video.h` (`COLOR_INDEX_MASK`, `FRAMES_PER_SECOND`), `adventure.h` (`PATH_SIZE`), `savefile.h` (the block 0 `G_*` offsets), `dungeon.h` (`TEXT_MODE_*`, `TEXT_SIZE`, `SCROLL_TEXT_SIZE`), `map.h` (`THING_INDEX`, `TELEPORTER_VISIBLE`, `THING_NONE`), `recruit.h` (`PORTRAITS_PER_ROW`), `ids.h` (`ICON_EMPTY_FLASK`). Each owner is in the lowest layer of the file map that uses the constant.

---

### Task 4: Source folders and the layer check

**Files:**
- Move: every file of `port/src/` to the folder in the [file map](#file-map), with `git mv`.
- Modify: `port/Makefile`, `tests/conftest.py`, the 11 test files that compile from `port/src/*.c` (`test_csb_adventure.py`, `test_control.py`, `test_csb_ending.py`, `test_csb_rules.py`, `test_csb_engine.py`, `test_csb_save.py`, `test_csb_magic_map.py`, `test_csb_text.py`, `test_entrance.py`, `test_modes.py`, `test_select.py`), `tests/test_tables.py`, `tests/test_standard.py`, `tests/standard_allowlist.txt`, `README.md`, `docs/coding-standard.md`, and the `docs/*.md` files that name `port/src/` paths (`grep -rl 'port/src/' docs README.md`). Do not change the dated plans in `docs/superpowers/`: they record history.

**Interfaces:**
- Produces: `tests/conftest.py` exports `PORT_SRC`, `PORT_INCLUDES` (list of `-I` flags), and `game_sources()` (list of `.c` paths of the library, without `platform/`).
- Include lines stay `#include "name.h"`. The `-I` flags find them.

- [ ] **Step 1: Add the layer test.** Append to `tests/test_standard.py`:

  ```python
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
  ```

  Run it. Expected: FAIL with `is not in a layer folder`.
- [ ] **Step 2: Move the files.** Create the four folders. `git mv` each file to its folder from the file map. Move `port/src/bytes.h` to `port/src/data/`.
- [ ] **Step 3: Update `port/Makefile`.** Replace the source lines:

  ```make
  LAYERS = data engine ui platform
  INCLUDES = $(addprefix -Isrc/,$(LAYERS))
  GAME = $(filter-out src/platform/main.c src/platform/platform.c,$(wildcard $(addsuffix /*.c,$(addprefix src/,$(LAYERS)))))
  SRC = $(wildcard $(addsuffix /*.[ch],$(addprefix src/,$(LAYERS))))
  ```

  Add `$(INCLUDES)` to both compile commands. Replace `src/main.c src/platform.c` with `src/platform/main.c src/platform/platform.c`. Run `make -C port`. Expected: builds `build/dm` and `build/libdm.dylib`. `libdm` also compiles `platform/control.c`, `platform/scale.c`, and `platform/state_json.c`, as it compiles them today.
- [ ] **Step 4: One source list for the tests.** Add to `tests/conftest.py`:

  ```python
  from pathlib import Path

  PORT_SRC = Path(__file__).resolve().parent.parent / "port" / "src"
  PORT_INCLUDES = [f"-I{PORT_SRC / layer}" for layer in ("data", "engine", "ui", "platform")]


  def game_sources():
      """The .c files of libdm: all layers without the SDL entry point and window."""
      return [str(p) for p in sorted(PORT_SRC.rglob("*.c")) if p.name not in ("main.c", "platform.c")]
  ```

  In each of the 11 test files, replace the `sources = [...]` line with `sources = game_sources()`, import it with `from conftest import PORT_INCLUDES, game_sources`, and add `*PORT_INCLUDES` to the `clang` command. In `tests/test_tables.py:65`, find each file with `next(PORT_SRC.rglob(f))` and add `*PORT_INCLUDES`.
- [ ] **Step 5: Update the paths.** In `tests/standard_allowlist.txt`, prefix each path with its folder (for example `view.c` → `ui/view.c`). In `README.md`, `docs/coding-standard.md`, and the `docs/*.md` files, change each `port/src/<file>` to `port/src/<layer>/<file>`.
- [ ] **Step 6: Run the layer test.** Expected: FAIL with a list of `layer` keys. Add each key to the allowlist with the reason `open: Task 5` or `open: Task 6`, as the file appears in Task 5 or Task 6. Run again. Expected: PASS.
- [ ] **Step 7: Run the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 8: Commit.** `git add -A port tests README.md docs && git commit -m "refactor: split the C port into data, engine, ui, and platform folders"`

Decisions made during the task:
- `port/src/` had no file outside the file map.
- `README.md` names no `port/src/` path. `CHANGELOG.md` records history and keeps the flat paths.
- The test imports go in the local import group, in alphabetical order: `from conftest import ...` comes first.
- The layer test found 17 keys: 2 for Task 5 and 15 for Task 6. `engine/dungeon.c` and `engine/savefile.c` have no `layer` key. A key goes to the task that removes the last use of the header. `action.c`, `champion.c`, and `magic.c` call `ui_string` (Task 6 Step 2), so their `ui.h` keys go to Task 6, although Task 5 removes the drawing calls of these files. The uses that Task 6 does not name are in the Task 6 notes.

---

### Task 5: Engine sets redraw flags instead of drawing

The game rules call drawing functions directly today. The tick already ends with `champion_draw_all_states(0)` (`gameloop.c:243`), which draws every champion with redraw flags set. The engine only needs to set the flags.

**Files:**
- Modify: `engine/champion.h`, `engine/champion.c`, `engine/group.c`, `engine/magic.c`, `engine/timeline_process.c`, `engine/action.c`, `engine/action.h`, `engine/game.h`, `engine/game.c`, `ui/ui.h`, `ui/ui.c`, `ui/ui_menus.c`, `ui/ui_champion.c`, `ui/command.c`, `ui/gameloop.c`
- Test: `tests/test_actions.py`, `tests/test_champion.py`, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces in `engine/champion.h`: the `CHAMPION_ATTR_*` enum (moved from `ui/ui.h:25-37`).
- Produces in `engine/game.h`: `extern uint16_t g_redraw;` and `enum { REDRAW_ACTION_AREA = 1, REDRAW_SPELL_AREA = 2 };` (bit flags).
- Produces in `engine/action.h`: `g_acting_champion_ordinal`, `g_action_list_indices`, `g_action_damage` (moved from `ui/ui.h:69-74`, with their definitions), and `void action_clear_acting_champion(void)`, which resets the acting champion state and sets `REDRAW_ACTION_AREA`.
- `ui/gameloop.c` draws the action area when `REDRAW_ACTION_AREA` is set, and the spell area when `REDRAW_SPELL_AREA` is set, then clears the flag. This happens after the tick, next to `gameloop.c:243`.

The calls to replace:

| File | Call today | Replacement |
|---|---|---|
| `champion.c`, `group.c`, `magic.c`, `action.c`, `timeline_process.c` | `champion_draw_state(i)` | none: the caller already sets the `CHAMPION_ATTR_*` flags. If a caller does not set a flag, set the flag that the drawing needs. |
| `magic.c` | `champion_draw_all_states(...)` | set the same flags on each champion |
| `timeline_process.c:402` | `draw_status_box_portrait(i)` | set `CHAMPION_ATTR_STATUS_BOX` on the champion |
| `champion.c:696`, `action.c:475` | `menus_clear_acting_champion()` | `action_clear_acting_champion()` |
| `champion.c:725-727` | `menus_set_magic_caster(alive)`, `menus_draw_spell_area_controls(...)` | set `g_magic_caster_champion_index`, set `REDRAW_SPELL_AREA` |
| `magic.c:364-365` | `menus_draw_available_symbols(0)`, `menus_draw_champion_symbols(...)` | set `REDRAW_SPELL_AREA` |

- [ ] **Step 1: Remove the Task 5 lines from the allowlist.** Run the layer test. Expected: FAIL with the `layer` keys of the engine files in the table.
- [ ] **Step 2: Write the failing test.** In `tests/test_actions.py`, add a test: after a champion action that clears the acting champion, `g_redraw & REDRAW_ACTION_AREA` is nonzero. Bind `g_redraw` with `ctypes.c_uint16.in_dll(lib, "g_redraw")`. Run it. Expected: FAIL (`g_redraw` not found).
- [ ] **Step 3: Move the flags and the action state.** Move the `CHAMPION_ATTR_*` enum to `engine/champion.h` and the action menu state to `engine/action.h` and `engine/action.c`. Add `g_redraw` to `engine/game.h` and its definition to `engine/game.c`. Reset `g_redraw` in `game_new`.
- [ ] **Step 4: Replace the calls.** Apply the table. In `ui/ui_menus.c`, `menus_clear_acting_champion` calls `action_clear_acting_champion` and draws as today. In `ui/gameloop.c`, add the flag handling after `champion_draw_all_states(0)`. When `menus_set_magic_caster` did more than set `g_magic_caster_champion_index`, move the state part to `engine/magic.c` and keep the drawing part in `ui/ui_menus.c`.
- [ ] **Step 5: Run the new test and the layer test.** Expected: PASS.
- [ ] **Step 6: Run the full suite.** `uv run pytest -q`. Expected: all pass. A failure in a screen test (`test_ui.py`, `test_panels.py`, `test_view.py`) means a redraw moved in time: check Review focus item 2 before a fix.
- [ ] **Step 7: Commit.** `git commit -am "refactor: the game rules set redraw flags instead of drawing"`

Decisions made during the task:
- `REDRAW_ACTION_AREA` replaces `g_refresh_action_area`. `menus_refresh_action_area` reads and clears the bit, so the action area is drawn at the same time in the tick as before (in `advance_game_time`, not after `champion_draw_all_states(0)`). `menus_reset` clears `g_redraw`.
- The spell area needs three bits to keep the picture: `REDRAW_SPELL_AREA` (the caster died: the whole area for the new caster), `REDRAW_SPELL_TABS` (another champion died: only the caster tabs), and `REDRAW_SPELL_SYMBOLS` (a cast: only the runes). A full redraw after a cast would erase the highlight of the cast box, and a full redraw over the frozen or sleeping pattern would differ from the original. `menus_refresh_spell_area` in `ui/ui_menus.c` draws the bits. `refresh_interface` in `ui/gameloop.c` calls it after `champion_draw_all_states(0)`.
- `menus_set_magic_caster` only sets `g_magic_caster_champion_index`. Its drawing part is now `draw_spell_area` in `ui/ui_menus.c`. No state part moves to `engine/magic.c`.
- A dead party skips `refresh_interface`. `gameloop_tick` calls `champion_draw_all_states(0)` before `endgame(1)`, so that the dead status box shows during the death scream as before.
- `magic.c` sets the flags on all champions with the static `set_attributes_of_all`.
- `menus_clear_acting_champion` calls `action_clear_acting_champion` first and then draws the action hand. The action hand is drawn without the acting border. `refresh_interface` draws the hand slots without the border at the end of the same frame, as before.
- `inventory.c` keeps its `champion_draw_state` call for Task 6 Step 10.
- Known difference: a champion state that a timeline event changes in the same tick as the endgame sensor shows after the sensor delay. Only the DM sensor path waits on the game screen, and no DM dungeon has an endgame sensor. In CSB the ending animation covers the screen, and the select screen clears the frame after the tick: a frame hash comparison against 30ab027 gives the same presented frames.
- Fix after the review: the control socket ran the command queue without `refresh_interface`, so the runes of a cast showed only after `command_highlight_disable` of the tick, black instead of inverted. `gameloop_run_commands` in `ui/gameloop.c` runs the queue and then `refresh_interface`, as `gameloop_frame` does. `platform/control.c` calls it. `test_a_cast_leaves_the_cleared_runes_inverted_as_with_the_mouse` in `tests/test_control.py` checks the cast box. A frame hash comparison against 30ab027 (23 socket actions: moves, runes, cast, action, inventory, sleep) gives the same frames.
- Fix after the review: before Task 5, `champion_kill` drew the spell area at each death that left a living champion. When the whole party died in one tick, the death scream showed the spell area of the death before the last one: the last champion as the caster, with only its own tab and with its runes. `champion_kill` resets the runes of the last champion, and no draw follows the last death, so the flags alone cannot give this picture. `gameloop_tick` keeps a copy of the champions before the damage step. For a dead party, the static `draw_spell_area_before_last_death` puts back the copy of the caster, calls `menus_refresh_spell_area`, and restores the dead caster. `test_a_party_that_dies_in_one_tick_shows_the_spell_area_of_the_last_caster` in `tests/test_gameloop.py` checks it. A frame hash comparison against 30ab027 (1 to 4 champions, each caster, each set of deaths in one tick: 98 cases, 10 of them a dead party) gives the same frames.

---

### Task 6: Remaining engine and data dependencies

**Files:**
- Create: `engine/explored.c`, `engine/explored.h`
- Modify: `ui/ui.h`, `ui/ui.c`, `data/graphics.h`, `data/graphics.c`, every caller of `ui_string`, `engine/message.c`, `engine/message.h`, `ui/ui.c` (message drawing), `engine/sound.c`, `engine/sound.h`, `platform/platform.c`, `ui/anim.c`, `ui/logo.c`, `ui/advanced.h`, `ui/advanced.c`, `engine/game.h`, `engine/game.c`, `ui/automap.c`, `ui/automap.h`, `engine/savegame.c`, `engine/champion.h`, `engine/timeline_process.c`, `ui/startend.c`, `ui/startend.h`, `ui/gameloop.c`, `engine/champion.c`, `ui/command.c`, `ui/command.h`, `engine/inventory.c`, `data/csbtext.c`, `engine/dungeon.c`, `engine/savefile.c`
- Test: `tests/test_standard.py` (layer), the test file of each changed module, `tests/standard_allowlist.txt`

**Interfaces:**
- Produces: `const char *gfx_string(int16_t index)` in `data/graphics.h` (was `ui_string` in `ui/ui.h:91`).
- Produces: `engine/explored.h` with `explored_reset`, `explored_mark`, `explored_is_seen`, `explored_save`, `explored_load`. These are the state functions of `automap.c` (`automap_reset`, `automap_is_seen`, `automap_save`, `automap_load`, and the function that marks a square). Same parameters and return types. Prefix line `/* Prefixes: explored_ */`.
- Produces in `engine/game.h`: `g_game_mode` and its `GAME_MODE_*` values (from `ui/advanced.h`), and the cheat flags `g_cheat_food`, `g_cheat_immortal`, and the light cheat flag.
- Produces in `engine/champion.h`: `g_champion_portraits` (from `ui/ui.h:79`).
- Produces in `engine/game.h`: `extern uint16_t g_endgame_delay;` with `enum { ENDGAME_NONE = 0xffff };`. The endgame sensor sets it. `ui/startend.c` reads it after each tick and calls `endgame_sensor(delay)`.
- Produces in `engine/champion.h`: `extern uint16_t g_dead_champions;`, one bit per champion index. `champion.c` sets the bit where it calls `command_release_dead_champion` today. `ui/gameloop.c` calls `command_release_dead_champion` for each bit after the tick and clears the bits.

The changes, one at a time, each with a full suite run:

- [ ] **Step 1: Remove the Task 6 lines from the allowlist.** Run the layer test. Expected: FAIL.
- [ ] **Step 2: `ui_string` → `gfx_string`.** Move the function to `data/graphics.c`. Rename all callers (`grep -rn ui_string port/src tests`). Run the full suite.
- [ ] **Step 3: Message state and drawing.** `message_draw` and every other function of `engine/message.c` that uses `text.h` or `video.h` move to `ui/ui.c` (prefix `ui_`). `engine/message.c` keeps the rows and the print functions. Run the full suite.
- [ ] **Step 4: Sound and music.** Delete `music_mix` from `sound_mix` (`sound.c:260`). In `plat_audio_pump` (`platform/platform.c:208`), call `music_mix(buf, FRAMES)` after `sound_mix`. Delete `music_stop()` from `sound_stop_all` (`sound.c:197`), and add `music_stop();` after `sound_stop_all();` in `ui/anim.c:82` and `ui/logo.c:60`. If `grep -rn sound_stop_all port/src/engine` finds a caller, stop and report. Run the full suite.
- [ ] **Step 5: Game mode and cheats.** Move `g_game_mode`, the `GAME_MODE_*` values, and the cheat flags to `engine/game.h` and `engine/game.c`. Run the full suite.
- [ ] **Step 6: Explored squares.** Create `engine/explored.c` and `engine/explored.h` with the state part of `ui/automap.c`. `ui/automap.c` keeps the drawing and calls `explored_*`. `engine/savegame.c` calls `explored_save`, `explored_load`, and `explored_reset`. Rename the `ctypes` bindings in the tests (`grep -rn automap_ tests`). Run the full suite.
- [ ] **Step 7: Portraits.** Move `g_champion_portraits` to `engine/champion.h` and its definition to `engine/champion.c`. Run the full suite.
- [ ] **Step 8: Endgame sensor.** Write a test in `tests/test_game.py`: trigger the endgame sensor through its timeline event, then assert `g_endgame_delay` equals the sensor delay. Run it: FAIL. Replace `endgame_sensor(...)` at `timeline_process.c:274` with `g_endgame_delay = ...`. In `ui/startend.c`, after each tick, call `endgame_sensor(g_endgame_delay)` when it is not `ENDGAME_NONE`, then reset it. `game_new` resets it. Run the test, then the full suite.
- [ ] **Step 9: Dead champion release.** Write a test in `tests/test_champion.py`: kill a champion, then assert its bit in `g_dead_champions` is set. Run it: FAIL. Replace `command_release_dead_champion(i)` at `champion.c:699` with `g_dead_champions |= 1 << i`. In `ui/gameloop.c`, release the champions after the tick. Run the test, then the full suite. Check Review focus item 3.
- [ ] **Step 10: Inventory drawing.** The mouth blink at `inventory.c:449-451` and the panel check at `inventory.c:456` are drawing. Move the blink loop into the click handler in `ui/command.c` that calls the eat or drink function, before the call, so the order on the screen stays. Replace the `g_panel_content` check with `c->attributes |= CHAMPION_ATTR_PANEL`: `champion_draw_state` draws the panel only when it is the food and water panel. If this changes a screen test, keep the original check in `ui/` instead. Run the full suite.
- [ ] **Step 11: Data and engine includes.** Fix the remaining `layer` keys of `data/csbtext.c` (`champion.h`), `engine/dungeon.c`, and `engine/savefile.c`. Move each used constant to its owner in the lower layer. If a file uses functions of the higher layer, move the file to that layer and update the file map in this plan. Run the full suite.
Uses from the Task 4 layer keys that the steps above do not name:
- Step 2: after the rename, delete the `ui.h` include from `engine/action.c` and `engine/magic.c` when they use nothing else of it.
- Step 3: `engine/message.h` uses `TEXT_CHAR_WIDTH` of `text.h` for `MESSAGE_COLS`.
- Step 6: `engine/savegame.c` uses `AUTOMAP_SAVE_MAGIC` of `automap.h`.
- Step 7: `engine/savegame.c` uses `CHAMPION_PORTRAIT_WIDTH` and `CHAMPION_PORTRAIT_HEIGHT` of `ui.h`. `engine/champion.c:229` calls `champion_set_portrait` of `ui/ui_champion.c`.
- Step 10: `engine/inventory.c` includes `startend.h` for `wait_frames` in the mouth blink.
- Step 11: `data/csbtext.c` uses `SKILL_BASE_COUNT` and `SKILL_FIGHTER` to `SKILL_WIZARD` of `engine/champion.h`.

- [ ] **Step 12: Check the allowlist.** It has no `layer` line. Run `uv run pytest tests/test_standard.py -q`: PASS.
- [ ] **Step 13: Commit.** One commit for each step that changes code: `refactor: <what the step did>`.

Additions from the Task 4 review:
- Step 10: `engine/inventory.c` also calls `champion_draw_state(champion_index)` of `ui.h`. Replace it with attribute flags (the tick-end `champion_draw_all_states(0)` draws them) or move it to the click handler in `ui/command.c`, whichever keeps the screen identical. Check Review focus item 2.
- Step 11: `engine/dungeon.c` and `engine/savefile.c` have no `layer` key. Only `data/csbtext.c:champion.h` (the `SKILL_*` constants) remains.
- Step 9: Task 5 sets flags in `champion_kill` and adds `REDRAW_*` bits in `engine/game.h`. The release of the dead champions keeps its order with these flags and with `refresh_interface` in `ui/gameloop.c`.

Decisions made during the task:
- Each step removes its own `layer` lines from the allowlist, so that the full suite is green after each step.
- Step 2: `ui_object_name` and `ui_action_name` share the string loader with `ui_string`. All three lists move to `data/graphics.c` and become `gfx_string`, `gfx_object_name`, and `gfx_action_name`. `gfx_load_strings` loads them, and `ui_init` calls it.
- Step 3: `message_print` uses the word wrap of `ui/text.c`. The word wrap moves to `engine/message.c` as `message_next_word_line`, and `ui/dialog.c` calls it. The four font metrics (`TEXT_CHAR_WIDTH`, `TEXT_CHAR_GAP`, `TEXT_LINE_HEIGHT`, `TEXT_BASELINE_GAP`) move from `ui/text.h` to `data/graphics.h`. `message_draw` becomes the static `draw_messages` in `ui/ui.c`. Files that got `ids.h` through `text.h` include it now.
- Step 4: no file of `engine/` calls `sound_stop_all`. `tests/test_music.py` calls `music_mix` after `sound_mix` in its `mix` helper, and `music_stop` after `sound_stop_all`, as `platform.c`, `anim.c`, and `logo.c` do now. The assertions do not change. Commit 7d420b2 renames `test_sound_stop_all_stops_the_track` to `test_music_stop_after_sound_stop_all_stops_the_track_and_keeps_the_soundtrack`, because the test now checks `music_stop`.
- Step 6: `explored_mark(map, x, y)` marks one square. The view rules that decide which squares the party sees (`automap_mark_seen`, `automap_visible_depths`, `automap_side_is_visible`) stay in `ui/automap.c`, because they use `VIEW_DEPTH_COUNT` of `ui/view.h`. `automap_mark_seen` calls `explored_mark`. `AUTOMAP_SAVE_MAGIC` is `EXPLORED_SAVE_MAGIC` now. `explored_is_seen` returns `bool` (rule 3 of the standard, new header without an allowlist entry), and the tests bind it with `restype = ctypes.c_bool`. `ui/automap.c` and `engine/explored.c` each keep a static `is_on_map`.
- Step 7: `champion_set_portrait` moves with `g_champion_portraits` and `CHAMPION_PORTRAIT_WIDTH`/`HEIGHT` to `engine/champion.c` and `engine/champion.h`. It copies the pixels with a clipped loop, the same result as the opaque `draw_blit` of `ui/draw.c`. `champion_portrait` (the `Bitmap` view of a portrait) stays in `ui/ui_champion.c`.
- Step 8: the engine does not include `ui/startend.h`. `engine/timeline_process.h` declares the hook `g_endgame_sensor_hook`, and `gameloop_load_data` in `ui/gameloop.c` sets it to `endgame_sensor` (as `g_frame_hook` lets `ui/` call `platform/`). The sensor event calls the hook at once, as `endgame_sensor` was called at d2059c4: `endgame` returns, and the rest of the event, the later events of the same `timeline_process`, and the rest of the tick run after the ending and see `g_game_won` = 1. Each sensor runs the ending. No pending state is kept, so a load or a restart has nothing to reset. A first version kept the sensor delay in `g_endgame_delay` and ran the ending after the tick: a party death in the same tick took the lost ending, the delay stayed for the next loaded game, and later events saw `g_game_won` = 0. The review found it, and the hook replaced it. Tests in `tests/test_csb_ending.py`: `test_sensor_18_in_a_tick_starts_the_ending`, `test_sensor_18_wins_over_a_party_death_in_the_same_tick` (also no ending in the next tick of a loaded game), `test_an_event_after_sensor_18_in_the_same_tick_sees_the_won_game`. They pass on d2059c4. A comparison against d2059c4 and 30ab027 (sensor in a tick, with a party death, with a later fluxcage removal, two sensors, with a champion redraw event) gives the same game state and the same frame hashes.
- Step 9: only `champion_apply_and_draw_pending_damage_and_wounds` calls `champion_kill`, and only `gameloop_tick` calls it. `gameloop_tick` releases the dead champions right after it, before the dead party check and before `refresh_interface`, with the static `release_dead_champions`. `champion_kill` passes the leader to the first living champion before the release, so `command_release_dead_champion` gets the leader before the deaths as a second parameter. Moved in time: the inventory close, the end of the presses, and the black icon now come after the objects drop and after the other champions of the same damage step. With the inventory still open during the drop, `champion_get_object_removed_from_slot` sets `CHAMPION_ATTR_VIEWPORT` and `CHAMPION_ATTR_PANEL` on the dead champion, which `champion_draw_state` ignores for a dead champion, and closes the open chest at its slot, as the inventory close did before. A frame hash comparison against the build of Step 8 (inventory open or closed, the leader dies or not; 4 ticks each) gives the same frames.
- Step 10: a blink before the eat or drink call would show the object in the leader hand during the blink. So `process_click_on_mouth` returns `MOUTH_REFUSED`, `MOUTH_SWALLOWED`, or `MOUTH_USED_UP`, and the new `command_click_on_mouth` in `ui/command.c` does the rest in the original order: the blink when the object is used up, the swallow sound, the `g_panel_content` check (kept in `ui/`), and `champion_draw_state`. `CHAMPION_ATTR_STATISTICS` stays in the rule. `tests/test_panels.py` calls `command_click_on_mouth`; its assertions do not change. A frame hook comparison against the build of Step 9 (food and potion, food panel or not) gives the same frames and the same screen.
- Step 11: the skill enum (`SKILL_FIGHTER` to `SKILL_COUNT`) moves from `engine/champion.h` to `data/tables.h`, and `engine/champion.h` includes `tables.h`. `SKILL_IGNORE_*` stays in `engine/champion.h`. `engine/champion.c` used nothing of `ui.h` after Step 7; Step 11 removes the include and its `layer` key.

---

### Task 7: Prefixes and numbered names

Decisions of the user on 2026-10-07, before the task:
- A first attempt found many more collisions than Step 3 lists. All collisions get a new name. The header of the module that owns the game task keeps the prefix. No narrow prefix of more than one word avoids the check.
- Raw record accessors in `engine/map.h` and `engine/things.h` get the prefix `rec_`, for example `sensor_type` → `rec_sensor_type`. The modules (`sensor.h`, `group.h`, `projectile.h`, ...) keep their prefixes.
- `timeline_` stays with the event queue in `engine/game.h`. `engine/timeline_process.h` gets the prefix `event_`, for example `timeline_process_event1_door_animation` → `event_door_animation`. No function had the prefix `event_` before.

Decisions made during the task:
- One commit for each prefix group. A name gets the prefix of its header in place of the old prefix. Where this gives a bad name, the name changes more (see the list below).
- `engine/things.h` merges into `engine/map.h`, and `things.h` is deleted. Reason: a prefix belongs to one header only, so `rec_` cannot be in both headers. The 4 files that included `things.h` include `map.h`. The `bool`, `decimal`, and `hex` lines of `things.h` go, because `map.h` has the same lines.
- `rec_` is for an accessor of a thing record (`REC(thing)`), of the creature attribute bits, and of the dungeon header, when its name collides with a module prefix: `sensor_` (22), `text_` (2), `object_` (5), `group_` (4), `projectile_` (3), `explosion_` (3), `creature_height`, and `dungeon_` (2). Accessors without a collision keep their prefix: `door_`, `weapon_`, `armour_`, `junk_`, `potion_`, `container_`, `scroll_`, `teleporter_`, `thing_`. The map descriptor accessors of `things.h` (`map_floor_set`, ...) are of the same kind as `map_offset_x` of `map.h`. After the merge they do not collide, so they keep `map_`.
- `group_get_thing` of `map.h` finds the group on a square. It is not a record accessor: `get_square_group`, as `get_square_first_object`.
- `inventory.h` keeps `object_` (`object_get_type`, `object_get_icon_index`), because the `object_` accessors of `map.h` become `rec_`.
- Owners of the other shared prefixes: `get_`, `set_`, `is_` stay with `map.h`. `load_` stays with `dungeon.h` (`load_dungeon`). `save_` stays with `savefile.h`. `command_` stays with `command.h`. `menus_`, `panel_` stay with `ui.h`.
- `champion_` functions of other headers: `action.h` → `action_`, `game.h` → `game_`, `group.h` → `group_`, `inventory.h` → `inventory_`, `magic.h` → `magic_`, `move.h` → `move_`, `projectile.h` → `projectile_` (`champion_shoot_projectile` → `projectile_shoot_from_champion`), `ui.h` → `ui_` with the object after the verb (`champion_draw_state` → `ui_draw_champion_state`, `champion_draw_all_states` → `ui_draw_all_champion_states`, `champion_portrait` → `ui_champion_portrait`).
- Other names that change more than the prefix: `game_freeze`/`game_unfreeze` → `command_freeze_game`/`command_unfreeze_game`, `inventory_toggle` → `command_toggle_inventory`, `inventory_draw_open`/`close` → `ui_draw_inventory_open`/`close`, `command_move_party` → `move_party`, `magic_map_set_position`/`magic_map_position` of `game.h` → `game_set_magic_map_position`/`game_get_magic_map_position`, `move_is_killed_by_projectile_impact` → `projectile_is_killed_by_impact`, `sensor_is_launcher` of `projectile.h` → `projectile_is_launcher_sensor`, `timeline_trigger_projectile_launcher` → `projectile_trigger_launcher`, `timeline_process` → `event_process_expired`.
- Numbered names: `group_process_events_29_to_41` → `group_process_reaction_event`, `projectile_process_events_48_to_49` → `projectile_process_move_event`, `explosion_process_event_25` → `explosion_process_step_event`, `timeline_process_event60_61_move_group` (in `group.h`) → `group_process_move_event`, `menus_create_event70_light` (in `magic.h`) → `magic_create_light_event`, `timeline_process_eventN_<task>` → `event_<task>`.
- `csbimage.h` and `csbtext.h` both used `csb_`. Their prefixes are `csb_image_` and `csb_text_`: the module names, not a way around the check. `platform/scale.h` states `scale2x_ scale3x_`. `scale_` of `game.h` (`scale_value`) does not collide with them, because the check compares whole prefixes.
- Where the renames apply: `port/src`, `tests`, `src/dmtools`, `tools`, `README.md`, and `docs/` without `docs/superpowers/` and without `CHANGELOG.md` (history). The symbol lists `docs/kaos-symbols.csv` and `docs/csb-appb-symbols.csv` get the new names too, with the jump table names (`jt_<name>`), because rule 4 of the standard puts the address of a port function there. A regenerated `decomp/` listing shows the new names. Struct fields (`.name`, `->name`) and file names (`timeline_process.c`) do not change.

**Files:**
- Modify: every header that the `prefix` check names, the renamed functions and all their callers, the `ctypes` bindings in `tests/`
- Test: `tests/test_standard.py`, `tests/standard_allowlist.txt`

- [ ] **Step 1: Remove the `prefix` and `numbered` lines from the allowlist.** Run `uv run pytest tests/test_standard.py -q`. Expected: FAIL in two tests.
- [ ] **Step 2: Add the prefix lines.** Add one `/* Prefixes: ... */` line after the include guard of each header. List the prefixes that the header already uses. Examples: `data/graphics.h` → `gfx_`, `platform/platform.h` → `plat_`, `engine/map.h` → the accessor prefixes it uses (`map_`, `get_`, `door_`, `sensor_`, `teleporter_`, and others).
- [ ] **Step 3: Rename the collisions.** Run the test. For each collision, the header of the module that owns the game task keeps the prefix. Rename the functions of the other header. Expected renames:
  - `menus_` in `engine/action.h` → `action_`.
  - `champion_` functions declared in `ui/ui.h` (for example `champion_draw_state`) → `ui_`, for example `ui_draw_champion_state`.
  - `champion_` functions in `engine/inventory.h` (for example `champion_put_object_in_leader_hand`) → `inventory_`.
- [ ] **Step 4: Rename the numbered names.** Name the game task:
  - `group_process_events_29_to_41` → `group_process_reaction_event`.
  - `projectile_process_events_48_to_49` → `projectile_process_move_event`.
  - `timeline_process_eventN_<task>` → `timeline_process_<task>`, for example `timeline_process_event1_door_animation` → `timeline_process_door_animation`.
  Update the comments that name the old functions, for example `group.h:47`.
- [ ] **Step 5: Update the tests.** `grep -rn '<old name>' tests src` for each renamed function. Change the `ctypes` bindings. Check that `platform/state_json.c` and `platform/control.c` do not send a renamed name over the protocol.
- [ ] **Step 6: Run the standard test.** Expected: PASS.
- [ ] **Step 7: Run the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 8: Commit.** `git commit -am "refactor: one prefix for each header, no event numbers in names"`

---

### Tasks 8 to 11: Cleanup by module

Each of these tasks does the same work on its files. The files:

| Task | Files |
|---|---|
| 8 | `data/*`, `engine/random.c`, `engine/map.c`, `engine/map.h`, `engine/things.h`, `engine/dungeon.c`, `engine/savefile.c`, `engine/explored.c` |
| 9 | `engine/champion.c`, `engine/action.c`, `engine/inventory.c`, `engine/magic.c`, `engine/sensor.c`, `engine/move.c`, `engine/game.c`, `engine/recruit.c`, `engine/message.c`, `engine/sound.c` |
| 10 | `engine/group.c`, `engine/projectile.c`, `engine/timeline_process.c`, `engine/savegame.c` |
| 11 | `ui/*`, `platform/*` |

The headers of the `.c` files belong to the same task.

**Files:**
- Modify: the files of the task, their callers when a signature changes, the `ctypes` bindings in `tests/` when a signature changes
- Test: `tests/test_standard.py`, `tests/standard_allowlist.txt`, the test file of each module

Do the steps for one file at a time. Run the full suite after each file.

- [ ] **Step 1: Remove the allowlist lines of the file.** Keys `hex`, `decimal`, `decompiler`, `bool`, and `long`. Keep `hex data/csbimage.c`, `hex data/csbtext.c`, `hex engine/map.c`, and `hex engine/map.h`. Run `uv run pytest tests/test_standard.py -q`. Expected: FAIL for this file, or PASS if the file had no lines.
- [ ] **Step 2: Name the numbers.** For each `hex` and `decimal` failure, and each plain number 3 to 9 that is a game value, add a named constant. Find the meaning in `docs/` (`rules-*.md`, `tables.md`, `game-state.md`, `docs/symbols/`) and in `decomp/KAOS.c`. Put the constant in the header of the module that owns the value (rule 1). Examples from `engine/champion.c`: `/= 21` and `/= 25` (stamina gain divisors), `115 - wisdom`, `idle > 80` and `> 250`.
- [ ] **Step 3: Predicates return `bool`.** Change each `is_`, `has_`, and `can_` function of the file to return `bool`. Include `<stdbool.h>` in the header. In each test that binds the function, set `restype = ctypes.c_bool`.
- [ ] **Step 4: One type for each value.** For each cast `(int16_t)` or `(uint16_t)` in the file:
  - If the cast only passes a value between a variable, a parameter, or a return value of the other 16-bit type, change the declaration so both have the same type. Use the type of the struct field the value comes from. Then delete the cast.
  - If the cast changes the result (16-bit wrap, sign, truncation), keep it. Add a one-line comment when the reason is not obvious, for example `/* 16-bit wrap as in the original */`.
  - Values that are not game values (sizes, file offsets, loop counters over C arrays) become `int`.
  - In the tests, change `argtypes`/`restype` of a changed function to the new type.
  Check Review focus item 1 for each change.
- [ ] **Step 5: Split the long functions.** For each `long` failure, split the function where a part has its own game meaning, for example one `case` group of a `switch`. The new function is `static`, with a one-line comment. If no split has its own meaning, add the reason to the comment of the function and the line `long <path:function> <reason>` to the allowlist.
- [ ] **Step 6: Run the standard test and the full suite.** `uv run pytest -q`. Expected: all pass.
- [ ] **Step 7: Commit the file.** `git commit -am "refactor: coding standard in <file>"`.

---

### Task 12: Final check and release notes

**Files:**
- Modify: `CHANGELOG.md`, `tests/standard_allowlist.txt` (only if a line is stale)

- [ ] **Step 1: Check the allowlist.** It holds only these lines: `hex data/csbimage.c`, `hex data/csbtext.c`, `hex engine/map.c`, `hex engine/map.h`, and `long` lines with a reason. No line says `open:`.
- [ ] **Step 2: Build from clean.** `make -C port clean all`. Expected: no warnings.
- [ ] **Step 3: Run the full suite.** `uv run pytest -q`. Expected: all pass, at least 577 + the new tests.
- [ ] **Step 4: Update `CHANGELOG.md`.** Add a section `## 0.12.0 - <date>` with `### Changed`: the source folders `data/`, `engine/`, `ui/`, `platform/`; the standard test; no change of game behavior.
- [ ] **Step 5: Commit.** `git commit -am "docs: changelog for the coding standard refactor"`
- [ ] **Step 6: Manual test checklist for the user.** Give the user this list. Each item runs `build/dm`:
  1. DM, vanilla: enter the dungeon, move, turn, open an inventory, eat and drink, take and drop an object.
  2. Fight a creature with a melee action and a spell. The action area and the spell area redraw after each action.
  3. A champion dies with the inventory open: the inventory closes, the status box shows the dead champion.
  4. Save, quit, resume. Advanced mode: quick save F5, quick load F9, the automap keeps the explored squares.
  5. CSB: NEW ADVENTURE, the prison save makes the new adventure.
  6. Music on the automap (advanced): 2 and 3 play, a logo or an animation stops the music.
  7. Game won (DM): the endgame starts.
