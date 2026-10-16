# Final refactor: handover

Date: 2026-10-16. State: survey done, PRD not written. Next session: read this file and the reports in this
folder, ask the open questions below one at a time, write `docs/superpowers/2026-10-16-final-refactor.md`
(PRD, gherkin for the epics), then one `prd-reviewer` pass (agentic-workflow step 3).

The request (user, 2026-10-16): review the sources of the port (a reverse-engineered Amiga binary after a few
refactor rounds) and propose the final refactor: readability, maintainability, coding standard, a clear
directory structure, so that a very senior programmer finds nothing to be ashamed of. The coding standard may
change. Game behavior must not change.

## Reports in this folder

Four read-only review agents wrote one report each (if a file is missing, the agent did not finish; its brief
is in the section "Agent briefs"):

- `review-data.md`: `port/src/data/`.
- `review-engine.md`: `port/src/engine/`.
- `review-ui.md`: `port/src/ui/`.
- `review-platform-repo.md`: `port/src/platform/`, `port/Makefile`, `tests/`, `tools/`, root files, `docs/`.

## Facts measured

| Item | Value |
|---|---|
| C source | 34,511 lines in `port/src/` (29,131 without the vendored `ui/dr_mp3.h`) |
| Largest files | `ui/view.c` 2,595; `engine/group.c` 2,302; `platform/control.c` 1,431; `engine/champion.c` 1,156; `platform/state_json.c` 1,077; `ui/ui_champion.c` 975; `ui/command.c` 882; `engine/projectile.c` 879; `data/tables.c` 790; `engine/action.c` 778; `platform/main.c` 736 |
| Largest headers | `engine/map.h` 502; `data/ids.h` 449; `data/tables.h` 366; `engine/group.h` 276; `engine/game.h` 229 |
| `extern` globals per layer | data 6, engine 78, ui 36, platform 0 |
| Globals without `g_` | `csb_copies`, `csb_copy_count` (`data/csbimage.h`), `kaos` (`data/ftl.h`), `fb`, `palette`, `palette_split_top`, `palette_split_bottom` (`ui/video.h`) |
| Hooks across layers | `g_endgame_sensor_hook` (engine → ui), `g_frame_hook` (ui → platform), `g_os_make_folder_hook`, `g_os_list_folder_hook` (data → platform) |
| Full rebuild | 7 s (`make -C port` compiles all files in one clang command per target, no object files, no dependency tracking) |
| Full suite | 655 tests, 13 s wall (`uv run pytest -q`, `-n auto --dist loadfile`) |
| Extra warnings | `-Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla`: 0; `-Wimplicit-fallthrough`: 5; `-Wconversion`: 995 (not worth it) |
| Static analyzers | none installed (no cppcheck, clang-tidy, scan-build) |
| `goto` | 8 (cleanup labels) |
| Missing at the root | `LICENSE`, CI (`.github/`), `.editorconfig`, an architecture doc; `run.txt` is an untracked personal file |

## Findings so far (own read of all headers and samples of the big .c files)

Function bodies read well after the clean-code pass; the open problems are structural.

### Module boundaries

- `engine/game.h` is a grab bag: `NORMALIZE`, turn helpers, `STAT_*`, `SLOT_*`, the `Champion`, `Party`,
  `TimelineEvent`, `ActiveGroup` structs, 20 globals, modes and cheats, the timeline API, `min16`/`max16`/
  `bounded_value`/`scale_value`, magic map position helpers. Prefix line: `game_ min16 max16 bounded_ scale_
  timeline_ position_ turn_`.
- Record layout is split over three headers although rule 2 says one module owns it: `engine/map.h` (`door_*`,
  `teleporter_*`, `rec_sensor_*`, `rec_text_*`, `rec_group_*`, `rec_projectile_*`, `rec_explosion_*`,
  `weapon_*`, `map_*`), `engine/group.h` (`group_possessions`, `group_health`, `group_set_behavior`, ...),
  `engine/projectile.h` (`projectile_set_slot`, `explosion_set_type`, ...). Three naming schemes for the same
  kind of function. Accessors in `map.h` still hold raw layout numbers (`REC(thing)[4] & 0x08`, `>> 7 & 0x0f`),
  allowlisted as "record layout accessors".
- `engine/sound.c` holds the game rule part (sound table, volume by distance, pending sounds) and a 44.1 kHz
  stereo mixer with Paula voices and the CSB intro music segments (`sound_mix`, called from
  `platform/platform.c`). `ui/music.c` decodes MP3 (`dr_mp3.h`, vendored in `ui/`) and holds soundtrack tables;
  `ui/musicmap.c` is 316 lines of table data. Audio is not a game rule and not a user interface.
