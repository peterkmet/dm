# LLM mode: turn-based play over MCP

In turn-based mode, a program or an LLM plays the game through a Unix socket. The game time stands still between turns. The window shows the game, so a person can watch. The MCP server `dm-mcp` connects Claude Code to the socket.

The plan is in [2026-10-04-modes-plan.md](../superpowers/archive/2026-10-04-modes-plan.md).

## Status

| Part | File | State |
|---|---|---|
| State, view, and map as JSON | `src/platform/state_json.c` | Done, tested |
| Socket server and turns | `src/platform/control.c` | Done, tested in process |
| MCP server | `tools/dmtools/mcp_server.py` | Done, tested against the socket |
| Command-line client `dm-play` | `tools/dmtools/play.py` | Done, tested against the socket |
| `--mode turnbased --control PATH` | `src/platform/main.c` | Done, tested headless |

## Play with Claude Code

### Prerequisites

- The game files are in `game/` (see the [README](../../README.md)).
- `uv sync` is complete.
- Claude Code is installed.

### Procedure

1. Build the game.

   ```sh
   make
   ```

2. Start the game in turn-based mode. Replace `<socket>` with the socket path. Use `~/.dm/control.sock` to match the default of the MCP server.

   ```sh
   build/dm --mode turnbased --control ~/.dm/control.sock
   ```

   The game creates the directory of the socket when it does not exist. `--control` is optional; the default is `~/.dm/control.sock`. The game enters the dungeon at the start. The keyboard and the mouse do not control the game. Closing the window quits.

3. Register the MCP server in Claude Code. Do this one time.

   ```sh
   claude mcp add dm -- uv --directory /Users/lego/github/dm run dm-mcp
   ```

   For a socket path other than the default, set `DM_CONTROL`:

   ```sh
   claude mcp add dm -e DM_CONTROL=<socket> -- uv --directory /Users/lego/github/dm run dm-mcp
   ```

