# Final refactor: PRD

Date: 2026-10-16. Branch: `feature/2026-10-16-final-refactor`. Status: reviewed, waiting for the owner's approval of the change list.

## Goal

The repository reads as the work of a careful senior C programmer: a clear directory tree, one way to do each
thing, modules with one job and one header, no state that travels through globals, tests with shared helpers,
and documentation that matches the code. The plan rewrites the coding standard as a structured document, and
every rule has a mechanical check. Game behavior and the drawn pixels do not change.

The survey behind this PRD: `docs/superpowers/2026-10-16-final-refactor-inventory/` (a handover and four
review reports with `file:line` evidence; line numbers are of commit `0771406`). Every claim below without a
line reference is in one of the reports.

## Current state

Measured on 2026-10-16:

- `port/src/` has 34,511 lines of C in four layers; 29,131 without the vendored `ui/dr_mp3.h`. The largest
  files: `ui/view.c` 2,595, `engine/group.c` 2,302, `platform/control.c` 1,431, `engine/champion.c` 1,156,
  `platform/state_json.c` 1,077.
- `make -C port` compiles every file in one clang command per target (`port/Makefile:12-18`), 7 s per
  rebuild, no object files, no dependency tracking, no `-Werror`. The tests compile the library with
  `-Werror -O1` (`tests/test_control.py:35`), so the tested and the shipped flags differ.
- The full suite: 655 tests, 13 s. 12 test modules compile their own copy of the library from the sources
  (`tests/conftest.py:14-16`); 19 modules load `build/libdm.dylib` by the same path, so modules that run in one
  xdist worker share the C globals. 24 `from test_x import ...` lines in 20 files import helpers from other
  test modules; C constants are mirrored by hand in each file (`SLOT_HAND_ACTION = 1` in 7 files).
  `tests/test_automap.py:11` and `tests/test_modes.py:22` hold a scratchpad path of an old session.
- Constants: 1,578 `#define` lines and 829 anonymous `enum` members, 133 of them in headers without a
  written value. One block mixes decimal and hex (`WOUND_READY_HAND = 1 ... WOUND_LEGS = 0x10`,
  `engine/champion.h:25-31`; `SQUARE_HAS_THINGS = 0x10, SQUARE_RANDOM_ORNAMENT = 8`, `engine/map.h:20`),
  because the notation rule decides per value and not per block.
- Includes: `ui/gameloop.c` has 35 `#include` lines, `platform/control.c` 32, `ui/startend.c` and
  `platform/main.c` 29, `platform/state_json.c` 25; the include path is flat, so an include does not name
  its layer, and `.clang-format` does not sort includes.
- Names: 35 function names are longer than 36 characters (`game_update_map_coordinates_after_relative_movement`,
  `champion_apply_and_draw_pending_damage_and_wounds`, `inventory_get_object_removed_from_leader_hand`).
  The order of the functions in a file follows the history of the port, not the header; only two static
  functions have a forward declaration.
- Globals: 101 `g_` names in engine headers, 36 of them without a reader in `ui/` or `platform/`; result
  values travel through globals (`g_move_result_*`, `g_current_group_*`, `g_creature_damage_outcome`,
  `g_sensor_rotation_effect_*`). Globals without `g_`: `kaos` (`data/ftl.h:15`), `fb`, `palette`,
  `palette_split_top`, `palette_split_bottom` (`ui/video.h:28-33`), `csb_copies`, `csb_copy_count`
  (`data/csbimage.h:20-21`).
- Record layout: rule 2 names one owner, but accessors sit in `engine/map.h` (48 `REC` uses), `engine/group.h`
  (14), `engine/projectile.h` (10) and `engine/dungeon.c` (2), under four naming schemes (`rec_sensor_type`,
  `door_state`, `group_health`, `thing_weapon_type` beside `weapon_type`); 17 raw `g_current_map_data[x][y]`
  reads in `group.c`, `projectile.c`, `timeline_process.c`, `sensor.c`, `move.c`; the check matches only
  `g_current_map[` and `dungeon_get_thing_data(...)[`.
- `engine/game.h` holds seven prefixes, four structs of other modules, 29 globals, the timeline heap and the
  16-bit arithmetic helpers. `ui/ui.h` is one header for three files (`ui.c`, `ui_champion.c`, `ui_menus.c`).
  The frame wait (`g_frame_hook`, `wait_frame`) sits in `ui/startend.h:11-26`, so `dialog.c`, `logo.c`,
  `anim.c`, `command.c` depend on the top module of the layer.
- `platform/` holds three things: the SDL adapter (320 lines), the application (`main.c` 736 lines: start
  screens, options, frame loop) and the LLM protocol (2,532 lines, no SDL, linked into the test library).
- `engine/sound.c` is half rules and half a 44.1 kHz Paula mixer that `platform/platform.c:248` calls;
  `ui/music.c` decodes MP3 with `dr_mp3.h` (5,380 lines vendored inside `ui/`).
- Types: `champion_index` is `int16_t` in 18 declarations and `uint16_t` in 24; about 55 truth values are
  `int` or `int16_t` (the check covers only `is_/has_/can_` names); success is `int 0/-1` in 25 functions,
  `bool` in others, `1/0/-1` in one; nine `tables_get_*` accessors return a pointer to a scalar that every
  caller dereferences at once.
- Dead API: about 70 exported engine functions, 12 ui functions and 3 data functions have no caller outside
  their own file; 17 more exist only for the tests and are not marked.
- Undefined behavior and ownership: `data/swsh.c:26` forms a pointer 1 MiB before its buffer;
  `data/csbimage.c:184-185` publishes the image before `csb_text_build` can fail, so a failed CSB load leaves
  `kaos.data` set and `engine/dungeon.c:371` sees a loaded image; `ui/ui_champion.c:792-793` reads one byte
  before a scroll text that starts with a newline; `engine/group.h:144` indexes `g_active_groups` with an
  unchecked byte of the record.
- Docs: `docs/` is flat (17 pages of five audiences); `README.md` (300 lines) is the tooling guide, the
  build and the game manual in one; stale pages: `docs/superpowers/status.md` (2026-10-04),
  `docs/csb-port-integration.md` ("Nothing is implemented"), `docs/llm-mode.md:250-253` (`int` where the
  code has `bool`), `docs/game-state.md:196` (globals that no longer exist). No `LICENSE`, no `.editorconfig`,
  no architecture page. 25 files name `port/src/...` paths.

## Requirements

### R1. Repository layout

The tree after the refactor; every path in the code, the tests, the docs and `CLAUDE.md` follows it.

