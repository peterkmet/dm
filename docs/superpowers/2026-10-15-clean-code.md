# Clean C code: PRD

Date: 2026-10-15. Branch: `feature/2026-10-15-clean-code`. Status: reviewed.

## Goal

The C port in `port/src/` reads like C that a person wrote: readable, maintainable, one style. It builds on macOS (clang) and on Windows (MinGW-w64 gcc) with 0 warnings. The game behavior does not change.

The rules come from the sister repo `dm2` (`../dm2/docs/coding-standard.md`) and from a review of the full `port/src/` on 2026-10-15.

## Current state

- `tests/test_standard.py` passes. It checks hex and decimal literals, decompiler names, raw record access, duplicate constant names, header prefixes, predicates in headers, function length, and layers. `docs/coding-standard.md` describes only part of these checks.
- About 160 `#define`s of KAOS image addresses (`0x10xxxx`) are spread over 20 files. They use three name schemes (`KAOS_`, `G_`, no prefix). Nine addresses have two names, for example `0x10192e` is `CREATURE_INFO` (`data/tables.c:7`) and `KAOS_CREATURE_INFO` (`engine/map.c:14`). Engine and ui call `kaos_u8/u16/s16/ptr` about 130 times with address arithmetic.
- Other values also have two names, for example `SLOT_ACTION_HAND` (`engine/group.c:35`) and `SLOT_HAND_ACTION` (`engine/game.h:23`), `CELL_CENTERED` (`engine/projectile.h:25`) and `CELLS_CENTERED` (`engine/group.h:50`). `ZONE_*` and `GFX_*` ids are defined locally in `ui/dialog.c`, `ui/startend.c`, `ui/command.c`, `ui/gameloop.c`.
- Number notation is mixed: about 40 masks are decimal (`FEAR_RANDOM_MASK = 15`), sizes and counts are hex (`CHAMPION_RECORD_SIZE 0x140`, `SAVE_CHAMPIONS_BYTES 0x580`).
- About 26 shifts by 8 at call sites unpack or pack fields (`engine/group.c:986`, `engine/timeline_process.c:470`). About 60 `& 3` direction wraps do not use `NORMALIZE()` (`engine/game.h`).
- Dense code: about 30 comma-operator statements (`return SDL_Log(...), 0`), about 250 lines with more than one statement, lines up to 200 columns, two brace styles for functions, macros with side effects (`NEXT()`, `PUT()` in `data/graphics.c:77-78`).
- About 36 truth values are `int` or `int16_t`; static predicates in `.c` files do not return `bool`.
- Names: noun-only functions in `ui/view.c` (`wall()`, `object()`, `d3l()` … `d0c()`), file-static variables `vp`, `mw`, `pals` (`ui/view.c:196-205`), fields `event73_count_thieves_eye`, `event79_count_footprints`, `event71_count_invisibility` (`engine/game.h:86-91`), `H_BYTE_12A` (`engine/savefile.c:14`), `TimelineEvent` fields `b` and `c` whose meaning depends on the event type (`engine/game.h:94-100`). About 90 comments only name an original symbol (`/* g_viewport_bitmap */`).
- Game rules in `ui/`: fountain and drinking, the vi altar, door buttons (`ui/command.c:441-511`), rune mana cost (`ui/command.c:546-560`), light and torch burn (`ui/gameloop.c:180-215, 396-405`). `ui/` writes about 40 engine globals.
- `ui/view.c:1096-1468`: 16 square drawers `d3l` … `d0c`; the left and right functions differ only in ids.
- Portability:
  - Platform code outside `platform/`: `execinfo` backtrace in `engine/map.c:5,171`, `mkdir`/`_mkdir` and `strings.h` in `data/adf.c:6-13`, `dirent` and `strcasecmp` in `ui/anim.c:1,5`.
  - The Windows build needs `-Wno-format-truncation -Wno-implicit-fallthrough -Wno-unknown-pragmas` (`port/Makefile`).
  - `long` (32 bits on Windows) holds the game time and other values in `platform/state_json.c:81,126`, `platform/control.c:224,763,1112` and `platform/main.c:464,508`.
  - `data/adf.c:141` accepts Amiga file names with `\ : * ? " < > |`, which escape the target folder or make NTFS streams on Windows.
  - The menu offers the turn-based mode on Windows, but `platform/control_win.c` is a stub, so the game exits.
