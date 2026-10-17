# dm

A native C11/SDL3 port of Dungeon Master v3.60 (Amiga) for macOS and Windows. It is built from its own Ghidra decompilation of the original game.

This repository contains no game files. You supply your own disk images.

## Status

The full game of Dungeon Master and Chaos Strikes Back plays on macOS and Windows.
The LLM mode (a program plays through a Unix socket) runs on macOS.

## Quickstart

```sh
brew install sdl3
uv sync
make
build/dm --data game
make test
```

`game/` holds `KAOS.FTL`, `Graphics.DAT`, and `Dungeon.DAT`. If you put the `.adf` disk images in the current folder, the game extracts them at the first start.

## License

The code is GPL-3.0-or-later (`LICENSE`). The game data of Dungeon Master and Chaos Strikes Back is not included.

## Documentation

- [Player manual](docs/manual.md): run, options, controls, modes, saves, Windows
- [Development](docs/development.md): build, tests, layout, release
- [Decompiling](docs/decompiling.md): Ghidra tooling
- [Coding standard](docs/coding-standard.md)
- [Formats](docs/formats/ftl.md): [FTL](docs/formats/ftl.md), [Dungeon.DAT](docs/formats/dungeon.md), [save file](docs/formats/save.md)
- [Rules](docs/rules/game-state.md) of the game engine
- [Chaos Strikes Back](docs/csb/data.md)
- [LLM mode](docs/llm/llm-mode.md) and [LLM playbook](docs/llm/llm-playbook.md)
- [Reference](docs/reference/kaos-map.md): the KAOS map and the symbol notes in [docs/reference/symbols/](docs/reference/symbols)
