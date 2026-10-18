# Repository split and docs cleanup before the public release: PRD

Date: 2026-10-18. Branch: `feature/2026-10-18-repo-split` (from `feature/2026-10-18-filter-option`, commit
`4ba4f82`). Status: planned.

## Goal

Before the owner makes `dm` public, `dm` holds only what a player or a developer of the port needs. The private
material (plans, research tools, LLM runs, the original game files, personal notes) goes to a new private
repository `dm-research`. The docs get the cleanup that the docs review of 2026-10-18 found (`docs/todo.md`,
section "Docs review before the GitHub release"), and the README gets what the release lacks.

The findings list in `docs/todo.md` is the source of each docs change. A claim without a `file:line` source is an
assumption; the plan task that relies on it checks it first.

## Scope

In: the split (layout, moves, links, `.gitignore`, `pyproject.toml`, `CLAUDE.md`), every finding of the todo
section "Docs review before the GitHub release", the release runbook, one screenshot in the README, and the two
source comments that point into `docs/superpowers/` (R2).

Out:
- making `dm` public, creating the GitHub repository `dm-research`, and every push (owner commands);
- `playthroughs/local/`: the owner's game. No agent reads, writes, moves or deletes anything there;
- `reference/`: it stays local and git-ignored;
- the todo item "Small cleanups after the clean-code pass", and code changes in `src/` other than the two comments.

## Requirements

### R1. The private repository `dm-research`

- `dm-research` is a git repository in `research/` of the `dm` working folder. `dm` ignores `/research/`. One
  working folder has two histories. No submodule. No rewrite of the `dm` history.
- Layout:

  ```
  research/
    README.md            what it is, how to clone it into dm/, the links that dm needs, how to run the tools
    .gitignore           .venv/, __pycache__/, .DS_Store
    pyproject.toml       the research tools: capstone, numpy, dmcsb, and dmtools from the parent folder
    superpowers/         was docs/superpowers/ of dm (PRDs, plans, checklists, inventory, test audit, archive)
    docs/port-integration.md   was docs/csb/port-integration.md
    tools/csb_match.py   was tools/dmtools/csb_match.py
    tools/platforms/     was tools/research/platforms/; the text of tools/research/README.md goes into README.md
    notes.md             owner notes: the local Wine CX and the open Windows sound crackle (from docs/todo.md)
    playthroughs/run3/ run4/ run5/   the finished LLM runs; they were git-ignored in dm
    game/ game-csb/      the original game files; they were git-ignored in dm
  ```

- `tools/csb_match.py` reads its default paths from the current folder (`tools/dmtools/csb_match.py:412-416`).
  `research/README.md` says: run it from the `dm` root with
  `uv run --project research python research/tools/csb_match.py`.
- The first commit of `dm-research` holds the moved files.
- No file of `dm-research` is larger than 100 MB. GitHub refuses larger files.

### R2. `dm` after the split

- Before the agent moves a git-ignored folder, it copies `game/`, `game-csb/` and `playthroughs/run3..run5` to a
  backup folder outside the working folder. The agent removes the moved tracked files with `git rm` and moves the
  git-ignored folders on disk.
- `game` and `game-csb` in `dm` become symbolic links to `research/game` and `research/game-csb`.
  `playthroughs/current` points to `../research/playthroughs/run5`. The code, the tests and the docs keep the
  paths `game/` and `game-csb/`. A public user puts the files of the own disks there.
- `.gitignore`: `/game` and `/game-csb` replace `game/` and `game-csb/` (`.gitignore:2,9`). A pattern without the
  slash matches a folder and a symbolic link. New lines: `/research/`, `.ruff_cache/`, `.superpowers/`.
- `pyproject.toml`: no `research` dependency group, no `dm-csb-match` script. `version` is `1.0.0`, the last
  released version in `CHANGELOG.md`.
- `CLAUDE.md`: plans live in `research/superpowers/` (private and optional); the layout lists `research/`; the
  worktree note links `research/` too; no `tools/research/`; "macOS and Windows port"; the Windows test names
  Wine and points to `research/notes.md` for the local Wine CX. `CLAUDE.md` names `research/` paths as code text,
  never as Markdown links, because `tests/standard/test_docs.py` checks only tracked targets.
- `docs/development.md:77` no longer lists `tools/research/`.
- Every tracked file that links or points into `docs/superpowers/` or to a moved file names nothing private: the
  text states the fact, or the link goes. This includes the source comments `src/ui/include/advanced.h:9` and
  `src/ui/include/music.h:8`.
- `tests/standard/test_docs.py` checks all docs; the `superpowers` exception goes in the same commit as the move
  of `docs/superpowers/`. Every link resolves.

### R3. Stale and process content (todo findings)

Each item of the todo group "Stale and process content", with these rulings:
- `docs/csb/port-integration.md` moves to `dm-research` (R1). The move closes the todo item about its sections.
- The new page `docs/llm/llm-runs.md` takes "Goal of the runs", "Runs and checkpoints", and "Coordinator as a
  subagent". `.claude/agents/dm-coordinator.md` names the new page.
