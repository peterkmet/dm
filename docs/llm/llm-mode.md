# LLM mode: turn-based play over MCP

In the LLM mode, a program or an LLM plays the game through a Unix socket. The game time stands still between turns. The window shows the game, so a person can watch. The MCP server `dm-mcp` connects Claude Code to the socket.

## Play with Claude Code

### Prerequisites

- The game files are in `game/` (see the [Player manual](../manual.md)).
- `uv sync` is complete.
- Claude Code is installed.

### Procedure

1. Build the game.

   ```sh
   make
   ```

2. Start the game in the LLM mode with the default socket `~/.dm/control.sock`.

   ```sh
   build/dm --mode turnbased --control ~/.dm/control.sock
   ```

   The game creates the directory of the socket when it does not exist. The game enters the dungeon at the start. The keyboard and the mouse do not control the game. Closing the window quits.

3. Register the MCP server in Claude Code. Do this once. Run it from the repository root.

   ```sh
   claude mcp add dm -- uv --directory "$(pwd)" run dm-mcp
   ```

   For a socket path other than the default, set `DM_CONTROL`:

   ```sh
   claude mcp add dm -e DM_CONTROL=<socket> -- uv --directory "$(pwd)" run dm-mcp
   ```

4. Start Claude Code and ask it to play. Example prompt: `Use the dm tools. Recruit four champions and explore the first level.`

   The server instructions and the tool descriptions tell the LLM how to play. A long playthrough uses `dm-play` and the player instructions instead: see [LLM runs](llm-runs.md).

## Play Chaos Strikes Back

The Chaos Strikes Back (CSB) files must be in `game-csb/` (see the [Player manual](../manual.md)). Start the game with `--adventure csb`:

```sh
build/dm --adventure csb --mode turnbased --control ~/.dm/control.sock
```

The game selects NEW ADVENTURE at the start. `new_game` starts it again. `state.adventure` is `"csb"`.

