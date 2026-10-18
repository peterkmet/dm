# Decompiling

How to decompile Dungeon Master v3.60 (Amiga) from your own disk image with Ghidra. The output is the reference for the C port. The repository contains no game files.

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

   The [player manual](manual.md#extract-the-disks-by-hand) describes the folder layout and the output.

4. Decompile all FTL modules to `reference/decomp/dm/`.

   ```sh
   tools/ghidra/decompile-all.sh
   ```

   For each module, the script writes `reference/decomp/dm/<MODULE>.bin` (memory image), `reference/decomp/dm/<MODULE>.c` (decompiled C), and `reference/decomp/dm/<MODULE>.c.log` (Ghidra log). A full run takes about 20 seconds.

5. Optional: extract the two Chaos Strikes Back (CSB) disks to `game-csb/` and `game-csb/disk-b/`, see the [player manual](manual.md#extract-the-disks-by-hand).

The `.gitignore` file excludes `*.adf`, `/game`, `/game-csb`, and `reference/` (original disks, music, extracted versions, decompiled code). Do not commit game content.

## Tools

| Command | Purpose |
|---|---|
| `uv run dm-extract <adf> [out_dir]` | Extract all files from an AmigaDOS ADF (`tools/dmtools/extract.py`). The default `out_dir` is `game`. |
| `uv run dm-ftl <module.FTL> <out.bin> [--base ADDR]` | Load an FTL module into a flat memory image, as `BJELoad_R` does (`tools/dmtools/ftl.py`). Prints the addresses and the arguments for `decompile.sh`. The default base is `0x100000`. |
| `[SYMBOLS=<csv>] tools/ghidra/decompile.sh <image.bin> <base> <out.c> [REG=VALUE] [ENTRY ...]` | Decompile a flat 68000 image with Ghidra headless. |
| `tools/ghidra/decompile-all.sh` | Run `dm-ftl` and `decompile.sh` for every module in `game/`. It uses `docs/reference/<module>-symbols.csv` as `SYMBOLS` (for example `docs/reference/kaos-symbols.csv`). |
| `dmtools.emu` (Python) | Run a function of a module image in a 68000 emulator (`tools/dmtools/emu.py`). `Emu(image).call(addr, [(size, value), ...])` pushes the arguments right to left and returns `D0`. Access to custom chips, CIA, or ExecBase stops the call with `EmuError`. Use `allow_hw=True` to permit this access. |

## How decompile.sh finds functions

`decompile.sh` marks each `ENTRY` address as a function. It also marks each `LINK A5` or `LINK A6` instruction that follows an `RTS`. `REG=VALUE` sets a register for the whole program, for example `A5=0x104160` for the module data base.

After the auto-analysis, `decompile.sh` runs these Ghidra scripts in this order:

1. `ApplySymbols.java` reads the CSV file in `SYMBOLS`. The columns are `address,name,signature,subsystem`. The first line is a header. A row names a function when the row has a signature or when the address already holds an instruction. The script creates the function if it does not exist. Other rows label a global. The script does not apply the signature text. If `SYMBOLS` is empty or the file does not exist, the script does nothing.
2. `InferParams.java` sets the stack parameters of each function that starts with `LINK A5` or `LINK A6`. The compiler uses a 16-bit `int`: each argument uses a 2-byte (word) or 4-byte (long) stack slot. The script reads each `d16(A5)` or `d16(A6)` operand with `d16 >= 8`. The largest access size at an offset gives the type: byte gives `char`, word gives `short`, long gives `undefined4`. A byte access at an odd offset reads the low byte of a word slot. The script fills gaps with `undefined2` slots. The parameter names are `arg_<offset>`, for example `arg_8` and `arg_a`.
3. `ResolveJumpTable.java` finds each `JSR d16(A5)` or `JMP d16(A5)` that goes through a module jump table slot. It reads the `JMP abs.l` target of the slot and adds a call override reference. The decompiler then shows a direct call to the target function, with its parameters.
4. `ExportC.java` decompiles all functions to the output file.

## Known limitations

- Some decompiled code still contains `>> 0x10` casts, for example `(short)((uint)x >> 0x10)`. `grep -c '>> 0x10' reference/decomp/dm/KAOS.c` gives 92 lines (281 lines without `InferParams.java` and `ResolveJumpTable.java`). Most of these lines are the high word of a 32-bit value in D0 or D1, for example in `CONCAT22` or `in_D0 >> 0x10`.
- The decompiled C does not compile. Names, types, and register calling conventions still need work.

## Related pages

- [FTL module format](formats/ftl.md)
- [KAOS map](reference/kaos-map.md) and the symbol notes in [reference/symbols/](reference/symbols)
