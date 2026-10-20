# LLM mode: turn-based play over MCP

In turn-based mode, a program or an LLM plays the game through a Unix socket. The game time stands still between turns. The window shows the game, so a person can watch. The MCP server `dm-mcp` connects Claude Code to the socket.

## Play with Claude Code

### Prerequisites

- The game files are in `game/` (see the [manual](../manual.md)).
- `uv sync` is complete.
- Claude Code is installed.

### Procedure

1. Build the game.

   ```sh
   make
   ```

2. Start the game in turn-based mode with the default socket `~/.dm/control.sock`.

   ```sh
   build/dm --mode turnbased --control ~/.dm/control.sock
   ```

   The game creates the directory of the socket when it does not exist. `--control` is optional; the default is `~/.dm/control.sock`. The game enters the dungeon at the start. The keyboard and the mouse do not control the game. Closing the window quits.

3. Register the MCP server in Claude Code. Do this once. Run it from the repository root.

   ```sh
   claude mcp add dm -- uv --directory "$(pwd)" run dm-mcp
   ```

   For a socket path other than the default, set `DM_CONTROL`:

   ```sh
   claude mcp add dm -e DM_CONTROL=<socket> -- uv --directory "$(pwd)" run dm-mcp
   ```

4. Start Claude Code and ask it to play. Example prompt: `Use the dm tools. Recruit four champions and explore the first level.`

   The server instructions and the tool descriptions tell the LLM how to play. A long playthrough uses `dm-play` and the player instructions instead: see [the LLM runs page](llm-runs.md).

## Play Chaos Strikes Back

The Chaos Strikes Back (CSB) files must be in `game-csb/` (see the [manual](../manual.md)). Start the game with `--adventure csb`:

```sh
build/dm --adventure csb --mode turnbased --control ~/.dm/control.sock
```

The game selects NEW ADVENTURE at the start. `new_game` starts it again. `state.adventure` is `"csb"`.

1. The game starts in the prison (map 0, square 9,0). Recruit up to four champions at the mirrors, as in Dungeon Master.
2. Send `save`. In the prison, the save creates the adventure: the save merges the party with `disk-b/MINI.DAT`, writes `CSBGAME.DAT`, and continues at the start of the CSB dungeon (map 4).
3. The response of `save` is `{"ok":true}` only. Send `get_state` to see the new position.

After that, `save` writes `CSBGAME.DAT` (the old file becomes `CSBGAME.BAK`), and `load` loads it. The save directory is the folder of `--saves`, otherwise the CSB data folder.

## MCP tools

