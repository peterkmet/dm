# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Release on GitHub

- Left: make the repository public (owner command).
- The history holds no original game files (no `.adf`, `.ftl`, `.dat`, `game/` or `reference/` path was ever added; checked 2026-10-17).

## Small cleanups after the clean-code pass

- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.

## Windows build

- The sound crackles under Wine; test `dm.exe` on a real Windows (details are private notes).