1. The game starts in the prison (map 0, square 9,0). Recruit up to four champions at the mirrors.
2. Send `save`. In the prison, the save creates the adventure ([Player manual](../manual.md#chaos-strikes-back)) and continues at the start of the CSB dungeon (map 4).
3. The response of `save` is `{"ok":true}` only. Send `get_state` to see the new position.

After that, `save` writes `CSBGAME.DAT` (the old file becomes `CSBGAME.BAK`), and `load` loads it. The save directory is the folder of `--saves`, otherwise the CSB data folder.

## MCP tools

The MCP tools are the commands of the [socket protocol](#socket-protocol), with the same arguments and results. Two names differ: `save_game` and `load_game` for `save` and `load`; `screenshot` takes no path and returns the image.

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
| `act` | `action`, `args` | After the turn: `ticks`, `stopped` (only after a [displacement](#displacement)), `game_over`, `bumped` (only after a bump), `action_damage` (only `choose_action`), `health_lost`, `look` (only `look`), `seen` (see [Seen](#seen)), `state`, `view` |
| `act_many` | `actions` | After the last turn: `ticks` (all turns), `ran`, `stopped`, `error`, `game_over`, `bumped`, `action_damage` (a list, one value for each `choose_action`), `health_lost`, `seen`, `state`, `view`. See [Batches](#batches). |
| `wait` | `ticks` (1–5000) | After the turn: `ticks` (the ticks that ran), `stopped`, `game_over`, `health_lost`, `seen`, `state`, `view`. See [Wait](#wait). |
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
3. It ends when at least 2 ticks are done and the party can move again (`g_disabled_movement_ticks` is 0). It also ends after 64 ticks, and immediately when the game is over.

A `wait` turn runs `ticks` ticks. When the game cannot tick (for example a dialog is open), the turn ends after 16 frames per requested tick.

During a turn, each frame is a tick. The game does not wait 12 frames for input. The game rules do not change.

### Save and load

There is one save slot. The save directory is the folder of `--saves`, otherwise the data directory (`game/`), the same as for the SAVE button of the game. A run uses `--saves playthroughs/current/game`. `save` and `load` use the same functions as the SAVE dialog and the RESUME button (`savegame_save`, `savegame_load`, `gameloop_start_loaded_game`).

- `load` without a save file returns `{"ok": false, "error": "no saved game (use save first)"}`. Nothing changes.
- `load` of a damaged file returns an error. After that, only `load` and `new_game` work, because the game state can be incomplete.
- `load` and `new_game` work at all times, also after a game over. Each ends the old game without a question.

### Batches

A batch runs many turns with one request. Each step is one `act` turn. After each turn the batch stops early when something happens, so that the caller can look again. Each check compares with the time before the turn.

`act_many` runs the `actions` in order. An item is an action name (`"move_forward"`) or an object `{"action": "take_object", "args": {"cell": 0}}`.

```sh
uv run dm-play act_many '["turn_left","move_forward",{"action":"take_object","args":{"cell":2}}]'
```

The game does not find paths. Plan the path from your own map and send it as `act_many`.

The response has `ran` (the number of turns that ran) and `stopped`:

| `stopped` | Reason |
|---|---|
| `done` | All actions ran. |
| `damage` | A champion lost health. |
| `creature` | A creature group is in view that was not in view before the turn. The check compares the groups, not their count: a group that stays in view does not stop the batch again. |
| `message` | The game printed a new message. |
| `game_over` | The party died, or the game is won. |
| `bumped` | A move did not change the party square (a wall, a closed door, a creature). See [Bump](#bump). The batch checks this before `damage`, because a bump into a wall hurts. |
| `moved` | The dungeon moved the party: a teleporter, a pit, stairs. See [Displacement](#displacement). The batch checks this after `bumped` and before `damage`, because a pit fall hurts. |
| `turned` | The dungeon turned the party: a spinner. See [Displacement](#displacement). The batch checks this after `moved`. |
| `failed` | An action cannot run. `error` gives the reason. The failed action is not in `ran`. |

### Bump

When a move action (`move_forward`, `move_back`, `move_left`, `move_right`) does not change the party square, the turn response has `"bumped": true`. A wall, a closed door, or a creature is in the way. Without a bump, the response has no `bumped` field. A bump into a wall or a closed door gives 1 damage to the two champions on the side of the bump, as in the original game.

### Displacement

The game reports a displacement; it does not react to it. After each `act` turn and each `act_many` step, the game compares the party facing, square, and map with what the action does:

- A move action changes the square by one step in its direction, or nothing (a bump).
- `turn_left` and `turn_right` change the facing by 90°.
- All other actions change neither.

When the dungeon changes the party in another way, the turn has a displacement:

| `stopped` | Reason |
|---|---|
| `moved` | The square or the map changed in another way: a teleporter, a pit, stairs. A teleporter that sends the party back to the start square of the turn also counts as `moved`, not as `bumped`. |
| `turned` | The facing changed in another way: a spinner. |

`moved` comes before `turned` when both happen. An `act` response has `stopped` only after a displacement. An `act_many` batch stops after the step (see [Batches](#batches)).

### Wait

A `wait` turn checks the stop conditions after each tick and ends immediately when one occurs. The conditions are the same as for a batch: `damage`, `creature`, `message`, and `game_over` (see [Batches](#batches)). The response has `stopped`: `done` when all `ticks` ran, otherwise the stop condition. `ticks` gives the ticks that ran. This also works while the party sleeps: damage wakes the party.

Example: rest until something happens, at most 5000 ticks.

```sh
uv run dm-play wait '{"ticks":5000}'
```

### Seen

An explosion lasts only a few ticks. It can be gone before the turn ends, so the `view` of the response does not show it. A creature can die in the same way. For this reason, an `act`, `act_many`, or `wait` request records what the party sees after each tick, from the start of the request to the end of the last turn. The record uses the visibility rule of the view (see [View fields](#view-fields)).

The response has `seen: {explosions, creature_changes}` when the record is not empty. Without a record, the response has no `seen` field.

- `explosions[]`: `{name, x, y, depth, side}`, one entry for each explosion type on each square. `name` is the explosion type (see [View fields](#view-fields)). `depth` and `side` give the place in the view when the party first saw the explosion.
- `creature_changes[]`: `{type, x, y, depth, side, before, after}` for each creature group in view that lost creatures, or that was killed. `before` is the creature count when the party first saw the group in the request, `after` is the count when the party last saw it. `after` is 0 for a killed group. A group that only leaves the view is not in the list.

Example: a fireball kills a screamer two squares ahead.

```json
"seen": {"explosions": [{"name": "fireball", "x": 1, "y": 6, "depth": 2, "side": "center"}],
         "creature_changes": [{"type": "Screamer", "x": 1, "y": 6, "depth": 2, "side": "center", "before": 1, "after": 0}]}
```

### Game over

When the party dies or the game is won during a turn, the turn ends. The response has `"game_over": true`, and `state.party.dead` is `true` after a death and `false` after a win. The endgame runs without waiting for input, and the window shows the entrance. After that, `act`, `act_many`, `wait`, and `save` return `{"ok": false, "error": "game over"}`. `get_state`, `get_view`, `get_map`, and `screenshot` still work. To play again, send `load` or `new_game`.

### Action arguments

| `action` | `args` | Notes |
|---|---|---|
| `move_forward`, `move_back`, `move_left`, `move_right` | none | Left and right move to the side. The inventory must be closed. |
| `turn_left`, `turn_right` | none | |
| `click_wall_ahead` | none | Touches the wall ahead: mirror, button, lever, door button, fountain. The inventory must be closed. |
| `take_object` | `cell` 0–4 | The leader hand must be empty. The inventory must be closed. See `view.piles`. |
| `drop_object` | `cell` 0–4, default 0 | 0–1: own square, 2–3: square ahead, 4: alcove ahead. The inventory must be closed. |
| `open_inventory` | `champion` | |
| `close_inventory` | none | |
| `put_in_slot`, `take_from_slot` | `champion`, `slot` | Swaps the leader hand with the slot. Slots other than 0 and 1 need the open inventory of the champion. See the slot numbers below and [Chest](#chest). |
| `eat`, `drink` | none | Gives the leader hand object to the champion with the open inventory. |
| `choose_action` | `champion`, `index` | Index in the `actions` list of the champion. |
| `pass` | none | Closes an open action menu. |
| `set_caster` | `champion` | |
| `add_rune` | `symbol` 0–5 | |
| `delete_rune`, `cast` | none | |
| `recruit` | `choice`: `resurrect`, `reincarnate`, `cancel` | Only while a candidate is shown. |
| `set_leader` | `champion` | |
| `sleep`, `wake` | none | `sleep` opens the inventory of the leader when no inventory is open. |
| `swap_champions` | `a`, `b` | Clicks the icon of champion `a`, then the icon of champion `b`. The two champions swap their cells. See `position` in the state. |
| `look` | `champion`, `slot` (both optional) | Presses the eye of the open inventory and releases it. Without `slot`: the object in the leader hand. With `slot`: the empty leader hand takes the object of the slot (`champion`, default the open inventory), the eye looks, and the hand puts the object back. See [Look](#look). |

Slot numbers: 0 ready hand, 1 action hand, 2 head, 3 torso, 4 legs, 5 feet, 6 pouch 2, 7–9 quiver 2–4, 10 neck, 11 pouch 1, 12 quiver 1, 13–29 backpack, 30–37 the open chest. `state.champions[].slots[]` lists only the slots that hold an object.

While a candidate is shown, only `recruit` is accepted. While the party sleeps, only `wake` is accepted. `wait` is always accepted.

### Chest

A chest in the action hand (slot 1) of the open inventory opens in the panel. `state.inventory.panel` is `chest`, and `state.inventory.chest` lists the chest slots that hold an object. The chest slots are 30–37 (see the slot numbers above). `take_from_slot`, `put_in_slot`, and `look` with `slot` use the same numbers. The action clicks the chest slot box in the panel, as a player does. The game rules apply. An object that does not fit in a chest (for example a sword) stays in the leader hand. Slot 38 and higher, or a chest slot without an open chest, gives `slot out of range`. When the chest leaves the action hand, or the inventory closes, the chest closes.

### Look

The response of `look` has `look: {name, lines}`. These are the texts of the eye panel: the name, then the description lines (burnt out, compass direction, waterskin level, attributes, and the weight, in the form `WEIGHS <kg>.<tenth> KG.`). With the advanced features, a weapon also has `ATTACK <value>` and an armour `DEFENSE <value>`, the values of the stat meters. A scroll and a chest also give the name and the weight; the scroll text is in the state.

### Damage fields

`action_damage`: a value above 0 is the damage, 0 is no damage, -1 is "can't reach", -2 is "need ammo". After `act_many` it is a list with one value for each `choose_action`, in order.

`health_lost`: a list with one value for each champion: the health that the champion lost since the start of the request (`act`, `act_many`, or `wait`). It is only in the response when a champion lost health.

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
- `adventure`: `"dm"` or `"csb"`.
- `party`: `map`, `level`, `x`, `y`, `facing`, `darkness` (0 bright to 5 dark), `can_move`, `sleeping`, `dead`.
- `leader` (index, or `null`), `leader_hand` (object or `null`).
- `candidate`: the champion that a mirror shows, with the fields of a champion, or `null`. The candidate is not in the party. It is not in `champions[]` and is never the `leader`. Answer with `act recruit`.
- `champions[]`, one entry per party champion:
  - `index`, `name`, `title`, `alive`, `position`, `poisoned`, `action_disabled`.
  - `health`, `stamina`, `mana`: each `[current, maximum]`.
  - `food`, `water`, `load_kg` (`[current, maximum]`).
  - `statistics`, `wounds`.
  - `skills`: `{level, rank}` for fighter, ninja, priest, wizard. `rank` is the rank name of the eye panel, for example `APPRENTICE`; `null` for level 1, because the game shows no rank then.
  - `slots[]`: `slot`, `where`, `object`.
  - `actions`: the action menu of the action hand. `runes`: the chosen runes.

  Units: the values are the units of the game screen. `stamina` is the shown value (the game stores ten times that). `load_kg` is in kilograms with one decimal (the game stores tenths and shows `nn.n/ mm KG`). Health, mana, and statistics are not scaled. `food` and `water` are the stored values (the game shows them only as bars). The maximum is 2048; below 0 the champion starves or dies of thirst.
- `action_menu`: `{champion, actions}` while a menu is open, otherwise `null`.
- `spell`: `{caster, chosen, available[{symbol, name, mana}]}`, or `null`.
- `inventory`: `{champion, panel, chest?}`, or `null`.
- `messages`: the rows of the message area that hold text.

An object is `{name, kind}`. A scroll that a champion holds also has `text`.

### View fields

- `squares[]`: the visible squares, `depth` 0–3 and `side` `left`, `center`, `right`. Each has:
  - `x`, `y`, `element`: `wall`, `floor`, `door`, `pit`, `stairs_up`, `stairs_down`, `teleporter`. A door has `door` and `button`.
  - On a square that is not a wall: `objects[]` (`cell`, `name`), `flying[]` (projectiles and explosions), and `creatures` (`type`, `count`) when a group is there.
  - `floor_ornament`, from depth 1, when the view draws one: `grate`, `pressure_pad`, `moss`, `round_grate`, `round_pressure_pad`, `flame_pit`, `crack`, `tiny_pressure_pad`, or `puddle`. Another ornament gives its index in the floor ornament graphics as a number. The party row has no floor ornament, and a floor sensor without an ornament shows nothing, as on the screen.
- A `flying` entry is `{kind, name}`. A projectile has `kind` `projectile` and the object name, or `spell`. An explosion has `kind` `explosion`, `type` (the number of the game), and `name`: `fireball`, `slime`, `lightning`, `harm_non_material`, `open_door`, `poison_bolt`, `poison_cloud`, `smoke`, `fluxcage`, or `rebirth`. Another type gives its number as `name`.
- A wall has `front_face` (the side that faces the party) and `side_face` (the side that faces the lane of the party) when the face shows something. A face is `{facing, text?, mirror?, switch?, lock?, torch_holder?, alcove?, fountain?, vi_altar?, ornament?}`:
  - `facing`: the direction that the face points to.
  - `switch`: a button or a lever.
  - `lock`: a keyhole or a slot that takes or holds an object. The value names the picture, for example `iron_lock`, `gold_lock`, `sun_lock`, `eye_socket`, `gem_hole`, or `coin_slot`; a picture without a name gives its index in the wall ornament graphics.
  - `torch_holder`: a wall holder for a torch.
  - `ornament`: another ornament, named by its picture, for example `creeper` or `manacles`.
- The faces are the faces that the dungeon view draws: the front face of a center wall from depth 1, the side face of a side wall from depth 1, and the front face of a side wall from depth 2. The side walls of depth 0 have no faces.
- `piles[]`: `{cell, top, objects}` for each pile that `take_object` can reach now. `objects` lists every object of the pile, top first. `take_object` takes `objects[0]`, the same object as `top`.
- `wall_ahead_clickable`, `alcove`, `fountain`, `vi_altar`.

Visibility rule: the view has the party row (depth 0) and the rows ahead up to the first center square that blocks sight: a wall, a fake wall that looks closed, or a closed door. The row of that square is in the view, with its side squares. Nothing behind it is in the view. A side square is in the view only when the center square of its row or the side square one row nearer is open, because the screen does not draw a side square behind walls. Facing a wall at depth 1, the view has at most 6 squares. A square outside the map (for example `y` -1) is the border wall of the level; the screen draws its ornaments too.

The view shows what a player sees. Creature health is not in the view, because the original does not show it. Creature names and rune names do not come from the game data, because the original shows only pictures.

### Map fields

`map`, `level`, `width`, `height`, `party` (`[x, y]`), `facing`, `legend`, `rows`, and `grid`. Row index is `y` (north at the top). Column is `x`. The characters are in `legend`.

- `rows`: one string per `y`. Each string has exactly `width` characters, one per `x`. A square that is not explored is a space.
- `grid`: the same rows with indexes, for reading by eye. The first line is three spaces and then `x mod 10` for each column. Each next line is `y` in two characters, a space, and the row of `rows`. The character of square (`x`, `y`) is at position `3 + x` of line `y + 1`. `dm-play --brief get_map` prints `grid`.

```text
   0123456789
 0 ##########
 1 #...@....#
 2 ####D#####
```

The map shows the same as the automap of the game: the squares that the party saw, with walls, floors, doors and their state, stairs, open pits, open teleporters, and pressure pads. It shows no wall features. A wall is `#`. What is on a wall (switches, locks, alcoves, fountains, texts) is in the view, with its side; keep it in your own map. The map shows the squares as they are now, even when they changed after you saw them.

A floor square with a pressure pad (`pressure_pad`, `round_pressure_pad`, `tiny_pressure_pad`) is `_`.

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
- `priest`: healing and defense skill
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
- `look`: look at an object

## Play from the shell: dm-play

`dm-play` sends one request to the socket and prints the response as compact JSON on one line. Use it from a shell, or let an LLM use it through a shell tool instead of MCP. The socket path comes from `DM_CONTROL` (the same default as `--control`).

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

- `--brief` (before the command) prints a short summary instead of the JSON; it uses fewer tokens. The summary has the turn, the party, each champion, the open action menu, the messages, the walls to the sides, the visible squares ahead, the side passages, and the objects of each pile, top first. For `get_map`, it prints the `grid`. After `look`, a `look: <name> | <line> | ...` line follows the turn line. A `seen:` line lists explosions and creature losses seen during the turn. The turn line ends with `BUMPED` after a bump. While a mirror shows a champion, a `CANDIDATE (not recruited)` line follows the champions.
- `dm-play` does not import the `mcp` package. The socket helpers are in `tools/dmtools/control_client.py`, which `dm-play` and `dm-mcp` both use.
- A relative `path` becomes an absolute path, because the game runs in another directory.
- The exit code is 1 when the response has `"ok": false`, also when the game is not running.

## Known limits

- In the LLM mode the automap and the quick save keys do not work, because the keyboard does not control the game. `get_map` gives the automap.
- Throwing objects has no action.
- `recruit` with `reincarnate` keeps the old name and title, because the LLM mode has no name entry.