- Small items: the array length is written in about 12 ways (`COUNT` in `data/csbtext.c:81`, `GET` in `data/tables.c:170`, `sizeof a / sizeof a[0]` in 10 places); two spell APIs (`spell_get` in `data/spells.c`, used by `tests/test_spells.py:29`, and `tables_spell`); `data/csbimage.c:158` makes a pointer outside its buffer (`data - KAOS_BASE`, undefined behavior); `data/csbtext.c:153` writes bytes by hand instead of `WR16`; `read_file` (`data/ftl.c:56`) has no module prefix.

## Requirements

### R1. Standard and checks

`docs/coding-standard.md` gets these rules, and `tests/test_standard.py` checks each one. Open violations go into `tests/standard_allowlist.txt` with the reason `open: module cleanup`; the epic that fixes them removes the lines.

1. Notation: hex for masks, bit flags, packed layouts, image addresses and file-format bytes. Decimal for counts, sizes, times in ticks, coordinates, distances, divisors and thresholds. Check: the check splits a constant name at `_` into parts. A name whose last part is `MASK`, `FLAG`, `FLAGS` or `BIT` and whose value is 10 or more must be hex; this rule wins. Else a name with a part `COUNT`, `SIZE`, `BYTES`, `TICKS`, `MIN`, `MAX`, `LIMIT`, `THRESHOLD`, `WIDTH` or `HEIGHT` must be decimal.
2. Packed fields (dm2 rule 10): no shift by 8, 16 or 24 and no double integer cast in a `.c` file; a shift by a `*_SHIFT` constant only in the module that owns it.
3. Byte order: `BE16`, `WR16`, `BE32`, `WR32` are defined only in `data/bytes.h`; no module writes the bytes of a word by hand.
4. KAOS image: no `kaos_*` call and no image address outside `data/`. Each address has one name with the prefix `KAOS_`.
5. Predicates return `bool`, also static functions in `.c` files.
6. No event numbers and no byte offsets in names of functions, struct fields and constants. Check patterns: `events?_?\d` (event numbers, any case) and `(?:^|_)(?:BYTE|OFFSET|AT)_` followed by 2 or more hex characters with at least one digit (byte offsets, so `CREATURE_AT_ASPECT_INDEX` passes). Other names go into the allowlist with a reason.
7. `ui/` does not write a variable that an `engine/` header declares, except through engine functions. Check: `g_<name>` of an engine header, then optional `[...]`, `.field` or `->field` parts, then `=`, `+=`, `-=`, `|=`, `&=`, `++` or `--`; also `++`/`--` before the name. Exceptions: variables whose name ends in `_hook` (rule 8 of the standard), and redraw-flag writes (`attributes |= CHAMPION_ATTR_*`, `attributes &= ~CHAMPION_ATTR_*`, `g_redraw |= ...`), because ui consumes and requests its own redraws. Writes through a pointer are not checked; the task review checks them.
8. Platform code: `#ifdef _WIN32`, `#include <unistd.h>`, `<sys/...>`, `<dirent.h>`, `<strings.h>`, `<execinfo.h>` only in `platform/`.
9. Formatting: see R5; the format check starts in the R5 epic, not in this one.
10. Casts: a cast only where it changes the result (wrap, sign, truncation), with a short comment when the reason is not obvious. Not checked automatically except double casts (item 2); the task review checks it.

### R2. KAOS tables through accessors

