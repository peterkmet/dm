# Review: platform layer, build, tests, tools, repository and docs

Repository `/Users/lego/github/dm` at commit `0771406` (2026-10-16). Read-only review; nothing was built or run except `grep`, `wc`, `git log` and one heuristic script for unused includes. Line numbers refer to the files at this commit. Sizes: S under an hour, M a few hours, L a day or more.

Numbers used below: `port/src` is 34,556 lines (`data/` 3,575, `engine/` 12,675, `ui/` 14,553 of which 5,380 are `dr_mp3.h`, `platform/` 3,708). `tests/` has 45 test files, 427 tests. 255 tracked files, 383 commits since 2026-10-03, no tags.

---

## A. `port/src/platform/` and `port/Makefile`

### A1. `platform/` is three layers in one folder

- `platform/` holds (a) the SDL adapter: `platform.c` 253 lines, `scale.c` 67; (b) the application and composition root: `main.c` 736; (c) the LLM turn-based protocol: `control.c` 1,431, `state_json.c` 1,077, `control_win.c` 24. The protocol is 2,532 lines, 68 % of the layer.
- The protocol files have no SDL (`control.h:7` "Plain POSIX, no SDL", `state_json.h:8` "No SDL"); the Makefile links them into `libdm.dylib` (`port/Makefile:7` excludes only `main.c`, `platform.c`, `control_win.c`). They drive the game through `ui/` and `engine/` APIs only (`control.c:131-142` `command_process_click`, `gameloop_run_commands`; `control.c:155` `ui_command_from_mouse`). That is a second user interface (a client of the game), not platform code.
- Why it matters: the layer rule (`docs/coding-standard.md:37`) says `platform/` is "only platform/ includes SDL"; a reader who opens `platform/` to find the SDL code finds a JSON protocol. `test_layers` (`tests/test_standard.py:292-311`) cannot tell the two apart.
- Proposal: a fifth layer `control/` between `ui/` and `platform/` (`data → engine → ui → control → platform/app`). Move `control.c`, `state_json.c`, `control_win.c` there; the three places that list the layers change together (`port/Makefile:5`, `tests/conftest.py:11`, `tests/test_standard.py:292`, `docs/coding-standard.md:37`). Size M.

### A2. `control.c` has six jobs and five file-static state records

- Jobs by line range: socket I/O (53-57, 869-888, 1331-1431), request parsing (221-256, 1106-1142), 27 action handlers and their table (258-675), a PNG writer (677-747), what-the-party-saw bookkeeping and news (749-867, 1037-1095), turn and batch state machine (985-1035, 1097-1195, 1317-1329), request dispatch (1197-1315).
- State: `turn` (59-71), `batch` (75-84), `seen` (87-92), `witnessed` (106-112), `look_text` (115-119), `load_failed` (121) are file statics that most functions read and write.
- Why it matters: 1,431 lines is the largest file in the port after `view.c` and `group.c`, and unlike those it is not a port of one original module; its parts have separate reasons to change (protocol vocabulary, socket details, turn rules).
- Proposal: `control/control.c` (dispatch, 1197-1315), `control/control_socket.c` (listen, accept, line framing, send), `control/control_actions.c` (258-675; the "player input" table), `control/control_turn.c` (turn, batch, witnessing, news), `control/png.c` (677-747, with `crc32.h`). Keep the file-static state in `control_turn.c` behind three functions (`turn_start`, `turn_poll`, `turn_result_json`). Size L.

### A3. The hand-written JSON parser is three parsers