4. Start Claude Code and ask it to play. Example prompt: `Use the dm tools. Recruit four champions and explore the first level.`

   The server instructions and the tool descriptions tell the LLM how to play. A long playthrough uses `dm-play` and the player instructions instead: see [Runs and checkpoints](#runs-and-checkpoints).

## Goal of the runs

Each run measures how much the LLM player gets better: from session to session in the run, and against the earlier runs. To finish the game is the long-term goal, not the measure of one run.

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

- Start a new run N: create `playthroughs/run<N>/` with the folders `player/`, `coordinator/`, `game/`, and `saves/`. Copy the `*.md` files of `playthroughs/template/` to `player/`. Copy `playthroughs/template/DMGAME.DAT` (the party of 4 on the stairs to level 1) to `game/`. Create `saves/start/` with the start save and a copy of `player/` in `files/`. Point the link to the run: `ln -sfn run<N> playthroughs/current`. Run `SDL_AUDIO_DRIVER=dummy build/dm --mode turnbased --saves playthroughs/current/game --control ~/.dm/control.sock` (a run plays without sound), then `DM_PLAY_LOG_DIR=none uv run dm-play load`.
- Start a new run with the memory of an earlier run: as above, but copy into `player/` the files of the earlier run that describe the game, not its state: `map_L<n>.md`, `lessons.md`, `spells.md`, and `bestiary.md`. `progress.md`, `party.md`, `items.md`, and `bugs.md` come from the templates. Tell the player in the first session that the run starts again and that these notes come from its earlier game.
- Make a checkpoint after a session: copy `game/DMGAME.DAT` of the run and the `*.md` files of `player/` (as `files/`) to `saves/s<session>-level<level>/`, for example `saves/s3-level2/`. Add a row to `coordinator/session_costs.md`; count the calls of the session in `coordinator/calls.log`.
- The coordinator's own `dm-play` calls stay out of the log: `DM_PLAY_LOG_DIR=none uv run dm-play ...`.
- Resume from a checkpoint: copy its `DMGAME.DAT` to `game/` of the run and its `files/` to `player/`. Run `SDL_AUDIO_DRIVER=dummy build/dm --mode turnbased --saves playthroughs/current/game --control ~/.dm/control.sock` (a run plays without sound), then `DM_PLAY_LOG_DIR=none uv run dm-play load`.
- Before a player session: the game runs with `--saves playthroughs/current/game` (start it as in Resume when it does not run), and `DM_PLAY_LOG_DIR=none uv run dm-play --brief get_state` shows the position of `player/progress.md`. After the session: make the checkpoint, then stop the game or leave it for the next session. When the player reports a bad save, restore the checkpoint that it asks for (see Resume).
- Start a player session: the player reads only its instructions, the [player manual](llm-playbook.md), and its memory in `playthroughs/current/player/`. Give the player the current goal and the limit of `dm-play` calls. The player counts its calls in `coordinator/calls.log` and stops 20 calls before the limit. The player shell must run in the repository root: `dm-play` writes its log to `playthroughs/current/coordinator/` relative to it, else `calls.log` stays empty.
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
- When the coordinator cannot start a subagent, it returns at once with the task message of the next player. The main session then starts the player, and gives the report to the next coordinator.

## Play Chaos Strikes Back

The CSB files must be in `game-csb/` (see the [README](../../README.md)). Start the game with `--adventure csb`:

```sh
build/dm --adventure csb --mode turnbased --control ~/.dm/control.sock
```

The game selects NEW ADVENTURE at the start. `new_game` starts it again. `state.adventure` is `"csb"`.

1. The game starts in the prison (map 0, square 9,0). Recruit up to four champions at the mirrors, as in Dungeon Master.
2. Send `save`. In the prison, the save makes the adventure: the save merges the party with `disk-b/MINI.DAT`, writes `CSBGAME.DAT`, and continues at the start of the CSB dungeon (map 4).
3. The answer of `save` is `{"ok":true}` only. Send `get_state` to see the new position.

After that, `save` writes `CSBGAME.DAT` (the old file becomes `CSBGAME.BAK`), and `load` loads it. The save directory is the folder of `--saves`, else the CSB data folder.

## MCP tools

| Tool | Arguments | Result |
|---|---|---|
| `get_state` | none | `{ok, state}` |
| `get_view` | none | `{ok, view}` |
| `get_map` | none | `{ok, map}` |
| `act` | `action`, `args` (object) | `{ok, ticks, stopped?, game_over, bumped?, action_damage?, health_lost?, look?, seen?, state, view}` |
| `act_many` | `actions` (list) | `{ok, ticks, ran, stopped, error?, game_over, bumped?, action_damage? (list), health_lost?, seen?, state, view}` |
| `wait` | `ticks` (1-5000, default 10) | `{ok, ticks, stopped, game_over, health_lost?, seen?, state, view}` |
| `screenshot` | none | The screen as a 320 x 200 PNG image |
| `save_game` | none | `{ok}` |
| `load_game` | none | `{ok, state}` |
| `new_game` | none | `{ok, state}` |

The MCP server opens one socket connection per tool call. The timeout is 120 seconds.

## Socket protocol

Each request is one JSON object on one line. Each response is one JSON object on one line.

- Success: `{"ok": true, ...}`.
- Failure: `{"ok": false, "error": "<reason>"}`. Nothing in the game changed.

The server accepts one client at a time. While a turn runs, the server does not read the next request.

| `cmd` | Fields | Response |
|---|---|---|
| `get_state` | none | `state` |
| `get_view` | none | `view` |
| `get_map` | none | `map` |
| `act` | `action`, `args` | After the turn: `ticks`, `stopped` (only after a [displacement](llm-playbook.md#displacement)), `game_over`, `bumped` (only after a bump), `action_damage` (only `choose_action`), `health_lost`, `look` (only `look`), `seen` (see [Seen](llm-playbook.md#seen)), `state`, `view` |
| `act_many` | `actions` | After the last turn: `ticks` (all turns), `ran`, `stopped`, `error`, `game_over`, `bumped`, `action_damage` (a list, one value for each `choose_action`), `health_lost`, `seen`, `state`, `view`. See [Batches](llm-playbook.md#batches). |
| `wait` | `ticks` (1-5000) | After the turn: `ticks` (the ticks that ran), `stopped`, `game_over`, `health_lost`, `seen`, `state`, `view`. See [Wait](llm-playbook.md#wait). |
| `screenshot` | `path` | `path`. The game writes a PNG file to `path`. |
| `save` | none | Nothing. The game writes `DMGAME.DAT` (CSB: `CSBGAME.DAT`) in the save directory. The old file becomes `DMGAME.BAK` (CSB: `CSBGAME.BAK`). |
| `load` | none | `state`. The game loads `DMGAME.DAT` (else `DMGAME.BAK`; CSB: `CSBGAME.DAT`, else `CSBGAME.BAK`) from the save directory. |
| `new_game` | none | `state`. The game loads `Dungeon.DAT` from the data directory and starts on the start square (CSB: in the prison). |

Example: move one square forward.

```json
{"cmd": "act", "action": "move_forward", "args": {}}
```

The request parser is a flat search for `"key":`. Keys in `args` are found the same way. A string value cannot contain a `"` character.

### Turns

An `act` turn does these steps:

1. It gives the input as mouse clicks and keys to the command queue (`command.h`). This is the same path as the input of a player.
2. It runs the game one tick per frame.
3. It ends when at least 2 ticks are done and the party can move again (`g_disabled_movement_ticks` is 0). It also ends after 64 ticks, and at once when the game is over.

A `wait` turn runs `ticks` ticks. A game that does not tick ends a turn after 16 frames for each tick of the limit.

During a turn, each frame is a tick: the game does not wait 12 frames for input. The game rules do not change.

### Save and load

There is one save slot. The save directory is the folder of `--saves`, else the data directory (`game/`), the same as for the SAVE button of the game. A run uses `--saves playthroughs/current/game`. `save` and `load` use the same functions as the SAVE dialog and the RESUME button (`savegame_save`, `savegame_load`, `gameloop_start_loaded_game`).

- `load` without a save file returns `{"ok": false, "error": "no saved game (use save first)"}`. Nothing changes.
- `load` of a damaged file returns an error. After that, only `load` and `new_game` work, because the game state can be incomplete.
- `load` and `new_game` work at all times, also after a game over. Each ends the old game without a question.

### Actions, stops, and fields

The actions, why a turn or a batch stops (bump, displacement, wait, seen, game over), the cells, the chest and look answers, and the fields of `state`, `view`, and `map` are in the [player instructions](llm-playbook.md#how-to-play).

## Play from the shell: dm-play

`dm-play` sends one request to the socket and prints the response as compact JSON on one line. Use it from a shell, or let an LLM use it through a shell tool instead of MCP. The socket path comes from `DM_CONTROL`. The default is `~/.dm/control.sock`.

```sh
uv run dm-play <cmd> [json-fields]
uv run dm-play act <action> [json-args]
uv run dm-play act_many '<json-list>'
```

Examples:

```sh
uv run dm-play get_state
uv run dm-play act move_forward
uv run dm-play act take_object '{"cell":0}'
uv run dm-play act swap_champions '{"a":1,"b":2}'
uv run dm-play act look '{"champion":0,"slot":1}'
uv run dm-play wait '{"ticks":20}'
uv run dm-play wait '{"ticks":5000}'
uv run dm-play act_many '["turn_left","move_forward",{"action":"take_object","args":{"cell":2}}]'
uv run dm-play screenshot '{"path":"/tmp/x.png"}'
uv run dm-play save
uv run dm-play load
uv run dm-play new_game
```

- `--brief` (before the command) prints a short summary instead of the JSON: the turn (`ticks`, `ran`, `stopped`, `error`, `game_over`, `action_damage`, `health_lost`, and `BUMPED` after a bump), the party position (the first line has `| CSB` in a CSB game), the leader hand, a `CANDIDATE (not recruited)` line while a mirror shows a candidate, each champion with HP, stamina, mana, food, and water, the open action menu, the messages, the walls to the sides, the visible squares ahead, the side squares (also at depth 0, `here left` and `here right`) with a door and its state, an open pit, a wall face, a floor ornament, or creatures, every side square ahead that is not a wall (a side passage), and the piles. For `get_map`, it prints the `grid`. After `look`, a `look: <name> | <line> | ...` line follows the turn line. A response with `seen` has a line such as `seen: fireball at ahead 2; a group of 1 Screamer at ahead 2 was killed` (or `lost <n>`) after the turn line.
- `dm-play` does not import the `mcp` package. The socket helpers are in `tools/dmtools/control_client.py`, which `dm-play` and `dm-mcp` both use. The import of `dmtools.play` takes about 4 ms. This uses fewer tokens than the JSON.
- A relative `path` becomes an absolute path, because the game runs in another directory.
- The exit code is 1 when the response has `"ok": false`, also when the game is not running.

Example of `--brief`:

```text
$ uv run dm-play --brief act move_forward
ticks 2 | game_over False
time 2 | level 0 x 1 y 4 facing south | darkness 0
leader - | hand -
sides: left wall, right wall
here: floor
ahead: floor
ahead 2: floor
ahead 3: floor
```

With champions, each champion has one line, for example `0 <name> (front_left): HP 75/75 ST 63/63 MA 11/11 food 1500 water 1500`. A square shows its floor ornament, creatures, objects with their cell, a door with its state, and a wall face (`mirror`, `switch`, `lock`, `torch_holder`, `ornament`, `text`).

## C API

`control.h` (plain POSIX, in `libdm`):

| Function | Purpose |
|---|---|
| `bool control_open(const char *path)` | Listens on the socket. Replaces an old socket file. Returns false on an error. |
| `void control_poll(void)` | Call one time per frame. Accepts a client, answers requests, runs and ends turns. |
| `bool control_is_turn_pending(void)` | True while a turn runs. |
| `void control_close(void)` | Closes the sockets and removes the socket file. |

The commands `save`, `load`, and `new_game` use `startend_save_dir()` and `gameloop_get_data_dir()`. `gameloop_load_data` sets both directories.

`state_json.h` (in `libdm`, no SDL): `state_json_write_state`, `state_json_write_view`, `state_json_map` append one JSON object to a `Json` buffer. `state_json_get_string(part)` returns a new string. Release it with `free`.

## Integration in main.c

`main.c` does these steps in the `turnbased` mode:

1. It parses `--mode turnbased` and `--control PATH`, and calls `control_open(PATH)` before the main loop. It stops with an error when the call returns 0.
2. It presses ENTER at the entrance (as `DM_AUTO_ENTER` does). The entrance frames run without a turn.
3. `read_input` keeps only the quit event. Keys and clicks do not go to the command queue.
4. Each frame calls `control_poll()` after `read_input()`.
5. In the game, each frame calls `gameloop_run_frame()` and `gameloop_tick()` only when `control_is_turn_pending()` returns 1. The window still shows each frame.
6. `g_frame_hook` is `NULL`, so the endgame, the dialogs, and the fuse sequence do not wait for frames or input.
7. After a game over, the program stays in the game mode and shows the entrance. `load` and `new_game` start the next game.
8. It calls `control_close()` before `plat_quit()`.

## Known limits

- In the `turnbased` mode the automap and the quick save keys do not work, because the keyboard does not control the game. `get_map` gives the automap.
- Throwing objects has no action.
- Name entry for a reincarnated champion is not ported.

## Tests

`tests/port/test_control.py` builds a private `libdm`, starts a new game, and recruits one champion. It checks:

- The JSON of `state_json_get_string` parses and has the expected fields.
- A socket session: `get_state`, `act move_forward` changes the party position, walking to a mirror, `click_wall_ahead`, `recruit`, and `wait`.
- The MCP tool functions, called directly against the socket, including `save_game` and `load_game`.
- `save`, then a move and `wait`, then `load` restores the party position and the game time.
- `dm-play` prints one line of valid JSON, the exit code 1 for `"ok": false`, and the `--brief` summary.
- The view stops at a wall at depth 1 (6 squares) and at a closed door (nothing beyond the door, also not in the map). A mirror on a side wall is in `side_face`. The map shows the wall of a mirror as `#`: the map has no wall features, as the automap. A side square behind walls is not in the view and not in the map.
- The state: stamina in the units of the screen, skill ranks, `load_kg`, and the candidate outside `champions[]`, with the `--brief` line.
- `wait` without news runs all ticks with `stopped` `done`. Damage stops it early with `stopped` `damage` and the ticks that ran. `ticks` 5001 is refused.
- `act_many` with a creature group in view on a side square runs all turns while the group stays in view. It stops with `creature` after the first turn when a new group comes into view, also when the old group leaves the view in the same turn.
- `act_many`: the actions run in order, a failed action stops the batch, and damage stops it after the first turn. The MCP tools and `dm-play` send `act_many`.
- A move into a wall gives `"bumped": true` and the same square. `act_many` stops with `bumped`. `--brief` ends the turn line with `BUMPED`.
- A move onto a spinner of map 2 stops `act_many` with `turned` after that step. A move and turns before the spinner do not stop it. A single `act` onto the spinner has `stopped` `turned`.
- `dm-play` does not import `mcp`.
- A fireball flies to a screamer two squares ahead during a `wait`. The explosion is gone at the end of the turn, but `seen.explosions` has `fireball` on the square of the screamer, and `seen.creature_changes` has the killed screamer with `after` 0. `--brief` prints the `seen:` line. A `wait` without explosions and creature changes has no `seen`.
- `--brief` prints the name of an explosion in `flying`, also a number.
- A chest in the action hand opens. `take_from_slot`, `put_in_slot`, and `look` with a chest slot (30-37) click the chest slot box, and the state shows the change. Slot 38 gives `slot out of range`.
- `--brief` prints the creatures on a side square, also at depth 0.
- `--brief` prints a door with its state and an open pit on a side square.
- `get_map`: each row of `rows` has `width` characters. `grid` has the `x mod 10` header and the `y` in front of each row. `--brief get_map` prints `grid`.
- On map 1, the nine pads of the room east of (14, 1) and the pad at (25, 1) give `floor_ornament` `pressure_pad` in the view and `_` in the map. The party row gives none. The floor sensor at (14, 14) has no ornament and gives none. `--brief` prints the pad, also on a side square.
- `swap_champions` swaps the `position` of two champions. `look` on a slot returns the name of the sword and the weight line, and the sword is in its slot again. `look` on the leader hand gives the same text, with `ATTACK` in the advanced features. `--brief` prints the `look:` line.
- `dm-play` returns exit code 1 and logs the call when the game is not running.
- After a forced party death: `save` and `wait` return `game over`, `load` restores the game, and after a second death `new_game` starts a new game.
- A lock and another wall ornament give the name of their picture (`iron_lock`, `creeper`); `--brief` prints the name. A wall text has no leading new line.
- `alcove`, `fountain`, and `vi_altar` are false when no wall is ahead. A border wall outside the map shows its fountain.
- A teleporter that sends the party back to its start square gives `stopped` `moved`, not `bumped`.
- `act_many` gives `action_damage` as a list. `add_rune` without enough mana fails and adds no rune.
- The tests log the `dm-play` calls to a temporary folder (`tests/conftest.py`), never into a run.
- `tests/port/test_automap.py`: a pressure pad that the view draws is a gray square in the floor of the automap.

`tests/port/test_modes.py` starts `build/dm --mode turnbased --control <socket>` with the SDL dummy drivers and checks `get_state` and `act move_forward`. It also checks that a party death during a turn gives `"game_over": true`.

```sh
uv run pytest -q tests/port/test_control.py
```
