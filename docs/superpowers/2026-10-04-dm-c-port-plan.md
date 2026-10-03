# Dungeon Master v3.60 C port: implementation plan

## Goal

Produce a portable C version of Dungeon Master v3.60 (Amiga) that builds and runs on macOS. The C code is a functional equivalent of the original game code in `KAOS.FTL`. It is derived from the decompiled code. An optional upscaling filter improves the output image.

## Decisions

- Basis: our own decompilation of the user's disk (`docs/ftl-format.md`, stage 1). ReDMCSB and CSBwin knowledge may be used for names and data formats only. Do not copy code from ReDMCSB, CSBwin, or PyDM.
- Fidelity: a reimplementation, not an emulation. Game rules (movement, combat, magic, creatures, timeline, random numbers) follow the decompiled code exactly. The Amiga technical layer is not reproduced: no heap or bitmap cache, no blitter, bitplanes, copper, interrupts, or busy waits. Drawing goes straight to the chunky framebuffer, and a plain game loop drives timing.
- Scope: `KAOS.FTL` is the whole game. It does not call other modules except a return to `APPB`. Intro modules (`APPB`, `ANIM`, `SWSH`, `TITL.DAT`) are out of scope until the game runs.
- Language: C11, `clang`, plain `Makefile`. SDL3 from Homebrew (`brew install sdl3`). No CMake.
- Integer semantics: the original compiler uses 16-bit `int`. Use `int16_t`/`uint16_t` where the original uses 16-bit values. Keep overflow behavior.
- Data files are big-endian. Read them with explicit byte order.
- Screen: 320×200 indexed framebuffer (`uint8_t`, palette index per pixel). The platform layer converts it to RGB and scales it.
- Oracles for tests:
  1. The original code, run function by function in the `machine68k` CPU emulator (`dmtools.emu`). Use it for pure functions: same input, same output.
  2. PyDM in `/Users/lego/github/pyDM` and its data library `dmcsb`. Use them for data decoding and expected values. Read only.
- Tests: `pytest` is the single test runner. C code is built as a shared library `build/libdm.dylib` and tested through `ctypes`. Each step adds at least one automated test.
- Game files stay in `game/` (git-ignored). Tests that need game files skip when `game/` is missing.

## Repository layout (target)

```text
src/dmtools/        Python tools (extract, ftl, emu)
tools/ghidra/       Ghidra headless scripts
docs/               Format and architecture docs
docs/kaos-symbols.csv   Address, name, signature, subsystem for KAOS functions and globals
port/               C port
port/Makefile
port/src/*.c, *.h   Game code by subsystem, plus platform layer
tests/              pytest tests (Python tools and C port)
```

## Steps

Each step ends with green tests and a commit. A fresh subagent executes each step. The `reviewer` agent reviews each step. Each step lists the original functions it ports from `docs/kaos-symbols.csv`.

### Step 1: Emulator harness

- Add `src/dmtools/emu.py`. It loads a module image (from `dmtools.ftl.load_image`) into `machine68k` memory, sets `A5` to the data base, and calls a function at an address with arguments. Arguments are a list of `(size, value)` pairs, size 2 or 4 bytes, pushed right to left like the original compiler. It returns `D0` and gives access to memory.
- Stop on the return address with a trap. Raise an error on any access to `0xDFF000–0xDFFFFF` (custom chips), `0xBFD000–0xBFEFFF` (CIA), and on reads of address 4 (ExecBase), unless the caller allows it.
- Test: run a small pure function from `KAOS` (for example the 32-bit multiply or divide helper of the runtime library) and check the result for several inputs.

### Step 2: Better decompiled output