- `find_value` (`control.c:223-234`) is a flat `strstr` for `"key"`: a key that also appears inside a string value, or the same key at two nesting levels, resolves to the first occurrence. `get_string` (236-246) does not unescape. `next_element` (1108-1142) is a second scanner for `act_many` lists with its own brace matching. The limit is documented (`docs/llm-mode.md:172` "A string value cannot contain a `"` character").
- Why it matters: acceptable for a local, trusted client; the cost is that the contract lives in a doc line and in `control.c:1124` ("strings hold no '\"'"), and three scanners must agree.
- Proposal: keep the flat search (a tokenizer is not worth it for this protocol) but put the three functions in `control_request.c` with a five-line contract comment at the top (keys are unique per request, no `"` in strings, no nesting beyond `args`), and let `next_element` reuse `find_value` for the object case. Size S. Alternative, if the protocol grows: one ~150-line tokenizer that yields key/value pairs per object. Size M.

### A4. The JSON writer lives inside the state module

- `state_json.c:58-178` (`put`, `separate`, `json_raw` ... `json_key`, `key_int`, `key_str`, `key_bool`, `tenths`, `pair`) is a generic writer, exported through `state_json.h:12-26` and used by `control.c`.
- Proposal: `control/json.h` + `json.c` for the writer; `state_json.h` keeps only the state functions. Move `key_int/key_str/key_bool` into `json.h` too, since `control.c` re-implements the pattern (`control.c:801-808`). Size S.

### A5. Names in `state_json.h` do not say what they do

- `state_json_write_state` and `state_json_write_view` append JSON; `state_json_map` (`state_json.h:31`) also appends JSON but has no verb; `state_json_write_visible_groups` and `state_json_write_visible_things` (`state_json.h:35, 44`) write nothing: they fill C arrays for `control.c`.
- `DECIMAL_BASE` (`state_json.h:10`) is defined in the state header because `control.c:254` needs a number base for `strtoll` and the standard forbids the literal `10`. A constant named "the decimal base is 10" carries no game meaning.
- Proposal: `state_json_write_map`; `state_json_get_visible_groups` / `_get_visible_things`; allow the number base argument of `strtol`-family calls in the standard and drop `DECIMAL_BASE`. Size S.

### A6. Truth values as `int` (standard rule 3, not checked by `test_standard.py`)

- `test_predicates_return_bool` (`tests/test_standard.py:149-152`) checks only names with `is_/has_/can_`. Rule 3 (`docs/coding-standard.md:24`) says all truth values are `bool`.
- `main.c`: `load_kaos_and_graphics` (39-54), `show` (57-73), `view_start` (76-92), `entrance_frame` (344-376), `load_font` (416-429), `apply_mode` (444-464), `load_game_files` (474-488) return `int` 0/1 with "Returns 0" comments; `start_csb` is `bool` but has `return 0;` at 536.
- `state_json.c`: `json_bool(Json *, int)` (102, header 22), `key_bool` (155), `object_json(..., int held)` (212) whose `held` is `1` at every call (349, 446, 482): a dead parameter; `WallFace` (575-580) has eight `int` flags; `int wall_ahead` (945).
- `control.c`: `turn.has_damage = 0; look_text.shown = 0; turn.moving = 0;` (1006-1008) on `bool` fields; `start_turn(..., 0)` (1032, 1163) and `(..., 1)` (1253) for `bool wait_only`; `json_bool(&j, 0/1)` (895, 925, 950, 978).
- `platform.c`: `static int audio_open` with `static int tried` (225-230).
- Proposal: `bool` and `true/false` throughout; drop `held`; add a check to `test_standard.py` for `int` functions whose comment starts with "Returns 0" or "Returns 1", and for `= 0;`/`= 1;` on fields declared `bool` in the same file (regex over the struct). Size S for the fix, S for the check.

### A7. The start-up state machine is spread over globals and five functions

- File statics `mode, turnbased, auto_enter, title_pending, loaded_adventure, data_dir, data_override, saves_dir, control_socket, csb_dir` (`main.c:228-233`) plus engine globals `g_game_mode`, `g_adventure`, `g_new_game`, `g_quit_requested`, `g_return_to_entrance`. Transitions sit in `main()` (679-736), `run_frame` (579-631), `start_adventure` (551-574), `start_csb` (512-547), `entrance_frame` (346-376), `quit_to_start` (335-342). `select_screen()` is called four times in `main` (710, 712, 722) and once in `run_frame` (587).
- `parse_options` (644-677) writes `--data`, `--saves`, `--control` into file statics and the rest into `Options` (the comment at 633 admits it). Unknown options are ignored, there is no `--help`, `atoll` is unchecked, and the "turn-based needs macOS" test (667-670) runs on every loop iteration instead of once after parsing.
- Why it matters: this is the file a new reader opens first; today it needs `docs/llm-mode.md:261-272` to be understood.
- Proposal: `app/app.c` with one `App` struct (mode, options, loaded adventure, directories), `app_start_dm`, `app_start_csb`, `app_run_frame`; `app/options.c` with a table of options (`name, has_value, target`), `--help`, and an error for an unknown option; `main.c` becomes 30 lines. Game behavior unchanged. Size M-L.

### A8. Developer modes in the shipped binary

- `--show` (`show`, 57-73) and `--view` with its free-movement keys (`view_start`, `view_key`, 76-127) are development aids; `view_key` duplicates movement rules (`main.c:117-122`) that `engine/` owns.
- Proposal: move to `app/debug_view.c` behind one function, or drop them: `tests/test_golden_view.py` and `--frames` cover what they were for. Size S.

### A9. Dead includes

- `main.c:12` includes `ftl.h` (no `ftl_` symbol in the file; commit `e015ff5` "drop dead ftl.h includes" missed it). `state_json.c:11` includes `command.h` (no `command_`/`COMMAND_`/`CLICK_` symbol used). Heuristic (header declarations vs identifiers used); `platform.c` and `control.c` are clean.
- Proposal: drop them; add the heuristic as a `test_standard.py` check (the regexes exist there). Size S.

### A10. Types

- `turn.frames/min_ticks/max_ticks` are `int64_t` (`control.c:62`) while `g_game_time` is `uint32_t`; `int64_t ticks = g_game_time - turn.start_time` (1321) is a widened unsigned difference: correct, but mixed. `int teleports` (69) mirrors `extern int g_party_teleport_count` (`engine/move.h:24`, "port only"), the one `int` game global; it should be `int16_t` like its neighbours or documented why not. `PlatEvent` fields are `int` (`platform.h:11-14`): SDL values, allowed by rule 3. Size S.

### A11. Strings used as an enum

- `square_name()` returns a protocol name and `state_json.c:930, 937, 962` compare it with `strcmp(name, "wall")`; `map_char` (999-1016) does up to six `strcmp` per square of the map.
- Proposal: `enum SquareLook { LOOK_WALL, LOOK_FLOOR, LOOK_PIT, LOOK_STAIRS_UP, ... }`, `square_look()` returns the enum, one name table for the JSON. Size S.

### A12. Protocol vocabulary lives only in C tables

- `wall_ornament_names` (`state_json.c:583-645`, 60 entries by graphic index), `creature_names` (51-56), `slot_names` (43-49), `floor_ornament_names` (764-766), `cell_names`, `party_cell_names`. `docs/llm-mode.md` mentions two of the ornament names. The playbook (`docs/llm-playbook.md`) is the player's only reference.
- Proposal: one "Protocol vocabulary" section in `docs/llm/llm-mode.md` and a test that every name in the C tables appears there (regex over the C file, like `test_standard.py`). Size S-M.

### A13. Small things in `platform.c`, `scale.c`, `control_win.c`

- `scale2x(src, w, h, dst)` vs `scale3x(src, width, height, dst)` in one header (`scale.h:7-8`); `o` vs `out_top` in the bodies. S.
- `platform.c:24` six globals on one line; `enum { POINTER_MAX = 32 }` (21) between `#define`s. S.
- Two ways to choose the filter: `DM_FILTER` in `plat_init` (`platform.c:45`) and `--filter` in `main` (719); README documents both (244, 252). Keep one. S.
- `control_win.c` is the "no socket" stub chosen by the Makefile filter (`Makefile:31`); the name says Windows, the content says "none". Rename `control_none.c`; choose it by `ifeq ($(OS),Windows_NT)` or a `CONTROL=none` variable instead of a per-target filter. S.
- Error handling is adequate for a game: `abort()` on OOM (`state_json.c:68`, `control.c:1186`), ignored `mkdir`/`fcntl` results (`control.c:1341, 1392`), `write_png` checks `fclose` but not `fwrite` (691-694, 742-745). State "allocation failure aborts" once in the standard. S.

### A14. `port/Makefile`

- One compile command per target (12-14, 16-18): every change rebuilds 34 k lines twice (program and library); no object files, no `-MMD` dependency tracking.
- No `-Werror`, although the standard demands 0 warnings (`docs/coding-standard.md:45`); the tests compile with `-Werror -O1` (`tests/test_control.py:35`), so the tested library and the shipped binary are built with different flags.
- `SDL = $(shell brew --prefix sdl3)` (3) runs for every invocation, also `clean`, and binds the build to Homebrew; `pkg-config --cflags --libs sdl3` works on macOS and Linux.
- `-O2 -g` fixed; no `SANITIZE=1` (`-fsanitize=address,undefined`) variant. For a port of a decompilation whose risk is out-of-bounds reads (the skill's fidelity rule), the test library should run under ASan/UBSan at least in CI.
- `lego` target (20-24) is the owner's personal play folder; `WIN_CFLAGS = $(CFLAGS)` (30) is an alias without a purpose; the `windows` target downloads SDL with `curl` inside a rule (38-40).
- No `test`, `format` (`clang-format -i` is rule 11 of the standard) or `dist` targets; `make-dist*.sh` call make but not the other way round. `B = ../build` builds outside `port/`.
- CMake: not needed for 100 files and two targets; GNU make with pattern rules, `-MMD -MP` and `pkg-config` gives incremental builds and a CI-friendly build in ~40 lines. Choose CMake only if a native Windows (MSVC) build is a goal.
- Proposal: objects under `build/obj/<layer>/`, `-MMD -MP`, `-Werror` always, `pkg-config`, `SANITIZE=1`, targets `all test format check dist dist-win clean`, `play` instead of `lego` with the folder in a variable. Size M.

### A15. Vendored code inside a layer

- `port/src/ui/dr_mp3.h` (5,380 lines) sits in `ui/`; `test_standard.py:12` excludes it by name. Proposal: `port/third_party/dr_mp3/dr_mp3.h` with its license and version line, `-Iport/third_party`. Size S.

### A16. What a CI needs

- macOS runner: `brew install sdl3 mingw-w64`, `uv sync`, `make -C port`, `make -C port windows`, `uv run pytest -q`. Every game-data test skips (good design: `tests/test_port.py:55` already runs the binary headless with `SDL_VIDEO_DRIVER=dummy`), so CI covers the build, the standard, the format, and the synthetic-data tests (`test_adf`, `test_ftl`, `test_scale`, `test_port`, parts of `test_savefile`, `test_csb_ending`).
- Blocker: `test_standard.py:314-321` requires clang-format major 23 from Homebrew. The `clang-format` PyPI wheel pins the version in `uv.lock` and removes the brew dependency. Size S.
- With A14 (`pkg-config`, no brew in the Makefile) an Ubuntu runner also builds the library. Size S-M for the workflow.

---

## B. `tests/`

### B1. Structure: five kinds of tests in one flat folder

- Differential against the emulator: `test_champion`, `test_creatures`, `test_move`, `test_game`, `test_dungeon`, `test_spells`, `test_sound`, `test_text`, `test_tables`, `test_savefile`, `test_recruit`, `test_csb_rules`, `test_csb_engine`, `test_csb_magic_map`, `test_csb_text`, `test_csb_ending`.
- Port behavior through `libdm`: `test_gameloop`, `test_control`, `test_modes`, `test_select`, `test_entrance`, `test_ui`, `test_panels`, `test_view`, `test_automap`, `test_save`, `test_actions`, `test_port`, `test_port_data`, `test_music`, `test_scale`, `test_csb_adventure`, `test_csb_save`.
- Golden: `test_golden_rules`, `test_golden_save`, `test_golden_view` + `golden/*.json`.
- Tool tests: `test_adf`, `test_extract`, `test_ftl`, `test_emu`, `test_ghidra`, `test_symbols`, `test_csb_tools` (the only end-to-end test: it starts `build/dm --mode turnbased`, `test_csb_tools.py:24-27`).
- Standard: `test_standard.py` + `standard_allowlist.txt`.
- `csb_diff.py` (208 lines) is a manual script, not a test (`csb_diff.py:3` "Run: uv run python tests/csb_diff.py (from /Users/lego/github/dm)").
- Proposal: `tests/port/`, `tests/differential/`, `tests/golden/`, `tests/tools/`, `tests/standard/`; `csb_diff.py` becomes `tests/differential/test_csb_mini.py` or moves to `tools/dmtools/`. With subfolders, pytest needs `--import-mode=importlib` or `__init__.py` files, and `pythonpath = ["tests"]` for the helpers package (B2). Size M (moves, imports, markers).

### B2. Test modules are imported as libraries

- 24 `from test_x import ...` lines in 20 files, for example `tests/test_control.py:16` (`Box, Game` from `test_gameloop`), `tests/test_modes.py:17-18` (`Client, pump` from `test_control`, 13 names from `test_gameloop`), `tests/test_music.py:6` (11 names incl. fixtures and `pytestmark` from `test_modes`), `tests/test_panels.py:11`, `tests/test_golden_rules.py:12-16` (five modules). `# noqa: F401` marks fixtures re-exported through imports.
- Why it matters: importing a test module runs its module code: `test_port.sdl3_installed()` spawns `brew --prefix` at import (`test_port.py:13-20`) and is imported by six files; skip markers and `ROOT`/`GAME` constants are re-declared in every file; the dependency graph of the suite is invisible.
- Proposal: `tests/dmtest/` package: `lib.py` (build once, load a private copy), `game.py` (`Game`, `Box`, `Champion`, `Party`, `World`), `savefile.py` (`SaveFile`, `decode`), `control.py` (`Client`, `pump`, `pumped`), `markers.py`, `constants.py` (B3); fixtures in `conftest.py`. Size M-L (mechanical, but 20 files).

### B3. Hand-mirrored C constants and raw record decoding in tests

- `SLOT_HAND_ACTION = 1` in 7 files (`test_creatures.py:35`, `test_modes.py:30`, `test_save.py:20`, `test_gameloop.py:20`, `test_csb_magic_map.py:21`, `test_golden_rules.py:25`, `test_actions.py:26`); `KEY_RETURN = 0x4400` in 3 (`test_csb_ending.py:29`, `test_save.py:226`, `test_gameloop.py:16`); `LEFT_PRESS, LEFT_RELEASE = 2, 4` in 3; `KEY_TAB, KEY_M, KEY_F5, KEY_F9` (`test_modes.py:24`); zone numbers `570, 740, 520` (`test_gameloop.py:19`), `7, 11, 101` (`test_modes.py:27`); thing types `5, 6, 7` (`test_modes.py:26`).
- `test_gameloop.py:111-116` decodes thing records by hand (`thing >> 10 & 15 == SENSOR`, `(data[2] << 8 | data[3]) & 0x7F`, `thing >> 14`, `map_get_square(...) >> 5 == CORRIDOR`): the record layout the standard confines to `map.c` (rule 2) is re-implemented in Python.
- Proposal: `dmtest/constants.py` parses `#define` and `enum` members from `port/src/**/*.h` at import (regexes in `test_standard.py:174-175`) so tests write `C.KEY_RETURN`, `C.ZONE_RESURRECT`; tests call the exported `rec_*`/`map_*` accessors instead of shifting bytes. Size M.

### B4. Private library builds: 12 clang compiles per run, 22 modules share one dylib

- Private compile of all ~29 k lines (`clang -O1 -Werror -dynamiclib`): `test_control.py:35`, `test_csb_adventure.py:42`, `test_csb_ending.py:47`, `test_csb_rules.py:44`, `test_csb_engine.py:56`, `test_csb_magic_map.py:75`, `test_csb_text.py:61`, `test_csb_save.py:38`, `test_entrance.py:28`, `test_modes.py:45`, `test_select.py:37`, `test_tables.py:67` (`-O2`).
- The other modules load `build/libdm.dylib` and call `make -C port` again in their fixture (22 call sites, e.g. `test_gameloop.py:38`, `test_save.py:41`) although `conftest.py:83-86` built it once before the workers start; `test_automap.py:32-36` has a function-scoped fixture, so `make` runs per test.
- `ctypes.CDLL(path)` returns the same image for the same path: two modules that run in one xdist worker (`--dist loadfile` gives a worker several files) share the C globals. The skill rule "each test module loads its own copy of the library" holds for 12 of 34 modules.
- Suite time: not measured; no `pytest_sessionfinish` hook, no durations recorded (`.pytest_cache/v/cache` has only `lastfailed`, `nodeids`); 12 full compiles plus the two `-O2` builds of `make` plus the emulator runs put it well over the one-minute line of global CLAUDE.md §2.
- Proposal: `dmtest.lib.load_private(module)` copies `build/libdm.dylib` to a per-module temp path and loads it (a different path is a different dyld image with its own globals; the ad-hoc signature is content-based and survives a copy; verify once). Drop the 12 clang calls and the 22 redundant `make` calls; `-Werror` moves to the Makefile (A14). Expected: the suite loses 12 compiles. Size M.

### B5. Hard-coded machine paths

- `tests/test_automap.py:11` and `tests/test_modes.py:22`: `SCRATCH = Path("/private/tmp/claude-501/-Users-lego-github-dm/a035dd1c-…/scratchpad")`, a Claude session folder from another session, used to dump PNGs when it exists (`test_automap.py:225-226`, `test_modes.py:97-107`).
- `tests/csb_diff.py:3`: `/Users/lego/github/dm` in the docstring.
- `tests/test_control.py:60`, `tests/test_csb_tools.py:23`: `tempfile.mkdtemp(dir="/tmp")` on purpose (sun_path length, commented): keep, in one helper.
- Proposal: an optional `DM_TEST_PNG_DIR` environment variable or delete the dumps; fix the docstring. Size S.

### B6. Skip conditions are copy-pasted

- `all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT"))` in about 10 files, the CSB variant in 7, `shutil.which("clang")` in 20; some skips happen inside fixtures (`test_automap.py:35`, `test_creatures.py:79`, `test_move.py:49`) and are not visible as markers.
- Proposal: `needs_dm`, `needs_csb`, `needs_clang`, `needs_sdl`, `needs_ghidra` in `dmtest/markers.py`, or auto-skip by folder in `conftest.pytest_collection_modifyitems`. Size S.

### B7. `-n auto` in `addopts`

- `pyproject.toml:36` `addopts = "-n auto --dist loadfile"`; the python-uv skill says not to (one file starts all workers), the project CLAUDE.md:12 relies on it. Decide once: a `make test` / `scripts/test.sh` with `-n auto` and the green-tree record (global CLAUDE.md §2), and `addopts` without `-n`. Size S.

### B8. Lint and format for Python

- No `ruff`/`flake8` configuration in `pyproject.toml` although `# noqa: F401` comments show a linter was used by hand. Proposal: `ruff` in the dev group with `[tool.ruff]` (line length 160 to match the code), `uv run ruff check` in `make check`. Size S.

### B9. `test_standard.py` is a good custom linter; two gaps

- It checks only `port/src`; nothing checks the tests. Its format check needs clang-format 23 from Homebrew (A16). The 60-line function rule has an allowlist (`standard_allowlist.txt`, 11 lines) that is honest and small. Add the A6 and A9 checks. Size S.

---

## C. `tools/`

### C1. Layout mixes a package, scripts, Java, and research

- `tools/dmtools/` (7 modules, 947 lines) is the uv package: `pyproject.toml:21` `packages = ["tools/dmtools"]`; it works through the editable install only (`tools/` is on no path). CHANGELOG 0.13.0 shows the move from `src/dmtools/` was deliberate ("All tools are in tools/").
- `tools/csb_match.py` (455 lines) at top level imports `dmtools.ftl` and `machine68k` and hard-codes `game/KAOS.FTL`, `docs/kaos-symbols.csv`, `reference/decomp/csb-a/APPB.c` relative to the cwd (`csb_match.py:36-37`). It produces `docs/csb-appb-symbols.csv`: a real tool.
- `tools/platforms/` (27 files, ~1,100 lines): research scripts for the other versions: `import struct,wave,os,gdat,dec` (`extract.py:1`), `sys.path.insert` (`compare.py:4`), `ROOT = os.path.join(dirname, '..', '..', '')` string paths in 6 files, one-letter globals (`R, E, D, r`), no docstrings, `capstone`/`numpy` imports (4 files) that are in no dependency group, and the hyphenated folder `fm-towns-csb` that Python cannot import. README:74 says they "need extra packages for some scripts".
- Shell: `make-dist.sh`, `make-dist-win.sh`, `decompile-all.sh`, `ghidra/decompile.sh` are fine (`set -euo pipefail`, cd to the root). `make-dist.sh:16-35` embeds the distribution README in a heredoc; `make-dist-win.sh:28` uses BSD `sed -i ''`; `decompile.sh:5-6` is brew-bound.
- Proposal: `tools/dmtools/` stays (owner's decision; a second `src/` next to `port/src` would confuse more than it helps); `csb_match.py` → `tools/dmtools/csb_match.py` with a `dm-csb-match` entry point; `tools/platforms/` → `tools/research/platforms/` with a README that says "research scripts, not maintained, not tested, need capstone and numpy" and a `research` dependency group; `decompile-all.sh` → `tools/ghidra/`; `make-dist*.sh` → `tools/dist/` or Makefile targets `dist`, `dist-win`. Size S-M.

### C2. Packaging details

- `mcp>=2.3.0` is a runtime dependency used only by `dm-mcp` (`control_client.py:1` was written to avoid importing it). Proposal: `[project.optional-dependencies] mcp = ["mcp>=2.3.0"]` and `uv sync --extra mcp`. S.
- `dmcsb>=0.2.1` in the dev group is a PyPI reference library used by two tests (`test_recruit.py:7-8`, `test_tables.py:145`); fine, but say so in the testing doc. S.
- `pytest-timeout` with `timeout = 120`: fine.

### C3. What belongs in git

- Tracked and fine: `uv.lock`, `.git-blame-ignore-revs`, `playthroughs/template/*.md` (8 player-memory templates), `.claude/agents/*.md` (the LLM-run agents; they are part of the LLM-mode feature).
- `run.txt` (untracked, 77 bytes): the owner's launch command, a duplicate of README:106 and `Makefile:20`. Delete. S.
- `.gitignore:3` `/ghidra/` ignores a folder that no longer exists; `playthroughs/template/DMGAME.DAT` (a 60 KB save) is ignored but present locally: fine. S.

---

## D. Repository root and `docs/`

### D1. `README.md` is three documents and its status is stale

- Lines 3-76 and 256-271: the decompilation tooling guide (stage 1); 77-255: the player and operator manual of the port; 272-300: tests and doc index.
- Stale: line 3 "The goal is a portable C version of the game for macOS" (a Windows build exists, line 97); the status table (9-16) says "Game logic: In progress: game state, random numbers, timeline, text, sound done" and links the 2026-10-04 plan, while CHANGELOG is at 0.14.1 with the full game; "Known limitations" (256-259) are about the decompiled C not compiling.
- 11 of 17 docs are not linked from the README (`coding-standard`, `rules-*`, `csb-*`, `game-state`, `tables`, `todo`).
- Proposal: README = what it is, status in two lines, quickstart (build, run, test), doc index; `docs/manual.md` (start screens, controls, modes, saves, CSB: lines 111-255), `docs/decompiling.md` (lines 25-76, 258-270), `docs/development.md` (build, tests, layout, standard, CI). Size M.

### D2. `docs/` layout: 17 files of five audiences in one folder

- Formats (`ftl-format`, `dungeon-format`, `save-format`), rules (`rules-champions`, `rules-creatures`, `rules-movement`, `tables`, `game-state`), CSB research (`csb-data`, `csb-engine-diff`, `csb-new-adventure`, `csb-port-integration`), LLM mode (`llm-mode`, `llm-playbook`), process (`coding-standard`, `todo`), reference (`kaos-symbols.csv` 1,718 rows, `csb-appb-symbols.csv` 1,105 rows, `kaos-map.md`, `symbols/range-*.md`), and `superpowers/` (6 plans, 4,000+ lines of finished plans and inventories, `status.md`).
- Proposal: see the target tree (section F). Note the decision: `CLAUDE.md:22` says "Plans in `docs/superpowers/`"; if the superpowers plugin writes there by convention, keep the folder name and only archive finished plans into `docs/superpowers/archive/`. Size M (moves plus link fixes).

### D3. Docs the code no longer matches (five spot checks, all confirmed)

1. `docs/superpowers/status.md`: "Updated: 2026-10-04 16:50. Branch: feature/2026-10-03-dm-tooling-disk-analysis", "261 tests pass", "Next steps: Finish steps 7b, 10, 11". Last commit to the file: `15f216e` 2026-10-05. Today: 427 tests, v0.14.1, the clean-code pass merged. Delete it (CHANGELOG and README carry the status). S.
2. `docs/llm-mode.md:250-253` C API table: `int control_open` "Returns 1 on success", `int control_is_turn_pending` "1 while a turn runs". `control.h:12, 16` are `bool` since the clean-code pass. S.
3. `docs/game-state.md:196`: "Reads the step tables from KAOS: `g_direction_to_step_east` (0x101038) and `g_direction_to_step_north`". No such globals in `port/src`; the port has `tables_get_step_east/north` (`data/tables.h:183-184`). S.
4. `README.md:9-16` status table and `README.md:256-259` known limitations (see D1). S.
5. `docs/symbols/range-4-6.md` (2026-10-07): 27 of 36 function names in backticks (`cache_alloc_block`, `fill_viewport_zone`, `get_native_bitmap_or_graphic`, ...) do not exist in the port. The file documents the original KAOS code; nothing says so, and `docs/kaos-symbols.csv` (`address,name,signature,subsystem`) has no column for the port name. Add one line at the top of each reference doc ("original KAOS symbols; port names differ") or a `port_name` column where a mapping exists. S-M.

### D4. Missing files

- `LICENSE`: without one the repository is "all rights reserved". The code is a reimplementation from a decompilation of a copyrighted game: the owner must choose consciously (the game data is excluded already, `.gitignore`). S, but a decision.
- CI: none (`.github/` absent). See A16. S-M.
- `.editorconfig`: 4-space C and Python, LF, final newline, matching `.clang-format`. S.
- `docs/architecture.md`: the one document a senior reader expects. The pieces exist: layer rule (`coding-standard.md:37`), frame loop and turn integration (`llm-mode.md:261-272`), hooks (`data/os.h`), build and test strategy (CLAUDE.md). One page with a diagram of the layers, the frame loop, the control socket, and the test seams (`libdm.dylib`, `state_json_get_string`, `g_frame_hook`). S-M.
- CONTRIBUTING: not needed for a one-owner repo; `docs/development.md` covers it.

### D5. Versions

- CHANGELOG has 22 releases (0.1.0 → 0.14.1) and no git tag exists (`git tag` is empty). Tag the releases from the CHANGELOG dates, or drop version numbers from the CHANGELOG and keep dated sections. S.

### D6. Personal data in the repository

- `playthroughs/lego/` (ignored) holds the personal binary and saves; `port/Makefile:20-24` `lego` target and README:103-109 name the owner; `run.txt` (C3). Proposal: `make play PLAY_DIR=playthroughs/local` with the folder ignored; README says "a personal play folder". S.

---

## E. What is good and should stay

- The four-layer rule with a test that enforces it (`test_layers`), the prefix-per-header rule, and the `data/os.h` hooks that keep SDL out of the game code: this is the right shape for a testable port.
- `test_standard.py` as a repo-specific linter with an allowlist that carries reasons: honest, small, and it caught real things during the clean-code pass.
- The control socket design: one JSON line per request, turn semantics (`control.c:1317-1329`), `g_frame_hook = NULL` in turn-based mode (`main.c:451`), and `state_json_get_string` as a test seam. The `witnessed` record (what the party saw during a turn) is a thoughtful protocol feature.
- Headless smoke test with `SDL_VIDEO_DRIVER=dummy` (`test_port.py:55`): makes CI possible.
- Golden snapshots (`tests/golden/`, 20 KB) and the differential tests against the emulator: the real guarantee that behavior did not change; they make the refactor safe.
- `conftest.pytest_configure` builds once before the workers (`conftest.py:83-86`); `BOOL_GLOBALS` + `global_type` keep ctypes honest about `bool` globals.
- `.clang-format`, `.git-blame-ignore-revs`, Conventional Commits with one change per commit (the last 30 commits read as a story), CHANGELOG in Keep-a-Changelog form.
- `docs/coding-standard.md` is short (69 lines) and every rule has a check or an allowlist. Keep its shape; extend it (A6, A9, A12) rather than rewrite it.
- `tools/dmtools` entry points (`dm-extract`, `dm-ftl`, `dm-play`, `dm-mcp`) and `control_client.py` kept free of `mcp` for fast starts.
- `playthroughs/template/` as the player-memory contract of the LLM runs.

---

## F. Target directory structure

```
dm/
├── README.md                    what it is, status (2 lines), quickstart, doc index
├── CHANGELOG.md
├── LICENSE
├── CLAUDE.md
├── Makefile                     thin front: all, test, check, format, dist, dist-win, play, clean
├── pyproject.toml, uv.lock
├── .clang-format, .editorconfig, .gitignore, .git-blame-ignore-revs
├── .github/workflows/ci.yml     macOS: build, windows cross-build, standard, format, tests (synthetic data)
├── .claude/agents/              dm-coordinator, dm-player (LLM runs)
├── port/                        the C port
│   ├── Makefile                 objects + -MMD, -Werror, pkg-config sdl3, SANITIZE=1, windows
│   ├── src/
│   │   ├── data/                file formats, tables, KAOS addresses (unchanged)
│   │   ├── engine/              game rules (unchanged)
│   │   ├── ui/                  screens, dungeon view, input mapping (unchanged, minus dr_mp3.h)
│   │   ├── control/             turn-based protocol: json.c, state_json.c, control.c, control_actions.c,
│   │   │                        control_turn.c, control_socket.c, control_request.c, png.c, control_none.c
│   │   ├── platform/            SDL only: platform.c, scale.c
│   │   └── app/                 main.c (30 lines), options.c, app.c (start screens, frame loop)
│   └── third_party/dr_mp3/      vendored decoder, license, version
├── tools/                       developer tools (maintained, tested)
│   ├── dmtools/                 the uv package: extract, ftl, emu, csb_mini, csb_match, play, mcp_server, control_client
│   ├── ghidra/                  *.java, decompile.sh, decompile-all.sh
│   ├── dist/                    make-dist.sh, make-dist-win.sh (or Makefile targets)
│   └── research/                not maintained, not tested; README says so
│       └── platforms/           DOS/ST/IIGS/FM-Towns/SNES/DM2 scripts (fm_towns_csb/, snes/)
├── tests/
│   ├── conftest.py              build once, markers, sessionfinish green-tree record
│   ├── dmtest/                  helpers: lib.py (private copy), constants.py (from C headers), game.py,
│   │                            savefile.py, control.py, emu.py, markers.py
│   ├── port/                    port behavior through libdm (gameloop, control, modes, select, ui, view, ...)
│   ├── differential/            against the emulator (champion, creatures, move, tables, csb_*, csb_mini)
│   ├── golden/                  *.json + test_golden_*.py
│   ├── tools/                   dmtools tests (adf, extract, ftl, emu, ghidra, symbols) + e2e csb_tools
│   └── standard/                test_standard.py, allowlist.txt
├── docs/
│   ├── architecture.md          layers, frame loop, control socket, test seams (new)
│   ├── manual.md                player manual (from README)
│   ├── development.md           build, test, layout, CI (from README + CLAUDE.md)
│   ├── decompiling.md           Ghidra tooling (from README)
│   ├── coding-standard.md
│   ├── todo.md
│   ├── formats/                 ftl-format, dungeon-format, save-format
│   ├── rules/                   rules-champions, rules-creatures, rules-movement, tables, game-state
│   ├── csb/                     csb-data, csb-engine-diff, csb-new-adventure, csb-port-integration
│   ├── llm/                     llm-mode (protocol + vocabulary), llm-playbook
│   ├── reference/               kaos-symbols.csv, csb-appb-symbols.csv, kaos-map.md, symbols/range-*.md
│   └── superpowers/             PRDs and plans (name kept for the plugin); archive/ for finished plans
├── playthroughs/                ignored except template/; local/ for personal play (was lego/)
├── game/, game-csb/, reference/, build/   ignored (original files, decompilation, build output)
```

Reasons: (1) each folder has one kind of content and one audience; (2) the C layer list reads as a dependency order (`data → engine → ui → control → platform/app`) and the standard's check stays one list; (3) `tools/` holds only maintained tools, research is labelled; (4) tests are grouped by what they prove (differential = fidelity, port = behavior, golden = regression, tools = tooling, standard = style), which is also what a CI matrix wants; (5) docs are grouped by audience with one architecture page on top; (6) the root keeps four tracked folders.

### Rename map

| Today | Target |
|---|---|
| `port/src/platform/control.c` | `port/src/control/control.c` + `control_actions.c`, `control_turn.c`, `control_socket.c`, `control_request.c`, `png.c` |
| `port/src/platform/state_json.c`, `.h` | `port/src/control/state_json.c`, `.h` + `json.c`, `json.h` |
| `port/src/platform/control_win.c` | `port/src/control/control_none.c` |
| `port/src/platform/main.c` | `port/src/app/main.c` + `options.c`, `app.c` (+ `debug_view.c` or dropped) |
| `port/src/platform/platform.c`, `scale.c` (+ `.h`) | unchanged |
| `port/src/ui/dr_mp3.h` | `port/third_party/dr_mp3/dr_mp3.h` |
| `tools/csb_match.py` | `tools/dmtools/csb_match.py` (`dm-csb-match`) |
| `tools/platforms/**`, `fm-towns-csb/` | `tools/research/platforms/**`, `fm_towns_csb/` |
| `tools/decompile-all.sh` | `tools/ghidra/decompile-all.sh` |
| `tools/make-dist.sh`, `make-dist-win.sh` | `tools/dist/` or `make dist`, `make dist-win` |
| `tests/csb_diff.py` | `tests/differential/test_csb_mini.py` |
| `tests/test_{champion,creatures,move,game,dungeon,spells,sound,text,tables,savefile,recruit,csb_rules,csb_engine,csb_magic_map,csb_text,csb_ending}.py` | `tests/differential/` |
| `tests/test_{gameloop,control,modes,select,entrance,ui,panels,view,automap,save,actions,port,port_data,music,scale,csb_adventure,csb_save}.py` | `tests/port/` |
| `tests/test_golden_*.py`, `tests/golden/*.json` | `tests/golden/` |
| `tests/test_{adf,extract,ftl,emu,ghidra,symbols,csb_tools}.py` | `tests/tools/` |
| `tests/test_standard.py`, `tests/standard_allowlist.txt` | `tests/standard/test_standard.py`, `allowlist.txt` |
| shared classes in `test_gameloop`, `test_champion`, `test_creatures`, `test_savefile`, `test_control`, `test_port` | `tests/dmtest/*.py` |
| `docs/{ftl,dungeon,save}-format.md` | `docs/formats/` |
| `docs/rules-*.md`, `tables.md`, `game-state.md` | `docs/rules/` |
| `docs/csb-*.md` | `docs/csb/` |
| `docs/llm-mode.md`, `llm-playbook.md` | `docs/llm/` |
| `docs/kaos-symbols.csv`, `csb-appb-symbols.csv`, `kaos-map.md`, `symbols/` | `docs/reference/` |
| `docs/superpowers/status.md` | deleted |
| `docs/superpowers/2026-10-0{4,5,6}-*.md`, `2026-10-15-clean-code-plan.md`, `-inventory/` | `docs/superpowers/archive/` |
| `README.md` sections 111-255, 25-76 + 258-270 | `docs/manual.md`, `docs/decompiling.md` |
| `run.txt` | deleted |
| `playthroughs/lego/`, `make lego` | `playthroughs/local/`, `make play` |

Paths that change in code and config: `port/Makefile:5,7,31`, `tests/conftest.py:10-16`, `tests/test_standard.py:9-12,292`, `pyproject.toml:21,36-38`, `tools/ghidra/decompile-all.sh:11`, `tools/dmtools/csb_match.py:36-37`, `CLAUDE.md` (layout, suite command, plans folder), `.claude/agents/*.md` (doc paths), `docs/coding-standard.md:37`, every `docs/` cross-link, `README.md`.

---

## G. Priority order for the PRD

1. B4 private-library helper (biggest suite-time and isolation win; prerequisite for moving tests).
2. A14 Makefile (objects, `-Werror`, `pkg-config`, `test`/`format` targets) and A16 CI with clang-format from PyPI.
3. A1 + A2 + A4: `control/` layer and the split of `control.c`; `json.c`.
4. A6 + A9 + A11: `bool`, dead includes, enum instead of strings, with the new `test_standard.py` checks.
5. A7: `app.c`/`options.c`, `main.c` as a 30-line entry.
6. B2 + B3 + B5 + B6: `tests/dmtest/`, constants from headers, scratchpad paths, markers; then B1 folders.
7. D1 + D2 + D3: README split, docs folders, the five stale spots, `architecture.md`.
8. C1 + C3 + D6: `tools/research/`, `csb_match` into the package, `run.txt`, `play` target.
9. A15 `third_party/`, D4 LICENSE and `.editorconfig`, D5 tags.