All image addresses move to `data/kaos_addresses.h` with the prefix `KAOS_`; only `data/` files include it. `data/tables.c` loads the image (`tables_load`: `kaos_load`, then `tables_init`), so no `kaos_*` call stays outside `data/`. `data/tables.h` gives typed functions in game terms, for example `tables_step_east(direction)`, `tables_light_amount(power)`, `tables_palette(index, out)`. Engine and ui call only these functions. Values that `tables.c` already caches (`tables_creature_info()`) replace raw reads (`engine/map.c:261`). The writes into the image (`ui/startend.c:235-237`, click boxes) get a `tables_set_*` function. No address has a name with the part `TICKS` or `SIZE`. The APPB copy tables of `data/csbimage.c` and `data/csbtext.c` stay in their files; their KAOS columns use the `KAOS_*` names of `data/kaos_addresses.h`.

### R3. One value, one name, one owner

Each constant has one name and one definition in the header of the module that owns it. Graphic, zone and layout ids live in `data/ids.h`; square bits in `engine/map.h`; skills in `data/tables.h`; slots in `engine/game.h`. Duplicates (same meaning, other name) merge into one.

### R4. Mac and Windows

1. Platform services of lower layers:
   - Make a folder and list a folder: hooks in `data/` that `platform/` sets with `SDL_CreateDirectory` and `SDL_EnumerateDirectory` (`platform/main.c:352` uses SDL enumeration already).
   - Compare names without case: a short function in `data/`.
   - The square list guard in `engine/map.c` keeps its log line; the epic removes the backtrace.
2. The Windows build has no `-Wno-*` flags and 0 warnings: fix the fall-through comment (`engine/action.c:433`), put the clang pragmas of `ui/music.c:9-13` under `#ifdef __clang__`, size the `snprintf` buffers or check their result (`ui/anim.c:106`, `ui/automap.c:155`, `ui/gameloop.c:160`, `ui/ui_champion.c:548`).
3. No `long`: 32-bit values print as `uint32_t` with `PRIu32`, wider values as `int64_t` with `PRId64`; `strtol` results go into `int64_t` (`platform/state_json.c`, `platform/control.c`, `platform/main.c`).
4. ADF extraction rejects names with `\ : * ? " < > |` and control characters, as it rejects `/` today: the extraction stops with an error.
5. On Windows the mode menu does not offer the turn-based mode. `--mode turnbased` prints that the mode needs macOS and exits with an error code.

### R5. Formatting

1. `.clang-format` in the repo root: `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 120`, `BreakBeforeBraces: Linux` (function braces on their own line, control braces on the same line), `AllowShortIfStatementsOnASingleLine: Never`, `AllowShortFunctionsOnASingleLine: None`, `AllowShortCaseLabelsOnASingleLine: false`, `SortIncludes: Never`. The plan tunes options that the epic needs to keep the code readable (for example aligned constant tables); `docs/coding-standard.md` names the file.
2. One commit formats all of `port/src/` (vendored `ui/dr_mp3.h` excluded); `.git-blame-ignore-revs` lists it.
3. `tests/test_standard.py` runs `clang-format --dry-run --Werror` over `port/src/`. `clang-format` 23 is a test prerequisite (`brew install clang-format`), named in `README.md`. The check fails, and does not skip, when `clang-format` is missing or its major version is not 23; the failure message names the install command.

### R6. Packed fields, directions, truth values, casts

1. Each packed field has accessors in the header of its owner. `TimelineEvent` keeps its binary layout; each event type gets named accessors for `b` and `c` (for example the quiver slot of a throw, the aspect delay of a creature event).
2. `engine/game.h` gets `static inline int16_t turn_right(int16_t direction)`, `turn_left` and `turn_back`, each `NORMALIZE(direction + n)`. Direction and cell wraps use them or `NORMALIZE()`.
3. Every truth value and predicate is `bool`. Flags in game records keep their stored type.
4. The epic removes casts that change nothing.

### R7. Names and comments