- Add `tools/ghidra/InferParams.java` (pre-analysis or post-analysis script before `ExportC.java`). For each function with `LINK A5,#n`, collect accesses to `d16(A5)` with positive offsets from 8. Use the access size (byte, word, long) to build parameters with custom stack storage (8, 10, 12, ...). Apply the signature. Do the same for `LINK A6` functions.
- Add `tools/ghidra/ApplySymbols.java`. It reads `docs/kaos-symbols.csv` and renames functions and globals.
- Check: count `>> 0x10` casts in `decomp/KAOS.c` before and after. The count must drop strongly. Add a pytest that runs `InferParams` logic on a tiny synthetic image if practical. If not practical, record the metric in the step report.

### Step 3: KAOS map

- Classify all 749 `KAOS` functions and the main globals by subsystem: runtime library, memory, file I/O, graphics (blitter), palette, text, input, sound, dungeon data, dungeon view, champions, inventory, items, creatures, projectiles, magic, timeline, main loop, save/load, endgame.
- Give each function a descriptive name (ReDMCSB-style names are allowed) and a C signature. Write `docs/kaos-symbols.csv` and `docs/kaos-map.md` (entry point, main loop, call graph between subsystems, key data structures with offsets).
- Re-run the decompiler with `ApplySymbols.java`.
- Check: a pytest validates the CSV (unique addresses, every address is a function start in `decomp/KAOS.c`, no empty names).

### Step 4: C scaffold and platform layer

- `port/Makefile` builds `build/dm` (game) and `build/libdm.dylib` (for tests).
- Platform layer: SDL3 window, 320×200 framebuffer with 16-color palette, scale to window (integer scale, nearest), keyboard and mouse events in original coordinates, timer tick, audio stub.
- `main` shows a test pattern and quits on Escape.
- Test: pytest builds the library and checks a pure helper (for example the palette conversion) through `ctypes`.

### Step 5: Graphics data

- Load `Graphics.DAT` (header, item table, compressed images). Port the image decompression from `KAOS`.
- Test: decode every bitmap item and compare with the original decompression function run in `dmtools.emu`.

### Step 6: Dungeon data

- Load `Dungeon.DAT` (header, maps, object lists, text). Port the loader logic from `KAOS`.
- Test: compare map sizes, square data, and object counts with `dmcsb` for `Dungeon.DAT`.

### Step 7: Dungeon view rendering

Reimplement the dungeon view (floor, ceiling, walls, doors, ornaments, objects, creatures, projectiles, explosions, field effects) on the chunky framebuffer, using the drawing order and coordinate tables of the original. Test: unit tests for blit with transparency and flip, and for view square visibility; a manual visual check in `build/dm`.

### Step 8: Interface panels and text

Reimplement the screen layout, champion status boxes, action area, spell area, message area, inventory panel, and text drawing. Test: unit tests for text drawing and layout zones; a manual visual check.

### Step 9: Main loop, input, movement, timeline

Port `game_loop`, the command queue, mouse and keyboard handling, party movement and turning, sensors, doors, pits, teleporters, and the timeline. Pure rule functions get unit tests (differential tests against `dmtools.emu` where cheap). Stateful logic gets scenario tests: a command sequence on a loaded dungeon gives the expected party position and state.

### Step 10: Champions, inventory, items, actions

Port champion recruiting, stats, inventory, item use, eating and drinking, and actions. Tests as in Step 9.

### Step 11: Creatures, projectiles, magic, combat

Port creature groups and AI, projectiles, explosions, spells, damage, and death. Tests as in Step 9. The random number generator must match the original exactly.

### Step 12: Sound

Port sound playback through SDL3 audio (four channels, two stereo pairs, sample data from `Graphics.DAT`).

### Step 13: Entrance, save and load, endgame

Port the entrance screen, new game and resume, the save file format, and the endgame sequence.

### Step 14: Upscaling

- Add an optional pixel-art upscaling filter (Scale2x/EPX) in the platform layer, selected by a command-line flag.
- Test: Scale2x on a small known image gives the known output.

### Step 15: Full test and release

- Run the full test suite. Write a manual test checklist (start, movement, combat, magic, save, load, endgame). Update `CHANGELOG.md`. Merge to `main`.
