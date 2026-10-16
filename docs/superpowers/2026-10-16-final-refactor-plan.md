# Final Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the C port, its build, its tests and its docs to the target state of the PRD and of `docs/coding-standard.md` v2, with no change of game behavior and no change of the drawn pixels.

**Architecture:** First freeze the behavior in golden snapshots on unchanged code. Then move the tree (`port/src/` → `src/`, root `Makefile`, `dmtest` loader), implement every check of the standard with `open: epic N` allowlist lines, and refactor layer by layer (data, engine, ui, audio, control, platform). The tests, the docs and the repository files come last; the last task removes every `open:` line.

**Tech Stack:** C11, clang and MinGW-w64 GCC, GNU make, SDL3, Python 3.14 with uv, pytest with pytest-xdist, ctypes, clang-format 23 (PyPI), ruff.

**Spec:** `docs/superpowers/2026-10-16-final-refactor.md` (PRD). Target standard: `docs/coding-standard.md` v2. Evidence: `docs/superpowers/2026-10-16-final-refactor-inventory/`.

## Global Constraints

Project rules (CLAUDE.md, skills c-decomp-port and python-uv):

- Work on branch `feature/2026-10-16-final-refactor` in the main checkout. No push, no merge to main.
- Game behavior and the drawn pixels do not change. Invariants: the golden snapshots `tests/golden/save.json`, `view.json`, `rules.json`, `screens.json` (Task 2), `protocol.json` (Task 1) stay equal; the differential tests pass; `Party`, `TimelineEvent`, `ActiveGroup` keep `sizeof` and field offsets; `Champion` keeps the offsets of its saved fields and appends runtime fields after them; the save file layout does not change; the state JSON names and the request names of the protocol do not change.
- A difference in a differential or golden test during a rule move (move chain, rules out of ui) stops the run for the user.
- C11, 0 warnings. From package B on, the flags are `-std=c11 -Wall -Wextra -Werror -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla -Wimplicit-fallthrough -O2 -g`, for `make` and `make windows`.
- Every change in `src/` follows `docs/coding-standard.md` v2. A violation that a later epic removes goes into `tests/standard/allowlist.txt` as `<check> <target> open: epic N` (epic numbers in the Epic map below); the last task removes every `open:` line.
- Commands before package B: build `make -C port`, one test file `uv run pytest -q tests/test_x.py`. From package B on: build `make` (game and library), `make lib` (library only), one test file `uv run pytest -q tests/<folder>/test_x.py`, full suite `make test` (`uv run pytest -q -n auto`).
- The author runs the build and only the tests of the changed code, never the full suite. The controller runs the full suite.
- Names, prefixes, types and signatures that this plan states are binding; a task that needs a name the plan does not state picks one by the standard and reports it.
- Line numbers in this plan are of commit `72e24b5` (the plan base). Earlier tasks move code; find a place by the symbol name the plan gives, and use the line number only as a hint.
- Mechanical changes over many files (moves, include rewrites, renames, enum-to-define) run as the script that the task gives; the script stays out of the repository unless the task says otherwise. A task that is a scripted rename may touch many files; every other task changes one module.
- Moves use `git mv`, so `git blame` follows. The format commit, the include-sort commit and the SPDX commit go into `.git-blame-ignore-revs`.
- Renames: a rename updates the symbol table (`docs/reference/kaos-symbols.csv`, before package C `docs/kaos-symbols.csv`), the ctypes structures and names in the tests, and every doc that names the old name, in the same commit (grep `docs/`, `tests/`, `tools/`, `.claude/`).
- Keep existing comments. New comments only where needed, one line, about what the code does, never its history.
- Conventional Commits, one type per commit, one logical change per commit; each commit builds and passes the tests of the changed code.
- uv only (`uv add --dev`, `uv run`); never pip or the system python. No `-n auto` in `addopts`.
- Do not download or run third-party code (libraries, CDN files, packages outside the project's lock file) to check behavior; report what you could not verify. The packages that the plan adds (`clang-format`, `ruff`) come in with `uv add --dev`.
- No email address in any file, commit or request.

Test list (CLAUDE.md §2; the review focus adds a test only for code on this list):

- Write a test only for: a check against a reference (original program, golden snapshot), a parser or data format, a rule with more than one outcome that no reference check or type check covers (dates, parsing, prices, state machines), a regression of a real bug (one test per bug).
- No tests for: constants and data tables; wiring, scaffold, CLI arguments, logging; what the compiler, a linter or a standard check catches; UI and layout (the user tests them manually); what another test already covers. No mutation tests.
- Tests pass in any order and in parallel: each test file owns its state (no global state shared between test files, unique temp paths and sockets).
- Each test module loads its own copy of the library; tests reset the global state they read. Stub only the technical layer of the original; never game rules.

Weakened assertions (approval: pending):

Each item removes or loosens an existing assertion; the owner approves the list once before the run.

1. `tests/test_tables.py:76` `assert not lib.tables_get_creature_info(0)` before the load: removed (Task 30).
   R4.3 removes the `ready` flag; before `tables_load` an accessor returns a pointer to zeroed tables, not NULL.
   `tables_load(b"/nonexistent/KAOS.FTL")` failing replaces the "no image" case.
2. `tests/test_tables.py:77` `assert lib.tables_init() == -1`: removed (Task 30); `tables_init` becomes static.
   Same replacement as item 1.
3. `tests/test_tables.py:35,50` the `door_info` row (the byte check of `DoorInfo` against the image): removed
   (Task 30), because R4.3 deletes `tables_get_door_info` and its table.
4. `tests/test_tables.py` `test_accessor_rejects_out_of_range` and `test_symbol_mana_cost_rejects_out_of_range`
   for the nine scalar tables: `is None` becomes `== 0` (Task 31). R4.3 returns scalars by value; out of range
   gives 0. The struct tables keep `is None`.
5. `tests/test_tables.py` the out-of-range index `0x7FFFFFFF` becomes `0x7FFF` (Task 31): the parameter is
   `int16_t`.
6. `tests/csb_diff.py` becomes `tests/differential/test_csb_mini.py` with assertions (Task 17); a comparison that
   reports a known difference may stay out, named in the commit message. (New coverage; listed because the task
   chooses what to assert.)

Not weakening, for the record: the harness changes (paths, imports, names, `bool` instead of `0/-1`, struct
fields instead of globals), the stricter record and notation checks (Tasks 9-14), and the golden tests without
`scrub_free_groups` (Task 67, stricter).

## Review Focus

The inputs and conditions that no task's own tests would meet, most likely first, each with its test:

1. Two test modules in one xdist worker share a process: each must see its own C globals. Test: Task 7
   (`test_each_load_has_its_own_globals`).
2. A CSB load that fails in `csb_text_build` must leave no loaded image (a real bug,
   `port/src/data/csbimage.c:184-185`). Test: Task 36 (`test_failed_text_build_publishes_no_image`).
3. A client of the control socket sends two requests in one write, or disconnects in the middle of a line: two
   replies in order; the game keeps running and the next client gets answers. Test: Task 115.
4. A command line with an unknown option, a missing value or a bad number stops with an error; `--help` prints
   the table. Test: Task 117 (`tests/port/test_options.py`; the options parser is a parser).
5. A change of one header rebuilds every object that includes it (`-MMD -MP`), so a stale object never links.
   Check: Task 5, Step 8 (a Makefile step, not a pytest).

The scroll text that starts with a newline (R8.9) is probably not a bug at `72e24b5` (`end - 2 >= text`); Task 95
checks it first and adds the guard and one regression test only when the read before the text exists.

## Packages and streams

The run groups the tasks into packages A to I (the owner's request, 2026-10-16: parallel tasks, the tests after
each package, quality and run time both count). A package has parallel streams; a stream is a sequence of tasks
that one author works through in order. A package can end with a tail: tasks that run after the merge of the
streams, for work that rewrites many files. Packages run one after the other.

| Package | Epics (for `open: epic N`) | PRD | Tasks | Streams | Tail |
|---|---|---|---|---|---|
| A | 1 | R10.5, R12.7, R8.3 scenarios | 1-3 | A1 protocol golden (1), A2 screens golden (2), A3 rule scenarios (3) | none |
| B | 2 | R1, R2, R12.1, R14.3 | 4-8 | B1 one sequential stream (the tree moves touch every file) | none |
| C | 3, 12, 13, 14 | R3, R12 without R12.6/R12.7, R13.3, R14.1/2/4 | 9-26 | C1 standard checks (9-14), C2 tests (15-19), C3 docs folders and repository files (20-22) | 23-26: SPDX and guards, enums to defines, comments to headers, stale lines |
| D | 4, 9 | R4, R5.7, R9 | 27-47 | D1 os, kaos, ftl (27-29), D2 tables, spells, ids (30-35), D3 csb image, file, adventure, graphics (36-39), D4 audio (40-41) | 42-47 |
| E | 5 | R5.1-R5.3, R5.5, R5.6 | 48-61 | E1 record module (48-49), E2 game.h split (50-56, the critical path), E3 helpers, message, savefile, explored (57-61) | none |
| F | 5, 6, 7 | R5.4, R5.8, R6, R7 | 62-84 | F1 group and move, F2 champion, inventory, action, magic, F3 projectile, sensor, savegame, sound (62-76) | 77-84: damage outcome, sound requests, indices, Session, type table, dead API, game names, constants |
| G | 8 | R8 | 85-109 | G1 view (85-92), G2 status, panel, menus (93-96), G3 frame, entrance, command, select (97-100), G4 rules out of ui (101-104) | 105-109: Screen, view result, copied code, types, comments |
| H | 10, 11 | R10, R11 | 110-120 | H1 control and transport (110-116), H2 options, debug view, platform.c (117-119) | 120: `app.c`, `main.c` |
| I | 13 | R13.1, R13.2, R13.4-R13.6 | 121-126 | I1 architecture (121), I2 README split (122), I3 CLAUDE.md, agents, todo, CHANGELOG (123-124) | 125-126: the last `open:` lines, the PRD acceptance |

Epic numbers: 1 golden snapshots, 2 tree and build, 3 standard checks, 4 data, 5 engine modules and records,
6 engine API shape, 7 engine state, 8 ui, 9 audio, 10 control, 11 platform, 12 tests, 13 docs and repository
files. Package C moves R12, R13.3 and R14 before the code epics (the PRD order puts them last): the later tasks
then edit tests, docs and new files in their final place and with their SPDX line, once.

## Run (overrides of the agentic-workflow run for this plan)

The owner's request replaces "the next task starts after a green full suite" and "work in the main checkout":

- **Worktrees:** each stream runs in its own worktree `../dm-wt/<stream>` on branch
  `feature/2026-10-16-final-refactor-<stream>` from the head of `feature/2026-10-16-final-refactor`. Link
  `game/`, `game-csb/`, `reference/`, `playthroughs/` into it; run `uv sync` there. The build output stays in
  the worktree. A package tail runs in the main checkout.
- **Parallel:** the controller dispatches the first task of every stream of a package at the same time; a
  stream's next task starts when its previous task is reviewed (the review rules of SDD and agentic-workflow
  stay: one task review, one unreviewed fix).
- **Author checks:** the author runs the build and the tests of the changed code (the task names the command),
  never the full suite.
- **Merge:** when every stream of a package is done, the controller merges the stream branches into
  `feature/2026-10-16-final-refactor` with `git merge --no-ff`, in stream order. A conflict in an include block:
  take the union and run `make format`. A conflict in `tests/standard/allowlist.txt` cannot happen
  (`merge=union`, Task 9). Any other conflict: the controller resolves it when both sides are
  mechanical, else a fix author resolves it. Then the controller removes the worktrees and the stream branches.
- **Full suite:** after the merge of a package and again after its tail, in the main checkout: `make` (before
  package B: `make -C port`), `make windows` once per package from package B on, and the full suite. On red: a
  debugger subagent (superpowers: systematic-debugging), a fix author, one new run. The next package starts after
  a green run.
- **Package review:** after the green run, one opus task reviewer (effort medium) reads the merged diff of the
  package for cross-stream problems (two streams that changed one interface in different ways).

## Shared interfaces

Names that tasks of several epics use; each is produced by the task named.

- `tests/dmtest/lib.py`: `load_lib() -> ctypes.CDLL` loads a private copy of `build/libdm.dylib` for the calling test module (Epic 2).
- `tests/dmtest/constants.py`: `from dmtest import constants as C`; `C.KEY_RETURN` is the value of the C constant (Epic 2 makes the module, Epic 12 removes the mirrored constants).
- `tests/dmtest/markers.py`: `needs_dm`, `needs_csb`, `needs_sdl`, `needs_ghidra` (pytest skip markers).
- `tests/standard/test_standard.py` and `tests/standard/allowlist.txt`, line format `<check> <target> <reason>`, targets relative to `src/` (Epic 3).
- `src/engine/record.h`: `rec_<type>_<field>`, `rec_<type>_set_<field>`, `rec_read(thing)`, `rec_write(thing)` (Epic 5).
- `src/engine/map.h`: `Location {int16_t x, y;}`, `Position {int16_t map, x, y;}`, `map_step`, `direction_right/back/left` (Epic 5).
- `src/engine/game.h`: `Session g_session` (Epic 7); `src/ui/video.h`: `Screen g_screen` (Epic 8).
- `src/engine/sound.h`: `g_sound_play_hook(sample, length, period, volumes)` (Epic 5), set by `ui/audio.c` (Epic 9).

---

## Rulings of the plan (the owner can reject one)

- Packages and streams (the owner's request): R12 without R12.6/R12.7, R13.3 and R14 move from the end into
  package C; the SPDX lines, the enum-to-define script and the comment move run in the package C tail (Tasks
  23-25), so every later task works on final files.
- Fall-through: clang does not accept `/* fall through */` in C; the five sites get `__attribute__((fallthrough));`
  and keep the comment (Task 5).
- `-isystem third_party` instead of `-Ithird_party`, so GCC with `-Werror` does not warn in `dr_mp3.h` (Task 5).
- `conftest.py` runs `make all` when SDL3 is installed (five tests start `build/dm`), else `make lib` (Task 7).
- `--import-mode=importlib` comes with the test folders (Task 17), because the test modules import each other
  until Task 16; Task 7 removes `-n auto` and sets `pythonpath = ["tests"]`.
- `dungeon.c` asks `tables_is_loaded()`, not `kaos_is_loaded()`: standard rule 3.6 keeps `kaos_*` calls in
  `data/` (Tasks 28, 42).
- The 16-bit helpers are C11 `inline` with one external definition in `arith.c`, not `static inline`: the
  differential test calls them in the library (Task 50).
- `game_fatal` and the record log go through `os_fatal` and `os_log` of `data/os.h` (Tasks 27, 51).
- The move chain walker takes an `int16_t mover` with `MOVER_*` defines, not an enum (standard 3.2; Task 66).
- `g_current_group_map_x/_y` and `_thing` live across calls (the Couatl sound and the teleporter check,
  `port/src/engine/group.c:233, 907`): they become a file-static `last_turn`, not `GroupTurn` fields (Task 62).
- The view state shared by three files is `View g_view` in `view_internal.h`, not `static` (Task 92).
- `inventory_grab_object(thing, cell, at)` takes the pile top object from the view;
  `inventory_drop_object(cell, at, onto_vi_altar)` tests the altar (Task 101). The output mapping becomes the
  static `layout_output` and `map_mouse` in `platform.c` (Task 100), so the dead-API check passes.
- The transport moves in stream H1 (Task 115), not in the tail; `transport_poll` steps a running turn before it
  reads requests, as `control_poll` does today, so the golden protocol does not change; `control_drop_client`
  stops a batch when the client goes (Task 115).
- The `decimal` check accepts the base `10` of `strtol` (one sentence in standard rule 3.1, Task 113).
- The two `ACTION_STAB_*` game names come from the game tables at run time (no source in `docs/` names them);
  the run reports them (Task 83).

## Tasks

## Package A: golden snapshots on unchanged code (Epic 1)

The three streams add new golden tests before any code moves. Each stream writes one test file and one golden
file, so the streams touch disjoint files. The code under `port/src/` does not change in this package.

Facts that all three streams use:

- A private copy of the library has its own C globals: `tests/test_golden_rules.py:44-47` and
  `tests/test_golden_save.py:45-48` copy `build/libdm.dylib` into `tmp_path` and load the copy ("an xdist worker
  shares one image per path").
- The golden pattern: `check(name, got)` in `tests/test_golden_rules.py:79-86` compares with the JSON file and
  writes it when `DM_UPDATE_GOLDEN` is set; `snapshot(game)` (`:71-76`) hashes the saved state and the frame
  buffer `fb` (320 x 200 bytes).
- The library seeds nothing at load: `g_random_seed` is set only by `random_init` (`port/src/engine/random.c:13-18`),
  which only `port/src/platform/main.c:702` calls; tests set `g_random_seed` themselves.
- The game loop does not read the wall clock: `gameloop_run_frame` (`port/src/ui/gameloop.c:335-351`) counts
  frames, so a test that pumps frames is deterministic.

#### Stream A1: golden protocol snapshot (R10.5)

Shared files: none.

### Task 1: Golden snapshot of the control protocol

**Files:**
- Create: `tests/test_golden_protocol.py`
- Create: `tests/golden/protocol.json` (written by the test with `DM_UPDATE_GOLDEN=1`)

**Interfaces:**
- Consumes: `Client` (`tests/test_control.py:100-125`: `call(**payload)` sends one JSON line and pumps frames
  until the reply), `Game`, `STEP_EAST`, `STEP_NORTH` (`tests/test_gameloop.py:15,49-138`), `ROOT`
  (`tests/test_port.py:9`). C: `control_open(const char *path)` (`port/src/platform/control.h:12`),
  `control_close(void)`, `startend_set_save_dir(const char *)`, `gameloop_load_data(const char *)`.
- Produces: `tests/golden/protocol.json` = `{"replies": [<sha256 of each reply>, ...]}`; the gate of package H.
  Package B changes the fixture to `load_lib()`; package H changes the C names that the fixture calls
  (`control_open`, `control_close`); the replies and the golden file do not change.

Facts: the request names are in `port/src/platform/control.c:1274-1315` (`load`, `new_game`, `get_state`,
`get_view`, `get_map`, `save`, `act`, `act_many`, `wait`, `screenshot`); the act table is
`port/src/platform/control.c:648-674` (`look` is an act action). `new_game` does not set the seed
(`port/src/platform/control.c:1215-1224`). `save` answers `{"ok": true}` without the path
(`tests/test_control.py:227-231`). The `screenshot` reply holds the PNG path, so the sequence leaves it out.

- [ ] **Step 1: Write the test**

```python
"""Golden snapshot of the control protocol: a fixed request sequence gives the same JSON replies."""

import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import tempfile

import pytest

from test_control import Client
from test_gameloop import STEP_EAST, STEP_NORTH, Game
from test_port import ROOT

GOLDEN = ROOT / "tests" / "golden" / "protocol.json"
GAME = ROOT / "game"
SEED = 0x1234ABCD

needs_game = pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")),
                                reason="game files missing")


@pytest.fixture
def game(tmp_path):
    """The loaded game data in a private copy of libdm, with an open control socket and a temp save folder."""
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    copy = tmp_path / "libdm.dylib"
    shutil.copy(ROOT / "build/libdm.dylib", copy)
    lib = ctypes.CDLL(str(copy))
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.control_open.restype = ctypes.c_bool
    lib.control_open.argtypes = [ctypes.c_char_p]
    lib.control_is_turn_pending.restype = ctypes.c_bool
    lib.map_get_square.restype = ctypes.c_uint8
    lib.map_get_square_first_thing.restype = ctypes.c_uint16
    lib.map_get_next_thing.restype = ctypes.c_uint16
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.kaos_s16.restype = ctypes.c_int16
    assert lib.gameloop_load_data(str(GAME).encode())
    game = Game(lib)
    game.saves = tmp_path / "saves"
    game.saves.mkdir()
    lib.startend_set_save_dir(str(game.saves).encode())
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    game.socket = os.path.join(sock_dir, "control.sock")
    assert lib.control_open(game.socket.encode())
    yield game
    lib.control_close()
    shutil.rmtree(sock_dir, ignore_errors=True)


def act(client, action, **args):
    return client.call(cmd="act", action=action, args=args)


def session(game, client):
    """The fixed request sequence: a new game, the walk to the first mirror, a recruit, the queries, a save and a load."""
    ctypes.c_uint32.in_dll(game.lib, "g_random_seed").value = SEED
    replies = [client.call(cmd="new_game"), client.call(cmd="get_state")]
    path, direction = game.path_to_mirror()
    for (x0, y0), (x1, y1) in zip(path, path[1:]):
        step = next(k for k in range(4) if (x0 + STEP_EAST[k], y0 + STEP_NORTH[k]) == (x1, y1))
        while game.position()[2] != step:
            replies.append(act(client, "turn_left"))
        replies.append(act(client, "move_forward"))
    while game.position()[2] != direction:
        replies.append(act(client, "turn_left"))
    replies.append(act(client, "click_wall_ahead"))
    replies.append(act(client, "recruit", choice="resurrect"))
    replies.append(client.call(cmd="get_view"))
    replies.append(client.call(cmd="get_map"))
    replies.append(act(client, "look"))
    replies.append(client.call(cmd="wait", ticks=10))
    replies.append(client.call(cmd="act_many", actions=["turn_left", {"action": "turn_right", "args": {}}]))
    replies.append(act(client, "fly"))
    replies.append(client.call(cmd="goto"))
    replies.append(client.call(cmd="save"))
    replies.append(client.call(cmd="wait", ticks=5))
    replies.append(client.call(cmd="load"))
    replies.append(client.call(cmd="get_state"))
    return replies


@needs_game
def test_protocol_matches_the_golden_snapshot(game):
    client = Client(game)
    try:
        replies = [json.dumps(r, sort_keys=True) for r in session(game, client)]
    finally:
        client.sock.close()
    assert not any(str(game.saves) in r for r in replies)  # the replies hold no machine path
    got = {"replies": [hashlib.sha256(r.encode()).hexdigest() for r in replies]}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        GOLDEN.write_text(json.dumps(got, indent=1) + "\n")
    golden = json.loads(GOLDEN.read_text())
    first = next((i for i, (a, b) in enumerate(zip(got["replies"], golden["replies"])) if a != b), None)
    assert got == golden, f"reply {first} differs: {replies[first] if first is not None else 'count'}"
```

- [ ] **Step 2: Write the golden file**

Run: `make -C port && DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_protocol.py`
Expected: PASS; `tests/golden/protocol.json` exists with about 30-60 hashes.

- [ ] **Step 3: Prove that the snapshot is stable**

Run: `uv run pytest -q tests/test_golden_protocol.py` three times.
Expected: PASS each time. A failure means a reply depends on state outside the sequence (time, a path): find the
field, and fix the sequence (not the code), then repeat Step 2.

- [ ] **Step 4: Commit**

```bash
git add tests/test_golden_protocol.py tests/golden/protocol.json
git commit -m "test: golden snapshot of the control protocol replies"
```

#### Stream A2: golden screen snapshot (R12.7)

Shared files: none (imports helpers from `tests/test_golden_rules.py`, does not change it).

### Task 2: Golden snapshot of the status boxes, the inventory panel, the action and the spell areas

**Files:**
- Create: `tests/test_golden_screens.py`
- Create: `tests/golden/screens.json`

**Interfaces:**
- Consumes: the fixture `game`, `recruit`, `add_second_champion`, `zone_box`, `ZONE_ACTION_ICON_FIRST`,
  `ZONE_SPELL_SYMBOL_BOX_FIRST` of `tests/test_golden_rules.py:21-123`. C: `ui_draw_all_champion_states(uint16_t)`
  (`port/src/ui/ui.h:113`), `ui_draw_champion_damage`, `command_toggle_inventory(int16_t)`
  (`port/src/ui/command.h:108`), `menus_draw_action_damage(int16_t)` (`port/src/ui/ui.h:143`).
- Produces: `tests/golden/screens.json` with the keys `status`, `inventory`, `action`, `spell`; the gate of
  packages G and I. Later tasks rename the C functions that the test calls and the global `fb` (`g_screen`, Task
  87); the hashes do not change.

Facts: `CHAMPION_ATTR_REDRAW_MASK = 0xff80` (`port/src/engine/champion.h:57`); `ui_draw_champion_damage(1, 37)`
and `menus_draw_action_damage(25)` are called by `tests/test_ui.py:240-241`.

- [ ] **Step 1: Write the test**

```python
"""Golden snapshot of the champion screens: status boxes, the inventory panel, the action and spell areas."""

import ctypes
import hashlib
import json
import os

from test_golden_rules import (ZONE_ACTION_ICON_FIRST, ZONE_SPELL_SYMBOL_BOX_FIRST, add_second_champion, game,  # noqa: F401
                               needs_game, recruit, zone_box)
from test_port import ROOT

GOLDEN = ROOT / "tests" / "golden" / "screens.json"
CHAMPION_ATTR_REDRAW_MASK = 0xFF80
WOUND_READY_AND_ACTION_HAND = 0x03


def screen(game):
    """SHA-256 of the frame buffer."""
    return hashlib.sha256(bytes((ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb"))).hexdigest()


def check(name, got):
    """Compares the screens of a scenario with the golden file; DM_UPDATE_GOLDEN writes them."""
    golden = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        golden[name] = got
        GOLDEN.write_text(json.dumps(golden, indent=1, sort_keys=True) + "\n")
    assert got == golden[name]


@needs_game
def test_status_boxes(game):
    recruit(game)
    add_second_champion(game)
    second = game.champions[1]
    second.wounds = WOUND_READY_AND_ACTION_HAND
    second.current_health //= 2
    game.lib.ui_draw_all_champion_states(CHAMPION_ATTR_REDRAW_MASK)
    got = [screen(game)]
    game.lib.ui_draw_champion_damage(1, 37)
    got.append(screen(game))
    check("status", got)


@needs_game
def test_inventory_panel(game):
    recruit(game)
    game.lib.command_toggle_inventory(0)
    game.run()
    check("inventory", [screen(game)])


@needs_game
def test_action_area(game):
    recruit(game)
    game.click_box(zone_box(game, ZONE_ACTION_ICON_FIRST), viewport=False)
    got = [screen(game)]
    game.lib.menus_draw_action_damage(25)
    got.append(screen(game))
    check("action", got)


@needs_game
def test_spell_area(game):
    recruit(game)
    game.champions[game.value("g_magic_caster_champion_index")].current_mana = 100
    got = []
    for symbol in (0, 1):
        game.click_box(zone_box(game, ZONE_SPELL_SYMBOL_BOX_FIRST + symbol), viewport=False)
        got.append(screen(game))
    check("spell", got)
```

- [ ] **Step 2: Write the golden file**

Run: `make -C port && DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_screens.py`
Expected: 4 passed; `tests/golden/screens.json` has 4 keys.

- [ ] **Step 3: Prove that the snapshot is stable**

Run: `uv run pytest -q tests/test_golden_screens.py` twice. Expected: 4 passed each time.

- [ ] **Step 4: Commit**

```bash
git add tests/test_golden_screens.py tests/golden/screens.json
git commit -m "test: golden snapshot of the champion screens"
```

#### Stream A3: rule scenarios for the rules that leave ui (R8.3)

Shared files: none (`tests/test_golden_rules.py` and `tests/golden/rules.json` belong to this stream only).

### Task 3: Golden scenarios for every rule that R8.3 moves out of ui

**Files:**
- Modify: `tests/test_golden_rules.py` (new helpers and 8 scenarios at the end)
- Modify: `tests/golden/rules.json` (new keys only)

**Interfaces:**
- Consumes: the helpers of `tests/test_golden_rules.py` (`game`, `snapshot`, `check`, `ticks`, `recruit`,
  `place_party`, `new_object`); C: `projectile_fluxcage_action(int16_t x, int16_t y)`
  (`port/src/engine/projectile.c:585`), `startend_run_fuse_sequence(void)` (`port/src/ui/startend.h:44`),
  `automap_mark_seen(int16_t map, int16_t x, int16_t y, int16_t dir)` (`port/src/ui/automap.h:19`),
  `explored_reset(void)`, `explored_save(uint8_t **, size_t *)` (`port/src/engine/explored.h`),
  `champion_get_maximum_load(const Champion *)` (`port/src/engine/champion.h:97`).
- Produces: the keys `grab and drop`, `grab under a creature`, `alcove`, `imaginary wall`, `movement blocked`,
  `fuse`, `load`, `visibility` in `tests/golden/rules.json`: the gate of Task 88 (stream G4). G4 may change the C
  calls of these scenarios (the moved functions get engine names); it must not change the keys or the values.

Rules and their source (each rule gets one scenario; the view copy of the level change is covered by
`tests/golden/view.json`, which draws every square with its ceiling pits):

| Rule (R8.3 target) | Source today | Scenario |
|---|---|---|
| `inventory_grab_object`, `inventory_drop_object` | `port/src/ui/command.c:407-449` | `grab and drop`, `grab under a creature`, `alcove` |
| `map_is_imaginary_wall` | `port/src/ui/command.c:542-543` | `imaginary wall` |
| `move_is_blocked` | `port/src/ui/command.c:793-802` | `movement blocked` |
| `game_prepare_fuse_lord`, `map_remove_fluxcages`, `group_delete_all_except` | `port/src/ui/startend.c:431-443, 495-501, 503-543` | `fuse` |
| `champion_get_load_level` | `port/src/ui/ui_champion.c:250-256` | `load` |
| `explored_mark_visible` | `port/src/ui/automap.c:24-90` | `visibility` |
| `map_get_level_change_location` (view copy) | `port/src/ui/view.c:1421-1435` | `tests/golden/view.json` (no new scenario) |

Values used by the scenarios: `ZONE_DROP_CELL_FIRST 740` and `ZONE_THROW_LEFT 745` (`port/src/data/ids.h:259,95`);
`VIEW_CELL_FRONT_FIRST = 2` (`port/src/ui/command.c:35`); `VIEW_CLICK_BOX_ALCOVE = 4`, `VIEW_CLICK_BOX_WALL = 5`
(`port/src/ui/view.h:16`); square types `SQUARE_WALL 0`, `SQUARE_CORRIDOR 1`, `SQUARE_FAKE_WALL 6`
(`port/src/engine/map.h:19`, bits 5-7 of the square byte); `FAKE_WALL_IMAGINARY 1`, `FAKE_WALL_OPEN 4`
(`port/src/engine/map.h:26`); `KEY_UP 0x4c00` (`port/src/ui/command.h:60`); `CHAMPION_ATTR_LOAD 0x0200`
(`port/src/engine/champion.h:50`); `g_dungeon_view_palette_index` 0 (bright) to 5 (`port/src/engine/group.h:213`),
4 and 5 cut the visible rows (`port/src/ui/automap.c:43-54`). The fuse spot search is the one of
`tests/test_save.py:340-366`.

Assumption: the drop zones and the pile boxes of `g_dungeon_view_clickable_boxes` give the click results that the
asserts state (an object dropped into view cell 0 shows a pile box 0 after one `game.run()`). Step 2 checks it;
if an assert fails, change the scenario (another cell, another spot), never the code.

- [ ] **Step 1: Add the helpers and the scenarios at the end of `tests/test_golden_rules.py`**

```python
# Scenarios of the rules that leave ui (PRD R8.3): each one pins a rule before it moves into the engine.
CORRIDOR, FAKE_WALL, WALL = 1, 6, 0
FAKE_WALL_IMAGINARY, FAKE_WALL_OPEN = 1, 4
END, NONE = 0xFFFE, 0xFFFF
VIEW_CELL_FRONT_FIRST, CLICK_BOX_ALCOVE, CLICK_BOX_WALL = 2, 4, 5
ZONE_DROP_CELL_FIRST, ZONE_THROW_LEFT = 740, 745
KEY_UP = 0x4C00
CHAMPION_ATTR_LOAD = 0x0200
STEP = ((0, -1), (1, 0), (0, 1), (-1, 0))  # (dx, dy) of each direction: north, east, south, west
MAP_COUNT = 14
VISIBILITY_MAPS = (0, 1, 2, 3)
LIGHTS = (0, 4, 5)  # g_dungeon_view_palette_index: bright, one row less, two rows less


def square_type(lib, x, y):
    return lib.map_get_square(x, y) >> 5


def is_free(lib, x, y):
    """A corridor square with no thing and no group."""
    return square_type(lib, x, y) == CORRIDOR and lib.map_get_square_first_thing(x, y) == END


def find_spot(game, maps, ahead_ok):
    """The first party place (map, x, y, direction) in scan order on a free corridor whose square ahead passes ahead_ok."""
    lib = game.lib
    for m in maps:
        lib.map_set_current_map_and_party_map(m)
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        for y in range(height):
            for x in range(width):
                for d, (dx, dy) in enumerate(STEP):
                    ax, ay = x + dx, y + dy
                    if is_free(lib, x, y) and 0 <= ax < width and 0 <= ay < height and ahead_ok(ax, ay):
                        return m, x, y, d
    raise AssertionError("no spot")


def hold(game, thing):
    game.lib.inventory_put_object_in_leader_hand(thing)
    assert game.value("g_leader_hand_object", u16) == thing


@needs_game
def test_grab_and_drop(game):
    lib = game.lib
    recruit(game)
    place_party(game, *find_spot(game, [0], lambda x, y: is_free(lib, x, y)))
    flask = new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    got = []
    for cell in (0, VIEW_CELL_FRONT_FIRST):  # a cell of the party square, then a cell of the square ahead
        game.click_zone(ZONE_DROP_CELL_FIRST + cell)
        assert game.value("g_leader_hand_object", u16) == NONE
        got.append(snapshot(game))
        game.click_box(game.boxes[cell])
        assert game.value("g_leader_hand_object", u16) == flask
        got.append(snapshot(game))
    check("grab and drop", got)


@needs_game
def test_grab_under_a_creature(game):
    lib = game.lib
    recruit(game)
    m, x, y, d = find_spot(game, [ATTACKED[0]], lambda x, y: is_free(lib, x, y))
    place_party(game, m, x, y, d)
    hold(game, new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2))
    game.click_zone(ZONE_DROP_CELL_FIRST + VIEW_CELL_FRONT_FIRST)
    lib.group_get_generated(CREATURE_SKELETON, 1, 0, 3, x + STEP[d][0], y + STEP[d][1])
    got = []
    for cell in (VIEW_CELL_FRONT_FIRST, VIEW_CELL_FRONT_FIRST + 1):  # a creature in the cell protects the pile
        game.click_box(game.boxes[cell])
        got.append({"hand": game.value("g_leader_hand_object", u16), **snapshot(game)})
    check("grab under a creature", got)


@needs_game
def test_alcove(game):
    lib = game.lib
    recruit(game)
    spot = None
    lib.map_set_current_map_and_party_map(0)
    for m, x, y, d in [(0, x, y, d) for y in range(game.value("g_current_map_height"))
                       for x in range(game.value("g_current_map_width")) for d in range(4)]:
        ax, ay = x + STEP[d][0], y + STEP[d][1]
        if not is_free(lib, x, y) or square_type(lib, ax, ay) != WALL:
            continue
        place_party(game, m, x, y, d)
        if game.value("g_facing_alcove") and not game.value("g_facing_vi_altar"):
            spot = m, x, y, d
            break
    assert spot, "no alcove on map 0"
    flask = new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2)
    hold(game, flask)
    game.click_box(game.boxes[CLICK_BOX_WALL])
    assert game.value("g_leader_hand_object", u16) == NONE
    got = [snapshot(game)]
    game.click_box(game.boxes[CLICK_BOX_ALCOVE])
    assert game.value("g_leader_hand_object", u16) == flask
    got.append(snapshot(game))
    check("alcove", got)


@needs_game
def test_imaginary_wall(game):
    lib = game.lib
    recruit(game)

    def imaginary(x, y):
        square = lib.map_get_square(x, y)
        return square >> 5 == FAKE_WALL and square & FAKE_WALL_IMAGINARY and not square & FAKE_WALL_OPEN

    place_party(game, *find_spot(game, range(MAP_COUNT), imaginary))
    game.click_zone(ZONE_THROW_LEFT)  # an empty hand goes into the imaginary wall
    got = [snapshot(game)]
    hold(game, new_object(game, THING_POTION, POTION_EMPTY_FLASK, byte=2))
    game.click_zone(ZONE_DROP_CELL_FIRST + VIEW_CELL_FRONT_FIRST)  # an object falls through the imaginary wall
    assert game.value("g_leader_hand_object", u16) == NONE
    got.append(snapshot(game))
    check("imaginary wall", got)


@needs_game
def test_movement_blocked(game):
    lib = game.lib
    recruit(game)
    m, x, y, d = find_spot(game, [0], lambda x, y: is_free(lib, x, y))
    place_party(game, m, x, y, d)
    got = []
    for blocker in ("g_disabled_movement_ticks", "g_projectile_disabled_movement_ticks"):
        start = game.position()
        c16.in_dll(lib, blocker).value = 3
        c16.in_dll(lib, "g_last_projectile_disabled_movement_direction").value = start[2]
        game.key(KEY_UP)
        game.run(1)
        got.append({"position": game.position(), **snapshot(game)})
        assert game.until(lambda: game.position()[:2] != start[:2])
        got.append({"position": game.position(), **snapshot(game)})
        game.face((start[2] + 2) % 4)
    check("movement blocked", got)


@needs_game
def test_fuse_sequence(game):
    lib = game.lib
    recruit(game)
    lib.map_get_square_group.restype = u16
    spot = None
    for level in range(1, MAP_COUNT):
        lib.group_remove_all_active_groups()
        lib.map_set_current_map_and_party_map(level)
        lib.group_add_all_active_groups()
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        groups = {(x, y) for x in range(width) for y in range(height) if lib.map_get_square_group(x, y) != END}
        spot = next(((x - STEP[d][0], y - STEP[d][1], d, (x, y)) for x, y in sorted(groups) for d in range(4)
                     if 0 <= x - STEP[d][0] < width and 0 <= y - STEP[d][1] < height
                     and square_type(lib, x - STEP[d][0], y - STEP[d][1]) == CORRIDOR
                     and (x - STEP[d][0], y - STEP[d][1]) not in groups), None)
        if spot:
            break
    assert spot, "no group next to a corridor"
    px, py, direction, lord_square = spot
    for name, v in (("g_party_map_x", px), ("g_party_map_y", py), ("g_party_direction", direction)):
        c16.in_dll(lib, name).value = v
    lib.projectile_fluxcage_action(*lord_square)  # the sequence removes the fluxcage of the lord square
    lib.startend_run_fuse_sequence()
    assert game.value("g_game_won")
    check("fuse", [snapshot(game)])


@needs_game
def test_load_colors(game):
    lib = game.lib
    recruit(game)
    lib.champion_get_maximum_load.restype = u16
    champion = game.champions[0]
    maximum = lib.champion_get_maximum_load(ctypes.byref(champion))
    lib.command_toggle_inventory(0)
    game.run()
    got = []
    for load in (maximum // 2, maximum * 7 // 8, maximum + 1):  # light gray, yellow, red
        champion.load = load
        champion.attributes |= CHAMPION_ATTR_LOAD
        lib.ui_draw_champion_state(0)
        got.append(snapshot(game))
    check("load", got)


@needs_game
def test_visibility(game):
    lib = game.lib
    for name in ("dungeon_map_width", "dungeon_map_height"):
        getattr(lib, name).restype = c16
    lib.dungeon_get_square.restype = ctypes.c_uint8
    lib.explored_save.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    palette = c16.in_dll(lib, "g_dungeon_view_palette_index")
    digest = hashlib.sha256()
    for m in VISIBILITY_MAPS:
        for y in range(lib.dungeon_map_height(m)):
            for x in range(lib.dungeon_map_width(m)):
                if lib.dungeon_get_square(m, x, y) >> 5 != CORRIDOR:
                    continue
                for d in range(4):
                    for light in LIGHTS:
                        palette.value = light
                        lib.explored_reset()
                        lib.automap_mark_seen(m, x, y, d)
                        buf, size = ctypes.c_void_p(), ctypes.c_size_t()
                        assert lib.explored_save(ctypes.byref(buf), ctypes.byref(size)) == 0
                        digest.update(ctypes.string_at(buf, size.value))
                        lib.free(buf)
    check("visibility", [digest.hexdigest()])
```

- [ ] **Step 2: Write the new keys**

Run: `make -C port && DM_UPDATE_GOLDEN=1 uv run pytest -q tests/test_golden_rules.py`
Expected: all pass. If an assert of a new scenario fails, change the scenario as the assumption above says.
If `test_visibility` takes more than 10 s, keep only maps 0 and 1 in `VISIBILITY_MAPS`.

- [ ] **Step 3: Check that the old keys did not change**

Run: `git diff tests/golden/rules.json`
Expected: only added keys; no changed or removed line of the 11 existing keys (`action list` ... `vi altar`).

- [ ] **Step 4: Prove that the snapshot is stable**

Run: `uv run pytest -q tests/test_golden_rules.py` twice. Expected: all pass each time.

- [ ] **Step 5: Commit**

```bash
git add tests/test_golden_rules.py tests/golden/rules.json
git commit -m "test: golden scenarios for the rules that leave the user interface"
```

## Package B: tree, root Makefile, include paths, test loader (Epic 2)

One stream, five tasks in order. Every task touches files of all layers or all tests, so no task runs in
parallel. The package runs in the main checkout on the feature branch.

Facts for the package:

- The build today: `port/Makefile:1-48`: one clang command per target, include path `-Isrc/<layer>` for each
  layer (`:5-6`), no `-Werror`; the game excludes `control_win.c`, the library also `main.c` and `platform.c`
  (`:7`); Windows excludes `control.c` (`:31`) and fetches SDL MinGW 3.4.18 into `build/sdl-mingw/` (`:28-41`).
- Header names are unique over the four layers (`ls port/src/*/*.h | xargs -n1 basename | sort | uniq -d` prints
  nothing), so a script can map each include to its layer.
- Only `main.c` and `platform.c` include SDL; `scale.c`, `control.c`, `state_json.c` do not
  (`port/src/platform/*.c` heads), so only those two objects need the SDL include path.
- `ui/music.c:9-18` includes `dr_mp3.h` (dr_mp3 v0.7.3, "public domain or MIT-0", `port/src/ui/dr_mp3.h:1-3,
  5326-5370`) inside a `#pragma clang diagnostic ignored "-Weverything"` block; GCC ignores the pragma.
- The new flags give 5 warnings, all `-Wimplicit-fallthrough` (clang `-fsyntax-only` run on commit 72e24b5):
  `port/src/engine/action.c:550, 570`, `port/src/engine/sensor.c:201`, `port/src/ui/view.c:386, 1942`. Each site
  already has a `/* fall through */` comment; clang does not accept a comment in C, only
  `__attribute__((fallthrough));` (GCC accepts both). The MinGW GCC 16.2 `-fsyntax-only` run with the new flags
  gives 0 warnings; optimizer warnings at `-O2` are not checked yet (Task 5 builds it).
- Tests: 31 `subprocess.run(["make", "-C", "port", ...])` calls in 26 files; 12 modules compile the library with
  clang (`tests/test_control.py:31-36` is the pattern; `game_sources()` and `PORT_INCLUDES` in
  `tests/conftest.py:10-16`); `tests/test_tables.py:63-70` compiles a data-only library; 19 modules load
  `build/libdm.dylib` by its path; `tests/test_golden_rules.py:44-47` and `tests/test_golden_save.py:45-48` copy
  it first. Five tests start `build/dm` (`tests/test_gameloop.py:227-231`, `tests/test_modes.py:191, 418, 463,
  671`, `tests/test_csb_adventure.py:278-282`, `tests/test_csb_engine.py:189-193`, `tests/test_csb_tools.py:19,
  59`), so the tests need the game too, not only the library.
- `tests/test_standard.py:10-11, 28-33, 292-311, 316-322` hold the source root, the vendored file name, the layer
  check by header base name, and the format check.
- Paths `port/...` appear in `.claude/agents/dm-coordinator.md`, `CLAUDE.md`, `README.md`, 15 pages of `docs/`
  (`git grep -l -E "port/src|make -C port|port/Makefile" -- . ':!docs/superpowers'`), in two test docstrings
  (`tests/test_actions.py:1`, `tests/test_creatures.py:1`) and in `tools/make-dist.sh:5`,
  `tools/make-dist-win.sh:5-6`. `CHANGELOG.md` keeps its history.
- `.git-blame-ignore-revs` exists (commit `ad0435c`) and lists the format commit `77f7a30`.

Rulings of this package (record them in the ledger):

- Fall-through: the five sites get `__attribute__((fallthrough));` on the line before the next label, and the
  existing `/* fall through */` comment stays. PRD R2.2 says "a `/* fall through */` comment", which clang does
  not accept in Task 19.
- `third_party/` goes on the include path as `-isystem third_party`, not `-Ithird_party`: system headers give no
  warnings in clang and GCC, so `-Werror` does not depend on dr_mp3. The include still names the folder
  (`<dr_mp3/dr_mp3.h>`).
- `conftest.py` builds `make all` when SDL3 is installed and `make lib` when it is not: five tests start
  `build/dm`. PRD R2.4 names `make lib` only.
- `addopts` loses `-n auto` and `pythonpath = ["tests"]` comes in Task 7; `--import-mode=importlib` comes with
  the test folders in Task 17, because test modules still import each other until Task 16.

#### Stream B1: tree and build

Shared files: all of `src/`, `tests/`, `docs/` (single stream; no parallel work in package B).

### Task 4: Move the tools into their folders

**Files:**
- Move: `tools/decompile-all.sh` → `tools/ghidra/decompile-all.sh`
- Move: `tools/make-dist.sh`, `tools/make-dist-win.sh` → `tools/dist/`
- Move: `tools/platforms/` → `tools/research/platforms/`
- Create: `tools/research/README.md`
- Modify: `pyproject.toml` (group `research`), `README.md:50,72-74,95,97`, `docs/csb-engine-diff.md` (no change:
  `tools/csb_match.py` stays until stream C3), `tools/research/platforms/dm2sounds.py:105`

**Interfaces:**
- Produces: `tools/dist/make-dist.sh`, `tools/dist/make-dist-win.sh` (the `dist`, `dist-win` targets of Task 5
  call them); `tools/ghidra/decompile-all.sh`.

- [ ] **Step 1: Move the files**

```bash
mkdir -p tools/dist tools/research
git mv tools/decompile-all.sh tools/ghidra/decompile-all.sh
git mv tools/make-dist.sh tools/dist/make-dist.sh
git mv tools/make-dist-win.sh tools/dist/make-dist-win.sh
git mv tools/platforms tools/research/platforms
```

- [ ] **Step 2: Fix the root of the moved scripts**

In `tools/ghidra/decompile-all.sh:5`, `tools/dist/make-dist.sh:4` and `tools/dist/make-dist-win.sh:4`, replace
`cd "$(dirname "$0")/.."` with `cd "$(dirname "$0")/../.."`. In both dist scripts keep `make -C port` for now
(Task 5 changes it with the Makefile).

- [ ] **Step 3: Write `tools/research/README.md`**

```markdown
# Research scripts

Scripts that read the files of the other versions of Dungeon Master (DOS, Atari ST, Apple IIGS, FM-Towns,
Super NES, Dungeon Master II): extract and compare sounds and music, build `reference/extracted/music_map.json`.

They are not maintained and not tested. They read `reference/extracted/`. Some scripts need `capstone` and
`numpy`: `uv sync --group research`.
```

- [ ] **Step 4: Add the `research` dependency group**

Run: `uv add --group research capstone numpy`
Expected: `pyproject.toml` has `research = ["capstone>=...", "numpy>=..."]` under `[dependency-groups]`;
`uv.lock` changes. Assumption: the two packages have wheels for Python 3.14 on macOS arm64; if `uv add` fails,
stop and report.

- [ ] **Step 5: Fix the paths in the docs and the scripts**

Run:
```bash
sed -i '' -e 's#tools/decompile-all\.sh#tools/ghidra/decompile-all.sh#g' -e 's#tools/make-dist#tools/dist/make-dist#g' \
  -e 's#tools/platforms/#tools/research/platforms/#g' README.md tools/research/platforms/dm2sounds.py
git grep -n -E "tools/(decompile-all|make-dist|platforms/)" -- . ':!docs/superpowers' ':!CHANGELOG.md'
```
Expected: the grep finds only the new paths.

- [ ] **Step 6: Check**

Run: `bash -n tools/ghidra/decompile-all.sh tools/dist/make-dist.sh tools/dist/make-dist-win.sh && uv run pytest -q tests/test_ghidra.py`
Expected: no syntax error; the ghidra test passes or skips (Ghidra missing).

- [ ] **Step 7: Commit**

```bash
git add -A tools pyproject.toml uv.lock README.md
git commit -m "refactor: move the dist, Ghidra and research scripts into their tool folders"
```

### Task 5: Move the sources to `src/`, the Makefile to the root, and name the layer in every include

**Files:**
- Move: `port/src/<layer>/*` → `src/<layer>/*` (four layers)
- Move: `port/src/ui/dr_mp3.h` → `third_party/dr_mp3/dr_mp3.h`
- Create: `third_party/dr_mp3/README.md`, `Makefile`
- Delete: `port/Makefile`, `port/`
- Modify: every `src/*/*.[ch]` (includes, by script); `src/engine/action.c:550,570`, `src/engine/sensor.c:201`,
  `src/ui/view.c:386,1942` (fall-through); `src/ui/music.c:14` (dr_mp3 include);
  `tests/conftest.py:10-16,86`; the 31 `make -C port` calls in `tests/`; `tests/test_standard.py:1,10-12,28-29,
  292-311`; `tests/standard_allowlist.txt:2`; `tools/dist/make-dist.sh:5`, `tools/dist/make-dist-win.sh:5-6`

**Interfaces:**
- Consumes: `tools/dist/*.sh` (Task 4).
- Produces: the root `Makefile` with the targets `all lib windows test check format dist dist-win play clean`;
  the variables `SOURCES = $(wildcard src/*/*.c)` (a new layer folder such as `src/control/` joins without a
  Makefile change), `SDL_SOURCES`, `WINDOWS_ONLY`, `POSIX_ONLY` (package H renames `control.c` and
  `control_win.c` and changes these two lines); the include form `#include "<layer>/<file>.h"`;
  `tests/conftest.py` `ROOT`; `SANITIZE=1` (stream I2 documents it in `docs/development.md`).

- [ ] **Step 1: Move the tree**

```bash
mkdir -p src third_party/dr_mp3
for layer in data engine ui platform; do git mv port/src/$layer src/$layer; done
git mv src/ui/dr_mp3.h third_party/dr_mp3/dr_mp3.h
git rm -q port/Makefile
test -z "$(ls -A port 2>/dev/null)" && rmdir port/src port 2>/dev/null; ls port 2>/dev/null
```
Expected: `ls port` prints nothing (the folder is gone).

- [ ] **Step 2: Rewrite the includes**

Save as `$SCRATCH/include_layers.py` (the SDD workspace, not the repository) and run
`uv run python $SCRATCH/include_layers.py`:

```python
"""Rewrites every project include of src/ to name its layer: #include "game.h" -> #include "engine/game.h"."""

import re
import sys
from pathlib import Path

SRC = Path("src")
HOME = {p.name: f"{p.parent.name}/{p.name}" for p in SRC.glob("*/*.h")}
INCLUDE = re.compile(r'^(\s*#\s*include\s+)"([\w.]+)"', re.M)


def fix(path, match):
    name = match.group(2)
    if name == "dr_mp3.h":
        return f"{match.group(1)}<dr_mp3/dr_mp3.h>"
    if name not in HOME:
        sys.exit(f"{path}: unknown include {name}")
    return f'{match.group(1)}"{HOME[name]}"'


for path in sorted(SRC.glob("*/*.[ch]")):
    text = path.read_text()
    new = INCLUDE.sub(lambda m: fix(path, m), text)
    if new != text:
        path.write_text(new)
```

Run: `git grep -n -E '#\s*include\s+"[^/"]+"' -- src`
Expected: nothing (every project include names its layer).

- [ ] **Step 3: Annotate the five fall-through cases**

At each site, put `__attribute__((fallthrough));` on its own line directly before the next `case` label and
keep the comment. Example for `src/engine/action.c:549`:

```c
        /* no closed door: a melee attack */
        /* fall through */
        __attribute__((fallthrough));
    case ACTION_PUNCH:
```

For `src/ui/view.c:1942` (`draw_square_pit(s, a->state); /* fall through */`), keep the comment on that line and
add `__attribute__((fallthrough));` on the next line.

- [ ] **Step 4: Write `third_party/dr_mp3/README.md`**

```markdown
# dr_mp3

The MP3 decoder of `src/ui/music.c`: `dr_mp3.h` v0.7.3 (2026-01-17) by David Reid,
https://github.com/mackron/dr_libs. License: public domain or MIT-0, as the end of `dr_mp3.h` states.
Vendored without change; outside the format check and the standard checks.
```

- [ ] **Step 5: Write the root `Makefile`**

```make
# The Dungeon Master port: the game, the test library, the Windows build, the checks.
CC          = clang
BASE_CFLAGS = -std=c11 -Wall -Wextra -Werror -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla \
              -Wimplicit-fallthrough -O2 -g
CFLAGS      = $(BASE_CFLAGS)
CPPFLAGS    = -Isrc -isystem third_party -MMD -MP
LDFLAGS     =
ifeq ($(SANITIZE),1)
CFLAGS  += -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

# SDL3 from Homebrew. Recursively expanded: only the targets that compile or link SDL code run brew.
SDL_PREFIX ?= $(shell brew --prefix sdl3)

B   = build
OBJ = $(B)/obj

SOURCES      = $(wildcard src/*/*.c)
SDL_SOURCES  = src/platform/main.c src/platform/platform.c
WINDOWS_ONLY = src/platform/control_win.c
POSIX_ONLY   = src/platform/control.c
GAME_SOURCES = $(filter-out $(WINDOWS_ONLY),$(SOURCES))
LIB_SOURCES  = $(filter-out $(SDL_SOURCES) $(WINDOWS_ONLY),$(SOURCES))
WIN_SOURCES  = $(filter-out $(POSIX_ONLY),$(SOURCES))
C_FILES      = $(wildcard src/*/*.[ch])

objects = $(patsubst src/%.c,$(1)/%.o,$(2))

all: $(B)/dm $(B)/libdm.dylib

lib: $(B)/libdm.dylib

$(OBJ)/%.o: src/%.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(call objects,$(OBJ),$(SDL_SOURCES)): CPPFLAGS += -I$(SDL_PREFIX)/include

$(B)/dm: $(call objects,$(OBJ),$(GAME_SOURCES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(SDL_PREFIX)/lib -lSDL3

$(B)/libdm.dylib: $(call objects,$(OBJ),$(LIB_SOURCES))
	$(CC) $(CFLAGS) $(LDFLAGS) -dynamiclib -o $@ $^

# Windows: cross-build with MinGW-w64 (brew install mingw-w64) and the SDL3 MinGW package, fetched into build/.
# Windows has no control socket: control_win.c replaces control.c.
WIN_CC            = x86_64-w64-mingw32-gcc
WIN_OBJ           = $(B)/obj-win
SDL_MINGW_VERSION = 3.4.18
SDL_MINGW         = $(B)/sdl-mingw/SDL3-$(SDL_MINGW_VERSION)/x86_64-w64-mingw32

windows: $(B)/dm.exe

$(WIN_OBJ)/%.o: src/%.c Makefile | $(SDL_MINGW)
	@mkdir -p $(@D)
	$(WIN_CC) $(CPPFLAGS) -I$(SDL_MINGW)/include $(BASE_CFLAGS) -c -o $@ $<

$(B)/dm.exe: $(call objects,$(WIN_OBJ),$(WIN_SOURCES))
	$(WIN_CC) $(BASE_CFLAGS) -o $@ $^ -L$(SDL_MINGW)/lib -lSDL3

$(SDL_MINGW):
	@mkdir -p $(B)/sdl-mingw
	curl -sfL https://github.com/libsdl-org/SDL/releases/download/release-$(SDL_MINGW_VERSION)/SDL3-devel-$(SDL_MINGW_VERSION)-mingw.tar.gz | tar xz -C $(B)/sdl-mingw

test:
	uv run pytest -q -n auto

format:
	uv run clang-format -i $(C_FILES)

check:
	uv run clang-format --dry-run --Werror $(C_FILES)
	uv run pytest -q tests/test_standard.py
	$(MAKE) test

dist:
	tools/dist/make-dist.sh

dist-win:
	tools/dist/make-dist-win.sh

# The personal play folder: the latest game; the saves there stay. Run: playthroughs/local/dm --data game --saves playthroughs/local
PLAY_DIR ?= playthroughs/local
play: $(B)/dm
	@mkdir -p $(PLAY_DIR)
	cp $(B)/dm $(PLAY_DIR)/dm.new && mv $(PLAY_DIR)/dm.new $(PLAY_DIR)/dm # a new file: a running game keeps the old one

clean:
	rm -rf $(B)

-include $(wildcard $(OBJ)/*/*.d $(WIN_OBJ)/*/*.d)

.PHONY: all lib windows test check format dist dist-win play clean
```

Stream C1 changes the `check` line `tests/test_standard.py` to `tests/standard`; stream C2 adds `ruff`.

- [ ] **Step 6: Follow the move in the scripts and the tests**

Run:
```bash
sed -i '' -e 's#make -C port windows#make windows#' -e 's#make -C port#make#' -e "s#' port/Makefile#' Makefile#" \
  tools/dist/make-dist.sh tools/dist/make-dist-win.sh
sed -i '' -E 's#subprocess\.run\(\["make", "-C", "port"(, "\.\./build/libdm\.dylib")?\]#subprocess.run(["make"]#' tests/*.py
sed -i '' -e 's#port/src/#src/#' tests/test_actions.py tests/test_creatures.py
git grep -n -E '"-C", "port"|port/' -- tests tools/dist
```
Expected: the last grep prints nothing. In `tests/conftest.py`, set
`PORT_SRC = Path(__file__).resolve().parent.parent / "src"` and
`PORT_INCLUDES = ["-Isrc", "-isystem", "third_party"]` with absolute paths
(`[f"-I{PORT_SRC}", "-isystem", str(PORT_SRC.parent / "third_party")]`), and in `pytest_configure` run
`["make"]` with `cwd=PORT_SRC.parent`. (Task 7 replaces both with the loader; this step keeps every commit
green.)

- [ ] **Step 7: Follow the move in the standard check**

In `tests/test_standard.py`: line 1 docstring `src/`, `SRC = ROOT / "src"`, the allowlist comment line of
`tests/standard_allowlist.txt:2` says `src/`; remove `VENDORED` and its use in `sources()` (dr_mp3 is no longer
under `src/`). Replace the body of the include loop of `test_layers` (`:299-308`) so that the layer comes from
the include path, and an include without a layer fails:

```python
        for header in INCLUDE.findall(text):
            key = f"{rel(path)}:{header}"
            target = header.split("/", 1)[0] if "/" in header else None
            if target not in LAYERS:
                found.append(f"layer {key}  | the include does not name a layer")
            elif LAYERS.index(target) > LAYERS.index(layer) and ("layer", key) not in ok:
                found.append(f"layer {key}  | {layer} includes {target}")
```

`home` (`:298`) goes.

- [ ] **Step 8: Build and check the build rules**

Run:
```bash
make 2>&1 | grep -c -E "warning|error"     # Expected: 0
make 2>&1 | tail -1                          # Expected: make: Nothing to be done for `all'.
touch src/engine/random.c && make 2>&1 | grep -c -E "^clang .* -c "   # Expected: 1 (one compile), then two links
touch src/engine/random.h && make 2>&1 | grep -c -E "^clang .* -c "   # Expected: the number of files that include random.h (git grep -l '"engine/random.h"' -- src | grep -c '\.c$')
env PATH=/usr/bin:/bin make -n clean 2>&1 | grep -c brew               # Expected: 0 (no brew call outside the SDL targets)
make windows 2>&1 | grep -c -E "warning|error"                         # Expected: 0
```
If GCC reports a warning at `-O2` (only `-fsyntax-only` was checked), fix the code with the smallest change that
keeps the behavior, and report it; if no such change exists, stop and ask.

- [ ] **Step 9: Run the tests of the changed code**

Run: `uv run pytest -q tests/test_standard.py tests/test_port.py tests/test_control.py tests/test_tables.py tests/test_gameloop.py`
Expected: pass (the format check passes: the include rewrite keeps the line lengths under 120).

- [ ] **Step 10: Commit**

```bash
git add -A src third_party Makefile port tests tools/dist
git commit -m "refactor: move the port to src/ with a root Makefile and layer-named includes"
```

### Task 6: Sort the includes with clang-format 23 from PyPI

**Files:**
- Modify: `.clang-format`, `pyproject.toml`, `uv.lock`, `tests/test_standard.py:316-322`, every `src/*/*.[ch]`
  (format), `.git-blame-ignore-revs`

**Interfaces:**
- Consumes: the layer-named includes (Task 5).
- Produces: `uv run clang-format` (version 23.1.x) for `make format`, `make check` and the format check; the
  include order of standard rule 2.3.

- [ ] **Step 1: Add clang-format to the dev group**

Run: `uv add --dev "clang-format>=23.1,<24" && uv run clang-format --version`
Expected: `clang-format version 23.1.x`. Assumption: PyPI has a 23.1 wheel for macOS arm64; if not, stop and
report.

- [ ] **Step 2: Turn on the include sort in `.clang-format`**

Replace `SortIncludes: Never` with:

```yaml
SortIncludes: CaseSensitive
IncludeBlocks: Merge
IncludeCategories:
  - Regex: '^"data/'
    Priority: 1
  - Regex: '^"engine/'
    Priority: 2
  - Regex: '^"ui/'
    Priority: 3
  - Regex: '^"control/'
    Priority: 4
  - Regex: '^"platform/'
    Priority: 5
  - Regex: '^<(dr_mp3|SDL3)/'
    Priority: 6
  - Regex: '^<'
    Priority: 7
```

clang-format gives the own header priority 0 (it matches the file stem without the folder), so it stays first.

- [ ] **Step 3: Point the format check at the virtual environment**

In `tests/test_standard.py:318-319`, keep `shutil.which("clang-format")` (under `uv run` the `.venv/bin` comes
first) and change the message to `"clang-format is missing: uv sync"`.

- [ ] **Step 4: Format**

Run: `make format && git diff --stat | tail -1`
Expected: many files changed (include blocks only). Check one file: `git diff src/ui/gameloop.c | head -60`
shows only moved include lines.

- [ ] **Step 5: Build**

Run: `make 2>&1 | grep -c -E "warning|error"`
Expected: 0. A header that needs an include it does not have (it compiled only because of the old order) gets
that include in the header (standard rule 2.2); report each one.

- [ ] **Step 6: Run the tests of the changed code**

Run: `uv run pytest -q tests/test_standard.py tests/test_golden_rules.py tests/test_golden_view.py tests/test_golden_save.py`
Expected: pass.

- [ ] **Step 7: Commit, then list the commit in the blame ignore file**

```bash
git add -A .clang-format pyproject.toml uv.lock tests/test_standard.py src
git commit -m "style: sort the includes by layer with clang-format 23"
git rev-parse HEAD >> .git-blame-ignore-revs
git add .git-blame-ignore-revs
git commit -m "build: ignore the include sort commit in git blame"
```

### Task 7: Load a private copy of the library in every test module; no test compiles the library

**Files:**
- Create: `tests/dmtest/__init__.py`, `tests/dmtest/lib.py`, `tests/test_dmtest.py`
- Modify: `tests/conftest.py` (remove `PORT_SRC`, `PORT_INCLUDES`, `game_sources`; new `pytest_configure`), the
  31 `make` calls and the 31 library loads in `tests/*.py` (by script, then by hand for 14 fixtures),
  `pyproject.toml` (`addopts`)

**Interfaces:**
- Consumes: the root `Makefile` (`make all`, `make lib`).
- Produces: `tests/dmtest/lib.py`: `load_lib() -> ctypes.CDLL`; `tests/conftest.py` builds once. Stream C2 moves
  the remaining helpers into `tests/dmtest/`.

Fact: `pytest_configure` without `workerinput` runs once in the controller process before the xdist workers
start (`tests/conftest.py:83-86`, the current build hook). A copied ad-hoc signed dylib loads as a separate
image (`tests/test_golden_rules.py:44-47`, in use today).

- [ ] **Step 1: Measure the suite before the change**

Run: `time uv run pytest -q -n auto 2>&1 | tail -3`
Expected: about 655 passed in about 13 s wall. Write the time into the ledger.

- [ ] **Step 2: Write the loader**

`tests/dmtest/__init__.py`:

```python
"""Shared helpers of the tests: the library loader, and later the constants, the game harness and the markers."""
```

`tests/dmtest/lib.py`:

```python
"""The test library: a private copy of build/libdm.dylib per load, so each test module has its own C globals."""

import ctypes
import shutil
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIBRARY = ROOT / "build" / "libdm.dylib"


def load_lib():
    """Loads a new private copy of the library; conftest.py builds the library before the tests start."""
    folder = Path(tempfile.mkdtemp(prefix="libdm-"))
    copy = folder / LIBRARY.name
    shutil.copy2(LIBRARY, copy)
    lib = ctypes.CDLL(str(copy))
    shutil.rmtree(folder)  # the loaded image stays mapped after the file is gone
    return lib
```

- [ ] **Step 3: Write the separation test (review focus)**

`tests/test_dmtest.py`:

```python
"""The library loader: two loads in one process have separate C globals."""

import ctypes

from dmtest.lib import load_lib


def test_each_load_has_its_own_globals():
    a, b = load_lib(), load_lib()
    seed_a = ctypes.c_uint32.in_dll(a, "g_random_seed")
    seed_b = ctypes.c_uint32.in_dll(b, "g_random_seed")
    seed_a.value, seed_b.value = 1, 2
    assert (seed_a.value, seed_b.value) == (1, 2)
```

Run: `uv run pytest -q tests/test_dmtest.py`
Expected: PASS.

- [ ] **Step 4: Build once in `conftest.py`**

Replace `PORT_SRC`, `PORT_INCLUDES`, `game_sources()` and `pytest_configure` of `tests/conftest.py` with:

```python
ROOT = Path(__file__).resolve().parent.parent


def sdl3_installed():
    """SDL3 from Homebrew: the game binary needs it; the library does not."""
    if not shutil.which("brew"):
        return False
    prefix = subprocess.run(["brew", "--prefix", "sdl3"], capture_output=True, text=True).stdout.strip()
    return Path(prefix, "include/SDL3/SDL.h").exists()


def pytest_configure(config):
    """Builds the library, and the game when SDL3 is there, once in the controller before the workers start."""
    if hasattr(config, "workerinput"):
        return
    result = subprocess.run(["make", "all" if sdl3_installed() else "lib"], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise pytest.UsageError("make failed:\n" + result.stderr[-4000:])
```

Add `import shutil` and `import pytest` to the imports.

- [ ] **Step 5: Replace the builds and the loads in the test modules**

Save as `$SCRATCH/use_loader.py` and run `uv run python $SCRATCH/use_loader.py`:

```python
"""Replaces the make calls, the clang builds and the direct loads of libdm in tests/ with dmtest.lib.load_lib()."""

import re
from pathlib import Path

LOADS = re.compile(r'ctypes\.CDLL\(str\((?:ROOT / "build/libdm\.dylib"|ROOT / "build" / "libdm\.dylib"|BUILD / "libdm\.dylib"|out|copy)\)\)')
MAKE = re.compile(r'^\s*subprocess\.run\(\["make"\], cwd=ROOT, check=True, capture_output=True\)\n', re.M)
CLANG = re.compile(r'^\s*(?:r = )?subprocess\.run\(\["clang".*\n(?:\s+capture_output=True, text=True\)\n)?', re.M)
SOURCES = re.compile(r'^\s*(?:sources|src) = .*(?:game_sources\(\)|PORT_SRC\.rglob).*\n', re.M)

for path in sorted(Path("tests").glob("test_*.py")):
    text = path.read_text()
    # conftest.py builds the library and the game once, so every make call goes
    new = MAKE.sub("", SOURCES.sub("", CLANG.sub("", LOADS.sub("load_lib()", text))))
    if new != text:
        if "from dmtest.lib import load_lib" not in new:
            new = re.sub(r"^(import pytest\n)", r"\1\nfrom dmtest.lib import load_lib\n", new, count=1, flags=re.M)
        path.write_text(new)
        print(path)
```

Then fix by hand what the script leaves:
- `tests/test_creatures.py:45-51`: the `DM_LIB` branch goes; the fixture body is `lib = load_lib()`.
- `tests/test_tables.py:63-70`: `out`, `src`, the `assert r.returncode == 0, r.stderr` line go; `lib = load_lib()`.
- The 12 clang fixtures (`tests/test_control.py:33`, `test_csb_adventure.py`, `test_csb_ending.py`,
  `test_csb_engine.py`, `test_csb_magic_map.py`, `test_csb_rules.py`, `test_csb_save.py`, `test_csb_text.py`,
  `test_entrance.py`, `test_modes.py`, `test_select.py`): the `out = ...mktemp(...)` line goes when nothing else
  reads `out`.
- `tests/test_golden_rules.py:43-47`, `tests/test_golden_save.py:44-48`, `tests/test_golden_protocol.py` (Task 1):
  the copy lines go; `lib = load_lib()`.
- `from conftest import PORT_INCLUDES, game_sources` and similar imports lose the removed names.

Run: `git grep -n -E '"clang"|game_sources|PORT_INCLUDES|PORT_SRC|CDLL\(str|"make"' -- tests`
Expected: only `tests/conftest.py` (the one `make` call) and `ctypes.CDLL(None)` lines (libc `free`).

- [ ] **Step 6: Remove `-n auto` from `addopts`**

In `pyproject.toml`: `addopts = "--dist loadfile"` and `pythonpath = ["tests"]` (so `import dmtest` works from every folder). The suite command is `make test` (`uv run pytest -q -n auto`).

- [ ] **Step 7: Run the suite once and measure**

Run: `time uv run pytest -q -n auto 2>&1 | tail -3`
Expected: the same pass count as Step 1 plus 1 (`test_dmtest.py`), no failures; the time is not above 1.5 times
the time of Step 1 (write both into the ledger; above it, report). This is the one full-suite run of an author in
this plan: the task changes the harness of every module.

- [ ] **Step 8: Commit**

```bash
git add -A tests pyproject.toml
git commit -m "test: load a private copy of libdm per test module and build it once"
```

### Task 8: Follow the new layout in the docs, CLAUDE.md and the agents

**Files:**
- Modify: `CLAUDE.md`, `README.md`, `.claude/agents/dm-coordinator.md`, the 15 pages of `docs/` that name
  `port/...` (not `docs/superpowers/`, not `CHANGELOG.md`)

**Interfaces:**
- Consumes: the layout of Tasks 4-7.
- Produces: the project `CLAUDE.md` commands that every later task uses: build `make`, one test file
  `uv run pytest -q tests/test_x.py`, full suite `make test`, personal play `make play`.

- [ ] **Step 1: Rewrite the paths in the docs**

Save as `$SCRATCH/doc_paths.py` and run `uv run python $SCRATCH/doc_paths.py`:

```python
"""Rewrites port/ paths in the docs to the new layout; a path without a layer gets the layer of its file."""

import re
from pathlib import Path

HOME = {p.name: p.parent.name for p in Path("src").glob("*/*.[ch]")}
FILES = [Path("CLAUDE.md"), Path("README.md"), *Path(".claude/agents").glob("*.md"),
         *(p for p in Path("docs").glob("*.md"))]
PATH = re.compile(r"port/src/(?:(data|engine|ui|platform)/)?([\w.]+\.[ch])\b")


def fix(match):
    layer, name = match.group(1), match.group(2)
    if name == "dr_mp3.h":
        return "third_party/dr_mp3/dr_mp3.h"
    return f"src/{layer or HOME[name]}/{name}"


for path in FILES:
    text = path.read_text()
    new = PATH.sub(fix, text)
    new = new.replace("make -C port windows", "make windows").replace("make -C port", "make")
    new = new.replace("port/Makefile", "Makefile").replace("port/src/", "src/").replace("playthroughs/lego", "playthroughs/local")
    if new != text:
        path.write_text(new)
        print(path)
```

Run: `git grep -n -E "port/src|make -C port|port/Makefile|playthroughs/lego" -- docs README.md CLAUDE.md .claude ':!docs/superpowers'`
Expected: nothing.

- [ ] **Step 2: Update the project CLAUDE.md**

Set the sections to (keep the two skill imports and the Conventions section):

```markdown
## Commands
- Build: `make` (game and test library, 0 warnings); `make windows` (MinGW-w64); `make clean`.
- Run: `build/dm`; personal play: `make play`, then `playthroughs/local/dm --data game --saves playthroughs/local`.
- One test file: `uv run pytest -q tests/test_x.py`.
- Full suite: `make test` (`uv run pytest -q -n auto`).
- All checks: `make check` (format, standard, suite); `make format` before a commit.

## Layout
- `src/`: the C port in four layers (`data/`, `engine/`, `ui/`, `platform/`); every include names its layer
  (`#include "engine/game.h"`). The root `Makefile` builds `build/dm` and `build/libdm.dylib`.
- `third_party/dr_mp3/`: the vendored MP3 decoder.
- `tools/dmtools/`: Python tools: ADF extract, FTL loader, emulator harness, control client. `tools/ghidra/`,
  `tools/dist/`, `tools/research/` (not tested).
- `tests/`: differential tests against the original; `tests/dmtest/` shared helpers (`load_lib()`);
  `tests/test_standard.py` checks the coding standard.
- Git-ignored data: `game/`, `game-csb/` (original files), `reference/` (Ghidra output), `playthroughs/`. Link them into a new worktree.
```

- [ ] **Step 3: Note the personal play folder**

Do not move `playthroughs/lego/` (git-ignored, the owner's saves; a game may run from it). Add one line to the
handover of the ledger: "The owner moves `playthroughs/lego/` to `playthroughs/local/` (`make play` writes there)."
Stream I3 puts the same line into the manual test checklist.

- [ ] **Step 4: Check the links of the moved paths**

Run: `for f in $(git grep -h -o -E "\b(src|tools|third_party)/[A-Za-z0-9_./-]+\.(c|h|py|sh|md)\b" -- docs/*.md README.md CLAUDE.md | sort -u); do test -e "$f" || echo "missing $f"; done`
Expected: nothing missing.

- [ ] **Step 5: Commit**

```bash
git add -A CLAUDE.md README.md .claude docs
git commit -m "docs: follow the src/ layout, the root Makefile and the local play folder"
```

---

## Package C: the standard checks, the test layout, the docs folders and the repository files

Epic 3 (R3), R12 without R12.6/R12.7, R13.3 (moves and stale fixes), R14.1, R14.2, R14.4.

Starts after package B. State that package B leaves (Assumption: each task checks the part it uses in its first
step): the sources are in `src/<layer>/` with layer-named includes; the root `Makefile` has `all`, `lib`, `format`;
`clang-format` 23 is in the `dev` group; `tests/dmtest/lib.py` has `load_lib()`; `tests/conftest.py` builds the
library once with `make lib`; no test compiles the library; `pyproject.toml` has `pythonpath = ["tests"]` and
`addopts = "--dist loadfile"` (Task 17 adds `--import-mode=importlib`); the standard check is still `tests/test_standard.py` with
`SRC = ROOT / "src"` and `tests/standard_allowlist.txt`; the tools are in `tools/ghidra/`, `tools/dist/`,
`tools/research/platforms/`, and `tools/csb_match.py` is still at the top of `tools/`.

Three streams run in parallel; the tail runs after the merge.

- Stream C1 (Tasks 9-14): the checks of the standard, in `tests/standard/`.
- Stream C2 (Tasks 15-19): the test helpers and the test folders.
- Stream C3 (Tasks 20-22): the docs folders, the stale docs, the repository files.
- Tail (Tasks 23-26): the SPDX lines and the include guards, the enum-to-define script, the comment move, the
  stale allowlist lines.

Merge notes for the controller: `pyproject.toml` and `uv.lock` change in Task 19 (ruff) and Task 22 (mcp extra,
`dm-csb-match`): merge `pyproject.toml` by hand, then run `uv lock` and commit the lock file. `tests/test_symbols.py`
moves in Task 17 (to `tests/tools/`) and gets one path change in Task 20: git merges the rename and the edit.
`tests/standard/` gets `test_standard.py` and `allowlist.txt` from C1 and `test_docs.py` from C12.

Epic numbers of the `open:` lines (the mapping that `tests/standard/open_lines.py` of Task 9 applies):

| Target | Epic |
|---|---|
| `tests/...`, check `tests` | 12 |
| `data/...` | 4 |
| `engine/sound.*`, `ui/music*`, `ui/audio*` | 9 |
| `engine/...`, checks `record module prefix header include comment order` | 5 |
| `engine/...`, checks `global dead-api engine-write` | 7 |
| `engine/...`, every other check | 6 |
| `ui/...` | 8 |
| `control/...`, `platform/control*`, `platform/state_json*` | 10 |
| `platform/...` | 11 |
| anything else | 13 |

#### Stream C1: the checks of the standard

Shared files: none (`tests/test_standard.py` and `tests/standard_allowlist.txt` move to `tests/standard/`; no
other stream of package C touches them).

### Task 9: Move the standard check to tests/standard/ and adapt it to five layers

**Files:**
- Move: `tests/test_standard.py` → `tests/standard/test_standard.py`
- Move: `tests/standard_allowlist.txt` → `tests/standard/allowlist.txt`
- Create: `.gitattributes`
- Create: `tests/standard/open_lines.py`

**Interfaces:**
- Consumes: the tree of package B (`src/<layer>/`, includes `"layer/file.h"`, `.venv/bin/clang-format` from the
  `dev` group).
- Produces: `tests/standard/test_standard.py` with the helpers `sources()`, `headers()`, `rel(path)`,
  `allowed()`, `strip(text)`, `code_lines(path)`, `report(check, found)`, `line_check(check, pattern, owners=(),
  ignore=None)`, `declarations(path)`, `functions(path)`, `comment_above(lines, line)`, `definitions(path)`;
  the environment variables `DM_STANDARD_DUMP` (append every violation line to this file) and
  `DM_STANDARD_IGNORE_ALLOWLIST` (report every violation); `tests/standard/open_lines.py` (add `open:` lines,
  `--prune` removes stale ones). Every later task that adds a check or removes a violation uses them.

- [ ] **Step 1: Check the state of package B**

Run: `test -d src/engine && test -f tests/dmtest/lib.py && test -x .venv/bin/clang-format && grep -n 'SRC = ' tests/test_standard.py`
Expected: no error; the line shows `SRC = ROOT / "src"`. If a part is missing, stop and report: package B is
not complete.

- [ ] **Step 2: Move the files and set the union merge for the allowlist**

```bash
mkdir -p tests/standard
git mv tests/test_standard.py tests/standard/test_standard.py
git mv tests/standard_allowlist.txt tests/standard/allowlist.txt
printf 'tests/standard/allowlist.txt merge=union\n' > .gitattributes
```

- [ ] **Step 3: Replace the head of `tests/standard/test_standard.py` (the docstring, the imports and the path
  constants up to `MAX_FUNCTION_LINES`) and the helpers `sources`, `rel`, `allowed`, `report`, `line_check`**

```python
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
```

Delete `VENDORED` and its use: `third_party/` is outside `src/`, so `sources()` never sees `dr_mp3.h`.

- [ ] **Step 4: Add the shared parsers below `declarations` (later tasks use them)**

```python
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
```

Change `test_functions_are_short` to use `definitions(path)` (same key `f"{rel(path)}:{name}"`, same limit).

- [ ] **Step 5: Adapt the layer, platform, byte order, kaos and format checks**

Replace `LAYERS`/`INCLUDE`/`test_layers`, `PLATFORM_CODE`/`test_platform_code_only_in_platform`,
`BYTE_ORDER`/`test_byte_order_helpers_only_in_bytes_header`, `KAOS_HEADER`, and `test_sources_are_formatted`:

```python
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


BYTE_ORDER_DEF = re.compile(r"^\s*#\s*define\s+(be16|wr16|be32|wr32)\b|^(?:static\s+)?(?:inline\s+)?[\w\s*]+?\b"
                            r"(be16|wr16|be32|wr32)\s*\([^;]*\)\s*$", re.M | re.I)


def test_byte_order_helpers_only_in_bytes_header():
    report("byteorder", [f"byteorder {rel(path)}  | {m.group().strip()}" for path in sources()
                         if rel(path) != "data/bytes.h"
                         for m in BYTE_ORDER_DEF.finditer(strip(path.read_text(errors="replace")))])


KAOS_HEADER = re.compile(r'^\s*#\s*include\s*"(?:data/)?kaos_addresses\.h"', re.M)
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
```

Delete the old `BYTE_ORDER` tuple. In `engine_globals` keep `(SRC / "engine").glob("*.h")`.

- [ ] **Step 6: Add the allowlist check**

```python
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
```

- [ ] **Step 7: Rewrite the allowlist with the new reason format**

Write `tests/standard/allowlist.txt` (the targets of the current lines, `tests/standard_allowlist.txt:3-11`):

```text
# Accepted exceptions of tests/standard/test_standard.py. One per line: <check> <target> <reason>.
# A reason is "rule <n.m>: <text>" for an exception that docs/coding-standard.md names, or "open: epic <N>"
# for a violation that epic N of the final refactor removes. Targets are relative to src/.
decimal data/csbimage.c rule 3.12: the APPB address tables, not in the game files
hex data/csbimage.c rule 3.12: the APPB address tables, not in the game files
hex data/csbtext.c rule 3.12: the APPB address tables, not in the game files
decimal ui/musicmap.c rule 3.12: the FM-Towns CSB track of each square, not in the game files
decimal ui/music.c rule 3.12: the FM-Towns level tracks and the Super NES trigger squares, not in the game files
hex engine/map.c open: epic 5
hex engine/map.h open: epic 5
long engine/action.c:action_is_action_performed rule 5.8: one short case for each action of the action table
long engine/timeline_process.c:event_process_expired rule 5.8: one short case for each timeline event type
```

- [ ] **Step 8: Write `tests/standard/open_lines.py`**

```python
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
CONTROL = ("control/", "platform/control", "platform/state_json")
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
```

- [ ] **Step 9: Run the check, add the open lines for the adapted checks, run it again**

Run: `uv run python tests/standard/open_lines.py && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added" (the include-path and platform findings, if any), then all tests PASS. Inspect each
added line: a `layer` line for a missing layer folder or an SDL include outside `platform/` is a package B error;
stop and report it instead of allowlisting it.

- [ ] **Step 10: Commit**

```bash
git add .gitattributes tests/standard/
git commit -m "test: move the standard check to tests/standard and check five layers"
```

### Task 10: Constant checks: enum, block, notation per block, single digits

**Files:**
- Modify: `tests/standard/test_standard.py`
- Modify: `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: Task 9 helpers.
- Produces: the checks `enum`, `block`, `notation` (per block), `decimal` (single digits in `[N]` and `< N;`,
  the base argument of `strto*` allowed). Targets are file paths (`enum data/ids.h`).

- [ ] **Step 1: Extend the decimal check**

Replace `DECIMAL` and `test_no_decimal_literals`:

```python
DECIMAL = re.compile(r"(?<![\w.])[1-9][0-9]+(?![\w.])|\[\s*[2-9]\s*\]|<=?\s*[2-9]\s*;")
STRTOL_BASE = re.compile(r"\bstrto\w*\s*\([^;]*?,\s*10\s*\)")


def test_no_decimal_literals():
    line_check("decimal", DECIMAL, ignore=STRTOL_BASE)
```

- [ ] **Step 2: Add the enum check (an `enum` names a result type: a type that a function returns)**

```python
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
```

- [ ] **Step 3: Add the block parser and the block check**

```python
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
```

- [ ] **Step 4: Replace the per-value notation check by the per-block check**

Delete `CONST_VALUE`'s use in `notation_error`, `ENUM_VALUE`, `STATIC_CONST_VALUE`, `constant_values`,
`notation_error`, `test_number_notation` (keep `CONST_VALUE`: `test_each_image_address_has_one_name` uses it).
Add:

```python
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
    decimal_kind = any(DECIMAL_PARTS & set(parts) for parts in names)
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
```

- [ ] **Step 5: Add the open lines and run the checks**

Run: `uv run python tests/standard/open_lines.py -k "decimal or enum or block or notation" && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added", then PASS. The enum lines get the epic of their layer; the tail Task 24 converts the
anonymous enums and Task 26 prunes the lines that no longer apply.

- [ ] **Step 6: Commit**

```bash
git add tests/standard/
git commit -m "test: check enums, constant blocks and the notation per block"
```

### Task 11: Name checks: name length, static prefix, globals, ordinals, truth values

**Files:**
- Modify: `tests/standard/test_standard.py`
- Modify: `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: `functions(path)`, `comment_above`, `PREFIXES`, `PREDICATE`, `IDENTIFIER` (Task 9 and the current file).
- Produces: the checks `name`, `static-prefix`, `global`, `ordinal`, and the extended `bool`. Targets:
  `<path>:<function>` for `name`, `static-prefix`, `bool`; `<path>` for `global`; the identifier for `ordinal`.

- [ ] **Step 1: Add the prefix table helper**

```python
def all_prefixes():
    """Every prefix that a header states, longest first."""
    found = {p for h in headers() if (m := PREFIXES.search(h.read_text(errors="replace"))) for p in m.group(1).split()}
    return sorted(found, key=len, reverse=True)
```

- [ ] **Step 2: Add the name and static-prefix checks**

```python
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
            prefix = "" if f.static else next((p for p in prefixes if f.name.startswith(p)), "")
            words = f.name[len(prefix):].split("_")
            if len(f.name) > MAX_NAME_CHARS or len(words) > MAX_NAME_WORDS:
                found.append(f"name {key}  | {len(f.name)} characters, {len(words)} words after {prefix or 'no prefix'}")
    report("name", found)


def test_static_functions_have_no_module_prefix():
    prefixes, ok = all_prefixes(), allowed()
    report("static-prefix", sorted({f"static-prefix {rel(path)}:{f.name}  | {p}" for path in sources() for f in functions(path)
                                    if f.static and (p := next((p for p in prefixes if f.name.startswith(p)), None))
                                    and ("static-prefix", f"{rel(path)}:{f.name}") not in ok}))
```

- [ ] **Step 3: Add the global check**

```python
EXTERN_VARIABLE = re.compile(r"^\s*extern\b([^;]*);", re.M)
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
```

- [ ] **Step 4: Add the ordinal check**

```python
ORDINAL = re.compile(r"(?:^|_)ordinal(?:_|$)|_plus_1(?:_|$)", re.I)


def test_no_ordinals_in_names():
    ok = allowed()
    where = defaultdict(set)
    for path in sources():
        for name in IDENTIFIER.findall(strip(path.read_text(errors="replace"))):
            if ORDINAL.search(name) and ("ordinal", name) not in ok:
                where[name].add(rel(path))
    report("ordinal", [f"ordinal {name}  | {sorted(paths)}" for name, paths in sorted(where.items())])
```

- [ ] **Step 5: Extend the bool check (truth locals and "Returns 0/1" comments)**

Replace `test_predicates_return_bool`:

```python
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
```

- [ ] **Step 6: Add the open lines and run the checks**

Run: `uv run python tests/standard/open_lines.py -k "names or prefix or globals or ordinal or truth" && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added", then PASS. Spot-check three `bool` lines of the evidence by reading the code: a false
match (an `int` sum of comparisons) is a check error; narrow `TRUTH_LOCAL` instead of allowlisting it.

- [ ] **Step 7: Commit**

```bash
git add tests/standard/
git commit -m "test: check function names, static prefixes, globals, ordinals and truth values"
```

### Task 12: Module checks: module, header, include, comment, order

**Files:**
- Modify: `tests/standard/test_standard.py`
- Modify: `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: Task 9 helpers.
- Produces: the checks `module`, `header`, `include`, `comment`, `order`; the helper `module_files(header)`.
  Targets: file paths; `include` uses `<path>:<included header>`.

- [ ] **Step 1: Add the module check and `module_files`**

```python
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
```

- [ ] **Step 2: Add the header check**

```python
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
```

- [ ] **Step 3: Add the dead include check**

```python
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
```

- [ ] **Step 4: Add the comment check**

```python
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
                found.append(f"comment {rel(body[0])}  | {f.name}: a comment above the prototype and the definition")
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
```

- [ ] **Step 5: Add the order check**

```python
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
```

- [ ] **Step 6: Add the open lines and run the checks**

Run: `uv run python tests/standard/open_lines.py -k "module or header or includes or comments or order" && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added", then PASS. Most `header` and `comment` lines go in the tail (Tasks 23 and 25);
Task 26 prunes them.

- [ ] **Step 7: Commit**

```bash
git add tests/standard/
git commit -m "test: check modules, header heads, dead includes, function comments and file order"
```

### Task 13: API checks: type, result, params, dead-api, record

**Files:**
- Modify: `tests/standard/test_standard.py`
- Modify: `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: Tasks 9, 12 helpers (`functions`, `module_files`, `comment_above`, `extern_names`).
- Produces: the checks `type` (target `<layer>:<parameter name>`), `result` and `params` (target
  `<path>:<function>`), `dead-api` (target `<header path>:<name>`), and the new `record` pattern with the owners
  `record.c`, `record.h`.

- [ ] **Step 1: Add the type check (the type table of standard rule 5.1)**

```python
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
```

- [ ] **Step 2: Add the result and params checks**

```python
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
```

- [ ] **Step 3: Add the dead-api check**

```python
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
```

- [ ] **Step 4: Replace the record check**

```python
RECORD = re.compile(r"\bREC\s*\(|\bg_current_map_data\s*\[|\bg_dungeon_map_data\s*\[|\bg_current_map\s*\[|"
                    r"dungeon_get_thing_data\([^()]*\)\s*\[")


def test_raw_record_access_only_in_accessors():
    line_check("record", RECORD, owners={"record.c", "record.h"})
```

Delete the two allowlist lines `hex engine/map.c open: epic 5` and `hex engine/map.h open: epic 5` only if Step 5
adds `record` and `hex` lines for the same files; they stay otherwise (Epic 5 removes them).

- [ ] **Step 5: Add the open lines and run the checks**

Run: `uv run python tests/standard/open_lines.py -k "parameter or results or five or exports or record" && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added", then PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/standard/
git commit -m "test: check parameter types, results, parameter counts, unused exports and record access"
```

### Task 14: Comment and test checks: event-comment, todo, tests

**Files:**
- Modify: `tests/standard/test_standard.py`
- Modify: `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: Task 9 helpers.
- Produces: the checks `event-comment`, `todo` (target: path) and `tests` (target: the test file path relative to
  the repository root, `tests/...`).

- [ ] **Step 1: Add the event-comment and todo checks**

```python
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
```

- [ ] **Step 2: Add the tests check**

```python
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
```

- [ ] **Step 3: Add the open lines and run the whole check**

Run: `uv run python tests/standard/open_lines.py -k "event or todo or tests_follow" && uv run pytest -q tests/standard/test_standard.py`
Expected: "N lines added" (the `tests` lines get `open: epic 12`; stream C2 removes their cause in this package,
Task 26 prunes the lines), then PASS.

- [ ] **Step 4: Check that every key of the standard table has a check**

Run: `grep -o '^| \`[a-z-]*\`' docs/coding-standard.md | tr -d '|\` ' | sort -u > /tmp/keys-std.txt; grep -o 'report("[a-z-]*"\|line_check("[a-z-]*"\|comment_check("[a-z-]*"' tests/standard/test_standard.py | grep -o '"[a-z-]*"' | tr -d '"' | sort -u > /tmp/keys-test.txt; comm -3 /tmp/keys-std.txt /tmp/keys-test.txt`
Expected: only `hex,`-style table rows that hold two keys (`hex`, `decimal`; `kaos`, `address`) appear, and each
of those keys is in the test file. Any other key is a missing check: add it before the commit.

- [ ] **Step 5: Commit**

```bash
git add tests/standard/
git commit -m "test: check event numbers in comments, TODO markers and the test rules"
```

#### Stream C2: the test helpers and the test folders

Shared files: `pyproject.toml`, `uv.lock` (C14 also changes them), `tests/test_symbols.py` (C12 changes one path).

### Task 15: dmtest.constants and dmtest.markers

**Files:**
- Create: `tests/dmtest/constants.py`
- Create: `tests/dmtest/markers.py`
- Create: `tests/tools/test_dmtest_constants.py`

**Interfaces:**
- Consumes: `tests/dmtest/__init__.py` (package B; Assumption: it exists, Step 1 checks), `pythonpath = ["tests"]`.
- Produces: `from dmtest import constants as C` (`C.KEY_RETURN`); `dmtest.constants.parse(texts) -> dict[str, int]`,
  `dmtest.constants.evaluate(expression, known) -> int`, `dmtest.constants.VALUES`;
  `dmtest.markers`: `ROOT`, `GAME`, `GAME_CSB`, `MINI`, `needs_dm`, `needs_csb`, `needs_csb_mini`, `needs_sdl`,
  `needs_ghidra`, `needs_file(path, reason=None)`.

- [ ] **Step 1: Check the package**

Run: `ls tests/dmtest/__init__.py tests/dmtest/lib.py && grep -n pythonpath pyproject.toml`
Expected: both files exist; `pythonpath = ["tests"]`. Else create an empty `tests/dmtest/__init__.py` and report it.

- [ ] **Step 2: Write the failing parser test `tests/tools/test_dmtest_constants.py`**

```python
"""The constant parser of tests/dmtest against small C headers."""

from dmtest.constants import parse

HEADER = """
#define ROWS 5
#define NONE (-1)
#define LAST_ROW (ROWS - 1)
#define MIN_S16 ((int16_t)0x8000)
#define BIT_5 (1 << 5)
#define MASK 0x0fu /* a comment */
#define LETTER 'A'
#define NAME "text"
#define CALL(x) ((x) + 1)
enum MoveResult { MOVE_OK, MOVE_BLOCKED = 3, MOVE_FELL };
"""


def test_values_of_a_header():
    c = parse([HEADER])
    assert (c["ROWS"], c["NONE"], c["LAST_ROW"], c["MIN_S16"], c["BIT_5"], c["MASK"], c["LETTER"]) == (5, -1, 4, -32768, 32, 15, 65)
    assert (c["MOVE_OK"], c["MOVE_BLOCKED"], c["MOVE_FELL"]) == (0, 3, 4)
    assert "NAME" not in c and "CALL" not in c


def test_division_truncates_toward_zero_as_in_c():
    assert parse(["#define A (-7 / 2)\n#define B (-7 % 2)"]) == {"A": -3, "B": -1}


def test_a_constant_may_use_a_later_one():
    assert parse(["#define B (A + 1)", "#define A 2"])["B"] == 3


def test_unsigned_cast_wraps():
    assert parse(["#define ALL ((uint16_t)-1)"])["ALL"] == 0xFFFF
```

Run: `uv run pytest -q tests/tools/test_dmtest_constants.py`
Expected: FAIL with `ModuleNotFoundError: No module named 'dmtest.constants'`.

- [ ] **Step 3: Write `tests/dmtest/constants.py`**

```python
"""The constants of the C headers: #define lines and enum members of src/**/*.h, evaluated once at import.

Tests write C.KEY_RETURN. A value is an integer expression over literals, other constants, casts, shifts and
arithmetic; a define with another value (a string, a macro with arguments, sizeof) is not a constant here.
"""

import ast
import operator
import re
from pathlib import Path

_SRC = Path(__file__).resolve().parents[2] / "src"
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Z][A-Z0-9_]*)[ \t]+(.+)$", re.M)
_ENUM = re.compile(r"\benum\b\s*\w*\s*\{([^}]*)\}", re.S)
_INT_SUFFIX = re.compile(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b")
_CHAR = re.compile(r"'(?:\\.|[^'\\])'")
_CAST = re.compile(r"\(\s*(u?int(?:8|16|32)_t)\s*\)")
_OCTAL = re.compile(r"\b0([0-7]+)\b")
_WIDTH = {"int8_t": (8, True), "uint8_t": (8, False), "int16_t": (16, True), "uint16_t": (16, False),
          "int32_t": (32, True), "uint32_t": (32, False)}


def _wrap(value, bits, signed):
    value &= (1 << bits) - 1
    return value - (1 << bits) if signed and value >> (bits - 1) else value


def _c_div(a, b):
    quotient = abs(a) // abs(b)
    return quotient if (a >= 0) == (b >= 0) else -quotient


_BINARY = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: _c_div,
           ast.Mod: lambda a, b: a - b * _c_div(a, b), ast.LShift: operator.lshift, ast.RShift: operator.rshift,
           ast.BitAnd: operator.and_, ast.BitOr: operator.or_, ast.BitXor: operator.xor}
_UNARY = {ast.USub: operator.neg, ast.UAdd: operator.pos, ast.Invert: operator.invert}


def _value(node, known):
    if isinstance(node, ast.Constant) and type(node.value) is int:
        return node.value
    if isinstance(node, ast.Name):
        return known[node.id]
    if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARY:
        return _UNARY[type(node.op)](_value(node.operand, known))
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Pow) and isinstance(node.left, ast.Name) and node.left.id in _WIDTH:
        return _wrap(_value(node.right, known), *_WIDTH[node.left.id])
    if isinstance(node, ast.BinOp) and type(node.op) in _BINARY:
        return _BINARY[type(node.op)](_value(node.left, known), _value(node.right, known))
    raise ValueError(ast.dump(node))


def evaluate(expression, known):
    """The value of a C integer constant expression; KeyError for an unknown name, ValueError for other C.

    A cast (int16_t)x becomes int16_t ** x: ** binds like a C cast here and does not occur in C."""
    text = _INT_SUFFIX.sub(r"\1", expression)
    text = _CHAR.sub(lambda m: str(ord(ast.literal_eval(m.group()))), text)
    text = _OCTAL.sub(r"0o\1", _CAST.sub(r"\1 ** ", text))
    try:
        tree = ast.parse(text.strip(), mode="eval")
    except SyntaxError as error:
        raise ValueError(expression) from error
    return _value(tree.body, known)


def parse(texts):
    """The constants of C header texts as {name: value}; an enum member without a value counts up from the previous one."""
    raw = []
    for text in texts:
        text = _COMMENT.sub(" ", text).replace("\\\n", " ")
        raw += _DEFINE.findall(text)
        for body in _ENUM.findall(text):
            previous = None
            for item in body.split(","):
                if not (item := item.strip()):
                    continue
                name, _, expression = (part.strip() for part in item.partition("="))
                raw.append((name, expression or (f"({previous}) + 1" if previous else "0")))
                previous = name
    known, pending = {}, raw
    while pending:
        left = []
        for name, expression in pending:
            try:
                known[name] = evaluate(expression, known)
            except KeyError:
                left.append((name, expression))
            except (ValueError, TypeError, ZeroDivisionError):
                pass
        if len(left) == len(pending):
            break
        pending = left
    return known


VALUES = parse(path.read_text(errors="replace") for path in sorted(_SRC.rglob("*.h")))
globals().update(VALUES)


def __getattr__(name):
    raise AttributeError(f"{name} is not a constant of src/**/*.h")
```

- [ ] **Step 4: Run the parser test and a smoke import**

Run: `uv run pytest -q tests/tools/test_dmtest_constants.py && uv run python -c "import sys; sys.path.insert(0, 'tests'); from dmtest import constants as C; print(C.KEY_RETURN, C.SLOT_HAND_ACTION, len(C.VALUES))"`
Expected: PASS; the print shows `17408 1` and a count above 2000 (Assumption: `KEY_RETURN` is 0x4400 as in
`tests/test_gameloop.py:16`; `SLOT_HAND_ACTION` is 1 as in `tests/test_modes.py:30`).

- [ ] **Step 5: Write `tests/dmtest/markers.py`**

```python
"""Skip markers for tests that need game files or programs that a machine may not have."""

import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
GAME, GAME_CSB = ROOT / "game", ROOT / "game-csb"
MINI = GAME_CSB / "disk-b" / "MINI.DAT"
DM_FILES = ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")
CSB_FILES = ("APPB.FTL", "Graphics.DAT", "Dungeon.DAT")


def _brew_path(formula, relative):
    if not shutil.which("brew"):
        return None
    prefix = subprocess.run(["brew", "--prefix", formula], capture_output=True, text=True).stdout.strip()
    path = Path(prefix, relative)
    return path if prefix and path.exists() else None


def needs_file(path, reason=None):
    return pytest.mark.skipif(not Path(path).exists(), reason=reason or f"{Path(path).name} missing")


needs_dm = pytest.mark.skipif(not all((GAME / f).exists() for f in DM_FILES), reason="game files missing")
needs_csb = pytest.mark.skipif(not all((GAME_CSB / f).exists() for f in CSB_FILES), reason="game-csb files missing")
needs_csb_mini = needs_file(MINI, "game-csb/disk-b/MINI.DAT missing")
needs_sdl = pytest.mark.skipif(not _brew_path("sdl3", "include/SDL3/SDL.h"), reason="SDL3 not installed")
needs_ghidra = pytest.mark.skipif(not _brew_path("ghidra", "libexec/support/analyzeHeadless"), reason="Ghidra not installed")
```

- [ ] **Step 6: Commit**

```bash
git add tests/dmtest/constants.py tests/dmtest/markers.py tests/tools/test_dmtest_constants.py
git commit -m "test: add dmtest constants from the C headers and the skip markers"
```

### Task 16: Shared helpers in dmtest; no test module imports another

**Files:**
- Create: `tests/dmtest/game.py`, `tests/dmtest/savefile.py`, `tests/dmtest/control.py`, `tests/dmtest/emu.py`
- Modify: the 20 test files that import from a test module (list in Step 1) and `tests/conftest.py`

**Interfaces:**
- Consumes: `dmtest.lib.load_lib()` (package B), `dmtest.markers` (Task 15).
- Produces (later tasks and later packages import these; a rename of a C struct field changes these classes):
  - `dmtest.game`: `Box`, `Bitmap`, `Skill`, `Champion`, `Party`, `TimelineEvent`, `RecruitData`, `World`, `Game`,
    `Frames`, `BOOL_GLOBALS`, `global_type(name, default=c_int16)`, `STEP_EAST`, `STEP_NORTH`, `start_game(lib) -> Game`
    (load data, new game: the body of `tests/test_gameloop.py:37-51` without the `make` and `CDLL` lines),
    `recruit_first_champion(game)` (the recruit steps of `tests/test_modes.py:68-77`), `recruit_and_hold_object`,
    `state`, `scrub_free_groups`, `pack`, `events`, `things_of_type`, `mirrors`, `zone_records`.
  - `dmtest.savefile`: `SaveFile`, `u8p`, `encode`, `decode`, the KAOS addresses `SAVE_READ_CHECKED_BLOCK`,
    `SAVE_READ_HEADER`, `G_LOAD_FROM_MEMORY`.
  - `dmtest.control`: `Client`, `pump`, `pumped`, `socket_dir()` (the `tempfile.mkdtemp(dir="/tmp")` of
    `tests/test_control.py:60` with its comment on the socket path length).
  - `dmtest.emu`: `kaos_emu(max_cycles)` (an `Emu` with the `KAOS.FTL` image at `0x100000`, the setup that the
    differential modules repeat), `var(lib, ctype, name)`, `need(lib, *names)` (the `need` of
    `tests/test_champion.py:93-98`), `s16(v)`.
  - `dmtest.lib`: `set_folder_hooks(lib)`, `clear_folder_hooks(lib)` (moved from `tests/conftest.py:60-80`).

- [ ] **Step 1: List the cross imports**

Run: `grep -rn "^from test_\|^import test_\|^from conftest import" tests --include=*.py`
Expected: the 24 `from test_` lines of the inventory (`tests/test_actions.py:8-9`, `test_csb_adventure.py:13-14`,
`test_control.py:16`, `test_csb_ending.py:13`, `test_csb_rules.py:12-14`, `test_csb_save.py:11-13`,
`test_golden_rules.py:12-16`, `test_gameloop.py:10`, `test_golden_save.py:12-15`, `test_creatures.py:15`,
`test_move.py:11`, `test_music.py:6`, `test_golden_view.py:10-11`, `test_csb_magic_map.py:12-13`,
`test_panels.py:11`, `test_scale.py:5`, `test_save.py:11-12`, `test_view.py:9`, `test_sound.py:10`,
`test_ui.py:9-11`, `test_modes.py:17-18`, `test_port_data.py:11`, `test_text.py:8`) and the `from conftest`
lines that package B left. Save the list in the SDD workspace.

- [ ] **Step 2: Move the classes and helpers**

Move each name that a second module imports into the `dmtest` module of the Interfaces block, with its code
unchanged. Rules:
- One definition per class: `Champion` exists three times (`tests/test_champion.py:42`, `tests/test_ui.py:23`,
  `tests/test_game.py:33`); compare the field lists (`diff <(sed -n 42,53p tests/test_champion.py) <(sed -n 23,34p tests/test_ui.py)`);
  equal lists become `dmtest.game.Champion`; a different list (the original memory layout for the emulator) goes
  to `dmtest.emu` under a name that says so (`KaosChampion`). `Box` (`tests/test_gameloop.py:32`,
  `tests/test_view.py:22`, `tests/test_save.py:110`) and `Bitmap` (`tests/test_view.py:18`, `tests/test_champion.py:428`) the same.
- A C constant that a helper uses comes from `dmtest.constants` (`C.SLOT_HAND_ACTION`); a KAOS address stays a
  Python constant with its comment "Original addresses (docs/kaos-symbols.csv)".
- A shared fixture (`game` of `test_view`, `test_ui`, `test_modes`; `lib` of `test_creatures`, `test_port`)
  becomes a function in `dmtest` (`start_game(lib)`, `setup_view(lib)`, `setup_ui(lib)`) and each module keeps a
  module-scope fixture that calls it with its own `load_lib()`. No module imports a fixture any more; the
  `# noqa: F401` comments of the fixture imports go.
- `global_type` and `BOOL_GLOBALS` move from `tests/conftest.py:20-54` to `dmtest.game`.

- [ ] **Step 3: Rewrite the imports**

Each line of Step 1 becomes an import from `dmtest`, for example `tests/test_modes.py:17-18`:

```python
from dmtest import constants as C
from dmtest.control import Client, pump
from dmtest.game import STEP_EAST, STEP_NORTH, Box, Game, start_game
```

and every use of a mirrored name that came through the import (`KEY_RETURN`, `ZONE_RESURRECT`, `CORRIDOR`,
`LEFT_PRESS`, `THING_EXPLOSION`, ...) becomes `C.<NAME>`. Assumption: `LEFT_PRESS`, `LEFT_RELEASE` and `CORRIDOR`
have C names; check with `grep -rn "define LEFT_PRESS\|define CORRIDOR\|define SQUARE_CORRIDOR" src`; a value
without a C constant stays a Python constant in `dmtest.game` with a one-line comment.

- [ ] **Step 4: Run the changed modules**

Run: `uv run pytest -q -n auto $(grep -rln "dmtest" tests --include=test_*.py)`
Expected: PASS, the same pass and skip counts as before the change (record the counts of
`uv run pytest -q -n auto <same files>` on the commit before Step 2 in the SDD workspace). A changed count is a
lost test: find it before the commit.

- [ ] **Step 5: Check that no cross import is left**

Run: `grep -rn "^from test_\|^import test_\|^from conftest import" tests --include=*.py`
Expected: no output.

- [ ] **Step 6: Commit**

```bash
git add tests/
git commit -m "test: move the shared test helpers into dmtest"
```

### Task 17: Test folders by what a test proves

**Files:**
- Move: every test file by the map below
- Modify: `pyproject.toml`: `addopts = "--dist loadfile --import-mode=importlib"` (PRD R12.2), in Step 2; `testpaths`
  stays `["tests"]`

**Interfaces:**
- Consumes: Task 16 (no test module imports another, so a move breaks no import).
- Produces: the folders `tests/differential/`, `tests/port/`, `tests/golden/`, `tests/tools/`, `tests/standard/`;
  later tasks name test files by these paths (`uv run pytest -q tests/port/test_control.py`).

Move map (the classes of `docs/superpowers/2026-10-16-final-refactor-inventory/review-platform-repo.md:117-122`):

| From `tests/` | To |
|---|---|
| `test_champion.py`, `test_creatures.py`, `test_move.py`, `test_game.py`, `test_dungeon.py`, `test_spells.py`, `test_sound.py`, `test_text.py`, `test_tables.py`, `test_savefile.py`, `test_recruit.py`, `test_csb_rules.py`, `test_csb_engine.py`, `test_csb_magic_map.py`, `test_csb_text.py`, `test_csb_ending.py` | `tests/differential/` |
| `csb_diff.py` | `tests/differential/test_csb_mini.py` (Step 3) |
| `test_gameloop.py`, `test_control.py`, `test_modes.py`, `test_select.py`, `test_entrance.py`, `test_ui.py`, `test_panels.py`, `test_view.py`, `test_automap.py`, `test_save.py`, `test_actions.py`, `test_port.py`, `test_port_data.py`, `test_music.py`, `test_scale.py`, `test_csb_adventure.py`, `test_csb_save.py` | `tests/port/` |
| `test_golden_rules.py`, `test_golden_save.py`, `test_golden_view.py` (and the files of package A: `test_golden_protocol.py`, `test_golden_screens.py`) | `tests/golden/` (beside `rules.json`, `save.json`, `view.json`) |
| `test_adf.py`, `test_extract.py`, `test_ftl.py`, `test_emu.py`, `test_ghidra.py`, `test_symbols.py`, `test_csb_tools.py` | `tests/tools/` |
| `test_standard.py`, `standard_allowlist.txt` | `tests/standard/` (Task 9) |
| `conftest.py` | stays in `tests/` |

- [ ] **Step 1: Check that the list is complete**

Run: `ls tests/*.py`
Expected: only the files of the map and `conftest.py`. A file of package A or B that is not in the map: put it in
the folder of what it proves and report it.

- [ ] **Step 2: Move the files with git mv**

```bash
mkdir -p tests/differential tests/port tests/tools
for f in champion creatures move game dungeon spells sound text tables savefile recruit csb_rules csb_engine csb_magic_map csb_text csb_ending; do git mv tests/test_$f.py tests/differential/; done
for f in gameloop control modes select entrance ui panels view automap save actions port port_data music scale csb_adventure csb_save; do git mv tests/test_$f.py tests/port/; done
for f in tests/test_golden_*.py; do git mv "$f" tests/golden/; done
for f in adf extract ftl emu ghidra symbols csb_tools; do git mv tests/test_$f.py tests/tools/; done
```

- [ ] **Step 3: Turn `csb_diff.py` into a differential test**

`git mv tests/csb_diff.py tests/differential/test_csb_mini.py`. The file is a script (`tests/csb_diff.py:3`
"Run: uv run python tests/csb_diff.py"). Read its `main` and turn each comparison it prints into one test
function with the same inputs and an `assert` on the equality it reports; mark the module with
`needs_file(GAME_CSB / "CEDT.FTL")` (Assumption: it reads `CEDT.FTL`; Step 3 confirms by reading the file).
Delete the `/Users/lego/github/dm` docstring line. If a comparison prints a known difference instead of an
equality, keep it out and list it in the commit message.

- [ ] **Step 4: Fix the paths that count parent folders**

Run: `grep -rn "parent.parent\|parents\[1\]" tests --include=*.py`
Each hit computes the repository root from the old depth; replace it with `from dmtest.markers import ROOT` (and
`GAME`, `GAME_CSB`). Golden tests read their `*.json` beside them: `Path(__file__).with_name("rules.json")`.

- [ ] **Step 5: Collect and run the moved modules**

Run: `uv run pytest -q -n auto tests`
Expected: the same pass and skip counts as on the commit before Step 2 (record them), plus the new tests of
`test_csb_mini.py`.

- [ ] **Step 6: Commit**

```bash
git add -A tests/
git commit -m "test: group the tests into differential, port, golden, tools and standard folders"
```

### Task 18: C constants from dmtest.constants instead of mirrored values

**Files:**
- Modify: the test files with mirrored C constants (Step 1 lists them)
- Create: `/tmp`-free scratch script `$SCRATCH/mirrored.py` (not committed)

**Interfaces:**
- Consumes: `dmtest.constants` (Task 15), the folders of Task 17.
- Produces: test files that read C values only through `C.<NAME>`.

- [ ] **Step 1: Find the mirrored constants**

Write `$SCRATCH/mirrored.py` (`$SCRATCH` is the SDD workspace):

```python
"""Lists module-level assignments of tests/**/*.py whose names are C constants, with the C value."""
import ast
import sys
from pathlib import Path

sys.path.insert(0, "tests")
from dmtest.constants import VALUES

for path in sorted(Path("tests").rglob("test_*.py")):
    for node in ast.parse(path.read_text()).body:
        if not isinstance(node, ast.Assign):
            continue
        targets = [t for target in node.targets for t in (target.elts if isinstance(target, ast.Tuple) else [target])]
        values = node.value.elts if isinstance(node.value, ast.Tuple) else [node.value]
        for target, value in zip(targets, values):
            if isinstance(target, ast.Name) and target.id in VALUES:
                mirrored = value.value if isinstance(value, ast.Constant) else None
                flag = "" if mirrored == VALUES[target.id] else "  DIFFERS"
                print(f"{path}:{node.lineno} {target.id} = {mirrored} C={VALUES[target.id]}{flag}")
```

Run: `uv run python $SCRATCH/mirrored.py`
Expected: the list (for example `SLOT_HAND_ACTION` in 7 files, `KEY_RETURN` in 3, the 23 lines of
`test_csb_magic_map.py`; inventory `review-platform-repo.md:133`). A line with `DIFFERS` is a drifted constant:
stop and report it to the controller (the test may test the wrong value); do not change it.

- [ ] **Step 2: Replace them file by file**

For each file of the list: delete the mirrored assignment, add `from dmtest import constants as C`, and write
`C.<NAME>` at each use. A name of the list that is also a KAOS address of the original (`G_SOUNDS`, `CHAMPIONS`,
...) is not a C constant of the port; the script does not list it.

- [ ] **Step 3: Run the changed files**

Run: `uv run pytest -q -n auto <the changed files>`
Expected: the same pass and skip counts as before.

- [ ] **Step 4: Check that no mirrored constant is left**

Run: `uv run python $SCRATCH/mirrored.py`
Expected: no output.

- [ ] **Step 5: Commit**

```bash
git add tests/
git commit -m "test: read C constants from the headers instead of mirroring them"
```

### Task 19: Skip markers, no machine paths, no make calls, ruff

**Files:**
- Modify: the test files with skip conditions, `make` calls or machine paths (Step 1)
- Modify: `pyproject.toml`, `uv.lock`

**Interfaces:**
- Consumes: `dmtest.markers` (Task 15).
- Produces: `ruff` in the `dev` group with `[tool.ruff] line-length = 160`; `make check` (package B owns the
  Makefile; Assumption: B's `check` target already runs `uv run ruff check tests tools/dmtools` or has a place for
  it; Step 6 checks) runs it.

- [ ] **Step 1: List the places**

Run: `grep -rn 'skipif\|"make"\|/private/tmp\|/Users/\|SCRATCH' tests --include=*.py`
Expected: the skip conditions (`all((GAME / f).exists() ...)` in about 10 files, the CSB variant in 7,
`shutil.which("clang")` in about 20), the `make` calls that package B left, `SCRATCH` in
`tests/port/test_automap.py` (was `tests/test_automap.py:11`) and `tests/port/test_modes.py` (was `:22`).

- [ ] **Step 2: Use the markers**

Each skip condition becomes the marker of `dmtest.markers` (`pytestmark = [needs_dm]`; CSB modules
`[needs_csb, needs_csb_mini]` where the old condition read `MINI.exists()`). `shutil.which("clang")` skips go:
the tests do not compile any more. A skip inside a fixture (`tests/test_automap.py:35`, `test_creatures.py:79`,
`test_move.py:49`) becomes a marker on the module when it skips on a missing file.

- [ ] **Step 3: Remove the machine paths**

`SCRATCH` and the PNG dumps that use it (`screenshot` of `tests/test_modes.py:97-107`, the dump of
`tests/test_automap.py:225-226`) read `DM_TEST_PNG_DIR` instead:

```python
PNG_DIR = os.environ.get("DM_TEST_PNG_DIR")  # optional: a folder for PNG renders of the screen
```

and do nothing when it is not set (as today when the scratchpad folder is missing).

- [ ] **Step 4: Remove the make calls**

Delete every `subprocess.run(["make", ...])` in a test module: `tests/conftest.py` builds the library once.
`tests/tools/test_csb_tools.py:20,60` and the tests that start `build/dm` need the game binary: Assumption: the
`conftest.py` of package B runs `make lib` only; check with `grep -n make tests/conftest.py`. If it builds only
the library, change it to `make all` (the game and the library, one command) and say so in the commit message.

- [ ] **Step 5: Add ruff and fix the findings**

```bash
uv add --dev ruff
```

`pyproject.toml`:

```toml
[tool.ruff]
line-length = 160
src = ["tests", "tools/dmtools"]
```

Run: `uv run ruff check tests tools/dmtools`
Fix each finding (unused imports, undefined names); `uv run ruff check --fix` for the safe fixes. Do not run
`ruff format`.

- [ ] **Step 6: Wire ruff into make check**

Run: `grep -n "^check" -A4 Makefile`
If the recipe has no ruff line, add `uv run ruff check tests tools/dmtools` before the suite line.

- [ ] **Step 7: Run the changed files and the checks**

Run: `uv run pytest -q -n auto tests && uv run ruff check tests tools/dmtools`
Expected: the same pass counts; skips only for missing game files, SDL or Ghidra; ruff "All checks passed!".

- [ ] **Step 8: Commit**

```bash
git add tests/ pyproject.toml uv.lock Makefile
git commit -m "test: use the dmtest skip markers, drop machine paths and make calls, add ruff"
```

#### Stream C3: the docs folders, the stale docs and the repository files

Shared files: `tests/test_symbols.py` (one path; Task 17 moves the file), `pyproject.toml` and `uv.lock` (Task 22; Task 19
adds ruff), `src/**` comments that name a doc path (no other stream of package C changes `src/`).

### Task 20: Docs folders and the link check

**Files:**
- Move: the docs of the map below (git mv)
- Modify: every file outside `docs/superpowers/` and `tests/` that names a moved doc (`README.md`, `CHANGELOG.md`,
  `CLAUDE.md`, `.claude/agents/*.md`, `docs/**/*.md`, `src/**/*.[ch]`, `tools/**`), and `tests/test_symbols.py:4`
  (the CSV path; the file is `tests/tools/test_symbols.py` after Task 17)
- Create: `tests/standard/test_docs.py`

**Interfaces:**
- Consumes: nothing of other streams.
- Produces: the docs paths below (later packages write `docs/llm/llm-mode.md`, `docs/reference/kaos-symbols.csv`,
  `docs/rules/...`); `tests/standard/test_docs.py` with `test_doc_links_resolve` (package H adds the protocol
  vocabulary check to this file); the script `$SCRATCH/relink.py` (Task 26 runs it again over `tests/`).

Move map (`docs/` paths; the names drop the redundant folder word):

| From | To |
|---|---|
| `ftl-format.md`, `dungeon-format.md`, `save-format.md` | `formats/ftl.md`, `formats/dungeon.md`, `formats/save.md` |
| `rules-champions.md`, `rules-creatures.md`, `rules-movement.md` | `rules/champions.md`, `rules/creatures.md`, `rules/movement.md` |
| `tables.md`, `game-state.md` | `rules/tables.md`, `rules/game-state.md` |
| `csb-data.md`, `csb-engine-diff.md`, `csb-new-adventure.md`, `csb-port-integration.md` | `csb/data.md`, `csb/engine-diff.md`, `csb/new-adventure.md`, `csb/port-integration.md` |
| `llm-mode.md`, `llm-playbook.md` | `llm/llm-mode.md`, `llm/llm-playbook.md` (the names that PRD R10.4 uses) |
| `kaos-symbols.csv`, `csb-appb-symbols.csv`, `kaos-map.md`, `symbols/` | `reference/kaos-symbols.csv`, `reference/csb-appb-symbols.csv`, `reference/kaos-map.md`, `reference/symbols/` |
| `coding-standard.md`, `todo.md` | stay |

- [ ] **Step 1: Write `$SCRATCH/relink.py`**

```python
"""Moves docs with git mv and rewrites every reference to a moved doc. Usage: relink.py move | relink.py paths <dir>..."""
import os
import re
import subprocess
import sys
from pathlib import Path

MOVES = {
    "docs/ftl-format.md": "docs/formats/ftl.md", "docs/dungeon-format.md": "docs/formats/dungeon.md",
    "docs/save-format.md": "docs/formats/save.md", "docs/rules-champions.md": "docs/rules/champions.md",
    "docs/rules-creatures.md": "docs/rules/creatures.md", "docs/rules-movement.md": "docs/rules/movement.md",
    "docs/tables.md": "docs/rules/tables.md", "docs/game-state.md": "docs/rules/game-state.md",
    "docs/csb-data.md": "docs/csb/data.md", "docs/csb-engine-diff.md": "docs/csb/engine-diff.md",
    "docs/csb-new-adventure.md": "docs/csb/new-adventure.md", "docs/csb-port-integration.md": "docs/csb/port-integration.md",
    "docs/llm-mode.md": "docs/llm/llm-mode.md", "docs/llm-playbook.md": "docs/llm/llm-playbook.md",
    "docs/kaos-symbols.csv": "docs/reference/kaos-symbols.csv", "docs/csb-appb-symbols.csv": "docs/reference/csb-appb-symbols.csv",
    "docs/kaos-map.md": "docs/reference/kaos-map.md", "docs/symbols/range-1-3.md": "docs/reference/symbols/range-1-3.md",
    "docs/symbols/range-4-6.md": "docs/reference/symbols/range-4-6.md", "docs/symbols/range-7-9.md": "docs/reference/symbols/range-7-9.md",
}
ARCHIVE = [line for line in Path(os.environ.get("ARCHIVE_LIST", "/dev/null")).read_text().split() if line]
MOVES.update({p: p.replace("docs/superpowers/", "docs/superpowers/archive/", 1) for p in ARCHIVE})
SKIP = ("docs/superpowers/",)  # plans keep their text: their paths and line numbers are their evidence
LINK = re.compile(r"(\]\()([^)#\s]+)")
TEXT = (".md", ".c", ".h", ".py", ".sh", ".toml", ".java", ".txt")


def move():
    for old, new in MOVES.items():
        if Path(old).exists():
            Path(new).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["git", "mv", old, new], check=True)


def relink(path, old_location):
    """Rewrites repository paths (docs/x) and relative Markdown links of one file that lived at old_location."""
    text = original = path.read_text()
    for old, new in sorted(MOVES.items(), key=lambda kv: -len(kv[0])):
        text = re.sub(rf"(?<![\w/]){re.escape(old)}(?![\w-])", new, text)
    if path.suffix == ".md":
        def fix(m):
            target = m.group(2)
            if "://" in target or target.startswith(("mailto:", "#")):
                return m.group(0)
            resolved = os.path.normpath(os.path.join(os.path.dirname(old_location), target))
            resolved = MOVES.get(resolved, resolved)
            return m.group(1) + os.path.relpath(resolved, os.path.dirname(path.as_posix()))
        text = LINK.sub(fix, text)
    if text != original:
        path.write_text(text)


def paths(folders):
    back = {new: old for old, new in MOVES.items()}
    files = subprocess.run(["git", "ls-files", *folders], capture_output=True, text=True, check=True).stdout.split()
    for name in files:
        if name.endswith(TEXT) and not name.startswith(SKIP) and Path(name).exists():
            relink(Path(name), back.get(name, name))


if __name__ == "__main__":
    move() if sys.argv[1] == "move" else paths(sys.argv[2:])
```

- [ ] **Step 2: Move and relink**

Run: `uv run python $SCRATCH/relink.py move && uv run python $SCRATCH/relink.py paths docs README.md CHANGELOG.md CLAUDE.md .claude src tools`
Expected: the moves of the map; `git diff --stat` shows link changes in the files that name a moved doc
(the list of the plan base: `.claude/agents/dm-coordinator.md`, `.claude/agents/dm-player.md`, `CHANGELOG.md`,
`README.md`, `src/data/csbimage.c`, `src/data/csbtext.c`, `src/data/tables.c`, `src/data/tables.h`,
`src/engine/action.h`, `champion.h`, `group.h`, `inventory.h`, `magic.h`, `map.h`, `projectile.h`, `savefile.h`,
`savegame.h`, `src/platform/control.c`, `control.h`, `state_json.h`, `src/ui/anim.h`, `tools/csb_match.py`,
`tools/research/platforms/dm2gdat.py` and `dm2sounds.py` name `docs/skull-formats.md`, which does not exist and
is not moved). `CHANGELOG.md` keeps its released entries: revert the hunks of `CHANGELOG.md` below the
`## Unreleased` section (`git add -p` / edit) so released notes keep their historic paths.

- [ ] **Step 3: Fix the CSV path of the symbol test**

`tests/test_symbols.py:4` (`CSV = ROOT / "docs" / "kaos-symbols.csv"`) becomes
`CSV = ROOT / "docs" / "reference" / "kaos-symbols.csv"`. Do not change other test files: Task 26 relinks the
docstrings of `tests/` after the merge.

- [ ] **Step 4: Write `tests/standard/test_docs.py`**

```python
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
```

- [ ] **Step 5: Run the link check and the symbol test**

Run: `uv run pytest -q tests/standard/test_docs.py tests/test_symbols.py`
Expected: `test_symbols` PASS; `test_doc_links_resolve` fails only on the three `range-*.csv` links of
`docs/reference/symbols/range-*.md:3` (these CSV files do not exist; Task 21 fixes them). Any other broken link:
fix it in this task.

- [ ] **Step 6: Commit**

```bash
git add -A docs README.md CHANGELOG.md CLAUDE.md .claude src tools tests/test_symbols.py tests/standard/test_docs.py
git commit -m "docs: group the docs into formats, rules, csb, llm and reference folders"
```

### Task 21: Stale docs and the plan archive

**Files:**
- Delete: `docs/superpowers/status.md`
- Move: the finished plans and inventories to `docs/superpowers/archive/`
- Modify: `docs/llm/llm-mode.md`, `docs/rules/game-state.md`, `docs/csb/port-integration.md`,
  `docs/reference/symbols/range-*.md`, `docs/reference/kaos-map.md`, `src/ui/advanced.h`, `src/ui/music.h`,
  `docs/reference/kaos-map.md`, `README.md` (links only)

**Interfaces:**
- Consumes: Task 20 (`relink.py`, the new paths).
- Produces: `docs/superpowers/archive/`; the active PRD, plan and inventory of the final refactor stay in
  `docs/superpowers/` (they are in use until the run ends).

- [ ] **Step 1: Archive the finished plans**

```bash
cat > $SCRATCH/archive.txt <<'EOF'
docs/superpowers/2026-10-04-csb-plan.md
docs/superpowers/2026-10-04-dm-c-port-plan.md
docs/superpowers/2026-10-04-modes-plan.md
docs/superpowers/2026-10-05-music-decisions.md
docs/superpowers/2026-10-06-coding-standard-refactor-plan.md
docs/superpowers/2026-10-15-clean-code.md
docs/superpowers/2026-10-15-clean-code-plan.md
docs/superpowers/2026-10-15-clean-code-checklist.md
docs/superpowers/2026-10-15-clean-code-inventory
EOF
mkdir -p docs/superpowers/archive
for p in $(cat $SCRATCH/archive.txt); do git mv "$p" docs/superpowers/archive/; done
git rm -q docs/superpowers/status.md
ARCHIVE_LIST=$SCRATCH/archive.txt uv run python $SCRATCH/relink.py paths docs README.md CLAUDE.md src
```

Expected: the references in `src/ui/advanced.h` (modes plan), `src/ui/music.h` (music decisions),
`README.md:16` and `docs/reference/kaos-map.md:3` (C port plan), `docs/llm/llm-mode.md:5` (modes plan) point
into `archive/`. `docs/csb/port-integration.md` names `docs/superpowers/status.md`: Step 4 removes that sentence.

- [ ] **Step 2: Fix the C API table of the LLM mode page**

`docs/llm/llm-mode.md:250-253` (was `docs/llm-mode.md`) says `int control_open` "Returns 1 on success" and
`int control_is_turn_pending` "1 while a turn runs"; the code has `bool` (`src/platform/control.h:12,16`).
New rows:

```markdown
| `bool control_open(const char *path)` | Listens on the socket. Replaces an old socket file. Returns false on an error. |
| `bool control_is_turn_pending(void)` | True while a turn runs. |
```

(Package H rewrites this table for `control_handle_line` and the transport.)

- [ ] **Step 3: Fix the step tables line of the game state page**

`docs/rules/game-state.md:196` names `g_direction_to_step_east` (`0x101038`) and `g_direction_to_step_north`,
which the port does not have; it has `tables_get_step_east` and `tables_get_step_north`
(`src/data/tables.h:183-184`). New text of the sentence: "Reads the step tables through `tables_get_step_east`
and `tables_get_step_north` (KAOS `0x101038`: 0, 1, 0, −1 and `0x101040`: −1, 0, 1, 0)."

- [ ] **Step 4: Fix the status line of the CSB integration page**

`docs/csb/port-integration.md:5` says "Status: proposal. Nothing is implemented." The CSB adventure is
implemented (`CHANGELOG.md`, `src/data/csbimage.c`, `src/ui/select.c`). New line: "Status: implemented. This page
is the design record of the CSB integration; the code and `docs/csb/engine-diff.md` are the reference." Remove
the sentence that links `docs/superpowers/status.md`.

- [ ] **Step 5: Fix the symbol pages and mark the reference pages**

`docs/reference/symbols/range-1-3.md:3`, `range-4-6.md:3`, `range-7-9.md:3` link `range-*.csv`, which do not
exist. Replace "The symbol list is [range-4-6.csv](range-4-6.csv)." (and the two others) by "The symbols are in
[kaos-symbols.csv](../kaos-symbols.csv)." Then add one line after the title of each page of `docs/reference/`
(the three range pages and `kaos-map.md`): "This page uses the original KAOS names; the port names differ."

- [ ] **Step 6: Run the link check**

Run: `uv run pytest -q tests/standard/test_docs.py`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add -A docs src/ui README.md CLAUDE.md
git commit -m "docs: archive the finished plans and fix the stale pages"
```

### Task 22: Repository files: LICENSE, .editorconfig, the mcp extra, dm-csb-match

**Files:**
- Create: `LICENSE`, `.editorconfig`
- Move: `tools/csb_match.py` → `tools/dmtools/csb_match.py`
- Modify: `pyproject.toml`, `uv.lock`

**Interfaces:**
- Consumes: Task 20 (`docs/reference/` paths).
- Produces: `uv run dm-csb-match --kaos ... --kaos-csv ... --appb ... --appb-c ... --csv ...`;
  `uv sync --extra mcp`. Package I names both in `README.md` and `docs/development.md`, and the license sentence of
  R14.1 in `README.md`.

- [ ] **Step 1: Copy the GPL-3.0 text**

The text comes from a verbatim copy on this machine (no download): Homebrew packages ship it (for example
`/opt/homebrew/Cellar/gmp/*/COPYING`). The canonical `gpl-3.0.txt` has the MD5 `d32239bcb673463ab874e80d47fae504`.

```bash
src=$(for f in /opt/homebrew/Cellar/*/*/COPYING*; do [ "$(md5 -q "$f" 2>/dev/null)" = d32239bcb673463ab874e80d47fae504 ] && echo "$f" && break; done)
test -n "$src" && cp "$src" LICENSE && head -2 LICENSE
```

Expected: "GNU GENERAL PUBLIC LICENSE" / "Version 3, 29 June 2007". If no file matches, stop and ask the owner
for the text (do not fetch it from the network).

- [ ] **Step 2: Write `.editorconfig`**

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true

[*.{c,h,py}]
indent_style = space
indent_size = 4

[Makefile]
indent_style = tab
```

- [ ] **Step 3: Make mcp optional**

`pyproject.toml`: remove `"mcp>=2.3.0"` from `dependencies` (`pyproject.toml:8`) and add

```toml
[project.optional-dependencies]
mcp = ["mcp>=2.3.0"]
```

Add `"mcp>=2.3.0"` to the `dev` group too: `test_mcp_tools` (`tests/test_control.py:207`) imports it, and the
suite keeps it. Run `uv lock`.

- [ ] **Step 4: Move csb_match into the package with its paths as arguments**

```bash
git mv tools/csb_match.py tools/dmtools/csb_match.py
```

In `tools/dmtools/csb_match.py`: the module constants `KAOS_FTL, KAOS_CSV` and `CSB_FTL, CSB_C`
(`tools/csb_match.py:36-37`) become `main` arguments with these defaults (relative to the working directory, as
today), and the functions that read them (`:76`, `:81`, `:378`, `:392`) take them as parameters:

```python
    ap.add_argument("--kaos", default="game/KAOS.FTL", help="the DM engine module")
    ap.add_argument("--kaos-csv", default="docs/reference/kaos-symbols.csv", help="the DM symbol table")
    ap.add_argument("--appb", default="game-csb/APPB.FTL", help="the CSB engine module")
    ap.add_argument("--appb-c", default="reference/decomp/csb-a/APPB.c", help="the Ghidra output of APPB.FTL")
    ap.add_argument("--csv", default="docs/reference/csb-appb-symbols.csv")
```

The usage line of the docstring (`:3`) becomes `Usage: uv run dm-csb-match [--csv OUT] [--report OUT] [--diff KAOS_ADDR]`.
`pyproject.toml` `[project.scripts]`: `dm-csb-match = "dmtools.csb_match:main"`.

- [ ] **Step 5: Check the entry points**

Run: `uv sync && uv run dm-csb-match --help && uv run python -c "import dmtools.mcp_server" `
Expected: the help text with the five path options; the import works (the dev group has mcp).

- [ ] **Step 6: Commit**

```bash
git add LICENSE .editorconfig pyproject.toml uv.lock tools/
git commit -m "chore: add the GPL-3.0 license, .editorconfig, the mcp extra and dm-csb-match"
```

#### Tail

Shared files: every file of `src/`, `tests/`, `tools/dmtools/` (the tail runs after the merge of C1, C2, C3).

### Task 23: SPDX lines and the include guard scheme

**Files:**
- Modify: every `src/**/*.[ch]`, `tests/**/*.py`, `tools/dmtools/**/*.py`
- Modify: `.git-blame-ignore-revs`, `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: the merged package C.
- Produces: line 1 of every C file is `/* SPDX-License-Identifier: GPL-3.0-or-later */`; every Python file of
  `tests/` and `tools/dmtools` starts with `# SPDX-License-Identifier: GPL-3.0-or-later` (after a shebang); every
  header guard is `DM_<LAYER>_<FILE>_H`. Every new file of a later task starts the same way.

- [ ] **Step 1: Check the guards**

Run: `for f in src/*/*.h; do head -2 "$f" | grep -q '^#ifndef \(.*\)$' || echo "$f"; done`
Expected: no output (every header starts with its guard, as `src/engine/sensor.h:1-2` today). A header that
starts with something else: move its guard to lines 1-2 by hand first.

- [ ] **Step 2: Run the script**

```python
"""Adds the SPDX line to every source file and renames the include guards to DM_<LAYER>_<FILE>_H."""
import re
import sys
from pathlib import Path

C_SPDX = "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
PY_SPDX = "# SPDX-License-Identifier: GPL-3.0-or-later\n"

for path in sorted(Path("src").rglob("*.[ch]")):
    text = path.read_text()
    if path.suffix == ".h":
        guard = f"DM_{path.parent.name.upper()}_{path.stem.upper()}_H"
        m = re.match(r"#ifndef (\w+)\n#define \1\n", text)
        if not m:
            sys.exit(f"{path}: no include guard on lines 1-2")
        text = f"#ifndef {guard}\n#define {guard}\n" + text[m.end():]
        text = re.sub(rf"#endif\s*/\*\s*{m.group(1)}\s*\*/", f"#endif /* {guard} */", text)
    if not text.startswith(C_SPDX):
        text = C_SPDX + text
    path.write_text(text)

for path in sorted([*Path("tests").rglob("*.py"), *Path("tools/dmtools").rglob("*.py")]):
    text = path.read_text()
    if text.startswith(PY_SPDX) or PY_SPDX in text.split("\n", 2)[1:2]:
        continue
    if text.startswith("#!"):
        first, _, rest = text.partition("\n")
        text = f"{first}\n{PY_SPDX}{rest}"
    else:
        text = PY_SPDX + text
    path.write_text(text)
```

Run: `uv run python $SCRATCH/spdx.py && make format && make && make lib`
Expected: 0 warnings.

- [ ] **Step 3: Run the header check and the tests of the helpers**

Run: `uv run pytest -q tests/standard tests/tools/test_dmtest_constants.py`
Expected: PASS (the `header` open lines of the guard and SPDX causes are stale now; Task 26 prunes them).

- [ ] **Step 4: Commit and record the commit in the blame ignore list**

```bash
git add -A src tests tools/dmtools
git commit -m "chore: add SPDX license lines and the DM_<LAYER>_<FILE>_H include guards"
git rev-parse HEAD >> .git-blame-ignore-revs
git add .git-blame-ignore-revs
git commit -m "chore: ignore the SPDX commit in git blame"
```

### Task 24: Anonymous enums become #define lines

**Files:**
- Modify: every `src/**/*.[ch]` with an anonymous `enum { ... };`
- Modify: `.git-blame-ignore-revs`

**Interfaces:**
- Consumes: `dmtest.constants.evaluate`, `VALUES` (Task 15); Task 23.
- Produces: no anonymous enum in `src/`; each former enum is a block of `#define NAME value` lines with written
  values (PRD R3.5: the 829 anonymous members and the 133 implicit values). Named enums and enums that declare a
  field stay.

- [ ] **Step 1: Check that no enum member feeds the preprocessor**

Run: `grep -rn "^\s*#\s*if" src | grep -v "ifndef DM_\|_WIN32\|defined("`
Expected: no line that names an enum member (an enum member in `#if` is 0 for the preprocessor; a `#define`
would change that line). For each line, check its names with `grep -rn "enum.*\bNAME\b" src`; a hit stops the task
for the controller.

- [ ] **Step 2: Write `$SCRATCH/enum_to_define.py`**

```python
"""Turns every anonymous enum of src/ into #define lines with written values. Named enums stay."""
import re
import sys
from pathlib import Path

sys.path.insert(0, "tests")
from dmtest.constants import VALUES, evaluate

ANON = re.compile(r"^enum\s*\{(.*?)\}\s*;[ \t]*(/\*.*?\*/)?[ \t]*\n", re.M | re.S)
ITEM = re.compile(r"\s*([A-Z][A-Z0-9_]*)\s*(?:=\s*((?:[^,/]|/(?!\*))+?))?\s*(?:,|$)\s*(/\*.*?\*/)?", re.S)
LITERAL = re.compile(r"^-?(?:0x[0-9a-fA-F]+|\d+)$")


def defines(body, trailing):
    items = [(m.group(1), (m.group(2) or "").strip(), m.group(3)) for m in ITEM.finditer(body) if m.group(1)]
    hex_widths = [len(e) - 2 for _, e, _ in items if e.lower().startswith("0x")]
    known, previous, out = dict(VALUES), None, []
    for name, expression, comment in items:
        if expression:
            text = expression
            try:
                known[name] = evaluate(expression, known)
            except (KeyError, ValueError):
                known.pop(name, None)
        elif previous is None:
            known[name], text = 0, "0"
        elif previous in known:
            known[name] = known[previous] + 1
            text = None
        else:
            text = f"({previous} + 1)"
        if text is None:
            value = known[name]
            text = f"0x{value:0{max(hex_widths)}x}" if hex_widths and value >= 0 else str(value)
        if text.startswith("-") and LITERAL.match(text):
            text = f"({text})"
        elif not LITERAL.match(text) and not (text.startswith("(") and text.endswith(")")):
            text = f"({text})"
        out.append(f"#define {name} {text}" + (f" {comment}" if comment else ""))
        previous = name
    if trailing:
        out[0] += f" {trailing}"
    return "\n".join(out) + "\n"


for path in sorted(Path("src").rglob("*.[ch]")):
    text = path.read_text()
    new = ANON.sub(lambda m: defines(m.group(1), m.group(2)), text)
    if new != text:
        path.write_text(new)
        print(path)
```

The pattern matches only `enum {` at column 0 that ends with `};`: a named enum (`enum MoveResult {`), a
`typedef enum`, a field (`} kind;`) and an enum inside a function stay. Step 4 lists what stays.

- [ ] **Step 3: Run it, format, build**

Run: `uv run python $SCRATCH/enum_to_define.py && make format && make && make lib`
Expected: the list of changed files; 0 warnings.

- [ ] **Step 4: Check what stays**

Run: `grep -rn "enum\s*{" src`
Expected: no line, or only enums inside a function or with a field name; list each in the commit message (the
`enum` open lines of their layer epic stay).

- [ ] **Step 5: Run the tests of the values**

Run: `uv run pytest -q -n auto tests/golden tests/differential tests/standard`
Expected: PASS. The values are the same; a difference is a script error.

- [ ] **Step 6: Commit and record the commit in the blame ignore list**

```bash
git add -A src
git commit -m "refactor: turn the anonymous enums into #define constants with written values"
git rev-parse HEAD >> .git-blame-ignore-revs
git add .git-blame-ignore-revs
git commit -m "chore: ignore the enum-to-define commit in git blame"
```

### Task 25: Public function comments move to the header

**Files:**
- Modify: every `src/**/*.[ch]` with a public function whose comment stands above its definition

**Interfaces:**
- Consumes: Tasks 12 (`module_files`, `comment_above`, `definitions`, `functions`) and 23.
- Produces: the comment of a public function stands above its prototype in the header (standard rule 9.1).

- [ ] **Step 1: Write `$SCRATCH/move_comments.py`**

```python
"""Moves the comment of a public function from its definition in a .c file to its prototype in the header."""
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, "tests/standard")
from test_standard import comment_above, definitions, functions, headers, module_files

original = {}
deletions = defaultdict(list)  # .c path -> [(first line index, line count)], on the unchanged file
inserts = defaultdict(list)    # header path -> [(prototype line, comment lines)]
for header in headers():
    hlines = original.setdefault(header, header.read_text().splitlines())
    for f in functions(header):
        if f.static or comment_above(hlines, f.line):
            continue
        for c in sorted(p for p in module_files(header) if p.suffix == ".c"):
            clines = original.setdefault(c, c.read_text().splitlines())
            start = next((s for name, s, _, static in definitions(c) if name == f.name and not static), None)
            comment = comment_above(clines, start) if start else ""
            if comment:
                count = comment.count("\n") + 1
                deletions[c].append((start - 1 - count, count))
                inserts[header].append((f.line, comment.split("\n")))
                break
for path, lines in original.items():
    lines = list(lines)
    for first, count in sorted(deletions.get(path, []), reverse=True):
        del lines[first:first + count]
    for line, block in sorted(inserts.get(path, []), reverse=True):
        lines[line - 1:line - 1] = block
    if lines != original[path]:
        path.write_text("\n".join(lines) + "\n")
```

All positions come from the unchanged files; the deletions and the inserts apply bottom-up, so no position moves
before it is used.

- [ ] **Step 2: Run it, format, build, check**

Run: `uv run python $SCRATCH/move_comments.py && make format && make && git diff --stat | tail -1 && uv run pytest -q tests/standard/test_standard.py -k comments`
Expected: 0 warnings; the comment check passes with the open lines that remain. Read three moved comments in
`git diff` (one header of `data/`, `engine/`, `ui/`): each stands directly above its prototype. If a `.c`
deletion removed the wrong lines, revert the file and fix the script.

- [ ] **Step 3: Commit**

```bash
git add -A src
git commit -m "refactor: move the public function comments to the headers"
```

### Task 26: Docs paths in the tests and the stale allowlist lines

**Files:**
- Modify: `tests/**/*.py` (doc paths in docstrings and comments), `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: `$SCRATCH/relink.py` (code in Task 20 Step 1), `tests/standard/open_lines.py --prune` (Task 9).
- Produces: an allowlist with only live lines; the state that package D starts from.

- [ ] **Step 1: Relink the doc paths of the tests**

Run: `uv run python $SCRATCH/relink.py paths tests && git diff --stat tests | tail -1`
Expected: the docstrings that named `docs/csb-engine-diff.md`, `docs/kaos-symbols.csv`, `docs/csb-data.md`,
`docs/csb-new-adventure.md` now name the new paths. Create `relink.py` from Task 20 Step 1 if the workspace of
stream C3 is not at hand.

- [ ] **Step 2: Prune the stale open lines**

Run: `uv run python tests/standard/open_lines.py --prune && uv run python tests/standard/open_lines.py && uv run pytest -q tests/standard`
Expected: "N lines removed"; then "0 lines added" (a new line here means the tail created a violation: fix it
instead); then PASS.

- [ ] **Step 3: Report the open lines per epic**

Run: `grep -o 'open: epic [0-9]*' tests/standard/allowlist.txt | sort | uniq -c`
Write the counts into the SDD ledger (the later packages remove them).

- [ ] **Step 4: Commit**

```bash
git add tests/
git commit -m "test: relink the doc paths of the tests and prune the stale allowlist lines"
```

---

## Package D: data/ and audio (Epics 4 and 9, R5.7)

Package D brings `src/data/` to the target of R4 and moves the audio output out of the engine (R5.7, R9). Four
streams run in parallel; a tail of six tasks runs after the merge, because those tasks rename across the tree or
need the results of two streams.

Facts cite `port/src/<path>:<line>` of commit `72e24b5`; after package B the files are under `src/` with the same
names. Test modules are named by file; after package C they sit in the R1 folders, so the commands use the glob
`tests/*/test_x.py`.

Rules for every task of this package:

- Each task deletes the `open: epic 4` lines of `tests/standard/allowlist.txt` whose violation it removes, in the
  same commit (`grep -n "open: epic 4" tests/standard/allowlist.txt`).
- A new `.c` or `.h` file starts with `/* SPDX-License-Identifier: GPL-3.0-or-later */` (the package C tail
  added the line to every existing file) and, for a header, the head of standard rule 2.1.
- Verification of a task: `make` (game and library, 0 warnings), then the named pytest command, then
  `uv run pytest -q tests/standard` (the checks must not fail on a line this task added).
- The constant `enum` blocks of `data/` stay `enum` in this package; the constant script of the package F tail
  (R6.7) turns them into `#define` lines for every layer at once.

Ruling of this package (record it in the ledger): the `kaos` check of the standard (rule 3.6, "a `kaos_*` call
outside `data/`") wins over the wording of R4.1 ("`engine/dungeon.c:371` calls `kaos_is_loaded`"). Task 28
changes `dungeon.c` to `kaos_is_loaded()` with an `open: epic 4` line, because `tables.h` belongs to stream D2;
Task 42 replaces it with `tables_is_loaded()` and deletes the line.

#### Stream D1: kaos module, FTL loader, os hooks

Shared files: `src/data/tables.c` (Task 28 changes `tables_init` lines `port/src/data/tables.c:199-202` and
`tables_set_mouse_input_box` `port/src/data/tables.c:771-778`; stream D2 deletes the first block and keeps the
second; at the merge take the stream D2 version of `tables_init` and the stream D1 version of
`tables_set_mouse_input_box`), `src/engine/dungeon.c` (one line), `src/data/spells.c` (include line only).

### Task 27: Log and fatal hooks in data/os

**Files:**
- Modify: `src/data/os.h`, `src/data/os.c`

**Interfaces:**
- Consumes: nothing.
- Produces (used by Task 28 and by the packages E and F for `game_fatal`, `map.c:159`, `group.c:307`):
  ```c
  typedef void (*OsLogHook)(const char *message);
  typedef void (*OsFatalHook)(const char *message);
  extern OsLogHook g_os_log_hook;     /* NULL: the message goes to stderr */
  extern OsFatalHook g_os_fatal_hook; /* NULL: the message goes to stderr */
  void os_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
  _Noreturn void os_fatal(const char *format, ...) __attribute__((format(printf, 1, 2)));
  ```

Facts: `data/os.h` has only the folder hooks (`port/src/data/os.h:7-17`); the stderr writes of `data/` and
`engine/` are `port/src/data/ftl.c:276-277`, `port/src/engine/game.c:58-59`, `port/src/engine/map.c:159-166`,
`port/src/engine/group.c:307-308`. Standard rule 10.1: `data/os.c` holds the default (stderr and `exit`), and
`os_fatal` never returns. This task adds the hooks only; Task 28 uses `os_fatal` in `data/`, the packages E and
F replace the engine writes.

- [ ] **Step 1: Add the declarations to `src/data/os.h`.** Insert them after the line `extern OsListFolder
  g_os_list_folder_hook;` (`port/src/data/os.h:10`), not at the end of the file: stream D3 deletes
  `os_compare_names` at the end (Task 37), so the two hunks stay apart. The header needs no new include. Text:

  ```c
  /* Messages and fatal errors of data/ and engine/. platform/ sets the hooks to its log; tests set their own. */
  typedef void (*OsLogHook)(const char *message);
  typedef void (*OsFatalHook)(const char *message);
  extern OsLogHook g_os_log_hook;     /* NULL: the message goes to stderr */
  extern OsFatalHook g_os_fatal_hook; /* NULL: the message goes to stderr */
  /* Formats a message as printf and logs it. */
  void os_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
  /* Formats a message as printf, reports it, and ends the program: a port bug, never bad game data. */
  _Noreturn void os_fatal(const char *format, ...) __attribute__((format(printf, 1, 2)));
  ```

  Change the purpose comment of the header (`port/src/data/os.h:6`) to: `/* The operating system services that
  data/ and engine/ need: folders, the log, and fatal errors. platform/ sets the hooks; tests set their own. */`.

- [ ] **Step 2: Add the definitions to `src/data/os.c`** after `g_os_list_folder_hook` (`port/src/data/os.c:5`):

  ```c
  #define OS_MESSAGE_SIZE 512 /* longest log message */

  OsLogHook g_os_log_hook;
  OsFatalHook g_os_fatal_hook;

  /* Sends a formatted message to the hook, or to stderr without a hook. */
  static void report(OsLogHook hook, const char *format, va_list args)
  {
      char message[OS_MESSAGE_SIZE];
      vsnprintf(message, sizeof message, format, args);
      if (hook)
          hook(message);
      else
          fprintf(stderr, "%s\n", message);
  }

  void os_log(const char *format, ...)
  {
      va_list args;
      va_start(args, format);
      report(g_os_log_hook, format, args);
      va_end(args);
  }

  _Noreturn void os_fatal(const char *format, ...)
  {
      va_list args;
      va_start(args, format);
      report(g_os_fatal_hook, format, args);
      va_end(args);
      exit(EXIT_FAILURE);
  }
  ```

  Includes of `os.c` (sorted by `make format`): `"data/os.h"`, `<ctype.h>`, `<stdarg.h>`, `<stdio.h>`,
  `<stdlib.h>`. The `#define` goes above the globals (standard rule 8.1: constants before state).

- [ ] **Step 3: Verify.** `make`; `make format`; `uv run pytest -q tests/standard`. No pytest of game code:
  the hooks are wiring (no test, CLAUDE.md §2).

- [ ] **Step 4: Commit.** `git add src/data/os.h src/data/os.c && git commit -m "feat(data): log and fatal hooks in data/os"`

### Task 28: The kaos module owns the engine image

**Files:**
- Create: `src/data/kaos.h`, `src/data/kaos.c`
- Modify: `src/data/ftl.h`, `src/data/ftl.c`, `src/data/swsh.c`, `src/data/tables.c` (two functions),
  `src/data/spells.c` (include), `src/engine/dungeon.c:371`, tests that read the `kaos` global (none: checked by
  `grep -rn '"kaos"' tests` in Step 1), `tests/standard/allowlist.txt`

**Interfaces:**
- Consumes: `os_fatal` (Task 27).
- Produces (`src/data/kaos.h`):
  ```c
  int kaos_load(const char *path);               /* 0 or -1; Task 46 makes it bool */
  void kaos_free(void);
  bool kaos_is_loaded(void);
  const uint8_t *kaos_ptr(uint32_t address);
  uint8_t kaos_u8(uint32_t address);
  int8_t kaos_s8(uint32_t address);              /* used by Task 42 */
  uint16_t kaos_u16(uint32_t address);
  int16_t kaos_s16(uint32_t address);
  uint32_t kaos_u32(uint32_t address);
  void kaos_write_u16(uint32_t address, uint16_t value);
  ```
  and in `src/data/ftl.h`: `uint8_t *ftl_at(const FtlImage *image, uint32_t address, uint32_t bytes);` (used
  by Task 42 in `csb_image.c`).

Facts: `ftl.h` holds the loader and the KAOS singleton under two prefixes (`port/src/data/ftl.h:3,12-22`);
`extern FtlImage kaos` has no `g_` (`port/src/data/ftl.h:15`); `kaos_load` and the readers are
`port/src/data/ftl.c:257-301`; the reader aborts with `fprintf` (`port/src/data/ftl.c:273-280`); `kaos_s16`
converts a `uint16_t` to `int16_t` without a cast (`port/src/data/ftl.c:294-297`); `swsh.c` forms a pointer
1 MiB before its buffer (`port/src/data/swsh.c:26`); `engine/dungeon.c:371` and `data/tables.c:201` read
`kaos.data`; the one image write is `tables_set_mouse_input_box` through a writable `kaos_ptr`
(`port/src/data/tables.c:771-778`). Readers of `kaos_*`: `data/tables.c`, `data/spells.c`, `data/ftl.c`
(`grep -rln "kaos_" port/src`).

- [ ] **Step 1: Check the users.** Run `grep -rn "kaos\.\|&kaos\b\|\"kaos\"" src tests` and
  `grep -rln "kaos_ptr\|kaos_u8\|kaos_u16\|kaos_s16\|kaos_u32\|kaos_load" src`. Expected: the five `kaos.`
  lines of `ftl.c`, `tables.c:201`, `dungeon.c:371`; no test reads the global. If a test reads `kaos`, change it
  to `kaos_is_loaded()` in this task.

- [ ] **Step 2: Create `src/data/kaos.h`:**

  ```c
  /* SPDX-License-Identifier: GPL-3.0-or-later */
  #ifndef DM_DATA_KAOS_H
  #define DM_DATA_KAOS_H
  /* Prefixes: kaos_ */

  /* The loaded engine image (KAOS.FTL, or the KAOS-layout image of the CSB APPB.FTL) and its readers at the
     addresses of the original. Only data/ reads it; the other layers call tables_get_*. */

  #include <stdbool.h>
  #include <stdint.h>

  /* Setup */
  /* Loads the engine module at path: KAOS.FTL, or for CSB APPB.FTL as a KAOS-layout image. Returns 0 or -1. */
  int kaos_load(const char *path);
  /* Frees the image. */
  void kaos_free(void);

  /* Queries */
  /* True after a successful kaos_load. */
  bool kaos_is_loaded(void);
  /* The image byte at an address of the original; a port bug outside the image. */
  const uint8_t *kaos_ptr(uint32_t address);
  uint8_t kaos_u8(uint32_t address);
  int8_t kaos_s8(uint32_t address);
  uint16_t kaos_u16(uint32_t address);
  int16_t kaos_s16(uint32_t address);
  uint32_t kaos_u32(uint32_t address);

  /* Commands */
  /* Writes a big-endian word into the image: the mouse input boxes of the endgame screen. */
  void kaos_write_u16(uint32_t address, uint16_t value);

  #endif
  ```

- [ ] **Step 3: Add `ftl_at` to `src/data/ftl.h` and `src/data/ftl.c`.** In `ftl.h` delete the `kaos` lines
  (`port/src/data/ftl.h:15-22`), set the prefix line to `/* Prefixes: ftl_ */`, give the header a purpose line
  (`/* The FTL module loader of the Amiga engine files: lays a module out in memory at its base address. */`) and
  add after `ftl_free`:

  ```c
  /* The bytes of an image at an address of the original, or NULL when they are not all inside the image. */
  uint8_t *ftl_at(const FtlImage *image, uint32_t address, uint32_t bytes);
  ```

  In `ftl.c`, after `ftl_free`:

  ```c
  uint8_t *ftl_at(const FtlImage *image, uint32_t address, uint32_t bytes)
  {
      if (!image->data || address < image->base || (uint64_t)address - image->base + bytes > image->size)
          return NULL;
      return image->data + (address - image->base);
  }
  ```

- [ ] **Step 4: Move the singleton into `src/data/kaos.c`.** Cut `kaos_load`, `get_image_pointer` and the
  readers out of `ftl.c` (`port/src/data/ftl.c:257-301`) and drop the `FtlImage kaos;` definition there
  (`grep -n "^FtlImage kaos" src/data/ftl.c`). `kaos.c`:

  ```c
  /* SPDX-License-Identifier: GPL-3.0-or-later */
  #include "data/kaos.h"
  #include "data/adventure.h"
  #include "data/bytes.h"
  #include "data/csbimage.h"
  #include "data/file.h"
  #include "data/ftl.h"
  #include "data/kaos_addresses.h"
  #include "data/os.h"
  #include <inttypes.h>
  #include <stdlib.h>

  static FtlImage image; /* the engine image; image.data is owned */

  /* Setup */
  int kaos_load(const char *path)
  {
      size_t len;
      uint8_t *raw = file_read(path, &len);
      FtlImage appb = {0};
      bool csb = g_adventure == ADVENTURE_CSB;
      kaos_free();
      int r = raw ? ftl_load(raw, len, KAOS_BASE, csb ? &appb : &image) : -1;
      free(raw);
      if (!r && csb)
          r = csb_image_build(&appb, &image);
      ftl_free(&appb);
      return r;
  }
  void kaos_free(void)
  {
      ftl_free(&image);
  }

  /* Queries */
  bool kaos_is_loaded(void)
  {
      return image.data != NULL;
  }

  /* The bytes of the image at an address of the original; ends the program outside the image. */
  static uint8_t *get_bytes(uint32_t address, uint32_t bytes)
  {
      uint8_t *p = ftl_at(&image, address, bytes);
      if (!p)
          os_fatal("kaos address %#" PRIx32 " outside the image", address);
      return p;
  }
  const uint8_t *kaos_ptr(uint32_t address)
  {
      return get_bytes(address, 1);
  }
  uint8_t kaos_u8(uint32_t address)
  {
      return *get_bytes(address, 1);
  }
  int8_t kaos_s8(uint32_t address)
  {
      return (int8_t)*get_bytes(address, 1); /* sign of the byte */
  }
  uint16_t kaos_u16(uint32_t address)
  {
      return BE16(get_bytes(address, sizeof(uint16_t)));
  }
  int16_t kaos_s16(uint32_t address)
  {
      return (int16_t)kaos_u16(address); /* sign of the word */
  }
  uint32_t kaos_u32(uint32_t address)
  {
      return BE32(get_bytes(address, sizeof(uint32_t)));
  }

  /* Commands */
  void kaos_write_u16(uint32_t address, uint16_t value)
  {
      WR16(get_bytes(address, sizeof(uint16_t)), value);
  }
  ```

  The `kaos_load` body is the code of `port/src/data/ftl.c:257-270` with `kaos` renamed to `image`. Remove the
  includes that `ftl.c` no longer uses (`adventure.h`, `csbimage.h`, `file.h`, `kaos_addresses.h`, `stdio.h`
  if nothing else needs them: check with `grep -n "g_adventure\|csb_image\|file_read\|KAOS_\|printf" src/data/ftl.c`).

- [ ] **Step 5: Fix `swsh.c`.** Replace `port/src/data/swsh.c:26-35` by reads through `ftl_at` (the size check
  of line 20 makes every result non-NULL):

  ```c
      const FtlImage *image = &out->image;
      out->palette_script = ftl_at(image, SWSH_PALETTE_SCRIPT, SWSH_LOGO_GRAPHIC - SWSH_PALETTE_SCRIPT);
      out->logo = ftl_at(image, SWSH_LOGO_GRAPHIC, SWSH_SAMPLE_PERIOD - SWSH_LOGO_GRAPHIC);
      out->palette_script_end = out->logo;
      out->logo_bytes = SWSH_SAMPLE_PERIOD - SWSH_LOGO_GRAPHIC;
      out->period = BE16(ftl_at(image, SWSH_SAMPLE_PERIOD, sizeof(uint16_t)));
      out->sample_length = BE32(ftl_at(image, SWSH_SAMPLE_LENGTH, sizeof(uint32_t)));
      if (out->sample_length > out->image.size - (SWSH_SAMPLE_DATA - SWSH_BASE))
          out->sample_length = 0;
      out->sample = ftl_at(image, SWSH_SAMPLE_DATA, out->sample_length);
      return true;
  ```

- [ ] **Step 6: Update the users.** `src/data/tables.c:201`: `if (!kaos.data)` becomes `if (!kaos_is_loaded())`.
  `tables_set_mouse_input_box` (`port/src/data/tables.c:771-778`) becomes:

  ```c
  void tables_set_mouse_input_box(int16_t table, int16_t entry, int16_t x1, int16_t x2, int16_t y1, int16_t y2)
  {
      uint32_t a = mouse_table(table) + MOUSE_INPUT_BYTES * entry;
      kaos_write_u16(a + MOUSE_AT_X1, (uint16_t)x1);
      kaos_write_u16(a + MOUSE_AT_X2, (uint16_t)x2);
      kaos_write_u16(a + MOUSE_AT_Y1, (uint16_t)y1);
      kaos_write_u16(a + MOUSE_AT_Y2, (uint16_t)y2);
  }
  ```

  `tables.c` and `spells.c` include `"data/kaos.h"` instead of `"data/ftl.h"` (each keeps `ftl.h` only if it
  names `FtlImage`; neither does). Every other `const` mismatch of `kaos_ptr` callers: `grep -n "kaos_ptr"
  src/data/tables.c` lists 11 sites (`port/src/data/tables.c:384-773`); all but the one in Step 6 only read, so
  their local pointer types become `const uint8_t *` where the compiler asks. `engine/dungeon.c:371`:
  `kaos.data` becomes `kaos_is_loaded()`, and `dungeon.c` includes `"data/kaos.h"` instead of `"data/ftl.h"`;
  add the line `kaos engine/dungeon.c open: epic 4` to the allowlist (Task 42 removes it).

- [ ] **Step 7: Verify.** `make`; `make format`; `uv run pytest -q tests/*/test_port_data.py
  tests/*/test_tables.py tests/*/test_csb_engine.py tests/*/test_dungeon.py` (reference checks of the loader
  and the tables against the image); `uv run pytest -q tests/standard`. The `global` line for `kaos` and the
  `prefix` line for `ftl.h` (two prefixes) of the allowlist go.

- [ ] **Step 8: Commit.** `git add -A src/data src/engine/dungeon.c tests/standard/allowlist.txt && git commit -m "refactor(data): kaos module owns the engine image, ftl_at computes image pointers"`

### Task 29: FTL format names and a constant CRC table

**Files:**
- Modify: `src/data/ftl.c`, `src/data/crc32.c`

**Interfaces:**
- Consumes: nothing. Produces: nothing public (static names only).

Facts: the FTL version bytes `2, 1, 0` and the entry layout `e + 2`, `e + 4`, `e + 8` are unnamed
(`port/src/data/ftl.c:116-117,131-135`); `enum { SECTION_INFO = 16, SECTION_DATA, SECTION_CODE, SECTION_COUNT
= 3 }` mixes tag values and a count (`port/src/data/ftl.c:22`); word alignment by `& ~1ull` is written twice
(`port/src/data/ftl.c:227-228`); one dense nibble expression (`port/src/data/ftl.c:68`); the comment names the
original symbol `BJELoad_R` (`port/src/data/ftl.c:192`); `crc32_update` fills its table lazily on
`crc_table[1]` (`port/src/data/crc32.c:12-18`).

- [ ] **Step 1: Name the header values.** In `ftl.c` replace the version test of `find_sections` with named
  constants in the existing header block:

  ```c
  #define FTL_FORMAT_VERSION     2 /* word at FTL_HEADER_VERSION */
  #define FTL_FORMAT_REVISION    1 /* byte after the version word */
  #define FTL_FORMAT_LEVEL       0 /* byte after the revision */
  ```

  and the entry layout as a block `/* Fields of a section entry. */` `FTL_ENTRY_TAG 0`, `FTL_ENTRY_VERSION 2`,
  `FTL_ENTRY_OFFSET 4`, `FTL_ENTRY_SIZE 8`, `FTL_ENTRY_FORMAT 1` (the entry version that the loader accepts).
  The section tags become `#define FTL_SECTION_INFO 16`, `FTL_SECTION_DATA 17`, `FTL_SECTION_CODE 18` in one
  decimal block, and the count `#define FTL_SECTION_KINDS 3` in its own line with a comment; replace every
  `SECTION_*` use (`grep -n "SECTION_" src/data/ftl.c`).
- [ ] **Step 2: One alignment helper.** Add above `ftl_load`:

  ```c
  /* The address rounded up to the next word: the loader aligns each part of the image to a word. */
  static uint64_t align_to_word(uint64_t address)
  {
      return (address + 1) & ~(uint64_t)1;
  }
  ```

  and write `uint64_t data_base = align_to_word(base + dsize);` and
  `uint64_t code_base = align_to_word(data_base + region);` (`port/src/data/ftl.c:227-228`; the same values:
  `base + ((dsize + 1) & ~1)` equals `(base + dsize + 1) & ~1` because `base` is even, `KAOS_BASE 0x100000` and
  `SWSH_BASE 0x100000`, `port/src/data/swsh.c:7`).
- [ ] **Step 3: Split the nibble read** of `port/src/data/ftl.c:68` into two statements:

  ```c
      uint32_t nibble = 0;
      if (b->pos < b->n)
          nibble = b->s[b->pos / 2] >> (b->pos % 2 ? 0 : NIBBLE_BITS) & NIBBLE_MASK;
      v = v << NIBBLE_BITS | nibble;
  ```

  (keep the variable names of the function; read lines 60-75 first).
- [ ] **Step 4: The comment of `ftl_load`** (`port/src/data/ftl.c:192`) becomes `/* Lays out an FTL module in
  memory at base: data, region, code. Returns 0 or -1. */`; the original name stays in
  `docs/reference/kaos-symbols.csv` (check it is there: `grep -n BJELoad docs/reference/kaos-symbols.csv`; add
  the row if it is missing, with its address from `reference/`).
- [ ] **Step 5: A constant CRC table.** Generate the table once and paste it into `crc32.c` as
  `static const uint32_t crc_table[CRC_TABLE_SIZE] = {...};` (eight values per line); delete the lazy fill
  (`port/src/data/crc32.c:12-18`) and `CRC32_POLYNOMIAL` if nothing else uses it (keep it in the comment above
  the table: `/* CRC-32 of PNG, reflected polynomial 0xedb88320, one entry per byte value. */`). Generator:

  ```bash
  uv run python -c '
  t=[]
  for i in range(256):
      c=i
      for _ in range(8): c = 0xedb88320 ^ (c >> 1) if c & 1 else c >> 1
      t.append(c)
  for k in range(0,256,8): print("    " + ", ".join(f"0x{v:08x}" for v in t[k:k+8]) + ",")'
  ```

  The table is a hex block with one digit width (standard rule 3.4).
- [ ] **Step 6: Verify.** `make`; `make format`; `uv run pytest -q tests/*/test_ftl.py tests/*/test_port_data.py
  tests/*/test_adf.py` (the FTL loader against the image and the CRC of the game files); `uv run pytest -q
  tests/standard`.
- [ ] **Step 7: Commit.** `git add src/data/ftl.c src/data/crc32.c docs/reference/kaos-symbols.csv && git commit -m "refactor(data): name the FTL format values, constant CRC table"`

#### Stream D2: tables, spells, ids

Shared files: `src/data/tables.c` (with stream D1, see above), `src/data/spells.c` (include line, with Task 28),
`src/ui/view.c` (two lines and one comment), `src/engine/{inventory,champion,group,magic,action,timeline_process}.c`,
`src/ui/ui_champion.c`, `src/ui/ui_menus.c`, `src/platform/state_json.c` (call sites),
`src/ui/{command,dialog,startend,ui,ui_champion}.c`, `src/platform/control.c` (`UI_INPUT_` rename).

### Task 30: tables_load is the only entry

**Files:**
- Modify: `src/data/tables.h`, `src/data/tables.c`, `src/ui/view.c` (lazy init), `src/ui/ui.h:68` (comment),
  every test module that calls `tables_init` (12 modules: `grep -ln "tables_init" tests -r`)

**Interfaces:**
- Consumes: nothing.
- Produces: `int tables_load(const char *path);` (0 or -1; Task 46 makes it `bool`) is the only setup call of
  `tables.h`; `tables_init` does not exist.

Facts: `tables_init` is public (`port/src/data/tables.h:153-154`) with a `ready` flag
(`port/src/data/tables.c:76,199-209`); `tables_load` is `kaos_load` plus `tables_init`
(`port/src/data/tables.c:289-293`); `ui/view.c` calls `tables_init()` lazily when
`tables_get_creature_info(0)` is NULL (`port/src/ui/view.c:2159-2160`); 13 test modules call `kaos_load` then
`tables_init` (`grep -rn "tables_init" tests`: `test_recruit`, `test_tables`, `test_champion`, `test_dungeon`,
`test_ui`, `test_game`, `test_move`, `test_sound`, `test_view`, `test_creatures`, `test_spells`,
`test_csb_engine`, `test_automap`, plus helpers in `tests/dmtest/` if package C moved one there). The cached
`DoorInfo` table has no caller in `port/src` (`grep -rn "tables_get_door_info" port/src` finds only
`tables.h:164` and `tables.c:242`); `tables_get_door_attributes` (`port/src/data/tables.h:199`) has 2 callers.

- [ ] **Step 1: Make the decoder static.** Rename `int tables_init(void)` to `static void decode_tables(void)`
  (place it directly above `tables_load`, standard rule 8.3); drop its image check (`tables_load` calls it only
  after `kaos_load` succeeded) and the `ready = true;` line; delete `static bool ready;` and the `ready &&` term
  of `GET` (`port/src/data/tables.c:211`). `tables_load` becomes:

  ```c
  int tables_load(const char *path)
  {
      int r = kaos_load(path);
      if (!r)
          decode_tables();
      return r;
  }
  ```

  In `tables.h` delete the `tables_init` prototype and comment; the comment above the cached accessors
  (`port/src/data/tables.h:156`) becomes `/* Each accessor returns NULL for an out-of-range index. Call after
  tables_load. */`; the comment of the call-time block (`port/src/data/tables.h:182`) becomes `/* Each accessor
  below reads the loaded image at call time. */`. Move the `tables_load` prototype to the top of the function
  list under a `/* Setup */` section comment.
- [ ] **Step 2: Drop the door info table:** `DoorInfo` (`port/src/data/tables.h:120-123`), `door_info[]`
  (`port/src/data/tables.c:84`), its fill (`port/src/data/tables.c:138-139`), `tables_get_door_info`
  (`port/src/data/tables.h:164`, `port/src/data/tables.c:242-245`), and `DOOR_TYPE_COUNT` if nothing else uses
  it (`grep -rn DOOR_TYPE_COUNT src`). In the test of the tables, delete the `door_info` row of `TABLES` and the
  `DoorInfo` structure (`tests/test_tables.py:35,50`).
- [ ] **Step 3: Remove the lazy init** of `port/src/ui/view.c:2159-2160` (the two lines `if
  (!tables_get_creature_info(0))` / `tables_init();`). Fix the stale comment `port/src/ui/view.c:30` that names
  `g_wall_set_layout_ids` (it is `tables_get_wall_set_layout`; check the name with `grep -n "wall_set" src/data/tables.h`).
  `port/src/ui/ui.h:68`: `/* Setup: call after tables_load, gfx_open, layout_init, and text_init. */`.
- [ ] **Step 4: The tests call `tables_load`.** Replace each pair `assert lib.kaos_load(str(X).encode()) == 0`
  / `assert lib.tables_init() == 0` by `assert lib.tables_load(str(X).encode()) == 0` (keep the lines between the
  two calls in place). In `tests/*/test_tables.py` the fixture lines `assert not
  lib.tables_get_creature_info(0)` and `assert lib.tables_init() == -1` (`tests/test_tables.py:76-77`) go (see
  Weakened assertions); add instead `assert lib.tables_load(b"/nonexistent/KAOS.FTL") == -1` before the real
  load. In `tests/*/test_csb_engine.py` the helper `load` (`tests/test_csb_engine.py:73-77`) calls
  `tables_load`. If the test library build of `tests/*/test_tables.py` lists source files
  (`tests/test_tables.py:66`; package B or C may have removed this list), add `kaos.c`.
- [ ] **Step 5: Verify.** `make`; `uv run pytest -q tests/*/test_tables.py tests/*/test_csb_engine.py
  tests/*/test_view.py tests/*/test_golden_view.py tests/*/test_creatures.py tests/*/test_spells.py`; then every
  other changed test module (`git diff --name-only tests`).
- [ ] **Step 6: Commit.** `git commit -am "refactor(data): tables_load is the only table setup, the door info table goes"`

### Task 31: Table accessors take int16_t; scalar accessors return values

**Files:**
- Modify: `src/data/tables.h`, `src/data/tables.c`, the 20 call sites (list below), `tests/*/test_tables.py`,
  `tests/*/test_champion.py`

**Interfaces:**
- Consumes: Task 30.
- Produces (binding for later packages):
  ```c
  const CreatureInfo *tables_get_creature_info(int16_t creature_type);
  const CreatureSound *tables_get_creature_sound(int16_t index);
  const ObjectInfo *tables_get_object_info(int16_t index);
  const WeaponInfo *tables_get_weapon_info(int16_t weapon_type);
  const ArmourInfo *tables_get_armour_info(int16_t armour_type);
  const Spell *tables_get_spell(int16_t index);
  const ActionSet *tables_get_action_set(int16_t index);
  const ActionInfo *tables_get_action_info(int16_t action);
  uint8_t tables_get_junk_weight(int16_t junk_type);
  int16_t tables_get_food_value(int16_t index);
  uint8_t tables_get_symbol_base_mana_cost(int16_t step, int16_t symbol);
  uint8_t tables_get_symbol_mana_cost_multiplier(int16_t power);
  int16_t tables_get_light_power_to_light_amount(int16_t power);
  int16_t tables_get_palette_light_threshold(int16_t index);
  uint16_t tables_get_slot_mask(int16_t slot);
  uint8_t tables_get_wound_defense_factor(int16_t slot);
  uint16_t tables_get_fixed_possession(int16_t index);
  ```

Facts: the cached accessors take `int` (`port/src/data/tables.h:157-174`), the call-time accessors `int16_t`
(`port/src/data/tables.h:183-364`); nine accessors return a pointer to a scalar that every caller dereferences at
once (`port/src/data/tables.h:162-174`). Call sites (review-data finding 22, 20 sites):
`engine/inventory.c:140,296,343,371,521,570,588`, `engine/champion.c:297,301,319,731`, `engine/group.c:460`,
`engine/magic.c:208,240,407,410`, `engine/action.c:624`, `engine/timeline_process.c:358`,
`ui/ui_champion.c:836`, `platform/state_json.c:395,398`; Step 1 re-counts them. Tests bind `POINTER` for these
(`tests/test_tables.py:74-75,141`, `tests/test_champion.py:447-449`).

- [ ] **Step 1: List the sites.** `grep -rnE "\*tables_get_(junk_weight|food_value|symbol_base_mana_cost|symbol_mana_cost_multiplier|light_power_to_light_amount|palette_light_threshold|slot_mask|wound_defense_factor|fixed_possession)" src`
  and `grep -rnE "tables_get_(junk_weight|food_value|symbol_base_mana_cost|symbol_mana_cost_multiplier|light_power_to_light_amount|palette_light_threshold|slot_mask|wound_defense_factor|fixed_possession)" src | grep -v "\*tables_get"`.
  Expected: every use in `src/` outside `tables.*` is a `*tables_get_...` dereference. A use that keeps the
  pointer stops the task (report it).
- [ ] **Step 2: Change the header** to the prototypes of **Interfaces** (the creature, object, weapon, armour,
  spell and action accessors keep their pointer results; only the parameter type changes).
- [ ] **Step 3: Change the definitions.** A second macro beside `GET` returns the value:

  ```c
  /* The table entry at i, or 0 outside the table: a defined value for a corrupt index. */
  #define GET_VALUE(arr, i) ((i) >= 0 && (i) < ARRAY_COUNT(arr) ? (arr)[i] : 0)
  ```

  and each scalar accessor returns `GET_VALUE(...)`; `tables_get_symbol_base_mana_cost` checks `step` against
  `SYMBOL_STEP_COUNT` and returns `GET_VALUE(symbol_base_mana_cost[step], symbol)`.
- [ ] **Step 4: Change the call sites:** remove the `*` before each call of Step 1 (sed over the listed files:
  `sed -i '' -E 's/\*(tables_get_(junk_weight|food_value|symbol_base_mana_cost|symbol_mana_cost_multiplier|light_power_to_light_amount|palette_light_threshold|slot_mask|wound_defense_factor|fixed_possession))\(/\1(/g' <files>`),
  then read each changed line: a `*tables_get_x(i)` inside a larger expression keeps its operator precedence
  because a call binds tighter than `*`.
- [ ] **Step 5: Change the tests.** In `tests/*/test_tables.py` the `TABLES` loop sets `restype` to the ctypes
  scalar for the nine scalar tables and keeps `POINTER(rec)` for the structs; `argtypes` become
  `[ctypes.c_int16]` (`[ctypes.c_int16, ctypes.c_int16]` for the mana cost). `get()` returns the value for a
  scalar table. `test_accessor_rejects_out_of_range` asserts `== 0` for a scalar table and `is None` for a struct
  table; the out-of-range indices become `(-1, count, 0x7FFF)` (an `int16_t` parameter). `tests/*/test_champion.py:447-449`
  reads `lib.tables_get_food_value(...)` as a value with `restype = c16`.
- [ ] **Step 6: Verify.** `make`; `uv run pytest -q tests/*/test_tables.py tests/*/test_champion.py
  tests/*/test_spells.py tests/*/test_golden_rules.py tests/*/test_golden_save.py`; the differential modules of
  the changed engine files: `uv run pytest -q tests/differential`.
- [ ] **Step 7: Commit.** `git commit -am "refactor(data): table accessors take int16_t, scalar tables return values"`

### Task 32: Named record offsets and designated initializers in tables.c

**Files:**
- Modify: `src/data/tables.c`

**Interfaces:** none (static code).

Facts: `read_creatures` names every offset (`CREATURE_AT_*`, `port/src/data/tables.c:56-74`) but builds a
17-member positional literal (`port/src/data/tables.c:102-112`); `read_objects` and `read_actions` use raw
offsets (`a + 2`, `a + 3`, `a + 4`, `port/src/data/tables.c:120-140`; `a + 4`, `a + 5`, `a + 6`,
`port/src/data/tables.c:165-182`); the action set flags byte has no reader (`port/src/data/tables.h:137`).

- [ ] **Step 1: Read the struct fields** of `CreatureInfo`, `ObjectInfo`, `WeaponInfo`, `ArmourInfo`,
  `ActionSet`, `ActionInfo` (`port/src/data/tables.h:74-151`) and the layout in `docs/rules/tables.md` (after
  package C; before, `docs/tables.md`).
- [ ] **Step 2: Name the offsets** as decimal blocks beside `CREATURE_AT_*`, one block per record:
  `OBJECT_AT_*` (the four fields of `ObjectInfo`), `WEAPON_AT_*` (five), `ARMOUR_AT_*` (three),
  `ACTION_SET_AT_ACTIONS 0`, `ACTION_SET_AT_LEVELS 4`, `ACTION_SET_AT_FLAGS 6`. The field names come from the
  struct fields; the values from the current expressions.
- [ ] **Step 3: Use designated initializers** in `read_creatures`, `read_objects`, `read_actions` and the action
  info loop, for example `.aspect_index = kaos_u8(a + CREATURE_AT_ASPECT_INDEX)`. The `(int8_t)kaos_u8` casts
  stay in this task (Task 42 replaces them by `kaos_s8`).
- [ ] **Step 4: The flags byte.** `ActionSet.flags` has no reader (`grep -rn "\.flags" src | grep -i action`);
  delete the field and its read. Assumption: no test reads it; check `grep -n "flags" tests/*/test_tables.py`
  and adapt the `ActionSet` ctypes structure if a test mirrors it (a field removal changes the struct layout the
  test reads).
- [ ] **Step 5: Verify.** `make`; `uv run pytest -q tests/*/test_tables.py tests/*/test_csb_engine.py` (each
  table against the bytes of the image).
- [ ] **Step 6: Commit.** `git commit -am "refactor(data): named record offsets and designated initializers in the table reader"`

### Task 33: Spells have one owner

**Files:**
- Modify: `src/data/spells.h`, `src/data/spells.c`, `src/data/tables.c`, `src/engine/magic.c`,
  `src/ui/ui_menus.c`, `src/platform/state_json.c`, `tests/*/test_csb_engine.py`, `tests/*/test_spells.py`

**Interfaces:**
- Consumes: Task 31 (`tables_get_symbol_base_mana_cost` and `tables_get_symbol_mana_cost_multiplier` return
  values).
- Produces: `spells.h` without `SPELL_BYTES`, `SPELL_ENTRY_*`, the `spell_attributes_*` inlines, `spell_count`.
  `int16_t spell_find(const uint8_t *symbols);`, `uint8_t spell_get_symbol(uint16_t step, int16_t symbol_index);`,
  `uint16_t spell_get_symbol_mana_cost(uint16_t step, int16_t symbol_index, uint8_t power_symbol);` are the one
  copy of these rules.

Facts: `spells.h` holds byte offsets and three unpackers used only by `tables.c` (`port/src/data/spells.h:7-10,28-39`;
`port/src/data/tables.c:148-155`); `get_spells_address` and `spell_count` repeat the table choice of
`read_magic` (`port/src/data/spells.c:16-23`, `port/src/data/tables.c:145-148`); `spell_find` reads the raw
image (`port/src/data/spells.c:42-47`); `spell_get_symbol` and `spell_get_symbol_mana_cost` have no caller in
`port/src` (tests only, `tests/test_spells.py:68-71`), while the formulas are copied in
`port/src/engine/magic.c:407-416`, `port/src/ui/ui_menus.c:198` and `port/src/platform/state_json.c:395-400`;
`spell_count` is read by `tests/test_csb_engine.py:148,157`.

Two copies differ in a corner: the game path (`magic.c:407-410`) truncates the product to 16 bits and reads the
power symbol as unsigned (`(uint8_t)c->symbols[0]`); `spells.c:28,62` reads it as signed with the comment "The
original reads the symbol as signed". The values are equal for every valid power symbol (`0x60`-`0x65`).

- [ ] **Step 1: Find the original rule.** Assumption: one of the two power-symbol readings is the original.
  Check the decompiled original in `reference/` (`grep -rn "symbol_mana_cost\|SymbolManaCost" reference/ | head`,
  or the address of `KAOS_SYMBOL_MANA_COST_MULTIPLIER` in `docs/reference/kaos-symbols.csv`) and keep that
  reading in `spell_get_symbol_mana_cost`, with the 16-bit product of `magic.c` (`(uint16_t)(cost * multiplier)
  >> SPELL_MANA_MULTIPLIER_SHIFT`). If the reference cannot settle it, keep the `magic.c` reading (the game path
  that the differential tests cover) and record the ruling.
- [ ] **Step 2: Move the record layout into `tables.c`.** Delete `SPELL_BYTES`, `SPELL_ENTRY_*` and the three
  `spell_attributes_*` inlines from `spells.h`; in `tables.c` add a decimal block `SPELL_AT_LEVEL 4`,
  `SPELL_AT_SKILL 5`, `SPELL_AT_ATTRIBUTES 6`, `SPELL_BYTES 8` beside `CREATURE_AT_*`, and three static
  functions `spell_kind`, `spell_type`, `spell_disabled_ticks` (no module prefix: they are static) with the
  bodies of `port/src/data/spells.h:28-39`. `SPELL_KIND_MASK`, `SPELL_TYPE_SHIFT`, `SPELL_TYPE_MASK`,
  `SPELL_DISABLED_TICKS_SHIFT` move with them (`grep -rn "SPELL_KIND_MASK\|SPELL_TYPE_SHIFT\|SPELL_TYPE_MASK\|SPELL_DISABLED_TICKS_SHIFT" src`
  first: a use outside `tables.c` keeps the constant in `spells.h`).
- [ ] **Step 3: `spell_find` loops the cached table:**

  ```c
  int16_t spell_find(const uint8_t *symbols)
  {
      if (symbols[1] == 0)
          return -1;
      uint32_t packed = 0;
      for (int shift = SPELL_FIRST_SYMBOL_SHIFT; shift >= 0; shift -= CHAR_BIT, symbols++) {
          packed |= (uint32_t)*symbols << shift;
          if (symbols[1] == 0)
              break;
      }
      const Spell *spell;
      for (int16_t i = 0; (spell = tables_get_spell(i)); i++)
          /* a spell without a power symbol matches any power */
          if ((spell->symbols & SPELL_POWER_SYMBOL_MASK) ? packed == spell->symbols
                                                          : (packed & ~SPELL_POWER_SYMBOL_MASK) == spell->symbols)
              return i;
      return -1;
  }
  ```

  Delete `get_spells_address`, `spell_count` and the includes `adventure.h`, `csbimage.h`, `kaos.h`,
  `kaos_addresses.h` if unused; `spells.c` includes `"data/tables.h"`.
- [ ] **Step 4: `spell_get_symbol_mana_cost`** reads `tables_get_symbol_base_mana_cost(step, symbol_index)` and
  `tables_get_symbol_mana_cost_multiplier(power)` with the reading of Step 1; `spell_get_symbol` stays.
- [ ] **Step 5: The three copies call the owner.** `magic.c:407-416`: `uint16_t cost =
  spell_get_symbol_mana_cost(step, symbol, (uint8_t)c->symbols[0]);` and `c->symbols[step] =
  (char)spell_get_symbol(step, symbol);`. `ui_menus.c:198`: `s[0] = (char)spell_get_symbol(symbol_step, k);`.
  `state_json.c:395-400`: `uint16_t cost = spell_get_symbol_mana_cost(champion->symbol_step, s,
  (uint8_t)champion->symbols[0]);`. Each file includes `"data/spells.h"`; drop includes that it no longer uses.
- [ ] **Step 6: Tests.** `tests/*/test_csb_engine.py:148,157`: `spell_count() == 29` becomes `assert
  lib.tables_get_spell(28) and not lib.tables_get_spell(29)`; `== 25` becomes `assert lib.tables_get_spell(24)
  and not lib.tables_get_spell(25)` (adapted, not weakened: the same count is checked).
- [ ] **Step 7: Verify.** `make`; `uv run pytest -q tests/*/test_spells.py tests/*/test_csb_engine.py
  tests/*/test_tables.py tests/*/test_golden_rules.py`; `uv run pytest -q tests/differential` (casting uses
  `magic_add_champion_symbol`); the A1 protocol golden (`uv run pytest -q tests/golden`), because
  `state_json.c` changed.
- [ ] **Step 8: Commit.** `git commit -am "refactor(data): one owner of the spell table and the rune rules"`

### Task 34: Mouse table names TABLES_MOUSE_

**Files:**
- Modify: `src/data/tables.h`, `src/data/tables.c`, `src/ui/command.c`, `src/ui/dialog.c`, `src/ui/startend.c`,
  `src/ui/ui.c`, `src/ui/ui_champion.c`, `src/platform/control.c`, tests that name `UI_INPUT_` (`grep -rln UI_INPUT_ tests`)

**Interfaces:**
- Produces: `TABLES_MOUSE_INTERFACE` ... `TABLES_MOUSE_ENDGAME`, `TABLES_MOUSE_DIALOG_FIRST`,
  `TABLES_MOUSE_COUNT` (same values as `UI_INPUT_*`).

Facts: `UI_INPUT_*` is a data enum named after its consumer, with the comment "Mouse input tables of COMMAND.C"
(`port/src/data/tables.h:323-341`); uses: `tables.h` 16, `tables.c` 3, `control.c` 13, `command.c` 13,
`startend.c` 2, `dialog.c` 1, `ui.c` 1, `ui_champion.c` 1 (`grep -rn "UI_INPUT_" port/src | cut -d: -f1 | uniq -c`).

- [ ] **Step 1: Rename.** `grep -rl "UI_INPUT_" src tests docs | xargs sed -i '' 's/\bUI_INPUT_/TABLES_MOUSE_/g'`
  (BSD sed has no `\b`; use `sed -i '' -E 's/([^A-Z_])UI_INPUT_/\1TABLES_MOUSE_/g'` and check with
  `grep -rn "UI_INPUT_" src tests docs` that nothing is left).
- [ ] **Step 2: The comment** `port/src/data/tables.h:323` becomes `/* Mouse input tables: which screen boxes
  give which command, in the order of their image addresses. */`.
- [ ] **Step 3: Verify.** `make`; `uv run pytest -q tests/*/test_ui.py tests/*/test_control.py tests/*/test_entrance.py`;
  `uv run pytest -q tests/golden`.
- [ ] **Step 4: Commit.** `git commit -am "refactor(data): mouse input tables use the tables prefix"`

### Task 35: ids.h grouped by kind, one name per value

**Files:**
- Modify: `src/data/ids.h`, `src/ui/startend.c:340` (`ZONE_THE_END`)

**Interfaces:**
- Produces: `ZONE_THE_END` does not exist; every other name keeps its value.

Facts: `ids.h` is grouped by the module that added a section ("Each module adds its own section",
`port/src/data/ids.h:4-5`): `GFX_` in 4 sections, `ZONE_` in 5, `TEXT_` in 4, `ICON_` in 3 (section comments at
`port/src/data/ids.h:7,49,98,126,138,161,170,179,197,244,249,268,277,307,342,439`); 413 `#define` lines, no
`enum`. Duplicate values inside one prefix (`grep "^#define" src/data/ids.h | awk ...`): most pairs are an id and
a count or stride with the same number (`ZONE_ACTION_AREA` 11 and `ZONE_FLOOR_ORNAMENT_VIEWS` 11); the real
second names are `ZONE_THE_END` = `ZONE_DIALOG_ON_SCREEN` = 5 (`port/src/data/ids.h:78,90`; the PRD keeps
`ZONE_DIALOG_ON_SCREEN`) and `ZONE_FIELD_FIRST` = `ZONE_WALL_D3L2` = 702 (`port/src/data/ids.h:181,346`). The
comments name dead globals `g_base_skill_string_indices` ... (`port/src/data/ids.h:139-143`) and
`g_wall_set_layout_ids` (`port/src/data/ids.h:439`).

Rule of this task: ids are sorted by value inside their kind; counts, strides and range sizes (`*_COUNT`,
`*_SIZE`, `*_VIEWS`, `*_DEPTHS`, `*_DELTA`, `*_SIDES`, `*_CELLS`, `*_POSITIONS`, `*_SETS`, `*_LAST`) form a
separate decimal block per kind; the zone flags `ZONE_SHIFTED 0x8000` and `ZONE_SIZED 0x4000` form a hex block;
a name defined as another name (`ICON_LAST_WITH_STATE ICON_SCROLL_OPEN`) stays in the block of its kind as a
range marker. A range base used as `BASE + index` (`ZONE_FIELD_FIRST + field_index`, `port/src/ui/view.c:1054`)
is not a second name of the zone with the same number: it stays, in the block of range bases, with its comment.

- [ ] **Step 1: Generate the new order** with a script in the scratch folder (not in the repository):

  ```python
  # regroup_ids.py: prints src/data/ids.h grouped by kind; run: uv run python regroup_ids.py src/data/ids.h
  import re, sys
  KINDS = ["GFX", "ZONE", "LAYOUT", "ICON", "TEXT"]
  COUNT = re.compile(r"_(COUNT|SIZE|VIEWS|DEPTHS|DELTA|SIDES|CELLS|POSITIONS|SETS|LAST|STEP)$")
  lines = open(sys.argv[1]).read().splitlines()
  defs = []
  for line in lines:
      m = re.match(r"#define (\w+)\s+(\S+)\s*(/\*.*\*/)?\s*$", line)
      if m and m.group(1) != "DM_DATA_IDS_H":
          defs.append((m.group(1), m.group(2), m.group(3) or ""))
  def kind(name): return name.split("_")[0]
  def value(v):
      try: return int(v, 0)
      except ValueError: return None
  for k in KINDS:
      group = [d for d in defs if kind(d[0]) == k]
      flags = [d for d in group if d[1].startswith("0x")]
      counts = [d for d in group if COUNT.search(d[0]) or value(d[1]) is None and d not in flags]
      ids = [d for d in group if d not in flags and d not in counts]
      for title, block in ((f"{k} ids", sorted(ids, key=lambda d: (value(d[1]), d[0]))),
                           (f"{k} counts, strides and ranges", counts), (f"{k} flags", flags)):
          if block:
              print(f"\n/* {title} */")
              for n, v, c in block:
                  print(f"#define {n} {v} {c}".rstrip())
  rest = [d for d in defs if kind(d[0]) not in KINDS]
  print("\n/* not grouped: */", *[d[0] for d in rest], sep="\n")
  ```

  The printed `not grouped` list must be empty; a name outside the five kinds stops the task (report it).
- [ ] **Step 2: Write the new `ids.h`.** Keep the file head (SPDX, guard, prefix line) and a new purpose
  paragraph: `/* Graphic numbers (Graphics.DAT items), layout zone numbers (graphic 696), layout bitmap ids,
  object icons, and string numbers (graphic 700), one block per kind, sorted by value. */`. Paste the generated
  blocks; give each block comment the set name in game terms (for example `/* Zones of the screen and the
  viewport, graphic 696. */`). Keep every trailing comment. A trailing comment that named the old section's
  topic and is now needed for a reader goes into the trailing comment of its line.
- [ ] **Step 3: One name per value.** Delete `ZONE_THE_END`; `port/src/ui/startend.c:340` uses
  `ZONE_DIALOG_ON_SCREEN`. Check the id blocks for any other value with two names (`awk` over the new file);
  another pair stops the task for a ruling.
- [ ] **Step 4: Comments.** Replace the dead global names of `port/src/data/ids.h:139-143,439` by the accessors
  (`tables_get_base_skill_text`, `tables_get_skill_level_text`, `tables_get_direction_text`,
  `tables_get_statistic_text`, and the wall set layout accessor of `tables.h`).
- [ ] **Step 5: Verify.** `make format`; `make`; `uv run pytest -q tests/golden tests/*/test_ui.py
  tests/*/test_view.py` (the golden view and screen snapshots prove the values did not change);
  `uv run pytest -q tests/standard` (the notation and block checks of `ids.h`).
- [ ] **Step 6: Commit.** `git commit -am "refactor(data): ids.h grouped by kind and sorted by value"`

#### Stream D3: CSB image, adventure, file, graphics

Shared files: `src/data/os.h` and `src/data/os.c` (Task 37 deletes `os_compare_names` at the end of both;
Task 27 inserts in the middle), `src/ui/anim.c`, `src/ui/gameloop.c`, `src/platform/main.c`,
`src/engine/savegame.c`, `src/ui/select.c`, `src/ui/draw.h`, `src/ui/text.c`, `src/engine/message.c`,
`src/ui/{advanced,automap,dialog,startend,ui,ui_champion}.c` (call sites only).

### Task 36: The CSB image is published only on success

**Files:**
- Modify: `src/data/csbimage.c`, `src/data/csbimage.h`, `tests/*/test_csb_engine.py`
- Test: `tests/*/test_csb_engine.py` (one regression test)

**Interfaces:**
- Consumes: nothing.
- Produces: `extern const CsbCopy g_csb_copies[]; /* exported for the tests */` and
  `extern const int g_csb_copy_count; /* exported for the tests */`; `csb_image_build` leaves `*out` unchanged
  when it fails.

Facts: `csb_image_build` stores `*out = built;` and then calls `csb_text_build` (`port/src/data/csbimage.c:184-185`);
when the text build fails, `kaos_load` returns -1 while the image is set (`port/src/data/ftl.c:264-269`), and
`dungeon_load` sees a loaded image (`port/src/engine/dungeon.c:371`). The first failure path frees `mem`
(`port/src/data/csbimage.c:164-167`); `FtlImage built = {mem, size, KAOS_BASE, CSB_SPELLS, 0};` sets fields
positionally (`port/src/data/csbimage.c:168`). `csb_copies` and `csb_copy_count` have no `g_` and are read only by
`tests/test_csb_engine.py:81-82`. `csb_text_build` fails when a string pointer of the APPB data points outside the
image (`get_appb_pointed_string`, `port/src/data/csbtext.c` lines 132-138 of the file; the disk message pointers
start at `CSB_APPB_DISK_MESSAGES 0x104d08`, `port/src/data/csbtext.c:13`).

- [ ] **Step 1: Write the regression test** in `tests/*/test_csb_engine.py` (needs the CSB game files; use the
  module's skip marker):

  ```python
  def test_failed_text_build_publishes_no_image(lib):
      """A CSB image whose text build fails is freed and not published (bug: the image stayed loaded)."""
      raw = (GAME_CSB / "APPB.FTL").read_bytes()
      appb = FtlImage()
      assert lib.ftl_load(raw, len(raw), BASE, ctypes.byref(appb)) == 0
      at = CSB_APPB_DISK_MESSAGES - appb.base
      ctypes.memmove(ctypes.addressof(appb.data.contents) + at, b"\xff\xff\xff\xff", 4)  # a pointer outside the image
      out = FtlImage()
      assert lib.csb_image_build(ctypes.byref(appb), ctypes.byref(out)) == -1
      assert not out.data
      lib.ftl_free(ctypes.byref(appb))
  ```

  `FtlImage` is the ctypes structure of `tests/test_port_data.py:22` (after package C in `dmtest`; import it
  from there); `CSB_APPB_DISK_MESSAGES` comes from `dmtest.constants` if package C parses `.c` files, else
  the test defines it with a comment that names `src/data/csbtext.c` (the constant is file-local). Set
  `argtypes` of `ftl_load`, `csb_image_build` and `ftl_free` as `test_port_data.py:34` does.
- [ ] **Step 2: Run it to see it fail.** `uv run pytest -q tests/*/test_csb_engine.py -k failed_text_build`.
  Expected: FAIL on `assert not out.data`.
- [ ] **Step 3: Fix `csb_image_build`** (`port/src/data/csbimage.c:159-186`): build into the local `built`,
  call `csb_text_build(appb, &built)`, free `mem` and return -1 when it fails, assign `*out = built` only at the
  end. Use a designated initializer with only the fields that mean something:
  `FtlImage built = {.data = mem, .size = size, .base = KAOS_BASE};` (check with `grep -rn "data_base\|code_base"
  src` that no reader needs the other two fields of a built image; `csbimage.c:164` reads `appb->data_base`
  only of the input).
- [ ] **Step 4: Rename the test seam.** `csb_copies` → `g_csb_copies`, `csb_copy_count` → `g_csb_copy_count` in
  `csbimage.h` and `csbimage.c`, each declaration with `/* exported for the tests */`;
  `tests/*/test_csb_engine.py` (`in_dll(lib, "csb_copy_count")`, `"csb_copies"`) follows.
- [ ] **Step 5: Run the tests.** `make lib`; `uv run pytest -q tests/*/test_csb_engine.py tests/*/test_csb_text.py`.
  Expected: PASS.
- [ ] **Step 6: Commit.** `git commit -am "fix(data): a failed CSB text build no longer leaves the image loaded"`

### Task 37: file.h owns paths, name compares and file writes

**Files:**
- Modify: `src/data/file.h`, `src/data/file.c`, `src/data/os.h`, `src/data/os.c`, `src/data/adventure.h`,
  `src/data/adf.c`, `src/ui/anim.c`, users of `PATH_SIZE` that include only `adventure.h` for it

**Interfaces:**
- Produces (`src/data/file.h`, used by Task 38 and by package E for `savegame.c`, R5.5):
  ```c
  #define PATH_SIZE 1024 /* longest file path */
  bool file_write(const char *path, const uint8_t *data, size_t size);
  bool file_replace(const char *from, const char *to);
  int file_compare_names(const char *a, const char *b);
  ```

Facts: `PATH_SIZE` is in `adventure.h` (`port/src/data/adventure.h:15`) and is used in `data/adf.c`,
`data/adventure.c`, `engine/savegame.c`, `ui/anim.c`, `ui/music.c`, `ui/gameloop.c`, `platform/control.c`,
`platform/main.c` (`grep -rn PATH_SIZE port/src`); `os_compare_names` is a string compare in the folder module
(`port/src/data/os.c:18-27`, used by `data/adf.c:84` and `ui/anim.c:104`); `savegame.c` writes with
`remove`, `rename`, `fopen`, `fwrite` (`port/src/engine/savegame.c:322-334,583`); R4.6 asks for `file_write` and
`file_replace` in `data/file.h` for `savegame.c` (package E changes `savegame.c`).

- [ ] **Step 1: Add to `file.h`** (with a purpose line: `/* Whole-file reads and writes and file names: the C
  standard library only, no folder services. */`):

  ```c
  /* Longest file path. */
  #define PATH_SIZE 1024

  /* Queries */
  /* The malloc'd contents ... (unchanged) */
  uint8_t *file_read(const char *path, size_t *len);
  bool file_exists(const char *path);
  /* Compares two file names as ASCII without case, as strcasecmp. */
  int file_compare_names(const char *a, const char *b);
  /* Commands */
  /* Writes size bytes to path, replacing the file. True when every byte is written and the file is closed. */
  bool file_write(const char *path, const uint8_t *data, size_t size);
  /* Renames from to to, replacing to. True on success. */
  bool file_replace(const char *from, const char *to);
  ```

  Keep the existing comments of `file_read` and `file_exists`.
- [ ] **Step 2: Add to `file.c`:**

  ```c
  int file_compare_names(const char *a, const char *b)
  {
      const unsigned char *p = (const unsigned char *)a;
      const unsigned char *q = (const unsigned char *)b;
      while (*p && tolower(*p) == tolower(*q)) {
          p++;
          q++;
      }
      return tolower(*p) - tolower(*q);
  }

  bool file_write(const char *path, const uint8_t *data, size_t size)
  {
      FILE *f = fopen(path, "wb");
      bool ok = f && fwrite(data, 1, size, f) == size;
      if (f && fclose(f))
          ok = false;
      return ok;
  }

  bool file_replace(const char *from, const char *to)
  {
      remove(to);
      return rename(from, to) == 0;
  }
  ```

  (`file_write` is the body of `write_save_file`, `port/src/engine/savegame.c:329-333`; `file_replace` is the
  `remove` + `rename` pair of `port/src/engine/savegame.c:326-327`.)
- [ ] **Step 3: Remove the old places.** Delete `os_compare_names` from the end of `os.h` and `os.c`
  (`port/src/data/os.h:16-17`, `port/src/data/os.c:18-27`; `<ctype.h>` leaves `os.c` if nothing else uses it) and
  `PATH_SIZE` from `adventure.h`. `adf.c:84` and `anim.c:104` call `file_compare_names` and include
  `"data/file.h"`. Every file that uses `PATH_SIZE` includes `"data/file.h"` (add the include where it is
  missing; drop `adventure.h` where `PATH_SIZE` was its only use: check each file with `grep -n "adventure_\|ADVENTURE_\|g_adventure\|CSB_\|DM_DUNGEON"`).
- [ ] **Step 4: Verify.** `make`; `uv run pytest -q tests/*/test_adf.py tests/*/test_save.py
  tests/*/test_csb_adventure.py`; `uv run pytest -q tests/standard`. `file_write` and `file_replace` have no
  caller yet; the `dead-api` check gets `dead-api data/file.h:file_write open: epic 5` and the same for
  `file_replace` (package E calls them from `savegame.c` and removes the lines).
- [ ] **Step 5: Commit.** `git commit -am "refactor(data): file.h owns the path size, name compares and file writes"`

### Task 38: One Adventure record per adventure

**Files:**
- Modify: `src/data/adventure.h`, `src/data/adventure.c`, `src/platform/main.c`, `src/ui/gameloop.c`,
  `src/engine/savegame.c`, `src/ui/select.c` (and every other caller of Step 1), `tests/*/test_select.py`,
  `tests/*/test_adf.py`

**Interfaces:**
- Consumes: Task 37 (`PATH_SIZE` in `file.h`).
- Produces (binding):
  ```c
  typedef struct {
      const char *name;   /* the --adventure value */
      const char *dir;    /* the default disk A folder */
      const char *title;  /* the name in messages */
      const char *engine; /* the engine module file */
      const char *save;   /* the save file name */
      const char *backup; /* the backup file name */
      GameFile files[ADVENTURE_GAME_FILES]; /* the game files and their CRC32; a NULL name ends the list */
  } Adventure;
  const Adventure *adventure_get(int16_t adventure);
  int16_t adventure_find(const char *name);       /* ADVENTURE_DM, ADVENTURE_CSB, or -1 */
  bool adventure_is_csb_present(const char *dir);
  const char *adventure_find_bad_file(int16_t adventure, const char *dir);
  bool adventure_load_files(const char *dir, const char **failed);
  typedef enum { ADVENTURE_DISK_INSTALLED, ADVENTURE_DISK_SKIPPED, ADVENTURE_DISK_FAILED } AdventureDisk;
  AdventureDisk adventure_install_disk(const char *adf_path, const char **dir);
  ```

Facts: `Adventure` holds six names (`port/src/data/adventure.h:17-20`); the game files are a parallel array
`game_files[][GAME_FILES]` (`port/src/data/adventure.c:32-38`); `adventure_csb_present` loops all four entries
without the NULL stop that `adventure_bad_file` uses (`port/src/data/adventure.c:78-101`); the API has
`adventure_get` beside four field accessors (`port/src/data/adventure.h:25-36`); `adventure_of` returns `int`
(`port/src/data/adventure.h:28`); `adventure_install_disk` returns 1/0/-1 (`port/src/data/adventure.h:41-43`);
the sequence "engine module, then Graphics.DAT" is written in `port/src/platform/main.c:40-50` and
`port/src/ui/gameloop.c:50-60`. Callers (`grep -rnw` in `port/src` outside `data/`): `adventure_get` 1,
`adventure_of` 1, `adventure_default_dir` 3, `adventure_engine_name` 2, `adventure_save_name` 3,
`adventure_backup_name` 3, `adventure_bad_file` 1, `adventure_csb_present` 1, `adventure_install_disk` 1; tests:
`tests/test_select.py:44-105`, `tests/test_adf.py:35-129`.

Names: the predicate gets `is_` (standard rule 4.4), the two noun functions get verbs (R4.8): `adventure_of` →
`adventure_find`, `adventure_csb_present` → `adventure_is_csb_present`, `adventure_bad_file` →
`adventure_find_bad_file`. The three results of `adventure_install_disk` become the enum result type of standard
rule 6.1 (1 → `ADVENTURE_DISK_INSTALLED`, 0 → `ADVENTURE_DISK_SKIPPED` (no game disk, or the folder has the
files), -1 → `ADVENTURE_DISK_FAILED`).

- [ ] **Step 1: List the callers:** `grep -rnwE "adventure_(get|of|default_dir|engine_name|save_name|backup_name|bad_file|csb_present|install_disk)" src tests tools docs`.
- [ ] **Step 2: Change `adventure.h`** to the **Interfaces** block. `GameFile` and `ADVENTURE_GAME_FILES 4` move
  from `adventure.c` (`port/src/data/adventure.c:24-30`) into the header above `Adventure`. Delete the four field
  accessors and `adventure_default_dir`.
- [ ] **Step 3: Change `adventure.c`.** One table:

  ```c
  static const Adventure adventures[] = {
      {"dm", "game", "Dungeon Master v3.60", "KAOS.FTL", "DMGAME.DAT", "DMGAME.BAK",
       {{"KAOS.FTL", CRC_DM_KAOS}, {"Graphics.DAT", CRC_DM_GRAPHICS}, {"Dungeon.DAT", CRC_DM_DUNGEON}}},
      {"csb", "game-csb", "Chaos Strikes Back", "APPB.FTL", "CSBGAME.DAT", "CSBGAME.BAK",
       {{"APPB.FTL", CRC_CSB_APPB}, {"Graphics.DAT", CRC_CSB_GRAPHICS}, {"Dungeon.DAT", CRC_CSB_DUNGEON},
        {CSB_MINI_FILE, CRC_CSB_MINI}}},
  };
  ```

  Both file loops (`adventure_is_csb_present`, `adventure_find_bad_file`) run `for (int i = 0; i <
  ADVENTURE_GAME_FILES && files[i].name; i++)`. `adventure_find` returns `int16_t`.
- [ ] **Step 4: Add `adventure_load_files`:**

  ```c
  /* Loads the engine module and Graphics.DAT of g_adventure from dir. False with the failed file name in *failed. */
  bool adventure_load_files(const char *dir, const char **failed)
  {
      char path[PATH_SIZE];
      const Adventure *a = adventure_get(g_adventure);
      snprintf(path, sizeof path, "%s/%s", dir, a->engine);
      if (tables_load(path)) {
          *failed = a->engine;
          return false;
      }
      snprintf(path, sizeof path, "%s/%s", dir, ADVENTURE_GRAPHICS_FILE);
      if (gfx_open(path) < 0) {
          *failed = ADVENTURE_GRAPHICS_FILE;
          return false;
      }
      return true;
  }
  ```

  with `#define ADVENTURE_GRAPHICS_FILE "Graphics.DAT"` in `adventure.h` beside `CSB_MINI_FILE`. `adventure.c`
  includes `"data/graphics.h"` and `"data/tables.h"` (both in `data/`).
- [ ] **Step 5: Change the callers.** `platform/main.c:40-50` (`load_kaos_and_graphics`) calls
  `adventure_load_files(dir, &failed)` and logs `"cannot load %s/%s", dir, failed` as today
  (`SDL_Log("cannot load %s", path)`, `port/src/platform/main.c:45,49`: the logged text stays the full path).
  `ui/gameloop.c:50-60` calls `adventure_load_files(dir, &failed)` and then `layout_init`, `text_init`,
  `ui_init` as today. A field accessor call becomes `adventure_get(g_adventure)->engine`, `->save`, `->backup`,
  `adventure_get(adventure)->dir` (each site of Step 1). `adventure_install_disk` callers compare with the enum
  names.
- [ ] **Step 6: Tests.** `tests/*/test_select.py:44-105` and `tests/*/test_adf.py:35-129` call the new names;
  the field tests read the struct through a ctypes `Adventure` structure (seven `c_char_p` fields and the files
  array) returned by `adventure_get`, or keep testing the names through `adventure_get(DM).contents.save`. The
  install test compares with `C.ADVENTURE_DISK_INSTALLED` and `C.ADVENTURE_DISK_SKIPPED` (`dmtest.constants`
  parses the enum result type).
- [ ] **Step 7: Verify.** `make`; `uv run pytest -q tests/*/test_select.py tests/*/test_adf.py
  tests/*/test_csb_adventure.py tests/*/test_save.py`.
- [ ] **Step 8: Commit.** `git commit -am "refactor(data): one Adventure record per adventure, adventure_load_files"`

### Task 39: graphics.h owns Box and the font metrics; gfx_close

**Files:**
- Modify: `src/data/graphics.h`, `src/data/graphics.c`, `src/ui/draw.h`, users of `TEXT_CHAR_WIDTH`,
  `TEXT_CHAR_GAP`, `TEXT_LINE_HEIGHT`, `TEXT_BASELINE_GAP` (`engine/message.c`, `engine/message.h`,
  `ui/advanced.c`, `ui/automap.c`, `ui/dialog.c`, `ui/startend.c`, `ui/text.c`, `ui/ui_champion.c`, `ui/ui.c`)

**Interfaces:**
- Produces (binding for packages D tail and G):
  ```c
  typedef struct { int16_t x, y, w, h; } Box;          /* in data/graphics.h */
  bool box_contains(const Box *box, int16_t x, int16_t y);
  void gfx_close(void);
  #define FONT_CHAR_WIDTH 6
  #define FONT_CHAR_GAP 1
  #define FONT_LINE_HEIGHT 6
  #define FONT_BASELINE_GAP 1
  ```

Facts: `Box` is defined in `port/src/ui/draw.h:7-9`, and `draw.h` includes `graphics.h`
(`port/src/ui/draw.h:5`); the font metrics `TEXT_*` sit in `port/src/data/graphics.h:11-15` and share the
`TEXT_` prefix with string ids; uses: `engine/message.c` 6, `engine/message.h` 2, `ui/advanced.c` 5,
`ui/automap.c` 1, `ui/dialog.c` 2, `ui/startend.c` 2, `ui/text.c` 4, `ui/ui_champion.c` 4, `ui/ui.c` 2. There is
no `gfx_close`; `gfx_open` frees the previous state but keeps `graphics_data` when a later step fails
(`port/src/data/graphics.c:27-54`); the header literals `4`, `8u`, `2 * i` are unnamed
(`port/src/data/graphics.c:42-48`); five file-scope variables with the short name `count`
(`port/src/data/graphics.c:20-24`). R4.5 keeps the font metrics in `data/graphics.h` (engine uses them).

- [ ] **Step 1: Move `Box`.** Cut the struct from `ui/draw.h` and paste it into `graphics.h` after `Bitmap`,
  with `box_contains` (a point-in-box test; the five ui loops that it replaces change in package G, R8.7):

  ```c
  /* A rectangle of the screen or of a bitmap, in pixels. */
  typedef struct {
      int16_t x, y, w, h;
  } Box;
  /* True when the pixel x, y is inside the box. */
  bool box_contains(const Box *box, int16_t x, int16_t y);
  ```

  ```c
  bool box_contains(const Box *box, int16_t x, int16_t y)
  {
      return x >= box->x && x < box->x + box->w && y >= box->y && y < box->y + box->h;
  }
  ```

  The prefix line of `graphics.h` becomes `/* Prefixes: gfx_ box_ */`. Check `grep -rn "\bbox_" src` for another
  `box_` name first; a clash stops the task. `box_contains` has no caller until package G; add
  `dead-api data/graphics.h:box_contains open: epic 8`.
- [ ] **Step 2: Rename the font metrics** to `FONT_*`: `grep -rl "TEXT_CHAR_WIDTH\|TEXT_CHAR_GAP\|TEXT_LINE_HEIGHT\|TEXT_BASELINE_GAP" src tests docs | xargs sed -i '' -E 's/TEXT_(CHAR_WIDTH|CHAR_GAP|LINE_HEIGHT|BASELINE_GAP)/FONT_\1/g'`.
  The block comment stays `/* Text metrics of the built-in font. */`.
- [ ] **Step 3: Name the Graphics.DAT header** in `graphics.c`: a decimal block `GFX_HEADER_BYTES 4` (format word
  and item count), `GFX_ITEM_ENTRY_BYTES 8` (the size words of one item), `GFX_HEADER_ITEM_COUNT 2` (offset of the
  count word); `port/src/data/graphics.c:38-48` uses them (read the lines first and keep the arithmetic).
- [ ] **Step 4: One static struct and gfx_close.** The file-scope variables of `graphics.c:20-24` (and the string
  list variables near `graphics.c:217`) become `static struct { uint8_t *data; size_t size; uint16_t
  item_count; uint32_t *offsets; Bitmap *decoded; ... } graphics;` (standard rule 7.2: four or more file-scope
  variables). Add:

  ```c
  /* Frees Graphics.DAT and the decoded bitmaps. */
  void gfx_close(void);
  ```

  with the free code of the top of `gfx_open` (`port/src/data/graphics.c:29-36`); `gfx_open` calls
  `gfx_close()` first and on every failure path, so a failed open leaves nothing loaded.
  Assumption: no caller relies on the old data after a failed `gfx_open`; check `grep -rn "gfx_open" src` (4
  sites): each stops on failure.
- [ ] **Step 5: Verify.** `make`; `uv run pytest -q tests/*/test_port_data.py tests/*/test_text.py
  tests/*/test_ui.py tests/golden`.
- [ ] **Step 6: Commit.** `git commit -am "refactor(data): Box and the font metrics in graphics.h, gfx_close"`

#### Stream D4: Audio out of the engine

Shared files: `src/ui/ui.c` (one line in `ui_init`), `src/ui/anim.c`, `src/ui/logo.c`, `src/ui/gameloop.c`
(sound call sites), `src/platform/platform.c` (two names), `src/ui/startend.c:397` (one call).

### Task 40: The engine plays sounds through g_sound_play_hook; ui/audio.c mixes

**Files:**
- Create: `src/ui/audio.h`, `src/ui/audio.c`
- Modify: `src/engine/sound.h`, `src/engine/sound.c`, `src/ui/ui.c`, `src/ui/anim.c`, `src/ui/logo.c`,
  `src/ui/gameloop.c`, `src/ui/music.c`, `src/platform/platform.c`, `tests/*/test_sound.py`,
  `tests/*/test_music.py`, `tests/*/test_csb_ending.py`, `tests/*/test_entrance.py`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```c
  /* engine/sound.h */
  typedef void (*SoundPlayHook)(const int8_t *sample, uint16_t length, uint16_t period, const uint8_t *volumes);
  extern SoundPlayHook g_sound_play_hook; /* ui/audio.c sets it; NULL: no sound plays */
  const int8_t *sound_get_sample(int16_t sound, uint16_t *length, uint16_t *period);
  int16_t sound_compare_loudness(const uint8_t *a, const uint8_t *b);
  bool sound_get_volume(int16_t map_x, int16_t map_y, uint8_t *volumes);
  void sound_request_play(int16_t sound, int16_t map_x, int16_t map_y, uint16_t mode); /* package F changes the mode */
  void sound_play_pending(void);
  extern int16_t g_pending_sounds[SOUND_PENDING_SLOTS];           /* exported for the tests */
  extern uint8_t g_pending_sound_volumes[SOUND_PENDING_SLOTS][2]; /* exported for the tests */
  /* ui/audio.h */
  #define AUDIO_OUTPUT_RATE    44100
  #define AUDIO_PAL_CLOCK      3546895
  #define AUDIO_MUSIC_SEGMENTS 128
  void audio_init(void);
  void audio_play_sample(int16_t sound, const uint8_t *volumes);
  void audio_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes);
  void audio_stop_all(void);
  void audio_music_play(const int8_t *const *data, const uint16_t *lengths, int count, uint16_t period, uint8_t volume);
  void audio_music_set_volume(uint8_t volume);
  void audio_music_stop(void);
  void audio_mix(int16_t *out, int frames);
  ```

Facts: `engine/sound.c` holds the rules (`sound_get_sample`, `sound_compare_loudness`, `sound_get_volume`,
`queue_pending`, `sound_request_play`, `sound_play_pending`, `port/src/engine/sound.c:43-205`) and the Paula mixer
(`Voice`, `voices`, `pair_channels`, `music`, `sound_play_data`, `sound_stop_all`, `sound_music_*`,
`continue_music`, `sound_mix`, `sound_demo_play`, `port/src/engine/sound.c:16-41,113-145,207-286`).
`platform/platform.c:248` calls `sound_mix`, `port/src/platform/platform.c:231,243` use `SOUND_OUTPUT_RATE`;
`ui/music.c:85` uses `SOUND_OUTPUT_RATE`; ui callers: `anim.c:84,159-173,225,334`, `logo.c:17-45`,
`gameloop.c:142,167` (`sound_play_sample` at full volume). `sound_demo_play` has no caller in `port/src`
(`tests/test_sound.py:98` only). R8.9: `audio_play_data` copies the sample, so `anim_close` and `logo_play` need
no stop of the samples before they free (`port/src/ui/anim.c:84`, `port/src/ui/logo.c:45`); the intro music
segments still point into the animation file, so `anim_close` stops the music.

- [ ] **Step 1: Create `src/ui/audio.h`:**

  ```c
  /* SPDX-License-Identifier: GPL-3.0-or-later */
  #ifndef DM_UI_AUDIO_H
  #define DM_UI_AUDIO_H
  /* Prefixes: audio_ */

  /* The audio output: four Paula voices in two stereo pairs, the CSB intro music, and the mixer that
     platform/ calls on the game thread. It plays what the engine requests through g_sound_play_hook; it holds
     no game rule. */

  #include <stdint.h>

  /* Output format of the mixer. */
  #define AUDIO_OUTPUT_RATE 44100   /* frames per second, stereo int16 */
  #define AUDIO_PAL_CLOCK   3546895 /* Paula clock, Hz */
  /* Segments of the intro music. */
  #define AUDIO_MUSIC_SEGMENTS 128

  /* Setup */
  /* Sets g_sound_play_hook to audio_play_data; ui_init calls it. */
  void audio_init(void);

  /* Commands */
  /* Plays a sound of the sound table at the left and right volumes. */
  void audio_play_sample(int16_t sound, const uint8_t *volumes);
  /* Copies sample data and plays it on a free stereo pair, or on the quieter pair when both play. */
  void audio_play_data(const int8_t *data, uint16_t length, uint16_t period, const uint8_t *volumes);
  /* Stops every voice and the intro music. */
  void audio_stop_all(void);
  /* Intro music: the segments play one after another, the same data on both sides of pair B. Samples go to pair
     A while it plays. The data stays owned by the caller until the music ends or stops. */
  void audio_music_play(const int8_t *const *data, const uint16_t *lengths, int count, uint16_t period, uint8_t volume);
  void audio_music_set_volume(uint8_t volume);
  void audio_music_stop(void);
  /* Renders interleaved stereo int16 at AUDIO_OUTPUT_RATE. Paula has no interpolation: each sample holds for one
     period. Runs on the game thread. */
  void audio_mix(int16_t *out, int frames);

  #endif
  ```

- [ ] **Step 2: Create `src/ui/audio.c`** from the mixer part of `engine/sound.c`: move (cut, not copy)
  `SIDE_*`, `PAIR_*`, `AMIGA_CHANNELS`, `Voice`, `voices`, `pair_channels`, `pair_volumes`, `music`,
  `get_voice_position`, `is_voice_busy`, `sound_play_data` (as `audio_play_data`), `sound_stop_all`
  (`audio_stop_all`), `sound_music_play`, `sound_music_set_volume`, `sound_music_stop` (`audio_music_*`),
  `continue_music`, `sound_mix` (`audio_mix`), with `SOUND_OUTPUT_RATE` → `AUDIO_OUTPUT_RATE`, `SOUND_PAL_CLOCK`
  → `AUDIO_PAL_CLOCK`, `SOUND_MUSIC_SEGMENTS` → `AUDIO_MUSIC_SEGMENTS`. The pair choice keeps
  `sound_compare_loudness` of the engine. Add the sample copy and the two new functions:

  ```c
  static int8_t pair_samples[PAIR_COUNT][UINT16_MAX]; /* the copied sample of each pair */

  void audio_init(void)
  {
      g_sound_play_hook = audio_play_data;
  }

  void audio_play_sample(int16_t sound, const uint8_t *volumes)
  {
      uint16_t length, period;
      const int8_t *data = sound_get_sample(sound, &length, &period);
      audio_play_data(data, length, period, volumes);
  }
  ```

  In `audio_play_data` copy `data` into `pair_samples[pair]` (`memcpy`, `length` bytes) and point both voices of
  the pair at the copy. A file with four or more file-scope variables keeps them in one `static struct`
  (standard rule 7.2): `static struct { Voice voices[AMIGA_CHANNELS]; uint8_t pair_volumes[PAIR_COUNT][SIDE_COUNT];
  int8_t pair_samples[PAIR_COUNT][UINT16_MAX]; Music music; } audio;` (`pair_channels` is `static const` and stays
  outside).
- [ ] **Step 3: Change `engine/sound.c` and `sound.h`.** Keep the rules; `sound_play_sample` becomes the static
  `play_sample`:

  ```c
  SoundPlayHook g_sound_play_hook;

  /* Plays a sound of the table at the volumes through the hook of the audio output. */
  static void play_sample(int16_t sound, const uint8_t *volumes)
  {
      uint16_t length, period;
      const int8_t *data = sound_get_sample(sound, &length, &period);
      if (data && g_sound_play_hook)
          g_sound_play_hook(data, length, period, volumes);
  }
  ```

  `sound_request_play` (mode now) and `sound_play_pending` call `play_sample`. Delete `sound_demo_play`. In
  `sound.h` delete the mixer prototypes, `SOUND_OUTPUT_RATE`, `SOUND_PAL_CLOCK`, `SOUND_MUSIC_SEGMENTS`; add
  the hook type and global; mark `sound_get_sample` (used by `ui/audio.c`), `g_pending_sounds` and
  `g_pending_sound_volumes` (`/* exported for the tests */` on the two globals). The purpose line of `sound.h`:
  `/* The sound rules: the sound table, the volume by distance, the pending queue, and the play requests of the
  rules. The audio output plays through g_sound_play_hook. */`.
- [ ] **Step 4: Change the callers.** `ui/ui.c` `ui_init` calls `audio_init()` as its first line.
  `ui/gameloop.c:142,167`: `sound_play_sample` → `audio_play_sample`. `ui/anim.c`: `sound_music_play` →
  `audio_music_play`, `sound_music_set_volume` → `audio_music_set_volume`, `SOUND_MUSIC_SEGMENTS` →
  `AUDIO_MUSIC_SEGMENTS`, `sound_play_data` → `audio_play_data`; `anim_close` (`port/src/ui/anim.c:84`) calls
  `audio_music_stop();` with the comment `/* the music segments point into the file */` instead of
  `sound_stop_all()`. `ui/logo.c`: `sound_play_data` → `audio_play_data`, `SOUND_PAL_CLOCK` → `AUDIO_PAL_CLOCK`;
  the `sound_stop_all()` line of `port/src/ui/logo.c:45` goes (the voice plays a copy). `ui/music.c:85` and
  `platform/platform.c:231,243`: `AUDIO_OUTPUT_RATE`; `platform/platform.c:248`: `audio_mix`. Each file includes
  `"ui/audio.h"` and keeps `"engine/sound.h"` only if it still uses a `SOUND_` name.
- [ ] **Step 5: Change the tests.** `tests/*/test_sound.py`: the fixture calls `lib.audio_init()` after the
  table load; `sound_stop_all` → `audio_stop_all`, `sound_mix` → `audio_mix`, `sound_play_sample(i, v)` →
  `audio_play_sample(i, v)`, `sound_demo_play(0)` → `audio_play_sample(0, vols(64, 64))` (the same full volume).
  `tests/*/test_csb_ending.py:59,334,531`, `tests/*/test_music.py:59,117-120`, `tests/*/test_entrance.py:96`: the
  same renames (rename the test `test_music_stop_after_sound_stop_all_...` to `..._after_audio_stop_all_...`).
  A test that plays through the engine and mixes calls `audio_init()` (or `ui_init`) first.
- [ ] **Step 6: Verify.** `make`; `uv run pytest -q tests/*/test_sound.py tests/*/test_music.py
  tests/*/test_csb_ending.py tests/*/test_entrance.py tests/*/test_anim*.py`; `uv run pytest -q tests/golden`;
  `uv run pytest -q tests/standard` (the layer check: `ui/audio.c` includes `engine/sound.h`, allowed; the engine
  includes no ui header).
- [ ] **Step 7: Commit.** `git add src/ui/audio.h src/ui/audio.c && git commit -am "refactor(audio): the engine plays through g_sound_play_hook, ui/audio.c mixes"`

### Task 41: The music module keeps its tables in music_tables.c

**Files:**
- Create: `src/ui/music_internal.h`
- Rename: `src/ui/musicmap.c` → `src/ui/music_tables.c` (`git mv`)
- Modify: `src/ui/music.h`, `src/ui/music.c`, `src/ui/startend.c:397`, `tests/*/test_music.py`,
  `tests/standard/allowlist.txt`, `docs/todo.md`

**Interfaces:**
- Consumes: Task 40 (`AUDIO_OUTPUT_RATE`).
- Produces: `void music_play_end(bool won);`; `src/ui/music_internal.h` declares
  `g_music_csb_square_tracks`, `g_music_fm_towns_dm_level_tracks`, `g_music_super_nes_triggers`,
  `MusicTrigger`, `MUSIC_FM_TOWNS_DM_LEVELS`, `MUSIC_SUPER_NES_TRIGGERS`.

Facts: the CSB square table is a public global of `music.h` defined in `musicmap.c`
(`port/src/ui/music.h:19`, `port/src/ui/musicmap.c:6`); two more soundtrack tables sit in `music.c`
(`fm_towns_dm_level_tracks`, `super_nes_triggers`, `port/src/ui/music.c:30-59`); the allowlist names both files
(`tests/standard_allowlist.txt:3-4`); `music_play_end(int16_t won)` (`port/src/ui/music.h:28`; `docs/todo.md:20`
asks for `bool`); the only caller passes `g_game_won` (`port/src/ui/startend.c:397`); `test_music.py:32` reads
`g_music_csb_square_tracks` by name; `music_mix` runs on the game thread (`port/src/platform/platform.c:241`).
The decoder include: R9.3 asks for `<dr_mp3/dr_mp3.h>`; package B moved the file to `third_party/dr_mp3/`.

- [ ] **Step 1: Check the decoder include.** `grep -n "dr_mp3" src/ui/music.c` and `ls third_party/dr_mp3/`.
  Assumption: package B rewrote the include to `<dr_mp3/dr_mp3.h>` and added `third_party/dr_mp3/README.md`
  (version, license). If not, change the include line under the existing pragma block
  (`port/src/ui/music.c:10-18`) and write the README with the version line of the `dr_mp3.h` header comment and
  its license (public domain or MIT-0, as the header says).
- [ ] **Step 2: Create `src/ui/music_internal.h`:**

  ```c
  /* SPDX-License-Identifier: GPL-3.0-or-later */
  #ifndef DM_UI_MUSIC_INTERNAL_H
  #define DM_UI_MUSIC_INTERNAL_H
  /* Prefixes: music_ */

  /* The soundtrack tables of the music module, shared by music.c and music_tables.c. The tables are not in the
     game files: they come from the FM-Towns and Super NES versions. */

  #include <stdint.h>
  #include "ui/music.h"

  /* Sizes of the soundtrack tables. */
  #define MUSIC_FM_TOWNS_DM_LEVELS 14
  #define MUSIC_SUPER_NES_TRIGGERS 22

  /* A Super NES track starts when the party steps on its square (map-relative Amiga squares). */
  typedef struct {
      uint8_t map, x, y, track;
  } MusicTrigger;

  extern const uint8_t g_music_csb_square_tracks[MUSIC_CSB_MAP_COUNT][MUSIC_CSB_MAP_SIZE][MUSIC_CSB_MAP_SIZE];
  extern const uint8_t g_music_fm_towns_dm_level_tracks[MUSIC_FM_TOWNS_DM_LEVELS];
  extern const MusicTrigger g_music_super_nes_triggers[MUSIC_SUPER_NES_TRIGGERS];

  #endif
  ```

  The counts are the element counts of `port/src/ui/music.c:31` (14 values) and `port/src/ui/music.c:36-59`
  (22 rows); `static_assert`-free: the compiler rejects a wrong count of an initializer that is too long, and a
  short one is caught by the `ARRAY_COUNT` loops staying on the declared size.
- [ ] **Step 3: Move the tables.** `git mv src/ui/musicmap.c src/ui/music_tables.c`; move the two tables of
  `music.c` into it as `g_music_fm_towns_dm_level_tracks` and `g_music_super_nes_triggers`;
  `music_tables.c` includes `"ui/music_internal.h"` only. Delete the `g_music_csb_square_tracks` line of
  `music.h`. `music.c` includes `"ui/music_internal.h"` and uses the new names
  (`port/src/ui/music.c:120-152`); `sizeof fm_towns_dm_level_tracks` becomes `MUSIC_FM_TOWNS_DM_LEVELS`.
- [ ] **Step 4: Names and comments of `music.h`.** `void music_play_end(bool won);` (`music.c` and
  `startend.c:397` follow; `music.h` includes `<stdbool.h>`); the comment of `music_mix`: `/* Adds the music to
  interleaved stereo int16 at AUDIO_OUTPUT_RATE. Runs on the game thread, after audio_mix. */`.
  `tests/*/test_music.py:195` sets `game.lib.music_play_end.argtypes = [ctypes.c_bool]`. Delete the item of
  `docs/todo.md:20`.
- [ ] **Step 5: The allowlist.** Replace the two lines `decimal ui/musicmap.c ...` and `decimal ui/music.c ...`
  by one line `decimal ui/music_tables.c the soundtrack tables of the other versions are not in the game files`
  (standard rule 3.12); `music.c` has no table left.
- [ ] **Step 6: Verify.** `make`; `uv run pytest -q tests/*/test_music.py tests/standard`.
- [ ] **Step 7: Commit.** `git add -A src/ui tests docs/todo.md && git commit -m "refactor(audio): music tables in music_tables.c, music_play_end takes a bool"`

#### Tail

Runs after the four streams are merged and `make` is green. The tail tasks run in this order.

### Task 42: Merge fix-ups: file names, ftl_at in the CSB image, tables_is_loaded, kaos_s8

**Files:**
- Rename: `src/data/csbimage.{c,h}` → `src/data/csb_image.{c,h}`, `src/data/csbtext.{c,h}` →
  `src/data/csb_text.{c,h}` (`git mv`)
- Modify: every includer (`grep -rln "csbimage.h\|csbtext.h" src tests docs`), `src/data/csb_image.c`,
  `src/data/tables.h`, `src/data/tables.c`, `src/engine/dungeon.c`, `tests/standard/allowlist.txt`, docs that name
  the old file names

**Interfaces:**
- Consumes: `ftl_at`, `kaos_s8`, `kaos_is_loaded` (Task 28).
- Produces: `bool tables_is_loaded(void);` in `src/data/tables.h` (`/* True after a successful tables_load. */`).

Facts: the file names do not match their prefixes `csb_image_`, `csb_text_` (`port/src/data/csbimage.h:3`,
`port/src/data/csbtext.h:3`); `appb_at` repeats the image address arithmetic without a bounds check
(`port/src/data/csbimage.c:154-157`), and `csbtext.c` computes image pointers by hand
(`out->data + (... - out->base)`, about `port/src/data/csbtext.c:196`); nine `(int8_t)kaos_u8(...)` casts in
`tables.c` (`grep -c "(int8_t)kaos_u8" src/data/tables.c`: 11 at commit `72e24b5`, review-data finding 37 counts
nine call lines); `engine/dungeon.c` calls `kaos_is_loaded` with an `open: epic 4` line (Task 28).

- [ ] **Step 1: Rename the files** with `git mv` and fix the includes and the docs:
  `grep -rl "csbimage\|csbtext" src tests docs tools CLAUDE.md | xargs sed -i '' -e 's/csbimage\.\([ch]\)/csb_image.\1/g' -e 's/csbtext\.\([ch]\)/csb_text.\1/g'`;
  the guards become `DM_DATA_CSB_IMAGE_H`, `DM_DATA_CSB_TEXT_H`. Leave `docs/superpowers/archive/` unchanged
  (its paths are evidence).
- [ ] **Step 2: `ftl_at` in the CSB image.** Delete `appb_at`; every use calls `ftl_at(image, address, bytes)`
  with the byte count of the copy (`g_csb_copies[i].size`, `KAOS_ACTION_SET_BYTES`, `sizeof(uint32_t)` for a
  pointer); a NULL result makes `csb_image_build` free and return -1 (the regression test of Task 36 still
  passes). The hand-made pointers of `csb_text.c` (`grep -n "\->base)" src/data/csb_text.c`) call `ftl_at`.
- [ ] **Step 3: `tables_is_loaded`.** Add to `tables.h` (Queries section) and `tables.c`:
  `bool tables_is_loaded(void) { return kaos_is_loaded(); }`; `engine/dungeon.c` calls it and includes
  `"data/tables.h"` (already included, `port/src/engine/dungeon.c:7`) instead of `"data/kaos.h"`. Delete the
  allowlist line `kaos engine/dungeon.c open: epic 4`.
- [ ] **Step 4: `kaos_s8`.** `sed -i '' 's/(int8_t)kaos_u8(/kaos_s8(/g' src/data/tables.c`; check
  `grep -n "(int8_t)kaos" src` is empty.
- [ ] **Step 5: Dead includes after the merge.** `uv run pytest -q tests/standard -k include` and remove each
  include that a merged file no longer uses (for example `kaos.h` in `spells.c`).
- [ ] **Step 6: Verify.** `make`; `uv run pytest -q tests/*/test_csb_engine.py tests/*/test_csb_text.py
  tests/*/test_tables.py tests/*/test_dungeon.py tests/standard`.
- [ ] **Step 7: Commit.** `git add -A && git commit -m "refactor(data): CSB file names match their prefixes, one image pointer helper"`

### Task 43: Image address names follow their accessors

**Files:**
- Modify: `src/data/kaos_addresses.h`, `src/data/tables.c`, `src/data/csb_image.c`, `docs/csb/csb-engine-diff.md`
  (after package C; was `docs/csb-engine-diff.md`), `docs/reference/kaos-symbols.csv` if it names them

**Interfaces:** none outside `data/`.

Facts: `KAOS_PLAQUE_HEIGHTS 0x102c57` is one byte before the table (`port/src/data/kaos_addresses.h:93`), so
`tables.c:500` indexes with a 1-based line count and `csbimage.c:103` copies from `KAOS_PLAQUE_HEIGHTS + 1`;
`docs/csb-engine-diff.md:173` gives the real table start `0x102c58`. Names against their accessor nouns
(`port/src/data/kaos_addresses.h:23,78,96,110,124,129`): `KAOS_ACTIONS_DISABLED`, `KAOS_REBIRTH_SCALES` (step 2
beside `KAOS_REBIRTH_STEP_1_SCALES`), `KAOS_FOUNTAIN_ORNAMENT_INDICES` (one byte), `KAOS_INITIAL_COLORMAP` (the
interface palette), `KAOS_INVISIBILITY_PALETTE_CHANGES`, `KAOS_ICON_GRAPHIC_FIRST_INDEX`. Uses: `tables.c:177,466,500,515,579,605,615`,
`csbimage.c:26,40,71,97,103,116,142`, `docs/csb-engine-diff.md:91,97,104,105,118,161,171,173`.

- [ ] **Step 1: Rename** with one sed over `src/data docs/csb docs/reference`:

  ```bash
  sed -i '' -e 's/KAOS_ACTIONS_DISABLED/KAOS_ACTION_DISABLED_TICKS/g' \
    -e 's/KAOS_REBIRTH_SCALES\b/KAOS_REBIRTH_STEP_2_SCALES/g' \
    -e 's/KAOS_FOUNTAIN_ORNAMENT_INDICES/KAOS_FOUNTAIN_ORNAMENT/g' \
    -e 's/KAOS_INITIAL_COLORMAP/KAOS_INTERFACE_PALETTE/g' \
    -e 's/KAOS_INVISIBILITY_PALETTE_CHANGES/KAOS_PALETTE_CHANGES_INVISIBILITY/g' \
    -e 's/KAOS_ICON_GRAPHIC_FIRST_INDEX/KAOS_ICON_SHEET_FIRST_ICONS/g' <files>
  ```

  BSD sed has no `\b`: write the second expression as `s/KAOS_REBIRTH_SCALES\([^_A-Z]\)/KAOS_REBIRTH_STEP_2_SCALES\1/g`
  and check `grep -rn "KAOS_REBIRTH_SCALES" src docs` afterwards (outside `docs/superpowers/`).
- [ ] **Step 2: The plaque table start.** `#define KAOS_PLAQUE_HEIGHTS 0x102c58 /* uint8_t: the height of a
  plaque by line count - 1 and depth */`; `tables_get_plaque_height` reads `KAOS_PLAQUE_HEIGHTS + (lines - 1) +
  TABLES_PLAQUE_LINES * depth` (`port/src/data/tables.c:500`; the address is the same for every call);
  `csb_image.c:103` copies `KAOS_PLAQUE_HEIGHTS` (no `+ 1`); `docs/csb/csb-engine-diff.md:173` names
  `KAOS_PLAQUE_HEIGHTS`.
- [ ] **Step 3: Keep the blocks in order.** `KAOS_REBIRTH_STEP_2_SCALES` stays after `KAOS_REBIRTH_STEP_1_SCALES`
  only if the block is sorted by address; the block order of `kaos_addresses.h` is by address (check the block
  and keep its order); `make format` aligns.
- [ ] **Step 4: Verify.** `make`; `uv run pytest -q tests/*/test_csb_engine.py tests/*/test_tables.py
  tests/golden tests/standard` (the `address` check: one name per address).
- [ ] **Step 5: Commit.** `git commit -am "refactor(data): image address names follow their accessors"`

### Task 44: Box in the tables API, a pointer struct, inline graphic fields

**Files:**
- Modify: `src/data/tables.h`, `src/data/tables.c`, `src/ui/dialog.c:47`, `src/ui/startend.c:315`,
  `src/ui/view.c:430`, `src/ui/gameloop.c:438`, `src/engine/group.c:223-226`

**Interfaces:**
- Consumes: `Box` (Task 39).
- Produces:
  ```c
  Box tables_get_layout_bitmap(int16_t slot);
  Box tables_get_csb_restart_box(bool inner);
  typedef struct { const uint8_t *planes; int16_t width, height; } PointerBitmap;
  PointerBitmap tables_get_pointer_bitmap(bool hand);
  static inline uint16_t tables_get_horizontal_offset_range(uint16_t graphic_info);
  static inline uint16_t tables_get_vertical_offset_range(uint16_t graphic_info);
  ```

Facts: `TablesRect` is field for field `Box` (`port/src/data/tables.h:247-249`); users `ui/dialog.c:47`,
`ui/startend.c:315`, `ui/view.c:430`; `tables_get_pointer_planes(bool, int16_t *w, int16_t *h)` has two
out-parameters (`port/src/data/tables.h:321`, standard rule 6.5), one caller `ui/gameloop.c:438`;
`GRAPHIC_HORIZONTAL_OFFSET_RANGE` and `GRAPHIC_VERTICAL_OFFSET_RANGE` are function-like macros
(`port/src/data/tables.h:71-72`), used in `engine/group.c:223-226`.

- [ ] **Step 1: `TablesRect` → `Box`.** Delete `TablesRect`; `tables.h` includes `"data/graphics.h"`; the two
  accessors return `Box`; the three ui users declare `Box`.
- [ ] **Step 2: The pointer bitmap.** Replace `tables_get_pointer_planes` by `tables_get_pointer_bitmap`
  (struct of **Interfaces**, `width` and `height` from the same `kaos_s16` reads, `port/src/data/tables.c:734-739`);
  `ui/gameloop.c:438` reads the struct.
- [ ] **Step 3: Inline the graphic fields** (`static inline` in `tables.h`, beside the `GRAPHIC_*` bits), with
  named constants `GRAPHIC_HORIZONTAL_OFFSET_SHIFT 12`, `GRAPHIC_OFFSET_RANGE_MASK 0x3`,
  `GRAPHIC_VERTICAL_OFFSET_SHIFT 14`; `group.c:223-226` calls them. Check that the `static inline` names follow
  the prefix rule (`tables_`).
- [ ] **Step 4: Verify.** `make`; `uv run pytest -q tests/golden tests/*/test_creatures.py tests/*/test_view.py
  tests/*/test_ui.py tests/standard`.
- [ ] **Step 5: Commit.** `git commit -am "refactor(data): Box in the tables API, a pointer bitmap struct, inline graphic fields"`

### Task 45: Byte order helpers are static inline functions

**Files:**
- Modify: `src/data/bytes.h` and every file that uses `BE16`, `BE32`, `WR16`, `WR32` (21 files, 262 uses at
  commit `72e24b5`: `grep -rnoE "\b(BE16|BE32|WR16|WR32)\(" port/src | grep -v dr_mp3 | wc -l`)

**Interfaces:**
- Produces (standard rule 3.10):
  ```c
  static inline uint16_t be16(const uint8_t *p);
  static inline uint32_t be32(const uint8_t *p);
  static inline void wr16(uint8_t *p, uint16_t value);
  static inline void wr32(uint8_t *p, uint32_t value);
  ```

Facts: the helpers are macros; `BE16` evaluates `p` twice and `WR16`/`WR32` use the comma operator
(`port/src/data/bytes.h:7-10`); standard rule 3.10 names them `be16`, `wr16`, `be32`, `wr32`, and rule 5.5 asks for a
`static_assert` of the arithmetic right shift in `data/bytes.h`. Largest users: `engine/savegame.c` 66,
`data/ftl.c` 32, `engine/savefile.c` 28, `engine/dungeon.c` 21, `engine/map.c` 20, `engine/map.h` 18.

- [ ] **Step 1: Write the functions** in `bytes.h` (purpose line `/* Big-endian access to the Amiga data: game
  files, save files, module images. */` moves above the includes):

  ```c
  #include <assert.h>
  #include <stdint.h>

  static_assert((-2 >> 1) == -1, "the port assumes an arithmetic right shift of a negative value");

  static inline uint16_t be16(const uint8_t *p)
  {
      return (uint16_t)(p[0] << BYTE_BITS | p[1]);
  }
  static inline uint32_t be32(const uint8_t *p)
  {
      return (uint32_t)be16(p) << WORD_BITS | be16(p + sizeof(uint16_t));
  }
  static inline void wr16(uint8_t *p, uint16_t value)
  {
      p[0] = (uint8_t)(value >> BYTE_BITS);
      p[1] = (uint8_t)value;
  }
  static inline void wr32(uint8_t *p, uint32_t value)
  {
      wr16(p, (uint16_t)(value >> WORD_BITS));
      wr16(p + sizeof(uint16_t), (uint16_t)value);
  }
  ```

  `WORD_BITS` and `BYTE_BITS` move above the functions.
- [ ] **Step 2: Rename the uses:** `grep -rlE "\b(BE16|BE32|WR16|WR32)\(" src | xargs sed -i '' -E 's/(^|[^A-Za-z0-9_])BE16\(/\1be16(/g; s/(^|[^A-Za-z0-9_])BE32\(/\1be32(/g; s/(^|[^A-Za-z0-9_])WR16\(/\1wr16(/g; s/(^|[^A-Za-z0-9_])WR32\(/\1wr32(/g'`.
  A `WR16(p, v)` whose `v` is signed now converts at the call; the compiler shows a site that needs a
  `(uint16_t)` cast (`-Wall -Wextra` give no conversion warning; read every `wr16(` site whose value is an
  `int16_t` expression and add the cast with no comment: it is the 16-bit wrap of the original).
- [ ] **Step 3: Verify.** `make`; `make windows`; `uv run pytest -q tests/*/test_savefile.py tests/*/test_save.py
  tests/golden tests/*/test_port_data.py tests/*/test_ftl.py tests/standard` (the `byteorder` check).
- [ ] **Step 4: Commit.** `git commit -am "refactor(data): byte order helpers are static inline functions"`

### Task 46: Data results are bool or a result enum

**Files:**
- Modify: `src/data/{ftl,kaos,tables,graphics,adf,csb_image,csb_text,adventure}.{h,c}`, their callers in
  `src/ui/`, `src/platform/`, and the tests that compare their results with `0` or `-1`

**Interfaces:**
- Produces (binding):
  ```c
  bool ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img);
  bool kaos_load(const char *path);
  bool tables_load(const char *path);
  bool gfx_open(const char *path);        /* the item count: gfx_count(), exported for the tests */
  bool gfx_expand(const uint8_t *packed, size_t len, Bitmap *bm);
  bool gfx_expand_over(const uint8_t *packed, size_t len, Bitmap *bm);
  bool gfx_load_bitmap(int16_t i, Bitmap *bm);
  bool gfx_load_strings(void);
  bool adf_extract(const char *adf_path, const char *dir);
  bool csb_image_build(const FtlImage *appb, FtlImage *out);
  bool csb_text_build(const FtlImage *appb, FtlImage *out);
  ```

Facts: `int 0/-1` results in `data/` (review-data finding 24): `ftl_load`, `kaos_load`, `tables_load`,
`gfx_expand`, `gfx_expand_over`, `gfx_load_bitmap`, `gfx_load_strings`, `adf_extract`, `csb_image_build`,
`csb_text_build`; `gfx_open` returns the count or -1 (`port/src/data/graphics.c:27-54`); the static helpers
`find_sections`, `join_texts`, `copy_action_names` return 0/-1 (`port/src/data/ftl.c:131`,
`port/src/data/csbtext.c` lines 141, 165); the ADF visitor returns 1/0/-1 (`port/src/data/adf.c:38,59-90`).
Standard rule 6.1: two outcomes are `bool`, more outcomes an enum type. `adventure_install_disk` got its enum in
Task 38.

- [ ] **Step 1: List every result use:** `grep -rnwE "(ftl_load|kaos_load|tables_load|gfx_open|gfx_expand|gfx_expand_over|gfx_load_bitmap|gfx_load_strings|adf_extract|csb_image_build|csb_text_build)" src tests tools`.
  Each call site is read and rewritten by hand: `if (f(...))` meant failure and becomes `if (!f(...))`;
  `!f(...)` meant success and becomes `f(...)`; `f(...) >= 0` (`gfx_open`) becomes `f(...)`; `== 0` in a test
  becomes a truth assertion.
- [ ] **Step 2: Change the functions** one module at a time (ftl, kaos, tables, graphics, adf, csb_image,
  csb_text), the prototypes of **Interfaces**, and the comments "Returns 0 or -1" become "False when ..."; the
  static helpers become `bool` too. The ADF visitor becomes `typedef enum { ADF_VISIT_CONTINUE, ADF_VISIT_STOP,
  ADF_VISIT_BROKEN } AdfVisit;` in `adf.c` (a result type; standard rule 6.1).
- [ ] **Step 3: Change the callers** of Step 1 (`ui/gameloop.c`, `ui/anim.c`, `ui/ui.c`, `platform/main.c`,
  `data/adventure.c`, `data/swsh.c`, `data/kaos.c`, `data/tables.c`), then the tests: `restype = ctypes.c_bool`
  for each function, and `assert lib.x(...)` / `assert not lib.x(...)` for `== 0` / `== -1`. The test of
  `gfx_open` that compares the count (`tests/test_port_data.py:140`) compares `gfx_count()`; `gfx_count` gets
  `/* exported for the tests */`.
- [ ] **Step 4: Verify.** `make`; `make windows`; `uv run pytest -q tests/*/test_port_data.py tests/*/test_ftl.py
  tests/*/test_adf.py tests/*/test_tables.py tests/*/test_csb_engine.py tests/*/test_csb_text.py
  tests/*/test_select.py tests/*/test_ui.py`; `uv run pytest -q tests/standard` (the `result` check of `data/`:
  delete its `open: epic 4` lines).
- [ ] **Step 5: Commit.** `git commit -am "refactor(data): loaders return bool"`

### Task 47: gfx_ functions get verbs

**Files:**
- Modify: `src/data/graphics.h`, `src/data/graphics.c`, every caller (`gfx_string` 42 sites outside `data/`,
  `gfx_object_name` 6, `gfx_action_name` 4; tests 9, 7, 5)

**Interfaces:**
- Produces: `const char *gfx_get_string(int16_t index);`, `const char *gfx_get_object_name(int16_t icon);`,
  `const char *gfx_get_action_name(int16_t action);`.

Facts: noun-only public functions `gfx_string`, `gfx_object_name`, `gfx_action_name` (`port/src/data/graphics.h:33-35`;
R4.8 names `gfx_get_string`); the comment of `gfx_object_name` names "graphic 0x2b6" instead of
`GFX_OBJECT_NAMES` (`port/src/data/graphics.h:34`).

- [ ] **Step 1: Rename:** `grep -rlE "gfx_(string|object_name|action_name)\b" src tests docs | xargs sed -i '' -E 's/gfx_(string|object_name|action_name)\(/gfx_get_\1(/g'`,
  then the `getattr`/attribute names in tests (`lib.gfx_string` → `lib.gfx_get_string`, `grep -rn "gfx_string\|gfx_object_name\|gfx_action_name" tests`).
- [ ] **Step 2: Comments** of the three prototypes: `/* A text of graphic GFX_STRINGS. */`, `/* An object name
  of graphic GFX_OBJECT_NAMES. */`, `/* An action name of graphic GFX_ACTION_NAMES. */` (check the three names in
  `ids.h`).
- [ ] **Step 3: Verify.** `make`; `uv run pytest -q tests/*/test_port_data.py tests/*/test_text.py tests/*/test_ui.py
  tests/golden tests/standard` (the `name` check of `data/`: delete its `open: epic 4` lines; `grep -n "open:
  epic 4" tests/standard/allowlist.txt` is empty after this task).
- [ ] **Step 4: Commit.** `git commit -am "refactor(data): gfx functions get verbs"`

---

## Package E: engine modules and records (Epic 5 without R5.4, R5.7, R5.8)

Three streams. E1 makes `engine/record.h/.c` the one owner of the record, square, map descriptor and dungeon
header bytes (R5.1). E2 splits `engine/game.h` (R5.2) and removes the `game_fatal` copies (R5.3 part). E3 removes
the other copied helpers (R5.3), and renames `savefile` (R5.5), `explored` and `message` (R5.6). The package has
no tail: the full suite runs after the three streams are merged.

Package facts (commit `72e24b5`, `port/src/`; the paths below are the paths after package B: `src/...`):

- Record bytes are read in `engine/map.h` (48 `REC(` uses, `port/src/engine/map.h:65-500`), `engine/map.c`
  (`dungeon_get_thing_data(thing)[N]`, `port/src/engine/map.c:281-431`), `engine/group.h` (`port/src/engine/group.h:93-141`),
  `engine/projectile.h` (`port/src/engine/projectile.h:56-92`) and `engine/dungeon.c:286, 564`.
- Raw square reads `g_current_map_data[x][y]` outside `map.c`: `group.c:356, 707, 726, 882, 2102`,
  `timeline_process.c:68, 107, 114, 131, 151, 379`, `move.c:155`, `sensor.c:111, 422`, `projectile.c:272, 460, 508`;
  `g_dungeon_map_data[` in `group.c:158`, `ui/view.c:2186`, `dungeon.c:245-246, 339, 463, 471`.
- `engine/game.h` (`port/src/engine/game.h:1-229`) holds seven prefixes (`game.h:3`), the structs `Champion` (75),
  `Party` (136), `TimelineEvent` (146), `ActiveGroup` (154), 29 globals (162-191), the timeline API (210-220),
  the 16-bit helpers (203-206) and the statistic helpers (224-227).
- `tests/test_game.py:129-134` compares `min16`, `max16`, `bounded_value`, `scale_value` of the library with the
  original in the emulator: these four must stay exported symbols of `libdm.dylib`.
- The test modules are in the folders of package C. A test module is found by name:
  `$(find tests -name 'test_game.py')`. Several modules run with `-n auto` (python-uv skill).

Every task of this package ends with `make` (0 warnings) and `uv run pytest -q tests/standard` green. A task that
moves a declaration runs the standard check `include` and deletes each include line that the check reports as
unused in a file the task changed.

Shared script of this package (each task that needs it carries the full text): `add_includes.py`, written to the
SDD workspace, not to the repository.

---

#### Stream E1: the record module

Shared files: `src/engine/map.h`, `src/engine/map.c`, `src/engine/dungeon.c`, `src/engine/group.h`,
`src/engine/projectile.h`, `src/engine/timeline_process.c` (E2 renames it to `event.c`), `src/engine/group.c`,
`src/engine/move.c`, `src/engine/sensor.c`, `src/engine/projectile.c`, `src/ui/view.c`, `src/ui/automap.c`,
`src/control/state_json.c` (or `src/platform/state_json.c` before package H), `src/platform/main.c`, and the test
modules that bind renamed names. E2 and E3 edit other lines of these files; expect merges.

### Task 48: record module for the thing records

**Files:**
- Create: `src/engine/record.h`, `src/engine/record.c`
- Modify: `src/engine/map.h` (delete lines of `REC`, `thing_set_next`, the object, sensor, teleporter, door, text,
  group, projectile and explosion accessors: `port/src/engine/map.h:65-307, 311-314, 408-456`), `src/engine/map.c`
  (delete the accessors `port/src/engine/map.c:263-431`), `src/engine/group.h` (delete `port/src/engine/group.h:84-141`),
  `src/engine/projectile.h` (delete `port/src/engine/projectile.h:55-92`), `src/engine/dungeon.h`,
  `src/engine/dungeon.c` (`dungeon_get_thing_data` at 482-486 goes; `get_carried_first_thing` at 284-287 and the text
  offset at 564 use accessors), every caller of a renamed name in `src/`, `tests/`, `tools/`, `docs/` (not
  `docs/superpowers/`)
- Modify: `tests/standard/allowlist.txt` (delete the two lines `hex engine/map.c ...` and `hex engine/map.h ...` of
  the record layout accessors, today `tests/standard_allowlist.txt:7-8`)
- Test: the existing tests (no new test: the golden and differential tests cover every field)

**Interfaces:**
- Consumes: `be16`, `wr16` of `src/data/bytes.h` (package D, R4.9). Check the names in `src/data/bytes.h` first.
- Produces (`src/engine/record.h`, prefix `rec_`): `const uint8_t *rec_read(uint16_t thing)`,
  `uint8_t *rec_write(uint16_t thing)`, and the accessors of the rename map below with these types: a getter takes
  `uint16_t thing` and returns `uint16_t` for a word or a type, `uint8_t` for a byte, `int16_t` for a coordinate
  or a map, `bool` for an `is_`/`has_` name; a setter returns `void` and takes `(uint16_t thing, <type of the
  getter> value)`; `rec_group_health(uint16_t group, int16_t creature)` and
  `rec_group_set_health(uint16_t group, int16_t creature, uint16_t health)`;
  `rec_object_set_important(uint16_t thing, bool important)`; `uint16_t rec_find_unused_thing(uint16_t thing_type)`.
  Public constants: `GROUP_RECORD_BYTES` (used by `dungeon.c:20`), `NO_JUNK_RESERVE`.

Rename map of the thing records (old name, source line, new name). Every getter and setter keeps its arithmetic;
a byte access stays a byte access, so the bytes that a setter writes do not change.

| Old | Source | New |
|---|---|---|
| `REC(thing)` | `map.h:65` | private `rec_write` in `record.c` |
| `dungeon_get_thing_data` | `dungeon.c:482` | `rec_write` (and `rec_read` for every read) |
| `thing_set_next` | `map.h:68` | `rec_thing_set_next` |
| `map_get_next_thing` | `map.c:115` | `rec_thing_next` |
| `thing_set_free` | `map.c:281` | `rec_thing_set_free` |
| `map_get_unused_thing` | `map.c:263` | `rec_find_unused_thing` |
| `get_carried_first_thing` (static) | `dungeon.c:284` | `rec_group_possessions` / `rec_container_first_thing` at the one caller (`dungeon.c:326`) |
| `door_type` | `map.h:259` | `rec_door_type` |
| `door_breaks_by_magic` (`int16_t`) | `map.h:263` | `rec_door_is_magic_breakable` (`bool`) |
| `door_breaks_by_force` (`int16_t`) | `map.h:267` | `rec_door_is_force_breakable` (`bool`) |
| `door_button_present` (`int16_t`) | `map.h:271` | `rec_door_has_button` (`bool`) |
| `door_is_vertical` | `map.h:276` | `rec_door_is_vertical` |
| `door_ornament_ordinal` | `map.h:280` | `rec_door_ornament` (stored value: 0 none, else ornament index + 1; comment says so) |
| `teleporter_target_x` | `map.h:229` | `rec_teleporter_target_x` |
| `teleporter_target_y` | `map.h:233` | `rec_teleporter_target_y` |
| `teleporter_target_map` | `map.h:237` | `rec_teleporter_target_map` |
| `teleporter_scope` | `map.h:241` | `rec_teleporter_scope` |
| `teleporter_rotation` | `map.h:245` | `rec_teleporter_rotation` |
| `teleporter_is_rotation_absolute` | `map.h:249` | `rec_teleporter_is_absolute` |
| `teleporter_is_audible` | `map.h:253` | `rec_teleporter_is_audible` |
| `rec_text_is_visible` | `map.h:290` | `rec_text_is_visible` |
| `rec_text_set_visible` | `map.h:294` | `rec_text_set_visible` |
| text offset `BE16(REC(t) + 2) >> 3` | `dungeon.c:564` | `rec_text_offset` |
| `rec_sensor_type` | `map.h:138` | `rec_sensor_type` |
| `rec_sensor_data` | `map.h:142` | `rec_sensor_data` |
| `rec_sensor_set_type` | `map.h:146` | `rec_sensor_set_type` |
| `rec_sensor_set_data` | `map.h:150` | `rec_sensor_set_data` |
| `rec_sensor_disable` | `map.c:428` | goes; callers write `rec_sensor_set_type(thing, SENSOR_DISABLED)` (same bytes: `byte 3 &= 0x80` equals the type setter with type 0) |
| `rec_sensor_is_local` | `map.h:155` | `rec_sensor_is_local` |
| `rec_sensor_delay` | `map.h:159` | `rec_sensor_delay` |
| `rec_sensor_is_once_only` | `map.h:163` | `rec_sensor_is_once_only` |
| `rec_sensor_effect` | `map.h:167` | `rec_sensor_effect` |
| `rec_sensor_revert` (`uint16_t`, used as a truth value at `sensor.c:297`, `timeline_process.c:230`) | `map.h:171` | `rec_sensor_is_reverted` (`bool`) |
| `rec_sensor_is_audible` | `map.h:175` | `rec_sensor_is_audible` |
| `rec_sensor_target_x` | `map.h:179` | `rec_sensor_target_x` |
| `rec_sensor_target_y` | `map.h:183` | `rec_sensor_target_y` |
| `rec_sensor_target_cell` | `map.h:187` | `rec_sensor_target_cell` |
| `rec_sensor_local_effect` | `map.h:191` | `rec_sensor_local_effect` |
| `rec_sensor_generator_delay` | `map.h:195` | `rec_sensor_generator_delay` |
| `rec_sensor_ornament_ordinal` | `map.h:199` | `rec_sensor_ornament` (stored value, as the door) |
| `rec_sensor_generator_creature_type` (34 characters) | `map.h:205` | `rec_sensor_creature_type` |
| `rec_sensor_generator_count_field` | `map.h:209` | `rec_sensor_generator_count` (the same bits as the delay; the comment says so) |
| `rec_sensor_generator_health_multiplier` | `map.h:213` | `rec_sensor_health_multiplier` |
| `rec_sensor_launcher_kinetic_energy` | `map.h:218` | `rec_sensor_kinetic_energy` |
| `rec_sensor_launcher_step_energy` | `map.h:222` | `rec_sensor_step_energy` |
| `group_possessions` | `group.h:93` | `rec_group_possessions` |
| `group_set_possessions` | `group.h:97` | `rec_group_set_possessions` |
| `rec_group_creature_type` | `map.h:301` | `rec_group_creature_type` |
| `group_set_creature_type` | `group.h:101` | `rec_group_set_creature_type` |
| `rec_group_cells_byte` | `map.h:420` | `rec_group_cells` |
| `group_set_cells_byte` | `group.h:105` | `rec_group_set_cells` |
| `group_health` | `group.h:109` | `rec_group_health` |
| `group_set_health` | `group.h:113` | `rec_group_set_health` |
| `group_behavior` | `group.h:117` | `rec_group_behavior` |
| `group_set_behavior` | `group.h:121` | `rec_group_set_behavior` |
| `rec_group_last_creature_index` | `map.h:425` | `rec_group_last_creature` |
| `group_set_last_creature_index` (33 characters) | `group.h:126` | `rec_group_set_last_creature` |
| `rec_group_direction` | `map.h:429` | `rec_group_direction` |
| `group_set_direction_field` | `group.h:132` | `rec_group_set_direction` |
| `group_clear_do_not_discard` | `group.h:138` | `rec_group_set_do_not_discard(group, false)` (one caller, `group.c:380`) |
| `weapon_type`, `thing_weapon_type` (two names of one field) | `map.c:285`, `map.h:412` | `rec_weapon_type` |
| `armour_type` | `map.c:289` | `rec_armour_type` |
| `junk_type` | `map.c:293` | `rec_junk_type` |
| `potion_type` | `map.c:297` | `rec_potion_type` |
| `potion_power` | `map.c:301` | `rec_potion_power` |
| `container_type` | `map.c:305` | `rec_container_type` |
| `container_first_thing` | `map.c:309` | `rec_container_first_thing` |
| `rec_object_is_cursed` | `map.c:313` | `rec_object_is_cursed` |
| `rec_object_set_important`, `rec_object_clear_important` | `map.c:317, 321` | `rec_object_set_important(thing, bool)` |
| `weapon_is_lit` | `map.c:325` | `rec_weapon_is_lit` |
| `weapon_charges` | `map.c:329` | `rec_weapon_charges` |
| `junk_charges` | `map.c:333` | `rec_junk_charges` |
| `armour_charges` | `map.c:337` | `rec_armour_charges` |
| `scroll_is_closed` | `map.c:341` | `rec_scroll_is_closed` |
| `weapon_is_poisoned` | `map.c:345` | `rec_weapon_is_poisoned` |
| `weapon_is_broken` | `map.c:349` | `rec_weapon_is_broken` |
| `armour_is_broken` | `map.c:353` | `rec_armour_is_broken` |
| `scroll_text_index` | `map.c:357` | `rec_scroll_text_index` |
| `container_set_first_thing` | `map.c:361` | `rec_container_set_first_thing` |
| `weapon_set_lit` | `map.c:366` | `rec_weapon_set_lit` |
| `weapon_set_charges` | `map.c:372` | `rec_weapon_set_charges` |
| `armour_set_charges` | `map.c:378` | `rec_armour_set_charges` |
| `junk_set_charges` | `map.c:384` | `rec_junk_set_charges` |
| `junk_set_type` | `map.c:390` | `rec_junk_set_type` |
| `potion_set_type` | `map.c:396` | `rec_potion_set_type` |
| `potion_set_power` | `map.c:402` | `rec_potion_set_power` |
| `rec_object_set_type` | `map.c:408` | `rec_object_set_type` |
| `rec_object_set_cursed` | `map.c:414` | `rec_object_set_cursed` |
| `scroll_set_closed` | `map.c:421` | `rec_scroll_set_closed` |
| `rec_projectile_slot` | `map.h:435` | `rec_projectile_slot` |
| `projectile_set_slot` | `projectile.h:56` | `rec_projectile_set_slot` |
| `rec_projectile_kinetic_energy` | `map.h:439` | `rec_projectile_energy` |
| `projectile_set_kinetic_energy` (33 characters) | `projectile.h:64` | `rec_projectile_set_energy` |
| `projectile_attack` | `projectile.h:60` | `rec_projectile_attack` |
| `projectile_set_attack` | `projectile.h:68` | `rec_projectile_set_attack` |
| `rec_projectile_event_index` | `map.h:311` | `rec_projectile_event_index` |
| `projectile_set_event_index` | `projectile.h:72` | `rec_projectile_set_event_index` |
| `rec_explosion_attack` | `map.h:445` | `rec_explosion_attack` |
| `explosion_set_attack` | `projectile.h:79` | `rec_explosion_set_attack` |
| `rec_explosion_type` | `map.h:449` | `rec_explosion_type` |
| `explosion_set_type` | `projectile.h:83` | `rec_explosion_set_type` |
| `rec_explosion_is_centered` | `map.h:453` | `rec_explosion_is_centered` |
| `explosion_set_centered` | `projectile.h:88` | `rec_explosion_set_centered` |
| `THING_END` (alias) | `map.h:55` | `THING_ENDOFLIST` (27 uses) |
| `rec_creature_height(attributes)` (not a record field) | `map.h:305` | `creature_height(attributes)` in `group.h`, beside `creature_sight_range` (`group.h:155`) |
| `CREATURE_LEVITATES`, `CREATURE_NON_MATERIAL` | `map.h:300` | the creature attribute block of `group.h:16-27` |

Named constants (private to `record.c`, one block per record; offsets and masks hex, shifts decimal; the values
are the ones of the current code):

```c
/* Byte offsets of the fields of every record: the next thing of the list. */
#define THING_NEXT 0x00
/* Door record bytes and bits. */
#define DOOR_FLAGS          0x02
#define DOOR_ATTRIBUTES     0x03
#define DOOR_FORCE_BIT      0x01 /* in DOOR_FLAGS */
#define DOOR_TYPE_BIT       0x01 /* in DOOR_ATTRIBUTES */
#define DOOR_ORNAMENT_MASK  0x1e
#define DOOR_VERTICAL_BIT   0x20
#define DOOR_BUTTON_BIT     0x40
#define DOOR_MAGIC_BIT      0x80
/* Bit shifts of the door record. */
#define DOOR_ORNAMENT_SHIFT 1
```

The same shape for the other records, with these values (byte offset: bits):
teleporter `0x02`: rotation `0x0c` shift 2, absolute `0x10`, scope `0x60` shift 5, audible `0x80`; `0x03`: target x
`MAP_COORD_MASK`; word `0x02`: target y shift 5 mask `MAP_COORD_MASK`; `0x04`: target map.
Text: `0x03`: visible `0x01`; word `0x02`: offset shift 3.
Sensor: word `0x02`: type mask `0x7f`, data shift 7; `0x03`: the type byte; `0x04`: local `0x08`, ornament shift 4;
word `0x04`: delay shift 7 mask `0x0f`; `0x05`: once only `0x04`, effect `0x18` shift 3, revert `0x20` shift 5,
audible `0x40`; word `0x06`: target x shift 6 mask `MAP_COORD_MASK`, local effect shift 4, health multiplier shift 4
mask `0x0f`, kinetic energy shift 4 (the `uint8_t` result keeps 8 bits); `0x06`: target y shift 3, generator delay,
step energy shift 4; `0x07`: target cell `0x30` shift 4.
Group: word `0x02` possessions; `0x04` creature type; `0x05` cells; words `0x06` + 2 * creature health; `0x0e`:
direction `0x03`, do not discard `0x04`; `0x0f`: behavior `0x0f`, last creature `0x60` shift 5.
Objects: `0x03`: type `0x7f`, important `0x80`; `0x02`: cursed `0x01`, weapon poisoned `0x02`, weapon charges `0x3c`
shift 2, weapon broken `0x40`, weapon lit `0x80`, armour charges `0x1e` shift 1, armour broken `0x20`, junk charges
shift 6, scroll closed `0xfc` (set: `0x03` kept, `0x04` set); word `0x02`: scroll text index `0x3ff`, container
first thing; `0x02`: potion type `0x7f` (set keeps `0x80`); `0x03`: potion power; `0x05`: container type `0x06` shift 1.
Projectile: word `0x02` slot, `0x04` kinetic energy, `0x05` attack, word `0x06` event index.
Explosion: `0x02` attack; `0x03`: type `0x7f`, centered `0x80`.

`record.c` has two helpers at the top of the functions:

```c
/* The value of the bits mask << shift of word. */
static uint16_t bits(uint16_t word, int16_t shift, uint16_t mask)
{
    return word >> shift & mask;
}

#define REC(thing) rec_write(thing)
```

- [ ] **Step 1: List the call sites.** Run
  `grep -rnwE "$(cut -d' ' -f1 renames-e1.txt | paste -sd'|' -)" src tests tools docs --exclude-dir=superpowers | wc -l`
  after you write `renames-e1.txt` (Step 2). Expected: about 600 lines. Check that `be16` and `wr16` exist in
  `src/data/bytes.h` (`grep -n "be16\|wr16" src/data/bytes.h`); if package D kept `BE16`/`WR16`, use them.
- [ ] **Step 2: Write the rename list** `renames-e1.txt` in the SDD workspace: one line `old new` per row of the
  table above where the name changes, plus `THING_END THING_ENDOFLIST`. Do not list `REC`,
  `dungeon_get_thing_data`, `rec_sensor_disable`, `get_carried_first_thing`, `group_clear_do_not_discard`,
  `rec_object_clear_important`: Steps 4 and 5 change their callers by hand.
- [ ] **Step 3: Write `record.h` and `record.c`.** Header head as in standard 2.1 (guard `DM_ENGINE_RECORD_H`,
  `/* Prefixes: rec_ */`, purpose paragraph "The byte layout of the dungeon records: one accessor per field, no
  record byte outside this module."), includes `<stdbool.h>`, `<stdint.h>` only. The prototypes stand in thing
  type order (door, teleporter, text, sensor, group, weapon, armour, scroll, potion, container, junk, projectile,
  explosion) after `rec_read`, `rec_write` and the generic `rec_thing_*`, one section comment per record. Move each
  body from its source line; replace each raw offset and mask by the constant of the block above. One-line record
  accessors get no comment (standard 9.1); keep the existing comments of the others (`map.h:203-204, 217`,
  `map.c:260-262, 407, 420`).
  `rec_read` and `rec_write` take the body of `dungeon_get_thing_data` (`dungeon.c:482-486`).
- [ ] **Step 4: Rename with the script** (run from the repository root; it touches `src/`, `tests/`, `tools/`,
  `docs/` without `docs/superpowers/`):

  ```python
  # rename.py: replaces whole words; usage: uv run python rename.py renames-e1.txt
  import re
  import sys
  from pathlib import Path

  pairs = [line.split() for line in Path(sys.argv[1]).read_text().splitlines() if line.strip()]
  pattern = re.compile(r"\b(" + "|".join(re.escape(old) for old, _ in pairs) + r")\b")
  new = dict(pairs)
  roots = [Path("src"), Path("tests"), Path("tools"), Path("docs")]
  for root in roots:
      for path in root.rglob("*"):
          if path.suffix not in {".c", ".h", ".py", ".md", ".csv"} or "superpowers" in path.parts:
              continue
          text = path.read_text()
          changed = pattern.sub(lambda m: new[m.group(1)], text)
          if changed != text:
              path.write_text(changed)
              print(path)
  ```

- [ ] **Step 5: Change the callers by hand:** `rec_sensor_disable(t)` → `rec_sensor_set_type(t, SENSOR_DISABLED)`;
  `rec_object_clear_important(t)` → `rec_object_set_important(t, false)` and `rec_object_set_important(t)` →
  `(t, true)`; `group_clear_do_not_discard(g)` → `rec_group_set_do_not_discard(g, false)`; the uses of
  `rec_sensor_is_reverted`, `rec_door_is_*_breakable`, `rec_door_has_button` as numbers (grep each, for example
  `effect ^ rec_sensor_is_reverted(...)`) get an explicit `? 1 : 0` only where the value enters arithmetic.
  `dungeon.c:286` and `dungeon.c:564` use the accessors. `creature_height` and the two creature bits move to
  `group.h`.
- [ ] **Step 6: Add the includes** with `add_includes.py` and the map `{"engine/record.h": [<every rec_ name>],
  "engine/group.h": ["creature_height", "CREATURE_LEVITATES", "CREATURE_NON_MATERIAL"]}`:

  ```python
  # add_includes.py: adds #include "<header>" to each src/ file that uses a symbol of the header.
  # usage: uv run python add_includes.py map.json   (map: {"engine/record.h": ["rec_read", ...]})
  import json
  import re
  import sys
  from pathlib import Path

  mapping = json.loads(Path(sys.argv[1]).read_text())
  for path in sorted(Path("src").rglob("*.[ch]")):
      rel = path.relative_to("src").as_posix()
      text = path.read_text()
      added = [
          f'#include "{header}"\n'
          for header, symbols in mapping.items()
          if rel != header
          and f'#include "{header}"' not in text
          and re.search(r"\b(" + "|".join(map(re.escape, symbols)) + r")\b", text)
      ]
      if not added:
          continue
      lines = text.splitlines(keepends=True)
      last = max((i for i, line in enumerate(lines) if line.startswith('#include "')), default=-1)
      lines[last + 1 : last + 1] = added
      path.write_text("".join(lines))
      print(rel, *(a.split('"')[1] for a in added))
  ```

  Then `make format`.
- [ ] **Step 7: Build and test.** `make` (0 warnings); `uv run pytest -q tests/standard`; delete the include lines
  that the `include` check reports for the files of this task;
  `uv run pytest -q -n auto $(find tests -name 'test_golden_*.py' -o -name 'test_port*.py' -o -name 'test_dungeon.py' -o -name 'test_creatures.py' -o -name 'test_move.py' -o -name 'test_champion.py' -o -name 'test_csb_text.py' -o -name 'test_spells.py' -o -name 'test_actions.py')`.
  Expected: all pass, the golden snapshots unchanged.
- [ ] **Step 8: Symbol table.** `grep -n` each old name in `docs/reference/kaos-symbols.csv`; the script of Step 4
  changed them; check that each row still names one port function.
- [ ] **Step 9: Commit.** `git add -A src tests tools docs && git commit -m "refactor: one record module owns the thing record layout"`

### Task 49: record module for squares, map descriptors and the dungeon header

**Files:**
- Modify: `src/engine/record.h`, `src/engine/record.c`, `src/engine/map.h` (delete `SQUARE_TYPE`, `door_state`,
  `square_with_door_state`: `port/src/engine/map.h:27-36`; the map descriptor block 316-354, 380-386, 458-494; the
  header accessors 356-362, 496-500; `g_current_map_data` at 365), `src/engine/map.c` (`map_set_current_map`
  31-32, the reads 58-67, 124-128, 181, 215), `src/engine/dungeon.h` (`dungeon_get_square` at 34),
  `src/engine/dungeon.c` (`build_map_columns` 238-252, `validate_squares` 339, `dungeon_get_square` 461-464,
  the header statics 127-142), the raw readers: `group.c:158, 356, 707, 726, 882, 2102`,
  `timeline_process.c:68-108, 114, 131, 151, 379-383`, `move.c:155`, `sensor.c:111, 422`,
  `projectile.c:272, 460, 508`, `ui/view.c:2186`, `ui/automap.c:28, 222`, `ui/view.c:286-295, 1441`,
  `state_json.c:556, 1001`, `platform/main.c:120`
- Modify: `tests/standard/allowlist.txt` (delete the `record ... open: epic 5` lines)

**Interfaces:**
- Consumes: `record.h` of Task 48.
- Produces (`record.h`):
  - Squares: `uint8_t rec_square_byte(int16_t map, int16_t x, int16_t y)` (was `dungeon_get_square`, no bound
    check), `void rec_square_set_byte(int16_t map, int16_t x, int16_t y, uint8_t square)`,
    `int16_t rec_square_type(uint8_t square)` (was the macro `SQUARE_TYPE`, 58 uses),
    `int16_t rec_door_state(uint8_t square)` (was `door_state`),
    `void rec_door_set_state(int16_t map, int16_t x, int16_t y, int16_t state)` (replaces
    `*square = square_with_door_state(*square, state)`).
  - Map descriptor (was `map_*` in `map.h`): `rec_map_descriptor(int16_t map)` returns `const uint8_t *`;
    `rec_map_raw_offset` (was the static `map_raw_data_offset`, `dungeon.c:137`), `rec_map_offset_x`,
    `rec_map_offset_y`, `rec_map_level`, `rec_map_last_x`, `rec_map_last_y`, `rec_map_door_set(map, door_type)`,
    `rec_map_random_floor_ornaments` (was `map_random_floor_ornament_count`), `rec_map_floor_ornaments`,
    `rec_map_random_wall_ornaments`, `rec_map_wall_ornaments`, `rec_map_difficulty`, `rec_map_door_ornaments`,
    `rec_map_creature_types` (was `map_creature_type_count`), `rec_map_floor_set`, `rec_map_wall_set`, all
    `(int16_t map)` returning `int16_t` (`rec_map_level` `uint16_t`, as today);
    `const uint8_t *rec_map_type_lists(int16_t map)`: the creature types, then the ornament indices that follow the
    squares of a map (replaces `group.c:158` and `view.c:2186`);
    `uint16_t rec_current_map_difficulty(void)` (was `map_current_difficulty`, `map.h:383`; it reads
    `g_current_map`, because `tests/test_champion.py:164` points `g_current_map` to its own bytes).
  - Dungeon header: `uint16_t rec_dungeon_square_things(void)` (was `rec_dungeon_square_first_thing_count`, 36
    characters), `void rec_dungeon_set_square_things(uint16_t count)` (the write of `dungeon.c:187`),
    `uint16_t rec_dungeon_thing_count(int16_t type)`, `void rec_dungeon_set_thing_count(int16_t type, uint16_t
    count)` (the statics of `dungeon.c:131-138` and the read of `map.c:267`), `uint16_t rec_dungeon_ornament_seed(void)`.
  - Public constants: `MAP_BYTES` (`map.h:317`, used by `dungeon.c`).
  - `g_current_map_data` goes. `g_current_map` stays in `map.h` as `const uint8_t *`.

Named constants of `record.c`: map descriptor bytes `0x00` raw offset (word), `0x06` offset x, `0x07` offset y,
`0x08` last x and y (word: x shift 6 mask `MAP_COORD_MASK`; byte: y shift 3), `0x09` level (mask `0x3f`), `0x0a`
floor ornaments (random count high nibble, count low nibble), `0x0b` wall ornaments, `0x0c` difficulty (shift 4),
`0x0d` door ornaments (low nibble) and creature types (high nibble), `0x0e` door sets (set 0 low, set 1 high),
`0x0f` graphic sets (floor low, wall high); dungeon header words `0x00` ornament seed, `0x0a` square things, `0x0c`
thing counts (one word per type). The values come from `map.h:318-329, 357-358` and `dungeon.c:131-138`.

- [ ] **Step 1: Check the assumptions.** `grep -rn "g_current_map_data\|g_dungeon_map_data\[\|MAP_DESCRIPTOR\|g_current_map\[" src tests`:
  expected only the lines that the Files list names, plus `record.c`. `grep -rn "g_current_map\b" src | grep "="`:
  Assumption: only `map.c:32` and `map_reset` write `g_current_map`; if another writer exists, keep its write.
- [ ] **Step 2: Move the functions** into `record.c` in this order after the explosion section: squares, map
  descriptor, dungeon header, each with a section comment in `record.h` and `record.c`. `rec_square_byte` reads
  `g_dungeon_map_data[map][x][y]`. `rec_door_set_state` writes
  `(square & ~DOOR_STATE_MASK) | (state & DOOR_STATE_MASK)` (the body of `square_with_door_state`, `map.h:33-36`).
- [ ] **Step 3: Rename** with `rename.py` (the script of Task 48, text below) and the list `SQUARE_TYPE rec_square_type`,
  `door_state rec_door_state`, `dungeon_get_square rec_square_byte`, `map_offset_x rec_map_offset_x`,
  `map_offset_y rec_map_offset_y`, `map_level rec_map_level`, `map_last_x rec_map_last_x`,
  `map_last_y rec_map_last_y`, `map_door_set rec_map_door_set`,
  `map_random_floor_ornament_count rec_map_random_floor_ornaments`,
  `map_floor_ornament_count rec_map_floor_ornaments`,
  `map_random_wall_ornament_count rec_map_random_wall_ornaments`,
  `map_wall_ornament_count rec_map_wall_ornaments`, `map_difficulty rec_map_difficulty`,
  `map_door_ornament_count rec_map_door_ornaments`, `map_creature_type_count rec_map_creature_types`,
  `map_floor_set rec_map_floor_set`, `map_wall_set rec_map_wall_set`,
  `map_current_difficulty rec_current_map_difficulty`,
  `rec_dungeon_square_first_thing_count rec_dungeon_square_things`.

  ```python
  # rename.py: replaces whole words; usage: uv run python rename.py renames.txt
  import re
  import sys
  from pathlib import Path

  pairs = [line.split() for line in Path(sys.argv[1]).read_text().splitlines() if line.strip()]
  pattern = re.compile(r"\b(" + "|".join(re.escape(old) for old, _ in pairs) + r")\b")
  new = dict(pairs)
  for root in [Path("src"), Path("tests"), Path("tools"), Path("docs")]:
      for path in root.rglob("*"):
          if path.suffix not in {".c", ".h", ".py", ".md", ".csv"} or "superpowers" in path.parts:
              continue
          text = path.read_text()
          changed = pattern.sub(lambda m: new[m.group(1)], text)
          if changed != text:
              path.write_text(changed)
              print(path)
  ```

- [ ] **Step 4: Rewrite the raw square reads** of the Files list: a read `g_current_map_data[x][y]` becomes
  `rec_square_byte(g_current_map_index, x, y)`; a pointer `uint8_t *square = &g_current_map_data[x][y]` followed by
  `*square = square_with_door_state(*square, s)` becomes `rec_door_set_state(g_current_map_index, x, y, s)`;
  `g_current_map_data[x][y] &= ~SQUARE_HAS_THINGS` (`map.c:215`) and the write of `map.c:181` become
  `rec_square_set_byte(...)` with the same bit operation. `group.c:158` and `view.c:2186` call `rec_map_type_lists`.
  `map_set_current_map` drops `g_current_map_data` and sets `g_current_map = rec_map_descriptor(map)`.
- [ ] **Step 5: The loader.** `build_map_columns` (`dungeon.c:238-252`) fills a local `uint8_t ***columns` and
  assigns `g_dungeon_map_data = columns` at the end, so the loader builds the column table without an indexed write
  of `g_dungeon_map_data`; `validate_squares` (`dungeon.c:339`) calls `rec_square_byte(m, x, y)`; `read_square_things_and_text`
  calls `rec_dungeon_set_square_things`; `thing_count`, `set_thing_count`, `map_raw_data_offset` go (callers use the
  `rec_` functions). The loader statics `raw_map_bytes`, `count_text_words` and `read_header` stay: they parse the
  file container that `dungeon.c` owns.
- [ ] **Step 6: Add the includes** with `add_includes.py` (text in Task 48, Step 6) and a map of every new `rec_`
  name to `engine/record.h`; `make format`.
- [ ] **Step 7: Build and test.** `make`; `uv run pytest -q tests/standard` (the `record` check passes with no
  `open:` line for epic 5; delete those lines); the test selection of Task 48, Step 7, plus
  `$(find tests -name 'test_automap.py' -o -name 'test_view.py' -o -name 'test_control.py' -o -name 'test_golden_protocol.py')`.
  Expected: all pass, golden snapshots unchanged.
- [ ] **Step 8: Commit.** `git commit -am "refactor: the record module owns the square, map and header bytes"`

---

#### Stream E2: split of game.h

Shared files: `src/engine/game.h`, `src/engine/game.c` (only this stream changes them), `src/engine/champion.h`,
`src/engine/group.h`, `src/engine/dungeon.h`, `src/engine/inventory.c`, `src/engine/inventory.h`,
`src/engine/savegame.c`, `src/engine/map.h`, `src/engine/map.c`, `src/engine/magic.h`, every file that includes
`engine/game.h` (28 files): include lines only. E1 and E3 change other lines of these files.

Split map of `engine/game.h` (`port/src/engine/game.h`, line, target; names keep their spelling unless the target
column gives a new one):

| Declaration | Line | Target |
|---|---|---|
| `NORMALIZE` | 9 | `engine/map.h` |
| `CELL_COUNT` | 10 | `engine/map.h` |
| `CHAMPION_MAX_COUNT`, `STAT_LIMIT`, `STAT_MINIMUM_VALUE` | 11-13 | `engine/champion.h` |
| `PARTY_SCENT_COUNT` | 14 | `engine/party.h` |
| `ACTIVE_GROUP_MAX_COUNT`, `CSB_ACTIVE_GROUP_MAX_COUNT` | 15-16 | `engine/group.h` |
| `WATCHDOG_INTERVAL` | 17 | `engine/timeline.c` (private) |
| `SHIFT_COUNT_MASK`, `LONG_SHIFT_MAXIMUM` | 18-19 | `engine/arith.h` |
| `turn_right`, `turn_back`, `turn_left` | 22-33 | `engine/map.h` as `direction_right`, `direction_back`, `direction_left` |
| `STAT_*` blocks, `STAT_MANA` | 35-46 | `engine/champion.h` |
| `SLOT_*` block | 48-66 | `engine/champion.h` |
| `CHAMPION_NAME_SIZE`, `CHAMPION_TITLE_SIZE`, `Skill`, `Champion` | 67-99 | `engine/champion.h` (struct unchanged) |
| `MAGIC_MAP_*` | 103 | `engine/magic.h` |
| `POSITION_*`, `position_pack`, `position_x`, `position_y` | 105-119 | `engine/party.h` as `party_pack_position`, `party_position_x`, `party_position_y` |
| `game_set_magic_map_position`, `game_get_magic_map_position` | 122-134 | `engine/magic.h` as `magic_set_map_position`, `magic_get_map_position` |
| `Party` | 136-144 | `engine/party.h` (struct unchanged) |
| `TimelineEvent` | 146-152 | `engine/timeline.h` (struct unchanged) |
| `ActiveGroup` | 154-160 | `engine/group.h` (struct unchanged) |
| `g_party` | 162 | `engine/party.h` |
| `g_champions`, `g_party_champion_count`, `g_leader_index`, `g_magic_caster_champion_index` | 163-165 | `engine/champion.h` |
| `g_leader_hand_object` | 166 | `engine/inventory.h` |
| `g_party_is_sleeping`, `g_party_dead` | 167 | `engine/game.h` (stay; R7.1 makes them `g_session` fields) |
| `g_game_time`, `g_game_id` | 168 | `engine/game.h` |
| `g_last_creature_attack_time` | 169 | `engine/group.h` |
| `g_last_party_movement_time` | 169 | `engine/party.h` |
| `g_watchdog_time` | 169 | `Timeline.watchdog_time` (`engine/timeline.h`) |
| `g_disabled_movement_ticks`, `g_projectile_disabled_movement_ticks`, `g_last_projectile_disabled_movement_direction` | 170-171 | `engine/party.h` |
| `g_timeline_events`, `g_timeline`, `g_timeline_count`, `g_event_high_water`, `g_first_unused_event_index` | 173-176 | `Timeline g_timeline` fields `events`, `heap`, `count`, `high_water`, `first_unused` |
| `g_active_groups`, `g_active_group_count`, `g_max_active_group_count` | 178-179 | `engine/group.h` |
| `g_redraw`, `REDRAW_*` | 182-183 | `engine/game.h` |
| `GAME_MODE_*`, `g_game_mode`, `g_cheat_*`, `CHEAT_*` | 186-192 | `engine/game.h` |
| `game_start_new` ... `game_apply_tick_effects` | 194-201 | `engine/game.h` |
| `min16`, `max16`, `bounded_value`, `scale_value` | 203-206 | `engine/arith.h` |
| `game_update_map_coordinates_after_relative_movement` | 207-208 | `engine/map.h` as `map_step` |
| `timeline_*` | 210-220 | `engine/timeline.h` |
| `game_init_active_groups` | 222 | `engine/group.h` as `group_init_active_groups` |
| `game_get_stamina_adjusted_value` | 224 | `engine/champion.h` as `champion_adjust_by_stamina` |
| `game_get_statistic_adjusted_attack` | 225 | `engine/champion.h` as `champion_scale_attack` |
| `game_adjust_statistic_current_value` | 226 | `engine/champion.h` as `champion_adjust_statistic` |
| `game_is_lucky` | 227 | `engine/champion.h` as `champion_is_lucky` |
| (from `dungeon.h:22`) `g_event_max_count` | | `Timeline.capacity` |
| (from `dungeon.h:23`) `g_party_map_x`, `g_party_map_y`, `g_party_direction`, `g_party_map_index` | | `engine/party.h` |
| (from `timeline_process.h:121`) `g_game_won` | | `engine/game.h` |
| (from `group.h:213`) `g_dungeon_view_palette_index` | | `engine/party.h` as `g_party_light_level` |
| (from `inventory.h:68-71`) `inventory_get_party_light`, `inventory_set_dungeon_view_palette`, `inventory_decrease_torches_light_power` | | `engine/party.h` as `party_get_light`, `party_set_light_level`, `party_burn_torches` |
| (from `champion.h:114`) `champion_delete_scent` | | `engine/party.h` as `party_delete_scent` |
| (from `move.h:35`) `move_add_scent_strength` | | `engine/party.h` as `party_add_scent_strength` |

Each task of this stream: moves its rows, adds the includes with `add_includes.py`, deletes the include lines of
`engine/game.h` that the standard `include` check reports for the files it changed, renames the old names in
`tests/`, `tools/`, `docs/` (not `docs/superpowers/`) and in the ctypes structures of `tests/dmtest/game.py`, and
updates `docs/reference/kaos-symbols.csv` for each renamed function.

### Task 50: arith module

**Files:**
- Create: `src/engine/arith.h`, `src/engine/arith.c`
- Modify: `src/engine/game.h:18-19, 203-206`, `src/engine/game.c:151-172`, `src/engine/group.c:97-100` (static
  `abs16` goes), every user of the four functions (28 + 33 + 13 + 6 uses)

**Interfaces:**
- Produces: `engine/arith.h`, `/* Prefixes: min16 max16 abs16 bounded_ scale_ */`:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_ARITH_H
#define DM_ENGINE_ARITH_H
/* Prefixes: min16 max16 abs16 bounded_ scale_ */

/* The 16-bit arithmetic of the original rules: minimum, maximum, bounds, and the scaled product of the 68000. */

#include <stdint.h>

/* The 68000 takes a shift count modulo 64. */
#define SHIFT_COUNT_MASK   0x3f
/* A larger shift of a 32-bit value leaves only the sign or 0. */
#define LONG_SHIFT_MAXIMUM 31

inline int16_t min16(int16_t a, int16_t b)
{
    return b <= a ? b : a;
}

inline int16_t max16(int16_t a, int16_t b)
{
    return a <= b ? b : a;
}

inline int16_t abs16(int16_t value)
{
    return value < 0 ? -value : value;
}

/* value limited to min..max; min wins when min > max. */
inline int16_t bounded_value(int16_t min, int16_t value, int16_t max)
{
    return value <= min ? min : value > max ? max : value;
}

/* a * b >> shift. Like the original asr.l of the 32-bit mulu product, the sign bit is the
   product's bit 31 and the shift count is taken modulo 64. */
inline int32_t scale_value(uint16_t a, int16_t shift, uint16_t b)
{
    int32_t product = (int32_t)((uint32_t)a * b); /* unsigned multiply: no signed overflow */
    shift &= SHIFT_COUNT_MASK;
    return shift > LONG_SHIFT_MAXIMUM ? (product < 0 ? -1 : 0) : product >> shift;
}

#endif
```

`arith.c` holds the one external definition of each, so the library exports the four functions that
`tests/test_game.py:129-134` compares with the original:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/arith.h"

extern inline int16_t min16(int16_t a, int16_t b);
extern inline int16_t max16(int16_t a, int16_t b);
extern inline int16_t abs16(int16_t value);
extern inline int16_t bounded_value(int16_t min, int16_t value, int16_t max);
extern inline int32_t scale_value(uint16_t a, int16_t shift, uint16_t b);
```

This is C11 `inline` with one external definition, not `static inline` as the PRD (R5.2) writes: a `static
inline` function has no symbol in `libdm.dylib`, and the differential test would lose its target. The
`bounded_value` comment: Assumption: `value <= min` is tested first, so min wins; check it against
`game.c:160-163` before you write the comment.

- [ ] **Step 1:** Write `arith.h` and `arith.c` as above. Delete `game.h:18-19, 203-206`, `game.c:151-172` and the
  static `abs16` of `group.c:97-100`.
- [ ] **Step 2:** `add_includes.py` (text in Task 48, Step 6) with
  `{"engine/arith.h": ["min16", "max16", "abs16", "bounded_value", "scale_value", "SHIFT_COUNT_MASK", "LONG_SHIFT_MAXIMUM"]}`; `make format`.
- [ ] **Step 3:** `make`; `nm -gU build/libdm.dylib | grep -E "_(min16|max16|abs16|bounded_value|scale_value)$"`.
  Expected: five lines.
- [ ] **Step 4:** `uv run pytest -q tests/standard`; delete the unused `engine/game.h` includes it reports in the
  changed files; `uv run pytest -q $(find tests -name 'test_game.py')`. Expected: pass.
- [ ] **Step 5:** `git commit -am "refactor: the 16-bit helpers become the arith module"`

### Task 51: one game_fatal

**Files:**
- Modify: `src/engine/game.h`, `src/engine/game.c:55-60` (static `fatal_error` becomes public `game_fatal`),
  `src/engine/map.c:157-161` (static `stop_fatal` goes; caller `map.c:185`), `src/engine/group.c:307-308` (the
  inline `fprintf` and `exit`)

**Interfaces:**
- Produces: `engine/game.h`: `/* Stops the game with the "System Error" message of the original. */
  _Noreturn void game_fatal(int16_t code);`

- [ ] **Step 1:** Rename `fatal_error` to `game_fatal`, make it public with `_Noreturn`, move its comment to the
  prototype in `game.h`. Its body calls `os_fatal("System Error %d", code)` of `data/os.h` (Task 27, package D) instead of
  `fprintf(stderr, ...)` and `exit(1)`, with the same message text as `port/src/engine/game.c:58`; the
  `platform` lines of `engine/game.c` go from the allowlist (standard 10.1).
- [ ] **Step 2:** `map.c:185` and `group.c:307-308` call `game_fatal(code)`; delete `stop_fatal`. `map.c` and
  `group.c` lose their `exit` and the `fprintf` of the system error; the `report_free_record` log of
  `map.c:164` calls `os_log` (Task 27).
- [ ] **Step 3:** `make`; `uv run pytest -q tests/standard` (the `platform` lines of `engine/map.c` and
  `engine/group.c` for `exit(` go if no other `exit(` remains in the file); `uv run pytest -q $(find tests -name 'test_game.py')`.
- [ ] **Step 4:** `git commit -am "refactor: one game_fatal for the system errors"`

### Task 52: timeline module

**Files:**
- Create: `src/engine/timeline.h`, `src/engine/timeline.c`
- Modify: `src/engine/game.h:146-152, 173-176, 210-220`, `src/engine/game.c:47-50, 183-378`,
  `src/engine/dungeon.h:22`, `src/engine/dungeon.c:16, 209, 227` (`g_event_max_count`), `src/engine/savegame.c:472-490`
  (`allocate_tables`), every user of the moved globals (`g_timeline_events` 37, `g_timeline` 22,
  `g_timeline_count` 14, `g_event_high_water` 14, `g_first_unused_event_index` 11, `g_event_max_count` 21 uses in
  `src/`; 32 lines in `tests/`), `tests/dmtest/game.py` (a `Timeline` structure)

**Interfaces:**
- Produces (`engine/timeline.h`, `/* Prefixes: timeline_ */`, purpose: "The timeline: the event slots and the
  min-heap that orders them by time. It stores events; `engine/event.h` runs them."):

```c
typedef struct {
    uint32_t map_time; /* map bits 24-31, time bits 0-23; free slot: next free index */
    uint8_t type;      /* 0: free slot */
    uint8_t priority;  /* champion events: champion index */
    uint16_t b;        /* square events: x << 8 | y */
    uint16_t c;        /* square events: cell << 8 | effect */
} TimelineEvent;

typedef struct {
    TimelineEvent *events; /* capacity slots, owned */
    uint16_t *heap;        /* min-heap of slot indices, owned */
    int16_t capacity;      /* was g_event_max_count: the dungeon loader and the save set it */
    uint16_t count;        /* events in the heap */
    uint16_t high_water;   /* slots below this index were used */
    int16_t first_unused;  /* first free slot, -1 none */
    int32_t watchdog_time; /* game time of the next watchdog event */
} Timeline;

extern Timeline g_timeline;

/* Allocates capacity free event slots and an empty heap; false when the allocation fails. */
bool timeline_allocate(int16_t capacity);
```

  The other functions keep their names and signatures (`game.h:210-220`): `timeline_init` (calls
  `timeline_allocate(g_timeline.capacity)` and `game_fatal(SYSTEM_ERROR_MEMORY)` on false), `timeline_reset`,
  `timeline_is_event_a_before_b`, `timeline_get_index`, `timeline_fix_chronology`, `timeline_delete_event`,
  `timeline_find_mergeable_event`, `timeline_add_event`, `timeline_extract_first_event`,
  `timeline_is_first_event_expired`, `timeline_add_watchdog_event`.
- Consumes: `game_fatal` (Task 51).

- [ ] **Step 1:** Write `timeline.h` and move the bodies of `game.c:183-378` to `timeline.c` with their constants
  (`game.c:15-17, 25`: `SYSTEM_ERROR_TIMELINE_FULL`, `SYSTEM_ERROR_MEMORY` stays in `game.c` too if `game.c` still
  uses it, else it moves; `FREE_LIST_END`, `EVENT_KEY_TYPE_SHIFT`) and the macros `BEFORE`, `IS_SQUARE_EVENT`,
  `SAME_PLACE`, `OPPOSITE_EFFECT` (`game.c:247, 294-297`). `timeline.c` includes `engine/timeline_process.h` (after
  Task 53: `engine/event.h`) for the event types and the event word accessors.
- [ ] **Step 2:** `timeline_allocate(capacity)` holds the shared part of `timeline_init` (`game.c:186-194`) and
  `allocate_tables` (`savegame.c:478-486`): free both arrays, `calloc` both, fill the free list, return false on a
  failed allocation or `capacity <= 0`. `allocate_tables` calls it and keeps its group part (Task 55 changes that
  part).
- [ ] **Step 3:** Replace the globals by `g_timeline` fields in `src/` (script `rename.py` of Task 48 cannot map a
  name to a field access with a different base; use `sed -E -i '' 's/\bg_timeline_events\b/g_timeline.events/g; s/\bg_timeline_count\b/g_timeline.count/g; s/\bg_event_high_water\b/g_timeline.high_water/g; s/\bg_first_unused_event_index\b/g_timeline.first_unused/g; s/\bg_event_max_count\b/g_timeline.capacity/g; s/\bg_watchdog_time\b/g_timeline.watchdog_time/g' $(grep -rlE "g_timeline_events|g_timeline_count|g_event_high_water|g_first_unused_event_index|g_event_max_count|g_watchdog_time" src)`
  first, then `sed -E -i '' 's/\bg_timeline\[/g_timeline.heap[/g'` over the same files). Check
  `grep -rn "g_timeline\b[^.]" src`: expected none.
- [ ] **Step 4:** Tests: add `Timeline` to `tests/dmtest/game.py` as a `ctypes.Structure` with the field order and
  types above, and replace each `in_dll(lib, "g_timeline_events")` (and the other five names) by
  `Timeline.in_dll(lib, "g_timeline").<field>`; `grep -rnE "g_timeline_events|g_timeline_count|g_event_high_water|g_first_unused_event_index|g_event_max_count|g_watchdog_time" tests tools docs --exclude-dir=superpowers`:
  expected none after the change (the docs name the fields).
- [ ] **Step 5:** `add_includes.py` with `{"engine/timeline.h": ["TimelineEvent", "Timeline", "g_timeline", "timeline_init", "timeline_reset", "timeline_add_event", "timeline_delete_event", "timeline_extract_first_event", "timeline_is_first_event_expired", "timeline_add_watchdog_event", "timeline_get_index", "timeline_fix_chronology", "timeline_find_mergeable_event", "timeline_is_event_a_before_b", "timeline_allocate"]}`; `make format`.
- [ ] **Step 6:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_game.py' -o -name 'test_save.py' -o -name 'test_csb_save.py' -o -name 'test_golden_save.py' -o -name 'test_creatures.py' -o -name 'test_move.py')`.
  Expected: pass; `tests/golden/save.json` unchanged (`TimelineEvent` keeps its layout).
- [ ] **Step 7:** `git commit -am "refactor: the timeline module owns the event slots and the heap"`

### Task 53: event module (was timeline_process)

**Files:**
- Rename: `git mv src/engine/timeline_process.c src/engine/event.c`, `git mv src/engine/timeline_process.h src/engine/event.h`
- Modify: `src/engine/event.h` (guard `DM_ENGINE_EVENT_H`; `g_game_won` at 121 goes to `game.h`; the 14 handler
  prototypes of 126-140 go except `event_process_expired`, `event_is_door_destroyed_by_attack`,
  `event_process_wall`), `src/engine/event.c`, `src/engine/game.h`, every `#include "engine/timeline_process.h"`
  (grep), `tests/standard/allowlist.txt` (the line `long engine/timeline_process.c:event_process_expired`, today
  `tests/standard_allowlist.txt:11`, names `engine/event.c:event_process_expired`), `docs/` paths that name
  `timeline_process`

**Interfaces:**
- Produces (`engine/event.h`, `/* Prefixes: event_ */`): the event types, the event word accessors
  (`timeline_process.h:74-119`), `void event_process_expired(void)`,
  `bool event_is_door_destroyed_by_attack(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, uint16_t ticks)`
  (callers `projectile.c`, `action.c`), `void event_process_wall(TimelineEvent *event); /* exported for the tests */`
  (`tests/test_csb_ending.py` calls it), `extern void (*g_endgame_sensor_hook)(uint16_t delay)`.
  `engine/game.h`: `extern bool g_game_won;`.

Callers outside `timeline_process.c` (grep at `72e24b5`): none for `event_process_door_animation`,
`_door_destruction`, `_corridor`, `_fake_wall`, `_teleporter`, `_pit`, `_door`, `event_enable_champion_action`,
`event_hide_damage_received`, `event_process_vi_altar_rebirth`, `event_enable_group_generator`,
`event_process_light`, `event_move_teleporter_or_pit_square_things`; one test for `event_process_wall`.

- [ ] **Step 1:** `git mv` both files; fix the guard and the include lines (`grep -rl 'timeline_process.h' src | xargs sed -i '' 's#engine/timeline_process.h#engine/event.h#'`).
- [ ] **Step 2:** Make the 13 handlers without an outside caller `static`; delete their prototypes; add the test
  marker to `event_process_wall`. A static handler that is called before its definition: move the definition above
  the first caller (standard 8.3), no forward declaration. A static function name with the module prefix keeps it
  for now: the standard check `static-prefix` lists it as `open: epic 7` (the dead-API task of package F renames
  the statics).
- [ ] **Step 3:** Move `g_game_won` to `game.h` (definition in `game.c`); `add_includes.py` with
  `{"engine/game.h": ["g_game_won"]}`.
- [ ] **Step 4:** Update the allowlist line and `grep -rn "timeline_process" src tests docs --exclude-dir=superpowers`:
  replace each path by `engine/event.c`/`engine/event.h`.
- [ ] **Step 5:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_csb_ending.py' -o -name 'test_move.py' -o -name 'test_creatures.py' -o -name 'test_game.py')`.
- [ ] **Step 6:** `git commit -am "refactor: timeline_process becomes the event module with static handlers"`

### Task 54: party module and the active groups

**Files:**
- Create: `src/engine/party.h`, `src/engine/party.c`
- Modify: `src/engine/game.h:14, 105-119, 136-144, 162, 169-171`, `src/engine/game.c`, `src/engine/dungeon.h:23`,
  `src/engine/dungeon.c:18`, `src/engine/inventory.h:67-71`, `src/engine/inventory.c:544-606`,
  `src/engine/group.h:213` and `group.c:81` (`g_dungeon_view_palette_index`), `src/engine/champion.h:114`,
  `src/engine/champion.c:979-991` (`champion_delete_scent`), `src/engine/move.h:34-35`, `src/engine/move.c:63-83`
  (`move_add_scent_strength`); for the active groups: `src/engine/group.h`, `src/engine/group.c`,
  `src/engine/game.h:15-16, 154-160, 178-179, 222`, `game.c:52-53, 380-390`, `src/engine/savegame.c:472-490`;
  every user; tests (`g_dungeon_view_palette_index` 8 lines, the light functions 7 lines)

**Interfaces:**
- Produces (`engine/party.h`, `/* Prefixes: party_ */`, purpose "The party as one body: its position, its
  scents, its light, and its movement delays."):
  `PARTY_SCENT_COUNT`, `POSITION_*` (as `PARTY_POSITION_*`), `Party` (byte-identical), `extern Party g_party;`,
  `extern int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;`,
  `extern int32_t g_last_party_movement_time;`,
  `extern int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks, g_last_projectile_disabled_movement_direction;`,
  `extern int16_t g_party_light_level; /* 0 (bright) to 5; the view step sets it */`,
  `uint16_t party_pack_position(int16_t map, int16_t x, int16_t y)`, `int16_t party_position_x(uint16_t word)`,
  `int16_t party_position_y(uint16_t word)`, `int16_t party_get_light(void)`,
  `void party_set_light_level(int16_t map_index, int16_t light)`, `void party_burn_torches(void)`,
  `void party_delete_scent(uint16_t scent_index)`, `void party_add_scent_strength(int16_t x, int16_t y, int16_t strength_and_flags)`.
- Produces (`engine/group.h`): `ActiveGroup` (byte-identical), `ACTIVE_GROUP_MAX_COUNT`,
  `CSB_ACTIVE_GROUP_MAX_COUNT`, `g_active_groups`, `g_active_group_count`, `g_max_active_group_count`,
  `g_last_creature_attack_time`, `void group_init_active_groups(void)` (was `game_init_active_groups`),
  `bool group_allocate_active(uint16_t count)` (the shared part of `game.c:384-389` and `savegame.c:476-489`:
  free, `malloc`, fill `group_index = -1`; false on a failed allocation).

- [ ] **Step 1:** Write `party.h`/`party.c` with the rows above; the bodies of the three light functions
  (`inventory.c:546-606`) and their constants move to `party.c` (`LIGHT_SOURCE_COUNT`, `LIGHT_SOURCES_USED`,
  `LIGHT_FIRST_SHIFT`, `LIGHT_DARKEST`: grep their definitions in `inventory.c`); `champion_delete_scent` and
  `move_add_scent_strength` move with their constants. The static scent helpers `fade_scents` (`champion.c:993`)
  and `add_party_scent` (`move.c:85`) stay with their callers.
- [ ] **Step 2:** `rename.py` (Task 48) with `g_dungeon_view_palette_index g_party_light_level`,
  `inventory_get_party_light party_get_light`, `inventory_set_dungeon_view_palette party_set_light_level`,
  `inventory_decrease_torches_light_power party_burn_torches`, `champion_delete_scent party_delete_scent`,
  `move_add_scent_strength party_add_scent_strength`, `position_pack party_pack_position`,
  `position_x party_position_x`, `position_y party_position_y`, `POSITION_X_SHIFT PARTY_POSITION_X_SHIFT`,
  `POSITION_Y_SHIFT PARTY_POSITION_Y_SHIFT`, `POSITION_XY_MASK PARTY_POSITION_XY_MASK`,
  `POSITION_MAP_MASK PARTY_POSITION_MAP_MASK`, `game_init_active_groups group_init_active_groups`.
- [ ] **Step 3:** Move `ActiveGroup`, the active group globals and `group_init_active_groups` to `group.h` /
  `group.c`; write `group_allocate_active`; `group_init_active_groups` calls it and `game_fatal(SYSTEM_ERROR_MEMORY)`
  on false; `allocate_tables` (`savegame.c:472`) calls it with its count (`savegame.c:474-476` keeps the DM minimum).
  Move `g_last_creature_attack_time` to `group.h`.
- [ ] **Step 4:** `add_includes.py` with every moved name mapped to its header; `make format`.
- [ ] **Step 5:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_champion.py' -o -name 'test_move.py' -o -name 'test_creatures.py' -o -name 'test_save.py' -o -name 'test_golden_save.py' -o -name 'test_golden_view.py' -o -name 'test_view.py')`.
- [ ] **Step 6:** `git commit -am "refactor: the party module owns position, scents and light"`

### Task 55: champion header takes the champion struct and the statistic rules

**Files:**
- Modify: `src/engine/champion.h` (drops `#include "engine/game.h"`), `src/engine/champion.c`,
  `src/engine/game.h:11-13, 35-99, 163-165, 224-227`, `src/engine/game.c:32-37, 392-435`, `src/engine/magic.h`,
  `src/engine/magic.c` (MAGIC_MAP rows), `src/engine/inventory.h` (`g_leader_hand_object`), every user; tests and
  `docs/reference/kaos-symbols.csv` for the four renamed functions

**Interfaces:**
- Produces: `engine/champion.h`: `CHAMPION_MAX_COUNT`, `STAT_*`, `SLOT_*`, `Skill`, `Champion` (byte-identical
  definition), `g_champions`, `g_party_champion_count`, `g_leader_index`, `g_magic_caster_champion_index`,
  `int16_t champion_adjust_by_stamina(const Champion *champion, int16_t value)`,
  `uint16_t champion_scale_attack(const Champion *champion, uint16_t statistic, uint16_t attack)`,
  `void champion_adjust_statistic(Champion *champion, uint16_t statistic, int16_t delta)`,
  `bool champion_is_lucky(Champion *champion, uint16_t percentage)` (package F changes them to take an index,
  R6.1). `engine/magic.h`: `MAGIC_MAP_*`, `magic_set_map_position(uint8_t *spells, int16_t map, int16_t x, int16_t y)`,
  `magic_get_map_position(const uint8_t *spells, int16_t *map, int16_t *x, int16_t *y)` (package F, R6.4, makes
  the getter return a `Position`). `engine/inventory.h`: `g_leader_hand_object`.
- Consumes: `party_pack_position`, `party_position_x`, `party_position_y` (Task 54) for the magic map helpers.

- [ ] **Step 1:** Move the rows of the split map to `champion.h`, `magic.h`, `inventory.h`; definitions of the
  globals to `champion.c` and `inventory.c`; the four statistic functions (`game.c:392-435`) with their constants
  (`game.c:18-24`: `STAT_HIGH`, `STAT_VERY_HIGH`, `ATTACK_FACTOR_MIN`, `ATTACK_FACTOR_SHIFT`, `LUCK_STEP`,
  `PERCENT_RANGE`, `ATTACK_LOW_FACTOR_SHIFT`) to `champion.c`.
- [ ] **Step 2:** `rename.py` (Task 48) with `game_get_stamina_adjusted_value champion_adjust_by_stamina`,
  `game_get_statistic_adjusted_attack champion_scale_attack`, `game_adjust_statistic_current_value champion_adjust_statistic`,
  `game_is_lucky champion_is_lucky`, `game_set_magic_map_position magic_set_map_position`,
  `game_get_magic_map_position magic_get_map_position`.
- [ ] **Step 3:** `champion.h` includes `<stdbool.h>`, `<stdint.h>`, `data/tables.h` (for `SKILL_COUNT`) and no
  `engine/game.h`. `add_includes.py` with every moved name mapped to its new header; `make format`.
- [ ] **Step 4:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_champion.py' -o -name 'test_game.py' -o -name 'test_golden_save.py' -o -name 'test_csb_magic_map.py' -o -name 'test_actions.py')`.
  Expected: pass; `Champion` offsets unchanged (the golden save proves the packed form).
- [ ] **Step 5:** `git commit -am "refactor: champion.h owns the champion struct and the statistic rules"`

### Task 56: direction helpers, map_step, and the rest of game.h

**Files:**
- Modify: `src/engine/map.h`, `src/engine/map.c`, `src/engine/game.h` (only the session remains),
  `src/engine/game.c:174-181` (`game_update_map_coordinates_after_relative_movement` goes),
  the three wrappers `group.c:111-114` (`step`), `action.c:187-193` (`step_forward`), `champion.c:198-203`
  (`get_square_in_front`), every caller of `turn_right` (25), `turn_back` (19), `turn_left` (11),
  `game_update_map_coordinates_after_relative_movement` (20)

**Interfaces:**
- Produces (`engine/map.h`, prefix line gains `direction_`): `NORMALIZE`, `CELL_COUNT`,
  `static inline int16_t direction_right(int16_t direction)`, `direction_back`, `direction_left` (bodies of
  `game.h:22-33`), and

```c
/* Moves x and y by steps forward and to the right of a direction. */
void map_step(int16_t direction, int16_t forward, int16_t right, int16_t *x, int16_t *y);
```

  The body of `game.c:175-181` with `direction_right`. `engine/game.h` after this task:
  `/* Prefixes: game_ */`, purpose "The game session: the clock, the run flags, the modes and cheats, and the redraw
  flags that the rules raise.", and only the session rows of the split map plus `game_fatal`.

- [ ] **Step 1:** Move `NORMALIZE`, `CELL_COUNT` and the three turn helpers to `map.h`; `rename.py` (Task 48) with
  `turn_right direction_right`, `turn_back direction_back`, `turn_left direction_left`,
  `game_update_map_coordinates_after_relative_movement map_step` over `src/` only (the tests use `turn_left` and
  `turn_right` as protocol action names: `tests/test_control.py:175, 184`; do not run the rename over `tests/`).
- [ ] **Step 2:** Replace the wrappers: `step(d, n, &x, &y)` → `map_step(d, n, 0, &x, &y)`; `step_forward(d, &x, &y)`
  → `x = g_party_map_x; y = g_party_map_y; map_step(d, 1, 0, &x, &y)`; `get_square_in_front(&x, &y)` → the same with
  `g_party_direction`. Delete the three statics.
- [ ] **Step 3:** `game.h` keeps the session rows; its includes are `<stdbool.h>`, `<stdint.h>` only (no
  `data/bytes.h`, no `data/tables.h`). Run `add_includes.py` with every name of the split map mapped to its new
  header, then `uv run pytest -q tests/standard -k include` and delete every unused `engine/game.h` include it
  reports. `make format`.
- [ ] **Step 4:** `grep -c "" src/engine/game.h`: expected below 60 lines. `grep -n "Prefixes" src/engine/game.h`:
  expected `/* Prefixes: game_ */`.
- [ ] **Step 5:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_move.py' -o -name 'test_creatures.py' -o -name 'test_actions.py' -o -name 'test_champion.py' -o -name 'test_golden_rules.py' -o -name 'test_golden_view.py')`.
- [ ] **Step 6:** `git commit -am "refactor: direction helpers and map_step move to map.h; game.h is the session"`

---

#### Stream E3: copied helpers, savefile, explored, message

Shared files: `src/engine/map.h`, `src/engine/map.c`, `src/engine/dungeon.c` (`map_at_level` 289-301,
`validate_squares` 342-345, `dungeon_get_square_first_thing` 467-480), `src/engine/group.c`,
`src/engine/projectile.c`, `src/engine/timeline_process.c` (E2 renames it), `src/engine/move.c`,
`src/engine/sensor.c`, `src/engine/action.c`, `src/engine/magic.c`, `src/engine/champion.c`,
`src/engine/recruit.c`, `src/engine/savegame.c`, `src/ui/view.c`, `src/ui/advanced.c`, `src/ui/startend.c`,
`src/ui/gameloop.c`, `src/ui/dialog.c`, `state_json.c`.

### Task 57: map helpers: party square, things of a type, the front cell, any map

**Files:**
- Modify: `src/engine/map.h`, `src/engine/map.c`, `src/engine/dungeon.h:35`, `src/engine/dungeon.c:289-301, 342-345,
  466-480`; callers: `is_party_on` in `group.c:101`, `projectile.c:50`, `timeline_process.c:33`, `move.c:102`; the
  side-of-cell formula in `group.c:1021`, `group.c:2141`, `move.c:179`, `projectile.c:859`; the first-thing loops
  `map.c:244-250`, `champion.c:457-459`; `dungeon_get_square_first_thing` in `recruit.c:101`, `ui/view.c:367, 2567`,
  `state_json.c:670, 831, 925, 964`; `tests/test_csb_text.py:68, 123` (the test file of package C)

**Interfaces:**
- Produces (`engine/map.h`):

```c
/* A square of the current map. */
typedef struct {
    int16_t x, y;
} Location;

/* A square with its map. */
typedef struct {
    int16_t map, x, y;
} Position;

/* The party stands on the square of the current map. */
bool map_is_party_on(Location at);
/* The first thing of a type on a square of the current map, or THING_ENDOFLIST. */
uint16_t map_find_thing(Location at, int16_t type);
/* The first thing on a square of any map, or THING_ENDOFLIST. */
uint16_t map_get_first_thing_at(Position at);
/* The map one level up or down that covers the dungeon position of (x, y) on map, or -1. */
int16_t map_find_level_map(int16_t map, int16_t level_delta, int16_t x, int16_t y);
/* Of the two cells on the side of a direction, the one beside cell. */
static inline int16_t direction_front_cell(int16_t direction, int16_t cell)
{
    return NORMALIZE(direction + (((cell - direction + 1) & 2) >> 1));
}
```

  `map_get_square_group(x, y)` becomes `map_find_thing((Location){x, y}, THING_GROUP)` at its callers and goes;
  `dungeon_get_square_first_thing` and the static `map_at_level` go.
- Consumes: `NORMALIZE` in `map.h` (Task 56 moves it; until the merge it is in `game.h`, which `map.c` includes):
  this task uses `NORMALIZE` only in `map.h`; if `map.h` does not see it, put `direction_front_cell` in `map.c`
  as a public function instead of an inline.

Facts: the four `is_party_on` copies are equal (`group.c:101-104`, `projectile.c:50-53`, `timeline_process.c:33-36`,
`move.c:102-105`). The side-of-cell formula at `group.c:1021` uses `cells + 5 - primary` and `NORMALIZE`; the others
use `cell - direction + 1`; `(v + 4) & 2 == v & 2` for every `int16_t` v, so `direction_front_cell` gives the same
cell; `move.c:179` and `group.c:2141` pass the value to `THING_WITH_CELL`, which masks it with 3 (`map.h:63`).
`map_at_level` (`dungeon.c:290-301`) is the search of `map_get_location_after_level_change` (`map.c:86-103`)
without its guard `g_party_map_index == NO_DUNGEON_MAP` (`map.c:88`); `dungeon_validate` runs before a party map
exists, so `map_find_level_map` has no guard and `map_get_location_after_level_change` keeps it.
`dungeon_get_square_first_thing` (`dungeon.c:467-480`) returns `THING_ENDOFLIST` when the index reaches the table
size; `map_get_square_first_thing` (`map.c:132-136`) does not check. Assumption: a validated dungeon never reaches
that bound (`validate_squares` checks it, `dungeon.c:341-343`); the merged function keeps the check, and the golden
and differential tests confirm the same result.

- [ ] **Step 1:** Write the types and functions in `map.h`/`map.c`. `map_get_first_thing_at` takes the body of
  `dungeon_get_square_first_thing` (with `rec_square_byte` after Task 49, or `g_dungeon_map_data` until the merge);
  `map_get_square_first_thing(x, y)` calls `map_get_first_thing_at((Position){g_current_map_index, x, y})`.
  Note: `map_get_square_first_thing_index` stays: `map_link_thing_to_list` and `map_unlink_thing_from_list` need the
  index.
- [ ] **Step 2:** `map_find_level_map` takes the loop of `map.c:90-102` without the coordinate writes;
  `map_get_location_after_level_change` calls it, then writes `*x`, `*y`. `dungeon.c:342-345` call
  `map_find_level_map`; delete `map_at_level`.
- [ ] **Step 3:** Delete the four `is_party_on` copies; callers write `map_is_party_on((Location){x, y})`.
- [ ] **Step 4:** Replace the four side-of-cell formulas by `direction_front_cell(direction, cell)`.
- [ ] **Step 5:** `map.c:244-250` and `champion.c:457-459` use `map_find_thing`. The other loops of the survey
  (`sensor.c:81`, `projectile.c:569, 751`) test more than the type and stay. `recruit.c:101`, `view.c`, `state_json.c`
  and the test call `map_get_first_thing_at`.
- [ ] **Step 6:** `make format`; `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_dungeon.py' -o -name 'test_move.py' -o -name 'test_creatures.py' -o -name 'test_recruit.py' -o -name 'test_csb_text.py' -o -name 'test_golden_*.py' -o -name 'test_view.py' -o -name 'test_control.py')`.
  Expected: pass, golden snapshots unchanged.
- [ ] **Step 7:** `git commit -am "refactor: one copy of the party-square, front-cell and first-thing helpers"`

### Task 58: champion faces the party direction

**Files:**
- Modify: `src/engine/champion.h`, `src/engine/champion.c`, `src/engine/action.c:195-201` (static
  `champion_face_party_direction`, 5 callers at 335, 366, 497, 617, 636), `src/engine/magic.c:193-196` (inline copy)

**Interfaces:**
- Produces: `engine/champion.h`: `/* The champion turns to the party direction. */ void champion_face_party_direction(Champion *champion);`
  (package F changes the parameter to an index, R6.1).

Facts: `action.c:195-201` and `magic.c:193-196` are equal (direction and `CHAMPION_ATTR_ICON`); the turn of
`champion.c:1125-1129` also clears `maximum_damage_received` and has a time condition: it stays.

- [ ] **Step 1:** Move the body of `action.c:195-201` to `champion.c` as public `champion_face_party_direction`;
  `magic.c:193-196` calls it with `c`.
- [ ] **Step 2:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_actions.py' -o -name 'test_spells.py' -o -name 'test_golden_rules.py')`.
- [ ] **Step 3:** `git commit -am "refactor: one champion_face_party_direction"`

### Task 59: message module: one dungeon text print, the game time, int16_t columns

**Files:**
- Modify: `src/engine/message.h`, `src/engine/message.c`, `src/engine/sensor.h:11` (`SENSOR_TEXT_COLOR`),
  `src/engine/sensor.c:307-312`, `src/engine/timeline_process.c:54-59` (static `print_text` copies), the callers of
  `message_print` (18 lines: `champion.c:192-195`, `magic.c:109-112`, `action.c:224`, `ui/advanced.c:116-117`,
  `ui/startend.c:473`, `ui/gameloop.c:429`), `ui/dialog.c:101` (`message_next_word_line`), the tests
  `test_text.py:106-132`, `test_ui.py:231-232`

**Interfaces:**
- Produces (`engine/message.h`):
  `void message_clear_expired_rows(void)`, `void message_print(int16_t color, const char *s)`,
  `void message_print_number(int16_t color, int32_t value)`, `void message_new_line(void)`,
  `int16_t message_next_word_line(const char *s, char *line, int16_t *pos, int16_t max_width)`,
  `/* Prints the text of a text record in the color of the dungeon texts. */ void message_print_dungeon_text(uint16_t text_thing);`,
  `#define MESSAGE_COLOR_DUNGEON_TEXT 15 /* white */` (was `SENSOR_TEXT_COLOR`, `sensor.h:11`).

Facts: every caller passes `g_game_time` (grep above); only the tests pass other times (`test_text.py:106-113`
passes 100, 110, 169, 170). The two `print_text` copies are equal (`sensor.c:307-312`, `timeline_process.c:54-59`)
and decode with mode 1 = `TEXT_MODE_MESSAGE` (`dungeon.h:38`).

- [ ] **Step 1:** `message.c` reads `g_game_time` (include `engine/game.h`); drop the `game_time` parameters; columns
  and `cursor_x` arithmetic of `message.c:78-109` use `int16_t`; `message_next_word_line` takes `int16_t`.
- [ ] **Step 2:** Write `message_print_dungeon_text` (body of `sensor.c:307-312` with `TEXT_MODE_MESSAGE` and
  `MESSAGE_COLOR_DUNGEON_TEXT`); delete both `print_text` copies and `SENSOR_TEXT_COLOR`.
- [ ] **Step 3:** Callers drop the last argument (`sed -E -i '' 's/(message_(print|print_number|new_line|clear_expired_rows)\(.*), g_game_time\)/\1)/; s/message_(new_line|clear_expired_rows)\(g_game_time\)/message_\1()/' <files>`;
  check each line with `grep -rn "g_game_time)" src | grep message_`: expected none). `ui/dialog.c:101` passes
  `int16_t` variables.
- [ ] **Step 4:** Tests: `test_text.py` sets `ctypes.c_uint32.in_dll(lib, "g_game_time").value = 100` before the first
  print, 110 before the second, 169 and 170 before each `message_clear_expired_rows()`; the other calls drop the
  time argument. The assertions stay.
- [ ] **Step 5:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_text.py' -o -name 'test_ui.py' -o -name 'test_golden_rules.py' -o -name 'test_csb_ending.py')`.
- [ ] **Step 6:** `git commit -am "refactor: message functions read the game time; one dungeon text print"`

### Task 60: savefile names and data ownership

**Files:**
- Modify: `src/engine/savefile.h`, `src/engine/savefile.c` (`H_*` at 10-26), `src/engine/savegame.c` (`P_*`, `E_*`,
  `A_*` at 26-44; `write_save_file` 318-334; `rename` at 583), `src/engine/champion.c:106-126` (`REC_*`), callers
  of the `save_` functions (`savegame.c`), `tests/test_savefile.py`, `tests/test_save.py`, `tests/test_csb_save.py`,
  `tools/csb_match.py` (or `tools/dmtools/csb_match.py` after package C), `docs/formats/save-format.md`,
  `docs/csb/*.md`, `docs/reference/kaos-symbols.csv`, `docs/reference/csb-appb-symbols.csv`, `docs/reference/kaos-map.md`,
  `docs/reference/symbols/range-7-9.md`

**Interfaces:**
- Consumes (package D, R4.6, `data/file.h`): `file_write` and `file_replace`. Assumption: `bool file_write(const char
  *path, const uint8_t *data, size_t size)` writes the file and checks `fclose`; `bool file_replace(const char *path,
  const char *backup)` removes `backup` and renames `path` to `backup`. Step 1 checks the real signatures in
  `src/data/file.h` and uses them.
- Produces (`engine/savefile.h`, `/* Prefixes: savefile_ */`):

| Old (`savefile.h`) | New |
|---|---|
| `save_block_xor` (57) | `savefile_xor_block` |
| `save_decode` (58) | `savefile_decode` |
| `save_encode` (59) | `savefile_encode` |
| `save_free` (60) | `savefile_free` |
| `save_header_init` (63) | `savefile_init_header` |
| `save_header_randomize` (65) | `savefile_randomize_header` |
| `save_header_new_adventure` (67) | `savefile_make_adventure_header` |
| `save_header_game_id` (68) | `savefile_get_game_id` |
| `save_header_dungeon_id` (69) | `savefile_get_dungeon_id` |
| `save_header_extra` (70) | `savefile_get_header_extra` |
| `save_header_extra_size` (72) | `savefile_get_extra_size` |
| `G_*` offsets (16-35) | `SAVE_GLOBAL_*` (`G_GAME_TIME` → `SAVE_GLOBAL_GAME_TIME`, ...) |
| `H_*` (`savefile.c:10-26`) | `SAVE_HEADER_*` (`H_PAYLOAD` → `SAVE_HEADER_PAYLOAD`, ...; `H_KEY_COUNT` → `SAVE_HEADER_KEY_COUNT`) |
| `P_*` (`savegame.c:27-39`) | `SAVE_PARTY_*` |
| `E_*` (`savegame.c:42`) | `SAVE_EVENT_*` |
| `A_*` (`savegame.c:44`) | `SAVE_GROUP_*` |
| `REC_*` (`champion.c:107-126`) | `CHAMPION_RECORD_*` |

  `SaveFile` gets `bool owns_data; /* true: savefile_decode allocated data, savefile_free frees it */` after `data`;
  `savefile_free` frees `data` only when `owns_data` is true. The offset blocks become `#define` blocks in hex with
  one digit width per block (file format bytes, standard 3.4), one comment line above each block.

- [ ] **Step 1:** Check `src/data/file.h` for `file_write` and `file_replace`; write down their signatures.
- [ ] **Step 2:** `rename.py` (text in Task 48, Step 4) with the rows of the table that are whole-name renames and
  the `G_`, `P_`, `E_`, `A_`, `H_`, `REC_` constants listed one by one (generate the list with
  `grep -ohE "\b(G|P|E|A|H)_[A-Z_]+\b" src/engine/savefile.[ch] src/engine/savegame.c | sort -u` and
  `grep -ohE "\bREC_[A-Z_]+\b" src/engine/champion.c | sort -u`; check that no other module uses a name of the list:
  `grep -rlwE "<names>" src | sort -u` lists only these files).
- [ ] **Step 3:** Rewrite the offset `enum` blocks as `#define` blocks (hex, same digit width per block:
  `0x00`, `0x04`, ... `0x2e` for the globals), with the values unchanged.
- [ ] **Step 4:** `owns_data`: `savefile_decode` sets it true; the struct of `savegame.c:253` leaves it false.
- [ ] **Step 5:** `write_save_file` (`savegame.c:318-334`) calls `file_replace(path, backup)` when there is a backup
  name, then `file_write(path, data, size)`; `savegame.c:583` calls `file_replace` or keeps `rename` through
  `data/file.h` as Step 1 found. `savegame.c` has no `fopen`, `rename(`, `remove(` after the step:
  `grep -n "fopen\|rename(\|remove(" src/engine/savegame.c` expected empty.
- [ ] **Step 6:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_savefile.py' -o -name 'test_save.py' -o -name 'test_csb_save.py' -o -name 'test_golden_save.py' -o -name 'test_csb_tools.py')`.
  Expected: pass, `tests/golden/save.json` unchanged.
- [ ] **Step 7:** `git commit -am "refactor: savefile prefix, save offset names and data ownership"`

### Task 61: explored module returns bool

**Files:**
- Modify: `src/engine/explored.h:19-21`, `src/engine/explored.c:6-8, 14-25` and its returns, `src/engine/savegame.c:284,
  576`, `tests/test_automap.py:42-43, 122, 136-137`

**Interfaces:**
- Produces: `bool explored_save(uint8_t **buf, size_t *len)`, `bool explored_load(const uint8_t *buf, size_t len)`
  (true on success); constants `EXPLORED_VERSION 1`, `EXPLORED_HEADER_SIZE 6`, `EXPLORED_MAGIC_SIZE`; the statics
  `map_square_count`, `map_first_square` become `square_count`, `first_square` (no foreign prefix).

Facts: `savegame.c:284` tests `if (explored_save(&block, &length))` as "failed" (0 success); `savegame.c:576` tests
`!automap || explored_load(...)` as "no block or failed" (`explored.h:19-20`).

- [ ] **Step 1:** Change the types and every `return 0` / `return -1` of both functions to `true` / `false`;
  rename the constants and statics.
- [ ] **Step 2:** `savegame.c:284` becomes `if (!explored_save(&block, &length))`; `savegame.c:576` becomes
  `if (ok && (!automap || !explored_load(automap, automap_length)))`.
- [ ] **Step 3:** `test_automap.py`: `restype = ctypes.c_bool` for both; `== 0` becomes `is True`, `== -1` becomes
  `is False`. The checks keep their strength.
- [ ] **Step 4:** `make`; `uv run pytest -q tests/standard`;
  `uv run pytest -q -n auto $(find tests -name 'test_automap.py' -o -name 'test_save.py')`.
- [ ] **Step 5:** `git commit -am "refactor: explored save and load return bool"`

---

## Package F: engine API shape and state (Epics 6, 7; R5.4, R5.8)

Package F runs after package E: `engine/record.h` (`rec_*`), `engine/timeline.h`, `engine/event.h` (was
`timeline_process`), `engine/party.h`, `engine/champion.h` (holds `Champion`), `engine/arith.h`, `map_step`,
`map_is_party_on`, `map_find_thing`, `direction_right/back/left`, `Location {int16_t x, y;}` and
`Position {int16_t map, x, y;}` in `engine/map.h` exist. The tasks name the code by its 72e24b5 place
(`port/src/engine/x.c:NN`); find it in `src/engine/` by the symbol name (package E moved some of it, for example
`timeline_process.c` is `event.c`).

Test commands find a module in its R1 folder with `git ls-files` (a pathspec `*` matches `/`):

- one module: `uv run pytest -q $(git ls-files 'tests/*test_creatures.py')`
- several modules: `uv run pytest -q -n auto $(git ls-files 'tests/*test_creatures.py' 'tests/*test_move.py')`
- the golden gate: `uv run pytest -q -n auto $(git ls-files 'tests/golden/test_*.py')`

In this package, "the differential gate" is:
`uv run pytest -q -n auto $(git ls-files 'tests/*test_creatures.py' 'tests/*test_move.py' 'tests/*test_champion.py' 'tests/*test_actions.py' 'tests/*test_csb_rules.py' 'tests/golden/test_*.py')`.
A red differential or golden test after a task that moves a rule (Tasks 66, 67) stops the run for the user
(Global Constraints).

New result types use this form; the Epic 3 `enum` check accepts a `typedef enum` whose type name ends in
`Result`, `Outcome` or is listed in the task (reported to the controller when the check rejects it):

```c
/* Results of move_get_move_result. */
typedef enum {
    MOVE_RESULT_DONE,
    MOVE_RESULT_STOPPED,
    MOVE_RESULT_TURNED,
} MoveResult;
```

Rulings of this package (the controller copies them into the PRD Decisions at the end of the run):

- The current group square (`g_current_group_map_x/_y`, `g_current_group_thing`) is state that lives across
  calls: `group_add_active_group` (`port/src/engine/group.c:325`) reaches the Couatl flip sound
  (`group.c:233-234`) and `wander_off_party_map` reaches the teleporter check (`group.c:907`) with the values of
  the last group turn. The port keeps this as a kept quirk in a file-static `last_turn` of `group.c`; the
  distance and the two directions to the party become fields of `GroupTurn` (R7.2).
- `MovementTest` holds only `possible` and `blocked_by_party`: `movement_blocked_by_wall`, `_door` and
  `_group` are written (`group.c:872-923`) but never read (grep: only `group.c:1583` reads `_party`).
- Two outcomes of `move_get_move_result` reach the callers today (0, nonzero: `group.c:401, 1437, 1927`,
  `projectile.c:670`); the enum keeps the three outcomes that the group path computes (0, 1, active + 2 at
  `group.c:2083-2085`), so `MoveResult` has three values and no value encodes an index.
- `sound_request_play` takes `SOUND_MODE_NOW`, `SOUND_MODE_AFTER_TICK` or `SOUND_MODE_EVENT` and a `delay`
  in ticks that only `SOUND_MODE_EVENT` uses; today mode `>= 2` means "event at time + mode - 1"
  (`port/src/engine/sound.c:176-187`) and only mode 2 (`SOUND_MODE_ONE_TICK_LATER`) is used.

#### Stream F1: group and move

Shared files: `src/engine/projectile.c` (callers of `group_get_directions_to_destination` and
`move_get_move_result`; F3 also changes this file), `src/engine/action.c` (melee damage call, rope climb; F2
also changes it), `src/engine/champion.c:843`, `src/engine/magic.c:226`, `src/engine/event.c` (reaction
dispatch, square moves), `src/engine/savegame.c` (active group allocation; F3 also changes it),
`src/engine/map.c`, `src/engine/map.h`, `src/ui/command.c:424, 445`, the tests named per task.

### Task 62: Group turn state: current group and second direction

**Files:**
- Modify: `src/engine/group.c` (`port/src/engine/group.c:76-80` globals, `:1115-1144`
  `group_get_directions_to_destination`, `:1346-1365` `GroupTurn`, `:1960-1975` `set_current_group`, every
  reader listed below)
- Modify: `src/engine/group.h` (`port/src/engine/group.h:209-212` externs, the prototype of
  `group_get_directions_to_destination`)
- Modify: `src/engine/projectile.c` (`port/src/engine/projectile.c:723` caller)
- Modify: `tests/*/test_creatures.py` (`tests/test_creatures.py:158`), `docs/rules/rules-creatures.md` if it
  names the globals (grep)

**Interfaces:**
- Consumes: `Location` (`engine/map.h`, package E).
- Produces:
  ```c
  /* The two directions from a square toward a destination: the main one and the second best. */
  typedef struct {
      int16_t primary, secondary;
  } DirectionPair;
  /* The directions from a source square to a destination. */
  DirectionPair group_get_directions_to_destination(Location from, Location to);
  ```
  `g_group_secondary_direction`, `g_current_group_map_x/_y`, `g_current_group_thing`,
  `g_current_group_distance_to_party`, `g_current_group_primary_direction_to_party`,
  `g_current_group_secondary_direction_to_party` no longer exist.

- [ ] **Step 1: Check the reads of the second direction.** Run
  `grep -n "g_group_secondary_direction" src/engine/*.c`. Expected: writes at `group.c:855, 1118, 1122, 1135,
  1139, 1142, 1488, 1528, 1652, 1974` (72e24b5 numbers), reads at `1500, 1504, 1550, 1652, 1974`. Confirm for
  each read that a write of the same turn comes before it: `jump_archenemy` is entered only from `after_move`
  (write at 1488) and `move_toward` (`t->direction` comes from `group_get_directions_to_destination` or from the
  smell at 1749-1752, both write); the loop of `jump_archenemy` writes at 1528; `flee` reads at 1652 after the
  call at 1651. `try_move` runs `move_get_move_result`, which can call `group_get_directions_to_destination`
  through `projectile_is_killed_by_impact` (`projectile.c:723`); no read follows a `try_move` without a new write.
  If a read without a preceding write of the same turn exists, stop and report it.
- [ ] **Step 2: Return the pair.** Change `group_get_directions_to_destination` to the signature above: the
  function body keeps its rule and random calls in the same order; `secondary` replaces every write of the
  global inside it. Add `DirectionPair` and the prototype to `group.h`.
- [ ] **Step 3: Move the turn values into `GroupTurn`.** Add the fields
  `int16_t secondary_direction;`, `uint16_t distance_to_party;`, `DirectionPair directions_to_party;` to
  `GroupTurn`. `set_current_group` fills them (`distance_to_party` from `group_get_square_distance`,
  `directions_to_party` from the call at 1972). Replace every read: `g_current_group_distance_to_party`
  (`group.c:820, 821, 829, 852, 1023`), `g_current_group_primary_direction_to_party` (`1013, 1409, 1869`),
  `g_current_group_secondary_direction_to_party` (`855`), `g_group_secondary_direction` (Step 1 list) by the
  field. A static function that reads them without a `GroupTurn` gets `const GroupTurn *t` as its first
  parameter: `group_get_distance_to_visible_party` (`group.c:788`), `group_get_smelled_party_direction_ordinal`
  (`847`), `group_is_creature_attacking` (`1011`). Keep the names of the static `*_ordinal` functions; Task 79
  renames them.
- [ ] **Step 4: Keep the last turn square as a kept quirk.** Replace `g_current_group_map_x/_y` and
  `g_current_group_thing` by one file-static struct and its comment:
  ```c
  /* The group of the last group turn. Kept quirk: a group that becomes active plays its flip sound at this
     square, and a group off the party map tests teleporters with this thing, as in the original. */
  static struct {
      uint16_t thing;
      Location at;
  } last_turn;
  ```
  `set_current_group` writes it; `group.c:233-234, 248` read `last_turn.at`; `group.c:907` reads
  `last_turn.thing`. `group_reset` (`group.c:2295`) clears it. Add one line on this quirk to the kept quirks of
  `docs/rules/rules-creatures.md` (the section that lists kept quirks; grep "Quirk").
- [ ] **Step 5: Callers.** `projectile.c:723` uses `.primary` of the returned pair. Delete the five externs from
  `group.h`. `grep -rn "g_current_group_\|g_group_secondary_direction" src tests tools docs` finds nothing
  outside `docs/superpowers/`.
- [ ] **Step 6: Adapt the differential test.** In `test_creatures.py`
  `test_directions_to_destination_match_original`, declare the struct and compare both fields:
  ```python
  class DirectionPair(ctypes.Structure):
      _fields_ = [("primary", ctypes.c_int16), ("secondary", ctypes.c_int16)]

  lib.group_get_directions_to_destination.restype = DirectionPair
  lib.group_get_directions_to_destination.argtypes = [Location, Location]
  ...
  got = lib.group_get_directions_to_destination(Location(sx, sy), Location(dx, dy))
  assert got.primary == want, (sx, sy, dx, dy)
  assert got.secondary == s16(struct.unpack(">H", emu.read(SECONDARY_DIRECTION, 2))[0])
  ```
  `Location` comes from `dmtest.game` (add it there if package E did not: `_fields_ = [("x", c_int16),
  ("y", c_int16)]`).
- [ ] **Step 7: Build and test.** `make lib`, then the differential gate. Expected: all pass.
- [ ] **Step 8: Commit.** `git commit -m "refactor(engine): group turn values travel in GroupTurn, not in globals"`

### Task 63: Movement test result

**Files:**
- Modify: `src/engine/group.c` (`port/src/engine/group.c:83-87` statics, `:868-948`, `:1497-1508`, `:1583`,
  `:1969-1970`, `:2020-2027`, `:2295-2302`)

**Interfaces:**
- Consumes: `GroupTurn` of Task 62.
- Produces (file-static, in `group.c`):
  ```c
  /* The result of one step test of a group. */
  typedef struct {
      bool possible;
      bool blocked_by_party;
  } MovementTest;
  static MovementTest test_movement(GroupTurn *t, int16_t direction, bool allow_imaginary);
  ```

- [ ] **Step 1: Confirm the readers.** `grep -n "movement_blocked_by\|movement_tested_directions\|flux_cage"
  src/engine/group.c`. Expected: `movement_blocked_by_party` is the only `movement_blocked_by_*` that is read
  (`group.c:1583`); `flux_cages` and `flux_cage_count` are read only in `group_is_archenemy_double_movement_possible`
  (942) and `jump_archenemy` (1503, 1507); `movement_tested_directions` only in
  `group_get_first_possible_movement_direction_ordinal` (932). Every caller of `group_is_movement_possible` is a
  function with a `GroupTurn` (also `wander_off_party_map`, `group.c:1923`).
- [ ] **Step 2: Per-turn fields.** Add to `GroupTurn`: `uint8_t tested_directions[DIRECTION_COUNT];`,
  `uint8_t flux_cages[DIRECTION_COUNT];`, `int16_t flux_cage_count;` (use the direction count constant of
  `engine/map.h`; at 72e24b5 none exists (`grep DIRECTION_COUNT port/src/engine/*.h` is empty): add
  `#define DIRECTION_COUNT 4` to the direction block of `map.h` if package E did not). A `GroupTurn` starts zeroed
  (`group.c:2009` designated initializer), which equals the resets at `group.c:1969-1970` (every turn that
  tests directions runs `set_current_group` first) and `2024-2027` (archenemy turns). The writes of
  `wander_off_party_map` before `set_current_group` are never read (Step 1).
- [ ] **Step 3: Replace the function.** `group_is_movement_possible(info, x, y, direction, allow_imaginary)`
  becomes `test_movement(t, direction, allow_imaginary)`: it reads `t->info`, `t->x`, `t->y`, writes
  `t->tested_directions`, `t->flux_cages`, `t->flux_cage_count`, and returns `MovementTest`. The three unread
  `movement_blocked_by_*` writes go. Callers that only test the result use `.possible`; `random_move`
  (`group.c:1580-1583`) keeps the struct in a local and reads `.blocked_by_party`. Keep the function order rule:
  `test_movement` stands above its first caller.
- [ ] **Step 4: The helpers.** `group_get_first_possible_movement_direction_ordinal` and
  `group_is_archenemy_double_movement_possible` take `GroupTurn *t` instead of `info, x, y` (they keep their
  names until Task 79). Delete the five statics of `group.c:83-87` and their resets in `group_reset`.
- [ ] **Step 5: Build and test.** `make lib`, then the differential gate. Expected: all pass.
- [ ] **Step 6: Commit.** `git commit -m "refactor(engine): the group movement test returns a struct"`

### Task 64: Group API: reactions, spell explosions, long parameter lists

**Files:**
- Modify: `src/engine/group.c`, `src/engine/group.h`
- Modify (call sites): `src/engine/event.c` (`port/src/engine/timeline_process.c:579-581`),
  `src/engine/projectile.c:256, 525, 623`, `src/engine/move.c:379`, `src/engine/action.c:326`
- Modify: `tests/*/test_creatures.py`, `tests/*/test_actions.py` (calls of `group_get_generated`,
  `group_get_melee_action_damage`; grep)

**Interfaces:**
- Consumes: `Location`, `DirectionPair` (Task 62).
- Produces:
  ```c
  /* A rule tells the group on a square about a danger, a projectile hit or the adjacent party (REACTION_*). */
  void group_process_reaction_event(Location at, int16_t reaction);
  /* The group event of the timeline (EVENT_GROUP_REACTION_* to EVENT_UPDATE_BEHAVIOR_CREATURE_3). */
  void group_process_event(const TimelineEvent *event);

  /* A melee attack of a champion on a group. */
  typedef struct {
      int16_t champion_index;
      uint16_t group;
      int16_t creature_index;
      Location at;
      int16_t icon;       /* the weapon icon of the action hand */
      int16_t difficulty; /* the action difficulty */
      int16_t skill;      /* the skill of the action */
  } MeleeAttack;
  int16_t group_get_melee_action_damage(const MeleeAttack *attack);
  ```
  The field list of `MeleeAttack` follows the current parameter list of `group_get_melee_action_damage`
  (`port/src/engine/group.c:1298`, 8 parameters); read it and keep the same names and order.

- [ ] **Step 1: Split the reaction entry.** Today `group_process_reaction_event(x, y, event_type, ticks)` takes a
  timeline event type 29-41 or a `REACTION_*` -3..-1 (`port/src/engine/group.h:69-70, 270`). Rename the current
  body to the static `process_turn(GroupTurn *t)` setup (everything after the `GroupTurn` initializer at
  `group.c:2008-2009`) and add the two public entries above: `group_process_reaction_event` builds the turn with
  `.event_type = reaction, .ticks = 0` (every reaction caller passes 0 or `false` today: `group.c:675, 1341`,
  `move.c:379`, `projectile.c:256, 525, 623`); `group_process_event` builds it from
  `event_square_x/_y(event)`, `event->type` and the ticks accessor that `timeline_process.c:580-581` uses. Both
  stay in `group.c`. `GroupTurn.event_type` keeps both value ranges inside the file (comment stays).
- [ ] **Step 2: Callers.** The dispatcher in `event.c` calls `group_process_event(&event)`; the six reaction
  callers pass `(Location){x, y}` and the reaction. The `bool` `false` passed as ticks (`group.c:675`,
  `projectile.c:525`) goes with the parameter.
- [ ] **Step 3: Spell explosion.** `get_creature_spell_explosion(type, &spell_sound)` (`group.c:979`) returns
  `typedef struct { uint16_t explosion; bool sound; } CreatureSpell;` (file-static type); the caller at
  `group.c:1051` reads both fields. The random calls stay in the same order.
- [ ] **Step 4: Long parameter lists.** Bring every function of `group.c` to at most five parameters, with
  `Location` for a square and the struct above for the melee damage:
  `group_get_generated` (`group.c:369`, 6), `remove_dead_creature` (566, 6), `group_get_damage_creature_outcome`
  (608, 6), `group_set_ordered_cells_to_attack` (1147, 6), `group_get_melee_action_damage` (1298, 8). A function
  that has `x, y` of one square takes `Location at`; one that has a source and a target takes `Location from,
  Location to`. Update the callers (grep each name in `src/` and `tests/`).
- [ ] **Step 5: Tests.** The ctypes declarations of the changed functions in the tests use `Location` and a
  `MeleeAttack` structure in `dmtest.game` (fields as in C). The assertions stay.
- [ ] **Step 6: Build and test.** `make lib`, then the differential gate. Expected: all pass.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): one group entry for reactions, structs for long parameter lists"`

### Task 65: Move API: request, destination and result

**Files:**
- Modify: `src/engine/move.h`, `src/engine/move.c` (`port/src/engine/move.c:21-23, 31-45, 110-261, 400`)
- Modify: `src/engine/group.c` (`port/src/engine/group.c:2191-2241` `group_get_move_result`, readers at
  `1437-1439`, `1927-1929`; call sites `401, 483, 520, 535, 2265`)
- Modify: `src/engine/map.h`, `src/engine/map.c` (`port/src/engine/map.c:85-101`)
- Modify (call sites): `src/engine/event.c` (`port/src/engine/timeline_process.c:298-341`),
  `src/engine/projectile.c:69, 172, 473-476, 670`, `src/engine/champion.c:843`, `src/engine/magic.c:226`,
  `src/engine/action.c:439-442`, `src/ui/command.c:424, 445`, `src/engine/move.c:277-387`
- Modify: `tests/conftest.py` (`BOOL_GLOBALS`: drop `g_use_rope_to_climb_down_pit`), `tests/*/test_move.py:36`

**Interfaces:**
- Consumes: `Location`, `Position`.
- Produces (in `engine/move.h`):
  ```c
  /* Results of move_get_move_result. */
  typedef enum {
      MOVE_RESULT_DONE,    /* the thing is on its destination (or stays where it is) */
      MOVE_RESULT_STOPPED, /* the mover died, was deleted, waits off the map, or became active or inactive */
      MOVE_RESULT_TURNED,  /* a teleporter turned an attacking group, which stopped its attack */
  } MoveResult;

  /* One move of a thing; THING_NONE is the party. */
  typedef struct {
      uint16_t thing;
      Location from;   /* not read when enters */
      Location to;     /* not read when leaves */
      bool enters;     /* the thing has no source square: it appears on the map */
      bool leaves;     /* the thing has no destination: it leaves the map */
      bool keeps_cell; /* a teleporter does not turn the thing (an object that a launcher creates) */
      bool uses_rope;  /* the party climbs down with a rope: no fall damage */
  } Move;

  /* Where a move ended: the square after teleporters, pits and stairs, the cell and the direction. */
  typedef struct {
      int16_t map, x, y, cell, direction;
  } MoveDestination;

  /* Moves a thing (the party, a group, an object, a projectile) and follows teleporters, pits and stairs.
     Fills destination (when not NULL) for a move with a destination. */
  MoveResult move_get_move_result(const Move *move, MoveDestination *destination);
  ```
  In `engine/map.h`:
  ```c
  /* The square one level below (LEVEL_DOWN) or above (LEVEL_UP) at the same dungeon place; map -1 when none. */
  Position map_get_level_change_location(Position from, int16_t level_delta);
  ```
  `g_move_result_map_x/_y/_map_index/_cell/_direction`, `g_use_rope_to_climb_down_pit`,
  `NO_TELEPORTER_ROTATION` and `map_get_location_after_level_change` no longer exist.

- [ ] **Step 1: Map the old arguments.** Old `(thing, src_x, src_y, dst_x, dst_y)` (`move.c:215-216`):
  `src_x < 0` is `enters`; `src_x == -2` (`NO_TELEPORTER_ROTATION`, only `projectile.c:172`) is `enters` plus
  `keeps_cell`; `dst_x < 0` is `leaves`. The rope flag is set around one call (`action.c:440-442`) and read in the
  pit branch (`move.c:166-167`). Write this map as the comment of `Move` (done above) and check every call site
  (16 in `src/`, list from `grep -rn "move_get_move_result(" src`) against it.
- [ ] **Step 2: Level change.** Replace `map_get_location_after_level_change(map, delta, &x, &y)`
  (`map.c:85-101`; returns -1 and leaves x, y unchanged when there is no map) by
  `map_get_level_change_location(Position from, int16_t level_delta)`: it returns `{-1, from.x, from.y}` when
  there is no map, else the new map and square. Update the callers `group.c:2124, 2134`, `move.c:161, 172, 295`.
  The copy in `ui/view.c:1419-1435` stays for Task 88.
- [ ] **Step 3: Destination out-parameter.** In `move.c` and `group.c`, the walk writes a local
  `MoveDestination` instead of the five globals (`move.c:40-43, 238-244`, `group.c:2220-2224`; the projectile
  direction is the input of the walk at `move.c:238` and the rotation updates it at `move.c:40-43`). At the end
  the function copies it to `*destination` when `destination` is not NULL and the move has a destination. The
  copy happens on the same paths where the globals were written at 72e24b5; on the early returns (impact kill at
  `move.c:229`, `group.c:2207-2209`; group deleted at `group.c:2211-2217`) `*destination` stays unchanged. No
  caller reads it on those paths: `group.c:1437-1439`, `1927-1929` read only after result 0;
  `timeline_process.c:300-317` and `projectile.c:473-476` move objects and projectiles, which never take the early
  returns.
- [ ] **Step 4: The readers.** `try_move` (`group.c:1437-1439`), `wander_off_party_map` (`1927-1929`),
  `update_projectile_event` and `update_explosion_events` (`timeline_process.c:298-319`, they get a
  `const MoveDestination *` parameter from `event_move_teleporter_or_pit_square_things`), and
  `projectile_process_move_event` (`projectile.c:473-476`) read their local `MoveDestination`.
- [ ] **Step 5: Result values.** `move_get_move_result` returns `MOVE_RESULT_DONE` where it returned 0,
  `MOVE_RESULT_STOPPED` where it returned 1, and `MOVE_RESULT_TURNED` where the group path returned
  `active + 2` (`group.c:2085` through `enter_destination`). The group path keeps the active index of the turned
  group in a field of `GroupMove` (`int16_t turned_active; /* -1: none */`) and calls `group_stop_attacking` with
  it (`group.c:2183-2184`). Callers that tested nonzero test `!= MOVE_RESULT_DONE`.
- [ ] **Step 6: The rope.** `action.c:439-442` passes `.uses_rope = true`; `follow_square_chain` gets the flag
  from the request and passes it to `champion_fall_damage`. Delete `g_use_rope_to_climb_down_pit` and its reset
  (`move.c:400`) and its name in `tests/conftest.py` `BOOL_GLOBALS`.
- [ ] **Step 7: Call sites.** Every caller builds a compound literal, for example
  `move_get_move_result(&(Move){.thing = thing, .to = {x, y}, .enters = true}, NULL)`. `group_get_move_result`
  keeps its body for Task 66 but takes `(const Move *, MoveDestination *)` and stays callable only from
  `move.c` (prototype in `group.h` until Task 66).
- [ ] **Step 8: Tests.** `test_move.py:36` lists the symbol name only; keep it. `grep -rn "move_get_move_result\|
  map_get_location_after_level_change\|g_move_result" tests docs` and update every hit (docs name the new API).
- [ ] **Step 9: Build and test.** `make lib`, then the differential gate. Expected: all pass.
- [ ] **Step 10: Commit.** `git commit -m "refactor(engine): move requests and destinations instead of coordinate codes and globals"`

### Task 66: One move chain for the party, groups, objects and projectiles

**Files:**
- Modify: `src/engine/move.c` (`port/src/engine/move.c:104-213` `is_teleported`, `follow_square_chain`,
  `arrive_on_square`, `:217-261` `move_get_move_result`)
- Modify: `src/engine/group.c` (`port/src/engine/group.c:2056-2241`: `move_get_teleporter_rotated_group_result`,
  `GroupMove`, `follow_destination_squares`, `enter_destination`, `group_get_move_result`; `:486-497`,
  `:566-580`, `:640-645` the moving-possessions cells)
- Modify: `src/engine/group.h`, `src/engine/move.h`

**Interfaces:**
- Consumes: `Move`, `MoveDestination`, `MoveResult` (Task 65); `map_step`, `direction_back` (package E).
- Produces: one walker in `move.c`, file-static:
  ```c
  /* Kinds of thing that move along a square chain: the chain rules differ per kind. */
  #define MOVER_PARTY      0
  #define MOVER_GROUP      1
  #define MOVER_OBJECT     2
  #define MOVER_PROJECTILE 3
  ```
  The walker takes `int16_t mover` (standard 3.2: no `enum` for constants; `enum` is for result types only).
  Group hooks in `engine/group.h`, called only by `move.c` (names binding, parameters chosen by the author within
  five, reported in the ledger):
  - `bool group_is_killed_by_fall(...)`: the pit branch of `follow_destination_squares` (`group.c:2123-2131`).
  - `int16_t group_turn_by_teleporter(...)`: `move_get_teleporter_rotated_group_result` (`group.c:2058-2086`);
    returns the active index of a turned attacking group on the party map, else -1.
  - `void group_remove_after_move(...)`: the "dies or forbidden map" branch (`group.c:2211-2217`).
  - `MoveResult group_enter_square(...)`: `enter_destination` (`group.c:2152-2189`).
  `group_get_move_result` no longer exists.

- [ ] **Step 1: Baseline.** Before any change run the differential gate and save the output:
  `uv run pytest -q -n auto ... > "$SCRATCH/f5-before.txt"` (SCRATCH: the SDD workspace). Expected: all pass.
- [ ] **Step 2: Write the rule table.** Read both chains and write, as the comment above the walker, one line
  per square type and mover kind. Facts at 72e24b5: teleporter: party scope `TELEPORTER_SCOPE_PARTY`, objects and
  projectiles any scope except creatures-only (`move.c:115-125`), groups only with `TELEPORTER_SCOPE_CREATURES`
  (`group.c:2107-2108`); party sound `SOUND_TELEPORT` now (`move.c:136-137`), group sound `SOUND_BUZZ` after the
  tick when audible (`group.c:2117-2118`); rotation: party direction (`move.c:138-139`), projectile direction and
  cell (`move.c:31-45`), object cell unless `keeps_cell` (`move.c:142-143`), group directions and cells
  (`group.c:2058-2086`); a self-pointing teleporter ends the chain (`move.c:146`, `group.c:2120`). Pit: not for
  levitating things (`move.c:157`, `group.c:2122`); party fall damage with the rope flag (`move.c:162-168`);
  groups take `FALL_DAMAGE` on the source map and drop the possessions of creatures that died
  (`group.c:2123-2131`). Stairs: objects (not projectiles, not the party) and groups; down stairs change the
  level; the thing steps out and takes the cell beside the stairs (`move.c:169-180`, `group.c:2132-2141`, same
  cell formula).
- [ ] **Step 3: One walker.** Write `static bool follow_square_chain(Walk *w)` in `move.c`, where `Walk` is a
  file-static struct with the mover kind, the thing, the destination `Position`, the start direction, the
  `MoveDestination` being built, the rope flag, `keeps_cell`, the source map and square, and the group fields of
  `GroupMove` (`audible`, `turned_active`). It returns true when a fall killed the whole group. It calls the
  group hooks for `MOVER_GROUP`. `SQUARE_CHAIN_MAXIMUM` bounds it as both copies do. The side-of-cell formula
  uses the helper of package E (`direction_` prefix in `map.h`).
- [ ] **Step 4: One entry.** `move_get_move_result` handles all kinds: the impact kill test at the start
  (party at `move.c:229`, group at `group.c:2206-2209`), the walk, the same-square tests, the leave of the source
  square, the arrival (`arrive_on_square` for party, objects and projectiles; `group_enter_square` for groups).
  The order of calls with side effects (random numbers, sounds, timeline events, sensor calls,
  `map_set_current_map`) stays the order of each old path; compare each kind path line by line with the old
  function in the commit message notes.
- [ ] **Step 5: Moving possessions.** `drop_moving_creature_possessions_cell_count/_cells`
  (`group.c:88-89`) collect the cells of creatures that die in a fall and are dropped in the chain: they become
  fields of the group part of `Walk`, filled through the fall hook (the hook takes a pointer to the list), and
  `group_drop_moving_creature_fixed_possessions` takes the list. `group_get_damage_all_creatures_outcome` resets
  the list at its start today (`group.c:644`); the hook clears it at the same point.
- [ ] **Step 6: Delete the old code.** `follow_destination_squares`, `GroupMove`, `group_get_move_result`, the
  `is_teleported` and `follow_square_chain` of `move.c` go; `group.h` loses `group_get_move_result`.
- [ ] **Step 7: Build and test.** `make lib`, then the differential gate; compare with
  `"$SCRATCH/f5-before.txt"`. Expected: the same tests pass. A difference stops the task for the user (Global
  Constraints).
- [ ] **Step 8: Docs.** `docs/rules/rules-movement.md` and `docs/rules/rules-creatures.md`: the sections that
  name `follow_square_chain`, `follow_destination_squares` or `group_get_move_result` (grep) describe the one
  chain with the rule table of Step 2.
- [ ] **Step 9: Commit.** `git commit -m "refactor(engine): one move chain for every mover"`

### Task 67: Active groups: defined values on corrupt data, cleared allocation

**Files:**
- Modify: `src/engine/group.c`, `src/engine/group.h` (`port/src/engine/group.h:143-147` `group_active`)
- Modify: the active group allocation: `game_init_active_groups` (`port/src/engine/game.c:381-390`; package E
  may have moved it, grep) and `port/src/engine/savegame.c:472-490`
- Modify: `src/engine/map.c` (`port/src/engine/map.c:252-255` `map_get_creature_attributes`)
- Modify: `tests/golden/test_golden_save.py:33-39, 63`, `tests/golden/test_golden_rules.py:14, 73`
- Modify: `docs/formats/save-format.md` (section "Limits"), `docs/formats/dungeon-format.md` (section "Not
  ported" or a new "Deviations" list), `docs/todo.md` (delete the two items "Uninitialized bytes of free active
  groups in a save" and "NULL read of creature attributes on a corrupt dungeon")

**Interfaces:**
- Produces:
  ```c
  /* The active group entry of a group on the party map (a corrupt index reads a free entry). */
  const ActiveGroup *group_get_active(uint16_t group);
  /* Allocates count free active group entries, all bytes 0 except group_index -1. */
  void group_allocate_active(uint16_t count);
  ```
  The inline `group_active` of `group.h` goes; `group.c` uses a file-static `active_of(group)` that returns a
  writable pointer.

- [ ] **Step 1: Bounded entry.** In `group.c`:
  ```c
  /* The entry for a cells byte beyond the table (a corrupt save): reads give a free entry, writes are lost. */
  static ActiveGroup corrupt_entry;

  /* The active group entry of a group on the party map. Deviation: the original reads beyond the table when the
     cells byte of the record is too large. */
  static ActiveGroup *active_of(uint16_t group)
  {
      uint16_t index = rec_group_cells_byte(group);
      if (index >= g_max_active_group_count) {
          corrupt_entry = (ActiveGroup){.group_index = -1};
          return &corrupt_entry;
      }
      return &g_active_groups[index];
  }

  const ActiveGroup *group_get_active(uint16_t group)
  {
      return active_of(group);
  }
  ```
  (`rec_group_cells_byte` is the 72e24b5 name; use the name of `engine/record.h`.) Replace the 9 `group_active(`
  calls in `group.c` (`grep -c` gives 9) by `active_of(`; `projectile.c:492` and `action.c:287` read only:
  `group_get_active(`. `ui/view.c:674-675` changes in Task 82.
- [ ] **Step 2: Creature attributes.** `map_get_creature_attributes` returns 0 (no attribute) for a creature type
  at or above the creature count of the table, with the comment
  `/* Deviation: a creature type beyond the table (a corrupt dungeon) has no attributes; the original reads
  beyond it. */`. Use the count accessor of `data/tables.h` (grep `CREATURE_TYPE_COUNT` or the table count);
  if none exists, add `#define CREATURE_TYPE_COUNT 27` beside the creature info accessor in `data/tables.h`
  (27 types: `tests/test_tables.py:161` iterates `range(27)`).
- [ ] **Step 3: One allocation.** `group_allocate_active(count)` in `group.c`: `free(g_active_groups)`,
  `calloc(count, sizeof *g_active_groups)`, abort on NULL through `game_fatal` (package E), every
  `group_index = -1`, `g_max_active_group_count = count`. `game_init_active_groups` and the loader
  (`savegame.c:474-488`) call it. The loader keeps its order: it allocates before `unpack_active_group`
  (`savegame.c:500-501`), and the DM count raise at `savegame.c:503-504` stays after the unpack.
- [ ] **Step 4: Drop the scrub.** Delete `scrub_free_groups` (`tests/test_golden_save.py:33-39`) and its two
  calls (`test_golden_save.py:63`, `test_golden_rules.py:73`) and the import (`test_golden_rules.py:14`).
- [ ] **Step 5: Golden gate.** `make lib`, then the golden gate. Expected: all pass with the unchanged
  `tests/golden/*.json`. If `save.json` or `rules.json` differs, an entry freed by `group_remove_active_group`
  (it sets only `group_index = -1`, `group.c:342`) kept old bytes in the scenario: stop and report (the PRD
  invariant says the snapshot stays equal).
- [ ] **Step 6: Deviation entries.** `docs/formats/save-format.md`, section "Limits": "Free active group entries
  are saved with 0 bytes after a new game or a load; the original saves whatever its memory holds. An entry that
  a group leaves keeps its bytes, as in the original." and "A cells byte beyond the active group table reads a
  free entry (`group_get_active`)." `docs/formats/dungeon-format.md`: "A creature type beyond the creature table
  has no attributes (`map_get_creature_attributes`)."
- [ ] **Step 7: Build and test.** `make lib`, the differential gate. Expected: all pass.
- [ ] **Step 8: Commit.** `git commit -m "fix(engine): defined values for corrupt active group and creature indices, cleared active groups"`

#### Stream F2: champion, inventory, action, magic

Shared files: `src/engine/group.c` (calls of the champion helpers at `port/src/engine/group.c:683-685, 1208-1238,
1316-1329`; F1 changes this file), `src/engine/move.c:351, 391` (F1 changes this file), `src/engine/game.c`
(resets), `src/engine/savegame.c:250, 443, 514` (portraits; F3 changes this file), `src/ui/startend.c:269`,
`src/ui/ui_champion.c:206, 254, 469, 473, 578`, `src/ui/ui_menus.c:97-105`, `src/ui/view.c:2505`,
`src/platform/state_json.c:261-322, 499-500`, the tests named per task.

### Task 68: Champion functions take an index; skill and wound flags as parameters

**Files:**
- Modify: `src/engine/champion.h` (`port/src/engine/champion.h:95-101`), `src/engine/champion.c`
  (`port/src/engine/champion.c:516-540, 611-622, 628-670, 718-760`)
- Modify: the four statistic helpers that package E moved from `game.c:392-435` into the champion module
  (`game_get_stamina_adjusted_value`, `game_get_statistic_adjusted_attack`, `game_adjust_statistic_current_value`,
  `game_is_lucky` at `port/src/engine/game.h:225-228`; grep their current names)
- Modify (call sites): `src/engine/group.c`, `src/engine/move.c`, `src/engine/inventory.c:443-452`,
  `src/engine/magic.c:358`, `src/engine/action.c` (7 calls), `src/ui/startend.c:269-270`,
  `src/ui/ui_champion.c:254, 469, 578`, `src/platform/state_json.c:268, 289, 322`
- Modify: `tests/*/test_champion.py:70-73, 270-278`, `tests/*/test_creatures.py:481-492`

**Interfaces:**
- Produces:
  ```c
  /* The skill level of a champion; the flags leave out object modifiers or temporary experience. */
  int16_t champion_get_skill_level(int16_t champion_index, int16_t skill, bool ignore_modifiers,
                                   bool ignore_temporary);
  /* The defense of a body slot against a wound; sharp adds the sharp defense of the armour. */
  int16_t champion_get_wound_defense(int16_t champion_index, int16_t slot, bool sharp);
  uint16_t champion_get_maximum_load(int16_t champion_index);
  int16_t champion_get_movement_ticks(int16_t champion_index);
  int16_t champion_get_dexterity(int16_t champion_index);
  ```
  The four statistic helpers take `int16_t champion_index` instead of `Champion *`. `SKILL_IGNORE_OBJECT_MODIFIERS`,
  `SKILL_IGNORE_TEMPORARY_EXPERIENCE` and `WOUND_DEFENSE_USE_SHARP` (`champion.h:22, 31`) no longer exist.

- [ ] **Step 1: Check the pointer callers.** `grep -rn "champion_get_maximum_load\|champion_get_movement_ticks\|
  champion_get_dexterity" src` and the four helpers: every argument is `&g_champions[i]` or a local
  `Champion *c = &g_champions[i]` (`champion.c:631, 643, 667, 683, 706, 763-791, 973`, `group.c:683-685, 1208-1238,
  1316-1320`, `inventory.c:443-452`, `move.c:351, 391`, `ui_champion.c:254`, `state_json.c:322`). An argument
  that is not a member of `g_champions` (a copy, a candidate buffer) stops the task: report it.
- [ ] **Step 2: Change the signatures** to the ones above; inside, `const Champion *c = &g_champions[champion_index];`.
- [ ] **Step 3: Split the flags.** `champion_get_skill_level` reads the two bools instead of masking
  (`champion.c:521-537`); `champion_get_wound_defense` reads `sharp` (`champion.c:721-722`). Callers:
  `startend.c:269-270` passes `true, true`; `ui_champion.c:469` passes `false, true`; `champion.c:611-622`
  passes `true, true`; `champion.c:756-757` passes `attack_type == ATTACK_SHARP`; every other caller passes
  `false, false`. Delete the three flag constants.
- [ ] **Step 4: Tests.** `test_champion.py`: the argtypes become `[c16, c16, ctypes.c_bool, ctypes.c_bool]`,
  `[c16, c16, ctypes.c_bool]`, `[c16]`; the differential loop keeps the packed value for the emulator and unpacks
  it for the port:
  ```python
  for flags in (0, 0x4000, 0x8000, 0xC000):
      got = lib.champion_get_skill_level(i, skill, bool(flags & 0x4000), bool(flags & 0x8000))
      assert got == s16(w.emu.call(SKILL_LEVEL, [(2, i), (2, skill | flags)]))
  ...
  assert lib.champion_get_maximum_load(i) == w.emu.call(MAX_LOAD, [(4, addr)]) & 0xFFFF
  ...
  for sharp in (0, 0x8000):
      assert lib.champion_get_wound_defense(i, slot, bool(sharp)) == s16(w.emu.call(WOUND_DEFENSE, [(2, i), (2, slot | sharp)]))
  ```
  (`0x4000` and `0x8000` are the original flag bits of the emulator call, not port constants.) The same for
  `test_creatures.py:481-492`.
- [ ] **Step 5: Docs.** `grep -rn "SKILL_IGNORE\|WOUND_DEFENSE_USE_SHARP\|champion_get_skill_level" docs` and
  update the hits outside `docs/superpowers/`.
- [ ] **Step 6: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_champion.py' 'tests/*test_creatures.py' 'tests/*test_actions.py' 'tests/golden/test_*.py')`.
  Expected: all pass.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): champion functions take an index and explicit flags"`

### Task 69: Runtime fields of a champion

**Files:**
- Modify: `src/engine/champion.h` (the `Champion` struct, which package E moved from
  `port/src/engine/game.h:70-98`; `port/src/engine/champion.h:63-71` the parallel arrays),
  `src/engine/champion.c` (`port/src/engine/champion.c:98-102, 160-185, 346, 594, 779-820`)
- Modify (call sites): `src/engine/game.c:66-68, 99`, `src/engine/savegame.c:250, 443, 514`,
  `src/ui/ui_champion.c:206, 473`
- Modify: `tests/dmtest/game.py` (the `Champion` structure), `tests/*/test_champion.py:223-239, 310, 438`,
  `tests/*/test_gameloop.py:402`, `tests/*/test_csb_ending.py:433`, `tests/*/test_actions.py:273`,
  `tests/*/test_save.py:86, 159`, `tests/*/test_control.py:352, 412, 871, 884`, `tests/*/test_move.py:186`,
  `tests/*/test_creatures.py:343`, `tests/*/test_modes.py:655`

**Interfaces:**
- Produces:
  ```c
  /* State of a champion that the save file does not hold. */
  typedef struct {
      int16_t pending_damage;                    /* damage of the tick, applied after the tick */
      uint16_t pending_wounds;                   /* WOUND_* bits of the tick */
      bool skill_level_up[SKILL_BASE_COUNT];     /* the panel shows the base skill in green */
      uint8_t portrait[CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT];
  } ChampionRuntime;
  ```
  `Champion` gets `ChampionRuntime runtime;` as its last field, after `magic_map`. `g_champion_pending_damage`,
  `g_champion_pending_wounds`, `g_champion_skill_level_up`, `g_champion_portraits` no longer exist.

- [ ] **Step 1: Check the resets.** At 72e24b5: `game_start_new` clears `g_champions` and the pending damage and
  wounds (`game.c:65-68`) but not the portraits; `game_reset_session` clears the level-ups (`game.c:99`);
  `champion_unpack` clears the whole struct (`champion.c:162`); the load restores the portraits after the unpack
  (`savegame.c:506` then `514`). With the portraits inside `Champion`, `game_start_new` would clear them.
  Assumption: no portrait is set between a `champion_set_portrait` call and a later `game_start_new` that a screen
  then shows. Check: `grep -rn "game_start_new\|champion_set_portrait" src`; every `champion_set_portrait` call
  is a recruit after the new game (`champion.c:346` path). If a portrait must survive `game_start_new`, keep the
  portrait out of `ChampionRuntime` as `g_champion_portraits` and report it.
- [ ] **Step 2: The struct.** Add `ChampionRuntime` and the field. `skill_level_up` is `bool` (today `int16_t`
  set to 1 at `champion.c:594`, read as a truth value at `ui_champion.c:473`); the ctypes mirror uses
  `ctypes.c_bool`.
- [ ] **Step 3: Unpack keeps the runtime part.** `champion_unpack`:
  ```c
  ChampionRuntime runtime = c->runtime; /* the save file does not hold it */
  memset(c, 0, sizeof *c);
  c->runtime = runtime;
  ```
  `champion_pack` does not read `runtime`. The load path keeps its order (unpack, then portraits).
- [ ] **Step 4: Replace the arrays.** Every use (`grep -rn "g_champion_pending_damage\|g_champion_pending_wounds\|
  g_champion_skill_level_up\|g_champion_portraits" src`: 17 lines) reads
  `g_champions[i].runtime.<field>`. `game_start_new` drops the two `memset`s of the pending arrays (the clear of
  `g_champions` covers them); `game_reset_session` clears `skill_level_up` of each champion.
- [ ] **Step 5: The ctypes mirror.** `tests/dmtest/game.py`: add a `ChampionRuntime` structure and append
  `("runtime", ChampionRuntime)` to `Champion._fields_`. Assert the size in the module that defines it:
  `assert ctypes.sizeof(Champion) == lib.champion_sizeof()` only if such an export exists; else compare the
  offset of `slots` with the C offset that `tests/dmtest/game.py` already checks (grep `offsetof`), and add no new
  export.
- [ ] **Step 6: Tests.** Each test that reads `(c16 * 4).in_dll(lib, "g_champion_pending_damage")` reads
  `[c.runtime.pending_damage for c in champions(lib)]` (the `champions` helper of `dmtest.game`); writes go to the
  fields. `test_champion.py:238-239` compares the four fields with the emulator words as before.
  `test_save.py:86, 159` and `test_champion.py:438` read the portraits from `c.runtime.portrait`.
- [ ] **Step 7: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_champion.py' 'tests/*test_save.py' 'tests/*test_control.py' 'tests/*test_actions.py' 'tests/*test_gameloop.py' 'tests/*test_csb_ending.py' 'tests/*test_move.py' 'tests/*test_creatures.py' 'tests/*test_modes.py' 'tests/golden/test_*.py')`.
  Expected: all pass; `tests/golden/save.json` unchanged (the packed form does not change).
- [ ] **Step 8: Docs.** `docs/rules/game-state.md` and `docs/rules/rules-champions.md` name the fields instead of
  the four arrays (grep).
- [ ] **Step 9: Commit.** `git commit -m "refactor(engine): pending damage, level-ups and portraits are fields of the champion"`

### Task 70: Action list and the action target

**Files:**
- Modify: `src/engine/action.h` (`port/src/engine/action.h:60-64, 77`), `src/engine/action.c`
  (`port/src/engine/action.c:91-137, 165-173, 250-330, 525, 650-660`)
- Modify (call sites): `src/ui/ui_menus.c:97-105`, `src/ui/command.c:638`, `src/platform/state_json.c:261,
  499-500`, `src/platform/control.c:531`
- Modify: `tests/*/test_actions.py:264`, `tests/*/test_ui.py:252`, `tests/golden/test_golden_rules.py:259-261`,
  `tests/dmtest/game.py` (the `ActionList` structure)

**Interfaces:**
- Produces:
  ```c
  /* The actions of the acting champion's action hand: up to ACTION_LIST_COUNT, ACTION_NONE for an empty line. */
  typedef struct {
      uint8_t actions[ACTION_LIST_COUNT];
      uint8_t minimum_skill_levels[ACTION_LIST_COUNT];
      uint16_t count;
  } ActionList;
  extern ActionList g_action_list;
  ```
  `g_action_list_indices`, `g_action_list_minimum_skill_levels`, `g_action_count` and the extern
  `g_action_target_group_thing` no longer exist.

- [ ] **Step 1: The struct.** Replace the three globals (`action.c:93-96`, `action.h:61-64`; the header writes the
  literal 3, the definition `ACTION_LIST_COUNT`) by `g_action_list`. `action_reset` (`action.c:170-172`) clears it
  the same way (`ACTION_NONE` in `actions`, 0 elsewhere).
- [ ] **Step 2: The target group.** `g_action_target_group_thing` is written at `action.c:525` and read at
  `action.c:252, 307, 320, 326` (one file, no other reader: grep). It becomes a parameter of the static functions
  that read it, passed from the function that computes it at line 525. Delete the extern.
- [ ] **Step 3: Callers.** The ui, `state_json.c` and `control.c` read `g_action_list.actions[k]` and
  `g_action_list.count`.
- [ ] **Step 4: Tests.** `tests/dmtest/game.py` gets
  ```python
  class ActionList(ctypes.Structure):
      _fields_ = [("actions", ctypes.c_uint8 * 3), ("minimum_skill_levels", ctypes.c_uint8 * 3), ("count", ctypes.c_uint16)]
  ```
  (3 from `C.ACTION_LIST_COUNT`); the three tests read `ActionList.in_dll(lib, "g_action_list")`.
- [ ] **Step 5: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_actions.py' 'tests/*test_ui.py' 'tests/*test_control.py' 'tests/golden/test_*.py')`.
  Expected: all pass.
- [ ] **Step 6: Commit.** `git commit -m "refactor(engine): the action list is one struct, the action target a parameter"`

### Task 71: Magic and inventory results as structs

**Files:**
- Modify: `src/engine/magic.c` (`port/src/engine/magic.c:303-330, 375-385`), `src/engine/inventory.c`
  (`port/src/engine/inventory.c:203-250`)
- Modify: the magic map position helpers (`port/src/engine/game.h:116-133`; grep their current place after
  package E)
- Modify (call site): `src/ui/view.c:2505`

**Interfaces:**
- Produces:
  ```c
  /* The kept map position of NETA from the magic map bytes of a champion. */
  Position game_get_magic_map_position(const uint8_t *spells);
  /* Stores the kept map position of NETA in the magic map bytes. */
  void game_set_magic_map_position(uint8_t *spells, Position position);
  ```
  (keep the prefix of the header that holds them after package E.) File-static in `magic.c`:
  `typedef struct { bool cast; uint16_t disabled_ticks; } MagicMapCast;` returned by `cast_magic_map`. File-static
  in `inventory.c`: `typedef struct { int16_t bonus; uint16_t statistic; } ObjectModifier;` returned by
  `get_object_modifier`.

- [ ] **Step 1: Magic map position.** Change the two helpers to the signatures above (`position_x/_y` and the
  map mask stay inside). Callers: `magic.c:319` passes `(Position){g_party_map_index, g_party_map_x,
  g_party_map_y}` (the party position names of package E); `view.c:2505` reads the struct.
- [ ] **Step 2: `cast_magic_map`.** Returns `MagicMapCast`; `disabled_ticks` is set only on the NETA path
  (`magic.c:320`) and 0 else, as the caller's local was (check its initializer at `magic.c:375-380`; keep the value
  the caller used before). `magic.c:380` reads both fields.
- [ ] **Step 3: `get_object_modifier`.** Returns `ObjectModifier`; every return path sets `statistic` to the
  value the out-parameter had on that path (`STAT_LUCK` default at `inventory.c:206`, `STAT_MANA` at 213, and the
  `stat_bonus` paths). `inventory.c:248` reads both.
- [ ] **Step 4: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_champion.py' 'tests/*test_csb_magic_map.py' 'tests/golden/test_*.py')`.
  Expected: all pass.
- [ ] **Step 5: Commit.** `git commit -m "refactor(engine): magic map and object modifier results as structs"`

#### Stream F3: projectile, sensor, savegame, sound

Shared files: `src/engine/group.c:637, 1055` (explosion and projectile creation in the creature attack; F1
changes the lines around 1051 in Task 64: expect a merge near them), `src/engine/action.c:371, 724` (F2 changes
this file), `src/engine/magic.c:130` (F2 changes this file), `src/engine/event.c` (door attack, material group),
`src/ui/startend.c:124-210, 449, 529-530`, `src/ui/advanced.c:123-133`, `src/platform/control.c:1200-1205`,
`src/engine/savegame.c` (F1 and F2 change other lines), the tests named per task.

### Task 72: Projectile impact and launch without globals

**Files:**
- Modify: `src/engine/projectile.h` (`port/src/engine/projectile.h:125-133, 152`), `src/engine/projectile.c`
  (`port/src/engine/projectile.c:46-47, 63-100, 180-225, 245-260, 380-390, 795-870`)
- Modify (call sites): `src/engine/group.c:1055`, `src/engine/action.c:371, 724`, `src/engine/magic.c:130`
- Modify: `tests/conftest.py` (`BOOL_GLOBALS`: drop `g_create_launcher_projectile`),
  `tests/*/test_creatures.py:200-220`, the tests that call `projectile_create` (5 calls: grep)

**Interfaces:**
- Produces:
  ```c
  /* The attack of a projectile hit: damage, attack type and poison. */
  typedef struct {
      uint16_t attack;
      int16_t type;   /* ATTACK_* */
      int16_t poison;
  } Impact;
  /* The attack of a flying thing when it hits. */
  Impact projectile_get_impact_attack(uint16_t projectile, uint16_t thing);

  /* A new projectile: the flying thing, its square, cell and direction, its energies, and whether a launcher
     made it (the first move then ignores impacts). */
  typedef struct {
      uint16_t thing;
      Location at;
      int16_t cell, direction;
      uint8_t kinetic_energy, attack, step_energy;
      bool from_launcher;
  } ProjectileLaunch;
  void projectile_create(const ProjectileLaunch *launch);
  ```
  `g_create_launcher_projectile`, `g_projectile_poison_attack`, `g_projectile_attack_type` no longer exist.

- [ ] **Step 1: Check the flow of the two attack globals.** `grep -n "g_projectile_poison_attack\|
  g_projectile_attack_type" src/engine/projectile.c`. Expected: written only in `projectile_get_impact_attack`
  (`188-216`) and `projectile_reset` (`869-870`); read at `253` (after the call at `250`, same function), `386-388`
  (after the call at `375`, same function). If a read can see a value of an earlier call, stop and report.
- [ ] **Step 2: Return `Impact`.** `projectile_get_impact_attack` returns the struct; the callers in
  `hit_creature` and `projectile_has_impact_occurred` keep it in a local; the black flame path
  (`projectile.c:239-241`) uses `.attack`. The random calls stay in the same order.
- [ ] **Step 3: Launch struct.** `projectile_create` takes `const ProjectileLaunch *`; `from_launcher` replaces
  `g_create_launcher_projectile` (`projectile.c:79` reads it; `801-806` set it around two calls in
  `projectile_trigger_launcher`). Callers: `group.c:1055`, `action.c:724`, `projectile.c:802, 804, 860`
  (and `projectile_shoot_from_champion`, which keeps its five parameters).
- [ ] **Step 4: Tests.** `test_creatures.py` `test_projectile_impact_attack_matches_original`:
  ```python
  class Impact(ctypes.Structure):
      _fields_ = [("attack", ctypes.c_uint16), ("type", ctypes.c_int16), ("poison", ctypes.c_int16)]

  lib.projectile_get_impact_attack.restype = Impact
  got = lib.projectile_get_impact_attack(14 << 10, thing)
  assert got.attack == want, (hex(thing), kinetic, attack)
  assert got.type == s16(struct.unpack(">H", w.emu.read(ATTACK_TYPE, 2))[0])
  assert got.poison == s16(struct.unpack(">H", w.emu.read(POISON_ATTACK, 2))[0])
  ```
  `Impact` and `ProjectileLaunch` live in `tests/dmtest/game.py`. The `projectile_create` callers in the tests
  pass `ctypes.byref(ProjectileLaunch(...))`.
- [ ] **Step 5: Build and test.** `make`, then the differential gate. Expected: all pass.
- [ ] **Step 6: Commit.** `git commit -m "refactor(engine): projectile attack and launch values without globals"`

### Task 73: Projectile squares and hit results

**Files:**
- Modify: `src/engine/projectile.h`, `src/engine/projectile.c` (`port/src/engine/projectile.c:14-16, 120-145,
  227-266, 300-340, 344-400, 695-745`)
- Modify (call sites): `src/engine/group.c:637`, `src/engine/event.c` (`port/src/engine/timeline_process.c:447`),
  `src/ui/startend.c:449, 529-530`, `src/engine/projectile.c:633`

**Interfaces:**
- Produces:
  ```c
  /* An explosion appears at a square; fireball and lightning damage aim at the target square (the same square
     for a plain explosion). */
  void explosion_create(uint16_t explosion_thing, uint16_t attack, Location at, Location target, int16_t cell);
  ```
  File-static in `projectile.c`:
  ```c
  /* Results of a projectile hit on a creature cell. */
  typedef enum {
      HIT_RESULT_NONE,     /* no creature in the cell, or a non-material creature */
      HIT_RESULT_HIT,      /* the projectile hit */
      HIT_RESULT_ABSORBED, /* the black flame ate the fireball */
  } HitResult;
  typedef struct { uint16_t explosion; uint16_t attack; } FlaskExplosion;
  ```
  `SQUARE_COMBO_MIN`, `SQUARE_COMBO_TARGET_MASK`, `SQUARE_COMBO_SHIFT`, `x_combo` and `y_combo` no longer exist.

- [ ] **Step 1: Decode the combined squares.** At 72e24b5 a combined square is `x_combo = (dst_x + 1) << 8 |
  src_x`, `y_combo = dst_y << 8 | src_y` (`projectile.c:739-740`); the decode gives the square `x = dst`, the
  target `target = src` (`projectile.c:131-136, 362-367`); a plain value is the same square for both. The only
  producer is `projectile_is_killed_by_impact`; every other caller passes a plain square.
- [ ] **Step 2: Locations.** `explosion_create`, `projectile_has_impact_occurred` (static after Task 82, keep
  it public here), `create_impact_explosion` and `hit_creature` take `Location at, Location target` instead of the
  combos; `projectile_is_killed_by_impact` passes `(Location){dst_x, dst_y}` and `(Location){src_x, src_y}` for
  the destination pass and the source square twice for the first pass. Callers with a plain square pass it twice.
  Delete the three `SQUARE_COMBO_*` constants.
- [ ] **Step 3: Hit result.** `hit_creature` returns `HitResult` (0, 1, 2 today, `projectile.c:229-231`); its
  caller (`projectile.c:377-381`) tests `!= HIT_RESULT_NONE` and `== HIT_RESULT_HIT`. `hit_creature` keeps at most
  five parameters: `possessing_group` and `is_explosion` move into a file-static `Hit` struct of the caller if the
  count is above five after Step 2.
- [ ] **Step 4: Flask.** `get_flask_explosion(thing, &attack)` (`projectile.c:304`) returns `FlaskExplosion`
  (`explosion` 0 for no flask); `projectile.c:352` reads both.
- [ ] **Step 5: Build and test.** `make`, then the differential gate. Expected: all pass.
- [ ] **Step 6: Commit.** `git commit -m "refactor(engine): projectile squares as locations and hit results as an enum"`

### Task 74: Sensor and event results

**Files:**
- Modify: `src/engine/sensor.h` (`port/src/engine/sensor.h:13-18`), `src/engine/sensor.c`
  (`port/src/engine/sensor.c:19-30, 55-98, 155-275, 440-450`)
- Modify: `src/engine/event.c` (`port/src/engine/timeline_process.c:39-46, 84, 139`) and `src/engine/event.h`
  (`event_is_door_destroyed_by_attack`, `port/src/engine/timeline_process.h:140`)
- Modify (call sites): `src/engine/projectile.c:276, 299, 558`, `src/engine/action.c:486`,
  `src/engine/sensor.c:113, 467`
- Modify: `tests/*/test_csb_rules.py:52, 189`, `tests/*/test_move.py:160-164`

**Interfaces:**
- Produces:
  ```c
  /* Adds a square event on the current map. */
  void sensor_add_event(int16_t event_type, Location at, int16_t cell, int16_t effect, uint32_t time);
  /* A door hit by an attack; magic is true for a spell. */
  bool event_is_door_destroyed_by_attack(Location at, int16_t attack, bool magic, uint16_t ticks);
  ```
  (72e24b5: `(int16_t x, int16_t y, int16_t attack, int16_t magic_attack, uint16_t ticks)`,
  `port/src/engine/timeline_process.c:374`.) File-static in
  `sensor.c`:
  ```c
  /* Results of a wall sensor test for one click. */
  typedef enum {
      CLICK_RESULT_IGNORED,   /* the sensor does nothing for this click */
      CLICK_RESULT_MATCH,     /* the condition holds: the sensor triggers (a hold sensor sets) */
      CLICK_RESULT_NO_MATCH,  /* the condition fails (a hold sensor clears) */
  } ClickResult;
  typedef struct {
      ClickResult result;
      uint16_t thing; /* the thing where the sensor loop goes on */
  } ClickTest;
  /* The rotation effect that a sensor asked for in this pass. */
  static struct {
      int16_t effect; /* -1: none */
      Location at;
      int16_t cell;
  } rotation = {.effect = -1};
  ```
  `g_sensor_rotation_effect`, `_map_x`, `_map_y`, `_cell` and `CLICK_IGNORED` no longer exist.

- [ ] **Step 1: `click_test`.** Today it returns -1, 0 or 1 and rewrites the caller's loop cursor
  (`sensor.c:184, 207-226`; caller `269-277`: 0 triggers, 1 skips, a hold sensor maps 0 to set and 1 to clear).
  It becomes `static ClickTest click_test(uint16_t thing, uint16_t previous, Location at, int16_t remaining)`;
  -1 is `CLICK_RESULT_IGNORED`, 0 `CLICK_RESULT_MATCH`, 1 `CLICK_RESULT_NO_MATCH`; the caller sets
  `t = test.thing` and maps the result as before.
- [ ] **Step 2: Rotation state.** Replace the four globals (`sensor.c:19-20`, written `59-62, 97, 446-449`, read
  `68-79`; no reader outside `sensor.c`: grep) by the static struct above; `sensor_reset` (`sensor.c:446-449`)
  sets `rotation.effect = -1`, `rotation.at = (Location){0, 0}`, `rotation.cell = 0`.
- [ ] **Step 3: `sensor_add_event`.** Takes `Location at`; callers `sensor.c:113, 467`, `projectile.c:276`, and the
  tests (`test_csb_rules.py:52` argtypes `[c16, Location, c16, c16, u32]`, `:189`; `test_move.py:160, 164`).
- [ ] **Step 4: Door attack.** The `magic_attack` parameter of `event_is_door_destroyed_by_attack` becomes
  `bool magic` (callers pass 0 or 1 today: `projectile.c:299, 558`, `action.c:486`); the square becomes
  `Location at`.
- [ ] **Step 5: Material group.** `is_material_group_on(x, y, &attributes)` (`timeline_process.c:39`) returns
  `typedef struct { bool material; uint16_t attributes; } GroupMaterial;` (file-static); callers `84, 139`.
- [ ] **Step 6: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_csb_rules.py' 'tests/*test_move.py' 'tests/golden/test_*.py')`.
  Expected: all pass.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): sensor click results and rotation state without globals"`

### Task 75: Save results

**Files:**
- Modify: `src/engine/savegame.h` (`port/src/engine/savegame.h:10-38`), `src/engine/savegame.c`
- Modify (call sites): `src/ui/startend.c:124-132, 181, 205-210`, `src/ui/advanced.c:123-133`,
  `src/platform/control.c:1200-1205`
- Modify: the tests that call the save functions (`grep -rln "savegame_" tests`: `test_save.py`,
  `test_csb_ending.py`, `test_select.py`, `test_csb_adventure.py`, `test_csb_save.py`, `test_golden_save.py`,
  `test_csb_magic_map.py`, `test_modes.py`), `docs/formats/save-format.md` (C API section), `docs/todo.md` (the
  `savegame_quick_save` line of "Truth values that are still int16_t")

**Interfaces:**
- Produces:
  ```c
  /* Results of a load. */
  typedef enum {
      SAVEGAME_DAMAGED = -1,
      SAVEGAME_MISSING = 0,
      SAVEGAME_LOADED = 1,
  } SavegameResult;
  /* Results of the CSB prison (make new adventure). */
  typedef enum {
      SAVEGAME_ADVENTURE_FAILED,
      SAVEGAME_ADVENTURE_MADE,
      SAVEGAME_ADVENTURE_NO_CHAMPIONS,
      SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE,
  } SavegameAdventureResult;
  bool savegame_save(const char *dir);
  bool savegame_quick_save(const char *dir);
  SavegameResult savegame_load(const char *dir);
  SavegameResult savegame_load_from(const char *dir, const char *name, const char *backup);
  SavegameResult savegame_quick_load(const char *dir);
  SavegameAdventureResult savegame_make_adventure(const char *dir, const char *mini_path);
  ```
  The values keep their numbers (the tests and the protocol compare them). `ADVENTURE_FAILED`,
  `ADVENTURE_MADE`, `ADVENTURE_NO_CHAMPIONS`, `ADVENTURE_NAMES_NOT_UNIQUE` (`savegame.h:11`) are renamed: their
  prefix collides with `ADVENTURE_DM`/`ADVENTURE_CSB` of `data/adventure.h`.

- [ ] **Step 1: Types.** Apply the declarations above; `savegame_save` and `savegame_quick_save` return `true`
  where they returned 1 and `false` where they returned 0. Update the header comments ("Returns true on
  success").
- [ ] **Step 2: Callers.** `startend.c:124-132` switches over the renamed values; the other callers keep their
  comparisons.
- [ ] **Step 3: Tests.** The `restype` of `savegame_save` becomes `ctypes.c_bool`; `== 1` on its result becomes
  `is True` (the same check); the hand-written `ADVENTURE_MADE, ... = 1, 2, 3` of `test_csb_adventure.py:22` and
  the `LOADED`, `MISSING`, `DAMAGED` values come from `dmtest.constants` (`C.SAVEGAME_ADVENTURE_MADE`,
  `C.SAVEGAME_LOADED`); `dmtest.constants` reads `enum` result types (R12.1).
- [ ] **Step 4: Build and test.** `make`, then
  `uv run pytest -q -n auto $(git ls-files 'tests/*test_save.py' 'tests/*test_csb_*.py' 'tests/*test_select.py' 'tests/*test_modes.py' 'tests/golden/test_*.py')`.
  Expected: all pass.
- [ ] **Step 5: Commit.** `git commit -m "refactor(engine): save results as bool and named result types"`

### Task 76: Pending sounds and loudness

**Files:**
- Modify: `src/engine/sound.h` (`port/src/engine/sound.h:42-46`), `src/engine/sound.c`
  (`port/src/engine/sound.c:40-41, 120-205`)
- Modify: `tests/*/test_sound.py:144-145` and the tests of `sound_compare_loudness` (grep)

**Interfaces:**
- Produces:
  ```c
  /* A sound that plays after the tick, with its volume per side. */
  typedef struct {
      int16_t sound; /* -1: empty slot */
      uint8_t volumes[SOUND_SIDE_COUNT];
  } PendingSound;
  extern PendingSound g_pending_sounds[SOUND_PENDING_SLOTS]; /* exported for the tests */
  /* Results of a loudness comparison. */
  typedef enum {
      LOUDNESS_LOWER = -1,
      LOUDNESS_SAME = 0,
      LOUDNESS_HIGHER = 1,
  } LoudnessResult;
  LoudnessResult sound_compare_loudness(const uint8_t *a, const uint8_t *b); /* exported for the tests */
  ```
  (`SIDE_COUNT` is file-static in `port/src/engine/sound.c:16`; the header gets `#define SOUND_SIDE_COUNT 2` and
  `sound.c` uses it.)
  `g_pending_sound_volumes` no longer exists.

- [ ] **Step 1: Check the values.** Read `sound_compare_loudness` (`sound.c`, grep) and confirm that it returns
  exactly -1, 0 or 1; the callers compare with 1 and 0 (`sound.c:162-173`). Keep the numbers in the enum.
- [ ] **Step 2: The struct.** Replace the two arrays by `g_pending_sounds` of `PendingSound`; `set_pending`,
  `queue_pending`, `sound_play_pending` use the fields.
- [ ] **Step 3: Tests.** `test_sound.py:144-145` reads `(PendingSound * 2).in_dll(lib, "g_pending_sounds")` with
  `PendingSound` in `tests/dmtest/game.py`; the loudness test compares with `C.LOUDNESS_HIGHER` etc.
- [ ] **Step 4: Build and test.** `make`, then `uv run pytest -q $(git ls-files 'tests/*test_sound.py')`.
  Expected: all pass.
- [ ] **Step 5: Commit.** `git commit -m "refactor(engine): pending sounds as structs, loudness as a result type"`

#### Tail

Shared files: the tail runs after the merge of the three streams, one task after the other; each task may touch
every engine file and its callers in `ui/`, `platform/`, `tests/`, `docs/`.

### Task 77: Creature damage outcome as a return value

**Files:**
- Modify: `src/engine/group.h` (`port/src/engine/group.h:72-76, 208`), `src/engine/group.c`
  (`port/src/engine/group.c:76, 608-660, 1800-1830`), `src/engine/projectile.h`, `src/engine/projectile.c`
  (`port/src/engine/projectile.c:117, 257, 351, 409-419, 729`)
- Modify: `tests/*/test_creatures.py` (`group_get_damage_all_creatures_outcome` calls: grep)

**Interfaces:**
- Produces:
  ```c
  /* Results of a damage to the creatures of a group. */
  typedef enum {
      OUTCOME_NO_CREATURE_KILLED,
      OUTCOME_SOME_CREATURES_KILLED,
      OUTCOME_ALL_CREATURES_KILLED,
  } DamageOutcome;
  DamageOutcome group_get_damage_all_creatures_outcome(uint16_t group, Location at, int16_t attack, bool notify);
  /* The hits of every projectile in a cell; a creature impact stops when the whole group died. */
  typedef struct {
      int16_t count;
      DamageOutcome outcome;
  } ImpactCount;
  ImpactCount projectile_get_impact_count(int16_t impact_type, Location at, int16_t cell);
  ```
  (the values 0, 1, 2 of `port/src/engine/group.h:73`.) `g_creature_damage_outcome` no longer exists.

- [ ] **Step 1: The flow.** At 72e24b5 `g_creature_damage_outcome` is written at `projectile.c:117`
  (`explosion` fire damage), `257` (`hit_creature`), `351` (`projectile_has_impact_occurred` start), `409`
  (`projectile_get_impact_count` start), and read at `projectile.c:419, 729`, `group.c:1802, 1826, 1828`. Each
  read follows a call of `projectile_get_impact_count` or `projectile_has_impact_occurred` in the same function;
  the write at 117 is not read before the next reset (check: the fire damage function is not called between a
  reset and a read).
- [ ] **Step 2: Return values.** `group_get_damage_creature_outcome` and
  `group_get_damage_all_creatures_outcome` return `DamageOutcome`; `projectile_has_impact_occurred` returns
  `bool` and writes the outcome to a `DamageOutcome *` parameter; `projectile_get_impact_count` returns
  `ImpactCount`; `projectile_is_killed_by_impact` keeps a local. `group.c:1801-1802, 1825-1828` read
  `.count` and `.outcome`. The write at 117 becomes a local that is dropped (if Step 1 confirms no reader).
- [ ] **Step 3: Build and test.** `make`, then the differential gate. Expected: all pass.
- [ ] **Step 4: Commit.** `git commit -m "refactor(engine): the creature damage outcome is a return value"`

### Task 78: Sound requests: mode and delay

**Files:**
- Modify: `src/engine/sound.h` (`port/src/engine/sound.h:14-39`), `src/engine/sound.c`
  (`port/src/engine/sound.c:176-195`)
- Modify (call sites, 41 lines at 72e24b5): `grep -rn "sound_request_play(" src` in `engine/group.c` (12),
  `engine/action.c` (7), `engine/event.c` (5), `engine/projectile.c` (4), `engine/sensor.c` (3),
  `engine/champion.c` (2), `engine/move.c` (2), `ui/startend.c` (3), `ui/command.c` (2); and the group
  possession functions with a `sound_mode` parameter (`port/src/engine/group.c:452-525`)
- Modify: the tests that call `sound_request_play` (2 calls: grep)

**Interfaces:**
- Produces:
  ```c
  /* When a requested sound plays. */
  #define SOUND_MODE_NONE       (-1) /* no sound (possession drops of a moving group) */
  #define SOUND_MODE_NOW        0
  #define SOUND_MODE_AFTER_TICK 1
  #define SOUND_MODE_EVENT      2    /* a play sound event after the delay */
  /* Plays a sound of a square now, after the tick, or as a timeline event after delay ticks (SOUND_MODE_EVENT
     only; 0 for the other modes). */
  void sound_request_play(int16_t sound, Location at, int16_t mode, uint16_t delay);
  ```
  `SOUND_MODE_ONE_TICK_LATER` no longer exists; each of its uses becomes `SOUND_MODE_EVENT, 1`.

- [ ] **Step 1: The rule.** `sound_request_play` adds the event at `g_game_time + delay` for `SOUND_MODE_EVENT`
  (today `g_game_time + (mode - 1)` for mode 2, `sound.c:183-186`). The other modes ignore `delay`.
- [ ] **Step 2: Callers.** Each call passes `(Location){x, y}`, the mode and 0; the 10 `SOUND_MODE_ONE_TICK_LATER`
  uses pass `SOUND_MODE_EVENT, 1`. `champion.c:967` passes `SOUND_MODE_NOW` instead of `0`.
- [ ] **Step 3: Possession drops.** `group_drop_creature_fixed_possessions` and `group_drop_group_possessions`
  take the mode (`SOUND_MODE_NONE` replaces the test `sound_mode >= 0`, `group.c:504, 523`) and pass the delay 1
  for `SOUND_MODE_EVENT`, 0 else.
- [ ] **Step 4: Comments.** The header comment "Mode: 0 plays now, 1 plays after the tick" above the sound list
  (`sound.h:14`) goes (the mode block says it).
- [ ] **Step 5: Build and test.** `make`, then the differential gate and
  `uv run pytest -q $(git ls-files 'tests/*test_sound.py')`. Expected: all pass.
- [ ] **Step 6: Commit.** `git commit -m "refactor(engine): sound requests take a mode and a separate delay"`

### Task 79: Indices instead of ordinals

**Files:**
- Modify: every file that names an ordinal (`grep -rlw "ordinal\|[a-z_]*_ordinal[a-z_]*" src tests docs`): at
  72e24b5 `engine/{action,champion,game,group,inventory,magic,projectile,timeline_process}.c`,
  `engine/{action,champion,group,inventory,map}.h`, `data/tables.h:76`, `ui/{advanced,command,gameloop,startend,
  ui_champion,ui_menus,ui,view}.c`, `ui/view.h:41-43`, `platform/{control,state_json}.c`, `tests/test_{actions,
  champion,csb_rules,gameloop,golden_rules,modes,panels,tables,ui}.py`
- Modify: `engine/record.h`, `engine/record.c` (the two ornament accessors of package E)

**Interfaces:**
- Produces (index -1 for none everywhere):

  | 72e24b5 name | New name | Uses at 72e24b5 |
  |---|---|---|
  | `g_inventory_champion_ordinal` | `g_inventory_champion_index` | 63 in 13 files |
  | `g_candidate_champion_ordinal` | `g_candidate_champion_index` | 34 in 11 files |
  | `g_acting_champion_ordinal` | `g_acting_champion_index` | 30 in 8 files |
  | `group_get_creature_ordinal_in_cell` (`group.c:166`) | `group_get_creature_in_cell` | `ui/command.c`, `projectile.c`, `group.c` |
  | `group_get_melee_target_creature_ordinal` (`group.c:183`) | `group_get_melee_target` | `action.c` |
  | `group_get_party_scent_ordinal` (`group.c:837`) | `group_get_party_scent` | `ui/view.c`, `group.c` |
  | `group_get_smelled_party_direction_ordinal` (static, `group.c:847`) | `get_smelled_party_direction` | `group.c` |
  | `group_get_first_possible_movement_direction_ordinal` (static, `group.c:928`) | `get_first_possible_direction` | `group.c` |
  | `get_random_ornament_ordinal` (static, `ui/view.c`) | `get_random_ornament` | `ui/view.c` |
  | `rec_sensor_ornament_ordinal` (`map.h:199`) | `rec_sensor_ornament` (returns stored - 1) | `sensor.c`, `state_json.c:680-687`, `ui/view.c` |
  | `door_ornament_ordinal` (`map.h:280`) | `rec_door_ornament` (returns stored - 1) | `ui/view.c` |
  | `view_get_wall_ornament_index(uint16_t ordinal)` (`view.h:43`) | takes `int16_t ornament` (index) | `state_json.c:687`, `ui/view.c` |
  | `inventory_set_open_champion(int16_t ordinal)` (`inventory.h:66`) | takes `int16_t champion_index` | grep |
  | `CreatureInfo.attack_sound_ordinal` (`data/tables.h:76`, a game table byte) | `attack_sound_byte` (comment: "0: none, else the sound index + 1, as stored") | `tests/test_tables.py:27, 161` |

  Static and public functions that return an ordinal return the index (-1 none). The names stay at most 32
  characters.

- [ ] **Step 1: Inventory of the sites.** Save `grep -rn "ordinal" src tests` to the SDD workspace. At 72e24b5:
  42 `ordinal - 1` lines (27 with `_ordinal - 1`), 17 `+ 1 == g_..._ordinal` tests (`ui/command.c:318, 370`,
  `ui/gameloop.c:203, 496`, `ui/ui_champion.c:127, 134, 156, 169, 317`, `engine/inventory.c:290, 328, 358`,
  `engine/champion.c:782, 863, 902, 1115`, `engine/timeline_process.c:427`, `engine/magic.c:184`), 61 locals or
  parameters named `ordinal`.
- [ ] **Step 2: The three globals.** Initial value -1 (was 0: `action.c:92`, `champion.c:99`, `inventory.c:56`;
  `game.c:93` resets the candidate to -1). Rewrite each use: `!g_x_ordinal` → `g_x_index < 0`; `g_x_ordinal` as
  a truth value → `g_x_index >= 0`; `g_x_ordinal - 1` → `g_x_index`; `i + 1 == g_x_ordinal` → `i == g_x_index`;
  an assignment of `i + 1` → `i`; of 0 → -1. Task 80 then moves them into `g_session`.
- [ ] **Step 3: The functions** of the table. A caller that tested `!= 0` tests `>= 0`; a caller that subtracted 1
  drops the subtraction.
- [ ] **Step 4: Record and table ordinals.** `rec_sensor_ornament` and `rec_door_ornament` return the stored
  value - 1 (the record keeps the ordinal: R3.3); callers that tested the ordinal for 0 test `< 0`; callers that
  passed it on to `view_get_wall_ornament_index` pass the index. Rename the `CreatureInfo` field and its two test
  uses.
- [ ] **Step 5: The protocol stays.** `state_json.c:430-435, 474, 494-496` and the `control.c` lines of Step 1
  write the same JSON values as before (the JSON already holds indices: `"champion": ordinal - 1`).
- [ ] **Step 6: Tests.** `tests/conftest.py`, `dmtest.game` and the tests of the files list read the new names;
  an assertion `== 1` on an ordinal becomes `== 0`, `== 0` becomes `== -1`, `== 2` becomes `== 1`; a write of
  `1` becomes `0`. The checked facts stay the same.
- [ ] **Step 7: Docs.** `grep -rln "ordinal" docs | grep -v superpowers`; every hit that names a renamed symbol
  uses the new name and says "index, -1 for none".
- [ ] **Step 8: Build and test.** `make`, `make windows`, then `make test`. Expected: all pass, 0 warnings; the
  golden protocol snapshot is unchanged. `grep -rn "_ordinal\|_plus_1" src` finds nothing.
- [ ] **Step 9: Allowlist.** Delete the `ordinal ... open: epic 6` lines of `tests/standard/allowlist.txt`;
  `uv run pytest -q tests/standard -k ordinal` passes.
- [ ] **Step 10: Commit.** `git commit -m "refactor: indices instead of ordinals"`

### Task 80: The session struct

**Files:**
- Modify: `src/engine/game.h`, `src/engine/game.c` (`port/src/engine/game.c:38, 44-45, 62-100`),
  `src/engine/savegame.h:13-14`, `src/engine/savegame.c:46-47, 80-81, 282, 347-348, 518-546, 589-609`,
  `src/engine/action.h:60, 62`, `src/engine/inventory.h:49-50`, `src/engine/champion.h:68`,
  `src/engine/event.h` (`g_game_won`, `port/src/engine/timeline_process.h:121`), `src/engine/move.h:20`
- Modify (readers): every file that names a moved global (counts at 72e24b5: `g_party_is_sleeping` 31 in 12
  files, `g_party_dead` 11 in 8, `g_game_won` 13 in 8, `g_cheat_*` 19 in 5, `g_saved_game_exists` 12 in 3,
  `g_last_save_game_time` 7, `g_last_load_game_time` 8, `g_action_damage` 13 in 4, `g_new_party_map_index` 13 in
  4, `g_game_mode` 16 in 6, the three champion indices of Task 79)
- Modify: `tests/conftest.py` (`BOOL_GLOBALS`, `global_type`), `tests/dmtest/game.py` (the `Session` structure,
  `Game.value`), the tests that read these globals (`grep -rln "g_party_is_sleeping\|g_party_dead\|g_game_won\|
  g_cheat_\|g_saved_game_exists\|g_last_load_game_time\|g_action_damage\|g_new_party_map_index\|g_game_mode\|
  _champion_index\|g_leader_empty_handed" tests`), `docs/rules/game-state.md`

**Interfaces:**
- Produces (in `engine/game.h`):
  ```c
  /* The state of one run of the game: a new game or a load resets it, the save file does not hold it. */
  typedef struct {
      int16_t mode;                     /* GAME_MODE_*; a load keeps it */
      bool party_sleeping;
      bool party_dead;
      bool won;
      bool cheat_light;                 /* the view draws in full light; the creature rules keep the real light */
      bool cheat_immortal;              /* the health of a champion stops at 1 */
      bool cheat_food;                  /* food and water do not go down */
      bool saved_game_exists;           /* set after a save or a load: the endgame offers a restart */
      uint32_t last_save_time, last_load_time;
      int16_t candidate_champion_index; /* -1: no candidate */
      int16_t inventory_champion_index; /* -1: no inventory open */
      int16_t acting_champion_index;    /* -1: none */
      int16_t action_damage;            /* 0: none, -1 can't reach, -2 need ammo */
      int16_t new_party_map;            /* -1: no map change pending */
  } Session;
  extern Session g_session;
  ```
  Definition in `game.c`:
  `Session g_session = {.mode = GAME_MODE_VANILLA, .candidate_champion_index = -1, .inventory_champion_index = -1,
  .acting_champion_index = -1, .new_party_map = -1};`

- [ ] **Step 1: Same resets.** Each existing reset of a moved global stays where it is and writes the field:
  `game_start_new` (`game.c:73-74` sleeping and dead), `game_reset_session` (`game.c:93-98`), the load
  (`savegame.c:518-520`, the keep-and-restore blocks `533-546`, `594-598`, `604-609`), `action_reset`,
  `move_reset`. Do not add a reset that did not exist: a field that no function reset keeps its value as the
  global did.
- [ ] **Step 2: Move the globals.** Delete the definitions and externs of the moved globals; rewrite every use
  as `g_session.<field>` (a script per name: `perl -pi -e 's/\bg_party_is_sleeping\b/g_session.party_sleeping/g'`
  over `src`, one name at a time, then build).
- [ ] **Step 3: `g_leader_empty_handed`.** Check whether it always equals `g_leader_hand_object == THING_NONE`:
  writes at `inventory.c:57` (initial true, `g_leader_hand_object` starts `THING_NONE` at `game.c:37`), `159`,
  `170`, `champion.c:1152`. Add a temporary check in `inventory.c` and `champion.c` after each write and at the
  entry of `savegame_save`:
  `if (g_leader_empty_handed != (g_leader_hand_object == THING_NONE)) game_fatal(...)` with an unused code, run
  `make lib` and `make test` once (the author runs the full suite for this check only), then remove the check.
  Pass: replace `g_leader_empty_handed` by an inline `inventory_is_leader_empty_handed()` that returns
  `g_leader_hand_object == THING_NONE`, at all 26 uses (grep). Fail: keep the global, add the comment
  `/* differs from g_leader_hand_object == THING_NONE while <the case found> */` and report the case.
- [ ] **Step 4: Tests.** `tests/dmtest/game.py` gets the `Session` ctypes structure with the fields in C order and
  types (`c_int16`, `c_bool`, `c_uint32`); `Game.value("g_inventory_champion_index")` and the like become
  `game.session.inventory_champion_index` (`Session.in_dll(lib, "g_session")`). `BOOL_GLOBALS` loses the moved
  names. The assertions keep their values.
- [ ] **Step 5: Docs.** `docs/rules/game-state.md`: one table of the session fields with the reset points of
  Step 1; the stale globals of `docs/game-state.md:196` (PRD current state) go.
- [ ] **Step 6: Build and test.** `make`, `make windows`, `make test`. Expected: all pass, 0 warnings, the golden
  save and protocol snapshots unchanged.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): per-run flags live in g_session"`

### Task 81: One type per game value

**Files:**
- Modify: the engine headers and their `.c` files that the `type` check of `tests/standard/test_standard.py`
  (Epic 3) reports; callers in `ui/` and `platform/` that the build then reports.

**Interfaces:**
- Consumes: the type table of `docs/coding-standard.md` 5.1 (`champion_index`, `cell`, `direction`, `map`, `slot`,
  `x`, `y`: `int16_t`; `thing`, `ticks`: `uint16_t`; game time `uint32_t`).
- Produces: every engine prototype uses the table type for these parameter names; `g_party_champion_count` is
  `int16_t` (it is compared with `int16_t` indices, `champion.c:264`).

- [ ] **Step 1: The list.** `uv run pytest -q tests/standard -k "type or bool"` and save the failures. At
  72e24b5 `champion_index` is `uint16_t` in 24 declarations (`champion_resurrect`, `champion_add_skill_experience`,
  `champion_kill`, `champion_drop_all_objects`, `champion_add_redraw_flags`, the nine `action_*`, two
  `inventory_*`, five `magic_*`, `group_get_champion_damage`, `event_enable_champion_action`,
  `event_hide_damage_received`), `slot` 3 / 3, `cell` 13 / 3, `direction` 8 / 2 / 1 `uint8_t`, `ticks` 1 / 4 / 1
  (engine review F18).
- [ ] **Step 2: Change the types** in the prototypes and the definitions, and the local variables that carry the
  value. A comparison that changes meaning with the sign (a `uint16_t` compared with `-1`, a `>= 0` test of an
  unsigned value) is rewritten to the same truth value; list each in the commit message. A `uint8_t` record byte
  stays `uint8_t` in the record accessor and converts at the accessor.
- [ ] **Step 3: Truth values.** The `bool` check failures in `engine/` (locals assigned from a comparison,
  `int16_t` flags: `group.c` `absolute`, `points_to_itself`, `all_killed`, `some_killed`, `spell`, `both_visible`,
  `retried`, `GroupTurn.moved/was_attacking/archenemy`; `projectile.c` `single`, `solid`, `weakened_to_nothing`,
  `hit`; `champion.c` `temporary`; `move.c` `keep_larger`, `found`; `action.c` `side`; `recruit.h` `male`)
  become `bool`, unless an earlier task removed them.
- [ ] **Step 4: The ctypes declarations** of the changed functions in `tests/` follow (argtypes `c16` instead
  of `u16`).
- [ ] **Step 5: Build and test.** `make`, `make windows`, `make test`. Expected: all pass, 0 warnings.
- [ ] **Step 6: Allowlist.** Delete the `type` and `bool` lines with `open: epic 6` for `engine/`; the two checks
  pass for `engine/`.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): one type per game value"`

### Task 82: Dead API, test markers, private engine arrays

**Files:**
- Modify: the engine headers and `.c` files that the `dead-api` check reports; `src/ui/view.c`
  (`port/src/ui/view.c:674-675, 894, 2338`); `src/engine/projectile.h`, `src/engine/projectile.c`;
  `src/engine/game.h` (or the headers of package E that hold `g_active_groups`, `g_timeline_events`)

**Interfaces:**
- Consumes: `group_get_active` (Task 67).
- Produces:
  ```c
  /* The flight direction of a projectile (from its move event). */
  int16_t projectile_get_flight_direction(uint16_t projectile);
  ```
  `g_active_groups` and `g_timeline_events` are not declared in a header that `ui/` or `platform/` includes
  (they stay global inside the engine for `savegame.c` and the timeline module, declared in an `_internal.h`
  header of their module or in the module header with the marker if the tests read them).

- [ ] **Step 1: The list.** `uv run pytest -q tests/standard -k dead` and save the report. At 72e24b5 about 70
  engine functions have no caller outside their file (engine review F44 lists them) and 17 are test-only:
  `dungeon_decompress`, `dungeon_validate`, `save_block_xor`, `recruit_decoded_value`, `recruit_parse_text`,
  `sound_compare_loudness`, `sound_demo_play`, `sound_get_sample`, `sound_get_volume`, `message_move_cursor`,
  `champion_get_wound_defense`, `group_get_generated`, `group_get_champion_damage`,
  `group_is_destination_visible_from_source`, `magic_print_spell_failure_message`, `projectile_get_impact_attack`,
  `event_process_wall`. Recheck each against `tests/` and `tools/` (a name used through `getattr` or a string
  list counts as a test use: `grep -rn "\"<name>\"" tests`).
- [ ] **Step 2: Static.** A function or global with no use outside its file and no test use becomes `static`; its
  prototype leaves the header; its comment moves above the definition (standard 9.1); its name loses the module
  prefix (standard 4.2) when the new name stays unique in the file. Static inline accessors in headers that only
  their own `.c` file uses move into that file.
- [ ] **Step 3: Markers.** A function or global used only by the tests keeps its prototype with
  `/* exported for the tests */` on the declaration line.
- [ ] **Step 4: View access.** `view.c:674-675` calls `group_get_active(thing)` (the bounded read replaces the
  check `index < g_max_active_group_count`); `view.c:894, 2338` call `projectile_get_flight_direction(thing)`,
  which reads the event of `rec_projectile_event_index` (`move.c:238` does the same read). `move.c:238` uses it
  too. Then remove `g_active_groups`, `g_active_group_count`, `g_max_active_group_count`, `g_timeline_events`
  from the headers that `ui/` includes, if no `ui/`, `platform/` or test use remains; a test use keeps the
  declaration with the marker.
- [ ] **Step 5: Build and test.** `make`, `make windows`, `make test`. Expected: all pass, 0 warnings.
- [ ] **Step 6: Allowlist.** Delete the `dead-api` and `static-prefix` lines with `open: epic 7` for `engine/`.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): static for file-local functions, markers for test exports"`

### Task 83: Names from the game

**Files:**
- Modify: `src/engine/group.h` (`port/src/engine/group.h:57-63`), `src/engine/action.h`
  (`port/src/engine/action.h:10-40`), the `SLOT_*` block (`port/src/engine/game.h:45-66`, after package E in
  `engine/champion.h`), `src/data/tables.h` (skill block, `WeaponInfo`), `src/engine/action.c:24-32`,
  `src/engine/champion.c:96`, `src/engine/sensor.c:14`, every `event->b` / `event->c` reader outside the timeline
  module and `savegame.c`
- Modify: `docs/rules/rules-creatures.md:52-58`, `docs/reference/kaos-symbols.csv` (the symbol table lines of the
  renamed constants, if any)

**Interfaces:**
- Produces: `BEHAVIOR_WANDER_NO_APPROACH` (was `BEHAVIOR_USELESS_2`), `BEHAVIOR_WANDER_NO_SMELL` (was
  `BEHAVIOR_USELESS_3`); the two stab names; one `WEAPON_CLASS_*` block in `data/tables.h`; `SKILL_STEAL 8`.

- [ ] **Step 1: Behaviors.** No name for behavior 2 and 3 exists in `docs/kaos-symbols.csv` or `reference/`
  (searched at plan time; ReDMCSB calls both "useless"). The rule names them: `docs/rules-creatures.md:55` "2, 3:
  like wander, but does not follow the smell; behavior 2 does not approach". Rename: 2 →
  `BEHAVIOR_WANDER_NO_APPROACH`, 3 → `BEHAVIOR_WANDER_NO_SMELL`; the comparisons `> BEHAVIOR_USELESS_3`
  (`group.c:340`) keep the numeric order (comment above the block: "values in record order; the rules compare
  them"). Update `rules-creatures.md:55`.
- [ ] **Step 2: Stab actions.** Actions 9 and 14 both show "STAB" in the action names of graphic 699
  (`action.h:9`). Find which weapons offer each: print, with the library and `tables_get_*` (`uv run python` over
  `dmtest.lib`), the weapon icons whose action set holds action 9 and those that hold 14. Name each
  `ACTION_STAB_<WEAPON>` after the smallest weapon group that offers it (for example `ACTION_STAB_DAGGER`); record
  the two names and the evidence in the ledger. If both sets overlap, name them by their action table row
  (skill, defense): report the names.
- [ ] **Step 3: Slots.** The `SLOT_*` block lists the slots in the order of the champion record, with one comment
  line above it ("Slots of Champion.slots, in record order."); `SLOT_POUCH_2` after `SLOT_POUCH_1` only if that is
  the record order (check the values; the block is sorted by value).
- [ ] **Step 4: Weapon classes.** `CLASS_SWING_WEAPON ... CLASS_AMMUNITION_END` (`champion.c:96`) and
  `WEAPON_CLASS_*` (`action.c:24-32`) describe `WeaponInfo.weapon_class`: one `WEAPON_CLASS_*` block in
  `data/tables.h` beside `WeaponInfo`, the two local blocks go.
- [ ] **Step 5: Steal skill.** `#define SKILL_STEAL 8` in the skill block of `data/tables.h`;
  `SENSOR_EXPERIENCE_SKILL` (`sensor.c:14`) uses it.
- [ ] **Step 6: Event words.** `grep -rn "event->b\|event->c\|\.b = \|\.c = \|e->b\|e->c" src/engine` outside the
  timeline module and `savegame.c`: each read uses an accessor of `engine/event.h` or `engine/timeline.h`
  (`event_square_x/_y`, the projectile and group accessors exist; add a missing one beside them). Writes of a new
  event use the constructor helpers (`event_square`, `projectile_event_c`).
- [ ] **Step 7: Build and test.** `make`, `make test`. Expected: all pass.
- [ ] **Step 8: Commit.** `git commit -m "refactor(engine): constant names from the game rules"`

### Task 84: Engine constants: defines, notation, unused names

**Files:**
- Modify: every header and `.c` file of `src/engine/`
- Create (not committed): `$SCRATCH/engine_constants.py` (SCRATCH: the SDD workspace)

**Interfaces:**
- Produces: no anonymous `enum` in `src/engine/`; every constant is `#define NAME value` with the value written
  out; every block has one comment line and one notation; the result types of this package stay `typedef enum`.

Task 24 (package C tail) has already turned every anonymous enum of `src/` into `#define` lines. Steps 1-2
run the converter only on the enums that packages D, E and F added after Task 24 (Expected: none; a hit is a defect of
that task, fix it here).

- [ ] **Step 1: The converter.** Write `$SCRATCH/engine_constants.py`: for each `enum { ... };` without a type name
  in `src/engine/*.[ch]`, it writes one `#define NAME value` per member, with the implicit values written out
  (previous value + 1; a negative value in parentheses), keeps the comment line above the enum as the block
  comment, keeps member comments as trailing comments, and leaves `typedef enum { ... } XResult;` untouched. It
  prints the blocks that mix hex and decimal (a hex literal and a decimal literal other than 0 in one block).
  Run it with `--dry-run` first and read the diff of three files (`group.h`, `champion.h`, `event.h`).
- [ ] **Step 2: Apply and format.** Run it, then `make format`, `make lib`.
- [ ] **Step 3: Notation.** Fix each printed mixed block by its kind (standard 3.4): flags, masks, layouts:
  hex with the same digit width (`WOUND_*`: `0x01 ... 0x20`; `SQUARE_HAS_THINGS 0x10`, `SQUARE_RANDOM_ORNAMENT
  0x08`); counts, ids, ticks: decimal. A block that holds both kinds splits into two blocks with a comment each.
- [ ] **Step 4: Unused constants (R6.7).** For each constant defined in `src/engine/`, count uses outside its
  definition line in `src/`, `tests/`, `tools/`: `git grep -nw <NAME> -- src tests tools | wc -l`. A constant
  with no use goes, unless its block names a game table (the comment says which table: the action names of
  graphic 699 keep `ACTION_N`, `ACTION_BLOCK`, `ACTION_HIT`). List the deleted names in the commit message.
- [ ] **Step 5: Build and test.** `make`, `make windows`, `make test`. Expected: all pass, 0 warnings.
- [ ] **Step 6: Allowlist.** Delete the `enum`, `block`, `notation`, `hex`, `decimal` lines with `open: epic 6`
  for `engine/`; `uv run pytest -q tests/standard` passes with the remaining lines.
- [ ] **Step 7: Commit.** `git commit -m "refactor(engine): constants as defines with one notation per block"`

---

## Package G: ui (Epic 8)

PRD: R8 (all of it). Gates: `tests/golden/view.json` (dungeon view), `tests/golden/screens.json` (A2: status boxes, inventory panel, action and spell areas), `tests/golden/rules.json` with the A3 scenarios of every rule that R8.3 moves, the differential tests.

Paths: `src/...` (package B moved `port/src/`); tests in the R1 folders (package C). Code facts cite `port/src/<path>:<line>` of commit 72e24b5. Earlier packages renamed engine names; where this section names an engine function by its PRD name (`map_step`, `direction_back`, `g_session.inventory_champion_index`, `inventory_take_hand_object`, `group_get_active`, `projectile_get_flight_direction`, `map_get_level_change_location`, `Location`, `Position`, `g_party_light_level`), use that name; if a name differs on the branch, use the branch name and report it.

Consumes from earlier packages:
- D3: `Box` and `bool box_contains(const Box *box, int16_t x, int16_t y)` in `src/data/graphics.h`.
- E1: `src/engine/record.h` accessors; Assumption: E1 gives an accessor for the creature type list and the ornament list of a map (today `g_dungeon_map_data[m][map_width - 1] + map_height`, `port/src/ui/view.c:2185-2186`). Task 85 checks it first; if E1 has none, G1 adds `const uint8_t *rec_map_creature_types(int16_t map)` to `src/engine/record.h/.c` (the ornament list follows the creature types).
- E2: `Location`, `Position`, `map_step`, `direction_back/right/left`, `g_party_light_level` (was `g_dungeon_view_palette_index`).
- F: `map_get_level_change_location` (R6.4, returns a struct), `group_get_active(thing)`, `projectile_get_flight_direction(thing)` (R7.5), `g_session` (R7.1) with `inventory_champion_index`, `acting_champion_index`, `candidate_champion_index` (-1 none).
- A2, A3: the golden screen and rule scenarios.

#### Streams and file ownership

| Stream | Owns | Shared files (small hunks) |
|---|---|---|
| G1 view | `src/ui/view.c`, `view.h`, new `view_walls.c`, `view_things.c`, `view_internal.h` | `src/engine/map.h` (G4), `src/data/ids.h`, `src/ui/automap.c` (G4), `src/control/state_json.c` or `src/platform/state_json.c` (G4) |
| G2 status, panel, menus | `src/ui/ui.c`, `ui.h`, `ui_champion.c` → `status.c`, `panel.c`, `ui_menus.c` → `menus.c`, new `status.h`, `panel.h`, `menus.h`, `src/engine/champion.c/.h` | `src/ui/command.c` (G3, G4), `src/ui/gameloop.c` (G3), `src/ui/startend.c` (G3, G4), platform and control callers |
| G3 headers | new `src/ui/frame.h/.c`, `entrance.h/.c`; `src/ui/command.h`, `gameloop.h`, `dialog.h`, `startend.h`, `select.h/.c`, `advanced.h`, `video.h/.c`, `src/platform/platform.c` | `src/ui/command.c` (G2, G4), `gameloop.c` (G2), `startend.c` (G2, G4), `anim.c`, `logo.c`, `dialog.c`, `src/platform/main.c` |
| G4 rules out of ui | `src/engine/inventory.c/.h`, `map.c/.h`, `move.c/.h`, `game.c/.h`, `group.c/.h`, `explored.c/.h`, `sound.h`, `src/ui/automap.c/.h` | `src/ui/command.c` (G2, G3), `startend.c` (G2, G3), `gameloop.c` (G3), `src/engine/map.h` (G1), state JSON writer (G1) |

Tail (after the merge): `Screen g_screen`, `ViewResult`, copied code and dead exports, the remaining types, comments and markers. These touch every ui file.

---

#### Stream G1: dungeon view

Shared files: `src/engine/map.h` (G4 adds two prototypes), `src/ui/automap.c` (G4 moves the visibility rule out), the state JSON writer (G4 changes the visibility calls), `src/data/ids.h`.

The order puts the file split last, so that Tasks 85-90 edit one file and the split is a pure move.

### Task 85: view constants to their owners, engine reads through accessors

**Files:**
- Modify: `src/ui/view.c`, `src/ui/view.h`, `src/engine/map.h`, `src/data/ids.h`, `src/ui/automap.c` (one include line)
- Test: `tests/golden/test_golden_view.py`, `tests/port/test_view.py`

**Interfaces:**
- Consumes: `map_get_level_change_location` (F, R6.4), `group_get_active(uint16_t thing)` and `projectile_get_flight_direction(uint16_t thing)` (F tail, R7.5), the map list accessor of E1.
- Produces: `DOOR_NORTH_SOUTH` in `src/engine/map.h`; `GFX_ORNAMENT_VI_ALTAR`... see Step 3 for the exact names in `data/ids.h`.

- [ ] **Step 1: Check the consumed names.** Run `grep -n "map_get_level_change_location\|group_get_active\|projectile_get_flight_direction\|rec_map_" src/engine/*.h`. Each must exist. If the map list accessor is missing, add to `src/engine/record.h`:

```c
/* The creature types of map (map_creature_type_count entries), followed by its ornament lists. */
const uint8_t *rec_map_creature_types(int16_t map);
```

and to `src/engine/record.c`:

```c
const uint8_t *rec_map_creature_types(int16_t map)
{
    return g_dungeon_map_data[map][dungeon_map_width(map) - 1] + dungeon_map_height(map);
}
```

- [ ] **Step 2: Move `DOOR_NORTH_SOUTH`.** Delete the line of `port/src/ui/view.h:9` and add it to the square bit block of `src/engine/map.h` beside `STAIRS_NORTH_SOUTH` (`port/src/engine/map.h:23`) in the notation of that block: `#define DOOR_NORTH_SOUTH 0x08 /* door square: the passage runs north-south */`. `src/ui/automap.c` (uses it at `port/src/ui/automap.c:137`) includes `engine/map.h` already; check with grep.

- [ ] **Step 3: Move the ornament ids.** `ORNAMENT_VI_ALTAR 2` (`port/src/ui/view.c:21`) and the local enum `PRESSURE_PAD = 1, ROUND_PRESSURE_PAD = 4, TINY_PRESSURE_PAD = 7` of `view_is_pressure_pad` (`port/src/ui/view.c:2240-2246`) go to `src/data/ids.h` in a new block, decimal, sorted by value:

```c
/* Wall and floor ornament ids of the game data that the view tests. */
#define ORNAMENT_FLOOR_PRESSURE_PAD       1
#define ORNAMENT_WALL_VI_ALTAR            2
#define ORNAMENT_FLOOR_ROUND_PRESSURE_PAD 4
#define ORNAMENT_FLOOR_TINY_PRESSURE_PAD  7
```

If the `numbered`/`prefix` checks of `data/ids.h` reject the `ORNAMENT_` kind (C1 defines the kinds `GFX_ ZONE_ LAYOUT_ ICON_ TEXT_`), use `GFX_ORNAMENT_*` instead and report it.

- [ ] **Step 4: Remove the level-change copy.** Delete `get_location_after_level_change` (`port/src/ui/view.c:1419-1435`). `draw_ceiling_pit` (`port/src/ui/view.c:1438-1444`) calls `map_get_level_change_location(view.map, LEVEL_UP, at)` (F's signature) instead. Assumption: the engine version differs only by its guard `g_party_map_index == NO_DUNGEON_MAP` (`port/src/engine/map.c:88-89`) and by the 16-bit level wrap (`port/src/engine/map.c:92`); the view draws only after a map load, so the guard never fires, and no map has level -1 or 0xffff. The golden view test checks it (Step 7).

- [ ] **Step 5: Raw engine reads go.** Replace:
  - `g_dungeon_map_data[m][map_width - 1] + map_height` (`port/src/ui/view.c:2185-2186`) by the accessor of Step 1.
  - The active-group read of `load_group_aspect` (`port/src/ui/view.c:674-677`: `g_active_groups && view_map == g_party_map_index && index < g_max_active_group_count ? &g_active_groups[index] : NULL`) by `view_map == g_party_map_index ? group_get_active(thing) : NULL`. Assumption: `group_get_active` returns NULL when the array is not allocated or the index is out of range (R5.8); check its comment.
  - `g_timeline_events[rec_projectile_event_index(thing)]` (`port/src/ui/view.c:894` and `:2338`) by `projectile_get_flight_direction(thing)`. At `:894` the fallback `g_timeline_events ? ... : dir` stays: Assumption: `projectile_get_flight_direction` returns -1 without a timeline; then use `dir`. Check its comment first.

- [ ] **Step 6: Build.** Run `make`. Expected: 0 warnings.

- [ ] **Step 7: Run the view tests.** Run `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py tests/port/test_automap.py -n auto`. Expected: PASS, golden view unchanged.

- [ ] **Step 8: Commit.** Delete the allowlist lines of `ui/view.c` and `ui/view.h` that these steps fix (`record`, `duplicate`, `decimal`), then:

```bash
git add -A src tests/standard/allowlist.txt
git commit -m "refactor(view): constants to their owners, engine state through accessors"
```

### Task 86: one View struct, Aspect without the union, bools

**Files:**
- Modify: `src/ui/view.c`
- Test: `tests/golden/test_golden_view.py`

**Interfaces:**
- Produces (inside `view.c`; moves to `view_internal.h` in Task 91):

```c
/* An ornament graphic and its coordinate set. */
typedef struct {
    int16_t graphic, coord;
} Ornament;

/* The state of the dungeon view: the loaded map, its graphics, and the palettes. */
typedef struct {
    Bitmap viewport;                     /* the dungeon view bitmap */
    int16_t map, map_width, map_height;  /* the loaded map, -1 none */
    int16_t wall_set_ids[WS_COUNT];      /* layout ids of the wall set, swapped for mirrored views */
    int16_t slot_graphics[LAYOUT_SLOTS]; /* graphic of each layout bitmap slot */
    bool slot_flip[LAYOUT_SLOTS];        /* the slot holds the mirrored graphic */
    Ornament wall_ornaments[MAP_ORNAMENTS];
    Ornament floor_ornaments[MAP_ORNAMENTS];
    Ornament door_ornaments[DOOR_ORNAMENT_THIEVES_EYE];
    int16_t alcoves[ALCOVE_ORNAMENT_COUNT];
    int16_t inscription, stairs;
    int16_t door_graphics[DOOR_DEPTHS][DOOR_TYPES];
    uint16_t door_type_attributes[DOOR_TYPES];
    bool use_flipped_walls; /* the view draws the mirrored wall set */
    uint16_t dungeon_palettes[LIGHT_LEVELS][PALETTE_COLORS];
    uint8_t creature_palette_changes_d3[PALETTE_COLORS];
    uint8_t creature_palette_changes_d2[PALETTE_COLORS];
    int16_t vi_altar, fountain; /* wall ornament index of the vi altar and the fountain, or -1 */
} View;

/* What the view draws of a square. */
typedef struct {
    uint16_t element;          /* square type, or a door or stairs element */
    uint16_t square;           /* square byte */
    uint16_t first_thing;      /* first thing after the doors, teleporters, texts, and sensors */
    uint16_t side[WALL_SIDES]; /* wall: ornament ordinal of each side (stored in the map), clockwise from the far side */
    uint16_t state;            /* door state, pit invisible, stairs up */
    uint16_t door;             /* the door thing */
    uint16_t floor_ornament;   /* floor ornament ordinal (stored in the map) */
    uint16_t text;             /* the text thing of an inscription on the wall */
    int16_t portrait;          /* a champion portrait on the wall: portrait index, -1 none */
} Aspect;
```

`a->far_side`, `a->right`, `a->front`, `a->left` become `a->side[SIDE_FAR]`, `a->side[SIDE_RIGHT]`, `a->side[SIDE_FRONT]`, `a->side[SIDE_LEFT]` with `/* Sides of Aspect.side, clockwise from the far side. */ #define SIDE_FAR 0 ... #define SIDE_LEFT 3`.

- [ ] **Step 1: List the aliases.** In `port/src/ui/view.c:217-226` the union aliases `floor_ornament` with `front`, `state` with `far_side`, `door` with `right`. Run `grep -n "\.state\|->state\|\.door\b\|->door\b\|floor_ornament\|far_side\|->front\|\.front\|->right\|\.right\|->left\|\.left" src/ui/view.c` and write down, per use, which element it serves. A read of an alias that the writer did not set (for example a wall side read as `state`) is a behavior today; keep it by reading the field that the writer set, and record each such place in the commit message.
- [ ] **Step 2: Replace the union.** Write the `Aspect` above. `get_square_aspect` (`port/src/ui/view.c:364-418`) writes the separate fields. The ornament ordinals stay ordinals: the map record stores them (standard 4.8).
- [ ] **Step 3: `portrait` becomes an index.** `portrait` holds "portrait index + 1, or 0" (`port/src/ui/view.c:225`). Write `index` or -1; `draw_wall_portrait` and its caller test `>= 0`. Grep `portrait` in `view.c` for every use.
- [ ] **Step 4: One View struct.** Replace the 20 file statics of `port/src/ui/view.c:235-249,265` (not the track state of `:252-264`, Step 5 below) by `static View view = {.map = -1};`. Rename uses: `viewport` → `view.viewport`, `view_map` → `view.map`, `map_width` → `view.map_width`, and so on. `view_draw_magic_map` (`port/src/ui/view.c:2493-2535`) saves and restores the three map fields; it now copies `int16_t saved[3]` once before and writes them back once after (no logic change).
- [ ] **Step 5: The flight state.** The track statics (`port/src/ui/view.c:252-264`) become `static struct { ProjectileTrack tracks[PROJECTILE_TRACK_COUNT], last_tracks[PROJECTILE_TRACK_COUNT]; int track_count, last_track_count; ViewPosition track_view; int16_t motion_step, motion_steps; } flight;` (`ViewPosition` comes in Task 89; until then keep `int32_t track_view`). `shift_x/y`, `zone_relative_x/y`, `drawn_box` stay for Task 87.
- [ ] **Step 6: bools.** `bool` for: `alcove_front`, `alcove` (`port/src/ui/view.c:620`), `drawn` (`:856`), `allowed` (`:307`, `:2317`), `blink` (`:2365`), the return of `draw_wall_ornament` (`:1263`), `use_flipped_walls` (`:244`), the locals `fluxcage`, `rebirth`, `mirrored`, `side` (`:1064`, `:1072`, `:1271`, `:1273`), `slot_flip`. `int flip` parameters that hold `FLIP_H`/`0` stay `int16_t` (they are flag values, not truth values).
- [ ] **Step 7: Names.** `Group` (`port/src/ui/view.c:657-664`) becomes `ViewGroup`; `get_creature_ordinal_in_cell` (`:698`) returns the creature index or -1 and becomes `get_creature_index_in_cell` (its callers subtract 1 today: grep `get_creature_ordinal_in_cell` and drop the `- 1`, change `!ordinal` tests to `index < 0`). `get_random_ornament_ordinal` keeps its name: it returns the ordinal that the map stores.
- [ ] **Step 8: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py`. Expected: 0 warnings, PASS.
- [ ] **Step 9: Commit.** `git commit -am "refactor(view): module state in one View struct, Aspect without a union"`

### Task 87: Placement and the drawn box as arguments

**Files:**
- Modify: `src/ui/view.c`
- Test: `tests/golden/test_golden_view.py`

**Interfaces:**
- Produces:

```c
/* Where view_draw_in_zone puts a bitmap: the relative position that a ZONE_SHIFTED or ZONE_SIZED zone passes to
   layout_get_zone, and the shift of a projectile between two ticks. */
typedef struct {
    int16_t zone_dx, zone_dy;
    int16_t shift_x, shift_y;
} Placement;

/* One bitmap draw into a zone of the viewport. */
typedef struct {
    int16_t zone;
    int16_t flip;        /* FLIP_H or 0 */
    int16_t transparent; /* transparent color, or COLOR_NONE */
    Placement place;
} ZoneBlit;

/* Draws bm into zone of dst. Returns true when it drew; drawn (NULL: not wanted) gets the drawn box. */
static bool draw_in_zone(const Bitmap *bm, Bitmap *dst, const ZoneBlit *blit, Box *drawn);
```

- [ ] **Step 1: Check the stale reads.** `zone_relative_x/y` are set at `port/src/ui/view.c:642-643, 753-754, 1309-1310, 1405-1406` and read only in `draw_bitmap_in_zone` (`:454-455`) for `ZONE_SHIFTED` and `ZONE_SIZED` zones. For each `draw_bitmap_in_zone` call, grep its zone: if a `ZONE_SHIFTED`/`ZONE_SIZED` zone is drawn without a preceding set in the same function, it reads the value of an earlier draw. Assumption: no call does. If one does, keep a `static Placement last_placement` that the caller passes, write it in the commit message, and report it.
- [ ] **Step 2: Rename and change the signature.** `draw_bitmap_in_zone(bm, dst, zone, flip, transparent)` (`port/src/ui/view.c:446-475`) becomes `draw_in_zone` above. It reads `blit->place.zone_dx/dy` where it read `zone_relative_x/y`, and `blit->place.shift_x/y` where it read `shift_x/y`; it writes `*drawn` where it wrote `drawn_box`.
- [ ] **Step 3: The callers.** 13 calls (`grep -n "draw_bitmap_in_zone(" src/ui/view.c`). A caller that set `zone_relative_x/y` builds the `Placement` with those values; the others pass `{0}`.
- [ ] **Step 4: Projectiles.** `begin_projectile(thing)` (`:845-854`) returns the `Placement` with the shift instead of writing `shift_x/y`; `end_projectile(thing, bool drawn, const Box *box)` takes the drawn box instead of reading `drawn_box`. `draw_projectile` (`:923-958`) passes a local `Box drawn = {0}` and tests `drawn.w > 0` where it cleared `drawn_box.w` (`:929-932`).
- [ ] **Step 5: Clickable boxes.** `add_clickable_object(index, thing)` (`:590-614`) reads `drawn_box`; it takes `const Box *drawn`. The wall ornament and the door button (`:1314`, `:1377`) take the box from their own `draw_in_zone` call.
- [ ] **Step 6: Delete the statics** `zone_relative_x/y`, `shift_x/y`, `drawn_box`.
- [ ] **Step 7: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py tests/port/test_modes.py` (smooth flight). Expected: PASS.
- [ ] **Step 8: Commit.** `git commit -am "refactor(view): placement and drawn box as arguments, no hidden statics"`

### Task 88: SquareDraw context, at most five parameters

**Files:**
- Modify: `src/ui/view.c`
- Test: `tests/golden/test_golden_view.py`, `tests/differential/test_csb_magic_map.py`

**Interfaces:**
- Produces:

```c
/* One view square while its things draw. */
typedef struct {
    int16_t dir;         /* party direction */
    Location at;         /* the map square */
    int16_t view_square; /* VIEW_SQUARE_* */
    int16_t lane, depth; /* of the view square */
    int16_t coord;       /* object or alcove coordinate set, -1 none */
    int16_t pass;        /* DOOR_PASS_* of a door square, else 0 */
    int16_t cell;        /* the view cell that draws, VIEW_CELL_* */
    int16_t map_cell;    /* its map cell */
} SquareDraw;

/* Where an object of a pile goes. */
typedef struct {
    int16_t zone;      /* the zone of an object in flight */
    int16_t *pile;     /* objects drawn on the pile so far; NULL: an object in flight */
    bool alcove;       /* the pile is in an alcove */
    bool alcove_front; /* the alcove faces the party */
} ObjectSpot;

/* A projectile graphic: the pose and the mirror flag. */
typedef struct {
    int16_t pose;
    int16_t flip;
} ProjectilePose;

/* One magic map square draw. */
typedef struct {
    Bitmap *dst;
    const uint8_t *spells;
    bool blink;
    int16_t dir;
    int16_t px, py;
} MapDraw;
```

New signatures (all static):

```c
static void draw_object(const SquareDraw *sq, uint16_t thing, int16_t aspect_index, const ObjectSpot *spot);
static bool draw_creatures_in_cell(const SquareDraw *sq, const ViewGroup *group, uint16_t rest, bool front);
static ProjectilePose get_projectile_pose(const SquareDraw *sq, const uint8_t *aspect, uint16_t thing);
static void draw_projectile(const SquareDraw *sq, uint16_t thing);
static void draw_explosions(const SquareDraw *sq, uint16_t first);
static void draw_cell_objects(const SquareDraw *sq, uint16_t first, int16_t *pile, bool alcove);
static void draw_creature(const SquareDraw *sq, const ViewGroup *group, uint8_t look, uint16_t facing, int16_t pos);
static void draw_map_square_things(const MapDraw *m, const Aspect *aspect);
static void draw_map_square(const MapDraw *m, Location at);
```

- [ ] **Step 1: List the wide functions.** `draw_object` 10 (`port/src/ui/view.c:619-620`), `draw_creatures_in_cell` 9 (`:765-766`), `draw_projectile` 8 (`:923-924`), `get_projectile_pose` 8 (`:882-883`), `draw_explosions` 7 (`:1058-1059`), `draw_cell_objects` 8 (`:1120-1121`), `draw_map_square_things` 7 (`:2365`), `draw_map_square` 7 (`:2436`), `draw_creature` 6 (`:718`). Check with `grep -c` after the change that none has more than five.
- [ ] **Step 2: `draw_square_things`** (`:1149-1207`) builds one `SquareDraw sq` from its arguments and the `ViewSquare` (`lane`, `depth`, `coord`, `pass`), sets `sq.cell` and `sq.map_cell` in the loop, and passes `&sq`. The map cell that the loop leaves in `mcell` and that `draw_explosions` reads (`:1159`, `:1205`) is `sq.map_cell` after the loop; name it in a one-line comment: "the last drawn map cell decides the rebirth check, as in the original".
- [ ] **Step 3: `draw_projectile`** reads `sq->coord`, which is `square.object_coord` on every path where the loop draws projectiles (`:1197-1201`: the alcove pass breaks before them, `:1183`). `get_projectile_pose` returns `ProjectilePose`.
- [ ] **Step 4: Magic map.** `view_draw_magic_map` builds one `MapDraw` per square and passes it.
- [ ] **Step 5: `get_relative_square`** (`:2195-2200`) is the step rule of `map_step` (both step `forward` along `dir` and `right` along `turn_right(dir)` with the step tables). Replace it by `map_step` when E2's signature takes a direction, a forward and a right count; else keep it and report it.
- [ ] **Step 6: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py tests/differential/test_csb_magic_map.py tests/differential/test_creatures.py`. Expected: PASS.
- [ ] **Step 7: Commit.** `git commit -am "refactor(view): SquareDraw context replaces the long parameter lists"`

### Task 89: types instead of encoded values

**Files:**
- Modify: `src/ui/view.c`
- Test: `tests/golden/test_golden_view.py`

**Interfaces:**
- Produces:

```c
/* A wall set bitmap: a graphic, or a layout slot of the wall set. */
typedef struct {
    int16_t graphic; /* graphic id, -1 when slot is set */
    int16_t slot;    /* LAYOUT_* slot, -1 when graphic is set */
} WallBitmap;

/* The aspect of a thing in flight: an object aspect, or a projectile aspect. */
typedef struct {
    bool projectile;
    int16_t index;
} ProjectileAspect;

/* A bitmap at a scale: a cached graphic, or an owned copy. */
typedef struct {
    const Bitmap *bitmap; /* the bitmap to draw, NULL when the graphic is missing */
    Bitmap own;           /* own.px owns the scaled pixels, NULL when bitmap is the cached graphic */
} ScaledBitmap;
static void scaled_bitmap_free(ScaledBitmap *s);

/* The map, square and direction of the projectile tracks. */
typedef struct {
    int16_t map, x, y, dir;
} ViewPosition;
```

- [ ] **Step 1: WallBitmap.** `draw_wall_bitmap(bitmap_id, zone, mode)` treats `bitmap_id < 0` as a layout slot (`port/src/ui/view.c:420-437`) and `load_wall_set` writes `slot_graphics[-1 - LAYOUT_...]` (`:2092-2099`). It takes `WallBitmap` (by value, 4 bytes); `wall_set_ids[]` becomes `WallBitmap wall_set[WS_COUNT]`; the `-1 - x` encodings go. Grep `-1 -` in `view.c` for every site.
- [ ] **Step 2: ProjectileAspect.** `get_projectile_aspect` (`:561-586`) returns a negative number for a projectile aspect; it returns `ProjectileAspect`. `draw_projectile` tests `.projectile` instead of the sign (`:927`, `:935` `-aspect_index`).
- [ ] **Step 3: ScaledBitmap.** `get_shrunk_bitmap(g, scale, pal, out)` returns the cached bitmap or `out`, and the ten callers `free(own.px)` either way (`:534-545`; grep `free(own.px)`). It becomes `static ScaledBitmap get_scaled(int16_t graphic, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes)` (it also replaces `get_scaled_bitmap`, `:1215-1223`, which takes two scales), and every caller ends with `scaled_bitmap_free(&s)`.
- [ ] **Step 4: ViewPosition.** `track_view` packs map, position and direction with `<< MAP_COORD_BITS` and `<< DIRECTION_BITS` (`:826-834`). `start_tracks` builds a `ViewPosition` and compares the four fields.
- [ ] **Step 5: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py tests/port/test_modes.py`. Expected: PASS.
- [ ] **Step 6: Commit.** `git commit -am "refactor(view): WallBitmap, ProjectileAspect, ScaledBitmap, ViewPosition"`

### Task 90: the square table: derive the right rows, or mark every unread field

**Files:**
- Modify: `src/ui/view.c`
- Test: `tests/golden/test_golden_view.py`

**Interfaces:**
- Produces: `static SquareView mirror_square_view(const SquareView *left)` (only when the check of Step 2 passes).

- [ ] **Step 1: The rule.** The table `square_views` (`port/src/ui/view.c:1504-1855`, 16 rows) lists each left square before its right twin (`:1505-1530` D3L2, `:1531-1555` D3R2). A right row is the mirror of its left row when: `right` is negated; `mirror` is `BLIT_FLIPPED` for `BLIT_TRANSPARENT`; `side_face` swaps `SIDE_FACE_RIGHT` and `SIDE_FACE_LEFT`; each cell order swaps the nibbles of the left and right cells (`CELLS_BL_BR_FL_FR` → `CELLS_BR_BL_FR_FL`); each id field (`view`, `view_square`, `wall`, `side_ornament_view`, `pit_zone`, `stairs_*_zone`, `stairs_front`, `stairs_front_down`, `door_*`, `button`, `ceiling_zone`) is the left id plus one stride per field kind; `pit` and graphic fields are equal.
- [ ] **Step 2: The check.** Write `mirror_square_view` with that rule. Add a temporary block at the end of `view_load_map`:

```c
#ifdef VIEW_MIRROR_CHECK
    for (int i = 0; i + 1 < DRAWN_SQUARE_COUNT; i += 2) {
        SquareView m = mirror_square_view(&square_views[i]);
        if (square_views[i].right < 0 && memcmp(&m, &square_views[i + 1], sizeof m) != 0)
            os_log("view: square row %d does not mirror row %d", i + 1, i);
    }
#endif
```

  Build the library with `make lib CFLAGS_EXTRA=-DVIEW_MIRROR_CHECK` (Assumption: the Makefile of package B has a `CFLAGS_EXTRA` variable; else pass the flag the way its `SANITIZE` option does) and run `uv run pytest -q tests/golden/test_golden_view.py -s`. A `SquareView` is zero-initialized by its designated initializer, so `memcmp` over padding is defined (static storage).
- [ ] **Step 3a: The check passes** (no log line): keep only the left rows in `square_views_left[DRAWN_SQUARE_COUNT / 2 + ...]` and build the full table once in `view_load_map` (or at the first `view_draw`) into `static SquareView square_views[DRAWN_SQUARE_COUNT]`. The rows without a twin (center rows and D0 rows) stay written out. Remove the temporary block.
- [ ] **Step 3b: The check fails:** remove `mirror_square_view` and the block; keep the table and write `-1` into every field that the draw functions do not read for that row (`docs/todo.md` lists the unread 0 fields and the literal D0C stairs zone; read the item and close it). Add one comment line above the table that names the table each id enum indexes (`WS_*`: wall set layout ids, `VIEW_SQUARE_*`: view square table, `VIEW_D*`: floor ornament and door views, `VIEW_WALL_*`: wall ornament views, `BUTTON_*`: door buttons).
- [ ] **Step 4: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py`. Expected: PASS, golden view unchanged.
- [ ] **Step 5: Commit.** `git commit -am "refactor(view): square table derives the right rows"` (3a) or `"refactor(view): square table marks unread fields"` (3b); update `docs/todo.md` in the same commit.

### Task 91: comments and markers of the view

**Files:**
- Modify: `src/ui/view.c`, `src/ui/view.h`, `docs/todo.md`

- [ ] **Step 1: Original symbol names.** Rewrite in game terms the comments that name an original function or global: `port/src/ui/view.c:418` (draw_wall_set_bitmap), `:445` (g_drawn_box), `:1214` (get_scaled_flipped_bitmap), `:2178` (set_current_map_and_party_map, load_current_map_graphics), `:2062` (load_floor_set, load_wall_set), the cell order comment `:144-146` (draw_objects_creatures_projectiles_explosions), `:2539` (inventory_set_dungeon_view_palette in the `view_render` comment). The mapping stays in `docs/reference/kaos-symbols.csv`; add a line there for any name that is not in it.
- [ ] **Step 2: The marker.** `port/src/ui/view.c:1889` "ponytail: no thieves eye hole yet, needs the party spell state (step 9)": move the open work to `docs/todo.md` (one line under the view) and delete the marker.
- [ ] **Step 3: The stray comment** at `port/src/ui/view.c:481` (a comment attached to nothing): delete it if it describes nothing below; else attach it to its function.
- [ ] **Step 4: Comment place.** Public functions of `view.h` get their one-line comment in the header (move it from the definition where it is there: `view_draw`, `view_render`, `view_load_map`, `view_set_motion`); static functions keep theirs above the definition. Add the missing comments of `reset_wall_set_ids`, `get_wall_set_slot`, `set_slot`, `get_object_aspect` (`:2045-2060`, `:555`).
- [ ] **Step 5: Header purpose paragraph** of `view.h`: "Draws the dungeon view and the CSB magic map from game state and changes no game state; its outputs are the click boxes, the piles and the facing flags of the last view."
- [ ] **Step 6: Build.** `make`. Expected: 0 warnings.
- [ ] **Step 7: Commit.** `git commit -am "docs(view): comments in game terms, open work to the todo list"`

### Task 92: split view.c into three files with an internal header

**Files:**
- Create: `src/ui/view_internal.h`, `src/ui/view_walls.c`, `src/ui/view_things.c`
- Modify: `src/ui/view.c`, `tests/standard/allowlist.txt`
- Test: `tests/golden/test_golden_view.py`

**Interfaces:**
- Produces `src/ui/view_internal.h` (prefix `view_`, guard `DM_UI_VIEW_INTERNAL_H`): the types `View`, `Aspect`, `Ornament`, `SquareView`, `WallPart`, `WallBitmap`, `Placement`, `ZoneBlit`, `ScaledBitmap`, `SquareDraw`; the constants that two of the three files use; `extern View g_view;` (the `static View view` of Task 86 becomes `View g_view` in `view.c`: three files share it, so it cannot be `static`; the standard's rule 7.1 "module-private state is static" is met by declaring it only in the internal header); and these internal functions:

```c
/* view.c: shared drawing primitives */
bool view_draw_in_zone(const Bitmap *bm, Bitmap *dst, const ZoneBlit *blit, Box *drawn);
void view_draw_wall_bitmap(WallBitmap bitmap, int16_t zone, int16_t mode);
ScaledBitmap view_get_scaled(int16_t graphic, int16_t scale_x, int16_t scale_y, const uint8_t *palette_changes);
void view_free_scaled(ScaledBitmap *s);
bool view_is_alcove(int16_t ornament_index);
/* view_walls.c */
void view_draw_wall_part(const WallPart *part);
void view_draw_square_pit(const SquareView *s, bool invisible);
void view_draw_stairs_side(const SquareView *s, bool up);
void view_draw_ceiling_pit(const SquareView *s, Location at);
void view_draw_square_wall(const SquareView *s, const Aspect *a, int16_t dir, Location at);
void view_draw_door_front(const SquareView *s, const Aspect *a, int16_t dir, Location at);
void view_draw_stairs_front(const SquareView *s, bool up);
void view_draw_d0c_stairs(const SquareView *s, bool up);
void view_draw_floor_ornament(uint16_t ordinal, int16_t view);
/* view_things.c */
void view_draw_square_things(uint16_t first, int16_t dir, Location at, int16_t view_square, uint16_t order);
void view_draw_teleporter_field(int16_t view_square, int16_t zone);
void view_start_tracks(int16_t dir, Location at);
```

  (`scaled_bitmap_free` of Task 89 becomes `view_free_scaled`.) If a function of this list is called from one file only after the split, it stays `static` in that file and leaves the list.

Split map (every function of `port/src/ui/view.c` at commit 72e24b5; names after Tasks 86-89):

| Function (`view.c` line) | Goes to |
|---|---|
| `is_open_floor` 274, `get_square` 281, `is_alcove` 301 (→ `view_is_alcove`), `get_random_ornament_ordinal` 307, `set_random_wall_ornaments` 325, `scan_ornament_things` 340, `get_square_aspect` 364 | `view.c` (the aspect of a square) |
| `draw_wall_bitmap` 420 (→ `view_draw_wall_bitmap`), `draw_bitmap_in_zone` 446 (→ `view_draw_in_zone`), `get_shrunk_bitmap` 534 and `get_scaled_bitmap` 1215 (→ `view_get_scaled`) | `view.c` (shared primitives) |
| `get_object_palette_changes` 548, `get_object_aspect` 555, `get_projectile_aspect` 561, `add_clickable_object` 590, `draw_object` 619, `load_group_aspect` 667, `get_creature_cell` 688, `get_creature_direction` 692, `get_creature_ordinal_in_cell` 698, `draw_creature` 718, `draw_creatures_in_cell` 765, `start_tracks` 826, `find_track` 836, `begin_projectile` 845, `end_projectile` 856, `view_has_moving_projectiles` 867, `view_set_motion` 875, `get_projectile_pose` 882, `draw_projectile` 923, `draw_field` 960, `get_explosion_bitmap` 984, `draw_explosion_on_party` 993, `get_explosion_aspect` 1019, `draw_rebirth_step_1` 1037, `draw_fluxcage` 1049, `draw_explosions` 1058, `is_cell_in_view` 1114, `draw_cell_objects` 1120, `find_square_things` 1133, `draw_square_things` 1149, `draw_teleporter_field` 1209 | `view_things.c`, with the flight state (`static struct { ... } flight;` of Task 86) |
| `draw_wall_text` 1225, `draw_wall_portrait` 1248, `draw_wall_ornament` 1263, `draw_floor_ornament` 1322, `draw_door_ornament` 1341, `draw_door_button` 1363, `draw_door` 1383, `draw_ceiling_pit` 1438, `draw_stairs_front` 1446, `draw_wall_part` 1858, `draw_square_pit` 1864, `draw_square_stairs_side` 1871, `draw_square_ceiling_pit` 1880, `draw_square_wall` 1887, `draw_square_door_front` 1906, `draw_square_stairs_front` 1921 | `view_walls.c` |
| `square_views` 1504, `draw_d0_side_square` 1931, `draw_d0c_square` 1954, `draw_square` 1972 | `view.c` (the square table and the dispatch) |
| `set_creature_replacement_colors` 2037, `reset_wall_set_ids` 2045, `get_wall_set_slot` 2051, `set_slot` 2056, `load_wall_set` 2064, `load_doors` 2103, `load_ornaments` 2115, `load_creature_colors` 2157, `view_load_map` 2180 | `view.c` (the map load) |
| `get_relative_square` 2195 (if Task 88 kept it), `view_get_wall_ornament` 2202, `view_get_wall_face_ornament_index` 2222, `view_get_wall_ornament_index` 2233, `view_is_pressure_pad` 2240, `view_get_floor_ornament` 2247 | `view.c` (the API) |
| `draw_map_tile` 2305, `has_map_footprints` 2315, `get_map_projectile_tile` 2325, `get_map_explosion_tile` 2348, `draw_map_square_things` 2365, `draw_map_wall` 2403, `get_map_door_tile` 2423, `draw_map_square` 2436, `view_draw_magic_map` 2493 | `view.c` (the magic map) |
| `view_draw` 2538, `view_render` 2585 | `view.c` |

`draw_d0c_square` calls `draw_stairs_front` twice with a fixed zone (`:1962-1963`): in `view_walls.c` that pair becomes `view_draw_d0c_stairs`.

- [ ] **Step 1: Write `view_internal.h`** with the SPDX line, the guard, `/* Prefixes: view_ */`, the purpose line "Private declarations shared by view.c, view_walls.c and view_things.c.", the includes it needs, and the declarations above.
- [ ] **Step 2: Move the functions** with the table above, cut and paste only. Each new `.c` starts with `#include "ui/view.h"` then `#include "ui/view_internal.h"`, then the project and system headers it uses (`make format` sorts them). Keep the functions of one file in the order of the table rows (callers below their static helpers, standard 8.3).
- [ ] **Step 3: Export the internal functions.** A function of the internal list loses `static` and gets its `view_` name; grep the old name over `src/ui/view*.c` and rename every call.
- [ ] **Step 4: Format and build.** `make format && make`. Expected: 0 warnings; `wc -l src/ui/view*.c` shows three files, none above 1,400 lines.
- [ ] **Step 5: Test.** `uv run pytest -q tests/golden/test_golden_view.py tests/port/test_view.py tests/differential/test_csb_magic_map.py tests/port/test_modes.py -n auto`. Expected: PASS.
- [ ] **Step 6: Standard.** `uv run pytest -q tests/standard`. The `module` check accepts `view_internal.h`; delete the `open: epic 8` lines of `ui/view.c` that the split fixes (`long`, `params`, `order`).
- [ ] **Step 7: Commit.** `git add src/ui/view*.c src/ui/view*.h tests/standard/allowlist.txt && git commit -m "refactor(view): split into view, view_walls and view_things"`

---

#### Stream G2: status, panel, menus

Shared files: `src/ui/command.c`, `src/ui/gameloop.c`, `src/ui/startend.c`, `src/ui/advanced.c`, `src/ui/automap.c`, `src/control/state_json.c` and `src/control/` users of the panel functions (call-site renames only), `src/engine/champion.h/.c` (Task 96 only), the tests that call the renamed functions.

### Task 93: split ui_champion.c into status.c and panel.c

**Files:**
- Create: `src/ui/status.h`, `src/ui/panel.h`
- Move: `src/ui/ui_champion.c` → `src/ui/panel.c` (`git mv`, so the larger half keeps its history); new `src/ui/status.c`
- Modify: `src/ui/ui.h`, `src/ui/ui.c`, every caller (grep list in Step 4)
- Test: `tests/golden/test_golden_screens.py`, `tests/port/test_panels.py`, `tests/port/test_ui.py`

**Interfaces:**
- Produces `src/ui/status.h` (prefix `status_`): "The champion status boxes at the top of the screen: name, bars, hands, shields, damage, the party icons; while the inventory of a champion is open, its portrait, name and title and its bars in the panel."

```c
void status_draw_slot(int16_t champion_index, int16_t slot);              /* was ui_draw_champion_slot */
void status_draw_damage(int16_t champion_index, int16_t damage);          /* was ui_draw_champion_damage */
void status_draw_champion(int16_t champion_index);                        /* was ui_draw_champion_state */
void status_draw_all(uint16_t attributes);                                /* was ui_draw_all_champion_states */
```

- Produces `src/ui/panel.h` (prefix `panel_`): "The inventory panel: slots, the eye and the mouth, food and water, skills and statistics, the object description, the scroll, the chest." With the constants of `port/src/ui/ui.h:139-142` (`PANEL_CONTENT_*`, `PANEL_LINE_*`, `STAMINA_DISPLAY_UNIT`, `WEIGHT_TENTHS`) and `extern int16_t g_panel_content;`:

```c
void panel_open(int16_t champion_index);                                   /* was ui_draw_inventory_open */
void panel_close(int16_t champion_index);                                  /* was ui_draw_inventory_close */
void panel_draw(void);                                                     /* was ui_draw_panel */
void panel_draw_skills(void);                                              /* was ui_draw_champion_skills_and_statistics */
void panel_draw_statistics(int16_t champion_index);                        /* was static draw_inventory_statistics */
void panel_draw_load(int16_t champion_index);                              /* was static draw_load */
void panel_draw_object(uint16_t thing, bool pressing_eye);                 /* was ui_draw_panel_object */
void panel_draw_eye(void);                                                 /* was ui_draw_panel_eye */
void panel_redraw_scroll(void);
void panel_click_in_chest(int16_t x, int16_t y);
const char *panel_get_rank_name(int16_t level);                            /* was ui_get_skill_rank_name */
int16_t panel_format_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE]); /* was panel_draw_object_text */
bool panel_get_object_meter(uint16_t thing, const char **label, uint16_t *value);           /* was panel_draw_object_meter */
```

- Produces in `src/ui/ui.h`/`ui.c`: `Bitmap ui_get_champion_portrait(int16_t champion_index)` (was `ui_draw_champion_portrait`, `port/src/ui/ui_champion.c:204`), `void ui_draw_icon_to_viewport(int16_t icon, int16_t zone)` (moves from `port/src/ui/ui_champion.c:646`), `const char *ui_format_number(char *buf, size_t size, uint16_t value, bool pad, int16_t width)` (was static `format_number`, `:67`, shared by both files), `g_rune_names` (defined in `ui.c`).

Split map of `port/src/ui/ui_champion.c` (line: function → file):

| Line | Function | Goes to |
|---|---|---|
| 67 | `format_number` | `ui.c` as `ui_format_number` |
| 76 | `champion_color` | `status.c` |
| 83 | `ui_draw_champion_bar_graphs` | `status.c`, static `draw_bar_graphs` (no caller outside, finding 46 of `review-ui.md`) |
| 114 | `draw_value_pair` | `panel.c` |
| 124 | `ui_draw_champion_slot` | `status.c` (`status_draw_slot`; draws the hands of a status box and the slots of the open inventory) |
| 165 | `ui_draw_champion_damage` | `status.c` |
| 178 | `draw_icon` | `status.c` |
| 204 | `ui_draw_champion_portrait` | `ui.c` (`ui_get_champion_portrait`) |
| 210 | `ui_draw_status_box_portrait` | `status.c`, static `draw_portrait` |
| 227 | `draw_inventory_statistics` | `panel.c` (`panel_draw_statistics`) |
| 251 | `draw_load` | `panel.c` (`panel_draw_load`) |
| 269 | `draw_status_box` | `status.c` |
| 294 | `draw_name` | `status.c` |
| 312 | `ui_draw_champion_state` | `status.c` (`status_draw_champion`) |
| 371 | `ui_draw_all_champion_states` | `status.c` (`status_draw_all`) |
| 381 | `draw_food_water_bar` | `panel.c` |
| 399 | `ui_draw_panel_food_water_poisoned` | `panel.c`, static `draw_food_water_poisoned` |
| 416 | `ui_draw_panel` | `panel.c` (`panel_draw`) |
| 437 | `get_zone_point` | `panel.c` |
| 447 | `ui_get_skill_rank_name` | `panel.c` (`panel_get_rank_name`) |
| 458 | `ui_draw_champion_skills_and_statistics` | `panel.c` (`panel_draw_skills`) |
| 495, 521 | `ui_draw_inventory_open`, `ui_draw_inventory_close` | `panel.c` (`panel_open`, `panel_close`) |
| 533-626 | `append`, `format_attributes_text`, `panel_draw_object_text`, `print_panel_line` | `panel.c` |
| 646 | `ui_draw_icon_to_viewport` | `ui.c` |
| 658-811 | `get_rune_of_word`, `add_rune_symbols`, `draw_scroll_line`, `insert_creature_name`, `draw_panel_magic_map`, `draw_panel_scroll` | `panel.c` |
| 812-895 | `largest_attack`, `get_largest_defense`, `get_largest_weight`, `draw_stat_meter`, `panel_draw_object_meter`, `draw_stat_meters`, `open_and_draw_chest` | `panel.c` |
| 912-964 | `ui_draw_panel_object`, `ui_draw_panel_eye`, `panel_redraw_scroll`, `panel_click_in_chest` | `panel.c` |

`status_draw_champion` calls `panel_draw_statistics`, `panel_draw_load` and `panel_draw` for the open inventory (`port/src/ui/ui_champion.c:343-358`); `panel.c` calls `status_draw_slot`.

- [ ] **Step 1: Move the file.** `git mv src/ui/ui_champion.c src/ui/panel.c`; commit nothing yet.
- [ ] **Step 2: Cut the status functions** of the table into a new `src/ui/status.c`, the shared ones into `ui.c`. Write `status.h` and `panel.h` (SPDX, guard `DM_UI_STATUS_H`/`DM_UI_PANEL_H`, prefix line, purpose paragraph, the prototypes above with their one-line comments moved from the definitions).
- [ ] **Step 3: `ui.h`** keeps the `ui_` part (zones, blits, icons, palette, hit tests, colors, `UI_ICON_SIZE`, `UI_BODY_SLOT_COUNT`, `UI_SLOT_BOX_INVENTORY_FIRST`, `UI_BUTTON_*`, `g_rune_names`, `g_action_area_contains_icons` moves to `menus.h` in Task 94); its prefix line becomes `/* Prefixes: ui_ */`. It stops including `engine/champion.h` and `data/tables.h` (finding 49 of `review-ui.md`); each `.c` that rode on them includes them itself (build errors list them).
- [ ] **Step 4: Rename the calls** with this script (run from the repository root):

```bash
for pair in ui_draw_champion_slot:status_draw_slot ui_draw_champion_damage:status_draw_damage \
  ui_draw_all_champion_states:status_draw_all ui_draw_champion_state:status_draw_champion \
  ui_draw_inventory_open:panel_open ui_draw_inventory_close:panel_close \
  ui_draw_champion_skills_and_statistics:panel_draw_skills ui_draw_panel_object:panel_draw_object \
  ui_draw_panel_eye:panel_draw_eye ui_get_skill_rank_name:panel_get_rank_name \
  panel_draw_object_text:panel_format_object_text panel_draw_object_meter:panel_get_object_meter \
  ui_draw_champion_portrait:ui_get_champion_portrait; do
  old=${pair%%:*}; new=${pair##*:}
  grep -rlw "$old" src tests tools docs .claude | xargs sed -i '' "s/\\b$old\\b/$new/g"
done
grep -rlw "ui_draw_panel" src tests tools docs | xargs sed -i '' 's/\bui_draw_panel\b/panel_draw/g'
```

  (BSD `sed` on macOS: `\b` works in `sed -E`; if not, use `perl -pi -e 's/\bold\b/new/g'`.) Then add `#include "ui/status.h"` / `"ui/panel.h"` where the build reports a missing prototype.
- [ ] **Step 5: Format and build.** `make format && make`. Expected: 0 warnings.
- [ ] **Step 6: Test.** `uv run pytest -q tests/golden/test_golden_screens.py tests/port/test_panels.py tests/port/test_ui.py tests/port/test_control.py -n auto`. Expected: PASS, golden screens unchanged.
- [ ] **Step 7: Commit.** `git add -A src tests tools docs && git commit -m "refactor(ui): status and panel modules from ui_champion.c"`

### Task 94: menus module

**Files:**
- Move: `src/ui/ui_menus.c` → `src/ui/menus.c`
- Create: `src/ui/menus.h`
- Modify: `src/ui/ui.h`, callers
- Test: `tests/golden/test_golden_screens.py`

**Interfaces:**
- Produces `src/ui/menus.h` (prefix `menus_`, purpose: "The action area, the spell area and the movement arrows at the right of the screen."): the `menus_*` prototypes of `port/src/ui/ui.h:138-152` and `extern bool g_action_area_contains_icons;` (`port/src/ui/ui.h:64-65`). Types: `champion_index` parameters become `int16_t` (`menus_set_acting_champion`, `menus_draw_action_icon`, `menus_draw_spell_area_controls`, `menus_draw_champion_symbols` take `uint16_t` today, `port/src/ui/ui.h:140-148`); `menus_draw_available_symbols(int16_t symbol_step)`.

- [ ] **Step 1:** `git mv src/ui/ui_menus.c src/ui/menus.c`; write `menus.h`; remove the `menus_` block and `menus_` from the prefix line of `ui.h`.
- [ ] **Step 2: Wrapper.** `menus_draw_spell_area_controls` (`port/src/ui/ui_menus.c:187-190`) is a wrapper nobody calls: delete it (grep first; the tests must not call it).
- [ ] **Step 3: Includes.** Add `#include "ui/menus.h"` to every file that calls `menus_` (`grep -rl "menus_" src`).
- [ ] **Step 4:** `make format && make`; `uv run pytest -q tests/golden/test_golden_screens.py tests/port/test_ui.py`. Expected: PASS.
- [ ] **Step 5: Commit.** `git add -A src && git commit -m "refactor(ui): menus module with its own header"`

### Task 95: ui, status, panel and menus: constants, types, names, two guards

**Files:**
- Modify: `src/ui/ui.c`, `ui.h`, `status.c`, `panel.c`, `panel.h`, `menus.c`, `src/ui/command.c` (one block)
- Test: `tests/golden/test_golden_screens.py`, `tests/port/test_ui.py`

- [ ] **Step 1: Slot box numbering in one block of `ui.h`.** Today: `port/src/ui/ui.c:17-18`, `ui.h:58`, `ui_champion.c:44-45` (`SLOT_BOX_ACTION_HAND`, `SLOT_BOX_CHEST_FIRST`), `command.c:33`. Write one decimal block:

```c
/* Slot boxes: the two hands of each status box, then the inventory slots, then the chest slots. */
#define UI_SLOT_BOX_STATUS_FIRST      0
#define UI_SLOT_BOX_INVENTORY_FIRST   8
#define UI_SLOT_BOX_ACTION_HAND       (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_HAND_ACTION)
#define UI_SLOT_BOX_CHEST_FIRST       (UI_SLOT_BOX_INVENTORY_FIRST + SLOT_CHEST_FIRST)
```

  with the values that the four places use today (read them first; the names of `ui.c:17-18` and `command.c:33` join the block, the local copies go).
- [ ] **Step 2: Rune font.** `SCROLL_FONT_SHIFT 0x60` and `RUNE_FIRST 0x60` (`port/src/ui/ui_champion.c:32,35`) and the comment "font character - 0x60" (`port/src/ui/ui.h:103`) use `SPELL_SYMBOL_FIRST` of `data/spells.h` (`port/src/data/spells.h:13`); delete the two defines.
- [ ] **Step 3: Truth values and coordinates** (finding 17 and 19 of `review-ui.md`): `bool pad` (`ui_format_number`), `bool inv` (`draw_status_box`, `draw_name`), `bool pressing_eye` (`open_and_draw_chest`, `panel_draw_object`), `bool border` (`ui_get_icon_zone_box`, `ui_get_slot_box_zone`), `bool ui_zone_box(...)`, `bool ui_get_icon_zone_box(...)`, `bool ui_init(void)` (true on success; its caller `gameloop.c:60` and the tests test `!ui_init()` today: change to `ui_init()`). `champion_index` parameters `int16_t` (`status_draw_slot` and the others took `uint16_t`). Comments that say "Returns 1"/"1 ... else 0" say "Returns true".
- [ ] **Step 4: Names** (finding 13): `largest_attack` → `get_largest_attack`. Add the missing comments of `get_largest_defense`, `get_largest_weight`, `append`.
- [ ] **Step 5: The icon guard** (R8.9, `port/src/ui/ui.c:221-235`): when the loop ends with `sheet == 0` (the icon is below the first icon of the first sheet), `ui_extract_icon` fills `out` with color 0 and returns; it does not read `tables_get_icon_sheet_first(-1)`. Assumption: no game icon is below the first sheet (`tables_get_icon_sheet_first(0)` is 0); the guard changes no picture. One-line comment: "an icon below the first sheet draws empty (the original reads before the table)". Add the deviation line to the formats doc (`docs/formats/` page that lists the deviations; grep "deviation").
- [ ] **Step 6: The scroll read** (R8.9, `port/src/ui/ui_champion.c:782-793`). Verify the PRD claim first: after `*rest++ = 0`, `rest` points after the first newline at `text + p` (p ≥ 0); `end[-1] == '\n'` holds only when `strlen(rest) >= 1`, so `end - 2 >= rest - 1 = text + p >= text`. The read of `end[-2]` never leaves the array. If this reading holds, change nothing, write the proof in the commit message of this task and tell the controller that R8.9's first item and the Review Focus test of the scroll are void (a regression test needs a real bug). If the reading is wrong, add the guard `end - rest >= 2` and the regression test of the Review Focus to `tests/port/test_panels.py`.
- [ ] **Step 7: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_screens.py tests/port/test_ui.py tests/port/test_panels.py`. Expected: PASS.
- [ ] **Step 8: Commit.** `git commit -am "refactor(ui): one slot box block, bool results and flags, icon guard"`

### Task 96: the load level is an engine rule

**Files:**
- Modify: `src/engine/champion.h`, `src/engine/champion.c`, `src/ui/panel.c`
- Test: `tests/golden/test_golden_rules.py` (A3 scenario of the load level), `tests/golden/test_golden_screens.py`

**Interfaces:**
- Produces in `src/engine/champion.h`:

```c
/* The load of a champion against its maximum load. */
enum LoadLevel { LOAD_NORMAL, LOAD_HEAVY, LOAD_OVERLOADED };
/* The load level: overloaded above the maximum load, heavy above 5/8 of it. */
enum LoadLevel champion_get_load_level(int16_t champion_index);
```

- [ ] **Step 1: Gate.** Run the A3 scenario of the load level (`uv run pytest -q tests/golden/test_golden_rules.py -k load`). Expected: PASS on the unchanged code.
- [ ] **Step 2: Move the rule.** `draw_load` (`port/src/ui/ui_champion.c:251-262`) computes `max < c->load` (red) and `(int32_t)max * HEAVY_LOAD_NUMERATOR < (int32_t)c->load * HEAVY_LOAD_DENOMINATOR` (yellow). `champion_get_load_level` returns `LOAD_OVERLOADED`, `LOAD_HEAVY` or `LOAD_NORMAL` with the same comparisons in the same order and the same `int32_t` products; `champion_get_maximum_load` stays the source of `max`. `panel_draw_load` maps the level to `COLOR_RED`, `COLOR_YELLOW`, `COLOR_LIGHT_GRAY`. `HEAVY_LOAD_*` stay in `engine/champion.h` (grep their owner).
- [ ] **Step 3: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_rules.py tests/golden/test_golden_screens.py tests/differential/test_champion.py`. Expected: PASS, both goldens unchanged.
- [ ] **Step 4: Commit.** `git commit -am "refactor(champion): the load level rule leaves the panel"`

---

#### Stream G3: frame, entrance, command, select, video headers

Shared files: `src/ui/command.c` (G2, G4), `src/ui/gameloop.c` (G2), `src/ui/startend.c` (G2, G4), `src/ui/dialog.c`, `src/ui/anim.c`, `src/ui/logo.c`, `src/ui/advanced.c`, `src/platform/main.c`, `src/platform/platform.c`, the tests that call the renamed names.

### Task 97: frame module

**Files:**
- Create: `src/ui/frame.h`, `src/ui/frame.c`
- Modify: `src/ui/startend.h`, `src/ui/startend.c`, callers (Step 3)
- Test: `tests/port/test_entrance.py`, `tests/port/test_save.py`, `tests/differential/test_csb_ending.py`

**Interfaces:**
- Produces `src/ui/frame.h`:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_FRAME_H
#define DM_UI_FRAME_H
/* Prefixes: frame_ */

/* The frame wait of the blocking sequences: dialogs, the entrance, the endgame, the animations. The program
   shows each frame through g_frame_hook. */

#include <stdbool.h>
#include <stdint.h>

/* Shows one 50 Hz frame and reads the input into the command queue; it runs no game logic, so a wait inside a
   game tick is safe. NULL (tests, turn-based mode): waits end at once and waits for input end without input. */
extern void (*g_frame_hook)(void);
extern bool g_quit_requested;      /* the player quits: the program ends */
extern int16_t g_frame_wait_depth; /* not 0 while frame_wait runs a frame */

/* Returns true after one frame, false when there are no frames or the player quits. */
bool frame_wait(void);
/* Waits count frames, or less when frame_wait returns false. */
void frame_wait_count(int16_t count);

#endif
```

- [ ] **Step 1: Move** `g_frame_hook`, `g_quit_requested`, `g_waiting_frame` (→ `g_frame_wait_depth`), `wait_frame` (→ `frame_wait`), `wait_frames` (→ `frame_wait_count`) from `port/src/ui/startend.h:11-26` and `port/src/ui/startend.c:69-96` into `frame.h/.c`. `startend.h` keeps `g_restart_requested`, `g_return_to_entrance` and its `startend_` functions; its prefix line becomes `/* Prefixes: startend_ */`.
- [ ] **Step 2: Rename** with `perl -pi -e 's/\bwait_frames\b/frame_wait_count/g; s/\bwait_frame\b/frame_wait/g; s/\bg_waiting_frame\b/g_frame_wait_depth/g'` over `src tests tools docs` (`grep -rlw` first: `platform/main.c` 11 uses, `ui/anim.c` 3, `command.c` 2, `dialog.c` 2, `gameloop.c` 5, `logo.c` 2, `advanced.c` 1, tests 36 uses in 8 files, measured at commit 72e24b5).
- [ ] **Step 3: Includes.** `dialog.c`, `logo.c`, `anim.c`, `command.c`, `gameloop.c`, `advanced.c`, `platform/main.c` include `ui/frame.h`; drop `ui/startend.h` where the file used it only for the frame wait (grep each file for `startend_`, `g_restart_requested`, `g_return_to_entrance`).
- [ ] **Step 4: Build and test.** `make format && make`; `uv run pytest -q tests/port/test_entrance.py tests/port/test_save.py tests/differential/test_csb_ending.py tests/port/test_gameloop.py -n auto`. Expected: PASS.
- [ ] **Step 5: Commit.** `git add -A src tests tools docs && git commit -m "refactor(ui): frame wait in its own module at the bottom of the layer"`

### Task 98: entrance module

**Files:**
- Create: `src/ui/entrance.h`, `src/ui/entrance.c`
- Modify: `src/ui/gameloop.h`, `src/ui/gameloop.c`, `src/ui/command.h`, `src/ui/command.c`, `src/platform/main.c`
- Test: `tests/port/test_entrance.py`

**Interfaces:**
- Produces `src/ui/entrance.h` (prefix `entrance_`, purpose: "The entrance of Dungeon Master: the doors, the ENTER and RESUME buttons, the door animation."):

```c
/* The button that ended the entrance (g_entrance_choice). */
#define ENTRANCE_RESUME   0
#define ENTRANCE_NEW_GAME 1
#define ENTRANCE_WAITING  99
extern int16_t g_entrance_choice; /* ENTRANCE_WAITING until a button is pressed; was g_new_game */

/* Shows the entrance: graphic 4 with the closed doors and the ENTER, RESUME, and QUIT buttons. */
void entrance_draw(void);
/* ENTER or RESUME was clicked: the pressed button, a click, a short wait. */
void entrance_press_button(bool resume);
/* After ENTER: the doors open over the view of the start square. Needs the frames of g_frame_hook. */
void entrance_open_doors(void);
```

- [ ] **Step 1: Move** `entrance_draw`, `entrance_press_button`, `entrance_open_doors` and their static helpers (`port/src/ui/gameloop.c:68-171`, among them `draw_entrance_doors`, which returns `bool` now) to `entrance.c`; the three prototypes leave `gameloop.h` (prefix line `/* Prefixes: gameloop_ */`).
- [ ] **Step 2: The choice.** `ENTRANCE_*` (`port/src/ui/command.h:79-80`) and `g_new_game` (`port/src/ui/command.c:45`) move to `entrance.h/.c` as `g_entrance_choice`; `command.c:766-771` writes it; `platform/main.c:307,357-367` reads it (rename with `perl -pi -e 's/\bg_new_game\b/g_entrance_choice/g'` over `src tests`).
- [ ] **Step 3: bool.** `entrance_press_button(bool resume)`: the caller `platform/main.c:358` passes `g_entrance_choice == ENTRANCE_RESUME`, already a truth value. `int confirm` (`port/src/ui/gameloop.c:286`), `shown`, `ran` (`:76`, `:337`, `:356`) become `bool`; `gameloop_run_frame` returns `bool` and its comment says "Returns true when ..." (`port/src/ui/gameloop.h:29-31`).
- [ ] **Step 4: Markers.** The two `ponytail:` comments of the entrance (`port/src/ui/gameloop.c:119`, `:148`) become deviation comments ("deviation: no credits", "deviation: the doors open over the view of the start square; the original draws a made-up corridor") and one line each in the deviations list of the formats doc.
- [ ] **Step 5:** `make format && make`; `uv run pytest -q tests/port/test_entrance.py tests/port/test_gameloop.py`. Expected: PASS.
- [ ] **Step 6: Commit.** `git add -A src tests docs && git commit -m "refactor(ui): entrance module owns the entrance choice"`

### Task 99: command header: keys, tick state, sentinels

**Files:**
- Modify: `src/ui/command.h`, `src/ui/command.c`, `src/ui/advanced.h`, `src/ui/advanced.c`, `src/ui/gameloop.h`, `src/ui/gameloop.c`, `src/ui/dialog.h`, `src/ui/startend.c`, `src/platform/main.c`
- Test: `tests/port/test_gameloop.py`, `tests/port/test_modes.py`, `tests/port/test_ui.py`

**Interfaces:**
- Produces: all `KEY_*` in `command.h` (one hex block, four digits); in `gameloop.h`: `g_stop_waiting_for_input`, `g_game_time_ticking`, `g_wait_for_input_max_frames`, `g_game_frozen`, `extern bool g_game_screen_shown;`; in `command.h`: `void command_close_inventory(void);`, `void command_redraw_inventory(void);`.

- [ ] **Step 1: Keys.** Move `KEY_TAB ... KEY_3` (`port/src/ui/advanced.h:14-25`) into the key block of `command.h` (`port/src/ui/command.h:55-65`), sorted by value, `0x0100` style. `advanced.h` includes `ui/command.h` if it needs a key; it stops including `engine/game.h` (finding 49) unless it uses it.
- [ ] **Step 2: Tick state.** Move the four globals of `port/src/ui/command.h:82-85` to `gameloop.h` with their comments; add one line to `g_game_frozen`: "stays set while the party sleeps; command_unfreeze_game keeps it then" (`port/src/ui/command.c:239-242`). `command.c` includes `ui/gameloop.h` for them.
- [ ] **Step 3: `g_game_screen_shown`.** `g_game_screen_inactive` (`port/src/ui/dialog.h:12`, 17 uses: `grep -rn g_game_screen_inactive src tests`) becomes `g_game_screen_shown` in `gameloop.h` with the inverse value: every write `= true` becomes `= false` and the reverse, every read `g_game_screen_inactive` becomes `!g_game_screen_shown`; the initial value is `true` where it was `false`. The tests read it through `BOOL_GLOBALS` of `tests/conftest.py` (or `dmtest`); rename there.
- [ ] **Step 4: Sentinels.** `command_toggle_inventory(INVENTORY_CLOSE)` and `(INVENTORY_REDRAW)` (`port/src/ui/command.c:29-30`, 3 uses `:322`, `:353`, `:687`) and `startend.c:508` `command_toggle_inventory(CHAMPION_MAX_COUNT)` become `command_close_inventory()` and `command_redraw_inventory()`; `command_toggle_inventory` takes a champion index 0-3 only and keeps its toggle rule. Read `command_toggle_inventory` (`:259`) and move the branches of the two sentinels into the two new functions; the shared tail stays one static helper.
- [ ] **Step 5: `dragged_icon_cell`.** `dragged_icon_cell_plus_1` (`port/src/ui/command.c:64`, 9 uses) holds cell + 1, 0 for none; it becomes `dragged_icon_cell` with -1 for none (drop the `+ 1` on writes, the `- 1` on reads, test `>= 0`).
- [ ] **Step 6: bool and comments.** `int16_t thrown` (`port/src/ui/command.c:490`) becomes `bool`; the header comment "KAOS range 8, COMMAND.C" (`port/src/ui/command.h:8`) says "the command table of the original" without the KAOS range; `ponytail:` of `port/src/ui/command.c:789` becomes "deviation: no credits and no name entry" and a todo line.
- [ ] **Step 7:** `make format && make`; `uv run pytest -q tests/port/test_gameloop.py tests/port/test_modes.py tests/port/test_ui.py tests/port/test_entrance.py -n auto`. Expected: PASS.
- [ ] **Step 8: Commit.** `git add -A src tests docs && git commit -m "refactor(ui): one key table, tick state in gameloop, no inventory sentinels"`

### Task 100: select names, output mapping to platform

**Files:**
- Modify: `src/ui/select.h`, `src/ui/select.c`, `src/ui/video.h`, `src/ui/video.c`, `src/platform/platform.c`, `src/platform/main.c`
- Test: `tests/port/test_select.py`

- [ ] **Step 1: select names.** `SCREEN_ADVENTURE/MODE/CSB/DONE` (`port/src/ui/select.h:11`) → `SELECT_SCREEN_*`; `CSB_NEW_ADVENTURE/RESUME/QUIT` (`:12`) → `SELECT_CSB_*`; the struct `Screen` of `port/src/ui/select.c:17-21` → `SelectScreen` (this frees the name `Screen` for the tail Task 105). Rename over `src tests` with `perl -pi -e 's/\bSCREEN_(ADVENTURE|MODE|CSB|DONE)\b/SELECT_SCREEN_$1/g; s/\bCSB_(NEW_ADVENTURE|RESUME|QUIT)\b/SELECT_CSB_$1/g'` (check with grep that `CSB_RESUME` has no second owner).
- [ ] **Step 2: Output mapping.** `video_layout` and `video_map_mouse` (`port/src/ui/video.c:30-53`) have callers only in `port/src/platform/platform.c:165,193,202`. Move them with `clamp` into `platform.c` as static functions `layout_output` and `map_mouse` (static: no prefix, standard 4.2; PRD R8.2 names `plat_layout`/`plat_map_mouse`, but an exported name with no outside user fails the dead-api check). `video.c` stops including `<math.h>`.
- [ ] **Step 3: Casts.** `video_argb_red/green/blue` (`port/src/ui/video.h:15-26`) return `(uint8_t)(argb >> ARGB_RED_SHIFT)` with the comment "the low byte is the channel".
- [ ] **Step 4:** `make format && make`; `uv run pytest -q tests/port/test_select.py tests/port/test_scale.py`. Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "refactor(ui): select names, output mapping moves to platform"`

---

#### Stream G4: rules out of ui

Shared files: `src/ui/command.c` (G2, G3), `src/ui/startend.c` (G2, G3), `src/ui/gameloop.c` (G3: one call), `src/engine/map.h` (G1), the state JSON writer (G1), `src/engine/sound.h`.

Each task: first the A3 scenario of the rule passes on the unchanged code, then the move, then the scenario, the golden snapshots and the differential tests of the module pass. A difference stops the run for the user (Global Constraints).

### Task 101: grab and drop are inventory rules

**Files:**
- Modify: `src/engine/inventory.h`, `src/engine/inventory.c`, `src/ui/command.c`
- Test: `tests/golden/test_golden_rules.py` (A3 scenarios of grab and drop), `tests/port/test_actions.py`

**Interfaces:**
- Produces in `src/engine/inventory.h`:

```c
/* The leader takes object thing from cell of square at into the hand, unless a creature that does not levitate
   stands in that cell. Returns true when the hand took it. */
bool inventory_grab_object(uint16_t thing, int16_t cell, Location at);
/* The leader puts the hand object into cell of square at; onto a vi altar, bones start the rebirth of
   their champion. */
void inventory_drop_object(int16_t cell, Location at, bool onto_vi_altar);
```

  (PRD R8.3 names `inventory_grab_object(cell, location)` and `inventory_drop_object(cell, location, into_alcove)`: the grab needs the pile top object, which only the view knows, and the drop needs "a vi altar", not "an alcove", because the rule tests the altar.)

- [ ] **Step 1: Gate.** `uv run pytest -q tests/golden/test_golden_rules.py -k "grab or drop or altar"`. Expected: PASS.
- [ ] **Step 2: Grab.** From `grab_object` (`port/src/ui/command.c:407-428`) the engine part goes to `inventory_grab_object`: the creature check `group != THING_ENDOFLIST && !move_is_levitating(group) && group_get_creature_ordinal_in_cell(group, cell)` (after F: the index form), the `object_get_icon_index(thing) != -1` test, `move_get_move_result(thing, at, ...)` (F's signature) and `inventory_put_object_in_leader_hand(thing)`. The ui keeps: the leader test, the front square, the pile top object, the cell `NORMALIZE(pile + g_party_direction)`, `g_stop_waiting_for_input = true`. Assumption: the ui checks the creature only for front piles (`pile >= VIEW_CELL_FRONT_FIRST`, `:415`) because no group stands on the party square; the engine checks it on every square. Check: `map_get_square_group` on the party square returns `THING_ENDOFLIST` in every A3 scenario and in `tests/port/test_actions.py`; if a test shows a group on the party square, keep the check only for the front square by passing `cell` through the caller and report it.
- [ ] **Step 3: Drop.** From `drop_object` (`:432-449`) the engine part goes to `inventory_drop_object`: take the hand object (`inventory_take_hand_object`, was `inventory_get_object_removed_from_leader_hand`), `move_get_move_result` of the object with its cell (the `THING_WITH_CELL` packing moves with it), the bones test and `sensor_add_vi_altar_rebirth_event`. The ui passes `onto_vi_altar = alcove && g_facing_vi_altar` (the tail Task 106 changes the read to `view_get_result()`).
- [ ] **Step 4: Build and test.** `make`; `uv run pytest -q tests/golden/test_golden_rules.py tests/port/test_actions.py tests/differential/test_champion.py tests/differential -k "inventory or grab or drop" -n auto`. Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "refactor(inventory): grab and drop rules leave the command module"`

### Task 102: imaginary wall and the movement block

**Files:**
- Modify: `src/engine/map.h`, `src/engine/map.c`, `src/engine/move.h`, `src/engine/move.c`, `src/ui/command.c`
- Test: `tests/golden/test_golden_rules.py`, `tests/differential/test_move.py`

**Interfaces:**
- Produces:

```c
/* map.h: True when square at of the current map is a closed fake wall that the party can walk into. */
bool map_is_imaginary_wall(Location at);
/* move.h: True when a movement command must wait: the party cannot move, or a projectile blocks that direction. */
bool move_is_blocked(int16_t command);
```

- [ ] **Step 1: Gate.** `uv run pytest -q tests/golden/test_golden_rules.py -k "imaginary or blocked or movement"`. Expected: PASS.
- [ ] **Step 2:** The predicate of `port/src/ui/command.c:542-544` (`SQUARE_TYPE(square) == SQUARE_FAKE_WALL && (square & FAKE_WALL_IMAGINARY) && !(square & FAKE_WALL_OPEN)`) moves to `map_is_imaginary_wall`; `click_in_dungeon_view`, `click_empty_handed` and `touch_front_square_empty_handed` (`:465-502`) pass `bool imaginary_wall`.
- [ ] **Step 3:** `is_movement_blocked` (`port/src/ui/command.c:793-802`) moves to `move.c` as `move_is_blocked`; it reads `g_disabled_movement_ticks`, `g_projectile_disabled_movement_ticks`, `g_last_projectile_disabled_movement_direction`, which can then become `static` in the engine if no other file reads them (grep; tests: mark them `/* exported for the tests */` if a test reads them).
- [ ] **Step 4:** `make`; `uv run pytest -q tests/golden/test_golden_rules.py tests/differential/test_move.py tests/port/test_gameloop.py`. Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "refactor(move): imaginary wall and movement block are engine rules"`

### Task 103: the fuse sequence rules

**Files:**
- Modify: `src/engine/game.h`, `src/engine/game.c`, `src/engine/map.h`, `src/engine/map.c`, `src/engine/group.h`, `src/engine/group.c`, `src/engine/sound.h`, `src/ui/startend.c`
- Test: `tests/golden/test_golden_rules.py` (A3 fuse scenario), `tests/port/test_save.py` or the test module of the fuse sequence (grep `startend_run_fuse_sequence tests`)

**Interfaces:**
- Produces:

```c
/* map.h: The fluxcage explosions on square at disappear. */
void map_remove_fluxcages(Location at);
/* group.h: All creature groups of the current map except the one on square at disappear. */
void group_delete_all_except(Location at);
/* game.h: Lord Chaos on square at before the fuse: full fuse health, centered, facing the party; the fluxcages on
   the party square and on at disappear. */
void game_prepare_fuse_lord(uint16_t lord, Location at);
```

- [ ] **Step 1: Gate.** `uv run pytest -q tests/golden/test_golden_rules.py -k fuse`. Expected: PASS.
- [ ] **Step 2:** `remove_fluxcages` (`port/src/ui/startend.c:431-443`) → `map_remove_fluxcages`; `delete_other_groups` (`:495-501`) → `group_delete_all_except`; the lord setup `group_set_health(lord, 0, FUSE_LORD_HEALTH)`, `group_set_cells(...)`, `group_set_directions(...)` and the two `remove_fluxcages` calls (`:514-518`) → `game_prepare_fuse_lord`. `FUSE_LORD_HEALTH` moves to `game.c`. The creature type changes (`:525`, `:532`, `flash_lords`) stay in ui: they are the animation of the sequence.
- [ ] **Step 3: Constants.** `SOUND_FUSE = 14` (`port/src/ui/startend.c:66`) and `SOUND_TELEPORT = 14` (`port/src/engine/sound.h:20`) go; `SOUND_BUZZ` (`port/src/engine/sound.h:34`) is the one name of sound 14 (PRD Decisions). Check the original name in `docs/reference/kaos-symbols.csv` (grep `buzz`) and replace every use. `DECODE_MESSAGE_INVISIBLE ((int16_t)0x8001)` (`port/src/ui/startend.c:43`) becomes `TEXT_MODE_MESSAGE | TEXT_MODE_ANY_VISIBILITY` of `engine/dungeon.h` (`port/src/engine/dungeon.h:38-40`) at its one use (`:465`); check that the two values OR to `0x8001`.
- [ ] **Step 4:** `make`; `uv run pytest -q tests/golden/test_golden_rules.py tests/differential/test_sound.py` and the fuse test module. Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "refactor(game): fuse sequence rules leave the endgame module"`

### Task 104: what the party sees is an engine query

**Files:**
- Modify: `src/engine/explored.h`, `src/engine/explored.c`, `src/ui/automap.h`, `src/ui/automap.c`, `src/ui/gameloop.c` (one call), the state JSON writer (`src/platform/state_json.c` or `src/control/state_json.c`, whichever exists), tests that call the old names
- Test: `tests/port/test_automap.py`, `tests/golden/test_golden_rules.py`, `tests/port/test_control.py`

**Interfaces:**
- Produces in `src/engine/explored.h`:

```c
/* The rows of the view: the party row and 3 rows ahead. */
#define EXPLORED_VIEW_DEPTHS 4
/* The rows that the party sees from from facing direction: up to the first center square that blocks sight
   (a wall, a closed fake wall, a closed door), and fewer in the two darkest light levels. */
int16_t explored_get_visible_depths(Position from, int16_t direction);
/* True when the party at from facing direction sees the side square side (-1 left, 1 right) at depth. */
bool explored_is_side_visible(Position from, int16_t direction, int16_t depth, int16_t side);
/* Marks the squares that the party sees from from facing direction as seen. */
void explored_mark_visible(Position from, int16_t direction);
```

- [ ] **Step 1: Gate.** `uv run pytest -q tests/port/test_automap.py tests/golden/test_golden_rules.py -k "automap or seen or visible"`. Expected: PASS.
- [ ] **Step 2: Move** `is_on_map`, `blocks_sight`, `light_depths`, `automap_get_visible_depths`, `automap_side_is_visible`, `automap_mark_seen` (`port/src/ui/automap.c:17-95`) to `explored.c` with the names above. `light_depths` reads `g_party_light_level` (E2) and `g_cheat_light`; `DARKEST_PALETTE` moves with it. `VIEW_DEPTH_COUNT` (`port/src/ui/view.h:11`, 5 uses) becomes `EXPLORED_VIEW_DEPTHS` everywhere (view.h deletes it; the ui includes `engine/explored.h`). `game_update_map_coordinates_after_relative_movement` calls become `map_step` (E2).
- [ ] **Step 3: The marker** `port/src/ui/automap.c:46` ("ponytail: fixed cut by palette index ...") moves as a line to `docs/todo.md`; the code keeps a plain comment "the darkest two light levels hide the far rows".
- [ ] **Step 4: Callers.** `gameloop.c:190` calls `explored_mark_visible((Position){g_party_map_index, g_party_map_x, g_party_map_y}, g_party_direction)` (use E2's party position type if it has one); the state JSON writer (`port/src/platform/state_json.c:562`, `:874`) calls the two queries; the tests (`tests/port/test_gameloop.py:274-284`, `tests/port/test_automap.py:78-243`) call the new names with a `Position` ctypes structure from `dmtest.game` (add `Position` there if C2 did not).
- [ ] **Step 5:** `make format && make`; `uv run pytest -q tests/port/test_automap.py tests/port/test_gameloop.py tests/golden/test_golden_rules.py tests/port/test_control.py tests/golden/test_golden_protocol.py -n auto`. Expected: PASS, golden protocol unchanged.
- [ ] **Step 6: Commit.** `git add -A src tests docs && git commit -m "refactor(explored): what the party sees is an engine query"`

---

#### Tail

Runs after the four streams are merged and `make test` is green. Each task touches many ui files.

### Task 105: one Screen struct

**Files:**
- Modify: `src/ui/video.h`, `src/ui/video.c`, every user of `fb`, `palette`, `palette_split_top`, `palette_split_bottom`, `ui_get_screen` (Step 3), `src/ui/anim.c`, `tests/dmtest/game.py`, the tests that bind the four globals
- Test: `tests/golden/test_golden_screens.py`, `tests/golden/test_golden_view.py`, `tests/port/test_ui.py`

**Interfaces:**
- Produces in `src/ui/video.h`:

```c
/* The Amiga screen: chunky pixels of palette indices and the two palette halves of the copper list. Rows
   split_top to split_bottom - 1 use the top palette (palette[0..15], the dungeon view), the other rows the bottom
   palette (palette[16..31], the interface). The default puts every row in the top palette. */
typedef struct {
    uint8_t px[SCREEN_H][SCREEN_W];
    uint16_t palette[2 * PALETTE_COLORS];
    int16_t split_top, split_bottom;
} Screen;
extern Screen g_screen;

/* The screen as a bitmap. */
static inline Bitmap video_get_screen(void)
{
    return (Bitmap){SCREEN_W, SCREEN_H, &g_screen.px[0][0]};
}
/* An Amiga 12-bit color from 4-bit red, green, blue. */
static inline uint16_t video_amiga_color(uint16_t red, uint16_t green, uint16_t blue)
{
    return (uint16_t)(red << AMIGA_RED_SHIFT | green << AMIGA_GREEN_SHIFT | blue);
}
uint16_t video_get_color(int16_t y, uint8_t color);
```

- Produces in `tests/dmtest/game.py`:

```python
class Screen(ctypes.Structure):
    _fields_ = [("px", ctypes.c_uint8 * (320 * 200)), ("palette", ctypes.c_uint16 * 32),
                ("split_top", ctypes.c_int16), ("split_bottom", ctypes.c_int16)]

def screen(lib):
    """The Screen of a loaded library."""
    return Screen.in_dll(lib, "g_screen")
```

- [ ] **Step 1: The struct.** `port/src/ui/video.c:10-13` define the four globals; replace them by `Screen g_screen = {.split_bottom = SCREEN_H};` and the struct above. `AMIGA_RED_SHIFT` (8) and `AMIGA_GREEN_SHIFT` (4) go into `video.h` as the one definition; `anim.c` (`RED_SHIFT`, `GREEN_SHIFT`, `port/src/ui/anim.c:29-30`) and `video.c` (`AMIGA_CHANNEL_BITS`) use them, and the two packings use `video_amiga_color`.
- [ ] **Step 2: Rename the uses** with `perl -pi -e 's/\bpalette_split_top\b/g_screen.split_top/g; s/\bpalette_split_bottom\b/g_screen.split_bottom/g; s/\bfb\[/g_screen.px[/g'` over `src/` (measured at commit 72e24b5: `fb` and the split names in 20 files, 43 uses of `ui_get_screen` and the screen literal). The bare `palette` global has 30 hits, many of them locals and parameters of the same name: rename it by hand, file by file, with the compiler as the check (`make` after removing the old declaration lists every remaining use).
- [ ] **Step 3: The screen bitmap.** `ui_get_screen` (`port/src/ui/ui.c:43-46`) and the literals `{SCREEN_W, SCREEN_H, &fb[0][0]}` (`port/src/ui/text.c:57`, `port/src/ui/view.c:2575`) become `video_get_screen()`; delete `ui_get_screen`.
- [ ] **Step 4: Tests.** Replace `in_dll(lib, "fb")`, `"palette"`, `"palette_split_top"`, `"palette_split_bottom"` (26 binds in 15 files at commit 72e24b5: `grep -rn 'in_dll(.*"\(fb\|palette\|palette_split_top\|palette_split_bottom\)")' tests`) by `dmtest.game.screen(lib).px`, `.palette`, `.split_top`, `.split_bottom`. A test that wrote zeros with `ctypes.memset` on `fb` writes `ctypes.memset(ctypes.addressof(screen(lib).px), 0, 320 * 200)`.
- [ ] **Step 5: `video_get_color(int16_t y, ...)`** (`int y` today, `port/src/ui/video.h:38`); its platform caller passes the row as `int16_t`.
- [ ] **Step 6:** `make format && make`; `uv run pytest -q tests/golden tests/port/test_ui.py tests/port/test_view.py tests/port/test_select.py tests/port/test_entrance.py tests/differential/test_csb_ending.py -n auto`. Expected: PASS, all goldens unchanged.
- [ ] **Step 7: Commit.** `git add -A src tests && git commit -m "refactor(video): one Screen struct for pixels and palettes"`

### Task 106: the view result

**Files:**
- Modify: `src/ui/view.h`, `src/ui/view.c`, `view_walls.c`, `view_things.c`, `view_internal.h`, `src/ui/command.c`, the state JSON writer and `src/control/control.c` (or `src/platform/control.c`), `tests/dmtest/game.py`, `tests/conftest.py` (BOOL globals), tests that read the outputs
- Test: `tests/golden/test_golden_rules.py`, `tests/golden/test_golden_protocol.py`, `tests/port/test_view.py`

**Interfaces:**
- Produces in `src/ui/view.h`:

```c
/* The outputs of the last view_draw: what the square ahead is, the click boxes and the piles. */
typedef struct {
    int16_t square_ahead_element; /* VIEW_ELEMENT_* */
    bool facing_alcove, facing_fountain, facing_vi_altar;
    Box clickable_boxes[VIEW_CLICK_BOX_COUNT]; /* in viewport coordinates, x -1: empty */
    uint16_t pile_top_objects[VIEW_CLICK_BOX_WALL];
    uint16_t pile_objects[VIEW_CLICK_BOX_WALL][VIEW_PILE_OBJECT_MAX]; /* in draw order */
    int16_t pile_object_counts[VIEW_CLICK_BOX_WALL];
} ViewResult;
/* The outputs of the last view. */
const ViewResult *view_get_result(void);
```

  `g_do_not_draw_fluxcages` stays a global: it is an input of the view.
- Produces in `tests/dmtest/game.py`: `class ViewResult(ctypes.Structure)` with the same fields and `def view_result(lib)` that calls `view_get_result` (restype `ctypes.POINTER(ViewResult)`) and returns `.contents`.

- [ ] **Step 1:** `View` (`view_internal.h`) gets a `ViewResult result;` field; the nine globals of `port/src/ui/view.c:266-272` and their declarations (`port/src/ui/view.h:21-27`) go; inside the view they are `g_view.result.x`.
- [ ] **Step 2: Callers** (measured at commit 72e24b5: 13 uses in `command.c`, 9 in `state_json.c`, 6 in `control.c`): each function that reads the result takes `const ViewResult *view = view_get_result();` once at its top. The read of `g_facing_vi_altar` in `drop_object` (Task 101) becomes `view->facing_vi_altar`.
- [ ] **Step 3: Tests** (`tests/conftest.py` 3, `test_gameloop.py` 1, `test_golden_rules.py` 3, `test_modes.py` 1 at commit 72e24b5) read `view_result(lib).facing_fountain` and so on; the three names leave `BOOL_GLOBALS`.
- [ ] **Step 4:** `make format && make`; `uv run pytest -q tests/golden tests/port/test_view.py tests/port/test_gameloop.py tests/port/test_modes.py tests/port/test_control.py -n auto`. Expected: PASS, golden protocol and rules unchanged.
- [ ] **Step 5: Commit.** `git add -A src tests && git commit -m "refactor(view): view_get_result replaces the output globals"`

### Task 107: copied code, dead exports, test markers

**Files:**
- Modify: `src/ui/ui.c`, `ui.h`, `command.c`, `select.c`, `advanced.c`, `gameloop.c`, `startend.c`, `menus.c/.h`, `status.c/.h`, `panel.c/.h`, `advanced.h`, `music.h` (markers only), `view.h`, `anim.h`, `select.h`, `frame.h`
- Test: `tests/golden/test_golden_screens.py`, `tests/port/test_save.py`, `tests/standard`

**Interfaces:**
- Produces in `src/ui/ui.h`: `void ui_xor_screen_box(const Box *box);` (`ui_xor_screen_zone` calls it).

- [ ] **Step 1: Point in box.** `box_contains` (D3) replaces the five loops: `port/src/ui/ui.c:309`, `command.c:483` (`is_in_box`: keeps its `x != -1` empty test as `b->x != -1 && box_contains(b, x, y)`), `select.c:181`, `advanced.c:104`, `gameloop.c:488`.
- [ ] **Step 2: XOR.** The XOR loop of `ui_xor_screen_zone` (`port/src/ui/ui.c:78-87`) becomes `ui_xor_screen_box`; `command_highlight_disable` (`port/src/ui/command.c:189-198`) calls it.
- [ ] **Step 3: `draw_viewport_icon`** (`port/src/ui/command.c:645-651`) equals `ui_draw_icon_to_viewport`: delete it, call `ui_draw_icon_to_viewport`.
- [ ] **Step 4: Save dialog.** `save_game_csb` and `startend_save_game` (`port/src/ui/startend.c:142-199`) share the "saving game" / "unable to save" dialog pair: one static `save_with_dialog()` that both call. Run `uv run pytest -q tests/port/test_save.py tests/port/test_csb_save.py` before and after.
- [ ] **Step 5: Dead exports** (finding 46 of `review-ui.md`, re-measured: run the `dead-api` check of `tests/standard` and take its `ui/` list): make each `static` (rename without the prefix, standard 4.2) or delete it when it has no caller at all (`ui_get_icon_in_slot_box` and the `slot_box_icon` cache of `port/src/ui/ui.c:30,276`; `command_set_game_input` if unused).
- [ ] **Step 6: Test markers.** Add `/* exported for the tests */` to the declarations that only the tests use (finding 47: `view_draw`, `anim_open`, `anim_next_frame`, `anim_close`, `select_item_box`, `g_music_track`, `g_music_last_track`, `g_music_requested_track` if D4 kept them, `g_frame_wait_depth`, `ui_get_slot_box_zone`; take the final list from the `dead-api` check).
- [ ] **Step 7:** `make format && make`; `uv run pytest -q tests/standard tests/golden/test_golden_screens.py tests/port/test_ui.py tests/port/test_select.py`. Delete the `open: epic 8` `dead-api` lines of `ui/`.
- [ ] **Step 8: Commit.** `git add -A src tests/standard/allowlist.txt && git commit -m "refactor(ui): shared helpers instead of copies, dead exports static"`

### Task 108: the remaining types of ui

**Files:**
- Modify: `src/ui/text.h/.c`, `dialog.c`, `select.h/.c`, `draw.h/.c`, `layout.h/.c`, `anim.h/.c`, `logo.h/.c`, `startend.h/.c`, `automap.h/.c`, `gameloop.h/.c`, callers in `src/platform/` and the tests
- Test: `tests/differential/test_text.py`, `tests/port/test_select.py`, `tests/differential/test_csb_ending.py`, `tests/port/test_ui.py`

- [ ] **Step 1: Pixel coordinates `int16_t`** (finding 18): `text.h:12-15` (`int x, y, w, h` → `int16_t`; `text_get_size(const char *s, int16_t *w, int16_t *h)`), `dialog.c:135` (`fill`), `select.h:30-34` (`select_key(int16_t key)`, `select_click(int16_t x, int16_t y)`, `select_item_box(int16_t item, Box *box)`, `select_screen` and `select_csb_choice` return `int16_t`). The callers that widen and narrow around `text_get_size` (`port/src/ui/ui.c:116-121`, `startend.c:245-250`, `dialog.c:76-79`) drop their conversions.
- [ ] **Step 2: bool results** (finding 19): `text_init`, `layout_init`, `anim_open`, `anim_play`, `logo_play`, `draw_shrink` return `bool` (true on success); `layout_get_zone_box` returns `bool`. Every caller changes `!f()`/`f() < 0`/`f() == 0` to the bool form; `gameloop.c:60` reads `gfx_open(path) && layout_init() && text_init() && ui_init()` (after D's `gfx_open` returns `bool`; check). Tests that assert `== 0` or `== -1` on these assert `is True`/`is False` (harness adaptation, the outcome does not change).
- [ ] **Step 3: bool flags:** `int mask_flip` (`draw.h:17`), `int16_t party_dead` (`startend.h:42` `startend_run_endgame(bool party_dead)`), `int16_t loaded` (`startend.c:373`), `anim.c` locals `palette_changed`, `input_stops`, `running` (`port/src/ui/anim.c:65`, `:322`).
- [ ] **Step 4: Box by pointer** in `automap_draw(Bitmap *target, const Box *area, int16_t map)` and its static helpers (`port/src/ui/automap.c:98-203`).
- [ ] **Step 5: `select_item_box`** (`port/src/ui/select.c:166-177`) returns false when `text_get_size` returns false (R8.9; items are constants, so no test).
- [ ] **Step 6: `draw_scroll_line`** modifies its `char *text` in place (`port/src/ui/panel.c`, was `ui_champion.c:694-709`): its comment says so ("changes text: the rune words become rune symbols").
- [ ] **Step 7:** `make format && make && make windows`; `uv run pytest -q tests/differential/test_text.py tests/port/test_select.py tests/port/test_ui.py tests/differential/test_csb_ending.py tests/port/test_entrance.py tests/golden -n auto`. Expected: PASS. Delete the `open: epic 8` `bool`, `type`, `result` lines of `ui/`.
- [ ] **Step 8: Commit.** `git add -A src tests && git commit -m "refactor(ui): int16_t pixel coordinates, bool results and flags"`

### Task 109: comments and header paragraphs of ui

**Files:**
- Modify: every header of `src/ui/` without a purpose paragraph, the comments listed below, `docs/todo.md`, the deviations list of the formats doc
- Test: `tests/standard`

- [ ] **Step 1: Purpose paragraphs** (finding 28): `draw.h`, `text.h`, `layout.h`, `video.h` get one paragraph each: what the module owns and what it must not do. The headers made in this package have theirs.
- [ ] **Step 2: Original names in comments** (finding 16), rewritten in game terms: `ui.h` ("MENU.C state" goes with Task 94; "draw_viewport(0): the viewport bitmap to zone 7" → "draws the dungeon view bitmap into its zone"), `draw.c:6`, `:85`, `:89`, `gameloop.c:454`, `text.c:73`, `layout.c:153`, `:166`, `gameloop.h:27`, `startend.h:28` ("Command 140" → "The save command"), `startend.h:45` ("Sensor 18" → "The endgame sensor"), `ui.h:69` ("call after kaos_load" → "call after tables_load"). Add each original name to `docs/reference/kaos-symbols.csv` if it is missing.
- [ ] **Step 3: Outdated comments** (finding 27): the two comment blocks of `ui_set_screen_palette` (`port/src/ui/ui.c:147-149`): the first describes `ui_set_palette`; move it there. `dialog.c:148` explains nothing to a reader: delete.
- [ ] **Step 4: Comment place:** the one-line comment of each public ui function stands in its header; a definition repeats none (standard 9.1). The `comment` check lists the places; move them.
- [ ] **Step 5: Markers.** `grep -rn "ponytail" src/ui` must find nothing; the panel marker (`port/src/ui/ui_champion.c:940`, "the leader hand object name stays") becomes a deviation comment and a deviations line.
- [ ] **Step 6:** `make`; `uv run pytest -q tests/standard`. Delete the `open: epic 8` `comment`, `todo`, `header` lines of `ui/`.
- [ ] **Step 7: Commit.** `git add -A src docs tests/standard/allowlist.txt && git commit -m "docs(ui): header paragraphs and comments in game terms"`

---

## Package H: control/ and platform/ (Epics 10, 11)

Package H runs after Package G. It uses the names that the earlier packages produce: the `turn_`-free map helpers
(`direction_left/right/back`, `map_step`, Package E), the champion indices instead of ordinals
(`g_inventory_champion_index`, `g_acting_champion_index`, `g_candidate_champion_index`, Package F),
`TABLES_MOUSE_*` instead of `UI_INPUT_*` (Package D), `view_get_result()` instead of
`g_dungeon_view_clickable_boxes`, `g_square_ahead_element`, `g_facing_*` (Package G), `g_screen` instead of `fb`
and `palette` (Package G), `panel_format_object_text` and `panel_get_object_meter` instead of
`panel_draw_object_text` and `panel_draw_object_meter` (Package G). Assumption: these names exist with these
meanings at the start of Package H; the first step of each task that uses one greps for it, and the task uses
the name it finds.

Facts of the split (commit `72e24b5`):

- `port/src/platform/control.c` (1,431 lines) holds the socket (`control.c:53-57`, `869-888`, `1331-1431`), the
  request reader (`223-256`, `1108-1142`), the action handlers and their table (`129-221`, `260-675`), the PNG
  writer (`677-747`), the witness and news records (`749-867`, `1037-1095`), the turn and batch machine
  (`985-1035`, `1097-1195`, `1317-1329`) and the dispatch (`1197-1315`). File-static state: `turn` (`59-71`),
  `batch` (`75-84`), `seen` (`87-92`), `witnessed` (`106-112`), `look_text` (`115-119`), `load_failed` (`121`).
- `port/src/platform/state_json.c` (1,077 lines) holds the JSON writer (`58-178`) and the state, view and map
  writers; `state_json.h:10` defines `DECIMAL_BASE` for `strtoll` (`control.c:254`) and the map grid
  (`state_json.c:1046`).
- `control_poll` (`control.c:1367-1421`) first advances a pending turn (record the witnessed things, end the turn
  or run one frame of commands), then accepts a client and handles the request lines. The order matters: a
  request that starts a turn gets its first turn frame in the next frame. The split keeps this order.
- The tests drive the protocol through the library: `control_open`, `control_poll`, `control_close`,
  `control_is_turn_pending`, `state_json_get_string` (`tests/test_control.py:37-78`, `tests/test_modes.py:47-664`,
  `tests/test_csb_adventure.py:44-274`). After Package C the helper `pump` lives in `tests/dmtest/control.py`.
- `port/src/platform/main.c` (736 lines): debug modes `show`, `view_start`, `view_key` (`main.c:39-127`), input
  (`129-301`), start screens and mode (`216-574`), frame (`576-631`), options (`633-677`), `main` (`679-736`).
  `parse_options` ignores unknown options, does not check numbers (`atoll`, `main.c:651, 657`), and checks the
  turn-based mode inside the loop (`main.c:667-670`). `tests/test_select.py:318-320` checks the message
  `--adventure must be dm or csb` on stderr; the new parser keeps the messages.
- `DM_FILTER` is read in `platform.c:45` and documented in `README.md:252` and `CHANGELOG.md:258` (the history
  stays). `DM_AUTO_ENTER` stays (`main.c:681`, used by the tests).

Rulings of this section (the main agent lists them for the user):

- R10.2 says that `app_run_frame` calls `control_is_turn_pending` and `control_end_turn` after `transport_poll`.
  The plan puts that call into `transport_poll` before it reads, because `control_poll` does it in this order
  (`control.c:1370-1385`) and a different order changes the frame of each turn (the golden protocol snapshot).
  `app_run_frame` calls `transport_poll` only.
- `control_end_turn` advances the pending turn by one frame (its name is fixed by the PRD); the comment says so.
- `control_drop_client` is added to the PRD API: the transport calls it when the client is gone, so that a batch
  stops as it does today (`control.c:1386-1389`).
- "The list scanner reuses the key search" (R10.1): the key search and the list scanner share one static
  `skip_string` helper in `request.c`; the object case of the list scanner does not call the key search, because
  the key search finds a key, not the end of an object.
- The transport moves in stream H1 together with the dispatch (one split of one file, Task 115), not in the tail:
  the protocol module `control_` and the socket cannot share the prefix in one tree. The tail is the application
  (`app.c`, `main.c`), which needs `transport_*` and `options_*`.
- The debug view is two functions (`debug_view_open`, `debug_view_key`): the key handler runs from the input
  loop, the open function from the start.
- The turn-based check of Windows moves from the option loop to `options_parse` after the loop (Task 120, when
  the transport exists in the same tree).

#### Stream H1: the protocol in control/

Shared files: `Makefile` (layer list, transport file choice; Task 118 adds `debug_view.c` to the SDL file list),
`src/platform/main.c` (Task 115 changes six call sites; Task 118 moves the debug functions out),
`src/engine/move.h`, `src/engine/move.c` (Task 114: the type of `g_party_teleport_count`),
`tests/dmtest/control.py`, `tests/standard/allowlist.txt` (union merge), `tests/standard/test_standard.py`
(Task 113: the base argument of `strtol`).

### Task 110: control/json: the JSON writer

**Files:**
- Create: `src/control/json.h`, `src/control/json.c`
- Modify: `src/platform/state_json.h` (remove the writer, include `control/json.h`), `src/platform/state_json.c`
  (remove `put`, `separate`, `json_*`, `key_int`, `key_str`, `key_bool`: `port/src/platform/state_json.c:58-158`;
  call sites of `key_*` become `json_key_*`), `src/platform/control.c` (`json_bool(&j, 0/1)` becomes `false/true`:
  `port/src/platform/control.c:895, 925, 950, 978`; `free(j.s)` becomes `json_free`), `Makefile` (layer list)
- Test: none new (the golden protocol snapshot of Task 1 is the gate)

**Interfaces:**
- Consumes: the `Makefile` of Package B with its layer list (Assumption: a variable that lists the layers, e.g.
  `LAYERS = data engine ui platform`; step 1 checks it).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_JSON_H
#define DM_CONTROL_JSON_H
/* Prefixes: json_ */

/* A JSON text writer for the protocol replies. It only writes; an allocation failure aborts. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A growing JSON text. Start with {0}; s points to owned memory, release it with json_free. */
typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Json;

/* Setup */
/* Releases the text and leaves an empty writer. */
void json_free(Json *j);

/* Values: a value or a key gets its comma before it when it is not the first in its object or array. */
/* Appends text as it is. */
void json_raw(Json *j, const char *text);
/* Appends a key and its colon. */
void json_key(Json *j, const char *key);
/* Appends a number. */
void json_int(Json *j, int64_t value);
/* Appends true or false. */
void json_bool(Json *j, bool value);
/* Appends null. */
void json_null(Json *j);
/* Appends a quoted string with the JSON escapes. */
void json_str(Json *j, const char *text);
/* Opens an object '{' or an array '['. */
void json_open(Json *j, char bracket);
/* Closes an object '}' or an array ']'. */
void json_close(Json *j, char bracket);

/* Members */
/* Appends a key with a number. */
void json_key_int(Json *j, const char *key, int64_t value);
/* Appends a key with a string. */
void json_key_str(Json *j, const char *key, const char *text);
/* Appends a key with true or false. */
void json_key_bool(Json *j, const char *key, bool value);

#endif
```

- [ ] **Step 1: Check the inputs.** Run `grep -n "LAYERS\|control" Makefile` and `grep -n "control" tests/standard/test_standard.py | head`.
  Expected: the Makefile lists the layers in one variable; the layer check of Package C knows `control` between
  `ui` and `platform` (`docs/coding-standard.md` rule 1.1). If the Makefile has no layer variable, add `control`
  where the Makefile lists `ui` and `platform`.
- [ ] **Step 2: Create `src/control/json.h`** with the header above.
- [ ] **Step 3: Create `src/control/json.c`.** Move the bodies of `put`, `separate`, `json_raw`, `json_open`,
  `json_close`, `json_null`, `json_bool`, `json_int`, `json_str`, `json_key` from
  `port/src/platform/state_json.c:58-143` and `key_int`, `key_str`, `key_bool` (`145-160`) as `json_key_int`,
  `json_key_str`, `json_key_bool`; `JSON_START_SIZE` and `INT64_TEXT_SIZE` (`state_json.c:30-31`) move with them.
  `json_bool` takes `bool`. Add:

```c
void json_free(Json *j)
{
    free(j->s);
    *j = (Json){0};
}
```

  The public functions stand in header order; `put` and `separate` stand directly above their first caller.
- [ ] **Step 4: Remove the writer from `state_json.h` and `state_json.c`.** `state_json.h` includes
  `control/json.h`; replace `key_int(`, `key_str(`, `key_bool(` with `json_key_int(`, `json_key_str(`,
  `json_key_bool(` (run `grep -c "key_int(\|key_str(\|key_bool(" src/platform/state_json.c` before: every hit
  changes); `key_bool(j, key, 1)` becomes `json_key_bool(j, key, true)`.
- [ ] **Step 5: Use `bool` and `json_free` in `control.c`.** `json_bool(&j, 0)` → `json_bool(&j, false)`,
  `json_bool(&j, 1)` → `json_bool(&j, true)` (4 sites); `free(j->s)` in `send_line` (`control.c:887`) →
  `json_free(j)`.
- [ ] **Step 6: Add `control` to the layer list of the `Makefile`**, so that `src/control/*.c` goes into the game
  and the library.
- [ ] **Step 7: Build.** Run `make`. Expected: 0 warnings.
- [ ] **Step 8: Run the protocol tests.** Run `uv run pytest -q tests/port/test_control.py tests/golden/test_golden_protocol.py`.
  Expected: PASS (Assumption: Task 1 named the golden test `test_golden_protocol.py` and Package C moved it to
  `tests/golden/`; use the path that `ls tests/golden/*protocol*` shows).
- [ ] **Step 9: Commit.**

```bash
git add src/control/json.h src/control/json.c src/platform/state_json.h src/platform/state_json.c src/platform/control.c Makefile
git commit -m "refactor: control/json holds the JSON writer of the protocol"
```

### Task 111: control/request: the request reader

**Files:**
- Create: `src/control/request.h`, `src/control/request.c`, `tests/port/test_request.py`
- Modify: `src/platform/control.c` (remove `find_value`, `get_string`, `get_int`: `port/src/platform/control.c:223-256`;
  `next_element`: `1106-1142`; callers use `request_*`)
- Test: `tests/port/test_request.py` (a parser: on the test list)

**Interfaces:**
- Consumes: `dmtest.lib.load_lib()` (Package B).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_REQUEST_H
#define DM_CONTROL_REQUEST_H
/* Prefixes: request_ */

/* The reader of one request line. Contract: a key occurs once per request; a string holds no '"' and no
   escape; objects nest no deeper than the args of an action. A key is found by a flat search of "key". */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Result of request_next_item. */
typedef enum {
    REQUEST_ITEM_END,   /* the list ends */
    REQUEST_ITEM_FOUND, /* out holds the next item */
    REQUEST_ITEM_BAD    /* the next item is not a "name" or an {...} object, or is too long */
} RequestItem;

/* Queries */
/* The text after "key" and its colon in line, or NULL when the key is missing. */
const char *request_find_value(const char *line, const char *key);
/* Copies the string value of key into out, cut to size; false when the key is missing or not a string. */
bool request_get_string(const char *line, const char *key, char *out, size_t size);
/* The integer value of key, or missing when the key is missing or not a number. */
int64_t request_get_int(const char *line, const char *key, int64_t missing);
/* Copies the next item of a list ("name" or {...}) at *cursor into out and moves *cursor past it. */
RequestItem request_next_item(const char **cursor, char *out, size_t size);

#endif
```

- [ ] **Step 1: Write the failing test** `tests/port/test_request.py`:

```python
# SPDX-License-Identifier: GPL-3.0-or-later
"""The request reader of the control protocol: the flat key search and the list scanner."""

import ctypes

import pytest

from dmtest.lib import load_lib
from dmtest import constants as C

lib = load_lib()
lib.request_find_value.restype = ctypes.c_char_p
lib.request_find_value.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.request_get_string.restype = ctypes.c_bool
lib.request_get_string.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
lib.request_get_int.restype = ctypes.c_int64
lib.request_get_int.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int64]
lib.request_next_item.restype = ctypes.c_int
lib.request_next_item.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_char_p, ctypes.c_size_t]


def get_string(line, key, size=32):
    out = ctypes.create_string_buffer(size)
    found = lib.request_get_string(line.encode(), key.encode(), out, size)
    return out.value.decode() if found else None


@pytest.mark.parametrize("line, key, expected", [
    ('{"cmd": "act", "action": "move_forward"}', "action", "move_forward"),
    ('{"cmd":"act","args":{"choice":"resurrect"}}', "choice", "resurrect"),
    ('{"cmd": "wait", "ticks": 5}', "cmd", "wait"),
    ('{"cmd": "act"}', "action", None),
    ('{"ticks": 5}', "ticks", None),
])
def test_get_string(line, key, expected):
    assert get_string(line, key) == expected


def test_get_string_cuts_to_size():
    assert get_string('{"path": "/tmp/a-long-file-name.png"}', "path", size=8) == "/tmp/a-"


@pytest.mark.parametrize("line, expected", [
    ('{"ticks": 25}', 25),
    ('{"ticks":-3}', -3),
    ('{"ticks": "x"}', -1),
    ('{"cmd": "wait"}', -1),
])
def test_get_int(line, expected):
    assert lib.request_get_int(line.encode(), b"ticks", -1) == expected


def items(text):
    """All items of a list body (the text after '['), and the last result."""
    buffer = ctypes.create_string_buffer(text.encode())
    cursor = ctypes.c_char_p(ctypes.addressof(buffer))
    out = ctypes.create_string_buffer(256)
    found = []
    while (result := lib.request_next_item(ctypes.byref(cursor), out, 256)) == C.REQUEST_ITEM_FOUND:
        found.append(out.value.decode())
    return found, result


def test_list_of_names_and_objects():
    found, result = items('"move_forward", {"action": "take_object", "args": {"cell": 2}}, "turn_left"]}')
    assert found == ['"move_forward"', '{"action": "take_object", "args": {"cell": 2}}', '"turn_left"']
    assert result == C.REQUEST_ITEM_END


@pytest.mark.parametrize("text", ['5]', '"move_forward', '{"action": "pass"', '{"action": "pass}'])
def test_bad_item(text):
    assert items(text)[1] == C.REQUEST_ITEM_BAD


def test_item_too_long():
    buffer = ctypes.create_string_buffer(b'"move_forward"]')
    cursor = ctypes.c_char_p(ctypes.addressof(buffer))
    out = ctypes.create_string_buffer(4)
    assert lib.request_next_item(ctypes.byref(cursor), out, 4) == C.REQUEST_ITEM_BAD
```

  Assumption: `dmtest.constants` reads the members of a result `enum` (R12.1, Package C); if it does not, the test
  uses `REQUEST_ITEM_END, REQUEST_ITEM_FOUND, REQUEST_ITEM_BAD = range(3)` from the header order and the task
  reports it.
- [ ] **Step 2: Run it to see it fail.** Run `uv run pytest -q tests/port/test_request.py`. Expected: FAIL,
  `AttributeError: ... request_find_value`.
- [ ] **Step 3: Create `src/control/request.h`** with the header above.
- [ ] **Step 4: Create `src/control/request.c`.** Move `find_value`, `get_string`, `get_int` from
  `port/src/platform/control.c:223-256` as `request_find_value`, `request_get_string`, `request_get_int`, and
  `next_element` (`1106-1142`) as `request_next_item(const char **cursor, char *out, size_t size)`: it reads and
  moves `*cursor` instead of `batch.next`, and returns `REQUEST_ITEM_END` (was 0), `REQUEST_ITEM_FOUND` (1),
  `REQUEST_ITEM_BAD` (-1). `KEY_PATTERN_SIZE` moves with them. The base of `strtoll` is the literal `10` (the
  standard allows the base argument; Task 113 adapts the check). One static helper serves both scanners:

```c
/* The closing quote of the string that starts at the quote p, or NULL. */
static const char *skip_string(const char *p)
{
    return strchr(p + 1, '"');
}
```

  `request_get_string` and the string case and the in-object strings of `request_next_item` call `skip_string`.
- [ ] **Step 5: Use the reader in `control.c`.** Replace `find_value(`, `get_string(`, `get_int(` with the
  `request_` names (run `grep -c "find_value(\|get_string(\|get_int(" src/platform/control.c` before; every hit
  changes). `batch_next` (`control.c:1145-1165`) calls
  `request_next_item(&batch.next, element, sizeof element)` and compares with `REQUEST_ITEM_FOUND` and
  `REQUEST_ITEM_END`.
- [ ] **Step 6: Build and run the tests.** Run `make` and
  `uv run pytest -q tests/port/test_request.py tests/port/test_control.py tests/golden/test_golden_protocol.py`.
  Expected: 0 warnings; PASS.
- [ ] **Step 7: Commit.**

```bash
git add src/control/request.h src/control/request.c src/platform/control.c tests/port/test_request.py
git commit -m "refactor: control/request reads the request lines of the protocol"
```

### Task 112: control/png: the screenshot writer

**Files:**
- Create: `src/control/png.h`, `src/control/png.c`, `tests/port/test_png.py`
- Modify: `src/platform/control.c` (remove `port/src/platform/control.c:676-747`; `handle_screenshot` calls
  `png_write`)
- Test: `tests/port/test_png.py` (a data format: on the test list)

**Interfaces:**
- Consumes: `g_screen` and the color accessors of `ui/video.h` (Package G); `crc32_update` (`data/crc32.h`);
  `wr32` (`data/bytes.h`, Package D).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_PNG_H
#define DM_CONTROL_PNG_H
/* Prefixes: png_ */

/* The screenshot of the program player: the game screen as an RGB PNG with stored deflate blocks. */

#include <stdbool.h>

/* Writes the game screen in its colors to path; false when the file cannot be written. */
bool png_write(const char *path);

#endif
```

- [ ] **Step 1: Write the failing test** `tests/port/test_png.py`:

```python
# SPDX-License-Identifier: GPL-3.0-or-later
"""The PNG screenshot: valid chunks, valid zlib data, one RGB row per screen row."""

import ctypes
import struct
import zlib

from dmtest.lib import load_lib
from dmtest import constants as C

lib = load_lib()
lib.png_write.restype = ctypes.c_bool
lib.png_write.argtypes = [ctypes.c_char_p]


def chunks(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    at = 8
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        (crc,) = struct.unpack(">I", data[at + 8 + length:at + 12 + length])
        assert crc == zlib.crc32(kind + body)
        yield kind, body
        at += 12 + length


def test_png_of_the_screen(tmp_path):
    path = tmp_path / "screen.png"
    assert lib.png_write(str(path).encode())
    found = dict(chunks(path.read_bytes()))
    assert list(found) == [b"IHDR", b"IDAT", b"IEND"]
    width, height, depth, color = struct.unpack(">IIBB", found[b"IHDR"][:10])
    assert (width, height, depth, color) == (C.SCREEN_W, C.SCREEN_H, 8, 2)
    raw = zlib.decompress(found[b"IDAT"])
    assert len(raw) == (1 + 3 * C.SCREEN_W) * C.SCREEN_H
    assert all(raw[y * (1 + 3 * C.SCREEN_W)] == 0 for y in range(C.SCREEN_H))  # filter byte: none


def test_png_to_a_missing_folder(tmp_path):
    assert not lib.png_write(str(tmp_path / "missing" / "screen.png").encode())
```

- [ ] **Step 2: Run it to see it fail.** Run `uv run pytest -q tests/port/test_png.py`. Expected: FAIL,
  `AttributeError: ... png_write`.
- [ ] **Step 3: Create `src/control/png.h`** with the header above.
- [ ] **Step 4: Create `src/control/png.c`.** Move `ZLIB_METHOD` ... `PNG_IHDR_SIZE`, `png_chunk` (as the static
  `write_chunk`) and `write_png` (as `png_write`) from `port/src/platform/control.c:678-747`. Name the remaining
  numbers: the `enum { ROW, RAW, BLOCK }` and `enum { BLOCKS, ZLIB }` (`control.c:699-700`) become `#define`
  lines (`PNG_ROW_BYTES`, `PNG_RAW_BYTES`, `PNG_STORED_BLOCK_MAX 65535`, `PNG_BLOCKS`, `PNG_ZLIB_BYTES`); the
  bytes `8` (bits per channel), `2` (RGB), the signature size and the chunk header size get names. `png_write`
  checks every `fwrite`:

```c
/* Writes a chunk: length, type, data, CRC; false when a write fails. */
static bool write_chunk(FILE *file, const char *type, const uint8_t *data, uint32_t length)
{
    uint8_t head[PNG_CHUNK_HEAD_BYTES];
    uint8_t tail[PNG_CHUNK_CRC_BYTES];
    wr32(head, length);
    memcpy(head + PNG_LENGTH_BYTES, type, PNG_TYPE_BYTES);
    wr32(tail, crc32_update(crc32_update(0, head + PNG_LENGTH_BYTES, PNG_TYPE_BYTES), data, length));
    return fwrite(head, 1, sizeof head, file) == sizeof head && fwrite(data, 1, length, file) == length &&
           fwrite(tail, 1, sizeof tail, file) == sizeof tail;
}
```

  `png_write` returns `false` when a chunk or the signature write fails, and closes the file on every path.
  The pixel read uses the screen of Package G (`g_screen.px[y][x]`, the color of the row half; step 1 of
  this task greps `src/ui/video.h` for the current accessor of `video_get_color`).
- [ ] **Step 5: Call `png_write` from `handle_screenshot`** (`port/src/platform/control.c:1258-1272`) and include
  `control/png.h`.
- [ ] **Step 6: Build and run the tests.** Run `make` and `uv run pytest -q tests/port/test_png.py tests/port/test_control.py`.
  Expected: 0 warnings; PASS.
- [ ] **Step 7: Commit.**

```bash
git add src/control/png.h src/control/png.c src/platform/control.c tests/port/test_png.py
git commit -m "refactor: control/png writes the screenshot and checks every write"
```

### Task 113: control/state_json: the state writers with verbs

**Files:**
- Move: `git mv src/platform/state_json.h src/control/state_json.h`, `git mv src/platform/state_json.c src/control/state_json.c`
- Modify: `src/control/state_json.h`, `src/control/state_json.c`, `src/platform/control.c` (callers),
  `tests/dmtest/control.py` (`state_json_get_string` → `state_get_string`), `tests/port/test_control.py`,
  `tests/standard/test_standard.py` (the base argument of `strtol`), `docs/llm/llm-mode.md` (names in the C API
  section, Task 116 rewrites the section)
- Test: none new; the golden protocol snapshot is the gate

**Interfaces:**
- Consumes: `control/json.h` (Task 110).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_STATE_JSON_H
#define DM_CONTROL_STATE_JSON_H
/* Prefixes: state_ */

/* The game as JSON for the program player: the party, the champions, what the party sees, and the explored
   map (docs/llm/llm-mode.md). It reads the game and changes nothing. */

#include <stdint.h>
#include "control/json.h"

/* Parts of state_get_string. */
#define STATE_PART_STATE 0
#define STATE_PART_VIEW  1
#define STATE_PART_MAP   2

/* Counts of the visible things. */
#define STATE_VISIBLE_GROUPS_MAX 12 /* four depths of three squares; a square holds one group */

/* A thing on a square that the party sees now: the view row (depth), the side name, and the map square. */
typedef struct {
    int16_t depth;
    int16_t x;
    int16_t y;
    const char *side;
    uint16_t thing;
} StateSeenThing;

/* Writers */
/* Appends the party, champions, messages, actions, runes and panels as one object. */
void state_write_state(Json *j);
/* Appends the squares in front of the party, 4 deep and the sides, as one object. */
void state_write_view(Json *j);
/* Appends the explored squares of the party map as text rows. */
void state_write_map(Json *j);
/* Appends the name of an explosion type, or its number when it has no name. */
void state_write_explosion_name(Json *j, uint16_t type);

/* Queries */
/* The creature groups (their things) on the squares the party sees now. Returns the count. */
int state_get_visible_groups(uint16_t groups[STATE_VISIBLE_GROUPS_MAX]);
/* The things of a thing type on the squares the party sees now (no walls), in view order. Returns the count. */
int state_get_visible_things(uint16_t type, StateSeenThing *out, int max);
/* The name of a creature type, or "unknown". */
const char *state_get_creature_name(uint16_t type);
/* One part (STATE_PART_*) as a new string; release it with free. */
char *state_get_string(int16_t part); /* exported for the tests */

#endif
```

- [ ] **Step 1: Find the users.** Run `grep -rn "state_json_\|StateJsonSeenThing\|STATE_JSON_\|DECIMAL_BASE" src tests tools docs .claude`.
  Expected: `src/platform/state_json.*`, `src/platform/control.c`, the control tests and `dmtest/control.py`,
  `docs/llm/llm-mode.md`. Every hit changes in this task.
- [ ] **Step 2: Move the files** with `git mv` into `src/control/` and write the header above; the include in
  `control.c` becomes `control/state_json.h`.
- [ ] **Step 3: Rename** with this script (run from the repo root):

```bash
for pair in state_json_write_state:state_write_state state_json_write_view:state_write_view \
    state_json_map:state_write_map state_json_explosion_name:state_write_explosion_name \
    state_json_write_visible_groups:state_get_visible_groups state_json_write_visible_things:state_get_visible_things \
    state_json_creature_name:state_get_creature_name state_json_get_string:state_get_string \
    StateJsonSeenThing:StateSeenThing STATE_JSON_VISIBLE_GROUPS_MAX:STATE_VISIBLE_GROUPS_MAX \
    STATE_JSON_STATE:STATE_PART_STATE STATE_JSON_VIEW:STATE_PART_VIEW STATE_JSON_MAP:STATE_PART_MAP; do
  old=${pair%%:*}; new=${pair##*:}
  grep -rlw "$old" src tests tools docs .claude | xargs sed -i '' "s/\b$old\b/$new/g"
done
```

  Then delete the `enum { STATE_JSON_STATE, ... }` line of the old header (the `#define` block replaces it).
- [ ] **Step 4: One look per square.** Replace the strings of `square_name` (`port/src/platform/state_json.c:530-547`)
  and the `strcmp` chains (`state_json.c:930, 937, 962, 999-1016`) with a result type and one name table:

```c
/* The look of a square: what the player sees there. */
typedef enum {
    LOOK_WALL,
    LOOK_FLOOR,
    LOOK_PIT,
    LOOK_STAIRS_UP,
    LOOK_STAIRS_DOWN,
    LOOK_DOOR,
    LOOK_TELEPORTER
} SquareLook;

static const char *const look_names[] = {"wall", "floor", "pit", "stairs_up", "stairs_down", "door", "teleporter"};
static const char look_chars[] = {'#', '.', 'O', '<', '>', 'D', 'T'};
```

  `square_look(uint8_t square)` returns the `SquareLook`; the JSON writes `look_names[look]`; `map_char` uses
  `look_chars[look]` with the two special cases it has today (open or destroyed door `'d'`, pressure pad `'_'`).
  Every `!strcmp(name, "wall")` becomes `look == LOOK_WALL`.
- [ ] **Step 5: Truth values.** `WallFace` (`state_json.c:575-580`): the eight `int` flags become `bool`, one
  field per line; `wall_face` sets `true`; `int wall_ahead` (`state_json.c:945`) becomes `bool`;
  `ornament_json(..., 1)` passes `true`. `object_json(Json *j, uint16_t thing, int held)` loses `held` (every
  caller passes 1: `state_json.c:349, 446, 482`); the branch for `held == 0` goes.
- [ ] **Step 6: Names and includes.** `DECIMAL_BASE` goes: the map grid writes `x % 10` through a named constant
  `MAP_GRID_DIGITS 10` (a count of grid digits); `request.c` passes `10` as the base of `strtoll`. Remove the
  include of `ui/command.h` (`port/src/platform/state_json.c:11`: no `command_`, `COMMAND_` or `CLICK_` name is
  used). The `teleports`-free names of Package E (`direction_back` for `turn_back`, `map_step` for
  `game_update_map_coordinates_after_relative_movement`) are used where `state_json.c:566-572, 759` call them
  (Assumption: Package E renamed them already; grep).
- [ ] **Step 7: Let the decimal check accept the base argument.** In `tests/standard/test_standard.py`, the
  decimal check skips the third argument of a `strtol`, `strtoll`, `strtoul` or `strtoull` call when it is `10`
  (Assumption: the check is a regex per line; add the call pattern to its skip list). This is the exception that
  `docs/coding-standard.md` rule 3.1 needs; add the sentence "The base argument of `strtol` and its siblings is a
  plain number." to rule 3.1 in the same commit.
- [ ] **Step 8: Build and run the tests.** Run `make` and
  `uv run pytest -q tests/port/test_control.py tests/golden/test_golden_protocol.py tests/standard`.
  Expected: 0 warnings; PASS.
- [ ] **Step 9: Commit.**

```bash
git add -A src/control src/platform tests docs
git commit -m "refactor: control/state_json writes the state with verbs and one look per square"
```

### Task 114: control/turn: the turn, the batch, and what the party saw

**Files:**
- Create: `src/control/turn.h`, `src/control/turn.c`
- Modify: `src/platform/control.c` (remove the turn and batch machine, the witness and news records:
  `port/src/platform/control.c:59-121, 302-323, 749-867, 903-995, 1036-1105, 1145-1195, 1317-1329`),
  `src/engine/move.h`, `src/engine/move.c` (`g_party_teleport_count` becomes `int16_t`:
  `port/src/engine/move.h:24`, `move.c:24`)
- Test: none new (the golden protocol snapshot and `tests/port/test_control.py` cover the turn results)

**Interfaces:**
- Consumes: `control/json.h` (H1), `control/request.h` (H2), `control/state_json.h` (H4), `PANEL_LINE_SIZE`,
  `PANEL_LINE_COUNT` (`ui/panel.h`, Package G; today `ui/ui.h`).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_TURN_H
#define DM_CONTROL_TURN_H
/* Prefixes: turn_ */

/* The turns of the program player: an action or a wait runs game ticks until the party can move again, a batch
   runs its actions as turns until news, and the reply tells what the party saw. No socket, no input. */

#include <stdbool.h>
#include <stdint.h>
#include "control/json.h"
#include "ui/panel.h"

/* Turn lengths in game ticks. */
#define TURN_MIN_TICKS      2 /* an action shows its result, then the action area returns */
#define TURN_MAX_TICKS      64
#define TURN_WAIT_MAX_TICKS 5000

/* Gives the input of action name with the args in request: NULL, or the reason why it cannot. */
typedef const char *(*TurnActionFn)(const char *name, const char *request);

/* Setup */
/* Forgets the turn, the batch and what the party saw. */
void turn_reset(void);
/* The client is gone: the pending turn ends without a reply and the batch stops. */
void turn_drop_client(void);

/* Queries */
/* True while a turn runs: the frame loop then runs game frames and ticks. */
bool turn_is_pending(void);
/* True when the party died or the game is won: no turn can follow until load or new_game. */
bool turn_is_game_over(void);

/* Actions */
/* Remembers the party place before an action and forgets the facts of the last action. */
void turn_begin_action(void);
/* The action is a step: forward and right squares from the party direction. */
void turn_set_step(int16_t forward, int16_t right);
/* The action is a turn of the party: 1 right, -1 left. */
void turn_set_rotation(int16_t rotation);
/* The damage of a chosen action; a batch keeps each one. */
void turn_add_damage(int16_t damage);
/* The eye panel text of a look action. */
void turn_set_look(const char *name, const char lines[][PANEL_LINE_SIZE], int16_t count);

/* Turns */
/* Starts the record of what the party sees during the request. */
void turn_start_witnessing(void);
/* Remembers what the news of a wait or a batch compares. */
void turn_take_snapshot(void);
/* Starts a turn of min to max ticks; a wait ends early on news. */
void turn_start(uint16_t min_ticks, uint16_t max_ticks, bool wait_only);
/* Starts a batch over the list body of an act_many request; true when reply already holds the answer. */
bool turn_start_batch(const char *list, TurnActionFn run, Json *reply);
/* One frame of the pending turn: true when the request ended and reply holds its result. */
bool turn_run_frame(Json *reply);

#endif
```

- [ ] **Step 1: Create `src/control/turn.h`** with the header above.
- [ ] **Step 2: Move the state.** `turn`, `batch`, `seen`, `witnessed`, `look_text` and their types
  (`port/src/platform/control.c:59-119`) move into `turn.c` as file-static state; with five file-scope records
  the file keeps them in one `static struct { ... } turn;` with the members `now` (the turn), `batch`, `seen`,
  `witnessed`, `look` (rule 7.2). `BATCH_DAMAGE_MAX` becomes a `#define`; `int64_t frames, min_ticks, max_ticks`
  become `uint16_t min_ticks, max_ticks` and `uint32_t frames`; `int teleports` becomes `int16_t`;
  `int ran`, `int damage_count` stay `int` (C counts). `StateJsonSeenThing` is `StateSeenThing`.
- [ ] **Step 3: Move the functions** into `turn.c`, static unless the header names them:
  `is_game_over` → `turn_is_game_over`; `turn_bumped` → static `is_bumped`; `turn_displaced` → static
  `displaced_reason`; `record_witnessed`, `start_witnessing` (→ `turn_start_witnessing`), `seen_place_json`,
  `seen_json`, `health_lost_json`, `look_json`, `send_turn_result` → static `write_result(Json *reply)`;
  `start_turn` → `turn_start`; `latest_message_expiry`, `take_snapshot` (→ `turn_take_snapshot`),
  `new_group_in_view`, `send_news` → static `get_news`, `batch_stop_reason`, `batch_finish(stop, error, reply)`,
  `batch_next(reply)`, `batch_after_turn(reply)`, `turn_finished`. The fields set in `run_action`
  (`control.c:1006-1014`) become `turn_begin_action`; the fields set in `step`, `rotate`, `choose_action` and
  `press_eye` (`control.c:287-299, 535-539, 453-456`) become `turn_set_step`, `turn_set_rotation`,
  `turn_add_damage`, `turn_set_look`. Bool fields get `true`/`false` (`control.c:1006-1008, 1032, 1163, 1253`).
- [ ] **Step 4: Write replies into a `Json`.** `send_turn_result` and `batch_finish` write into the `Json *reply`
  of their caller instead of `send_line`. `turn_run_frame` is the turn part of `control_poll`
  (`control.c:1370-1390`):

```c
bool turn_run_frame(Json *reply)
{
    record_witnessed();
    if (!is_finished()) {
        turn.now.frames++;
        gameloop_run_commands();
        g_stop_waiting_for_input = true; /* every frame of a turn is a tick */
        return false;
    }
    turn.now.pending = false;
    if (turn.client_gone) {
        reset_batch();
        turn.client_gone = false;
        return false;
    }
    if (turn.batch.active)
        return batch_after_turn(reply);
    write_result(reply);
    return true;
}
```

  `batch_after_turn` returns `true` when it finished the batch (the reply is written) and `false` when it started
  the next step. `turn_start_batch` copies the list (`strdup`, `abort` on failure as `control.c:1184-1186`),
  keeps `run` in the batch, and calls `batch_next`; `batch_next` calls `request_next_item` and `run`.
- [ ] **Step 5: `turn_drop_client`** sets `turn.client_gone = true` when a turn is pending and else calls
  `reset_batch()`; `turn_reset` clears the whole record (`memset` and `free` of the batch copy).
- [ ] **Step 6: Change `g_party_teleport_count` to `int16_t`** in `src/engine/move.h:24` and `src/engine/move.c:24`
  (the only writer is `move.c:133`; it is not in the save file: run `grep -rn g_party_teleport_count src tests`,
  expected hits only in `move.*`, `control/turn.c`, and a test that reads it with `c_int` becomes `c_int16`).
- [ ] **Step 7: Use the module in `platform/control.c`.** The action handlers call `turn_set_step`,
  `turn_set_rotation`, `turn_add_damage`, `turn_set_look`; `handle_act`, `handle_act_many`, `handle_wait` call
  `turn_start_witnessing`, `turn_take_snapshot`, `turn_start`, `turn_start_batch`; `control_poll` calls
  `turn_run_frame(&reply)` and sends the reply when it returns `true`; `close_client` calls `turn_drop_client`.
- [ ] **Step 8: Build and run the tests.** Run `make` and
  `uv run pytest -q tests/port/test_control.py tests/golden/test_golden_protocol.py tests/differential/test_move.py`.
  Expected: 0 warnings; PASS; the golden protocol snapshot is unchanged.
- [ ] **Step 9: Commit.**

```bash
git add src/control/turn.h src/control/turn.c src/platform/control.c src/engine/move.h src/engine/move.c tests
git commit -m "refactor: control/turn runs the turns and batches of the program player"
```

### Task 115: control/control and platform/transport: the dispatch, the actions, and the socket

**Files:**
- Create: `src/control/control.h`, `src/control/control.c`, `src/control/actions.c`, `src/control/control_internal.h`
- Move: `git mv src/platform/control.c src/platform/transport.c`, `git mv src/platform/control.h src/platform/transport.h`,
  `git mv src/platform/control_win.c src/platform/transport_none.c`
- Modify: `src/platform/transport.c`, `src/platform/transport.h`, `src/platform/transport_none.c`,
  `src/platform/main.c` (call sites: `port/src/platform/main.c:10, 444-464, 586, 590, 667, 709, 733`), `Makefile`
  (the transport file by OS; `transport.c` in the library), `tests/dmtest/control.py` (`pump`), the tests that
  call `control_open`, `control_close`, `control_poll` (`grep -rln "control_open\|control_poll\|control_close" tests`)
- Test: `tests/port/test_control.py` (Review Focus: two requests in one write; a client that disconnects mid-line)

**Interfaces:**
- Consumes: H1-H5.
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_CONTROL_H
#define DM_CONTROL_CONTROL_H
/* Prefixes: control_ */

/* The turn-based protocol of a program player: one JSON request line in, one JSON reply out
   (docs/llm/llm-mode.md). A client of ui/ and engine/ like a player at the keyboard; no socket, no SDL. */

#include <stdbool.h>
#include "control/json.h"

/* Setup */
/* The client is gone: a pending turn ends without a reply and a batch stops. */
void control_drop_client(void);

/* Queries */
/* True while a turn runs: the frame loop then runs game frames and ticks; else the game time stands still. */
bool control_is_turn_pending(void);

/* Commands */
/* Answers one request line: true when reply holds the answer, false when a turn started (control_end_turn answers). */
bool control_handle_line(const char *line, Json *reply);
/* Advances the pending turn by one frame: true when the request ended and reply holds the answer. */
bool control_end_turn(Json *reply);

#endif
```

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_CONTROL_INTERNAL_H
#define DM_CONTROL_CONTROL_INTERNAL_H
/* Prefixes: control_ */

/* Shared declarations of the control module: the action handlers of actions.c for the dispatch of control.c. */

/* Gives the input of action name with the args in request: NULL, or the reason why it cannot. */
const char *control_run_action(const char *name, const char *request);

#endif
```

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_TRANSPORT_H
#define DM_PLATFORM_TRANSPORT_H
/* Prefixes: transport_ */

/* The control socket: a Unix domain socket with one request line in and one reply line out (POSIX).
   transport_none.c is the build without a socket (Windows). The protocol is control/control.h. */

#include <stdbool.h>

/* Setup */
/* True where the control socket exists (macOS); false on Windows, which has no turn-based mode. */
bool transport_is_available(void);
/* Listens on the socket path (an old socket file is replaced). */
bool transport_open(const char *path);
/* Closes the sockets and removes the socket file. */
void transport_close(void);

/* Commands */
/* One frame: advances a pending turn and sends its answer, then accepts a client and answers its request lines. */
void transport_poll(void);

#endif
```

- [ ] **Step 1: Create `src/control/actions.c` and `control_internal.h`.** Move from `port/src/platform/control.c`
  the input helpers `click`, `press_key`, `get_viewport_origin`, `find_in_box`, `find_click`, `click_command`,
  `click_view_box` (`129-221`), `parse_champion_arg`, `need_movement_input`, `parse_movement`, `step`, `rotate`
  (`262-299`), the 27 handlers and the table (`325-675`), `press_eye` (`447-460`), and `run_action`
  (`997-1018`) as `control_run_action`: its field resets call `turn_begin_action()`. Static names carry no module
  prefix and no other module's prefix: `turn_party_left` → `rotate_left`, `turn_party_right` → `rotate_right`,
  `step` → `give_step`, `rotate` → `give_rotation`, `move_forward` ... `move_right` → `step_forward`,
  `step_back`, `step_left`, `step_right`; `send_look` → `look`; `recruit` → `answer_mirror`; the table maps the
  same protocol names (`"turn_left"`, `"move_forward"`, ...). Use the post-Package-F index globals
  (`g_inventory_champion_index != -1` for `g_inventory_champion_ordinal`, `g_inventory_champion_index ==
  champion_index` for `g_inventory_champion_ordinal == champion_index + 1`; the same for the acting and the
  candidate index); `int64_t` request numbers that index an array are checked and then cast to `int16_t`.
- [ ] **Step 2: Create `src/control/control.c` and `control.h`.** Move `handle_act`, `handle_act_many`,
  `handle_load`, `handle_new_game`, `handle_save`, `handle_wait`, `handle_screenshot`, `handle_request`,
  `send_part`, `send_error` and `load_failed` (`port/src/platform/control.c:121, 890-901, 973-983, 1020-1035,
  1177-1315`); each `send_*` writes into `Json *reply` (`send_error` → static `write_error(reply, error)`,
  `send_part` → static `write_part(reply, key, part)`). Then:

```c
bool control_handle_line(const char *line, Json *reply)
{
    handle_request(line, reply);
    return reply->s != NULL;
}

bool control_end_turn(Json *reply)
{
    return turn_run_frame(reply);
}

bool control_is_turn_pending(void)
{
    return turn_is_pending();
}

void control_drop_client(void)
{
    turn_drop_client();
}
```

  `handle_act_many` calls `turn_start_batch(list + 1, control_run_action, reply)`.
- [ ] **Step 3: Keep only the socket in `src/platform/transport.c`.** What stays: the includes of the socket
  headers, `INPUT_SIZE`, `SEND_FLAGS`, `listen_fd`, `client_fd`, `socket_path`, `input`, `input_length`,
  `close_client` (calls `control_drop_client`), `send_line` (sends a `Json` and calls `json_free`),
  `control_open` → `transport_open`, `control_is_available` → `transport_is_available`, `control_close` →
  `transport_close`, `control_poll` → `transport_poll`:

```c
void transport_poll(void)
{
    if (listen_fd < 0)
        return;
    if (control_is_turn_pending()) {
        Json reply = {0};
        if (control_end_turn(&reply))
            send_line(&reply);
        json_free(&reply);
        if (control_is_turn_pending())
            return;
    }
    if (client_fd < 0 && !accept_client())
        return;
    if (!read_input())
        return;
    answer_lines();
}
```

  `accept_client`, `read_input` and `answer_lines` are the parts of `control.c:1391-1420` (static, above
  `transport_poll`); `answer_lines` handles the lines while no turn is pending:
  `Json reply = {0}; if (control_handle_line(line, &reply)) send_line(&reply); json_free(&reply);`, and answers a
  too-long line with the same error JSON as `control.c:1417-1419` (`{"ok":false,"error":"request line too long"}`,
  written with `json_*`). The include list of `transport.c` has no `engine/` or `ui/` header any more.
- [ ] **Step 4: `transport_none.c`** keeps the four functions with the `transport_` names; the comment says
  "The build without a control socket (Windows): the turn-based mode cannot start." `transport.h` gets the
  header above.
- [ ] **Step 5: The Makefile picks the transport.** The Windows build (the `windows` target and
  `ifeq ($(OS),Windows_NT)`) compiles `src/platform/transport_none.c`, every other build `transport.c`;
  `transport.c` has no SDL, so it stays in `build/libdm.dylib` (the control tests open the socket through the
  library). Run `grep -n "control_win\|control\.c" Makefile` before: every hit changes.
- [ ] **Step 6: Change the call sites in `main.c`** (`control_open` → `transport_open`, `control_poll` →
  `transport_poll`, `control_close` → `transport_close`, `control_is_available` → `transport_is_available`;
  `control_is_turn_pending` stays and `main.c` includes `control/control.h` and `platform/transport.h`), and in
  the tests and `tests/dmtest/control.py`:

```python
def pump(lib):
    """One frame of the turn-based main loop."""
    lib.transport_poll()
    if lib.control_is_turn_pending() and lib.gameloop_run_frame():
        lib.gameloop_tick()
```

- [ ] **Step 7: Write the Review Focus tests** in `tests/port/test_control.py` (line framing is a parser: on the
  test list); they use the module fixture `game` and the socket helpers of `dmtest.control` (Assumption: `Client`
  wraps a connected socket with `send(text)` and `reply()`; adapt to the names that `tests/dmtest/control.py`
  has):

```python
def test_two_requests_in_one_write(game):
    with Client(game) as client:
        client.sock.sendall(b'{"cmd": "get_state"}\n{"cmd": "get_map"}\n')
        first, second = client.reply(), client.reply()
    assert "state" in first and "map" in second


def test_client_gone_mid_line(game):
    with Client(game) as client:
        client.sock.sendall(b'{"cmd": "get_')
    for _ in range(3):
        pump(game.lib)
    with Client(game) as client:
        client.send('{"cmd": "get_state"}')
        assert client.reply()["ok"] is True
```

- [ ] **Step 8: Build both targets and run the tests.** Run `make`, `make windows`, and
  `uv run pytest -q tests/port/test_control.py tests/port/test_modes.py tests/port/test_csb_adventure.py tests/golden/test_golden_protocol.py`.
  Expected: 0 warnings in both builds; PASS; the golden protocol snapshot is unchanged.
- [ ] **Step 9: Commit.**

```bash
git add -A src/control src/platform Makefile tests
git commit -m "refactor: control/ holds the protocol dispatch and actions, platform/transport the socket"
```

### Task 116: the protocol vocabulary in the docs and its check

**Files:**
- Modify: `docs/llm/llm-mode.md` (a "Protocol vocabulary" section; the "C API" and "Integration in main.c"
  sections: `docs/llm-mode.md:246-272` before Package C)
- Create: `tests/standard/test_vocabulary.py`
- Test: `tests/standard/test_vocabulary.py` (a docs check: a repo check, PRD Decisions)

**Interfaces:**
- Consumes: the name tables of `src/control/state_json.c` (`direction_names`, `statistic_names`, `skill_names`,
  `wound_names`, `cell_names`, `side_names`, `party_cell_names`, `slot_names`, `creature_names`,
  `wall_ornament_names`, `floor_ornament_names`, `look_names`; `port/src/platform/state_json.c:33-56, 583-645,
  764-766`) and the action table of `src/control/actions.c`.
- Produces: the section `## Protocol vocabulary` in `docs/llm/llm-mode.md`.

- [ ] **Step 1: Write the failing check** `tests/standard/test_vocabulary.py`:

```python
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
    return text[start:end if end > 0 else None]


def protocol_names():
    names = set()
    state = (ROOT / "src/control/state_json.c").read_text()
    for _, body in TABLE.findall(state):
        names |= set(re.findall(r'"([^"]+)"', body))
    names |= set(ACTION.findall((ROOT / "src/control/actions.c").read_text()))
    return names


def test_every_protocol_name_is_documented():
    documented = section()
    missing = sorted(name for name in protocol_names() if f"`{name}`" not in documented)
    assert not missing, f"names missing in docs/llm/llm-mode.md, Protocol vocabulary: {missing}"
```

- [ ] **Step 2: Run it to see it fail.** Run `uv run pytest -q tests/standard/test_vocabulary.py`. Expected: FAIL,
  `ValueError: substring not found` or a list of missing names.
- [ ] **Step 3: Write the section** `## Protocol vocabulary` after the section on actions: one sub-list per table
  (directions, statistics, skills, wounds, cells of the view, sides, party cells, slots, creatures, wall ornaments,
  floor ornaments, square looks, actions), each name in backticks with a short meaning, in table order. Generate
  the lists with a one-off script that prints the tables, then write the meanings by hand; the creature and
  ornament names need no meaning.
- [ ] **Step 4: Rewrite the sections "C API" and "Integration in main.c"** for the new modules: `control/control.h`
  (`control_handle_line`, `control_end_turn`, `control_is_turn_pending`, `control_drop_client`),
  `platform/transport.h` (`transport_open`, `transport_poll`, `transport_close`, `transport_is_available`),
  `control/state_json.h` (`state_write_state`, `state_write_view`, `state_write_map`, `state_get_string`), and the
  frame loop of `platform/app.c` (Task 120: `transport_poll` once per frame, game frames only while
  `control_is_turn_pending()`). Use `bool` where the code has `bool`.
- [ ] **Step 5: Run the check and the docs checks.** Run `uv run pytest -q tests/standard`. Expected: PASS.
- [ ] **Step 6: Commit.**

```bash
git add docs/llm/llm-mode.md tests/standard/test_vocabulary.py
git commit -m "docs: the protocol vocabulary of the LLM mode, checked against the C tables"
```

#### Stream H2: the platform modules

Shared files: `src/platform/main.c` (Task 118 moves the debug functions; Task 115 changes other lines), `Makefile`
(Task 118: `debug_view.c` in the SDL file list), `README.md` (Task 119: `DM_FILTER`),
`tests/standard/allowlist.txt` (union merge).

### Task 117: platform/options: the command line from a table

**Files:**
- Create: `src/platform/options.h`, `src/platform/options.c`, `tests/port/test_options.py`
- Modify: `tests/standard/allowlist.txt` (`dead-api platform/options.h open: epic 11`: `main.c` uses the module
  from Task 120 on)
- Test: `tests/port/test_options.py` (a parser: on the test list; Review Focus)

**Interfaces:**
- Consumes: `adventure_find` (Package D; today `adventure_of`, `port/src/data/adventure.h`), the game mode names
  (`port/src/platform/main.c:396`), `GAME_MODE_*` (`engine/game.h`).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_OPTIONS_H
#define DM_PLATFORM_OPTIONS_H
/* Prefixes: options_ */

/* The command line of the game: one table of options, --help, and an error for an unknown option or a bad
   value. No SDL, so the tests call the parser through the library. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* The options of one run; a field that the command line does not give keeps its "none" value. */
typedef struct {
    int64_t frames;             /* --frames: stop after this many frames; -1 runs until quit */
    int16_t graphic;            /* --show: the graphic to show; -1 none */
    bool view;                  /* --view: the dungeon view with free movement */
    int16_t adventure;          /* --adventure: ADVENTURE_*, -1 not given */
    int16_t mode;               /* --mode: GAME_MODE_*, -1 not given */
    const char *data_dir;       /* --data: the game files; NULL not given */
    const char *saves_dir;      /* --saves: the save folder; NULL not given */
    const char *control_socket; /* --control: the socket path; NULL not given */
    const char *filter;         /* --filter: none, scale2x, scale3x; NULL not given */
} Options;

/* Result of options_parse. */
typedef enum {
    OPTIONS_RUN,  /* options holds the run */
    OPTIONS_HELP, /* --help: print the help and stop */
    OPTIONS_ERROR /* an error line went to stderr: stop with exit code 1 */
} OptionsResult;

/* Reads argv into options. */
OptionsResult options_parse(int argc, char **argv, Options *options);
/* Prints one line per option with its value and meaning. */
void options_print_help(FILE *out);
/* The game mode of a --mode value, or -1. */
int16_t options_find_mode(const char *name);
/* The name of a game mode for the log. */
const char *options_get_mode_name(int16_t mode);

#endif
```

- [ ] **Step 1: Write the failing test** `tests/port/test_options.py`:

```python
# SPDX-License-Identifier: GPL-3.0-or-later
"""The command-line parser: values, errors, --help."""

import ctypes

import pytest

from dmtest.lib import load_lib
from dmtest import constants as C


class Options(ctypes.Structure):
    _fields_ = [("frames", ctypes.c_int64), ("graphic", ctypes.c_int16), ("view", ctypes.c_bool),
                ("adventure", ctypes.c_int16), ("mode", ctypes.c_int16), ("data_dir", ctypes.c_char_p),
                ("saves_dir", ctypes.c_char_p), ("control_socket", ctypes.c_char_p), ("filter", ctypes.c_char_p)]


lib = load_lib()
lib.options_parse.restype = ctypes.c_int
lib.options_parse.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(Options)]


def parse(*args):
    argv = (ctypes.c_char_p * (len(args) + 1))(b"dm", *(a.encode() for a in args))
    options = Options()
    return lib.options_parse(len(args) + 1, argv, ctypes.byref(options)), options


def test_no_options():
    result, options = parse()
    assert result == C.OPTIONS_RUN
    assert (options.frames, options.graphic, options.view, options.adventure, options.mode) == (-1, -1, False, -1, -1)
    assert options.data_dir is None and options.filter is None


def test_values():
    result, options = parse("--frames", "100", "--data", "game", "--saves", "s", "--control", "c.sock",
                            "--mode", "advanced", "--adventure", "csb", "--filter", "scale2x", "--show", "7", "--view")
    assert result == C.OPTIONS_RUN
    assert (options.frames, options.graphic, options.view) == (100, 7, True)
    assert (options.mode, options.adventure) == (C.GAME_MODE_ADVANCED, C.ADVENTURE_CSB)
    assert (options.data_dir, options.saves_dir, options.control_socket, options.filter) == (b"game", b"s", b"c.sock", b"scale2x")


@pytest.mark.parametrize("args", [
    ["--frams", "5"],          # unknown option
    ["game"],                  # a value without an option
    ["--frames"],              # a missing value
    ["--frames", "5x"],        # a bad number
    ["--show", "-2"],          # a graphic below 0
    ["--mode", "fast"],        # an unknown mode
    ["--adventure", "x"],      # an unknown adventure
])
def test_errors(args):
    assert parse(*args)[0] == C.OPTIONS_ERROR


def test_help():
    assert parse("--help")[0] == C.OPTIONS_HELP
```

- [ ] **Step 2: Run it to see it fail.** Run `uv run pytest -q tests/port/test_options.py`. Expected: FAIL,
  `AttributeError: ... options_parse`.
- [ ] **Step 3: Create `src/platform/options.h`** with the header above.
- [ ] **Step 4: Create `src/platform/options.c`** with the table:

```c
/* Kinds of option values. */
#define VALUE_NONE      0 /* a flag */
#define VALUE_TEXT      1
#define VALUE_NUMBER    2 /* an integer of 0 or more */
#define VALUE_MODE      3 /* a game mode name */
#define VALUE_ADVENTURE 4 /* an adventure name */

typedef struct {
    const char *name;
    int16_t value;   /* VALUE_* */
    size_t field;    /* offsetof the target in Options */
    const char *help;
} Option;

static const Option options_table[] = {
    {"--data",      VALUE_TEXT,      offsetof(Options, data_dir),       "DIR   the game files (DM; CSB with --adventure csb)"},
    {"--saves",     VALUE_TEXT,      offsetof(Options, saves_dir),      "DIR   the folder of the saved games"},
    {"--adventure", VALUE_ADVENTURE, offsetof(Options, adventure),      "dm|csb  skip the adventure screen"},
    {"--mode",      VALUE_MODE,      offsetof(Options, mode),           "vanilla|advanced|turnbased  skip the mode screen"},
    {"--control",   VALUE_TEXT,      offsetof(Options, control_socket), "PATH  the control socket of the turn-based mode"},
    {"--filter",    VALUE_TEXT,      offsetof(Options, filter),         "none|scale2x|scale3x  the pixel-art upscale filter"},
    {"--frames",    VALUE_NUMBER,    offsetof(Options, frames),         "N     stop after N frames"},
    {"--show",      VALUE_NUMBER,    offsetof(Options, graphic),        "N     show graphic N of Graphics.DAT (developer)"},
    {"--view",      VALUE_NONE,      offsetof(Options, view),           "      the dungeon view with free movement (developer)"},
    {"--help",      VALUE_NONE,      0,                                 "      print this help"},
};
```

  `options_parse` sets the "none" values (`frames -1`, `graphic -1`, `adventure -1`, `mode -1`), reads the
  arguments left to right, finds each in the table, and writes the target by its kind (`frames` as `int64_t`,
  `graphic` as `int16_t` with a range check `0..INT16_MAX`). Numbers use `strtoll` with an end check. Errors print
  one line to stderr and return `OPTIONS_ERROR`, with these texts (the first two are today's texts,
  `port/src/platform/main.c:665, 672`, `tests/test_select.py:320`):
  `--mode must be vanilla, advanced, or turnbased`, `--adventure must be dm or csb`, `unknown option: %s`,
  `%s needs a value`, `%s needs a number of 0 or more`. `--help` returns `OPTIONS_HELP`. Move `game_mode_names`
  and `get_game_mode` (`main.c:396-405`) here as `options_find_mode` and `options_get_mode_name`.
  `options_print_help` prints `Usage: dm [options]` and one line per table entry.
- [ ] **Step 5: Add the allowlist line** `dead-api platform/options.h open: epic 11` (the game uses the parser from
  Task 120 on; the test is not a user outside the file).
- [ ] **Step 6: Build and run the test.** Run `make` and `uv run pytest -q tests/port/test_options.py tests/standard`.
  Expected: 0 warnings; PASS. (`options.c` has no SDL, so it is in the library.)
- [ ] **Step 7: Commit.**

```bash
git add src/platform/options.h src/platform/options.c tests/port/test_options.py tests/standard/allowlist.txt
git commit -m "feat: platform/options parses the command line from a table with --help and errors"
```

### Task 118: platform/debug_view: the developer modes

**Files:**
- Create: `src/platform/debug_view.h`, `src/platform/debug_view.c`
- Modify: `src/platform/main.c` (remove `load_kaos_and_graphics`, `show`, `view_start`, `view_key`:
  `port/src/platform/main.c:39-127`; the calls at `main.c:277, 714-717`), `Makefile` (`debug_view.c` uses SDL key
  codes: it joins the files that stay out of the library)
- Test: none (developer tool; `tests/port/test_port_data.py:169` runs `--show` as a smoke run)

**Interfaces:**
- Consumes: `map_step` (Package E; Assumption: `bool map_step(int16_t direction, int16_t forward, int16_t right,
  Location *at)` or the signature that `src/engine/map.h` declares; step 1 greps it), `adventure_load_files`
  (Package D, R4.7: the engine module plus Graphics.DAT).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_DEBUG_VIEW_H
#define DM_PLATFORM_DEBUG_VIEW_H
/* Prefixes: debug_view_ */

/* The developer modes: --show draws one graphic, --view draws the dungeon view with free movement.
   Not part of the game. */

#include <stdbool.h>
#include <stdint.h>

/* Loads the game files of dir and draws graphic (>= 0) or, with graphic -1, the view at the party start. */
bool debug_view_open(const char *dir, int16_t graphic);
/* Free movement in the view: up/down move, left/right turn, A/D strafe; walls block. */
void debug_view_key(int key);

#endif
```

- [ ] **Step 1: Find the inputs.** Run `grep -n "map_step\|adventure_load_files" src/engine/map.h src/data/adventure.h`.
  Expected: both exist (Packages D and E). Use their declared signatures.
- [ ] **Step 2: Create `debug_view.h`** with the header above.
- [ ] **Step 3: Create `debug_view.c`.** Move `show` and `view_start` (`port/src/platform/main.c:57-92`) into
  `debug_view_open`; the "load the engine module and Graphics.DAT" part calls `adventure_load_files(dir)` instead
  of `load_kaos_and_graphics` (R4.7 removes the copy). Move `view_key` (`main.c:95-127`) as `debug_view_key`:
  the coordinates come from `map_step`; the wall check (`main.c:118-122`) stays, because free movement is not the
  party move rule. `int` results become `bool`.
- [ ] **Step 4: Call the module from `main.c`:** `MODE_SHOW` and `MODE_VIEW` call `debug_view_open(data_dir,
  options.graphic)` (`-1` for `--view`); `read_input` calls `debug_view_key(event.key)` in `MODE_VIEW`. Remove the
  include of `data/ftl.h` (`port/src/platform/main.c:12`: no `ftl_` name).
- [ ] **Step 5: Keep `debug_view.c` out of the library** in the `Makefile` (the list of SDL files beside `main.c`
  and `platform.c`). Run `grep -n "main.c" Makefile` to find the list.
- [ ] **Step 6: Build and run the smoke tests.** Run `make` and
  `uv run pytest -q tests/port/test_port_data.py tests/port/test_port.py`. Expected: 0 warnings; PASS.
- [ ] **Step 7: Commit.**

```bash
git add src/platform/debug_view.h src/platform/debug_view.c src/platform/main.c Makefile
git commit -m "refactor: platform/debug_view holds the --show and --view developer modes"
```

### Task 119: platform.c and scale.h: one filter option, bool, one declaration per line

**Files:**
- Modify: `src/platform/platform.c`, `src/platform/platform.h`, `src/platform/scale.h`, `src/platform/scale.c`,
  `README.md` (`README.md:252`; Package I moves the text to `docs/manual.md`)
- Test: none (SDL adapter; manual check)

**Interfaces:**
- Consumes: nothing new.
- Produces: `bool plat_init(int scale)` without the environment read; the filter comes only from `--filter`.

- [ ] **Step 1: Remove `DM_FILTER`.** `plat_init` (`port/src/platform/platform.c:37-50`) calls
  `plat_set_filter(1)` (no filter) instead of reading `SDL_getenv("DM_FILTER")`; the comment of `plat_init` in
  `platform.h:17` says "Opens the window at scale times the game screen." Run
  `grep -rn "DM_FILTER" src docs README.md tests tools .claude` and remove every hit outside `CHANGELOG.md` and
  `docs/superpowers/` (README: the `--filter` sentence stays, the `DM_FILTER` sentence goes).
- [ ] **Step 2: `bool` for the audio open.** `static int audio_open` with `static int tried`
  (`platform.c:225-238`) become `static bool audio_open(void)` with `static bool tried`; `return 0` → `false`.
- [ ] **Step 3: One declaration per line.** `platform.c:24` (six `int` globals) becomes six lines; with more than
  four file-scope variables the file keeps them in one `static struct { ... } plat;` (rule 7.2): `window`,
  `renderer`, `texture`, `next_tick`, `stream`, `filter`, `scaled_screen`, `pointer` (`px`, `argb`, `w`, `h`,
  `hot_x`, `hot_y`), `mouse_x`, `mouse_y`. `enum { POINTER_MAX = 32 }` (`platform.c:21`) becomes
  `#define POINTER_MAX 32` in the constant block.
- [ ] **Step 4: `scale.h`:** both functions take `(const uint8_t *src, int w, int h, uint8_t *dst)`
  (`port/src/platform/scale.h:7-8`, `scale.c:31`); the body of `scale3x` uses `w` and `h`.
- [ ] **Step 5: Build and run the tests.** Run `make`, `make windows`, and `uv run pytest -q tests/port/test_scale.py`.
  Expected: 0 warnings; PASS.
- [ ] **Step 6: Manual check.** Run `build/dm --filter scale2x --frames 200` with the game files: the window shows
  the filtered picture. Report the result.
- [ ] **Step 7: Commit.**

```bash
git add src/platform/platform.c src/platform/platform.h src/platform/scale.h src/platform/scale.c README.md
git commit -m "refactor: one filter option, bool audio open, platform state in one struct"
```

#### Tail H

Shared files: none (the tail runs after the merge of H1 and H2).

### Task 120: platform/app: the application, and main.c as the entry point

**Files:**
- Create: `src/platform/app.h`, `src/platform/app.c`
- Modify: `src/platform/main.c` (about 30 lines), `src/platform/options.c` (the turn-based check),
  `tests/standard/allowlist.txt` (remove the `open: epic 11` lines of Task 117 and of `platform/main.c`),
  `Makefile` (`app.c` uses SDL: out of the library), `docs/llm/llm-mode.md` ("Integration" section names
  `app_run_frame`)
- Test: none new; `tests/port/test_select.py` (subprocess runs of `build/dm` with options), `tests/tools/test_csb_tools.py`
  (turn-based end-to-end), `tests/port/test_port.py` (`--frames`) are the gate

**Interfaces:**
- Consumes: `options_parse`, `options_print_help`, `options_get_mode_name` (H8), `debug_view_open`,
  `debug_view_key` (H9), `transport_*` (H6), `control_is_turn_pending` (H6).
- Produces:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_APP_H
#define DM_PLATFORM_APP_H
/* Prefixes: app_ */

/* The application: the start screens, the choice of the adventure and the mode, the frame loop, and the turn-based
   socket. It owns the run state; the game rules stay in engine/ and the screens in ui/. */

#include <stdbool.h>
#include "platform/options.h"

/* Result of app_run_frame. */
typedef enum {
    APP_FRAME_NEXT,  /* run the next frame */
    APP_FRAME_QUIT,  /* the player quit */
    APP_FRAME_FAILED /* the game files or the socket cannot be opened: exit code 1 */
} AppFrame;

/* Setup */
/* Installs the disk images, checks the adventure, opens the window and the first screen; false on an error. */
bool app_open(const Options *options);
/* Closes the socket and the window. */
void app_close(void);

/* Commands */
/* Starts the DM entrance after the start screens. */
bool app_start_dm(void);
/* Starts the choice of the CSB screen: new adventure or resume. */
bool app_start_csb(void);
/* QUIT on the DM entrance or the CSB screen: back to the first start screen, or the end of the program. */
void app_quit_to_start(void);
/* One frame: the input, the socket, the start screens or the game, and the picture. */
AppFrame app_run_frame(void);

#endif
```

  The run state in `app.c` (rule 7.2: one static struct; `port/src/platform/main.c:226-233` today):

```c
/* What the frame runs. */
#define SCREEN_DEBUG    0 /* --show or --view */
#define SCREEN_SELECT   1 /* the start screens */
#define SCREEN_ENTRANCE 2 /* the DM entrance */
#define SCREEN_GAME     3

typedef struct {
    int16_t screen;                      /* SCREEN_* (was mode: MODE_SHOW ... MODE_GAME) */
    bool view;                           /* the debug screen is --view (free movement keys) */
    bool turnbased;                      /* the game mode is turn-based: the socket gives the turns */
    bool auto_enter;                     /* DM_AUTO_ENTER or turn-based: NEW ADVENTURE without the title */
    bool title_pending;                  /* the DM title plays before the entrance */
    int16_t loaded_adventure;            /* the adventure whose game files are loaded; -1 none */
    Options options;                     /* the command line */
    const char *data_dir;                /* the disk A folder of the running adventure */
    const char *csb_dir;                 /* the CSB folder: --data with --adventure csb, else game-csb/ */
    char default_control[PATH_SIZE];     /* ~/.dm/control.sock when --control is not given */
} App;

static App app;
```

  `main.c`:

```c
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/app.h"
#include "platform/options.h"
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    Options options;
    OptionsResult result = options_parse(argc, argv, &options);
    if (result == OPTIONS_HELP) {
        options_print_help(stdout);
        return 0;
    }
    if (result == OPTIONS_ERROR || !app_open(&options))
        return 1;
    for (int64_t frame = 0; options.frames < 0 || frame < options.frames; frame++) {
        AppFrame step = app_run_frame();
        if (step == APP_FRAME_FAILED)
            return 1;
        if (step == APP_FRAME_QUIT)
            break;
    }
    app_close();
    return 0;
}
```

- [ ] **Step 1: The turn-based check once.** At the end of `options_parse`, after the loop:
  `if (options->mode == GAME_MODE_TURNBASED && !transport_is_available())` prints
  `turn-based mode needs macOS` (the text of `port/src/platform/main.c:668`) and returns `OPTIONS_ERROR`.
- [ ] **Step 2: Create `app.h`** with the header above and `app.c` with the `App` struct.
- [ ] **Step 3: Move into `app.c`** (static unless `app.h` names them): the input helpers `advanced_key`,
  `amiga_key`, `click_kind`, `set_pointer`, `select_input`, `read_input`, `run_frame_hook`
  (`port/src/platform/main.c:129-301`); the start helpers `show_entrance`, `play_title`, `play_csb_intro`,
  `quit_to_start` (→ `app_quit_to_start`), `entrance_frame`, `make_folder`, `visit_name`, `list_folder`,
  `load_font`, `set_icon`, `apply_mode`, `get_adventure_dir`, `load_game_files`, `install_disk`, `start_csb`
  (→ `app_start_csb`), `start_adventure` (its DM part → `app_start_dm`; the dispatch by `g_adventure` stays a
  static `start_adventure`), `run_frame` (→ `app_run_frame`) (`main.c:303-631`); and the body of `main` after
  the parse (`main.c:681-728`) as `app_open`. The file statics (`main.c:226-233`) become fields of `app`;
  `mode` becomes `app.screen` with the `SCREEN_*` values; `MODE_SHOW` and `MODE_VIEW` become `SCREEN_DEBUG` with
  `app.view`; `enum { RUN_NEXT_FRAME, RUN_QUIT, RUN_FAILED }` becomes `AppFrame`.
- [ ] **Step 4: Results and truth values.** `load_font`, `apply_mode`, `load_game_files`, `entrance_frame` return
  `bool` (`main.c:344, 416, 444, 474`); `start_csb` returns `false` where it has `return 0;` (`main.c:536`);
  `int tick` in `run_frame` becomes `bool tick`. `SDL_Log` stays (platform). The game-mode name of the log comes
  from `options_get_mode_name`.
- [ ] **Step 5: The socket in the frame.** `app_run_frame` calls `transport_poll()` where `run_frame` calls
  `control_poll()` (`main.c:586`); `apply_mode` calls `transport_open`; `app_close` calls `transport_close()`
  and `plat_quit()`. `select_start` receives `transport_is_available()`.
- [ ] **Step 6: Write `main.c`** as above; keep `app.c` out of the library in the `Makefile` (it uses SDL).
- [ ] **Step 7: Remove the `open: epic 11` lines** of `tests/standard/allowlist.txt` (run
  `grep -n "open: epic 11" tests/standard/allowlist.txt`; every line goes) and fix what the standard check then
  reports in `src/platform/` (function length: `app_open` above 60 lines splits into `install_disks`,
  `choose_first_screen` and `open_window`).
- [ ] **Step 8: Build and run the tests.** Run `make`, `make windows`, and
  `uv run pytest -q tests/port/test_select.py tests/port/test_port.py tests/port/test_port_data.py tests/tools/test_csb_tools.py tests/port/test_options.py tests/standard`.
  Expected: 0 warnings; PASS.
- [ ] **Step 9: Manual check.** Run `build/dm --help` (the table), `build/dm --frams 5` (exit code 1, one error
  line), and `build/dm` with the game files: the start screens, the DM entrance, a few steps, quit. Report the
  result.
- [ ] **Step 10: Commit.**

```bash
git add -A src/platform tests/standard/allowlist.txt Makefile docs/llm/llm-mode.md
git commit -m "refactor: platform/app runs the application; main.c is the entry point"
```

## Package I: docs, the last open lines, acceptance (Epic 13)

Package I runs after Package H. Package C moved the docs into `docs/formats/`, `docs/rules/`, `docs/csb/`,
`docs/llm/`, `docs/reference/` and wrote `LICENSE`, `.editorconfig` and the SPDX lines (R13.3, R14). Package I
writes the new pages, fixes `CLAUDE.md`, `docs/todo.md` and `CHANGELOG.md`, removes the last `open:` lines, and
runs the PRD acceptance.

Facts: `README.md` (300 lines) has these parts (`grep -n "^#" README.md`): Status (7), Prerequisites (18),
Quickstart (25), Tools (66), C port (77) with Start screens (111), Entrance (132), Chaos Strikes Back (136), Saved
games (151), Controls (160), Modes (191); Known limitations (256), How decompile.sh finds functions (261), Tests
(272), Documentation (288). Stale: the goal line and the status table (`README.md:3, 9-16`), the known limitations
(`256-259`) (`docs/superpowers/2026-10-16-final-refactor-inventory/review-platform-repo.md` D1). `docs/todo.md`
holds five items; three of them close in this refactor (uninitialized free active groups: R5.8; NULL read of
creature attributes: R5.8; truth values: R6.2, R9.2; the cleanups of `action_reset`, the square table, the
fountain result, `group_cell`, `check()` of the golden rules: see each item).

#### Stream I1: the architecture page

Shared files: none (`README.md` links to the page; Task 122 writes the link).

### Task 121: docs/architecture.md

**Files:**
- Create: `docs/architecture.md`
- Test: the docs link check of Package C (`tests/standard/`; Assumption: Package C added a link check, e.g.
  `test_docs.py`; step 1 checks it)

**Interfaces:**
- Consumes: the purpose paragraph and the prefix line of every header in `src/` (R3: rule 2.1).
- Produces: `docs/architecture.md` with the outline below; `README.md` (Task 122) and `docs/development.md` link to it.

Outline (each heading one short section; facts from the code, with `src/` paths):

1. `# Architecture`: one paragraph: a reimplementation of DM 3.60 (Amiga) and CSB in C11; five layers; one shared
   library for the tests.
2. `## Layers`: the diagram, then one sentence per layer (what it owns, what it must not do; from
   `docs/coding-standard.md` rule 1):

```
platform/   SDL window, sound device, socket, options, the application, main
   │
control/    the turn-based protocol for a program player (JSON over the socket)
   │
ui/         screens, the dungeon view, input mapping, audio output
   │
engine/     game rules: dungeon, party, champions, creatures, timeline, saves
   │
data/       file formats, the engine image (KAOS.FTL, APPB.FTL) and its tables, run configuration
```

3. `## Modules`: one table per layer, one line per module: the header, the prefix, the purpose sentence.
   Generate the table with a script that reads the prefix line and the first sentence of the purpose paragraph
   of each `src/<layer>/*.h` (not committed), then check each line by hand.
4. `## The frame loop`: `main` → `app_run_frame` (50 Hz): input (`plat_poll`) → `transport_poll` → the start
   screens or `gameloop_run_frame` → `plat_present` → `gameloop_tick` → `plat_wait_tick` (which pumps the audio:
   `audio_mix`, `music_mix`). The frame hook `g_frame_hook` (`ui/frame.h`): a dialog, the endgame and the fuse
   sequence run their own frames inside a rule; the hook shows a frame and reads input, it runs no game logic.
5. `## The tick`: what `gameloop_tick` advances: the game time, the timeline (`timeline_`, `event_`), creatures,
   projectiles, sensors, champions; the redraw flags (`g_redraw`, `CHAMPION_ATTR_*`) that the ui reads.
6. `## The control socket and the turn`: `transport_poll` reads a request line; `control_handle_line` answers it or
   starts a turn; while `control_is_turn_pending()`, the frame loop runs game frames; `control_end_turn` ends the
   turn and the reply goes out; the game time stands still between turns. Link `docs/llm/llm-mode.md`.
7. `## Global state`: what is a global and why: the game state that the save file holds (`g_party`, `g_champions`,
   the dungeon, the timeline), `g_session` (the per-run flags), `g_screen` (the frame buffer and the palette),
   the redraw flags; module state is `static`; a result never travels through a global (rule 6.6).
8. `## Hooks between layers`: one table: hook, header, set by, called by, a NULL hook means:
   `g_endgame_sensor_hook` (engine → ui), `g_sound_play_hook` (engine → ui/audio), `g_frame_hook` (ui → platform),
   `g_os_log_hook`, `g_os_fatal_hook`, `g_os_make_folder_hook`, `g_os_list_folder_hook` (data → platform and the
   tests).
9. `## Test seams`: `build/libdm.dylib` (every layer except the SDL files: `platform/main.c`, `platform/app.c`,
   `platform/platform.c`, `platform/debug_view.c`), loaded per test module by `dmtest.lib.load_lib()`;
   `state_get_string` for the JSON of the state; `g_frame_hook` set by the tests; the os hooks; the golden
   snapshots (`tests/golden/`) and the differential tests against the emulator (`tests/differential/`).
10. `## How to add a game rule`: the steps: find the original function in `docs/reference/kaos-symbols.csv`; write
    the rule in the engine module that owns the data; name every value; a ui change only through redraw flags or a
    hook; a differential test against the emulator, or a golden scenario when the emulator cannot reach it;
    `make check`.

- [ ] **Step 1: Check the inputs.** Run `ls tests/standard/` and `grep -ln "link" tests/standard/*.py`. Expected: a
  docs link check exists. If none exists, report it; the tail (Task 125) runs the link scan of step 5 by hand.
- [ ] **Step 2: Generate the module list** with a throw-away script (in the scratchpad, not committed):

```python
import re
from pathlib import Path

for layer in ("data", "engine", "ui", "control", "platform"):
    print(f"\n### {layer}/\n\n| Header | Prefixes | Purpose |\n|---|---|---|")
    for header in sorted(Path("src", layer).glob("*.h")):
        text = header.read_text()
        prefixes = re.search(r"/\* Prefixes: (.*?) \*/", text)
        purpose = re.search(r"\*/\n\n/\* (.*?)(?:\. |\*/)", text, re.S)
        if header.name.endswith("_internal.h"):
            continue
        print(f"| `{layer}/{header.name}` | {prefixes.group(1) if prefixes else ''} | "
              f"{' '.join(purpose.group(1).split()) if purpose else ''} |")
```

- [ ] **Step 3: Write `docs/architecture.md`** with the outline above (the docs skill: reference and concept parts,
  short sentences, no history). Check each fact of sections 4-9 against the code with `grep` (for example
  `grep -n "transport_poll\|gameloop_run_frame\|plat_present\|gameloop_tick" src/platform/app.c`).
- [ ] **Step 4: Run the docs checks.** Run `uv run pytest -q tests/standard`. Expected: PASS.
- [ ] **Step 5: Commit.**

```bash
git add docs/architecture.md
git commit -m "docs: architecture page: layers, modules, frame loop, turn, state, hooks, test seams"
```

#### Stream I2: README and the three guides

Shared files: none.

### Task 122: README.md short; docs/manual.md, docs/development.md, docs/decompiling.md

**Files:**
- Create: `docs/manual.md`, `docs/development.md`, `docs/decompiling.md`
- Modify: `README.md`
- Test: the docs link check of Package C

**Interfaces:**
- Consumes: `README.md` as Package H leaves it; the Makefile targets of Package B (`all`, `lib`, `windows`, `test`,
  `check`, `format`, `dist`, `dist-win`, `play`, `clean`; `SANITIZE=1`).
- Produces: the four pages; the doc index in `README.md` links `docs/architecture.md` (Task 121).

- [ ] **Step 1: `docs/manual.md`** (the player manual): move `README.md` "C port" from Start screens to Modes
  (`README.md:111-255`) and the run part of the Quickstart; the options table as `build/dm --help` prints it (Task
  120); saves; CSB; the Windows notes. Remove what is stale (no `DM_FILTER`).
- [ ] **Step 2: `docs/development.md`**: the build (`make`, `make windows`, `SANITIZE=1`, `make play` with
  `PLAY_DIR`), the tests (`make test` = `uv run pytest -q -n auto`; one file
  `uv run pytest -q tests/<folder>/test_x.py`; the folders `differential`, `port`, `golden`, `tools`, `standard`
  and what each proves; `dmtest` helpers; the game files that the tests need and the skip markers), the layout
  (the tree of PRD R1), the coding standard (`docs/coding-standard.md`, `make format`, `make check`), the release
  steps (`make dist`, `make dist-win`, `CHANGELOG.md`). Move `README.md` "Tests" (`272-287`) here.
- [ ] **Step 3: `docs/decompiling.md`**: the Ghidra tooling: `README.md` Prerequisites, the decompile part of the
  Quickstart, Tools, "How decompile.sh finds functions" (`README.md:18-76, 261-271`), with the paths of
  `tools/ghidra/` and `tools/dmtools/` (`dm-extract`, `dm-ftl`, `dm-csb-match`).
- [ ] **Step 4: `README.md`**: what it is (one paragraph), the status in two lines (the full game of DM and CSB
  plays; macOS and Windows; the LLM mode on macOS), the quickstart (`brew install sdl3`, `uv sync`, `make`,
  `build/dm --data game`, `make test`), the license ("The code is GPL-3.0-or-later (`LICENSE`). The game data of
  Dungeon Master and Chaos Strikes Back is not included."), and the doc index (manual, development, decompiling,
  architecture, coding standard, formats, rules, CSB, LLM, reference). No person's name, no personal folder.
- [ ] **Step 5: Fix the links into the old README sections.** Run `grep -rn "README.md#" docs CLAUDE.md .claude tools`
  and point each hit to its new page.
- [ ] **Step 6: Run the docs checks.** Run `uv run pytest -q tests/standard`. Expected: PASS.
- [ ] **Step 7: Commit.**

```bash
git add README.md docs/manual.md docs/development.md docs/decompiling.md docs CLAUDE.md .claude tools
git commit -m "docs: short README; manual, development and decompiling guides"
```

#### Stream I3: project files

Shared files: none.

### Task 123: CLAUDE.md and the agent files

**Files:**
- Modify: `CLAUDE.md`, `.claude/agents/dm-coordinator.md`, `.claude/agents/dm-player.md`
- Test: none (docs)

**Interfaces:**
- Consumes: the commands of Package B, the layers of R1.
- Produces: the project `CLAUDE.md` of R13.4.

- [ ] **Step 1: Commands** in `CLAUDE.md`: Build `make` (0 warnings; `make windows` for Windows); Run `build/dm`;
  personal play `make play`, then `playthroughs/local/dm --data game --saves playthroughs/local`; one test file
  `uv run pytest -q tests/<folder>/test_x.py`; full suite `make test` (`uv run pytest -q -n auto`); all checks
  `make check`.
- [ ] **Step 2: Layout** in `CLAUDE.md`: `src/` with the five layers in order and one line each; `third_party/`;
  `tools/dmtools/`, `tools/ghidra/`, `tools/dist/`, `tools/research/`; `tests/` folders and `tests/dmtest/`;
  `docs/` (architecture, development, coding standard). Keep the git-ignored data line and the Conventions
  section; the conventions name `docs/architecture.md` beside `docs/coding-standard.md`.
- [ ] **Step 3: Agent files.** Run `grep -n "docs/\|port/\|playthroughs/lego\|README" .claude/agents/*.md`; every doc
  path follows the new tree (`docs/llm/llm-mode.md`, `docs/llm/llm-playbook.md`, ...).
- [ ] **Step 4: Check the paths.** Run `grep -rn "port/src\|playthroughs/lego\|docs/llm-mode.md\|docs/llm-playbook.md" CLAUDE.md .claude`.
  Expected: nothing.
- [ ] **Step 5: Commit.**

```bash
git add CLAUDE.md .claude/agents
git commit -m "docs: project CLAUDE.md and agent files follow the new layout"
```

### Task 124: docs/todo.md and CHANGELOG.md

**Files:**
- Modify: `docs/todo.md`, `CHANGELOG.md`
- Test: none (docs)

**Interfaces:**
- Consumes: the commits of the branch (`git log --oneline main..HEAD`).
- Produces: the todo list without the closed items; one CHANGELOG entry.

- [ ] **Step 1: Close the todo items** that the refactor closed; check each with grep before it goes:
  "Uninitialized bytes of free active groups" (`grep -n "calloc\|memset" src/engine/game.c src/engine/group.c` shows
  the clear; `grep -rn scrub_free_groups tests` finds nothing), "NULL read of creature attributes" (the range check
  in `map_get_creature_attributes`), "Truth values that are still int16_t" (`savegame_quick_save` returns `bool`;
  `music_play_end(bool)`), and each line of "Small cleanups" that the code shows as done. An item that is still
  open stays. "Test audit" stays.
- [ ] **Step 2: Add the open `ponytail:` items.** Package G moved the `ponytail:` markers to `docs/todo.md` or
  turned them into deviation comments (R8.8); run `grep -rn "ponytail\|TODO\|FIXME" src` (expected: nothing) and
  check that each moved item is in `docs/todo.md` under a heading.
- [ ] **Step 3: CHANGELOG.** Under `## Unreleased`, `### Changed`, one entry: "Final refactor, with no change of
  game behavior: the source is in `src/` with five layers (`data`, `engine`, `ui`, `control`, `platform`); a root
  `Makefile` with incremental builds, `-Werror` and `make check`; coding standard v2 with a check per rule; the
  record accessors in `engine/record.c`; the LLM protocol in `control/`; tests in folders with shared `dmtest`
  helpers; `docs/architecture.md`, `docs/manual.md`, `docs/development.md`; GPL-3.0 license." Under `### Removed`:
  "`DM_FILTER` (use `--filter`)." Under `### Added`: "`build/dm --help`; unknown options stop with an error."
- [ ] **Step 4: Commit.**

```bash
git add docs/todo.md CHANGELOG.md
git commit -m "docs: todo items that the final refactor closed, and its CHANGELOG entry"
```

#### Tail I

Shared files: none.

### Task 125: the allowlist without open lines

**Files:**
- Modify: `tests/standard/allowlist.txt`, the source files that the checks then report
- Test: `tests/standard/`

**Interfaces:**
- Consumes: every package.
- Produces: an allowlist with only the documented exceptions (the soundtrack tables, the APPB address tables, the
  per-type switch functions).

- [ ] **Step 1: List the open lines.** Run `grep -n "open:" tests/standard/allowlist.txt`. Expected: only stale
  lines (each epic removed its own lines). Note each line.
- [ ] **Step 2: Remove every `open:` line** and run `uv run pytest -q tests/standard`.
- [ ] **Step 3: Fix each failure** in the file it names, in the style of the epic that owned it; a failure that
  needs a design decision stops the run for the user. Commit each fix by its check
  (`git commit -m "refactor: <check>: <what>"`).
- [ ] **Step 4: Check the reasons.** Every remaining line names an exception of `docs/coding-standard.md`: run
  `uv run pytest -q tests/standard -k allowlist`. Expected: PASS.
- [ ] **Step 5: The ctypes structures follow the C structs** (R12.6): run
  `grep -rn "ctypes.Structure" tests | grep -v "tests/dmtest/"`. Expected: only structures of modules that the
  tests own (for example `Options` of `test_options.py`); a mirror of a game struct (`Champion`, `Party`,
  `Session`, `Screen`, `TimelineEvent`, `ActiveGroup`) moves to `tests/dmtest/game.py`.
- [ ] **Step 6: Build and run the checks.** Run `make`, `make windows`, `uv run pytest -q tests/standard`.
  Expected: 0 warnings; PASS.
- [ ] **Step 7: Commit.**

```bash
git add tests/standard/allowlist.txt
git commit -m "test: the standard allowlist holds only documented exceptions"
```

### Task 126: the PRD acceptance

**Files:**
- Modify: none, unless a scenario fails (then the fix goes into the file that the failure names, with its own
  commit)
- Test: the scenarios of the PRD Acceptance

**Interfaces:**
- Consumes: every package.
- Produces: the acceptance record in the SDD ledger (one line per scenario: pass, or the fix commit).

- [ ] **Step 1: Layout.**
  `ls -d src third_party tools/dmtools tools/research tests/dmtest docs/architecture.md LICENSE && test ! -e port && ! grep -rn "port/src" docs README.md CLAUDE.md .claude --exclude-dir=superpowers`.
  Expected: the paths exist; no `port/`; grep finds nothing.
- [ ] **Step 2: Build.** `make clean && make 2>&1 | tee /tmp/dm-build-1.log; make; touch src/engine/random.c; make 2>&1 | tee /tmp/dm-build-3.log`.
  Expected: no `warning:` in the first log; the third log compiles `random.c` only (one compile line) and links.
  Then `make windows 2>&1 | grep -c "warning:"`: expected `0`. Then `touch src/engine/map.h; make -n | grep -c " -c "`:
  expected one compile line per `.c` file that includes `engine/map.h` (compare with
  `grep -rl '"engine/map.h"' src | wc -l` plus the headers that include it).
- [ ] **Step 3: Standard.** `uv run pytest -q tests/standard && ! grep -n "open:" tests/standard/allowlist.txt`.
  Expected: PASS; grep finds nothing.
- [ ] **Step 4: Same behavior.** `make test` and `git diff --stat <plan base 72e24b5> -- tests/golden/*.json`.
  Expected: all tests pass; `save.json`, `view.json`, `rules.json` show no change of their values (new scenarios
  of Package A only add keys; check with `git diff 72e24b5 -- tests/golden/rules.json | grep "^-" | grep -v "^---"`:
  nothing); `screens.json` and `protocol.json` are unchanged since Package A.
- [ ] **Step 5: Constants.** `grep -rn "enum {" src` finds nothing; `grep -rn "\benum\b" src` finds only result types
  (`typedef enum { ... } <Name>Result`-style types used as a return type); the `notation` and `block` checks pass
  (step 3).
- [ ] **Step 6: Globals.** `grep -rhn "^extern" src | grep -v " g_"` finds nothing;
  `grep -rn "g_move_result_\|g_current_group_\|g_creature_damage_outcome\|g_sensor_rotation_effect\|_ordinal" src`
  finds nothing.
- [ ] **Step 7: Records.** `grep -rn "REC(" src | grep -v "src/engine/record\.[ch]"` and
  `grep -rn "^[a-z].* rec_[a-z_]*(" src --include=*.c | grep -v "src/engine/record.c"` find nothing.
- [ ] **Step 8: Layers.** `grep -rn "SDL\|<sys/\|<unistd.h>\|fprintf(stderr\|exit(" src/ui src/engine src/data | grep -v "src/data/os.c"`
  finds nothing; `grep -rn '#include "platform/' src/control` finds nothing.
- [ ] **Step 9: Tests and docs.** `grep -rn "from test_\|/private/tmp\|/Users/" tests` finds nothing;
  `grep -rn "\"clang\"\|'clang'" tests` finds nothing; the dead-API check passes (step 3);
  `ls docs/architecture.md docs/manual.md docs/development.md && test ! -e docs/superpowers/status.md`; the docs
  link check passes (step 3).
- [ ] **Step 10: Record.** Write one line per scenario into the ledger. A failed scenario gets a fix commit and the
  scenario runs again; a failure that needs a design decision stops the run for the user.

---


