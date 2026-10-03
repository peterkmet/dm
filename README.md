# dm

Tools to decompile Dungeon Master v3.60 (Amiga) from your own disk image. The goal is a portable C version of the game for macOS.

This repository contains no game files. You supply your own ADF disk image.

## Status

| Stage | Result | State |
|---|---|---|
| 1. Tooling | Extract the ADF, load FTL modules, decompile with Ghidra headless | Done |
| 2. Data layer | Read `Graphics.DAT` and `Dungeon.DAT` in C | Not started |
| 3. Game logic | Clean C from the decompiled modules | Not started |
| 4. Platform layer | SDL video, sound, and input | Not started |

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
| `tools/ghidra/decompile.sh <image.bin> <base> <out.c> [REG=VALUE] [ENTRY ...]` | Decompile a flat 68000 image with Ghidra headless. |
| `tools/decompile-all.sh` | Run `dm-ftl` and `decompile.sh` for every module in `game/`. |

## Known limitations

- Ghidra cannot decompile 24 functions: 15 in `KAOS`, 8 in `VDEO`, and 1 in `ANIM`. The output contains `/* failed: Low-level Error: Cannot properly adjust input varnodes */` for each one. The cause is not yet known.
- The decompiled C does not compile. Names, types, and register calling conventions still need work in stages 2–3.

## How decompile.sh finds functions

`decompile.sh` marks each `ENTRY` address as a function. It also marks each `LINK A5` or `LINK A6` instruction that follows an `RTS`. `REG=VALUE` sets a register for the whole program, for example `A5=0x104160` for the module data base.

## Tests

```sh
uv run pytest
```

## Documentation

- [FTL module format](docs/ftl-format.md)
