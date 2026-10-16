# dm

@~/.claude/skills/c-decomp-port/SKILL.md
@~/.claude/skills/python-uv/SKILL.md

Native C11/SDL3 macOS port of Dungeon Master v3.60 (Amiga), built from its own Ghidra decompilation.

## Commands
- Build: `make` (game and test library, 0 warnings); `make windows` (MinGW-w64); `make clean`.
- Run: `build/dm`; personal play: `make play PLAY_DIR=playthroughs/lego`, then `playthroughs/lego/dm --data game --saves playthroughs/lego`.
- One test file: `uv run pytest -q tests/test_x.py`.
- Full suite: `make test` (`uv run pytest -q -n auto`).
- All checks: `make check` (format, standard, suite); `make format` before a commit.

## Layout
- `src/`: the C port in four layers (`data/`, `engine/`, `ui/`, `platform/`); every include names its layer
  (`#include "engine/game.h"`). The root `Makefile` builds `build/dm` and `build/libdm.dylib`.
- `third_party/dr_mp3/`: the vendored MP3 decoder.
- `tools/dmtools/`: Python tools: ADF extract, FTL loader, emulator harness, control client. `tools/ghidra/`,
  `tools/dist/`, `tools/research/` (not tested).
- `tests/`: differential tests against the original; `tests/dmtest/` shared helpers (`load_lib()`);
  `tests/test_standard.py` checks the coding standard.
- Git-ignored data: `game/`, `game-csb/` (original files), `reference/` (Ghidra output), `playthroughs/`. Link them into a new worktree.

## Conventions
- `docs/coding-standard.md` is the coding standard of the C port; follow it for every change in `src/`.
- Plans in `docs/superpowers/`; open work in `docs/todo.md`.
