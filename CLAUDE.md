# dm

@~/.claude/skills/c-decomp-port/SKILL.md
@~/.claude/skills/python-uv/SKILL.md

Native C11/SDL3 macOS port of Dungeon Master v3.60 (Amiga), built from its own Ghidra decompilation.

## Commands
- Build: `make -C port` (0 warnings).
- Run: `build/dm`; personal play: `playthroughs/lego/dm --data game --saves playthroughs/lego`.
- One test file: `uv run pytest -q tests/test_x.py`.
- Full suite: `uv run pytest -q` (`addopts` runs it in parallel).

## Layout
- `port/src/`: the C port in four layers (`data/`, `engine/`, `ui/`, `platform/`); `port/Makefile` builds `build/dm` and `build/libdm.dylib`.
- `tools/dmtools/`: Python tools: ADF extract, FTL loader, emulator harness, control client.
- `tests/`: differential tests against the original; `tests/test_standard.py` checks the coding standard.
- Git-ignored data: `game/`, `game-csb/` (original files), `reference/` (Ghidra output), `playthroughs/`. Link them into a new worktree.

## Conventions
- `docs/coding-standard.md` is the coding standard of the C port; follow it for every change in `port/src/`.
- Plans in `docs/superpowers/`; open work in `docs/todo.md`.
