# LLM mode: turn-based play over MCP

In turn-based mode, a program or an LLM plays the game through a Unix socket. The game time stands still between turns. The window shows the game, so a person can watch. The MCP server `dm-mcp` connects Claude Code to the socket.

The plan is in [2026-10-04-modes-plan.md](superpowers/2026-10-04-modes-plan.md).

## Status

| Part | File | State |
|---|---|---|
| State, view, and map as JSON | `port/src/state_json.c` | Done, tested |
| Socket server and turns | `port/src/control.c` | Done, tested in process |
| MCP server | `src/dmtools/mcp_server.py` | Done, tested against the socket |
| Command-line client `dm-play` | `src/dmtools/play.py` | Done, tested against the socket |
| `--mode turnbased --control PATH` | `port/src/main.c` | Done, tested headless |

## Play with Claude Code

### Prerequisites

- The game files are in `game/` (see the [README](../README.md)).
- `uv sync` is complete.
- Claude Code is installed.

### Procedure

1. Build the game.

   ```sh
   make -C port
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

   The server instructions and the tool descriptions tell the LLM how to play.

## MCP tools

| Tool | Arguments | Result |
|---|---|---|
| `get_state` | none | `{ok, state}` |
| `get_view` | none | `{ok, view}` |
| `get_map` | none | `{ok, map}` |
| `act` | `action`, `args` (object) | `{ok, ticks, game_over, action_damage?, state, view}` |
| `act_many` | `actions` (list) | `{ok, ticks, ran, stopped, error?, game_over, action_damage?, state, view}` |
| `goto` | `x`, `y` | The same as `act_many` |
| `wait` | `ticks` (1-1000, default 10) | `{ok, ticks, game_over, state, view}` |
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
| `act` | `action`, `args` | After the turn: `ticks`, `game_over`, `action_damage` (only `choose_action`), `state`, `view` |
| `act_many` | `actions` | After the last turn: `ticks` (all turns), `ran`, `stopped`, `error`, `game_over`, `state`, `view`. See [Batches](#batches). |
| `goto` | `x`, `y` | The same as `act_many`. See [Batches](#batches). |
| `wait` | `ticks` (1-1000) | After the turn: `ticks`, `game_over`, `state`, `view` |
| `screenshot` | `path` | `path`. The game writes a PNG file to `path`. |
| `save` | none | Nothing. The game writes `DMGAME.DAT` in the save directory. The old file becomes `DMGAME.BAK`. |
| `load` | none | `state`. The game loads `DMGAME.DAT` (else `DMGAME.BAK`) from the save directory. |
| `new_game` | none | `state`. The game loads `Dungeon.DAT` from the data directory and starts on the start square. |

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

A `wait` turn runs exactly `ticks` ticks. A game that does not tick ends a turn after 16 frames for each tick of the limit.

During a turn, each frame is a tick: the game does not wait 12 frames for input. The game rules do not change.

### Batches

A batch runs many turns with one request. Each step is one `act` turn. After each turn the batch stops early when something happens, so that the caller can look again.

`act_many` runs the `actions` in order. An item is an action name (`"move_forward"`) or an object `{"action": "take_object", "args": {"cell": 0}}`.

```json
{"cmd": "act_many", "actions": ["turn_left", "move_forward", {"action": "take_object", "args": {"cell": 2}}]}
```

`goto` walks to the square (`x`, `y`) of the party map. It finds the shortest path (breadth-first search) over explored squares that the party can pass: floor, open doors, and destroyed doors. Stairs are allowed only as the target. Pits, walls, closed doors, visible teleporters, and squares that are not explored are not on the path. The party turns toward the next square (`turn_left`, `turn_right`), then moves forward. When no path exists, the response is `{"ok": false, "error": ...}` and nothing changes.

The response has `ran` (the number of turns that ran) and `stopped`:

| `stopped` | Reason |
|---|---|
| `done` | All actions ran, or the party is at the target. |
| `damage` | A champion lost health. |
| `creature` | More creature groups are in view than before the turn. |
| `message` | The game printed a new message. |
| `game_over` | The party died, or the game is won. |
| `failed` | An action cannot run. `error` gives the reason. The failed action is not in `ran`. |
| `blocked` | `goto` only: a move did not change the party square (a creature, a closed door). |
| `moved_elsewhere` | `goto` only: a move took the party to another square than the next one of the path (a teleporter, stairs on the way). |

### Game over

When the party dies or the game is won during a turn, the turn ends. The response has `"game_over": true`, and `state.party.dead` is `true` after a death. The endgame runs without waiting for input, and the window shows the entrance. After that, `act`, `act_many`, `goto`, `wait`, and `save` return `{"ok": false, "error": "game over"}`. `get_state`, `get_view`, `get_map`, and `screenshot` still work. To play again, send `load` or `new_game`.

### Save and load

There is one save slot. The save directory is the data directory (`game/`), the same as for the SAVE button of the game. `save` and `load` use the same functions as the SAVE dialog and the RESUME button (`savegame_save`, `savegame_load`, `gameloop_start_loaded_game`).

- `load` without a save file returns `{"ok": false, "error": "no saved game (use save first)"}`. Nothing changes.
- `load` of a damaged file returns an error. After that, only `load` and `new_game` work, because the game state can be incomplete.
- `load` and `new_game` work at all times, also after a game over. Each ends the old game without a question.

### Actions

| `action` | `args` | Notes |
|---|---|---|
| `move_forward`, `move_back`, `move_left`, `move_right` | none | Left and right move to the side. The inventory must be closed. |
| `turn_left`, `turn_right` | none | |
| `click_wall_ahead` | none | Touches the wall ahead: mirror, button, lever, door button, fountain. |
| `take_object` | `cell` 0-4 | The leader hand must be empty. See `view.piles`. |
| `drop_object` | `cell` 0-4, default 0 | 0-1: own square, 2-3: square ahead, 4: alcove ahead. |
| `open_inventory` | `champion` | |
| `close_inventory` | none | |
| `put_in_slot`, `take_from_slot` | `champion`, `slot` | Swaps the leader hand with the slot. Slots other than 0 and 1 need the open inventory of the champion. |
| `eat`, `drink` | none | Gives the leader hand object to the champion with the open inventory. |
| `choose_action` | `champion`, `index` | Index in the `actions` list of the champion. |
| `pass` | none | Closes an open action menu. |
| `set_caster` | `champion` | |
| `add_rune` | `symbol` 0-5 | |
| `delete_rune`, `cast` | none | |
| `recruit` | `choice`: `resurrect`, `reincarnate`, `cancel` | Only while a candidate is shown. |
| `set_leader` | `champion` | |
| `sleep`, `wake` | none | `sleep` opens the inventory of the leader when no inventory is open. |

While a candidate is shown, only `recruit` is accepted. While the party sleeps, only `wake` is accepted. `wait` is always accepted.

`action_damage`: a value above 0 is the damage, 0 is no damage, -1 is "can't reach", -2 is "need ammo".

### Cells

The view and the actions use cells relative to the party direction:

| Cell | Name in the view | On the own square | On the square ahead |
|---|---|---|---|
| 0 | `far_left` | `take_object` / `drop_object` cell 0 | |
| 1 | `far_right` | cell 1 | |
| 2 | `near_right` | | cell 2 |
| 3 | `near_left` | | cell 3 |

The champion `position` uses `front_left`, `front_right`, `back_right`, `back_left`.

### State fields

- `time`: game ticks.
- `party`: `map`, `level`, `x`, `y`, `facing`, `darkness` (0 bright to 5 dark), `can_move`, `sleeping`, `dead`.
- `leader` (index, or `null`), `leader_hand` (object or `null`).
- `candidate`: the champion that a mirror shows, with the fields of a champion, or `null`. The candidate is not in the party: it is not in `champions[]` and is never the `leader`. Answer with `act recruit`.
- `champions[]`: `index`, `name`, `title`, `alive`, `health`, `stamina`, `mana` (each `[current, maximum]`), `food`, `water`, `load_kg` (`[current, maximum]` in kg, one decimal), `position`, `poisoned`, `action_disabled`, `statistics`, `skills` (`{level, rank}` for fighter, ninja, priest, wizard), `wounds`, `slots[]` (`slot`, `where`, `object`), `actions` (action menu of the action hand), `runes` (chosen runes).
- The values use the units of the game screen. The game stores stamina in tenths of the value that the inventory shows, so `stamina` is the stored value divided by 10. The game stores the load in tenths of a kilogram and shows it as `nn.n/ mm KG`, so `load_kg` is in kilograms. Health, mana, and statistics are not scaled. `food` and `water` are the stored values (the game shows them only as bars).
- `rank` is the rank name that the eye panel shows for the level (from the string table of the game, for example `APPRENTICE`). It is `null` for level 1, because the game shows no rank then.
- `action_menu`: `{champion, actions}` while a menu is open, else `null`.
- `spell`: `{caster, chosen, available[{symbol, name, mana}]}`, or `null`.
- `inventory`: `{champion, panel, chest?}`, or `null`.
- `messages`: the rows of the message area that hold text.

An object is `{name, kind}`. A scroll that a champion holds also has `text`.

### View fields

- `squares[]`: the visible squares, `depth` 0-3 and `side` `left`, `center`, `right`. Each has `x`, `y`, `element` (`wall`, `floor`, `door`, `pit`, `stairs_up`, `stairs_down`, `teleporter`). A door has `door` and `button`. A square that is not a wall has `objects[]` (`cell`, `name`), `flying[]` (projectiles and explosions), and `creatures` (`type`, `count`) when a group is there.
- A wall has `front_face` (the side that faces the party) and `side_face` (the side that faces the lane of the party) when the face shows something. A face is `{facing, text?, mirror?, switch?, lock?, alcove?, fountain?, vi_altar?, ornament?}`. `facing` is the direction that the face points to. `switch` is a button or a lever. `lock` takes or holds an object: a keyhole, a torch holder, a slot. `ornament` is another ornament. The faces are the faces that the dungeon view draws: the front face of a center wall from depth 1, the side face of a side wall from depth 1, and also the front face of a side wall from depth 2. The view draws no ornaments on the side walls of depth 0, so these have no faces.
- `piles[]`: `{cell, top}` for each object that `take_object` can reach now.
- `wall_ahead_clickable`, `alcove`, `fountain`, `vi_altar`.

Visibility rule: the view has the party row (depth 0) and the rows ahead up to the first center square that blocks sight: a wall, a fake wall that looks closed, or a closed door. The row of that square is in the view, with its side squares. Nothing behind it is in the view. This is the same rule as the dungeon view and `automap_mark_seen` (`automap_visible_depths` in `port/src/automap.c`). Facing a wall at depth 1, the view has 6 squares.

The view shows what a player sees. Creature health is not in the view, because the original does not show it. Creature names and rune names do not come from the game data, because the original shows only pictures.

### Map fields

`map`, `level`, `width`, `height`, `party` (`[x, y]`), `facing`, `legend`, and `rows`. Row index is `y` (north at the top). Column is `x`. The characters are in `legend`.

A square is in the map when `automap_is_seen` (`port/src/automap.c`) says it was seen, or when it is in the current view (with the visibility rule). The game marks the seen squares after each tick, in all modes.

A wall shows a feature symbol when one of its faces shows the feature and the square in front of that face is in the map: `M` mirror, `L` switch, `K` lock, `V` vi altar, `A` alcove, `F` fountain, `?` text. With more than one feature, the first in this list wins. Other walls are `#`.

