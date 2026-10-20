# DMMac: Dungeon Master for macOS and Windows

DMMac is the Amiga version of **Dungeon Master v3.60** and **Chaos Strikes Back**, rewritten in C11 and
SDL3. It compiles to a native program for macOS (Apple Silicon) and Windows (x86-64).

<p align="center">
  <img src="docs/images/screenshot.png" alt="Dungeon Master in the port: a corridor that ends in a door">
  <img src="docs/images/screenshot-chaos.png" alt="Dungeon Master in the port: Lord Chaos on level 12">
</p>

The port is a reimplementation, not an emulator. It is built from its own Ghidra decompilation of the original
Amiga program: the game rules follow the decompiled code, and differential tests compare the port with the
original in a 68000 emulator. The Amiga hardware layer is not reproduced.

This repository contains no game files. You need your own Amiga disk images (`.adf`): Dungeon Master v3.60
(one disk) and, optional, Chaos Strikes Back (disks A and B). Other versions (DOS, Atari ST, other Amiga
versions) do not work. The game recognizes the disks by their content, not by the file name.

## Why one more port

There are many Dungeon Master ports and clones. DMMac exists for two reasons:

- The author wanted to play Dungeon Master as a native program on a modern Mac (Apple Silicon) and found no
  port that does this.
- The project was an experiment: can an LLM (Claude Code) decompile a complete game from its binary and write
  it again? The decompilation and the port were the exercise, not a shortcut around it.

DMMac is not [ReDMCSB](http://dmweb.free.fr/community/redmcsb/). ReDMCSB is the reconstructed source code of
the original program: it compiles to the same 68000 binary that FTL shipped, and that binary runs on an Amiga or
an Atari ST, or in an emulator. DMMac is a modern program: C11 and SDL3, compiled to a native binary for Apple
Silicon and for x86-64 Windows. From the original it takes only the data files on the disk (the graphics, the
dungeon, the sounds); the game rules were rewritten in C from the decompilation, and its own tests compare
them with the original. It adds the advanced mode and the LLM mode. ReDMCSB served as a reference for the names
of functions and structures.

## Features

- The complete Dungeon Master and Chaos Strikes Back. Dungeon Master is fully playable and tested in a complete
  playthrough, from the Hall of Champions to the defeat of Lord Chaos.
- **Vanilla** mode: the original game.
- **Advanced** mode: automap, options menu (volume, music, upscale filter), 8 save slots, quick save and
  quick load, cheat screen, WASD keys, 8 sounds at once, the FM-Towns and Super NES soundtracks, and a direct
  start of Chaos Strikes Back from the prison stairs.
- **LLM** mode (macOS): a program or a language model plays the game over a Unix socket, turn by turn.
- Pixel-art upscale filters (`scale2x`, `scale3x`) and a window that scales with its size.

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
make            # build/dm and the test library, 0 warnings
make test       # the test suite
make check      # format, coding standard, and the suite
make windows    # build/dm.exe with MinGW-w64
```

Run `build/dm` in the folder with the `.adf` images. The build, the tests and the release are in
[Development](docs/development.md); the structure of the code in [Architecture](docs/architecture.md) and the
[coding standard](docs/coding-standard.md); the changes in the [changelog](CHANGELOG.md).

## How it was made

The port was written with Claude Code.

1. Decompile: Ghidra decompiles the FTL code modules of the Amiga disk to C
   ([Decompiling](docs/decompiling.md)). The symbol tables in `docs/reference/` name the functions and the globals.
2. Port: the game rules are rewritten in C11 from this decompilation only. No code of another port is used.
3. Check against the original: the differential tests run a function of the original program in a 68000
   emulator and the same function of the port with the same input, then compare the state. Golden snapshots
   keep the save file, the dungeon view and the game rules.
4. Clean up: several passes without a change of game behavior: named constants and typed accessors, the game
   rules out of the user interface, five layers, a coding standard with a check per rule, and an audit of the
   tests.

More: the [file formats](docs/formats/ftl.md), the [game rules](docs/rules/game-state.md),
[Chaos Strikes Back](docs/csb/data.md), and the [KAOS map](docs/reference/kaos-map.md).

## Acknowledgements

- [SDL3](https://github.com/libsdl-org/SDL): the window, the input and the audio.
- [dr_mp3](https://github.com/mackron/dr_libs) by David Reid: the MP3 decoder of the music.
- [amitools](https://github.com/cnvogelg/amitools): the ADF extraction and the reference of the ADF reader tests.
- [machine68k](https://github.com/cnvogelg/machine68k): the 68000 emulator of the differential tests.
- [ReDMCSB](http://dmweb.free.fr/community/redmcsb/) by Christophe Fontanel: many function and structure names of
  the decompilation follow its source files.
- [dmcsb](https://codeberg.org/CyCl0ne/DungeonMasterTools) by Cycl0ne: the reference of the `Dungeon.DAT`
  checks in the tests.

## License

GPL-3.0-or-later ([LICENSE](LICENSE)). Dungeon Master and Chaos Strikes Back are games of FTL Games. This
project is not affiliated with them. The game data is not included and is not covered by this license.

<p align="center"><img src="docs/images/ai-code.svg" width="200" alt="100 % of this code is AI"></p>
