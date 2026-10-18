# Ideas for later

User wishes for future versions. Not planned yet, unless an item names its PRD.

## Release on GitHub

- Publish the source and the macOS and Windows builds on GitHub (the repository is private today).
- Version 1.0.0 (CHANGELOG), released on GitHub as `v1.0.0` with both zips while the repository is private. The disk install is tested end to end (2026-10-17): a clean folder with only the program and the three `.adf` disks extracts files equal to the reference data and starts DM and CSB, from the macOS zip and from the Windows zip under Wine CX; not on a real Windows.
- Left: make the repository public (owner command).
- The history holds no original game files (no `.adf`, `.ftl`, `.dat`, `game/` or `reference/` path was ever added; checked 2026-10-17).

## Docs review before the GitHub release

Findings of the docs review (2026-10-18). Nothing is changed yet. Order of work: stale content, then details, then duplicates; one commit per group.

### Two repositories (decided 2026-10-18)

The public `dm` holds only what the release needs: the build, the tests, the decompile tools, the play tools, and the docs. The research material goes to a private repository `dm-research`, cloned into a git-ignored path of `dm` (for example `research/`), as `reference/` is today: one working folder, two histories, two pushes. No submodule. No rewrite of the `dm` history (it holds no game files; checked 2026-10-17).

Moves to `dm-research`:

- `docs/superpowers/` whole: the PRDs, the plans, the checklists, `2026-10-16-final-refactor-inventory/`, `2026-10-17-test-audit.md`, and `archive/`. Three pages of `dm` link into it: `coding-standard.md`, `llm/llm-mode.md`, `reference/kaos-map.md`; `tests/standard/test_docs.py` names the broken links. `CLAUDE.md` says "Plans in `docs/superpowers/`": change it.
- `tools/research/` with its own `pyproject.toml` (`capstone`, `numpy`); remove the `research` dependency group from `pyproject.toml` and its row from `docs/decompiling.md`.
- `tools/dmtools/csb_match.py` and the script `dm-csb-match` (`pyproject.toml`); `docs/reference/csb-appb-symbols.csv` stays. Remove the rows in `docs/decompiling.md` and `docs/csb/engine-diff.md`.
- `docs/csb/port-integration.md` (a design record of done work).

Stays in `dm`: `docs/csb/data.md`, `engine-diff.md`, `new-adventure.md` (the source comments and 7 tests name them as the format source), `docs/reference/` (the CSV files feed `decompile-all.sh`; the symbol notes are linked from `docs/rules/`), `CLAUDE.md`, `.claude/agents/`, `playthroughs/template/`, `.clang-format`, `.editorconfig`, `.git-blame-ignore-revs`, `third_party/`, `uv.lock`.

### Stale and process content
- `docs/coding-standard.md`, intro: "during the final refactor ... until the run ends, the allowlist carries the difference". The refactor is done and `tests/standard/allowlist.txt` has no `open:` line. Rewrite the paragraph for the state after the refactor.
- `docs/csb/port-integration.md`: "Status: implemented", but "Open decisions" says "These points need the user's answer before the implementation", and "Order of work" is a pre-implementation list. It is a design record: move it to the archive, or remove the two sections.
- `docs/csb/data.md`: remove "Scratch scripts" (the scripts are not in the repository). Clean "Open points": the `MINI.DAT` format is in `new-adventure.md`, the CSB entrance is done, a real CSB save is tested.
- `docs/csb/engine-diff.md`: remove "Work estimate" (a pre-implementation estimate). In "Open questions", fold each "Answered:" item into the text and keep only the open ones.
- `docs/llm/llm-mode.md` mixes three page types: the protocol reference, the how-to (play with Claude Code), and the owner's run process (Goal of the runs, Runs and checkpoints, Coordinator as a subagent). The "Status" table is a progress table; "Integration in app.c" and "Tests" repeat the code and the test docstrings. Split: `llm-mode.md` keeps the protocol and the how-to; a new `docs/llm/llm-runs.md` takes the run process; remove Status, Integration in app.c, and Tests. `.claude/agents/*.md` name the sections: change them in the same commit.
- `docs/reference/kaos-map.md`: "a skeleton map for the naming work in Step 3b of the C port plan" points to an archived plan. Remove the sentence.
- `docs/todo.md`: after the release, remove "Release on GitHub" (done except the owner command).