## Play from the shell: dm-play

`dm-play` sends one request to the socket and prints the response as compact JSON on one line. Use it from a shell, or let an LLM use it through a shell tool instead of MCP. The socket path comes from `DM_CONTROL`. The default is `~/.dm/control.sock`.

```sh
uv run dm-play <cmd> [json-fields]
uv run dm-play act <action> [json-args]
uv run dm-play act_many '<json-list>'
uv run dm-play goto <x> <y>
```

Examples:

```sh
uv run dm-play get_state
uv run dm-play act move_forward
uv run dm-play act take_object '{"cell":0}'
uv run dm-play wait '{"ticks":20}'
uv run dm-play act_many '["turn_left","move_forward",{"action":"take_object","args":{"cell":2}}]'
uv run dm-play goto 3 7
uv run dm-play screenshot '{"path":"/tmp/x.png"}'
uv run dm-play save
uv run dm-play load
uv run dm-play new_game
```

- `--brief` (before the command) prints a short summary instead of the JSON: the turn (`ticks`, `ran`, `stopped`, `error`, `game_over`, `action_damage`), the party position, the leader hand, a `CANDIDATE (not recruited)` line while a mirror shows a candidate, each champion with HP, stamina, mana, food, and water, the open action menu, the messages, the walls to the sides, the visible squares ahead, the side walls with a face, and the piles.
- `dm-play` does not import the `mcp` package. The socket helpers are in `src/dmtools/control_client.py`, which `dm-play` and `dm-mcp` both use. The import of `dmtools.play` takes about 4 ms. This uses fewer tokens than the JSON.
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