```
dm/
├── Makefile                      the build front: all, lib, windows, test, check, format, dist, dist-win, play, clean
├── README.md, CHANGELOG.md, LICENSE, CLAUDE.md
├── .clang-format, .editorconfig, .gitignore, .git-blame-ignore-revs
├── pyproject.toml, uv.lock
├── src/                          the C port (was port/src/)
│   ├── data/                     file formats, the engine image and its tables, run configuration
│   ├── engine/                   game rules
│   ├── ui/                       screens, the dungeon view, input mapping, audio output
│   ├── control/                  the turn-based protocol for a program player (was platform/control.c, state_json.c)
│   └── platform/                 SDL, the control socket, options, the application, main
├── third_party/dr_mp3/           the vendored MP3 decoder with its license and version (was src/ui/dr_mp3.h)
├── tools/
│   ├── dmtools/                  the uv package (+ csb_match.py as dm-csb-match)
│   ├── ghidra/                   the Ghidra scripts (+ decompile-all.sh)
│   ├── dist/                     make-dist.sh, make-dist-win.sh
│   └── research/platforms/       the research scripts of the other versions (was tools/platforms/); not tested
├── tests/
│   ├── conftest.py
│   ├── dmtest/                   shared helpers: the library loader, constants from the C headers, the Game harness
│   ├── differential/             the same input through the port and the emulator
│   ├── port/                     port behavior through the library
│   ├── golden/                   the snapshot tests and their *.json
│   ├── tools/                    tests of tools/dmtools
│   └── standard/                 test_standard.py, the docs checks, allowlist.txt
├── docs/
│   ├── architecture.md           layers, modules, the frame loop, the control socket, global state, hooks, test seams
│   ├── manual.md                 the player manual (from README.md)
│   ├── development.md            build, tests, layout, the standard, release steps (from README.md and CLAUDE.md)
│   ├── decompiling.md            the Ghidra tooling (from README.md)
│   ├── coding-standard.md
│   ├── todo.md
│   ├── formats/                  ftl, dungeon, save formats
│   ├── rules/                    champions, creatures, movement, tables, game state
│   ├── csb/                      the CSB research and integration pages
│   ├── llm/                      llm-mode (protocol and vocabulary), llm-playbook
│   ├── reference/                kaos-symbols.csv, csb-appb-symbols.csv, kaos-map.md, symbols/
│   └── superpowers/              PRDs, plans, checklists; archive/ holds the finished ones
├── playthroughs/                 ignored except template/; local/ is the personal play folder (was lego/)
└── game/, game-csb/, reference/, build/   ignored
```

1. The include path is `-Isrc` and `-Ithird_party`; every include names its layer:
   `#include "engine/game.h"`. File names inside a layer carry no layer prefix (`ui/ui_champion.c` goes),
   except the module that carries the name of its layer (`control/control.h`, `platform/platform.h`).
2. The five layers in dependency order: `data`, `engine`, `ui`, `control`, `platform`. A file includes only
   headers of its own layer and of the layers before it. The layer check reads the layer from the include path.
3. `third_party/` is outside the standard checks and the format check by folder, not by file name.
4. The research scripts get a README that says they are not maintained and not tested, and a `research`
   dependency group for `capstone` and `numpy`.

### R2. Build

1. One `Makefile` at the root: objects under `build/obj/<layer>/`, `-MMD -MP` dependency files, a rebuild
   after one changed `.c` file compiles that file only. Targets: `all` (the game and the library), `lib`,
   `windows`, `test` (`uv run pytest -q -n auto`), `check` (format check, the standard, the suite), `format`
   (`clang-format -i`), `dist`, `dist-win`, `play` (copies the game into `PLAY_DIR`, default
   `playthroughs/local`), `clean`.
2. Flags: `-std=c11 -Wall -Wextra -Werror -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla
   -Wimplicit-fallthrough -O2 -g`. The five fall-through cases get a `/* fall through */` comment. The
   Windows build uses the same flags. `SANITIZE=1` adds `-fsanitize=address,undefined` to the library and the
   game; `docs/development.md` names it.
3. The Makefile evaluates `SDL_PREFIX ?= $(shell brew --prefix sdl3)` only in the targets that link SDL (not
   in `clean` or `test`), and a caller can override it.
4. The tests do not compile the library with clang. They load `build/libdm.dylib`, which `conftest.py` builds
   once with `make lib` in the controller process before the workers start (`pytest_configure` without
   `workerinput`, as today); no worker runs `make`. The Makefile builds the library and the game with the same flags.
5. `clang-format` comes from the PyPI package `clang-format` (23.1.x) in the `dev` group; the standard check
   runs the one of the virtual environment and needs no Homebrew.

### R3. Coding standard v2

`docs/coding-standard.md` v2 exists since 2026-10-16, written before the plan, with the sections: layers and
modules, headers and includes, constants, naming, types and values, results and parameters, state, file
order, comments, data and errors, tests. It keeps its form: short, one rule per line, every rule with a check
in `tests/standard/test_standard.py`, or the mark "review only" when no check is possible, or a named
exception. The epic R3 implements the checks that the standard names and the allowlist; it does not write
the rules.
The allowlist keeps only exceptions that the standard documents: the soundtrack tables, the APPB address
tables, and the two per-type switch functions. Open violations during the run go in with `open: epic N` and
the last epic removes them.

Rules that are new or change:

1. Layers: five layers (R1.2); the include path names the layer. A `.c` file includes exactly the headers it
   uses, in this order: the own header, then project headers by layer order and alphabetical inside a layer,
   then third-party, then system headers. `.clang-format` sorts them (`IncludeCategories`), so the check is
   the format check. No umbrella header per layer (user, 2026-10-16).
2. Modules: one public header per module. A module with several `.c` files has one public header and one
   `<module>_internal.h` for the shared private declarations; both carry the prefix of the module. A header
   starts with the SPDX line, the guard `DM_<LAYER>_<FILE>_H`, the prefix line, one purpose paragraph, then the
   includes. A header includes only what its declarations need.
3. Naming: static functions carry no module prefix. Record accessors are `rec_<type>_<field>` and
   `rec_<type>_set_<field>`; `rec_` belongs to `engine/record.h`. Every global has `g_`. A symbol that exists
   only for the tests carries the comment `/* exported for the tests */` on its declaration; the dead-API
   check knows the marker. No ordinal (index + 1, 0 none) in a name or a variable; an index is -1 for none.
   Ordinals stay only where a game record stores them.
4. Function names: `<prefix><verb>_<object>` with at most four words after the prefix and at most 32
   characters. The plan gives the 60 header names above 32 characters, and the static names above it, shorter
   names, for example
   `game_update_map_coordinates_after_relative_movement` becomes `map_step`,
   `champion_apply_and_draw_pending_damage_and_wounds` becomes `champion_apply_pending_damage`,
   `inventory_get_object_removed_from_leader_hand` becomes `inventory_take_hand_object`.
5. Constants: every constant is `#define NAME value` with the value written out, a negative value in
   parentheses (`(-1)`); no `enum` for constants.
   The plan turns the 829 anonymous enum members into `#define` lines and writes the 133 implicit values.
   Related constants stand in one block without blank lines, aligned by `clang-format`, with one comment line
   above the block that names the set.
