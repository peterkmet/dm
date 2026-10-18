# Repository split and docs cleanup: plan

PRD: `docs/superpowers/2026-10-18-repo-split.md`. Branch: `feature/2026-10-18-repo-split`.

**Goal:** `dm` keeps only what the public release needs; the private material goes to `dm-research` in
`research/`; the docs get the cleanup of the todo findings and the release additions.

**Architecture:** two git repositories in one working folder. `dm` ignores `/research/`. `dm-research` holds the
moved files; `dm` reaches its data through the git-ignored links `game`, `game-csb`, `playthroughs/current`.

## Global Constraints

- Project rules: `CLAUDE.md` of the repository and `~/github/CLAUDE.md`. Docs follow the docs skill
  (`~/.claude/skills/docs/SKILL.md`) and the ASD-STE100 style: short active sentences, one term per concept.
- `playthroughs/local/` is the owner's game. No agent reads, writes, moves or deletes anything there, and no agent
  stops a `dm` process that it did not start.
- No push, no `gh repo create`, no merge. The owner does them.
- Commits: Conventional Commits, one type per commit, one logical change per commit. `dm` and `dm-research` get
  their own commits (`git -C research ...`). Run `make format` before a commit that changes `src/` or `tests/`.
- The author runs the build (`make`) and only the tests of the changed code, never the full suite
  (agentic-workflow, Author tests). Plan decision: for a docs change, the test of the changed code is
  `uv run pytest -q tests/standard/test_docs.py`.
- The branch `feature/2026-10-18-repo-split` exists; its first commit holds the PRD, this plan and the todo
  findings (made at plan time).
- Do not download or run third-party code (libraries, CDN files, packages outside the lock file) to check
  behavior; report what you could not verify.
- Keep existing comments; a new comment is one line and says what the code does.
- `CHANGELOG.md` history lines stay as they are.
- Test list (`~/github/CLAUDE.md` §2): write a test only for a check against a reference (original program,
  golden snapshot), a parser or data format, a rule with more than one outcome that no reference check or type
  check covers (dates, parsing, prices, state machines), a regression of a real bug (one test per bug). No tests for
  constants and data tables; wiring, scaffold, CLI arguments, logging; what the compiler, a linter or a standard
  check catches; UI and layout; what another test already covers. No mutation tests. This plan adds no test.
- Weakened assertions: none. Task 4 makes `test_allowlist_names_documented_reasons` stricter; Task 13 makes
  `test_doc_links_resolve` check more files. Approved by the owner: (pending).

## Sourced facts

- `.gitignore:2` `game/`, `.gitignore:9` `game-csb/`, `.gitignore:7` `/reference/`, `.gitignore:10-12`
  playthroughs rules. A pattern with a trailing slash does not match a symbolic link (git docs, gitignore
  "PATTERN FORMAT").
- `playthroughs/current` is a link to `run5` (`ls -la playthroughs`, 2026-10-18).
- `game/` 104 MB, `game-csb/` 45 MB; no file larger than 90 MB (`find game game-csb -size +90M` is empty,
  2026-10-18).
- `pyproject.toml:14` `dm-csb-match`; `pyproject.toml:39-42` the `research` group; `pyproject.toml:3`
  `version = "0.1.0"`.
- The research scripts set `ROOT` three folders up (`tools/research/platforms/dm2gdat.py:3`); in
  `research/tools/platforms/` three folders up is still the `dm` root.
- The scripts in `tools/research/platforms/` import `dmtools.csb_mini`, `dmcsb` and `capstone`;
  `tools/dmtools/csb_match.py:33,35` imports `machine68k` and `dmtools.ftl`.
- `tools/dmtools/csb_match.py:412-416`: default paths relative to the current folder.
- References to moved material: `CLAUDE.md:32,43`, `docs/coding-standard.md:9-11`, `docs/llm/llm-mode.md:5`,
  `docs/reference/kaos-map.md:5`, `src/ui/include/advanced.h:9`, `src/ui/include/music.h:8`,
  `tests/standard/test_docs.py:2,13`, `docs/csb/engine-diff.md:19-22`, `docs/decompiling.md:58,61`,
  `docs/development.md:77` (`git grep`, 2026-10-18).
- Personal paths: `CLAUDE.md:15-16`, `docs/csb/new-adventure.md:201`, `docs/llm/llm-mode.md:45,51`,
  `docs/llm/llm-playbook.md:143`.