With champions, each champion has one line, for example `0 <name>: HP 75/75 ST 63/63 MA 11/11 food 1500 water 1500`. A square shows creatures, objects with their cell, a door with its state, and a wall face (`mirror`, `ornament`, `text`).

## C API

`control.h` (plain POSIX, in `libdm`):

| Function | Purpose |
|---|---|
| `int control_open(const char *path)` | Listens on the socket. Replaces an old socket file. Returns 1 on success. |
| `void control_poll(void)` | Call one time per frame. Accepts a client, answers requests, runs and ends turns. |
| `int control_is_turn_pending(void)` | 1 while a turn runs. |
| `void control_close(void)` | Closes the sockets and removes the socket file. |

The commands `save`, `load`, and `new_game` use `startend_save_dir()` and `gameloop_data_dir()`. `gameloop_load_data` sets both directories.

`state_json.h` (in `libdm`, no SDL): `state_json_state`, `state_json_view`, `state_json_map` append one JSON object to a `Json` buffer. `state_json_string(part)` returns a new string. Release it with `free`.

## Integration in main.c

`main.c` does these steps in the `turnbased` mode:

1. It parses `--mode turnbased` and `--control PATH`, and calls `control_open(PATH)` before the main loop. It stops with an error when the call returns 0.
2. It presses ENTER at the entrance (as `DM_AUTO_ENTER` does). The entrance frames run without a turn.
3. `read_input` keeps only the quit event. Keys and clicks do not go to the command queue.
4. Each frame calls `control_poll()` after `read_input()`.
5. In the game, each frame calls `gameloop_frame()` and `gameloop_tick()` only when `control_is_turn_pending()` returns 1. The window still shows each frame.
6. `g_frame_hook` is `NULL`, so the endgame, the dialogs, and the fuse sequence do not wait for frames or input.
7. After a game over, the program stays in the game mode and shows the entrance. `load` and `new_game` start the next game.
8. It calls `control_close()` before `plat_quit()`.

