# dm

@~/.claude/skills/c-decomp-port/SKILL.md
@~/.claude/skills/python-uv/SKILL.md

Native C11/SDL3 macOS port of Dungeon Master v3.60 (Amiga), built from its own Ghidra decompilation.

## Commands
- Build: `make` (game and test library, 0 warnings); `make windows` (MinGW-w64); `make clean`.
- Run: `build/dm`; personal play: `make play`, then `playthroughs/local/dm --data game --saves playthroughs/local`.
- Socket test: `build/dm --mode turnbased --saves DIR --control /tmp/x.sock` (a short path: AF_UNIX holds about 100
  bytes), then `DM_CONTROL=/tmp/x.sock DM_PLAY_LOG_DIR=none uv run dm-play [--brief] <cmd>`; a picture:
  `dm-play screenshot '{"path":"..."}'`.
- Windows test: `make windows`, copy `SDL3.dll` from `build/sdl-mingw/SDL3-*/x86_64-w64-mingw32/bin/` next to
  `dm.exe`, run it with the local Wine CX: `~/github/wine-cross/dist/Wine CX.app/Contents/Resources/wine/bin/wine`,
  prefix `~/github/wine-cross/prefix`. Wine CX does not pass macOS environment variables: `wine cmd /c "set X=1&& dm.exe ..."`.
  Install no other Wine.
- One test file: `uv run pytest -q tests/<folder>/test_x.py`.
- Full suite: `make test` (`uv run pytest -q -n auto`).
- All checks: `make check` (format, standard, suite); `make format` before a commit.

## Layout
- `src/`: the C port in five layers, in this order; public headers in `src/<layer>/include/`, an include names only the file (`#include "game.h"`).
  The root `Makefile` builds `build/dm` and `build/libdm.dylib`.
  - `data/`: tables and formats of the original data files (no game state).
  - `engine/`: game rules and state; no drawing, no SDL.
  - `ui/`: screens, input handling, drawing, audio.
  - `control/`: the LLM protocol (state JSON, requests).
  - `platform/`: SDL3 window, events, application loop, transport (`transport.c`).
- `third_party/dr_mp3/`: the vendored MP3 decoder.
- `tools/dmtools/`: Python tools: ADF extract, FTL loader, emulator harness, control client. `tools/ghidra/`,
  `tools/dist/`.
- `tests/`: folders `differential/` (against the original), `port/`, `golden/` (snapshots), `tools/`, `standard/`
  (`tests/standard/test_standard.py` checks the coding standard); `tests/dmtest/` shared helpers (`load_lib()`).
- `docs/`: `architecture.md`, `development.md`, `coding-standard.md`, `todo.md`.
- Git-ignored data: `game/`, `game-csb/` (original files), `reference/` (Ghidra output), `playthroughs/`. Link them into a new worktree.
- `playthroughs/local/` is the owner's personal game: no agent reads, writes, moves or deletes anything there, and no
  agent stops its game. Only `make play` replaces `dm` there.

## Conventions
- `docs/architecture.md` describes the layers and modules.
- `docs/coding-standard.md` is the coding standard of the C port; follow it for every change in `src/`.
- Plans in `docs/superpowers/`; open work in `docs/todo.md`.