1. Functions are `<prefix><verb>_<object>`: `ui/view.c` noun functions get verbs. File-static variables get descriptive names without `g_` (`viewport`, not `vp`), as the other 144 file-static variables of `port/src/`; global variables keep `g_`.
2. Struct fields and constants get game names: `event73_count_thieves_eye`, `event79_count_footprints`, `event71_count_invisibility`, `H_BYTE_12A` and similar. Binary layouts stay. The `ctypes` tests follow the new names.
3. A comment that only names an original symbol becomes a game-terms comment; the original name goes into `docs/kaos-symbols.csv` if it is not there.
4. Short names stay only for loop indexes and for a variable used in a few adjacent lines.
5. Each rename updates `docs/` in the same commit (grep `docs/` for the old name), as the c-decomp-port skill asks.

### R8. Game rules out of ui

The fountain, drinking, the vi altar, door buttons, rune mana cost, light amount and torch burn move into `engine/` functions. `ui/` translates a click or a tick into one engine call and draws from flags. The check of R1.7 has no open allowlist lines at the end; an exception that stays needs a reason, as for the action area of dm2.

### R9. Square drawing from a table

The 16 square drawers of `ui/view.c` become one table of square descriptors (relative position, side, the ids per element) and one draw function. The drawing order stays the same.

### R10. Small cleanups

1. One `ARRAY_COUNT(a)` in `data/bytes.h` replaces `COUNT`, `GET` and `sizeof a / sizeof a[0]`.
2. One spell API: the plan keeps the API with more callers (`tables_spell`); the epic removes the other and moves its tests to the same values through the kept API.
3. `data/csbimage.c` gets an address helper with no pointer outside the buffer.
4. `NEXT()` and `PUT()` become a small reader and writer like `Nibbles` in `data/ftl.c`.
5. New `data/file.c` and `data/file.h` (prefix `file_`): `file_read` (was `read_file` in `data/ftl.c`) and `file_exists` (replaces the open-and-close tests in `data/adventure.c`).
6. The adventure tables in `data/adventure.c` and `platform/main.c` become one struct per adventure.

## Invariants

- Game behavior: the full suite passes; the differential tests and `tests/test_view.py` prove it.
- Saves: a fixed game (fixed seed, one recruit, one held object) saves the same decoded file on `main` and on the branch. A golden snapshot (SHA-256 per save part, taken on `main`) proves it before the first change.
- Dungeon view: every square of every map in all 4 directions draws the same pixels; a golden snapshot (CRC-32 per map row, taken on `main`) proves it.
- ui rules: the rules that move to `engine/` give the same game state and screen; golden snapshots taken before the move prove it.
- Struct layouts: each `ctypes.Structure` in `tests/` has the same `sizeof` and field offsets as on `main`; field names may change (R7.2).
- The LLM protocol names in `platform/state_json.c` and `platform/control.c` stay.

## Acceptance (gherkin)

```gherkin
Feature: Clean C code

  Scenario: Standard check
    Given the branch after the last epic
    When I run "uv run pytest -q tests/test_standard.py"
    Then all checks pass, the format check included
    And tests/standard_allowlist.txt has no line with "open: module cleanup"

  Scenario: Same behavior
    When I run the full suite "uv run pytest -q"
    Then all tests pass
    And no test assertion was removed or loosened unless the plan lists it as approved

  Scenario: Save compatibility
    Given the golden save snapshot taken on main
    When the branch build saves the same fixed game
    Then each decoded save part has the same SHA-256

  Scenario: Mac build
    When I run "make -C port"
    Then the build has 0 warnings

  Scenario: Windows build
    When I run "make -C port windows"
    Then WIN_CFLAGS has no -Wno- flag
    And the build has 0 warnings

  Scenario: No KAOS addresses outside data
    When I search port/src/engine, port/src/ui and port/src/platform for "kaos_" and "0x10[0-9a-f]{4}"
    Then nothing is found

  Scenario: Old names gone
    When I search port/src for "SLOT_ACTION_HAND", "CELLS_CENTERED", "KAOS_CREATURE_INFO 0x", "event7[0-9]_", "H_BYTE_12A", "d3l(", "NEXT()", "read_file("
    Then nothing is found

  Scenario: One square drawer
    When I search port/src/ui/view.c for the functions d3l to d0c
    Then nothing is found
    And one function draws a square from its descriptor

  Scenario: Turn-based mode on Windows
    Given dm.exe on Windows
    When I open the mode menu
    Then the turn-based mode is not offered
    When I start "dm.exe --mode turnbased"
    Then the game prints that the mode needs macOS and exits with an error code

  Scenario: Unsafe ADF names
    Given an ADF image with a file named "a\b" or "a:b"
    When the game extracts the disk images
    Then the extraction stops with an error and writes no file outside the target folder
```

