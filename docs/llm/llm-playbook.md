# LLM player instructions

You play the original Dungeon Master (Amiga v3.60). Your goal is to win: find the Firestaff and the Power Gem, and fuse Lord Chaos with them.

## Your notes

Your files in `playthroughs/current/player/` are your memory. Keep each file short and up to date. Each file starts with a header that says what to write and in which format: keep the header and follow it. For a new level, copy the header of `map_L1.md`. Limit: `lessons.md` and each map have at most 80 lines. When a file is longer, merge and shorten it before the end of the session.

| File | Content |
|---|---|
| `progress.md` | Where the party is, the time of the last save, the current goal, the next steps, open problems, and a milestone log. |
| `lessons.md` | What you learned from play: what happens after an action, what works and what does not. |
| `party.md` | Each champion: role, row, stats, skill levels. |
| `items.md` | Each item you saw: stats, and where it is now. |
| `map_L<n>.md` | Your map of level `n`: keys and locks, switches, hidden plates, pits, teleporters, stairs, creatures, puzzles. |
| `spells.md` | Each spell you found: runes, mana cost, effect, who can cast it. |
| `bestiary.md` | Each creature: where you met it, how strong it is, what works against it. |
| `bugs.md` | What looks wrong in the game. |

## The party

- The party has up to 4 champions in 2 rows. The front row fights hand to hand and takes most of the hits from the front. The back row casts spells, throws objects, or shoots.
- Each champion has health, stamina, and mana, and the attributes strength, dexterity, wisdom, vitality, anti-magic, anti-fire, and luck.
- Champions can swap their places in the party. A champion in the back row is safer from attacks from the front.
- The party turns as one. A creature attacks the champions on the side that faces it: a creature behind the party hits the back row.
- The leader carries the object in the hand and uses it on walls and doors.
- A dead champion leaves bones. A Vi altar brings the champion back to life.

## Health, stamina, mana

- **Health:** a champion with 0 health dies. Potions, healing spells, and sleep give health back.
- **Stamina:** fighting, walking, and carrying a load use stamina. A champion with low stamina can carry less, and at 0 stamina loses health. Sleep gives stamina back.
- **Mana:** spells cost mana. Mana comes back over time, faster during sleep.
- **Poison:** a poisoned champion loses health until the poison ends or a cure removes it.

## Food, water, sleep

- The champions eat and drink all the time. Without food or water they lose stamina and then health.
- The maximum of `food` and `water` is 2048. Estimates from an earlier run: about 1 water per 25 ticks awake; a sleep of 1500 ticks costs 200–300 food and 130–170 water. Half of the maximum is enough for about 4 long sleeps.
- Carry a food reserve in the packs. Take every food item that you find. Eat only when a champion is below half, and keep the rest. Kill creatures that drop food when you meet them, and note where they live.
- Before you go down to a new level, each champion has food above a quarter (512), or the packs hold food for each champion below that. Otherwise find food on the known levels first: a new level may have no food near the stairs.
- Food in the dungeon is finite, and the champions eat with game time. Save game time: sleep only for low health, stamina, or mana; do not wait long for one creature; go back to an earlier level only for a clear reason (water, food, a known key).
- Food and water containers feed the champion who uses them. Fountains fill water containers.
- Sleep gives back health, stamina, and mana, but time goes on: food and water go down and creatures move. Damage, a creature near the party, or `wake` wakes the party. The party does not wake by itself.

## Light and load

- Without light the view gets dark and you see less. Torches and light spells give light. Both fade with time.
- Keep the light up all the time: cast a light spell, or hold a burning torch in a hand. Darkness is dangerous: creatures, pits, and wall features in the far rows stay hidden.
- Each champion can carry a limited weight; strength sets the limit. A heavy load uses more stamina and makes the party slower.

## Skills and training

- Each champion has 4 skills: fighter, ninja, priest, and wizard. A skill gets a new level when the champion uses it.
- Fighter: hand-to-hand attacks. Ninja: fast attacks, throwing, shooting. Priest: priest spells and potions. Wizard: wizard spells.
- A new level gives more health, stamina, or mana and better attributes.
- Before the stairs down to a new level, follow the training rule in `progress.md`.

## Magic

- A spell is a row of runes: a power rune (LO, UM, ON, EE, PAL, MON), then an element rune, then more runes for some spells.
- A stronger power rune makes a stronger spell and costs more mana.
- Potion spells need an empty flask in a hand.
- Scrolls and wall texts teach spells.

## Fights

- Each champion attacks with the object in the hand. The action menu of the object shows the attacks.
- Some creatures take damage only from magic or special weapons.
- A closed door blocks creatures. A door that closes on a creature hurts it.
- Before a fight with a creature type that is in `bestiary.md`, read its section again. Use what works against it, and avoid what does not work.
- Before a timed action (a timed switch, a door that closes, a run past a trap), check `wounds`, health, and stamina of each champion. A wound or low stamina makes the party slower. Heal first.

