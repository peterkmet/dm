# dm

Tools to decompile Dungeon Master v3.60 (Amiga) from your own disk image. The goal is a portable C version of the game for macOS.

This repository contains no game files. You supply your own ADF disk image.

## Status

| Stage | Result | State |
|---|---|---|
| 1. Tooling | Extract the ADF, load FTL modules, decompile with Ghidra headless | Done |
| 2. Data layer | Read `KAOS.FTL`, `Graphics.DAT` and `Dungeon.DAT` in C | Done |
| 3. Game logic | Reimplement the game rules in C from the decompiled code | In progress: game state, random numbers, timeline, text, sound done |
| 4. Platform layer | SDL video, sound, input, and upscaling | Done |

The detailed plan is in [docs/superpowers/2026-10-04-dm-c-port-plan.md](docs/superpowers/2026-10-04-dm-c-port-plan.md).

## Prerequisites

- macOS with [Homebrew](https://brew.sh).
- [uv](https://docs.astral.sh/uv/).
- A Dungeon Master v3.60 Amiga disk image (`.adf`).

## Quickstart

1. Install Ghidra. Homebrew also installs OpenJDK 21.

   ```sh
   brew install ghidra
   ```

2. Install the Python tools.

   ```sh
   uv sync
   ```

3. Extract the disk image to `game/`. Replace `<disk.adf>` with the path to your ADF file.

   ```sh
   uv run dm-extract <disk.adf> game
   ```

   The command prints one line for each file: SHA-256, size, and path.

4. Decompile all FTL modules to `decomp/`.

   ```sh
   tools/decompile-all.sh
   ```

   For each module, the script writes `decomp/<MODULE>.bin` (memory image), `decomp/<MODULE>.c` (decompiled C), and `decomp/<MODULE>.c.log` (Ghidra log). A full run takes about 20 seconds.

The `.gitignore` file excludes `*.adf`, `game/`, and `decomp/`. Do not commit game content.

## Tools

| Command | Purpose |
|---|---|
| `uv run dm-extract <adf> [out_dir]` | Extract all files from an AmigaDOS ADF. The default `out_dir` is `game`. |
| `uv run dm-ftl <module.FTL> <out.bin> [--base ADDR]` | Load an FTL module into a flat memory image, as `BJELoad_R` does. Prints the addresses and the arguments for `decompile.sh`. The default base is `0x100000`. |
| `[SYMBOLS=<csv>] tools/ghidra/decompile.sh <image.bin> <base> <out.c> [REG=VALUE] [ENTRY ...]` | Decompile a flat 68000 image with Ghidra headless. |
| `tools/decompile-all.sh` | Run `dm-ftl` and `decompile.sh` for every module in `game/`. It uses `docs/<module>-symbols.csv` as `SYMBOLS` (for example `docs/kaos-symbols.csv`). |
| `dmtools.emu` (Python) | Run a function of a module image in a 68000 emulator. `Emu(image).call(addr, [(size, value), ...])` pushes the arguments right to left and returns `D0`. Access to custom chips, CIA, or ExecBase stops the call with `EmuError`. Use `allow_hw=True` to permit this access. |

## C port

The C port is in `port/`. It uses C11, `clang`, and SDL3. Install SDL3 first.

```sh
brew install sdl3
```

Build the game and the test library. The output goes to `build/`.

```sh
make -C port
```

Run the game. It needs `KAOS.FTL`, `Graphics.DAT`, and `Dungeon.DAT` in `game/`. `--data DIR` selects another folder.

```sh
build/dm
```

The game starts at the entrance. Click ENTER or press Return to start a new game. RESUME shows "NO SAVED GAME YET", because saved games are not available yet. QUIT or Ctrl+Q closes the game. Closing the window also closes the game.

### Controls

The controls are the controls of the original Amiga game.

| Input | Action |
|---|---|
| Up, keypad 8 | Move forward |
| Down, keypad 2 or 5 | Move backward |
| Left, keypad 4 | Move left (sideways) |
| Right, keypad 6 | Move right (sideways) |
| Delete, keypad 7 | Turn left |
| Help, Insert, keypad 9 | Turn right |
| F1 to F4 | Open or close the inventory of champion 1 to 4 |
| Escape | Freeze the game. Escape again continues. |
| Return | Wake up a sleeping party |
| Click on the movement arrows | Move and turn |
| Click on a mirror | Show the champion. Then click RESURRECT, REINCARNATE, or CANCEL. |
| Click on an object in the view | Take the object into the leader hand |
| Click on the floor with an object in the hand | Drop the object |
| Click on the front wall or a door button | Use the wall switch or the door button |
| Click on a hand in a status box | Take or put an object |
| Click on a status box name | Select the leader |
| Right-click on a status box | Open the inventory |
| Inventory: click on a slot | Take or put an object |
| Inventory: click on the mouth | Eat or drink the object in the hand |
| Inventory: hold the eye | Show skills and statistics |
| Inventory: ZZZ or the X | Sleep or close the inventory |
| Action area | Click a champion hand, then an action |
| Spell area | Click runes, then the cast button. Click a tab to select another caster. |

Options:

- `--filter none|scale2x|scale3x` selects the pixel-art upscale filter.
- `--frames N` quits after N frames (50 frames per second).
- `--view` shows only the dungeon view at the party start. Up and Down move, Left and Right turn, A and D move sideways.
- `--show N` shows graphic N of `Graphics.DAT`.
- The environment variable `DM_AUTO_ENTER=1` presses ENTER at the start. The tests use it.

Not available yet: saved games, actions and throwing (the action menu opens, but the action has no effect), the description of an object under the eye, chests and scrolls in the panel, the credits, the door animation of the entrance, and the end of the game.

Set `DM_FILTER` to select a pixel-art upscale filter. The values are `none` (default), `scale2x`, and `scale3x`. The filter makes the image 2 or 3 times larger before the window scaling. The filter gives the sharpest image when the window scale is a multiple of the filter factor. Example: `DM_FILTER=scale2x build/dm`.

`build/libdm.dylib` contains the game code without SDL. `tests/test_port.py` loads it with `ctypes`.

## Known limitations

- Some decompiled code still contains `>> 0x10` casts, for example `(short)((uint)x >> 0x10)`. `grep -c '>> 0x10' decomp/KAOS.c` gives 92 lines (281 lines without `InferParams.java` and `ResolveJumpTable.java`). Most of these lines are the high word of a 32-bit value in D0 or D1, for example in `CONCAT22` or `in_D0 >> 0x10`.
- The decompiled C does not compile. Names, types, and register calling conventions still need work in stages 2–3.

## How decompile.sh finds functions

`decompile.sh` marks each `ENTRY` address as a function. It also marks each `LINK A5` or `LINK A6` instruction that follows an `RTS`. `REG=VALUE` sets a register for the whole program, for example `A5=0x104160` for the module data base.

After the auto-analysis, `decompile.sh` runs these Ghidra scripts in this order:

1. `ApplySymbols.java` reads the CSV file in `SYMBOLS`. The columns are `address,name,signature,subsystem`. The first line is a header. A row names a function when the row has a signature or when the address already holds an instruction. The script creates the function if it does not exist. Other rows label a global. The script does not apply the signature text. If `SYMBOLS` is empty or the file does not exist, the script does nothing.
2. `InferParams.java` sets the stack parameters of each function that starts with `LINK A5` or `LINK A6`. The compiler uses a 16-bit `int`: each argument uses a 2-byte (word) or 4-byte (long) stack slot. The script reads each `d16(A5)` or `d16(A6)` operand with `d16 >= 8`. The largest access size at an offset gives the type: byte gives `char`, word gives `short`, long gives `undefined4`. A byte access at an odd offset reads the low byte of a word slot. The script fills gaps with `undefined2` slots. The parameter names are `arg_<offset>`, for example `arg_8` and `arg_a`.
3. `ResolveJumpTable.java` finds each `JSR d16(A5)` or `JMP d16(A5)` that goes through a module jump table slot. It reads the `JMP abs.l` target of the slot and adds a call override reference. The decompiler then shows a direct call to the target function, with its parameters.
4. `ExportC.java` decompiles all functions to the output file.

## Tests

```sh
uv run pytest
```

`tests/test_port.py` builds the C port and tests it through `ctypes`. Pytest skips it when `clang` or SDL3 is not installed.

`tests/test_ghidra.py` runs `decompile.sh` on a small synthetic image. It takes about 4 seconds. Pytest skips it when Ghidra is not installed.

## Documentation

- [FTL module format](docs/ftl-format.md)
- [Dungeon.DAT format](docs/dungeon-format.md)
- [KAOS map](docs/kaos-map.md) and subsystem notes in [docs/symbols/](docs/symbols/)