### Details and personal paths

- `/Users/lego/github/dm` is in `docs/llm/llm-mode.md:45,51`, `docs/llm/llm-playbook.md:143`, and `docs/csb/new-adventure.md:201`. Write "the repository root" or `$(pwd)`.
- `~/github/wine-cross` is in `CLAUDE.md` and `docs/todo.md`: the owner's local tool. Keep or remove (owner decision).
- `CLAUDE.md` says "macOS port"; the port runs on macOS and Windows.
- Link texts with old file names: `docs/manual.md` "[save-format.md]", `docs/rules/creatures.md` "[rules-movement.md]".
- README "From a release", step 3 holds macOS and Windows in one numbered step. Split it.
- README "Documentation": the Formats line links `docs/formats/ftl.md` twice, as "Formats" and as "FTL".
- `pyproject.toml` has `version = "0.1.0"`; the CHANGELOG is at 1.0.0. Make them equal.
- `.gitignore`: `.ruff_cache/` and `.superpowers/` are ignored only by their own inner `.gitignore`. Add them.

### Duplicates

- README "Options" copies the block of the manual without the developer options; README "Develop" repeats the table of `development.md`. README keeps `build/dm --help` and a link to the manual; "Develop" keeps the four commands.
- `docs/manual.md` has developer content: "Run the game" (`make play`, the personal play folder), "Windows" (`make windows`, `make dist`), "Library" (`libdm.dylib`). `development.md` and `architecture.md` have it. The manual keeps only the player view (Windows has no LLM mode).
- The disk extraction is described in README, `docs/manual.md`, and `docs/decompiling.md`. One place (the manual); the others link.

### Missing for the release

- README: the system requirements in one place (the zip README says "Windows 10 or later", README.md does not), a link to `CHANGELOG.md`, a short "Acknowledgements" (SDL3, dr_mp3, amitools, machine68k, ReDMCSB, dmcsb), one screenshot.
- `docs/development.md` "Release" lacks: the version and the date in `CHANGELOG.md` (Unreleased holds the FILTER row), the tag `vX.Y.Z`, `gh release create` with both zips. The release repeats: write a runbook section or `tools/dist/release.sh`.
- The Windows build is tested only under Wine; README says "runs on Windows". One sentence in the release notes.

## Small cleanups after the clean-code pass

- `light_depths` (`src/engine/explored.c`): a fixed cut by palette index (4 -> 3 rows, 5 -> 2 rows); a finer rule if the LLM player needs it.

## Windows build: the sound crackles under Wine

- Found in the manual test (2026-10-17): `dm.exe` under the local Wine CX build (`~/github/wine-cross`) plays, saves and loads, but the sounds crackle and pop (wall bump, door). The macOS build sounds clean. Kairosoft games in the same Wine sound clean.
- Ruled out:
  - the game output: the SDL `disk` driver records clean audio from `dm.exe`;
  - a dry queue: a log in `pump_audio` (`src/platform/platform.c`) found no empty queue during play, and 6 queued ticks did not help;
  - the device rate: `SDL_AUDIO_FREQUENCY=48000` did not help;
  - CPU load: 15-20 % for `dm.exe`.
- Wine CX does not pass macOS environment variables to the Windows process; set them with `wine cmd /c "set NAME=value&& dm.exe ..."`.
- Next step: run `dm.exe` on a real Windows. If it crackles there, look at the SDL3 WASAPI path; if not, it is a Wine CX problem.