## The dungeon

- **Doors:** a door opens with a button, a key in a lock, a switch, or a spell. Some wooden doors break under attacks.
- **Keys and locks:** each key fits locks of its own kind.
- **Switches and plates:** wall switches and floor plates open doors, close doors, open pits, move walls, or start traps. Some are hidden.
- **Walls:** some walls are illusions that the party can walk through.
- **Pits:** a pit can be the way down or a trap.
- **Teleporters:** a teleporter moves the party or objects to another square. A teleporter shows only while it is open; a closed one looks like floor. Some teleporters are never visible. It can open when something steps on it or on a timer.
- **Stairs:** stairs lead to the level above or below.
- **Alcoves and altars:** alcoves hold objects. Some alcoves and altars react to the object that you put in.
- A cell can hold a pile of many objects. `view.piles[].objects` lists every object of the pile, top first. Take every object of a pile: send `take_object` for the cell until the pile is gone, then decide what to keep.

## Saves

- A save keeps the game. After a death or a mistake, load goes back to the last save.
- There is one save slot. `save` overwrites it. Save after each milestone (key, door, puzzle, new level) and before each risk: a pit, a fight with a new creature type, a timed switch, a teleporter.
- Do not save while a champion is poisoned, starving, or below 25 health, or when the party is in a fight: you cannot undo a bad save.
- The coordinator keeps a checkpoint after each session. When the save is bad or the party cannot survive from it, ask in your report for the checkpoint of an earlier game time.
- A win ends the game: the turn response has `"game_over": true` and `state.party.dead` is `false`. Save before you enter the level of Lord Chaos.

## When you are stuck

1. Stop moving. Save.
2. Read your own notes again: every lock, key, and door in the map of the level and in `items.md`. Try each key that you carry in each lock that you know, also in locks that you did not list as exits.
3. Walk over each square of the area once and look at the objects on the floor in the view. Turn to each wall of a dead end.
4. Write a list into `progress.md` under "Not tried yet": wall features you have not clicked, keys and items you have not tried on each lock or alcove, piles you did not empty, wall texts you did not read, unexplored edges of the map, walls you did not test for an illusion, doors you could not open.
5. Rank the list. Visible things come first: keys, piles, alcoves, switches, buttons, locks, doors. Hidden switches and illusory walls come last.
6. Try the most probable item first. After each result, rank again.

## Bugs

This port is under test. When something looks wrong, write it in `bugs.md` with the steps and the state. If the game stops answering or a bug blocks progress, stop and report.

## How to play

### Start of a session

1. Read this file.
2. Read your files in `playthroughs/current/player/`: `progress.md`, `lessons.md`, the map of the current level, `party.md`, `spells.md`.
3. Run `uv run dm-play --brief get_state`. If the position or the time does not match `progress.md`, run `uv run dm-play load` and check again.
4. When the coordinator gives you a goal, it replaces the goal in `progress.md`. Write it into `progress.md` first.
5. Count the calls of the run: `wc -l < playthroughs/current/coordinator/calls.log`. Note the number. The call limit of the coordinator counts from it.

### Rules