6. Notation per block, not per value: a block of bit flags, masks, packed layouts, addresses or file format
   bytes is hex in every line with the same digit width (`0x01`, `0x02`, `0x04`, `0x08`, `0x10`, `0x20`); a
   block of counts, ids, sizes, ticks, coordinates and thresholds is decimal in every line; a block never
   mixes the two. The check reads the kind from the name parts of the block and verifies every line of the
   block; the "10 or more" threshold goes.
7. Types: one table in the standard gives the type of each game value kind: `champion_index`, `cell`,
   `direction`, `map`, `slot`, `x`, `y`, pixel coordinates and sizes: `int16_t` (-1 none); `thing`,
   `ticks`: `uint16_t`; game time: `uint32_t`; `int` only for C sizes, C array counts and indices, and SDL
   values; every truth value is `bool`. The port assumes arithmetic right shifts of negative values: one
   `static_assert` in `data/bytes.h` and one sentence in the standard.
8. Results: two outcomes are a `bool`; more outcomes are an `enum` type with named values (the one use of
   `enum`: a result type, not a constant block); a lookup returns `NULL`; a count returns the count. No
   `0/-1`, no `1/0/-1`, no value encoded in a return (`index + 2`).
9. Parameters: at most five; a square is a `Location {int16_t x, y;}`, a square with its map a
   `Position {int16_t map, x, y;}` (review only); no flags packed into a parameter (`skill | flags`), no mode
   numbers that mean two things (`sound mode 2 and up`); a small struct replaces two or more output
   parameters.
10. State: a global holds state that lives across calls; a result travels through a return value or a struct
    out-parameter; module-private state is `static`; a file with four or more file-scope variables keeps them
    in one `static struct` named after the module (review only).
11. File order: includes; constants; types; file-static state; functions. The public functions stand in the
    order of the header, and the header lists them by topic (setup and reset first, then queries, then
    commands); a one-line section comment names each topic in the header and in the `.c` file. A static
    helper stands directly above the first function that calls it; a helper that several sections share
    stands at the top of the functions. A forward declaration of a static function is allowed only for mutual
    recursion (`group.c:1477` today), with a comment that names the pair.
12. Comments: the one-line comment of a public function is in the header above its prototype; the comment of
    a static function is above its definition; never both. No original symbol names, addresses or event
    numbers in comments: name the constant (`EVENT_ENABLE_CHAMPION_ACTION`). No TODO markers in code
    (`ponytail:`); open work goes to `docs/todo.md`.
13. Data and errors: `data/` and `engine/` never write to stderr and never exit; they log and fail through the
    hooks `g_os_log_hook` and `g_os_fatal_hook` of `data/os.h`. `data/os.c` holds the default (stderr and
    `exit`); `platform/` and the tests replace it; `os_fatal` never returns. A loader returns `false` on a bad
    file; an address outside the image is a port bug and calls `os_fatal`. The rule "read game data at run
    time" of the section data and errors gets its exception: a table that is not in the game files (the
    soundtrack tables) stays in the source, named in the allowlist.
14. Tests (a short section that points to `docs/development.md`): a test module imports helpers from
    `tests/dmtest/` only, never from another test module; C constants come from the headers through
    `dmtest.constants`; a test calls accessors, it does not decode records; no machine paths.

New checks (the plan sizes each): the RECORD check also matches `REC(`, `g_current_map_data[` and
`g_dungeon_map_data[` outside `engine/record.c`; the DECIMAL check also matches single digits in `[N]` and
`< N;`; `enum` in `src/` outside a result type; notation per block; function names above the limits; the
order of the public definitions against the header; one type per parameter name across the headers of a
layer; `bool` for truth values (locals assigned from a comparison or from `true`/`false`, `int` functions
with "Returns 0/1" comments); prototypes with more than five parameters; names with `_ordinal` or `_plus_1`;
a comment line above both the prototype and the definition; static functions without a module prefix;
exported names with no use outside the owner and no test marker; the header head order and the guard scheme;
dead includes (a header whose declarations the file does not use); TODO markers; event numbers in comments;
the allowlist with only documented reasons.

### R4. data/

1. `data/kaos.h/.c` owns the engine image: a `static` image, `kaos_load`, `kaos_is_loaded`, `kaos_free`,
   the typed readers (`const uint8_t *kaos_ptr`, `kaos_s8` for the nine `(int8_t)kaos_u8` casts) and the one
   writer `kaos_write_u16` for the mouse table patch. `data/ftl.h/.c` keeps the generic loader and gets
   `ftl_at(image, address)`: the one place that computes an image pointer; `swsh.c:26`, `csbimage.c:154-157`
   and `ftl.c:273-280` use it (the UB goes). `engine/dungeon.c:371` and `tables.c:201` call `kaos_is_loaded`.
2. `csb_image_build` builds into a local, runs `csb_text_build` on it, frees on failure and publishes the image
   only on success. Files: `csb_image.c`, `csb_text.c` (names match their prefixes).
3. `tables_load` is the only public entry; `tables_init` becomes static; the `ready` flag and the lazy
   `tables_init()` in `ui/view.c:2159-2160` go; the tests call `tables_load`. All table accessors take
   `int16_t`; the nine scalar accessors return by value. `tables_get_door_info` and its table go
   (`tables_get_door_attributes` stays). `UI_INPUT_*` becomes `TABLES_MOUSE_*`.
4. Spells: one owner. `spell_find` loops `tables_get_spell`; `get_spells_address` goes; `magic.c:407-416`,
   `ui_menus.c:198` and `state_json.c:395-400` call `spell_get_symbol` and `spell_get_symbol_mana_cost`
   instead of their own copies; the record byte offsets `SPELL_ENTRY_*` and the attribute unpackers move into
   `tables.c` beside `CREATURE_AT_*`.
5. `data/ids.h` is grouped by kind (`GFX_`, `ZONE_`, `LAYOUT_`, `ICON_`, `TEXT_`), sorted by value, one name
   per value (`ZONE_THE_END` goes, `ZONE_DIALOG_ON_SCREEN` stays); the comments name `tables_get_*`, not
   dead globals. The font metrics become `FONT_*` and stay in `data/graphics.h` (`engine/message.c` uses
   them).
6. `Box` moves to `data/graphics.h` beside `Bitmap`, with `box_contains(const Box *, x, y)`; `TablesRect`
   goes; `tables_get_pointer_planes` returns a struct. `PATH_SIZE` and `os_compare_names` (as
   `file_compare_names`) move to `data/file.h`; `file_write` and `file_replace` join them for
   `engine/savegame.c`.
