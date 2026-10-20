# DMMac: Dungeon Master for macOS and Windows

DMMac is the Amiga version of **Dungeon Master v3.60** and **Chaos Strikes Back**, rewritten in C11 and
SDL3. It compiles to a native program for macOS (Apple Silicon) and Windows (x86-64).

<p align="center">
  <img src="docs/images/screenshot.png" alt="A corridor that ends in a door">
  <img src="docs/images/screenshot-chaos.png" alt="Lord Chaos on level 12">
</p>

DMMac does not emulate the Amiga. Ghidra decompiled the original program, the game rules follow that code, and
differential tests compare the port with the original running in a 68000 emulator. The Amiga hardware layer is
not reproduced.

This repository contains no game files. You need your own Amiga disk images (`.adf`): Dungeon Master v3.60
(one disk) and, optional, Chaos Strikes Back (disks A and B). Other versions (DOS, Atari ST, other Amiga
versions) do not work. The game recognizes the disks by their content, not by the file name.

## Why one more port

DMMac exists for two reasons. The author wanted

- to play Dungeon Master natively on a modern Mac (Apple Silicon); no port did that;
- to see how far Claude Code gets when it decompiles a complete game and ports its rules to C.

[ReDMCSB](http://dmweb.free.fr/community/redmcsb/) is the reconstructed source code of the original program. It
compiles to the same 68000 binary that FTL shipped, and that binary runs on an Amiga or an Atari ST, or in an
emulator. DMMac takes from the original only the data files on the disk (graphics, dungeon, sounds). The rules
are rewritten in C from the decompilation and tested against the original. ReDMCSB gave the names of functions
and structures.

## Features

- The complete Dungeon Master and Chaos Strikes Back. Dungeon Master was played through to the defeat of Lord
  Chaos.
- **Vanilla** mode: the original game.
- **Advanced** mode: automap, options menu (volume, music, upscale filter), 8 save slots, quick save and
  quick load, cheat screen, WASD keys, 8 sounds at once, the FM-Towns and Super NES soundtracks, and a direct
  start of Chaos Strikes Back from the prison stairs.
- **LLM** mode (macOS): a program or a language model plays the game over a Unix socket, turn by turn.
- Pixel-art upscale filters (`scale2x`, `scale3x`); the picture scales with the window.

## Play

Download the [latest release](https://github.com/peterkmet/dmmac/releases/latest): `DMMac-macos-arm64.zip` or
`DMMac-windows-x64.zip`. Unpack it and copy your `.adf` disk images into the folder.

- macOS: remove the quarantine, then start `Dungeon Master.command`.

  ```sh
  xattr -d com.apple.quarantine dm "Dungeon Master.command" libSDL3.0.dylib
  ```

- Windows 10 or later: start `dm.exe`. Windows warns about an unknown publisher: "More info", "Run anyway".
  The Windows build is tested under Wine only.

At the first start the game extracts the disks into `game/` and `game-csb/`. The saved games go there too, in
the format of the Amiga original. The [player manual](docs/manual.md) has the options, the controls, the modes
and the saves.

## Let an LLM play

```sh
build/dm --mode turnbased --control /tmp/dm.sock
DM_CONTROL=/tmp/dm.sock uv run dm-play --brief get_state
DM_CONTROL=/tmp/dm.sock uv run dm-play act move_forward
```

The MCP server `dm-mcp` connects Claude Code to the socket. The protocol, the actions and the setup are in
[LLM mode](docs/llm/llm-mode.md); a long playthrough with a coordinator and players in
[LLM runs](docs/llm/llm-runs.md).

## Build

```sh
brew install sdl3
uv sync
make            # build/dm and the test library
make test       # the test suite
make check      # format, coding standard, and the suite
make windows    # build/dm.exe with MinGW-w64
```

Run `build/dm` in the folder with the `.adf` images. The build, the tests and the release are in
[Development](docs/development.md); the structure of the code in [Architecture](docs/architecture.md) and the
[coding standard](docs/coding-standard.md); the changes in the [changelog](CHANGELOG.md).

## How it was made

The port was written with Claude Code.

1. Ghidra decompiles the FTL code modules of the Amiga disk to C ([Decompiling](docs/decompiling.md)). The
   symbol tables in `docs/reference/` name the functions and the globals.
2. The game rules are rewritten in C11 from this decompilation only. No code of another port is used.
3. The differential tests run a function of the original program in a 68000 emulator and the same function of
   the port with the same input, then compare the state. Golden snapshots keep the save file, the dungeon view
   and the game rules.
4. Several cleanup passes kept the game behavior and introduced named constants, typed accessors, five layers,
   a coding standard with a check per rule, and an audit of the tests.

The docs describe the [file formats](docs/formats/ftl.md), the [game rules](docs/rules/game-state.md),
[Chaos Strikes Back](docs/csb/data.md), and the [KAOS map](docs/reference/kaos-map.md).

## Acknowledgements

- [SDL3](https://github.com/libsdl-org/SDL) handles the window, input and audio.
- [dr_mp3](https://github.com/mackron/dr_libs) by David Reid decodes the music.
- [amitools](https://github.com/cnvogelg/amitools) extracts ADF images and is the reference of the ADF reader
  tests.
- [machine68k](https://github.com/cnvogelg/machine68k) runs the original code in the differential tests.
- [ReDMCSB](http://dmweb.free.fr/community/redmcsb/) by Christophe Fontanel gave many function and structure
  names of the decompilation.
- [dmcsb](https://codeberg.org/CyCl0ne/DungeonMasterTools) by Cycl0ne is the reference of the `Dungeon.DAT`
  checks in the tests.

## License

GPL-3.0-or-later ([LICENSE](LICENSE)). Dungeon Master and Chaos Strikes Back are games of FTL Games. This
project is not affiliated with them. The game data is not included and is not covered by this license.

<p align="center"><img src="docs/images/ai-code.svg" width="200" alt="100 % of this code is AI"></p>
