# DMMac: Dungeon Master for macOS and Windows

DMMac is a native C11 and SDL3 port of the Amiga versions of **Dungeon Master v3.60** and **Chaos Strikes Back**
for macOS (Apple Silicon) and Windows (x86-64).

<p align="center">
  <img src="docs/images/screenshot.png" alt="Dungeon Master viewport showing a dungeon corridor">
  <img src="docs/images/screenshot-chaos.png" alt="Facing Lord Chaos on level 12">
</p>

This repository contains no game data. You must provide your own Amiga disk images (`.adf`): Dungeon Master
v3.60 (1 disk) and optionally Chaos Strikes Back (disks A and B). DOS, Atari ST, and other Amiga releases are
not supported. Disks are identified by content rather than filename.

## Features

- **Full campaigns:** Both Dungeon Master and Chaos Strikes Back are playable in full (Dungeon Master verified
  by a complete playthrough to the defeat of Lord Chaos).
- **Vanilla mode:** Faithful original rules, timing, and interface.
- **Advanced mode:** Automap, in-game options menu (volume, music, upscale filter), 8 save slots, quick save/load,
  cheat screen, WASD movement, 8 simultaneous sounds, FM-Towns and Super NES soundtracks, and direct entry to
  Chaos Strikes Back from the prison stairs.
- **LLM mode (macOS):** Turn-by-turn programmatic control over a Unix domain socket for scripts and language
  models.
- **Display scaling:** Dynamic window resizing with `scale2x` and `scale3x` pixel-art filters.

## Play

Download the [latest release](https://github.com/peterkmet/dmmac/releases/latest) (`DMMac-macos-arm64.zip` or
`DMMac-windows-x64.zip`), extract it, and copy your `.adf` disk images into the directory.

- **macOS:** Remove the quarantine attribute and launch `Dungeon Master.command` (or double-click it in
  Finder):

  ```sh
  xattr -d com.apple.quarantine dm "Dungeon Master.command" libSDL3.0.dylib
  open "Dungeon Master.command"
  ```

- **Windows 10+:** Run `dm.exe`. If Windows SmartScreen warns about an unknown publisher, select "More info",
  then "Run anyway".

On first launch, the engine extracts the disk images into `game/` and `game-csb/`. Save files are stored in the
same folders using the original Amiga binary format.

Keys of the advanced mode (in the dungeon view):

| Key | Action |
|---|---|
| W, A, S, D, Q, E | Move and turn (the cursor keys and the keypad also work) |
| M or Tab | Automap |
| Escape | Options menu (volume, music, upscale filter) |
| C | Cheat screen |
| F5, F9 | Quick save, quick load |
| Ctrl+S | Save dialog with the 8 slots |

See the [Player Manual](docs/manual.md) for all controls, game modes, and configuration.

## Let an LLM play

```sh
build/dm --mode turnbased --control /tmp/dm.sock
DM_CONTROL=/tmp/dm.sock uv run dm-play --brief get_state
DM_CONTROL=/tmp/dm.sock uv run dm-play act move_forward
```

The `dm-mcp` server exposes the control socket over the Model Context Protocol (MCP) for clients such as Claude
Code. Protocol specifications, available actions, and setup instructions are covered in
[LLM Mode](docs/llm/llm-mode.md). Multi-agent playthroughs (using coordinator and player agents) are documented
in [LLM Runs](docs/llm/llm-runs.md).

## Build

```sh
brew install sdl3
uv sync
make            # Build build/dm and the test library
make test       # Run the test suite
make check      # Check formatting, coding standard, and tests
make windows    # Cross-compile build/dm.exe with MinGW-w64
```

Run `build/dm` from the directory containing your `.adf` images. See [Development](docs/development.md) for
build and release workflows, [Architecture](docs/architecture.md) and [Coding Standard](docs/coding-standard.md)
for codebase structure, and [CHANGELOG.md](CHANGELOG.md) for release history.

## Engineering

DMMac is a native C11 engine that runs the Amiga releases of Dungeon Master and Chaos Strikes Back without
emulation. The port was developed with Claude Code in four stages:

1. **Decompilation:** The original 68000 FTL binary modules were decompiled to C using Ghidra
   ([Decompiling](docs/decompiling.md)), with function and global variable names mapped via the symbol tables
   in `docs/reference/`.
2. **Reimplementation:** Game logic was rewritten in C11 strictly from the decompilation, without using code
   from other ports.
3. **Differential verification:** Automated tests execute ported C functions alongside the original 68000 code
   in an emulator using identical inputs and compare the resulting state. Golden snapshots validate save files,
   viewport rendering, and game mechanics.
4. **Refactoring:** Subsequent passes preserved exact runtime behavior while introducing named constants, typed
   accessors, a five-layer architecture, automated coding-standard checks, and a test suite audit.

[ReDMCSB](http://dmweb.free.fr/community/redmcsb/) is Christophe Fontanel's reconstruction of the original 68000 source code, which compiles back into bit-identical Amiga and
Atari ST binaries. DMMac does not use its code: the game logic comes from DMMac's own decompilation (stage 2). DMMac
adopts ReDMCSB's function and structure names, which keeps the two code bases easy to compare.

Additional technical documentation covers [FTL file formats](docs/formats/ftl.md),
[game state rules](docs/rules/game-state.md), [Chaos Strikes Back internals](docs/csb/data.md), and the
[KAOS map](docs/reference/kaos-map.md).

## Acknowledgements

- [SDL3](https://github.com/libsdl-org/SDL) — windowing, input, and audio output.
- [dr_mp3](https://github.com/mackron/dr_libs) by David Reid — MP3 audio decoding.
- [amitools](https://github.com/cnvogelg/amitools) — ADF image extraction and reference implementation for ADF
  reader tests.
- [machine68k](https://github.com/cnvogelg/machine68k) — 68000 CPU emulation for differential tests.
- [ReDMCSB](http://dmweb.free.fr/community/redmcsb/) by Christophe Fontanel — symbol and structure naming
  reference.
- [dmcsb](https://codeberg.org/CyCl0ne/DungeonMasterTools) by Cycl0ne — reference validator for `Dungeon.DAT`
  test checks.

## License

GPL-3.0-or-later ([LICENSE](LICENSE)). Dungeon Master and Chaos Strikes Back are properties of FTL Games. This
project is not affiliated with or endorsed by the rights holders. Original game data is not included and is not
covered by this license.
