# C port coding standard

The C port is a readable reimplementation of the game. It is not a cleaned-up decompiler listing. The tests prove that the behavior matches the original. This standard makes the code readable.

## Rules

1. Name every game value. Use an `enum` or a `#define` with a name that tells the game meaning.
   - Thing types: `THING_DOOR`, `THING_SENSOR`, `THING_GROUP`, and so on, not `3`.
   - No thing: `THING_NONE`, not `0xffff`. End of list: `THING_ENDOFLIST`.
   - Directions, cells, champion slots (`SLOT_HAND_READY`, `SLOT_COUNT`), statistics, skills, square types, sensor types, door states, timeline event types, commands.
   - Message and text numbers: a named constant for each string id, for example `TEXT_RESURRECTED`.
   - Graphic and layout zone numbers: named constants in `port/src/ids.h`, for example `GFX_WALLSET_FIRST`, `ZONE_VIEWPORT`.
   - Plain numbers are allowed only when the number is obvious: 0, 1, -1, 2 for halves, loop bounds that come from a named constant.
2. Do not index raw record bytes outside one accessor module. Put field access in small functions or macros with names, for example `sensor_type(t)`, `sensor_disable(t)`, `door_state(t)`, `group_count(t)`. One module (`dungeon.c` or `things.h`) owns the record layout.
3. Write C, not decompiler output.
   - No names like `uVar1`, `iVar3`, `local_10`, `arg_8`.
   - No casts that only repeat the 68000 operand size. Keep a cast only where it changes the result (16-bit wrap, sign, truncation). Then add a short comment when the reason is not obvious.
   - Use `for`, `while`, `switch`, and early `return`. Do not use `goto`, except for one cleanup label in a function that frees memory.
   - One function does one game task. Split a long original function when parts have their own meaning.
4. Each non-trivial function has a one-line comment that tells what it does in game terms. The comment does not repeat the code, and does not give the original address. The address belongs in `docs/kaos-symbols.csv`.
5. Keep original quirks only when they change what the player sees. Fix the original bugs that are clearly bugs (stale registers, overflows), and mark each fix with a short comment.
6. Read game data at run time. Do not copy game tables into the source.

## Check before a step is done

- `grep -nE '0x[0-9a-fA-F]{2,}' port/src/<file>.c` shows only named-constant definitions or data addresses in a single table of addresses.
- A reader who knows Dungeon Master can understand each function without opening the decompiled code.
