# LLM runs

How to run an LLM playthrough of the game with a coordinator and players.

## Goal of the runs

Each run measures how much the LLM player improves: from session to session in the run, and against the earlier runs. To finish the game is the long-term goal, not the measure of one run.

- Measure each run with `coordinator/level_stats.md`: the game time, the calls, and the tokens at the arrival on each level. Compare the run with the earlier runs.
- Between sessions and between runs, you may change the player instructions: `docs/llm/llm-playbook.md` and the headers in `playthroughs/template/`. Change them in small steps. Write each change, its reason, and the session where it starts in `coordinator/instruction_changes.md`, so that you can compare its effect.
- Allowed changes: general rules about how to play and about the game, for example survival, fights, food and water, training, saves, the use of the commands, and how to keep the memory.
- Not allowed: hints for this dungeon, for example a position, a map, the solution of a puzzle, where an item lies, which key opens which door, or the way to the next level. The player finds these itself.
- The memory of an earlier run (maps, lessons) is the player's own knowledge, not a hint.

## Runs and checkpoints

A run is one playthrough from a start save. All run folders are in `playthroughs/`. Only `playthroughs/template/` is in git, without its `DMGAME.DAT`.

```text
playthroughs/
  template/                     the player files with their headers (in git), and the start save DMGAME.DAT (local only: a game file)
  current -> run<N>             a link to the folder of the current run
  run<N>/                       the files of run N
    player/                     the player memory (the player sees only this)
    coordinator/                the coordinator files
      session_costs.md          the cost of each session
      calls.log                 one line per dm-play call of the player
      level_stats.md            at the end of the run: game time, calls, and tokens at the arrival on each level
      instruction_changes.md    each change of the player instructions: what, why, and from which session
    game/                       the live save of the run: `build/dm --saves playthroughs/current/game`
    saves/<name>/               a checkpoint: a save DMGAME.DAT and a copy of player/ in files/
  local/                        the local play folder: `make play`, then `playthroughs/local/dm --data game --saves playthroughs/local`
```

- Start a new run N: the run number continues after the last run. Create `playthroughs/run<N>/` with the folders `player/`, `coordinator/`, `game/`, and `saves/`. Copy the `*.md` files of `playthroughs/template/` to `player/`. Copy `playthroughs/template/DMGAME.DAT` (the party of 4 on the stairs to level 1) to `game/`. Create `saves/start/` with the start save and a copy of `player/` in `files/`. Point the link to the run: `ln -sfn run<N> playthroughs/current`. Then start the game and load the save as in Resume.
- Start a new run with the memory of an earlier run: as above, but copy into `player/` the files of an earlier run folder that describe the game, not its state: `map_L<n>.md`, `lessons.md`, `spells.md`, and `bestiary.md`. `progress.md`, `party.md`, `items.md`, and `bugs.md` come from the templates. Tell the player in the first session that the run starts again and that these notes come from its earlier game.
- Make a checkpoint after a session: copy `game/DMGAME.DAT` of the run and the `*.md` files of `player/` (as `files/`) to `saves/s<session>-level<level>/`, for example `saves/s3-level2/`. Add a row to `coordinator/session_costs.md`; count the calls of the session in `coordinator/calls.log`.
- The coordinator's own `dm-play` calls stay out of the log: `DM_PLAY_LOG_DIR=none uv run dm-play ...`.
- Resume from a checkpoint: copy its `DMGAME.DAT` to `game/` of the run and its `files/` to `player/`. Run `SDL_AUDIO_DRIVER=dummy build/dm --mode turnbased --saves playthroughs/current/game --control ~/.dm/control.sock` (a run plays without sound), then `DM_PLAY_LOG_DIR=none uv run dm-play load`.
- Before a player session: the game runs with `--saves playthroughs/current/game` (start it as in Resume when it does not run), and `DM_PLAY_LOG_DIR=none uv run dm-play --brief get_state` shows the position of `player/progress.md`. After the session: make the checkpoint, then stop the game or leave it for the next session. When the player reports a bad save, restore the checkpoint that it asks for (see Resume).
- Start a player session: the player reads only its instructions, the [LLM player instructions](llm-playbook.md), and its memory in `playthroughs/current/player/`. Give the player the current goal and the limit of `dm-play` calls. The player counts its calls in `coordinator/calls.log` and stops 20 calls before the limit. The player shell must run in the repository root: `dm-play` writes its log to `playthroughs/current/coordinator/` relative to it; otherwise `calls.log` stays empty.
- End a run: copy the last save to `saves/end/`, and write `coordinator/level_stats.md`.

## Coordinator as a subagent

The main session starts the coordinator as a background subagent, so that the coordinator has a clean context and the main session stays free. The coordinator plays a batch of sessions and then hands over to the next coordinator.

The two roles are agent definitions in the repository:

| Agent | File | Role |
|---|---|---|
| `dm-coordinator` | `.claude/agents/dm-coordinator.md` | Plays a batch of at most 5 sessions, makes the checkpoints, logs costs and level statistics, may change the general player instructions, and writes `coordinator/handover.md`. |
| `dm-player` | `.claude/agents/dm-player.md` | Plays one session through `dm-play` and keeps its memory. Tools: Bash, Read, Edit, Write. |

- The main session starts `dm-coordinator` with the first session number of the batch. When the batch ends, it starts the next `dm-coordinator`.
- The coordinator starts one `dm-player` per session with the session number, the goal, and the call limit.
- When the coordinator cannot start a subagent, it returns immediately with the task message of the next player. The main session then starts the player, and gives the report to the next coordinator.