7. `adventure.c`: one `Adventure` struct per adventure with its file list; both loops stop at the NULL name;
   `adventure_load_files(dir)` owns the "engine module plus Graphics.DAT" sequence that `main.c:40-50` and
   `gameloop.c:50-60` repeat. One API: `adventure_get(adventure)` and `adventure_find(name)`; the field
   accessors go (a caller writes `adventure_get(g_adventure)->dir`).
8. Names: `KAOS_PLAQUE_HEIGHTS` names the real table start and the accessor writes `lines - 1`; the address
   names follow their accessor nouns (`KAOS_INTERFACE_PALETTE`, `KAOS_PALETTE_CHANGES_INVISIBILITY`,
   `KAOS_REBIRTH_STEP_2_SCALES`, `KAOS_ACTION_DISABLED_TICKS`, `KAOS_ICON_SHEET_FIRST_ICONS`,
   `KAOS_FOUNTAIN_ORNAMENT`); noun-only public functions get verbs (`gfx_get_string`). The FTL and
   Graphics.DAT format values get names (`FTL_FORMAT_VERSION`, `FTL_ENTRY_*`, `GFX_HEADER_BYTES`);
   `read_objects` and `read_actions` use named offsets and designated initializers.
9. `BE16`, `WR16`, `BE32`, `WR32` become `static inline` functions (no double evaluation, no comma operator);
   `GRAPHIC_*_OFFSET_RANGE` macros become `static inline` functions; `crc32` uses a `static const` table.
   `gfx_close` and `kaos_free` exist; `gfx_open` frees on every failure path.

### R5. engine: modules and records

1. `engine/record.h/.c` owns every record accessor (doors, teleporters, texts, sensors, groups, objects,
   projectiles, explosions, the map descriptor, the dungeon header) in thing-type order, with the scheme of
   R3.3, named bit constants per record (`SENSOR_DELAY_SHIFT`, `SENSOR_DELAY_MASK`, ...) and one `BITS(word,
   shift, mask)` helper; the `REC` macro is private to it; `const uint8_t *rec_read(thing)` and `uint8_t
   *rec_write(thing)` replace the one writable record pointer. The two allowlist lines for the record layout
   go. `thing_weapon_type` goes (`rec_weapon_type` stays); `THING_END` goes (`THING_ENDOFLIST` stays).
2. `engine/game.h` splits into these modules:
   - `timeline.h/.c`: the `TimelineEvent`, a `Timeline` struct with the events, the heap, the counts, and
     the allocation that `game.c:184-199` and `savegame.c:472-490` repeat.
   - `event.h/.c` (was `timeline_process`): the dispatcher; the 13 handlers become static.
   - `party.h/.c`: the `Party` struct, the party position from `dungeon.h`, the scents, the light rules from
     `inventory.c:546-606`, and `g_dungeon_view_palette_index` as `g_party_light_level`.
   - `champion.h`: the `Champion` struct and the statistic helpers of `game.c:392-435`.
   - `arith.h`: `min16`, `max16`, `bounded_value`, `scale_value`, `abs16` as `static inline`.
   - `game.h`: the session (R7.1), the time, the modes, the cheats, the redraw flags and `game_fatal`.
   - `map.h`: the direction helpers `turn_right`, `turn_back`, `turn_left` become `direction_right`,
     `direction_back`, `direction_left` (prefix `direction_`), so that `turn_` is free for `control/turn.h`.
3. One copy of each helper: `map_is_party_on`, `map_step` (replaces
   `game_update_map_coordinates_after_relative_movement` and its three wrappers),
   `map_find_thing(location, type)` (the first thing of a type on a square), `champion_face_party_direction`, the side-of-cell helper of
   `group.c:1021` in `map.h` under `direction_`, `game_fatal`, and `message_print_dungeon_text(text_thing)`
   in `message.c` for the two `print_text` copies. `map_at_level` and `dungeon_get_square_first_thing` in
   `dungeon.c` go; `dungeon_validate` uses the map functions.
4. One move chain: `follow_square_chain` (`move.c:148-186`) and `follow_destination_squares`
   (`group.c:2099-2147`) become one walker with a mover kind (party, group, object, projectile), and
   `group_get_move_result` (`group.c:2194`) and `move_get_move_result` (`move.c:217`) become one
   `move_get_move_result` with group-specific hooks. The epic runs the group differential tests and the golden
   rules before and after; a difference stops the epic.
5. `savefile.h` uses the prefix `savefile_`; `SaveFile` says who owns `data` (a `bool owns_data` or a view
   type); `savegame.c` writes files through `data/file.h`. The offset enums get full names (`SAVE_GLOBAL_*`,
   `SAVE_HEADER_*`, `SAVE_PARTY_*`, `SAVE_EVENT_*`, `SAVE_GROUP_*`, `CHAMPION_RECORD_*`).
6. `explored` returns `bool` and uses the `EXPLORED_` prefix for its constants. `message_print` and its
   siblings read `g_game_time` instead of a parameter; columns are `int16_t`.
7. Sound: `engine/sound.c` keeps the rules (the sound table, the volume by distance, the pending queue,
   `sound_request_play`) and plays through `g_sound_play_hook(sample, length, period, volumes)`, which
   `ui/audio.c` sets (R9); with a NULL hook the engine skips the play, as `timeline_process.c:292` does for
   `g_endgame_sensor_hook`. `g_pending_sounds` and `g_pending_sound_volumes` stay exported with the test
   marker (`tests/test_sound.py` reads them).
8. Corrupt data: `group_get_active(thing)` in `group.c` (R7.5) reads a bounded index with a defined value, a
   comment and a deviation entry in the formats doc; `map_get_creature_attributes` the same (from
   `docs/todo.md`). `game_init_active_groups` clears the array (`docs/todo.md`: deterministic saves), and the
   golden tests go without `scrub_free_groups`.

### R6. engine: API shape

1. The type table of R3.7 applies to every engine header: `champion_index`, `cell`, `direction`, `slot`,
   `map` are `int16_t` with -1 for none; `ticks` and `thing` are `uint16_t`. `champion_*` functions take an
   index, not a `const Champion *`.
2. Results: `enum MoveResult`, `enum ClickResult`, `enum HitResult`; `savegame_save`, `savegame_quick_save`
   and `savegame_save_as` return `bool`; `savegame_quick_load` keeps its `SAVEGAME_*` result type.
3. Parameters: `group_process_reaction_event` takes a `REACTION_*` value only; `sound_request_play` takes a
   `SOUND_MODE_*` value and a separate tick count; `move_get_move_result` takes a `Location` source and
   destination plus explicit `bool` flags instead of negative coordinates; `champion_get_skill_level` and
   `champion_get_wound_defense` take the flags as separate parameters; `x_combo`/`y_combo` go. Every function
   with more than five parameters takes a struct (`ProjectileLaunch`, `MeleeAttack`, ...).