- `docs/llm/llm-mode.md`: Status 7-17, Play with Claude Code 18-57 (line 56 links `#runs-and-checkpoints`), Goal of
  the runs 58-67, Runs and checkpoints 68-96, Coordinator as a subagent 97-111, Integration in app.c 527-539, Tests
  546-end. `.claude/agents/dm-coordinator.md:9,11` name the sections and "Resume" of llm-mode.md.
- `docs/csb/data.md`: Scratch scripts 319-331, Open points 332-end. `docs/csb/engine-diff.md`: Work estimate
  436-454, Open questions 455-end (items 2, 3, 4, 6 "Answered").
- `CLAUDE.md:6` "Native C11/SDL3 macOS port"; `CLAUDE.md:14-17` the Windows test with the local Wine CX and
  "Install no other Wine".
- `docs/coding-standard.md:318` the `allowlist` row names `open: epic N`; `tests/standard/test_standard.py:3-5` the
  module docstring names `open_lines.py`.
- `docs/csb/engine-diff.md:19-34`: the `csb_match.py` tool, its command, its report and its steps.
- The game process writes the screenshot PNG (`src/control/control.c:131-137`); a relative path resolves against
  the folder where `build/dm` runs.
- `docs/manual.md`: Run the game 5-22 (automatic extraction 13, `.gitignore` sentence 15, personal game 17-21), Windows 218-225, Library 226-end; the link text
  `[save-format.md]` at 98. `docs/rules/creatures.md:3,133` `[rules-movement.md]`.
- README: From a release 37-51, Options 75-89, Develop 104-114, Documentation 116-127 (Formats line 123).
- `docs/development.md`: Build table 12-21 (has `make windows`, `make play`), Release 89-end (4 steps).
- `tools/dist/make-dist-win.sh:13` "Windows 10 or later".
- `tests/standard/test_standard.py:23` `OPEN_REASON`; 618-629 the allowlist check; `tests/standard/allowlist.txt`
  has no `open:` line.

### Task 1: dm-research skeleton and the original data

Files: `research/` (new repository), `.gitignore`, links `game`, `game-csb`, `playthroughs/current`.

