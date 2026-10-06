# C port coding standard

The C port is a readable reimplementation of the game. It is not a cleaned-up decompiler listing. The tests prove that the behavior matches the original. This standard makes the code readable.

## Rules

1. Name every game value. Use an `enum` or a `#define` with a name that tells the game meaning.
   - Thing types: `THING_DOOR`, `THING_SENSOR`, `THING_GROUP`, and so on, not `3`.
   - No thing: `THING_NONE`, not `0xffff`. End of list: `THING_ENDOFLIST`.
   - Directions, cells, champion slots (`SLOT_HAND_READY`, `SLOT_COUNT`), statistics, skills, square types, sensor types, door states, timeline event types, commands.
   - Message and text numbers: a named constant for each string id, for example `TEXT_RESURRECTED`.
   - Graphic and layout zone numbers: named constants in `port/src/ids.h`, for example `GFX_WALLSET_FIRST`, `ZONE_VIEWPORT`.
   - Plain numbers are allowed only for 0, 1, -1, and 2 when it halves or doubles a value. Every other number is a named constant, also decimal game values such as divisors, limits, and thresholds.
   - Each constant has one definition, in the header of the module that owns it. Other modules include that header. They do not define the value again under the same or another name.
2. Do not index raw record bytes outside one accessor module. Put field access in small functions or macros with names, for example `sensor_type(t)`, `sensor_disable(t)`, `door_state(t)`, `group_count(t)`. One module (`map.c` and `map.h`) owns the record layout.
3. Write C, not decompiler output.
   - No names like `uVar1`, `iVar3`, `local_10`, `arg_8`.
   - Use `int16_t` or `uint16_t` only where the 16-bit wrap or sign changes the game result, as in the original 16-bit `int`. Mark each such variable with a short comment. Everywhere else, use `int`.
   - A cast is allowed only where it changes the result (16-bit wrap, sign, truncation). Add a short comment when the reason is not obvious.
   - Predicates (`is_*`, `has_*`, `can_*`) return `bool` from `<stdbool.h>`. Flags in game records keep their stored type.
   - Use `for`, `while`, `switch`, and early `return`. Do not use `goto`, except for one cleanup label in a function that frees memory.
   - One function does one game task. Split a long original function when parts have their own meaning. A function longer than 60 lines needs a reason in its comment, or a split.
4. Each non-trivial function has a one-line comment that tells what it does in game terms. The comment does not repeat the code, and does not give the original address. The address belongs in `docs/kaos-symbols.csv`.
5. Keep original quirks only when they change what the player sees. Fix the original bugs that are clearly bugs (stale registers, overflows), and mark each fix with a short comment.
6. Read game data at run time. Do not copy game tables into the source.
7. Names follow one scheme.
   - Functions: `<module>_<verb>_<object>` in `snake_case`. The prefix is the file name, for example `action_` in `action.c`.
   - Global variables: `g_` prefix. Module-private globals and functions are `static`.
   - Types: `PascalCase`. Constants and enum values: `UPPER_CASE`.
   - No event numbers or original addresses in names. Name the game task, for example `group_process_reaction_event`, not `group_process_events_29_to_41`.
8. Game rules do not depend on the user interface or on SDL. Modules in `engine/` include only `engine/` and `data/` headers. Only `platform/` includes SDL.
9. Big-endian read and write helpers (`BE16`, `WR16`, `BE32`, `WR32`) live in one header. Modules do not define their own copies.
10. Code that differs between Dungeon Master and Chaos Strikes Back tests `g_adventure == ADVENTURE_CSB`, or reads a table from the game data. No other way detects the adventure.

## Check before a step is done

`tests/test_standard.py` checks `port/src/` and fails on:

- hex literals outside constant definitions,
- decimal literals of 10 or more outside constant definitions,
- decompiler names,
- raw record access (`g_current_map[`, `get_thing_data(...)[`) outside `map.c` and `map.h`,
- constant names defined in more than one header.

`tests/standard_allowlist.txt` lists the accepted exceptions, one per line with a reason.

A reader who knows Dungeon Master can understand each function without opening the decompiled code.