4. Output: `game_get_magic_map_position`, `map_get_level_change_location` (was
   `map_get_location_after_level_change`, 36 characters),
   `get_creature_spell_explosion`, `get_object_modifier`, `get_flask_explosion`, `cast_magic_map`,
   `is_material_group_on` return a struct; `click_test` returns the next thing.
5. Ordinals become indices: `g_inventory_champion_index`, `g_acting_champion_index`,
   `g_candidate_champion_index`, `group_get_creature_in_cell` and the other `*_ordinal` functions; the
   39 `ordinal - 1` sites go. `ui/` and the tests follow.
6. Names: `BEHAVIOR_USELESS_*` and `ACTION_STAB_1/2` get game names from the original tables (the plan sources
   them from `docs/reference/`); the `SLOT_*` block lists the slots in record order with a comment;
   `event->b` and `event->c` are read only through accessors outside `timeline.c` and `savegame.c`;
   `WEAPON_CLASS_*` has one definition in `data/tables.h`; `SKILL_STEAL = 8` completes the skill block.
7. The plan runs a script over every constant block; `ACTION_N`, `ACTION_BLOCK`, `ACTION_HIT` and other
   unused members stay only when a table documents them.

### R7. engine: state

1. A `Session` struct `g_session` in `game.h` holds the per-run flags that `game_reset_session` resets: the
   party sleeping and dead flags, the won flag, the mode, the three cheats, the saved-game flag and times, the
   candidate, inventory and acting champion indices, the action damage, the pending new map index.
   `g_leader_empty_handed` goes when the differential tests confirm it always equals
   `g_leader_hand_object == THING_NONE`; else it stays with a comment that says why.
2. Result-passing globals go: `g_current_group_*` and `g_group_secondary_direction` become fields of
   `GroupTurn` or return values; `g_create_launcher_projectile`, `g_projectile_poison_attack`,
   `g_projectile_attack_type` become parameters or an `Impact` return struct; `g_creature_damage_outcome`
   is a return value; `g_move_result_*` becomes a `MoveDestination {int16_t map, x, y, cell, direction;}`
   out-parameter of `move_get_move_result`; `g_sensor_rotation_effect*`, `g_action_target_group_thing`,
   `g_use_rope_to_climb_down_pit` become static or parameters.
3. Per-entity parallel arrays become struct fields: the pending damage, wounds, level-ups and portraits of a
   champion live in `Champion` as runtime fields after the saved fields, which `champion_pack` does not save;
   `ActionList`, `PendingSound`, `MusicSegment` structs replace the paired arrays.
4. File-static "out" variables of `group.c` (`movement_blocked_by_*`, `movement_tested_directions`,
   `flux_cages`, `drop_moving_creature_possessions_*`, `set_direction_*`) become a `MovementTest` return
   struct and fields of `GroupTurn`.
5. About 70 exported engine functions with no caller outside their file become static; the test-only exports
   get the marker of R3.3. `ui/view.c` reaches active groups and projectile events through
   `group_get_active(thing)` (one function in `group.c`; the inline of `group.h:144` goes) and
   `projectile_get_flight_direction(thing)`, so `g_active_groups` and the timeline events leave the public
   headers.

### R8. ui

