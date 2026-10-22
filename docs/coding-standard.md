# Coding standard

The port is a readable reimplementation of the game. It is not a cleaned-up decompiler listing. The tests
prove that the behavior matches the original.

Every rule names its check in `tests/standard/test_standard.py` in brackets, or says "review only" when no
mechanical check is possible, or names its exception. `tests/standard/allowlist.txt` holds the accepted
exceptions, one per line: `<check> <target> <reason>`. A reason is `rule <n.m>: <text>` for an exception that
this document names.

## 1. Layers and modules

1. The source is `src/` with five layers in dependency order: `data/`, `engine/`, `ui/`, `control/`,
   `platform/` ([Architecture](architecture.md#layers) gives the content of each). A file includes only headers
   of its own layer and of the layers before it. [layer]
2. The public headers of a layer are in `src/<layer>/include/` (`src/engine/include/game.h`); a private
   `<module>_internal.h` is next to its `.c` files. The include path of a layer holds only the `include/`
   folders of its own layer and of the layers before it, and `-isystem src/third_party`, so the compiler rejects
   an include of a later layer. An include names only the file: `#include "game.h"`. Header names are unique
   across the layers. [layer]
3. Only `platform/` includes SDL and operating system headers (`_WIN32`, POSIX, `<sys/...>`, `<unistd.h>`,
   `<dirent.h>`). The other layers reach the operating system through the hooks of `data/os.h`, which
   `platform/` and the tests set. [platform]
4. Game rules in `engine/` do not call the user interface. They set redraw flags (`g_redraw`,
   `CHAMPION_ATTR_*`), and they call a hook that `ui/` sets for an event that must run the user interface
   inside the rule (`g_endgame_sensor_hook`, `g_sound_play_hook`). A NULL hook skips the call. [layer]
5. `ui/` changes game state only through engine functions. Exceptions: redraw flags and hooks. [engine-write]
6. `control/` is a client of `ui/` and `engine/`; it holds no socket. The socket is `platform/transport.c`.
   [platform]
7. A module is one `.c` file with one public header. A module with several `.c` files has one public header
   and one `<module>_internal.h` for the shared private declarations; both carry the prefix of the module.
   The entry point `platform/main.c` is the one exception. It has no header. [module]
8. Each header states its prefixes in one comment line, `/* Prefixes: rec_ */`. A prefix belongs to one
   module; no prefix starts with the prefix of another module. [prefix]
9. `src/third_party/` is vendored code, the one folder of `src/` outside the layers. The folder is outside every check,
   including the format check. [layer]

## 2. Headers and includes

1. A header starts with the SPDX line, the guard `DM_<LAYER>_<FILE>_H`, the prefix line, one purpose
   paragraph (what the module owns and what it must not do), then the includes: [header]

   ```c
   /* SPDX-License-Identifier: GPL-3.0-or-later */
   #ifndef DM_ENGINE_RECORD_H
   #define DM_ENGINE_RECORD_H
   /* Prefixes: rec_ */

   /* The byte layout of the dungeon records: one accessor per field, no record byte outside this module. */

   #include <stdbool.h>
   #include <stdint.h>
   #include "dungeon.h"
   ```

2. A header includes only what its declarations need; a struct that appears only behind a pointer is
   forward-declared. [include]
3. A `.c` file includes exactly the headers it uses, in this order: its own header; project headers by layer
   order and alphabetical inside a layer; third-party headers; system headers. `.clang-format` sorts them.
   The include categories of `.clang-format` name the headers of each layer. There is no umbrella header
   per layer. [format, include]
4. The one-line comment of a public function is in the header above its prototype. [comment]
5. A symbol that exists only for the tests carries `/* exported for the tests */` on its declaration.
   [dead-api]

## 3. Constants

1. Name every game value. Plain numbers are allowed only for 0, 1, -1, and for 2 when the 2 halves or doubles a
   value. Every other number is a named constant, also in an array size and in a loop bound. The base
   argument of `strtol` and its siblings is a plain number.
   [hex, decimal]
2. A constant is `#define NAME value` with the value written out; a negative value is in parentheses:
   `#define COLOR_NONE (-1)`. No `enum` for constants; `enum` names a result type only (rule 6.1). [enum]
3. Related constants are in one block without blank lines, with one comment line above the block that
   names the set; `clang-format` aligns the values. [block]
4. Notation is per block: a block of bit flags, masks, packed layouts, image addresses and file format bytes
   is hex in every line, with the same digit width; a block of counts, ids, sizes, ticks, coordinates,
   divisors and thresholds is decimal in every line. A block never mixes the two. [notation]

   ```c
   /* Wound flags of Champion.wounds, one bit per body part. */
   #define WOUND_READY_HAND  0x01
   #define WOUND_ACTION_HAND 0x02
   #define WOUND_HEAD        0x04
   #define WOUND_TORSO       0x08
   #define WOUND_LEGS        0x10
   #define WOUND_FEET        0x20

   /* Square types, bits 5–7 of the square byte. */
   #define SQUARE_WALL     0
   #define SQUARE_CORRIDOR 1
   #define SQUARE_PIT      2
   ```

5. Each constant has one definition, in the header of the module that owns it; other modules include that
   header. [duplicate]
6. Image addresses: `KAOS_` in `data/kaos_addresses.h`, which only `data/` includes; `CSB_` for the APPB
   image; `SWSH_` for `SWSH.FTL`; `VDEO_` for `VDEO.FTL`. Each address has one name. Other layers call
   `tables_get_*`. [kaos, address]
7. Graphic, zone, layout, icon and string numbers live in `data/ids.h`, grouped by kind (`GFX_`, `ZONE_`,
   `LAYOUT_`, `ICON_`, `TEXT_`), sorted by value, one name per value. [duplicate; one name per value: review
   only]
8. No byte offset, event number or original address in a name. [numbered]
9. A packed field has one accessor in the module that owns it. No shift by 8, 16 or 24 at a call site, and no
   shift by a `*_SHIFT` constant of another module. [packed]
10. The byte order helpers `be16`, `wr16`, `be32`, `wr32` are `static inline` functions in `data/bytes.h`
    only. [byteorder]
11. The record layout belongs to `engine/record.h` and `engine/record.c`. No other file indexes a record
    (`REC(`) or `g_dungeon_map_data[`. [record]
12. Read game data at run time; do not copy a game table into the source. Four tables stay in the source: the soundtrack tables of the other versions (not in the game files),
    the APPB address tables of the CSB image build (`data/csb_image.c`, `data/csb_text.c`), the CRC-32 table of a
    standard algorithm (`data/crc32.c`), and the spell names of the cheat screen (`ui/cheats.c`: the game files
    have no spell names; the potion names are rune names, not effects). The first three are named in the allowlist with this
    reason; the spell names need no allowlist line, because the table has no plain numbers. [decimal, hex, allowlist]

## 4. Naming

1. Functions: `<prefix><verb>_<object>` in `snake_case`, at most four words after the prefix, at most 32
   characters. [name]
2. A static function has no module prefix. [static-prefix]
3. Record accessors are nouns after the prefix: `rec_<type>_<field>` and `rec_<type>_set_<field>`. Table
   accessors are `tables_get_<table>`. [prefix]
4. Predicates (`is_`, `has_`, `can_`), also static ones, return `bool`. [bool]
5. Globals have the prefix `g_`. File-static variables have no `g_` and a descriptive name. A short name is
   allowed only for a loop index and for a variable used in a few adjacent lines. [global]
6. Types: `PascalCase`. Constants: `UPPER_CASE`. [review only]
7. No decompiler names (`uVar1`, `local_10`, `arg_8`, `param_1`). [decompiler]
8. No ordinal (index + 1, 0 for none) in a name or in a variable; an index is -1 for none. An ordinal stays
   only where a game record stores it. [ordinal]
9. A name says the game task, not an event number, a byte offset or an original address:
   `group_process_reaction_event`, not `group_process_events_29_to_41`. [numbered]
10. One term per concept: a `location` is a square of the current map (`x`, `y`); a `position` is a square
    with its map (`map`, `x`, `y`); a `square` is the square byte; a `cell` is one of the four cells of a
    square; a `thing` is a record reference; a `champion_index` is 0–3, -1 for none. [review only]

## 5. Types and values

1. One value has one type on its whole path (field, parameter, local, return value). The type of each game
   value kind: [type]

   | Value kind | Type | None |
   |---|---|---|
   | `champion_index`, `cell`, `direction`, `map`, `slot`, `x`, `y` | `int16_t` | -1 |
   | pixel coordinates and sizes | `int16_t` | |
   | `thing` | `uint16_t` | `THING_NONE` |
   | `ticks` | `uint16_t` | |
   | game time | `uint32_t` | |
   | C sizes, C array counts and indices, SDL values | `int` or `size_t` | |

2. Every truth value is `bool`: predicates, locals, parameters, return values. A flag in a game record or a
   save file keeps its stored type. [bool]
3. No `long`, because it has 32 bits on Windows and 64 bits on macOS. A value wider than `int` uses a fixed-width
   type with the `<inttypes.h>` formats. [review only]
4. A cast is allowed only where it changes the result (16-bit wrap, sign, truncation), with a short comment
   when the reason is not obvious. No cast that only repeats the 68000 operand size, no double integer cast.
   [packed; the rest review only]
5. The port assumes an arithmetic right shift of a negative value; `data/bytes.h` asserts it with
   `static_assert`. [review only]
6. One statement per line: no comma operator outside the header of a `for`, no chained assignment, at most
   one variable with an initializer per declaration. [format; review only]
7. Use `for`, `while`, `switch`, and early `return`. `goto` only for one cleanup label in a function that
   frees memory. [review only]
8. A function is at most 60 lines, or its allowlist line gives the reason (one short case for each type of a
   game table). [long]

## 6. Results and parameters

1. Two outcomes: `bool`. More outcomes: an `enum` type with named values (`enum MoveResult`). A lookup
   returns `NULL`. A count returns the count. No `0/-1`, no `1/0/-1`, no value encoded in a return value
   (`index + 2`). [result]
2. At most five parameters; a function that needs more takes a struct. [params]
3. A square passes as `Location {int16_t x, y;}`, a square with its map as `Position {int16_t map, x, y;}`.
   [review only]
4. No flags packed into a parameter (`skill | flags`), no mode number that means two things (a mode and a
   tick count). [review only]
5. Two or more output parameters become one returned struct. [review only]
6. A result does not pass through a global; it passes through the return value or a struct out-parameter.
   [review only]

## 7. State

1. A global holds state that lives across calls. Module-private state is `static`. [dead-api]
2. A file with four or more file-scope variables keeps them in one `static struct` named after the module.
   [review only]
3. Every exported function and global has a use outside its file, or the test marker of rule 2.5. [dead-api]
4. Data of one entity lives in the struct of the entity, not in parallel arrays. [review only]
5. Session state lives in `g_session` (`engine/game.h`); the outputs of the dungeon view come from
   `view_get_result()`. [review only]

## 8. File order

1. A `.c` file has this order: includes; constants; types; file-static state; functions. [order]
2. The public functions are in the order of the header. The header lists them by topic: setup and reset
   first, then queries, then commands; a one-line section comment names each topic in the header and in the
   `.c` file. [order]
3. A static helper is directly above the first function that calls it; a helper that several sections
   share is at the top of the functions. A forward declaration of a static function is allowed only for
   mutual recursion, with a comment that names the pair. [order]

   ```c
   /* SPDX-License-Identifier: GPL-3.0-or-later */
   #include "sensor.h"
   #include "record.h"
   #include <string.h>

   /* Effects of a sensor on its target. */
   #define EFFECT_SET    0
   #define EFFECT_CLEAR  1

   typedef struct { ... } SquareChange;

   static struct { ... } sensor;

   /* Setup */
   void sensor_reset(void) { ... }

   /* Floor sensors */
   static bool is_party_on(Location at) { ... }
   void sensor_process_square(Location at) { ... }
   ```

## 9. Comments

1. Each non-trivial function has a one-line comment that tells what it does in game terms: a public function
   in the header above its prototype, a static function above its definition, never both. A one-line record
   accessor needs none. [comment]
2. A comment does not repeat the code, and does not name an original symbol, address or event number. It
   names the constant (`EVENT_ENABLE_CHAMPION_ACTION`). The original name belongs in
   `docs/reference/kaos-symbols.csv`. [event-comment; the rest review only]
3. A kept quirk of the original and a fixed bug of the original get a short comment at the place, and an
   entry in the formats doc. [review only]
4. No TODO marker in code (`ponytail:`, `TODO`, `FIXME`); open work goes to `docs/todo.md`. [todo]
5. Keep existing comments. New comments only where needed, one line, about what the code does, never about
   its history. [review only]

## 10. Data and errors

1. `data/` and `engine/` never write to stderr and never exit. They log through `g_os_log_hook` and fail
   through `g_os_fatal_hook` of `data/os.h`; `data/os.c` holds the default (stderr and `exit`), `platform/`
   and the tests replace it; `os_fatal` never returns. [platform]
2. A loader returns `false` on a bad file. An image address outside the image is a port bug and calls `os_fatal`.
   [review only]
3. An allocation failure aborts. [review only]
4. A struct field that points to owned memory says so in its comment; a module that opens or allocates has a
   close or free; `malloc` and `free` of one buffer stay in one module. [review only]
5. Do not reproduce undefined register values, overflows or out-of-bounds reads of the original. Use a
   defined value, mark it with a short comment, record it as a deviation in the formats doc, mask it in the
   tests. Keep a quirk of the original only when it changes what the player sees. [review only]
6. Code that differs between Dungeon Master and Chaos Strikes Back tests `g_adventure == ADVENTURE_CSB`, or
   reads a table from the game data. No other way detects the adventure. [review only]

## 11. Tests

`docs/development.md` describes the test layout. The rules that the standard check enforces: [tests]

1. A test module imports helpers from `tests/dmtest/` only, never from another test module.
2. A C constant comes from the headers through `dmtest.constants`; no hand-mirrored value.
3. A test calls the accessors of the port; it does not decode a record by bit shifts. [review only]
4. No machine path in a test (`/Users/`, `/private/tmp/`).
5. Each test module loads its own copy of the library (`dmtest.lib`); no test compiles the library.

## Format

`.clang-format` in the repository root is the format; `make format` runs `clang-format -i` over `src/`
before each commit. The check runs `clang-format --dry-run --Werror` with the `clang-format` of the `dev`
dependency group (version 23). [format]

## Checks before a commit

`make` (clang) and `make windows` (MinGW-w64 GCC) build with 0 warnings under `-Wall -Wextra -Werror
-Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla -Wimplicit-fallthrough`.

`make check` runs the checks ([Development](development.md#coding-standard)). The checks of
`tests/standard/test_standard.py`, by their allowlist key:

| Key | Fails on |
|---|---|
| `layer` | an include that breaks the layer order, names a path or names no project header; SDL outside `platform/`; a file outside a layer folder, a public header outside `include/`, a private header in `include/`; a header name in two layers |
| `platform` | an operating system header or `_WIN32` outside `platform/`; `fprintf(stderr` or `exit(` outside `platform/` and `data/os.c` |
| `engine-write` | a write of `ui/` to an engine global, except redraw flags and hooks |
| `module` | a `.c` file without its header, a second public header of one module |
| `prefix` | a header without a prefix line, a prefix that another header owns or extends, a name outside its prefixes |
| `header` | a header head out of order, a guard that is not `DM_<LAYER>_<FILE>_H` |
| `include` | a header the file does not use |
| `comment` | a public prototype or a non-trivial static definition without a comment line above it, or a comment on both |
| `dead-api` | an exported name with no use outside its file and no test marker |
| `hex`, `decimal` | a number outside a constant definition, also a single digit in `[N]` and `< N;` |
| `enum` | an `enum` that is not a result type |
| `block` | a constant block without a comment line above it |
| `notation` | a block that mixes hex and decimal, a flag block in decimal, a count block in hex |
| `duplicate` | a constant name defined in more than one header |
| `kaos`, `address` | an image address or a `kaos_*` call outside `data/`, an address with two names |
| `numbered` | an event number, a byte offset or an address in a name |
| `packed` | a shift by 8, 16, 24 or a foreign `*_SHIFT` at a call site, a double integer cast |
| `byteorder` | a byte order helper outside `data/bytes.h` |
| `record` | `REC(` or `g_dungeon_map_data[` outside `engine/record.c` and `engine/record.h` |
| `name` | a function name above four words after the prefix or above 32 characters |
| `static-prefix` | a static function with a module prefix |
| `bool` | a predicate, a truth local or a "Returns 0/1" function that is not `bool` |
| `global` | an `extern` without `g_`, a file-static with `g_` |
| `decompiler` | a decompiler name |
| `ordinal` | `_ordinal` or `_plus_1` in a name |
| `type` | a parameter name with two types across the headers of a layer |
| `long` | a function above 60 lines without an allowlist reason |
| `result` | a function whose comment says "Returns 0" or "-1" for success |
| `params` | a prototype with more than five parameters |
| `order` | a `.c` file whose public definitions are not in header order, or a forward declaration without a recursion comment |
| `event-comment` | an event number in a comment (`event 11`) |
| `todo` | a TODO marker in code |
| `tests` | a `from test_` import, a machine path, a clang call in `tests/` |
| `format` | a file that `clang-format --dry-run --Werror` reports |
| `allowlist` | an allowlist line with a reason that this document does not name |
