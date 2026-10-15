# C port coding standard

The C port is a readable reimplementation of the game. It is not a cleaned-up decompiler listing. The tests prove that the behavior matches the original. This standard makes the code readable.

## Rules

1. Name every game value. Use an `enum` or a `#define` with a name that tells the game meaning.
   - Thing types: `THING_DOOR`, `THING_SENSOR`, `THING_GROUP`, and so on, not `3`.
   - No thing: `THING_NONE`, not `0xffff`. End of list: `THING_ENDOFLIST`.
   - Directions, cells, champion slots (`SLOT_HAND_READY`, `SLOT_COUNT`), statistics, skills, square types, sensor types, door states, timeline event types, commands.
   - Message and text numbers: a named constant for each string id, for example `TEXT_RESURRECTED`.
   - Graphic and layout zone numbers: named constants in `port/src/data/ids.h`, for example `GFX_WALLSET_FIRST`, `ZONE_VIEWPORT`.
   - Plain numbers are allowed only for 0, 1, -1, and 2 when it halves or doubles a value. Every other number is a named constant, also decimal game values such as divisors, limits, and thresholds.
   - Notation: hex for bit masks and flags, and for KAOS addresses and Amiga file format values. Decimal for counts, sizes, limits, thresholds, and ticks.
   - Each KAOS image address has one name with the prefix `KAOS_` in `data/kaos_addresses.h`, which only `data/` includes; other modules call `tables_*`.
   - Addresses of other modules live in `data/` too: `CSB_` for the CSB image and `APPB.FTL` (`CSB_APPB_*`), `SWSH_` for `SWSH.FTL` (`data/swsh.c`). Each value has one name per prefix.
   - Each constant has one definition, in the header of the module that owns it. Other modules include that header. They do not define the value again under the same or another name.
2. Do not index raw record bytes outside one accessor module. Put field access in small functions or macros with names, for example `rec_sensor_type(t)`, `rec_sensor_disable(t)`, `door_state(t)`, `group_count(t)`. One module (`map.c` and `map.h`) owns the record layout.
3. Write C, not decompiler output.
   - No names like `uVar1`, `iVar3`, `local_10`, `arg_8`.
   - Game values keep the 16-bit types of the original (`int16_t`, `uint16_t`). One value has one type on its whole path: field, parameter, local variable, and return value. Then no cast is needed to pass it on. Use `int` only for values that are not game values, for example sizes, file offsets, and loop counters over C arrays. Do not use `long`: it has 32 bits on Windows and 64 bits on macOS. A value wider than `int` uses a fixed-width type (`int64_t`, `uint32_t`) with the `<inttypes.h>` formats.
   - A cast is allowed only where it changes the result (16-bit wrap, sign, truncation). Add a short comment when the reason is not obvious.
   - No casts that only repeat the 68000 operand size.
   - Predicates (`is_*`, `has_*`, `can_*`), also `static` ones, return `bool` from `<stdbool.h>`. Other truth values (local variables, parameters, return values) are `bool` too. Flags in game records and save files keep their stored type.
   - One statement per line: no comma operator, no chained assignment (`a = b = 0`), and at most one variable with an initializer per declaration.
   - Use `for`, `while`, `switch`, and early `return`. Do not use `goto`, except for one cleanup label in a function that frees memory.
   - One function does one game task. Split a long original function when parts have their own meaning. A function longer than 60 lines needs a reason in its comment, or a split.
4. Each non-trivial function has a one-line comment that tells what it does in game terms. The comment does not repeat the code, and does not give the original address. The address belongs in `docs/kaos-symbols.csv`.
5. Keep original quirks only when they change what the player sees. Fix the original bugs that are clearly bugs (stale registers, overflows), and mark each fix with a short comment.
6. Read game data at run time. Do not copy game tables into the source.
7. Names follow one scheme.
   - Functions: `<prefix><verb>_<object>` in `snake_case`. Each header states its prefixes in one comment line, for example `/* Prefixes: gfx_ */` in `graphics.h`. A prefix belongs to one header only, and no prefix starts with the prefix of another header.
   - Global variables: `g_` prefix. Module-private globals and functions are `static`.
   - Types: `PascalCase`. Constants and enum values: `UPPER_CASE`.
   - No event numbers or byte offsets in names of functions, struct fields and constants. No original addresses in names. Name the game task, for example `group_process_reaction_event`, not `group_process_events_29_to_41`.
8. The source has four layers in subfolders of `port/src/`: `data/`, `engine/`, `ui/`, and `platform/`. A file includes only headers of its own layer and of the layers before it in this list. Game rules in `engine/` do not call the user interface: they set redraw flags. Exception: a hook that `ui/` sets, for an event that must run the user interface inside the rule, for example `g_endgame_sensor_hook`. Only `platform/` includes SDL. Platform code (`_WIN32`, POSIX headers) only in `platform/`; the other layers reach folder services through the hooks of `data/os.h`, which `platform/` sets. `ui/` does not write engine variables, except redraw flags and hooks.
9. Big-endian read and write helpers (`BE16`, `WR16`, `BE32`, `WR32`) live in one header, `data/bytes.h`. Modules do not define their own copies.
10. Code that differs between Dungeon Master and Chaos Strikes Back tests `g_adventure == ADVENTURE_CSB`, or reads a table from the game data. No other way detects the adventure.
11. Format: `.clang-format` in the repo root; `clang-format -i` before each commit.
12. Do not pack or unpack fields at call sites. A packed field has one accessor in the module that owns it. A `.c` file of another module does not shift by the `*_SHIFT` constants of that header, and does not shift by 8, 16 or 24.

## Check before a step is done

`make -C port` (clang) and `make -C port windows` (MinGW-w64 GCC) build with 0 warnings and no warning switches.

`tests/test_standard.py` checks `port/src/` and fails on:

- hex literals outside constant definitions,
- decimal literals of 10 or more outside constant definitions,
- decompiler names,
- raw record access (`g_current_map[`, `dungeon_get_thing_data(...)[`) outside `map.c` and `map.h`,
- constant names defined in more than one header,
- a constant with the wrong number base for its name (mask or flag in decimal, count or size in hex),
- packed fields at call sites (shifts by 8, 16, 24 or by a foreign `*_SHIFT`),
- byte order helpers defined outside `data/bytes.h`,
- event numbers or byte offsets in names,
- KAOS image addresses or `kaos_*` calls outside `data/`, and an image address with more than one name,
- predicates that do not return `bool`, also in `.c` files,
- platform code outside `platform/`,
- writes of `ui/` to engine variables (redraw flags and hooks excepted),
- source files that `clang-format --dry-run --Werror` (version 23) reports as not formatted.

`tests/standard_allowlist.txt` lists the accepted exceptions, one per line with a reason.

A reader who knows Dungeon Master can understand each function without opening the decompiled code.