- Play only through `uv run dm-play`. Do not read or change source code, game data files, or the decompiled code. Do not search the internet for tips, maps, or walkthroughs.
- Your general knowledge of Dungeon Master is allowed.
- Do not start or stop the game program. The coordinator runs it.
- The user watches the window. Before each `dm-play` call, write one short line of text: what you do next and why.
- Each call costs tokens. Use `--brief` for each call. Plan several steps per call: a whole route or a whole fight in one `act_many`, a long rest in one `wait`. Keep your text and your thinking per call short: do not repeat the state, decide the next steps.
- Plan a corridor or a fight in one `act_many` call. It stops early when something happens (see [Batches](#batches)). Check why it stopped before you continue.
- One `wait` with many ticks stops by itself on damage, a new creature, or a message.
- A script must not loop over `get_state`, `get_view`, `wait`, or `choose_action`: each turn is your decision.
- Ask for `get_map` only when you plan a route: one `get_map` per route plan, not one per step. A script reads the map once and sends the whole route as one `act_many`. The `act`, `act_many`, and `wait` responses already contain the state and the view.
- The view gives wall texts (`text`) and the picture of each lock (`lock`). Use screenshots only to check a suspected bug.
- Chosen runes stay after a stopped or failed cast. Before you add runes, check `state.spell.chosen` and send `delete_rune` until it is empty.

### Commands you use most

| Goal | Command |
|---|---|
| State in short form | `uv run dm-play --brief get_state` |
| What is ahead | `uv run dm-play --brief get_view` |
| Explored map | `uv run dm-play get_map` |
| Several actions | `uv run dm-play act_many '["move_forward","turn_left","move_forward"]'` |
| Pass time | `uv run dm-play wait '{"ticks":50}'` |
| Save, load | `uv run dm-play save`, `uv run dm-play load` |
| Sleep | `uv run dm-play act sleep`, then `wait`, then `uv run dm-play act wake` |
| See the screen | `uv run dm-play screenshot '{"path":"playthroughs/current/coordinator/shot.png"}'`, then read the PNG |

The screenshot path is relative to the folder where the game runs (the repository root in a run).

### dm-play

`dm-play` sends one command to the game and prints the answer.

```sh
uv run dm-play <cmd> [json-fields]
uv run dm-play act <action> [json-args]
uv run dm-play act_many '<json-list>'
```

`--brief` (before the command) prints a short summary instead of the JSON: the turn, the party, each champion, the open action menu, the messages, the walls to the sides, the visible squares ahead, the side passages, and the objects of each pile, top first. For `get_map`, it prints the `grid`. After `look`, a `look: <name> | <line> | ...` line follows the turn line. A `seen:` line lists explosions and creature losses seen during the turn. The turn line ends with `BUMPED` after a bump. While a mirror shows a champion, a `CANDIDATE (not recruited)` line follows the champions.

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

### Actions

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

A chest in the action hand (slot 1) of the open inventory opens in the panel. `state.inventory.panel` is `chest`, and `state.inventory.chest` lists the chest slots that hold an object. The chest slots are 30–37 (see the slot numbers above). `take_from_slot`, `put_in_slot`, and `look` with `slot` use the same numbers. The action clicks the chest slot box in the panel, as a player does. The game rules apply: an object that does not fit in a chest (for example a sword) stays in the leader hand. Slot 38 and higher, or a chest slot without an open chest, gives `slot out of range`. When the chest leaves the action hand, or the inventory closes, the chest closes.

### Look

The response of `look` has `look: {name, lines}`. These are the texts of the eye panel: the name, then the description lines (burnt out, compass direction, waterskin level, attributes, and the weight, in the form `WEIGHS <kg>.<tenth> KG.`). With the advanced features, a weapon also has `ATTACK <value>` and an armour `DEFENSE <value>`, the values of the stat meters. A scroll and a chest also give the name and the weight; the scroll text is in the state.

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
- `candidate`: the champion that a mirror shows, with the fields of a champion, or `null`. The candidate is not in the party: it is not in `champions[]` and is never the `leader`. Answer with `act recruit`.
- `champions[]`, one entry per party champion:
  - `index`, `name`, `title`, `alive`, `position`, `poisoned`, `action_disabled`.
  - `health`, `stamina`, `mana`: each `[current, maximum]`.
  - `food`, `water`, `load_kg` (`[current, maximum]`).
  - `statistics`, `wounds`.
  - `skills`: `{level, rank}` for fighter, ninja, priest, wizard. `rank` is the rank name of the eye panel, for example `APPRENTICE`; `null` for level 1, because the game shows no rank then.
  - `slots[]`: `slot`, `where`, `object`.
  - `actions`: the action menu of the action hand. `runes`: the chosen runes.

  Units: the values are the units of the game screen. `stamina` is the shown value (the game stores ten times that). `load_kg` is in kilograms with one decimal (the game stores tenths and shows `nn.n/ mm KG`). Health, mana, and statistics are not scaled. `food` and `water` are the stored values (the game shows them only as bars): the maximum is 2048; below 0 the champion starves or dies of thirst.
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

Visibility rule: the view has the party row (depth 0) and the rows ahead up to the first center square that blocks sight: a wall, a fake wall that looks closed, or a closed door. The row of that square is in the view, with its side squares. Nothing behind it is in the view. A side square is in the view only when the center square of its row or the side square one row nearer is open: the screen does not draw a side square behind walls. Facing a wall at depth 1, the view has at most 6 squares. A square outside the map (for example `y` -1) is the border wall of the level; the screen draws its ornaments too.

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

The map shows the same as the automap of the game: the squares that the party saw, with walls, floors, doors and their state, stairs, open pits, open teleporters, and pressure pads. It shows no wall features: a wall is `#`. What is on a wall (switches, locks, alcoves, fountains, texts) is in the view, with its side; keep it in your own map. The map shows the squares as they are now, even when they changed after you saw them.

A floor square with a pressure pad (`pressure_pad`, `round_pressure_pad`, `tiny_pressure_pad`) is `_`.

### End of a session

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Count the calls with `wc -l < playthroughs/current/coordinator/calls.log` from time to time. 20 calls before the limit, save the game and update your files, then stop. The report: where the party is, party status, what you achieved, the current goal, bugs found, and a request for an earlier checkpoint when the save is bad.