1. Headers, one module each:
   - `ui.h` (`ui_`): zones, blits, icons, palette, hit tests.
   - `status.h/.c` (`status_`): the champion status boxes, icons, bars, damage (from `ui_champion.c`).
   - `panel.h/.c` (`panel_`): the inventory panel, slots, eye, scroll, chest, food and water (from
     `ui_champion.c`).
   - `menus.h/.c` (`menus_`, was `ui_menus.c`): the action area, the spell area, the arrows.
   - `entrance.h/.c` (`entrance_`): the entrance from `gameloop.c:68-171` and `g_new_game` as
     `g_entrance_choice`.
   - `frame.h/.c` (`frame_`): `g_frame_hook`, `frame_wait`, `frame_wait_count`, `g_frame_wait_depth`,
     `g_quit_requested` from `startend.h`, with the hook contract at the declaration ("shows a frame and
     reads input; runs no game logic").
   - `command.h`: all `KEY_*` words. The tick state of `command.h:82-85` moves to `gameloop.h`;
     `g_game_screen_inactive` becomes `g_game_screen_shown` in `gameloop.h`.
2. `video.h`: one `Screen g_screen` (`px[SCREEN_H][SCREEN_W]`, `palette[2 * PALETTE_COLORS]`, `split_top`,
   `split_bottom`) with `video_get_screen()` returning the `Bitmap` view; the three hand-made screen bitmaps
   and `ui_get_screen` go. `video_layout` and `video_map_mouse` move to `platform/platform.c` as
   `plat_layout` and `plat_map_mouse`.
3. Rules out of ui (the rest of R8 of the clean-code pass): `inventory_grab_object(cell, location)`,
   `inventory_drop_object(cell, location, into_alcove)`, `map_is_imaginary_wall(location)`,
   `move_is_blocked(command)`, `game_prepare_fuse_lord(lord, location)`, `map_remove_fluxcages(location)`,
   `group_delete_all_except(location)`, `champion_get_load_level(index)`, `explored_mark_visible(position,
   direction)` (the visibility query of `automap.c:24-81`, also used by `control/`), and
   `map_get_level_change_location(map, ...)` instead of the copy in `view.c:1421-1435`. The epic
   takes a golden rule snapshot of each moved rule before the move (`tests/golden/`).
4. Constants to their owners: `DOOR_NORTH_SOUTH` to `map.h`; `SOUND_FUSE` and `SOUND_TELEPORT` go
   (`SOUND_BUZZ` is the one name of sound 14; the plan confirms it in `docs/reference/`);
   `DECODE_MESSAGE_INVISIBLE` becomes the composition of `TEXT_MODE_*`; `ORNAMENT_VI_ALTAR` and the
   pressure pad graphic ids to `data/ids.h`; `SCROLL_FONT_SHIFT`/`RUNE_FIRST` use `SPELL_SYMBOL_FIRST`; one
   slot-box numbering block in `ui.h`; `video_amiga_color(r, g, b)` replaces the two packings;
   `SELECT_SCREEN_*` and `SELECT_CSB_*` in `select.h`, and the `Screen` type of `select.c:21` becomes
   `SelectScreen`.
5. `view.c`: a `View` static struct for the module state; a `Placement` argument and a `Box *drawn`
   out-parameter replace `zone_relative_x/y`, `shift_x/y` and `drawn_box`; a `SquareDraw` context replaces
   the 7-10 parameter lists; `ViewResult view_get_result()` replaces the ten output globals of `view.h`;
   the `Aspect` union becomes separate fields; `WallBitmap` and `ProjectileAspect` replace the sign-encoded
   tags; `ScaledBitmap` with `scaled_bitmap_free` replaces the "free whether or not allocated" convention;
   `ViewPosition` replaces the packed `track_view`; the file splits into `view.c` (the API, the map load, the
   square table and dispatch), `view_walls.c` (walls, doors, stairs, ornaments, texts) and `view_things.c`
   (objects, creatures, projectiles, explosions, fields, tracks) with `view_internal.h`. The right rows of the
   square table derive from the left rows when the stride check of the plan passes; else the rows stay with
   -1 for every unread field.
6. `bool` for the 35 truth values; `int16_t` for every pixel coordinate (`text.h`, `dialog.c`, `select.h`,
   `video.h`); `bool` for the `0/-1` inits (`ui_init`, `text_init`, `layout_init`, `anim_open`, `anim_play`,
   `logo_play`, `draw_shrink`); `Box` by pointer in `automap.h`; a cast with a comment in the `video_argb_*`
   accessors.
7. Copied code: `box_contains` (R4.6) replaces the five point-in-box loops; `ui_xor_screen_box`;
   `draw_viewport_icon` goes; `save_with_dialog()` for the DM and CSB save dialogs. The twelve exported ui
   functions without an external caller become static; the test-only exports get the marker.
8. Names and comments: `ui_get_champion_portrait`, `panel_format_object_text`, `panel_get_object_meter`,
   `get_map_center`, `get_largest_attack`, `dragged_icon_cell` (-1 none), `ViewGroup`,
   `command_close_inventory`/`command_redraw_inventory` instead of sentinels; the comments that name an
   original symbol are rewritten in game terms; the `ponytail:` markers move to `docs/todo.md` or become
   deviation comments; every header gets its purpose paragraph.
9. Bugs: `draw_panel_scroll` guards the read before the text (`ui_champion.c:792-793`); `select_item_box`
   checks `text_get_size`; `ui_extract_icon` rejects an icon below the first sheet; `audio_play_data` (R9.1)
   copies the sample, so `anim_close` and `logo_play` need no `audio_stop_all` before they free.

### R9. Audio

1. `ui/audio.h/.c` (`audio_`): the Paula voices, `audio_play_sample`, `audio_play_data`, the intro music
   segments (`audio_music_play`, `audio_music_stop`), `audio_mix`, `audio_stop_all`. It sets
   `g_sound_play_hook` at `ui_init`. `platform/platform.c` calls `audio_mix` and `music_mix`.
2. `ui/music.c` keeps the soundtrack selection; `musicmap.c` becomes `music_tables.c`, a second file of the
   music module with `music_internal.h`; the tables stay in the source as the named exception of R3.13.
   `music_play_end` takes a `bool`. `music.h` states that `music_mix` runs on the game thread.
3. `third_party/dr_mp3/dr_mp3.h` with `README.md` (version, license); `music.c` includes it as
   `<dr_mp3/dr_mp3.h>` under the existing `-Weverything` pragma.

### R10. control/

1. The protocol leaves `platform/`, one module each:
   - `json.h/.c` (`json_`): the writer and `json_key_int`, `json_key_str`, `json_key_bool`.
   - `request.h/.c` (`request_`): the flat key search, the string and number getters and the list scanner,
     with the contract comment: keys are unique per request, no `"` in strings, no nesting beyond `args`;
     the list scanner reuses the key search.
   - `state_json.h/.c` (`state_`): the state, view and map writers with verbs: `state_write_map`,
     `state_get_visible_groups`, `state_get_visible_things`.
   - `turn.h/.c` (`turn_`): the turn and batch machine, what the party saw, the news.
   - `png.h/.c` (`png_`): the screenshot writer; `png_write` checks `fwrite`.
   - `control.h/.c` (`control_`) with `actions.c` and `control_internal.h`: `control_handle_line(line, Json
     *reply)`, `control_is_turn_pending`, `control_end_turn`, the dispatch, and the action handlers with
     their table in `actions.c`.
2. The socket is platform code: `platform/transport.h/.c` (`transport_`: listen, accept, read lines, send;
   POSIX) and `platform/transport_none.c` (Windows: `transport_is_available` returns false). The Makefile
   picks the file by `ifeq ($(OS),Windows_NT)` or the `windows` target. Per frame: `app_run_frame` calls
   `transport_poll` once; `transport_poll` accepts a client, reads one line, calls `control_handle_line`
   and sends the reply; `app_run_frame` then calls `control_is_turn_pending` and `control_end_turn` as
   `control_poll` does today.
3. `bool` for every truth value (`json_bool(Json *, bool)`, the `WallFace` flags, the turn fields); the dead
   `held` parameter goes; `enum SquareLook` replaces the `strcmp` chains of `state_json.c:930-1016`;
   `DECIMAL_BASE` goes (the standard allows the base argument of `strtol`); `int teleports` becomes
   `int16_t` with `g_party_teleport_count`.
4. `docs/llm/llm-mode.md` gets a "Protocol vocabulary" section with every name of the C tables (wall
   ornaments, creatures, slots, floor ornaments, cells); a docs check in `tests/standard/` verifies that every
   table name appears there.
5. A golden protocol snapshot (`tests/golden/protocol.json`): the JSON replies of a fixed request sequence
   (new game, a few moves, a look, a wait, a save) with `g_random_seed` fixed and the save written into a temp
   folder whose path the replies do not contain. The epic takes the snapshot before the split, and the replies
   do not change.

### R11. platform/ and the application

1. `platform/main.c` is the entry point only (about 30 lines). `platform/options.h/.c` (`options_`) parses
   the command line from a table (name, has value, target), prints `--help`, stops with an error on an
   unknown option or a bad number, and checks the turn-based mode on Windows once. `platform/app.h/.c`
   (`app_`) holds one `App` struct (mode, options, loaded adventure, directories) and `app_start_dm`,
   `app_start_csb`, `app_run_frame`, `app_quit_to_start`. The `--show` and `--view` developer modes move to
   `platform/debug_view.c` behind one function; `view_key` uses `map_step`, not its own movement rules.
2. One way to choose the filter: `--filter`; `DM_FILTER` goes from the code and the docs.
3. `platform.c`: `bool` for `audio_open` and `tried`; one declaration per line. `scale.h`: the same parameter
   names in both functions. Dead includes go (`main.c:12 ftl.h`, `state_json.c:11 command.h`).
4. The standard states once that an allocation failure aborts.

### R12. Tests

1. `tests/dmtest/`, one helper module each:
   - `lib.py`: loads a private copy of `build/libdm.dylib` per test module from a temp path, so each module
     has its own C globals; the plan verifies once that a copy loads as a separate image (the ad-hoc signature
     is in the file and survives a copy) and measures the suite time before and after the change (13 s today).
   - `constants.py`: parses `#define` lines and `enum` result types of `src/**/*.h` at import and evaluates
     a value expression over integer literals, known constants, casts, shifts and arithmetic with a small
     evaluator on the Python `ast` (no `eval`); tests write `C.KEY_RETURN`. A test with a header fixture
     covers the parser: `(-1)`, `(MESSAGE_ROWS - 1)`, `((int16_t)0x8000)`, `(1 << 5)`, an enum member.
   - `game.py`: the `Game` harness and the `Box`, `Champion`, `Party`, `Session`, `Screen` structures.
   - `savefile.py`, `control.py` (`Client`, `pump`), `emu.py`.
   - `markers.py`: `needs_dm`, `needs_csb`, `needs_sdl`, `needs_ghidra`.
   No test module imports another test module; no hand-mirrored constants; no record decoding by bit shifts
   in a test.
2. The folders of R1; `pyproject.toml`: `pythonpath = ["tests"]`, `addopts = "--dist loadfile
   --import-mode=importlib"`, no `-n auto` in `addopts`; the suite command is `uv run pytest -q -n auto`
   (`make test`); `tests/csb_diff.py` becomes `tests/differential/test_csb_mini.py`.
3. No machine paths: the PNG dumps of `test_automap.py` and `test_modes.py` use `DM_TEST_PNG_DIR` or go; the
   `/Users/lego` path goes from the `csb_diff` docstring.
4. The skip conditions come from `dmtest.markers`; the function-scoped `make` of `test_automap.py:32-36` and
   the 22 redundant `make` calls go (`conftest.py` builds once).
5. `ruff` in the `dev` group with `[tool.ruff]` (line length 160); `make check` runs it over `tests/` and
   `tools/dmtools/`.
6. The `ctypes` structures of the tests mirror the C structs through `dmtest.game`; the field names follow
   every rename in the same commit.
7. A golden screen snapshot (`tests/golden/screens.json`): the status boxes, the open inventory panel, the
   action and spell areas, one scenario each, hashed as `rules.json` hashes the screen. The epic takes it
   before R8.

### R13. Docs

1. `docs/architecture.md`: the layers with a diagram, the modules of each layer in one line each, the frame
   loop and the frame hook, the tick, the control socket and the turn, the global state (what is a global and
   why), the hooks between layers, the test seams (`libdm.dylib`, `state_get_string`, `g_frame_hook`, the
   os hooks), how to add a rule.
2. `README.md`: what it is, the status in two lines, the quickstart (build, run, test), the license, the doc
   index. `docs/manual.md` takes the player manual (start screens, controls, modes, saves, CSB);
   `docs/development.md` takes the build, the tests, the layout, the standard, the release steps;
   `docs/decompiling.md` takes the Ghidra tooling.
3. The folders of R1 with every cross-link fixed; `docs/superpowers/status.md` goes; the finished plans and
   the inventories move to `docs/superpowers/archive/` without a rewrite (their line numbers are their
   evidence); the five stale spots of the survey are fixed (`llm-mode.md:250-253`, `game-state.md:196`, the
   README status and limitations, the `csb-port-integration.md` status line, the `symbols/range-4-6.md`
   link); each reference page says in its first line that it uses the original KAOS names.
4. `CLAUDE.md` of the project: the commands (`make`, `make test`, `uv run pytest -q tests/<folder>/test_x.py`),
   the layout, the layers; `.claude/agents/*.md` doc paths follow.
5. `docs/todo.md`: the items that this refactor closes go; the `ponytail:` markers that stay open come in.
6. `CHANGELOG.md`: one entry for the refactor under Unreleased.

### R14. Repository files

1. `LICENSE`: GPL-3.0; every source file of `src/`, `tests/` and `tools/dmtools` starts with
   `SPDX-License-Identifier: GPL-3.0-or-later`;
   `README.md` says the code is GPL and the game data is not included.
2. `.editorconfig`: 4-space C and Python, LF, final newline.
3. `run.txt` goes; `/ghidra/` goes from `.gitignore`; the personal play folder is `playthroughs/local/`
   (`make play`); `README.md` names no person.
4. `mcp` becomes an optional dependency (`uv sync --extra mcp`); `tools/csb_match.py` joins the package as
   `dm-csb-match` with its paths as arguments.

## Invariants

- Game behavior: the full suite passes; the differential tests and the golden snapshots prove it.
- Saves: the golden save snapshot (`tests/golden/save.json`) stays equal; after R5.8 the test goes without
  `scrub_free_groups` and the snapshot stays equal.
- Dungeon view: the golden view snapshot (`tests/golden/view.json`) stays equal.
- Screens: the golden screen snapshot (R12.7) stays equal.
- ui rules: the golden rules snapshot (`tests/golden/rules.json`) stays equal; every rule that moves in R8.3
  gets its scenario before the move.
- Protocol: the golden protocol snapshot (R10.5) stays equal; the names of the state JSON and the requests do
  not change.
- Struct layouts: `Party`, `TimelineEvent`, `ActiveGroup` keep their `sizeof` and field offsets; `Champion`
  keeps the offsets of its saved fields and appends the runtime fields after them; the golden save proves the
  packed form. The save file layout does not change.
- Builds: macOS and Windows build with 0 warnings under the flags of R2.2.

## Acceptance (gherkin)

```gherkin
Feature: Final refactor

  Scenario: Layout
    Given the branch after the last epic
    Then src/, third_party/, tools/dmtools, tools/research, tests/dmtest, docs/architecture.md and LICENSE exist
    And port/ does not exist
    And "grep -r port/src docs README.md CLAUDE.md .claude" finds nothing outside docs/superpowers/

  Scenario: Build
    When I run "make" twice, then touch src/engine/random.c and run "make" again
    Then the first build has 0 warnings
    And the third build compiles one file and links
    When I run "make windows"
    Then the build has 0 warnings

  Scenario: Standard
    When I run "uv run pytest -q tests/standard"
    Then all checks pass
    And tests/standard/allowlist.txt has no line with "open:"
    And every line of the allowlist names an exception that docs/coding-standard.md documents

  Scenario: Same behavior
    When I run "make test"
    Then all tests pass
    And the golden save, view, screens, rules and protocol snapshots are unchanged

  Scenario: Constants
    When I search src/ for an anonymous enum ("^\s*enum\s*{", not after "typedef")
    Then nothing is found
    And every enum left is a named typedef enum that Task 25 keeps
    And every constant block is hex in every line or decimal in every line

  Scenario: Globals
    When I search src/ for "^extern" declarations whose name does not start with "g_"
    Then nothing is found
    When I search src/ for "g_move_result_", "g_current_group_", "g_creature_damage_outcome", "g_sensor_rotation_effect", "_ordinal"
    Then nothing is found

  Scenario: Records
    When I search src/ outside engine/record.h and engine/record.c for "REC(" and "rec_" definitions
    Then nothing is found

  Scenario: Layers
    When I search src/ui, src/engine, src/data for "SDL", "<sys/", "<unistd.h>", "fprintf(stderr", "exit("
    Then nothing is found
    And src/control includes no platform header

  Scenario: Tests
    When I search tests/ for "from test_", "/private/tmp", "/Users/"
    Then nothing is found
    And no test file compiles the library with clang

  Scenario: Dead API
    When the dead-API check runs
    Then every exported function has a caller outside its file or the test marker

  Scenario: Docs
    When I open docs/architecture.md, docs/manual.md, docs/development.md
    Then each exists and every link in docs/ resolves
    And docs/superpowers/status.md does not exist
```

## Decisions

- Repository: `port/src/` becomes `src/`, the Makefile moves to the root (user, 2026-10-16).
- Depth: level 3, all of R5 to R11 including the move chain, the `Session` and `Screen` structs and the view
  placement context (user, 2026-10-16).
- License: GPL-3.0, SPDX identifier `GPL-3.0-or-later` (user, 2026-10-16).
- No CI (user, 2026-10-16); `make check` is the local gate.
- Includes: explicit and sorted, no umbrella header per layer; the module splits bring the largest include
  lists from 35 to about 10-15 (user, 2026-10-16).
- Constants are `#define` with written values; the notation is per block; function names have a length
  limit; files follow the header order (user, 2026-10-16).
- The coding standard v2 is written before the plan (user, 2026-10-16); the plan takes its Global Constraints
  from it, and the standard describes the target state until the run ends.
- Build: GNU make stays (no MSVC goal); `pkg-config` is not installed on the owner's machine, so the SDL
  prefix stays a lazily evaluated, overridable `brew --prefix` variable.
- A fifth layer `control/` holds the protocol; the socket stays in `platform/` as `transport`, because the
  standard keeps POSIX headers out of every other layer.
- The application stays in `platform/` (`app.c`, `options.c`, `main.c`): no sixth layer for three files.
- The `--show` and `--view` developer modes stay, behind one function in `platform/debug_view.c`.
- The audio mixer is `ui/audio.c`: audio output is presentation, like `ui/video.c`; the engine plays through
  one hook, as it draws through redraw flags and ends the game through the endgame hook.
- Record accessors keep the `rec_` prefix: one prefix for one header, as the prefix rule asks; `door_state`
  becomes `rec_door_state`.
- `party_` is an engine prefix (`party.h`: the Party struct, position, scents, light); the ui module of the
  champion display splits into `status.c` and `panel.c`.
- Public function comments live in the header; static function comments above the definition.
- The soundtrack tables stay in the source as a documented exception: they are not in the game files, and a
  run-time table file would need a parser for one feature.
- Names: `Location {x, y}` for a square of the current map and `Position {map, x, y}` for a square with its
  map, because `square` is the name of the square byte everywhere. `MoveResult` is the result enum;
  `MoveDestination` is the struct that replaces `g_move_result_*`.
- Sound 14 keeps the name `SOUND_BUZZ` (the original name); `SOUND_TELEPORT` and `SOUND_FUSE` go.
- The shared text print of `sensor.c` and `timeline_process.c` is `message_print_dungeon_text` in
  `message.c`; the side-of-cell helper belongs to `map.h` under `direction_`.
- The protocol vocabulary check and the standard checks are docs checks in `tests/standard/`; they are repo
  checks, not game tests.
- `tests/` folders by what a test proves (differential, port, golden, tools, standard), because that is the
  test list of CLAUDE.md and the shape of a future CI matrix. `ruff` checks the Python of the tests and the
  tools.
- The `-n auto` flag leaves `addopts` as the python-uv skill asks; `make test` and CLAUDE.md carry it.
- Epic order: the golden protocol, screen and rule snapshots first, on unchanged code; then R1, R2 and R12.1
  (the private copy loader before the tests stop compiling); R3; R4; R5 to R7 (R5.2 before R10); R8 and R9;
  R10 and R11; the rest of R12; R13 and R14 last.
- Git tags for the past releases: out of scope; the CHANGELOG carries the history.

- Rulings of the run (2026-10-17), each kept by the final review:
  - `set_direction_*` of the group turn and the last party-blocked movement result stay file-static in
    `group.c`: the original keeps them across calls (the result is read after a skipped test); a `GroupTurn`
    field would lose that memory.
  - Rule 3.12 also allows the fixed table of a standard algorithm (the CRC-32 table of `data/crc32.c`).
  - Rule 4.8 "a game record stores it" covers the stored attack sound ordinal of the KAOS creature table
    (the owner confirmed it, 2026-10-17).
  - Rule 1.7: the entry point `src/platform/main.c` has no header; the module check names it as the one
    exception. The second file of a module is `<module>_<x>.c` (`control/control_actions.c`).
  - The notation check does not read `WIDTH`, `TICKS` and similar words as a decimal kind in a name with an
    address first part (`KAOS_*`): such a name holds the address of the field.
  - `layout_output` and `map_mouse` are exported from `platform/scale.c` (in the library), so the mouse
    mapping test keeps calling them.
  - The square table of the view marks every unread field; the right rows do not mirror the left ones
    (buttons, door frames, D2 flags differ). `view.c` keeps the 480-line square table and has 1448 lines.
  - Parameter structs of `ui/`: `draw_blit` takes a `BlitSource`, `text_draw` a `Point`, `layout_get_zone` a
    `ZoneItem` in and a `ZoneClip` out.
  - `transport_poll` does not return early after a turn frame. When the client drops during a batch, the
    batch runs one action fewer than before; the golden protocol is unchanged.
  - `sound_request_play` keeps the missing "current map == party map" check for mode >= 1 as a recorded
    deviation (`docs/formats/dungeon.md`, `docs/todo.md`); this refactor changes no game rule.
  - `tests/standard/allowlist.txt` has no `merge=union` driver: with no parallel streams left, the union merge
    only brought back deleted lines.
  - `docs/superpowers/` is history: the run does not edit the plan, the PRD inventory or the archive.

## Known consequences

- Every path in `docs/`, `CLAUDE.md`, `.claude/agents/*.md`, the tools and the tests changes; `git blame`
  survives moves (`git mv`); the include-sort commit and the SPDX commit join `.git-blame-ignore-revs` beside
  the format commit, and open worktrees rebase over them.
- The ctypes tests change with every rename and every struct (`Session`, `Screen`, `Timeline`); the shared
  helpers of R12 make each change one edit.
- The run touches rule paths in R5.4 (the move chain) and R8.3 (rules out of ui); the differential and golden
  tests are the gate, and a difference stops the epic for the user.
- The personal play folder moves from `playthroughs/lego/` to `playthroughs/local/` on the owner's machine.
- `playthroughs/`, the LLM agents and `dm-play` keep working; the protocol does not change.

## Out of scope

- Behavior changes, bug fixes of the game rules, new features.
- MSVC, CI, git tags for past releases.
- The research scripts in `tools/research/` beyond the move and the README.
- A run-time format for the soundtrack tables.