- `ui/ui.h` is one header for three files (`ui.c`, `ui_champion.c`, `ui_menus.c`) with prefixes `ui_ panel_
  menus_`; `ui/gameloop.h` has `gameloop_ entrance_`; `ui/startend.h` has `startend_ wait_`. File names with a
  redundant `ui_` prefix inside `ui/`. `KEY_*` words split between `ui/command.h` and `ui/advanced.h`.
- `data/ids.h` (449 lines): `GFX_`, `ZONE_`, `ICON_`, `TEXT_`, `LAYOUT_` ids grouped by the module that added
  them ("Each module adds its own section"), not by kind; some values have two names (`ZONE_THE_END` 5 =
  `ZONE_DIALOG_ON_SCREEN` 5, `ZONE_FIELD_FIRST` 702 = `ZONE_WALL_D3L2` 702). `data/tables.h` holds a `UI_INPUT_*`
  enum and two accessor styles (cached struct pointers with `int` indices; call-time image reads with
  `int16_t`), scalars returned as pointers (`tables_get_junk_weight` returns `const uint8_t *`).
- `data/spells.h` keeps byte offsets in names (`SPELL_ENTRY_LEVEL 4`): passes the check, breaks its spirit.
  `engine/savefile.h` names save offsets `G_GAME_TIME` ... (look like globals).
- `platform/control.c` (1,431 lines) is the LLM turn protocol: request parsing, turn rules, witnessed events,
  and the Unix socket in one file; `platform/state_json.c` holds a hand-written JSON writer plus the state
  export. Only the socket is platform code.
- `ui/view.c`: 77 static functions in one file (walls, doors, ornaments, objects, creatures, projectiles,
  explosions, fields, smooth projectile motion, magic map).

### Naming and types

- Predicates without `is_` that return `int16_t`: `door_breaks_by_magic`, `door_button_present`
  (`engine/map.h`).
- `champion_index` is `int16_t` in some functions and `uint16_t` in others (`engine/champion.h`,
  `engine/action.h`); coordinates are `int` in `ui/text.h` and `int16_t` in `ui/draw.h`; return conventions mix
  `0/-1`, `bool`, and `1/0` (`adf_extract`, `gfx_open`, `layout_init`, `savegame_quick_save`).
- Globals that only pass a result between two functions: `g_move_result_*` (`engine/move.h`),
  `g_current_group_*`, `g_creature_damage_outcome` (`engine/group.h`), `g_sensor_rotation_effect_*`
  (`engine/sensor.h`).
- Include guards: `DM_ADF_H`, `DM_BYTES_H` vs `ADVENTURE_H`, `GAME_H` (two schemes).

### Tests

- 45 flat test files; test modules import helpers from other test modules (`from test_gameloop import Box,
  Game`, `from test_control import Client, pump`, 40 such imports); the `Game` harness lives in
  `tests/test_gameloop.py:54`.
- C constants mirrored by hand in each test file (`KEY_TAB = 0x4200`, `THING_WEAPON = 5`, ...; 23 such lines
  in `test_csb_magic_map.py` alone): they drift silently.
- `tests/test_automap.py:11` and `tests/test_modes.py:22` hold a hard-coded scratchpad path of an old session
  (`/private/tmp/claude-501/.../a035dd1c-.../scratchpad`).
- 12 test modules compile their own `libdm` copy from the sources with clang (`game_sources()` in
  `tests/conftest.py`); 19 load `build/libdm.dylib`.
- No `pytest_sessionfinish` green-tree record (CLAUDE.md §2 asks for it only above 1 minute; the suite takes 13 s).

### Docs and repo

- `docs/` is flat (20 files) plus `docs/symbols/`, `docs/superpowers/`. Stale: `docs/superpowers/status.md`
  (2026-10-04), `docs/csb-port-integration.md` ("Status: proposal. Nothing is implemented."),
  `docs/llm-mode.md` status table; `docs/symbols/range-4-6.md` links `range-4-6.csv`, which does not exist.
- `README.md` (300 lines) mixes the decompilation tooling, the build, and the game manual.
- 25 files in `docs/`, `README.md`, `CLAUDE.md` name `port/src/...` paths (65 distinct file paths): a rename of
  `port/` touches them all.
- `pyproject.toml` packages `tools/dmtools` (no `src/` layout); `tools/platforms/` has hyphenated folder
  names (`fm-towns-csb`), research scripts; `tools/csb_match.py` sits at the top of `tools/`.

## PRD outline (draft epics; refine with the reports)