## Known limits

- In the `turnbased` mode the automap and the debug read-out keys do not work, because the keyboard does not control the game. `get_map` gives the automap.
- Throwing objects and the eye (object description panel) have no action.
- Name entry for a reincarnated champion is not ported.

## Tests

`tests/test_control.py` builds a private `libdm`, starts a new game, and recruits one champion. It checks:

- The JSON of `state_json_string` parses and has the expected fields.
- A socket session: `get_state`, `act move_forward` changes the party position, walking to a mirror, `click_wall_ahead`, `recruit`, and `wait`.
- The MCP tool functions, called directly against the socket, including `save_game` and `load_game`.
- `save`, then a move and `wait`, then `load` restores the party position and the game time.
- `dm-play` prints one line of valid JSON, the exit code 1 for `"ok": false`, and the `--brief` summary.
- The view stops at a wall at depth 1 (6 squares) and at a closed door (nothing beyond the door, also not in the map). A mirror on a side wall is in `side_face`. A mirror ahead is `M` in the map.
- The state: stamina in the units of the screen, skill ranks, `load_kg`, and the candidate outside `champions[]`, with the `--brief` line.
- `act_many`: the actions run in order, a failed action stops the batch, and damage stops it after the first turn. `goto` walks back to an explored square and refuses a wall and a square outside the map. The MCP tools and `dm-play` send `act_many` and `goto`.
- `dm-play` does not import `mcp`.
- After a forced party death: `save` and `wait` return `game over`, `load` restores the game, and after a second death `new_game` starts a new game.

`tests/test_modes.py` starts `build/dm --mode turnbased --control <socket>` with the SDL dummy drivers and checks `get_state` and `act move_forward`. It also checks that a party death during a turn gives `"game_over": true`.

```sh
uv run pytest -q tests/test_control.py
```

## Playing tips

- A pile on the floor can hide items under the top one. Take objects from the same cell until the cell is empty, then drop what you do not need. Keys are often under clothes.
- Darkness hides the far rows of the view, as on the real screen: at palette 4 you see 3 rows, at palette 5 only 2. Keep the light up: cast LIGHT with power rune 1 (LO) and element rune 4 (FUL), and cast it again when `darkness` rises.
- Train the spell casters: cast cheap spells (for example LIGHT) again and again while resting, so the wizard and the priest gain levels and mana.
- Sleep in a safe place when stamina is low. Eat and drink before food and water run out.