| Tool | Arguments | Result |
|---|---|---|
| `get_state` | none | `{ok, state}` |
| `get_view` | none | `{ok, view}` |
| `get_map` | none | `{ok, map}` |
| `act` | `action`, `args` (object) | `{ok, ticks, stopped?, game_over, bumped?, action_damage?, health_lost?, look?, seen?, state, view}` |
| `act_many` | `actions` (list) | `{ok, ticks, ran, stopped, error?, game_over, bumped?, action_damage? (list), health_lost?, seen?, state, view}` |
| `wait` | `ticks` (1–5000, default 10) | `{ok, ticks, stopped, game_over, health_lost?, seen?, state, view}` |
| `screenshot` | none | The screen as a 320 × 200 PNG image |
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
| `wait` | `ticks` (1–5000) | After the turn: `ticks` (the ticks that ran), `stopped`, `game_over`, `health_lost`, `seen`, `state`, `view`. See [Wait](llm-playbook.md#wait). |
| `screenshot` | `path` | `path`. The game writes a PNG file to `path`. |
| `save` | none | Nothing. The game writes `DMGAME.DAT` (CSB: `CSBGAME.DAT`) in the save directory. The old file becomes `DMGAME.BAK` (CSB: `CSBGAME.BAK`). |
| `load` | none | `state`. The game loads `DMGAME.DAT` (or `DMGAME.BAK` when it is missing; CSB: `CSBGAME.DAT` or `CSBGAME.BAK`) from the save directory. |
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

A `wait` turn runs `ticks` ticks. When the game cannot tick (for example a dialog is open), the turn ends after 16 frames per requested tick.

During a turn, each frame is a tick: the game does not wait 12 frames for input. The game rules do not change.

### Save and load

There is one save slot. The save directory is the folder of `--saves`, otherwise the data directory (`game/`), the same as for the SAVE button of the game. A run uses `--saves playthroughs/current/game`. `save` and `load` use the same functions as the SAVE dialog and the RESUME button (`savegame_save`, `savegame_load`, `gameloop_start_loaded_game`).

- `load` without a save file returns `{"ok": false, "error": "no saved game (use save first)"}`. Nothing changes.
- `load` of a damaged file returns an error. After that, only `load` and `new_game` work, because the game state can be incomplete.
- `load` and `new_game` work at all times, also after a game over. Each ends the old game without a question.

### Actions, stops, and fields

The actions, why a turn or a batch stops (bump, displacement, wait, seen, game over), the cells, the chest and look answers, and the fields of `state`, `view`, and `map` are in the [player instructions](llm-playbook.md#how-to-play).

## Protocol vocabulary

The names that the game writes into `state`, `view`, and `map`, and the names of the actions. Each list has the order of its table in `control/state_json.c` or `control/control_actions.c`. `tests/standard/test_vocabulary.py` checks that every name is here.

### Directions

The values of `facing`, in the order north to west.

- `north`: toward lower `y`
- `east`: toward higher `x`
- `south`: toward higher `y`
- `west`: toward lower `x`

### Statistics

The keys of the statistics of a champion.

- `luck`: chance in the game rules
- `strength`: carry weight and melee damage
- `dexterity`: hit and dodge
- `wisdom`: mana and spell skill
- `vitality`: health and stamina
- `antimagic`: resistance to magic
- `antifire`: resistance to fire

### Skills

The base skills.

- `fighter`: melee and shooting skill
- `ninja`: throwing and fast melee skill
- `priest`: healing and defence skill
- `wizard`: spell skill

### Wounds

The body parts that can hold a wound; they are also the first six slots.

- `ready_hand`: the left hand
- `action_hand`: the right hand
- `head`: the head
- `torso`: the torso
- `legs`: the legs
- `feet`: the feet

### Cells of the view

The cell of an object on a square, as the party sees it.

- `far_left`: the far left pile
- `far_right`: the far right pile
- `near_right`: the near right pile
- `near_left`: the near left pile

### Sides

The side of a square in the view.

- `left`: the lane left of the party
- `center`: the lane of the party
- `right`: the lane right of the party

### Party cells

The cell of a champion in the party.

- `front_left`: the front row, left
- `front_right`: the front row, right
- `back_right`: the back row, right
- `back_left`: the back row, left

### Slots

The places of an inventory, in index order.

- `ready_hand`: the left hand
- `action_hand`: the right hand
- `head`: the head
- `torso`: the torso
- `legs`: the legs
- `feet`: the feet
- `pouch_2`: the second pouch
- `quiver_2`: a quiver place
- `quiver_3`: a quiver place
- `quiver_4`: a quiver place
- `neck`: the neck
- `pouch_1`: the first pouch
- `quiver_1`: a quiver place
- `backpack_1`: a backpack place
- `backpack_2`: a backpack place
- `backpack_3`: a backpack place
- `backpack_4`: a backpack place
- `backpack_5`: a backpack place
- `backpack_6`: a backpack place
- `backpack_7`: a backpack place
- `backpack_8`: a backpack place
- `backpack_9`: a backpack place
- `backpack_10`: a backpack place
- `backpack_11`: a backpack place
- `backpack_12`: a backpack place
- `backpack_13`: a backpack place
- `backpack_14`: a backpack place
- `backpack_15`: a backpack place
- `backpack_16`: a backpack place
- `backpack_17`: a backpack place

### Creatures

The names of the creature types, by type. The original shows no names; these are the common ones.

- `Giant Scorpion`
- `Swamp Slime`
- `Giggler`
- `Wizard Eye`
- `Pain Rat`
- `Ruster`
- `Screamer`
- `Rockpile`
- `Ghost`
- `Stone Golem`
- `Mummy`
- `Black Flame`
- `Skeleton`
- `Couatl`
- `Vexirk`
- `Magenta Worm`
- `Trolin`
- `Giant Wasp`
- `Animated Armour`
- `Materializer`
- `Water Elemental`
- `Oitu`
- `Demon`
- `Lord Chaos`
- `Red Dragon`
- `Lord Order`
- `Grey Lord`

### Wall ornaments

The wall ornaments, by index, named from their picture. An index without a name is shown as a number.

- `square_alcove`
- `vi_altar`
- `arched_alcove`
- `hook`
- `iron_lock`
- `plate_ring`
- `dent`
- `tiny_button`
- `iron_ring`
- `crack`
- `slime_outlet`
- `green_button`
- `blue_button`
- `grate`
- `coin_slot`
- `wooden_lock`
- `rune_lock`
- `framed_lock`
- `studded_lock`
- `cross_lock`
- `gold_lock`
- `skull_lock`
- `ornate_gold_lock`
- `blue_lock`
- `green_lock`
- `diamond_lock`
- `sun_lock`
- `black_lock`
- `gem_hole`
- `creeper`
- `manacles`
- `fountain`
- `iron_ornament`
- `demon_face`
- `scratches`
- `tiny_hole`
- `round_hole`
- `narrow_slot`
- `mirror`
- `torch_holder`
- `empty_torch_holder`
- `long_holder`
- `red_button`
- `eye_socket`
- `grey_button`
- `small_crack`
- `green_gem`
- `blue_gem`
- `brown_button`
- `wall_slot`
- `wide_crack`
- `red_gem_roots`
- `blue_gem_roots`
- `rock_face`
- `lord_order`

### Floor ornaments

The floor ornaments, by index. An index without a name is shown as a number.

- `grate`
- `pressure_pad`
- `moss`
- `round_grate`
- `round_pressure_pad`
- `flame_pit`
- `crack`
- `tiny_pressure_pad`
- `puddle`

### Square looks

The look of a square on the map and in `look`.

- `wall`: a wall
- `floor`: an open floor
- `pit`: a pit
- `stairs_up`: stairs up
- `stairs_down`: stairs down
- `door`: a door
- `teleporter`: a teleporter

### Actions

The values of `action` for `act` and `act_many`.

- `move_forward`: one square forward
- `move_back`: one square back
- `move_left`: one square left, same facing
- `move_right`: one square right, same facing
- `turn_left`: turn 90° left
- `turn_right`: turn 90° right
- `click_wall_ahead`: click the wall ahead (switch, lock, mirror)
- `take_object`: take an object from the floor into the hand
- `drop_object`: drop the object of the hand
- `open_inventory`: open the inventory of a champion
- `close_inventory`: close the inventory
- `put_in_slot`: put the object of the hand into a slot
- `take_from_slot`: take the object of a slot into the hand
- `eat`: eat the object of the hand
- `drink`: drink the object of the hand
- `choose_action`: choose an action of the weapon in the hand
- `pass`: do nothing for one turn
- `set_caster`: choose the champion that casts
- `add_rune`: add a rune to the spell
- `delete_rune`: delete the last rune
- `cast`: cast the spell
- `recruit`: answer the mirror of a candidate
- `set_leader`: choose the leader
- `sleep`: let the party sleep
- `wake`: wake the party
- `swap_champions`: swap two champions
- `look`: look at a square

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

- `--brief` (before the command) prints a short summary instead of the JSON; it uses fewer tokens. The format is in the [player instructions](llm-playbook.md#dm-play).
- `dm-play` does not import the `mcp` package. The socket helpers are in `tools/dmtools/control_client.py`, which `dm-play` and `dm-mcp` both use.
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

`control/control.h` (no socket, no SDL, in `libdm`):

| Function | Purpose |
|---|---|
| `bool control_handle_line(const char *line, Json *reply)` | Answers one request line. Returns true when `reply` holds the answer, false when a turn started. |
| `bool control_end_turn(Json *reply)` | Advances the pending turn by one frame. Returns true when the request ended and `reply` holds the answer. |
| `bool control_is_turn_pending(void)` | True while a turn runs. |
| `void control_drop_client(void)` | The client is gone: a pending turn ends without a reply. |

`platform/transport.h` (plain POSIX; `transport_none.c` replaces it on Windows):

| Function | Purpose |
|---|---|
| `bool transport_is_available(void)` | True where the control socket exists (macOS). |
| `bool transport_open(const char *path)` | Listens on the socket. Replaces an old socket file. Returns false on an error. |
| `void transport_poll(void)` | Call once per frame. Advances a pending turn, accepts a client, answers requests. |
| `void transport_close(void)` | Closes the sockets and removes the socket file. |

The commands `save`, `load`, and `new_game` use `startend_save_dir()` and `gameloop_get_data_dir()`. `gameloop_load_data` sets both directories.

`control/state_json.h` (in `libdm`, no SDL): `state_write_state`, `state_write_view`, `state_write_map` append one JSON object to a `Json` buffer. `state_get_string(part)` returns a new string. Release it with `free`.

## Known limits

- In the turn-based mode the automap and the quick save keys do not work, because the keyboard does not control the game. `get_map` gives the automap.
- Throwing objects has no action.
- `recruit` with `reincarnate` keeps the old name and title: the turn-based mode has no name entry.