1. Back up: `mkdir -p ~/dm-split-backup && cp -a game game-csb playthroughs/run3 playthroughs/run4 playthroughs/run5 ~/dm-split-backup/`. Check with `du -sh` that the sizes match.
2. `mkdir research && git -C research init -b main`. Write `research/.gitignore`: `.venv/`, `__pycache__/`, `.DS_Store`.
3. `mv game research/game && mv game-csb research/game-csb && mkdir research/playthroughs && mv playthroughs/run3 playthroughs/run4 playthroughs/run5 research/playthroughs/`.
4. Links: `ln -s research/game game && ln -s research/game-csb game-csb && ln -sfn ../research/playthroughs/run5 playthroughs/current`. Check `ls game/KAOS.FTL game-csb/APPB.FTL playthroughs/current/player`.
5. `.gitignore` of dm: replace `game/` with `/game` and `game-csb/` with `/game-csb`; add `/research/`, `.ruff_cache/`, `.superpowers/`. Check: `git check-ignore -v game game-csb research .ruff_cache .superpowers` names a rule for each; `git status --short` shows only `.gitignore`.
6. Write `research/README.md`: what `dm-research` is (the private material of the `dm` port: plans, research tools, LLM runs, original game files, notes); how to set it up: `git clone <url> research` in the `dm` root, then the three `ln -s` commands of step 4; a section "Contents" with one line per top folder (fill later tasks' folders as they arrive: list only what exists).
7. `uv run pytest -q tests/tools/test_extract.py tests/port/test_port_data.py` (they read `game/` through the link).
8. Commit research: `git -C research add -A && git -C research commit -m "chore: original game files, LLM runs 3-5, README"`. Check `git -C research ls-files | wc -l` and that no file is over 100 MB.
9. Commit dm: `chore: ignore research/ and the data links`.

### Task 2: research tools move to dm-research

Files: `tools/research/`, `tools/dmtools/csb_match.py`, `pyproject.toml`, `uv.lock`, `research/pyproject.toml`,
`research/README.md`, `docs/decompiling.md`, `docs/csb/engine-diff.md`, `docs/development.md`, `CLAUDE.md`.

1. `mkdir -p research/tools && git mv` is not possible across repositories: copy `tools/research/platforms` to `research/tools/platforms` and `tools/dmtools/csb_match.py` to `research/tools/csb_match.py`, then `git rm -r tools/research tools/dmtools/csb_match.py`.
2. Fold the text of `tools/research/README.md` into a section "Research tools" of `research/README.md`. Add: run `csb_match.py` from the `dm` root with `uv run --project research python research/tools/csb_match.py` (its default paths are relative to the current folder); its usage line in the docstring (`csb_match.py:4`) names this command instead of `uv run dm-csb-match`.
3. Write `research/pyproject.toml`: `[project] name = "dm-research"`, `version = "0.1.0"`, `requires-python = ">=3.14"`, dependencies `capstone>=5.0.9`, `numpy>=2.5.4`, `dmcsb>=0.2.1`, `dmtools`; `[tool.uv.sources] dmtools = { path = "..", editable = true }`; `[tool.uv] package = false`. Run `uv lock --project research` (a lock of packages that `dm` already pins or that the old `research` group named; no new package). If it fails, report and stop.
4. `pyproject.toml` of dm: remove the line `dm-csb-match = ...` and the `research` group. Run `uv lock` and `uv sync`.
5. `docs/decompiling.md`: remove the rows at 58 and 61. `docs/csb/engine-diff.md:19-34`: say that `docs/reference/csb-appb-symbols.csv` holds the CSB-to-DM function mapping; keep the method (steps 1-6) as a description of how the mapping was made, without the tool name, the command, the report file and the tool tables (`MANUAL_PAIRS`, `CSB_GLOBALS` become "a list of pairs that a code reading proved", "verified CSB globals"). `docs/development.md:77`: remove "`tools/research/`: research scripts (not tested)". `CLAUDE.md:32`: remove "`tools/research/` (not tested)".
6. `uv run pytest -q tests/standard/test_docs.py` and `uv run ruff check`.
7. Commit research: `chore: research tools from dm`. Commit dm: `chore: move the research tools to dm-research`.

### Task 3: the CSB port design record moves

Files: `docs/csb/port-integration.md`, `research/docs/port-integration.md`.

1. `git grep -n port-integration` outside `docs/superpowers/` and `docs/todo.md`: each hit loses its link (none expected).
2. Copy the page to `research/docs/port-integration.md`; `git rm docs/csb/port-integration.md`.
3. Add the page to "Contents" of `research/README.md`.
4. `uv run pytest -q tests/standard/test_docs.py`.
5. Commit research: `docs: CSB port design record`. Commit dm: `docs: move the CSB port design record to dm-research`.

### Task 4: coding standard after the refactor

Files: `docs/coding-standard.md`, `tests/standard/test_standard.py`, `tests/standard/open_lines.py`.

1. `docs/coding-standard.md:5-11`: keep the first sentence about the bracket names. Rewrite the rest: the allowlist holds the accepted exceptions, one per line `<check> <target> <reason>`; a reason is `rule <n.m>: <text>` for an exception that this document names. No PRD link, no refactor words.
2. `docs/coding-standard.md:318`: the `allowlist` row says "an allowlist line with a reason that this document does not name".
3. `tests/standard/test_standard.py`: remove `OPEN_REASON` (line 23) and its use in `test_allowlist_names_documented_reasons`; the docstring says "Each allowlist line is an exception rule of the standard."; the module docstring (3-5) drops "tests/standard/open_lines.py uses both."
4. `git rm tests/standard/open_lines.py`; `git grep -n "open_lines\|open: epic" -- ':!docs/superpowers'` is empty.
5. `make format`; `uv run pytest -q -n auto tests/standard/test_standard.py tests/standard/test_docs.py`.
6. Commit: `docs: the coding standard describes the code after the refactor`, then `test: the allowlist accepts only exception rules` (with the removed tool).

### Task 5: stale content of the CSB pages

Files: `docs/csb/data.md`, `docs/csb/engine-diff.md`.

1. `docs/csb/data.md`: remove "Scratch scripts" (319-331).
2. `docs/csb/data.md` "Open points": remove the `MINI.DAT` item (the format is in `docs/csb/new-adventure.md`), the entrance item (the CSB entrance is done: `docs/manual.md` Chaos Strikes Back), and the real-save item (check first: `git grep -n CSBGAME tests/` names a test with a real CSB save; if none, keep the item). Keep the others.
3. `docs/csb/engine-diff.md`: remove "Work estimate" (436-454).
4. `docs/csb/engine-diff.md` "Open questions": for items 2, 3, 4, 6, check that the linked section holds the answer; add the missing fact to that section; remove the item. Item 1 (game start without `CEDT.FTL`): the port builds the new adventure from `MINI.DAT` (`docs/csb/new-adventure.md`); fold it the same way. Keep item 5. Renumber.
5. `uv run pytest -q tests/standard/test_docs.py`.
6. Commit: `docs: remove done work and answered questions from the CSB pages`.

### Task 6: LLM mode page split

Files: `docs/llm/llm-mode.md`, `docs/llm/llm-runs.md` (new), `docs/llm/llm-playbook.md`,
`.claude/agents/dm-coordinator.md`.

1. Move "Goal of the runs", "Runs and checkpoints", "Coordinator as a subagent" (58-111) to the new `docs/llm/llm-runs.md` with a title and one intro sentence (how-to: an LLM run with a coordinator and players). Keep the text; adapt heading levels.
2. In the moved "Start a new run with the memory of an earlier run": "the files of an earlier run folder". Add one sentence to "Start a new run N": the run number continues after the last run.
3. `llm-mode.md`: remove line 5 (the plan link), "Status" (7-17), "Integration in app.c" (527-539), "Tests" (546-end). Line 56: link `llm-runs.md` instead of `#runs-and-checkpoints`. Any other anchor to a moved section links into `llm-runs.md`.
4. Personal paths: `llm-mode.md:45,51` `uv --directory /Users/lego/github/dm` becomes `uv --directory "$(pwd)"` with one sentence "Run it from the repository root."; `llm-playbook.md:143` path becomes `playthroughs/current/coordinator/shot.png`, with the note: the path is relative to the folder where the game runs (the repository root in a run).
5. `.claude/agents/dm-coordinator.md:9,11`: name `docs/llm/llm-runs.md` for the three sections and "Resume".
6. `git grep -n "Runs and checkpoints\|Goal of the runs\|Coordinator as a subagent" -- ':!docs/superpowers' ':!docs/todo.md'` names only `llm-runs.md` and the agent file.
7. `uv run pytest -q tests/standard/test_docs.py`.
8. Commit: `docs: split the LLM run process from the LLM mode reference`.

### Task 7: last links into the plans

Files: `docs/reference/kaos-map.md`, `src/ui/include/advanced.h`, `src/ui/include/music.h`.

1. `docs/reference/kaos-map.md:5`: remove the sentence "It is a skeleton map ... C port plan."
2. `src/ui/include/advanced.h:9`: remove "Plan: docs/superpowers/archive/2026-10-04-modes-plan.md."; keep the sentence before it and the comment end.
3. `src/ui/include/music.h:8`: remove the line "Decisions: docs/superpowers/archive/2026-10-05-music-decisions.md." and keep the comment closed.
4. `git grep -n superpowers -- ':!docs/superpowers' ':!docs/todo.md'` names only `CLAUDE.md:43`, `tests/standard/test_docs.py` (Task 13) and the `.superpowers/` line of `.gitignore`.
5. `make format && make`; `uv run pytest -q -n auto tests/standard/test_standard.py tests/standard/test_docs.py`.
6. Commit: `docs: remove the links into the private plans`.

### Task 8: details, personal paths and the Wine notes

Files: `CLAUDE.md`, `docs/csb/new-adventure.md`, `docs/manual.md`, `docs/rules/creatures.md`, `README.md`,
`pyproject.toml`, `research/notes.md`, `research/README.md`.

1. Write `research/notes.md`: section "Local Wine CX" with the binary and prefix paths and the `wine cmd /c "set X=1&& dm.exe ..."` note (`CLAUDE.md:15-16`); section "Windows sound crackles under Wine" with the whole todo section (`docs/todo.md`, "Windows build: the sound crackles under Wine"). Add it to "Contents" of `research/README.md`.
2. `CLAUDE.md:6`: "Native C11/SDL3 macOS and Windows port". Windows test (14-17): `make windows`, copy `SDL3.dll` next to `dm.exe`, run it with Wine; Wine does not pass macOS environment variables (`wine cmd /c "set X=1&& dm.exe ..."`); the local Wine CX setup is in `research/notes.md` (code text, no link). "Install no other Wine" belongs to the local Wine CX: it moves to `research/notes.md`.
3. `docs/csb/new-adventure.md:201`: "Run the commands from the repository root:".
4. `docs/manual.md:98` link text `[save-format.md]` becomes `[save file format]`; `docs/rules/creatures.md:3,133` `[rules-movement.md]` becomes `[movement.md]`.
5. README "From a release": step 3 becomes "3. macOS: ..." and "4. Windows: ..." (two numbered steps).
6. README "Documentation" line 123: `[Formats](docs/formats/ftl.md): [FTL]...` becomes "Formats: [FTL](...), [Dungeon.DAT](...), [save file](...)".
7. `pyproject.toml:3`: `version = "1.0.0"`; `uv lock`.
8. `git grep -nE '/Users/lego|wine-cross' -- ':!docs/superpowers' ':!docs/todo.md'` is empty.
9. `uv run pytest -q tests/standard/test_docs.py`.
10. Commit research: `docs: Wine CX and sound notes`. Commit dm: `docs: remove personal paths and fix link texts`, then `chore: version 1.0.0 in pyproject.toml`.

### Task 9: duplicates between README, manual and development

Files: `README.md`, `docs/manual.md`, `docs/development.md`, `docs/decompiling.md`, `docs/architecture.md`.

1. Disk extraction in one place: `docs/manual.md` "Run the game" keeps the automatic extraction (line 13) and gets the hand extraction (the `uv run dm-extract` commands of `docs/decompiling.md:25-45`, in a short subsection "Extract the disks by hand"). `docs/decompiling.md` Quickstart steps 3 and 5 keep the commands they need for decompiling but link to the manual for the folder layout; `docs/decompiling.md:50` says `/game` and `/game-csb` as in `.gitignore`. README "Get the game data" keeps the disk list and one sentence plus a link to the manual.
2. `docs/manual.md`: remove the developer content: line 15 (`.gitignore` sentence), 17-21 (personal game, `make play`), "Windows" 218-225 except the player facts (the Windows build has no LLM mode; the mode screen offers VANILLA and ADVANCED) which go to "Modes" or "Start screens", and "Library" (226-end). Check first that `docs/development.md` has `make windows`, `make dist`, `make dist-win`, `make play` and that `docs/architecture.md` describes `libdm.dylib`; add a missing one there.
3. README "Options": replace the block with `build/dm --help` and a link to the manual options section.
4. README "Develop": keep the four commands, remove the layer paragraph, link `docs/development.md`.
5. README Documentation line for the manual: remove "Windows" from the topics if the manual no longer has the section.
6. `uv run pytest -q tests/standard/test_docs.py`.
7. Commit: `docs: one place for the options, the disk extraction and the developer commands`.

### Task 10: release additions to README and development

Files: `README.md`, `docs/development.md`, `tools/dist/` (only if a script is chosen).

1. README: a section "System requirements" after "What you get": macOS on Apple Silicon (check the minimum macOS from `Makefile` or `tools/dist/make-dist.sh`; if none is set, write "a current macOS"), Windows 10 or later x86-64 (`tools/dist/make-dist-win.sh:13`), the disk images; one sentence: the Windows build is tested under Wine only, not on a real Windows.
2. README: a link to `CHANGELOG.md` in "Documentation" ("Changes").
3. README: a section "Acknowledgements" before "License": SDL3, dr_mp3 (`third_party/dr_mp3/`), amitools and machine68k (`pyproject.toml` dependencies), ReDMCSB (names and data formats), dmcsb (`pyproject.toml` dev group). One line each: what the project uses it for. Use only facts from the repository.
4. `docs/development.md` "Release": a numbered runbook: move the "Unreleased" entries of `CHANGELOG.md` under `## X.Y.Z - YYYY-MM-DD`; set `version` in `pyproject.toml` to the same number and run `uv lock`; `make check`; `make dist` and `make dist-win`; commit `docs: version X.Y.Z`; `git tag vX.Y.Z`; push the branch and the tag (owner); `gh release create vX.Y.Z build/DungeonMaster.zip build/DungeonMaster-win.zip --notes-file <notes>` with the CHANGELOG section as notes and the Wine sentence. The steps are a short list: no `release.sh`.
5. `uv run pytest -q tests/standard/test_docs.py`.
6. Commit: `docs: system requirements, acknowledgements and the release runbook`.

### Task 11: README screenshot

Files: `docs/images/screenshot.png` (new), `README.md`.

1. `make`. `tmp=$(mktemp -d)`; copy `playthroughs/template/DMGAME.DAT` to `$tmp/`.
2. Start: `SDL_AUDIO_DRIVER=dummy build/dm --mode turnbased --saves "$tmp" --control /tmp/dmshot.sock` in the background; remember its PID.
3. `DM_CONTROL=/tmp/dmshot.sock DM_PLAY_LOG_DIR=none uv run dm-play load`, then a few moves that show a corridor of level 1 (`dm-play act move_forward`; check with `dm-play --brief get_state` that the party stands in level 1 with a view down a corridor).
4. `mkdir -p docs/images`; `DM_CONTROL=/tmp/dmshot.sock DM_PLAY_LOG_DIR=none uv run dm-play screenshot '{"path":"'"$PWD"'/docs/images/screenshot.png"}'`. Look at the PNG (Read tool): the dungeon view and the party panel show, no debug text.
5. Stop only the started process (`kill <PID>`); remove `$tmp` and the socket.
6. README: the image after the first paragraph: `![Dungeon Master in the port: a corridor of level 1](docs/images/screenshot.png)`.
7. `uv run pytest -q tests/standard/test_docs.py`.
8. Commit: `docs: README screenshot`.

### Task 12: todo after the work

Files: `docs/todo.md`.

1. Remove the section "Docs review before the GitHub release" whole.
2. "Release on GitHub": keep "Left: make the repository public (owner command)." and the history fact; remove the other lines.
3. "Windows build: the sound crackles under Wine": replace the section with one item under a heading "Windows build": the sound crackles under Wine; test `dm.exe` on a real Windows (details are private notes). No path to `research/`.
4. `git grep -nE '/Users/lego|wine-cross'` is empty outside `docs/superpowers/`.
5. Commit: `docs: todo after the repository split`.

### Task 13: the plans move to dm-research

Files: `docs/superpowers/`, `research/superpowers/`, `tests/standard/test_docs.py`, `CLAUDE.md`,
`research/README.md`.

1. Copy `docs/superpowers/` to `research/superpowers/` (all files, also this plan and the PRD); `git rm -r docs/superpowers`.
2. `tests/standard/test_docs.py`: docstring "every relative Markdown link in the docs resolves"; `markdown_files` takes all of `docs/` (remove the `superpowers` filter).
3. `CLAUDE.md`: "Plans in `research/superpowers/` (the private repository `dm-research`, cloned into the git-ignored `research/`); open work in `docs/todo.md`." Layout: a line for `research/` (private, optional: plans, research tools, LLM runs, original game files); the git-ignored data line names `research/` and says that `game` and `game-csb` link into it; the worktree note links `research/` too. Code text only, no Markdown links into `research/`.
4. Add "superpowers/" to "Contents" of `research/README.md`.
5. `make format`; `uv run pytest -q tests/standard/test_docs.py tests/standard/test_standard.py`.
6. Invariants of the PRD: `git check-ignore research game game-csb`; `git ls-files | grep -E 'superpowers|tools/research|csb_match|port-integration'` empty; `git grep -nE '/Users/lego|wine-cross'` empty; `pyproject.toml` version 1.0.0 equals the last released CHANGELOG version.
7. Commit research: `docs: plans and PRDs from dm`. Commit dm: `chore: move the plans to dm-research`.

### Task 14: check a fresh clone without research/

Controller step, no author. Checks the PRD Acceptance "a fresh clone of dm builds and tests without research/".

1. `tmp=$(mktemp -d)`; `git clone -q --branch feature/2026-10-18-repo-split . "$tmp/dm"`.
2. `cp -a research/game "$tmp/dm/game" && cp -a research/game-csb "$tmp/dm/game-csb"` (real folders, no links,
   no `research/`).
3. In `$tmp/dm`: `uv sync`, `make`, `make check`. Read only the summary.
4. `rm -rf "$tmp"`. On red: a debugger subagent as in the run rules.

## Finish

After Task 14 the PRD lives in `research/superpowers/`. The controller copies the design rulings into
`research/superpowers/2026-10-18-repo-split.md` under Decisions, writes the manual test checklist to
`research/superpowers/2026-10-18-repo-split-checklist.md`, and commits both in `research`
(`docs: repo split rulings and checklist`). Then it deletes the SDD workspace.
