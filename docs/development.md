# Development

How to build, test, and release the C port. For the player side, see [manual.md](manual.md). For the structure of the code, see [architecture.md](architecture.md).

## Build

The port uses C11, `clang`, and SDL3. Install the tools first.

```sh
brew install sdl3
uv sync
```

| Command | Result |
|---|---|
| `make` | The game `build/dm` and the test library `build/libdm.dylib`. The build has 0 warnings. |
| `make lib` | The library only. It has no SDL code. |
| `make windows` | `build/dm.exe` for Windows x86-64, with MinGW-w64 (`brew install mingw-w64`). The first run downloads the SDL3 MinGW package into `build/sdl-mingw/`. Test it with Wine. |
| `make SANITIZE=1` | The build with the address and undefined-behavior sanitizers. |
| `make play PLAY_DIR=<dir>` | Copy a new build to `<dir>/dm`. The default is `playthroughs/local`. The saves in the folder stay. Run it with `<dir>/dm --data game --saves <dir>`. |
| `make clean` | Remove `build/`. |

Run the game with `build/dm`. The options are in [manual.md](manual.md).

## Tests

The tests need `clang` and SDL3 (Homebrew). `clang-format` 23 comes with `uv sync`; `pyproject.toml` pins it to version 23, because other versions format differently.

```sh
make test
```

`make test` runs `uv run pytest -q -n auto`. To run one file:

```sh
uv run pytest -q tests/<folder>/test_x.py
```

| Folder | What it proves |
|---|---|
| `tests/differential/` | The same input through the port (`ctypes` on `build/libdm.dylib`) and through the original in a 68000 emulator gives the same state. |
| `tests/port/` | The port code by itself: the data layer, the UI, the control protocol, the options, and the modes. |
| `tests/golden/` | The state, the view, the rules, the screens, and the protocol hash to the stored snapshots. Never regenerate a snapshot to make a test pass. |
| `tests/tools/` | The Python tools in `tools/dmtools/` and `decompile.sh`. |
| `tests/standard/` | The coding standard, the docs links, and the docs vocabulary. |

`tests/dmtest/` has the shared helpers: `load_lib()` loads a private copy of the library for a test module, `constants` mirrors C constants, and `markers` has the skip markers.

Some tests need files that the repository does not have. Pytest skips them with a marker:

| Marker | Needs |
|---|---|
| `needs_dm` | `game/` with `KAOS.FTL`, `Graphics.DAT`, and `Dungeon.DAT` |
| `needs_csb` | `game-csb/` with `APPB.FTL`, `Graphics.DAT`, and `Dungeon.DAT` |
| `needs_csb_mini` | `game-csb/disk-b/MINI.DAT` |
| `needs_sdl` | SDL3 from Homebrew |
| `needs_ghidra` | Ghidra from Homebrew (`tests/tools/test_ghidra.py` runs `decompile.sh` on a small synthetic image, about 4 seconds) |

Each test module owns its state, so the tests run in any order and in parallel.

To hide the format commit in `git blame`:

```sh
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## Layout

- `src/`: the C port in five layers. The public headers of a layer stand in `src/<layer>/include/`; an include
  names only the file, for example `#include "game.h"`.
  - `data/`: the files of the game and the tables.
  - `engine/`: the game rules, without SDL.
  - `ui/`: the screens, the panels, and the sound.
  - `platform/`: SDL3, the options, the application (`app.c`), and the entry point (`main.c`).
  - `control/`: the control protocol of the turn-based mode.
- `src/third_party/dr_mp3/`: the vendored MP3 decoder.
- `tools/dmtools/`: the Python tools. `tools/ghidra/`: the decompile scripts. `tools/dist/`: the release scripts.
- `tests/`: see the table above.
- `docs/`: the documentation.
- Git-ignored data: `game/`, `game-csb/` (original files), `reference/` (Ghidra output), `playthroughs/`.

## Coding standard

[coding-standard.md](coding-standard.md) is the coding standard of the C port. Follow it for every change in `src/`.

- `make format` formats the C files. Run it before a commit.
- `make check` runs `ruff`, `tests/standard` (with the format check), and the full suite.

## Release

1. In `CHANGELOG.md`, move the entries of "Unreleased" under a new heading `## X.Y.Z - YYYY-MM-DD`.
2. Set `version` in `pyproject.toml` to the same number. Run `uv lock`.
3. `make check`.
4. `make dist` builds `build/DungeonMaster.zip` (macOS: the game and SDL3, without game files).
5. `make dist-win` builds `build/DungeonMaster-win.zip` (`dm.exe` and `SDL3.dll`).
6. Commit `CHANGELOG.md`, `pyproject.toml`, and `uv.lock` with the message `docs: version X.Y.Z`.
7. `git tag vX.Y.Z`.
8. The owner pushes the branch and the tag.
9. Write the notes file: the `CHANGELOG.md` section of the version and the sentence "The Windows build is tested
   under Wine only, not on a real Windows."
10. The owner creates the release:

    ```sh
    gh release create vX.Y.Z build/DungeonMaster.zip build/DungeonMaster-win.zip --notes-file <notes>
    ```

### New zips of a released version

A change that keeps the version number replaces the zips of its release:

1. In `CHANGELOG.md`, add the entries to the section of the released version.
2. `make check`, `make dist`, `make dist-win`.
3. Commit `CHANGELOG.md`.
4. The owner pushes the branch, moves the tag to the new commit, and replaces the zips and the notes:

   ```sh
   git tag -f vX.Y.Z && git push -f origin vX.Y.Z
   gh release upload vX.Y.Z build/DungeonMaster.zip build/DungeonMaster-win.zip --clobber
   gh release edit vX.Y.Z --notes-file <notes>
   ```