1. Coding standard v2: rewrite `docs/coding-standard.md` as a structured document (layers and modules, headers,
   naming, types, globals, errors and results, comments, tests), one check per rule in `test_standard.py`, the
   allowlist empty of "open" lines. New rules: one header per `.c` file; include guard `DM_<FILE>_H`; every
   header starts with a purpose comment; `g_` on every global; result conventions (`bool` for success, an enum
   for several outcomes, `int` only for C sizes); one type per value kind (`champion_index` is `int16_t`
   everywhere, coordinates `int16_t`); record accessors `rec_<type>_<field>` with named bit constants.
2. Repository layout: `port/src/` → `src/`, `port/Makefile` → `Makefile` (root; see Q1), `third_party/dr_mp3/`,
   `tools/platforms/` → `tools/research/`, `LICENSE` (Q3), `.editorconfig`, CI (Q4), `run.txt` out of the root.
3. Build: incremental Makefile (objects in `build/obj/`, `-MMD` dependencies), targets `all test windows dist
   lego format clean`, the stricter warning flags that give 0 warnings today (`-Wshadow -Wmissing-prototypes
   -Wstrict-prototypes -Wvla -Wimplicit-fallthrough`, the 5 fall-throughs annotated).
4. Engine headers: split `game.h` (timeline into `timeline.h`/`timeline.c` with `timeline_process.c` renamed to
   `event.c`, 16-bit helpers into `math16.h`, modes and cheats stay in `game.h`); one record module
   (`engine/record.h`/`record.c`) that owns every record accessor with one naming scheme and named bits;
   result-passing globals become structs returned or passed by pointer (scope: Q2).
5. Audio: `engine/sound.c` keeps the rules; the mixer, the voices, and the intro music move to `ui/audio.c`
   (or a new `audio/` folder: decide in the PRD); `ui/music.c` and `ui/musicmap.c` follow; `dr_mp3.h` in
   `third_party/`.
6. UI modules: one header per file (`ui.h` → `ui.h`, `panel.h`, `menus.h`; `entrance.h` out of `gameloop.h`;
   `wait_frame` into its own module or `startend.h` with one prefix); `ui_champion.c` → `panel.c`, `ui_menus.c`
   → `menus.c`; `KEY_*` in one header; `view.c` split along its sections with an internal header.
7. LLM protocol: `platform/control.c` split into the protocol (request parsing, turn rules, witnessed events;
   no sockets, testable) and the socket transport; `state_json.c` split into `json.c` (writer) and
   `state_json.c`.
8. Tests: `tests/dmtest/` helper package (library build and load, the `Game` harness, constants read from the C
   headers at import), no cross-imports between test modules, no hard-coded machine paths, test folders by
   subject (`tests/tools/`, `tests/data/`, `tests/engine/`, `tests/ui/`, `tests/llm/`, `tests/csb/`).
9. Docs: `docs/architecture.md` (layers, modules, data flow, global state, hooks, how to add a rule), `README.md`
   short with links, `docs/manual.md` (the game manual part of the README), `docs/formats/`, `docs/rules/`,
   `docs/reverse-engineering/`, `docs/llm/`; stale pages removed or marked; links fixed.

Invariants as in the clean-code PRD: golden save, golden view, golden rules, differential tests, struct layouts
of the ctypes tests, the LLM protocol names.

## Open questions for the user (ask one at a time)

- Q1. Rename `port/` to `src/` with the Makefile at the root (touches 25 docs and the commands in CLAUDE.md),
  or keep `port/`? Recommendation: rename; the repository is the port now.
- Q2. Global state scope: (a) only the result-passing globals become structs, every global gets `g_` and one
  documented home; (b) also the engine state into a few structs (`Game`, `Party`, `Timeline`) with the ctypes
  tests adapted. Recommendation: (a); (b) is a large churn with little gain for a port of a 1987 game.
- Q3. License for the code (the game data stays excluded): MIT, GPL-3.0, or none.
- Q4. CI on GitHub Actions (macOS: build with 0 warnings, clang-format, `test_standard`, the tests that skip
  without game files): yes or no.
- Q5. Build system: keep Make (incremental) or move to CMake. Recommendation: Make.

## Agent briefs (to re-run a missing report)

Each agent: read-only, read every file of its scope in full, read `docs/coding-standard.md` and the
"Current state" and "Requirements" of `docs/superpowers/2026-10-15-clean-code.md` first; report numbered
findings with `file:line`, what, why it matters, a one-line proposal, a size (S/M/L), in the categories: module
boundaries and cohesion, naming, types and API shape, global mutable state, comments, remaining decompiler
flavor, error handling and resource ownership, dead code, other senior-reviewer items; also what is good and
should stay. Scopes: `data/`; `engine/`; `ui/` (skip `dr_mp3.h`); `platform/` + `port/Makefile` + `tests/` +
`tools/` + root files + `docs/` with a proposed target directory tree and rename map.