## Decisions

- KAOS addresses: typed accessors in `data/tables` (user, 2026-10-15).
- Turn-based mode on Windows: hidden; no TCP or AF_UNIX transport (user, 2026-10-15).
- Formatting: `.clang-format`, one format commit, a check in `test_standard.py` (user, 2026-10-15).
- Scope: all epics, including rules out of ui, the square table and the field renames (user, 2026-10-15).
- No change of game behavior; see Invariants.
- The notation rule of dm2 applies as written; the check uses name parts because a value alone does not show its kind.
- `TimelineEvent` keeps `b` and `c` as storage and gets accessors per event type: a union would change many call sites without gain and risks the save layout.
- A comment that only names an original symbol: the epic rewrites it in game terms and does not remove it, because CLAUDE.md asks to keep comments and the user asks for clean code; `docs/kaos-symbols.csv` keeps the original name.
- File-static variables have no `g_`: 144 of 144 file-static variables have no `g_` today.
- Folder services through SDL hooks: SDL already runs on both systems, and C11 has no portable folder API.
- `.clang-format` base: LLVM with Linux braces, because most functions have the brace on its own line and most control statements on the same line. clang-format 23 is the stable Homebrew version on 2026-10-15.
- Epic order: the golden save and view snapshots first (on unchanged code), then R5 (format and its check), so every later diff is formatted; then R1 (rules, checks, allowlist), R3, R2, R4, R6, R10, R7, the golden rule snapshots, R8, R9. R8 and R9 come last: they move the most code and need the earlier names.
- New KAOS accessors read the image at call time, so they give the same value as the old raw read and need no `tables_init`; call sites that move onto an existing cached accessor get `tables_init` in their test fixtures.
- SWSH logo addresses (`ui/logo.c`) move into a new `data/swsh.c`; address names have the prefix of their image: `KAOS_`, `CSB_` (APPB) or `SWSH_`.
- Rules without a test (`ui.md` section 2) get golden snapshots before the move, not differential tests: a snapshot of the current behavior is a reference check and needs no emulator setup per rule.
- Inventory ordinal, magic caster, action list: engine setters (`inventory_set_open_champion`, `magic_set_caster`, `action_set_action_list`), so R1.7 needs no exception.
- New tests only where the test list of CLAUDE.md allows them: the checks of `test_standard.py`, the golden snapshots of saves, the dungeon view and the ui rules (reference checks), the ADF name rule (a parser rule), and a differential test for a rule that moves to `engine/` when no existing test covers it.

## Known consequences

- `git blame` skips the format commit only with `git config blame.ignoreRevsFile .git-blame-ignore-revs` on each clone; `README.md` names the command.
- Each open branch and worktree must rebase over the format commit.
- The test suite fails on a machine without `clang-format` 23.
- `tests/test_spells.py` changes when the spell API without callers goes away (R10.2).
- Renames touch `docs/` (for example `docs/game-state.md`, `docs/symbols/range-7-9.md`).

## Out of scope

- Behavior changes, bug fixes of the game, new features.
- MSVC support (MinGW only).
- The LLM control transport on Windows.
- Python tools in `tools/`.