- "Start a new run with the memory of an earlier run" (`docs/llm/llm-mode.md:89`) says "an earlier run folder".
  The next run number continues the archive (run6).
- The tool of the finished refactor goes: `tests/standard/open_lines.py` (it writes `open: epic N` lines) and the
  `open: epic N` reason in `test_allowlist_names_documented_reasons` (`tests/standard/test_standard.py:23,618-629`).
  `tests/standard/allowlist.txt` has no `open:` line, so the check gets stricter and stays green.

### R4. Details and personal paths (todo findings)

Each item of the todo group "Details and personal paths". `~/github/wine-cross` leaves `CLAUDE.md` and
`docs/todo.md` and goes to `research/notes.md`.

### R5. Duplicates (todo findings)

Each item of the todo group "Duplicates".

### R6. Missing for the release (todo findings)

- README: the system requirements in one place, a link to `CHANGELOG.md`, "Acknowledgements" (SDL3, dr_mp3,
  amitools, machine68k, ReDMCSB, dmcsb), and one screenshot `docs/images/screenshot.png`. The agent makes the
  screenshot with `dm-play screenshot` from a test game, not from `playthroughs/local/`.
- `docs/development.md` "Release": a runbook section (version and date in `CHANGELOG.md`, the `pyproject.toml`
  version, both zips, tag `vX.Y.Z`, `gh release create` with both zips), or `tools/dist/release.sh` when the
  steps are more than a short list.
- The README says that the Windows build is tested under Wine only.

### R7. `docs/todo.md` after the work

- The section "Docs review before the GitHub release" goes.
- "Release on GitHub" keeps only the open owner step (make the repository public) and the history fact.
- "Windows build: the sound crackles under Wine" moves to `research/notes.md`. `docs/todo.md` keeps one line: the
  sound crackles under Wine; test on a real Windows.

## Invariants

- `git check-ignore research game game-csb` matches all three.
- `git ls-files | grep -E 'superpowers|tools/research|csb_match|port-integration'` is empty.
- `git grep -E '/Users/lego|wine-cross'` is empty.
- The `pyproject.toml` version equals the last released version in `CHANGELOG.md`.
- `make check` is green with and without `research/`.

## Acceptance

```gherkin
Scenario: a fresh clone of dm builds and tests without research/
  Given a clone of dm with game/ and game-csb/ filled from the owner's disks and no research/ folder
  When the developer runs make and make check
  Then the build has 0 warnings and the suite is green

Scenario: the owner's working folder after the split
  Given research/ is the dm-research clone and game, game-csb, playthroughs/current are the links
  When the owner runs make check
  Then the suite is green and git status of dm shows nothing from research/

Scenario: no private material in dm
  When the owner lists git ls-files of dm
  Then the Invariants hold
```

## Rollback

Remove the links `game`, `game-csb`, `playthroughs/current`; move `research/game`, `research/game-csb`,
`research/playthroughs/run*` back (or restore them from the backup folder); `git revert` the split commits.

## Known consequences

- The `dm` history keeps the moved files: `docs/superpowers/`, `tools/research/`, `csb_match.py`, the personal
  paths. After the owner makes `dm` public, anyone can read them in `git log`. The history holds no original game
  files (`docs/todo.md`, checked 2026-10-17).

## Decisions

- `game/`, `game-csb/` stay as paths in `dm` through symbolic links. The code and the tests keep working, and a
  public user fills the same paths. The owner chose that the original files go to `dm-research`.
- `reference/` stays local and git-ignored. It holds 1 GB with `.7z` files, too large for GitHub, and the tools
  make it again.
- `docs/reference/` (CSV symbol tables, symbol notes) stays in `dm`. `tools/ghidra/decompile-all.sh` reads the CSV
  files, and `docs/rules/` links the notes (`docs/todo.md`, Two repositories).
- `docs/csb/data.md`, `engine-diff.md`, `new-adventure.md`, `.claude/agents/`, `playthroughs/template/` stay
  (`docs/todo.md`, Two repositories).
- The research tools import `dmtools` (`from dmtools import ...` in `tools/research/platforms/`). The research
  `pyproject.toml` takes `dmtools` as a path dependency on the parent folder.
- New LLM runs start in `playthroughs/run<N>/` of `dm` as before (`docs/llm/llm-mode.md:88`). The owner moves a
  finished run to `research/playthroughs/`. The public docs do not describe the archive.
- No rewrite of the `dm` history: the owner decided it on 2026-10-18 (`docs/todo.md`, Two repositories).
- The `pyproject.toml` version is `1.0.0`. The FILTER row stays under "Unreleased"; the release runbook raises
  both versions at the next release.
- `CHANGELOG.md` history lines stay as they are, also where they name moved files (`CHANGELOG.md:98`).
- The branch starts from the filter branch. The owner chose it: the filter commit goes to `main` with this work.
- The agent makes the README screenshot. The owner chose it.
- The local Wine CX notes move to `research/notes.md`. The owner chose it.
- The PRD and the plan of this work move with `docs/superpowers/` in the last task.
