# Dungeon Master: player manual

You play the original Dungeon Master (Amiga v3.60). Your goal is to win: find the Firestaff and the Power Gem, and fuse Lord Chaos with them.

## Your notes

Your files in `.playthrough/` are your memory. Keep each file short and up to date.

| File | Content |
|---|---|
| `progress.md` | Where the party is, the time of the last save, the current goal, the next steps, open problems, and a milestone log. |
| `lessons.md` | What you learned from play: what happens after an action, what works and what does not. |
| `party.md` | Each champion: role, row, stats, skill levels. |
| `items.md` | Each item you saw: stats, and where it is now. |
| `map_L<n>.md` | Your map of level `n`: keys and locks, switches, hidden plates, pits, teleporters, stairs, creatures, puzzles. |
| `spells.md` | Each spell you found: runes, mana cost, effect, who can cast it. |
| `bestiary.md` | Each creature: where you met it, how strong it is, what works against it. |
| `bugs.md` | What looks wrong in the game (this port is under test). |

## The party

- The party has up to 4 champions in 2 rows. The front row fights hand to hand and takes most of the hits from the front. The back row casts spells, throws objects, or shoots.
- Each champion has health, stamina, and mana, and the attributes strength, dexterity, wisdom, vitality, anti-magic, anti-fire, and luck.
- Champions can swap their places in the party. A champion in the back row is safer from attacks from the front.
- The party turns as one. A creature attacks the champions on the side that faces it: a creature behind the party hits the back row.
- The leader carries the object in the hand and uses it on walls and doors.
- A dead champion leaves bones. A VI altar brings the champion back to life.

## Health, stamina, mana

- **Health:** a champion with 0 health dies. Potions, healing spells, and sleep give health back.
- **Stamina:** fighting, walking, and carrying a load use stamina. A champion with low stamina can carry less, and at 0 stamina loses health. Sleep gives stamina back.
- **Mana:** spells cost mana. Mana comes back over time, faster during sleep.
- **Poison:** a poisoned champion loses health until the poison ends or a cure removes it.

## Food, water, sleep

- The champions eat and drink all the time. Without food or water they lose stamina and then health.
- Food and water containers feed the champion who uses them. Fountains fill water containers.
- Sleep gives back health, stamina, and mana, but time goes on: food and water go down and creatures move. Damage wakes the party.

## Light and load

- Without light the view gets dark and you see less. Torches and light spells give light. Both fade with time.
- Each champion can carry a limited weight; strength sets the limit. A heavy load uses more stamina and makes the party slower.

## Skills and training

- Each champion has 4 skills: fighter, ninja, priest, and wizard. A skill gets a new level when the champion uses it.
- Fighter: hand-to-hand attacks. Ninja: fast attacks, throwing, shooting. Priest: priest spells and potions. Wizard: wizard spells.
- A new level gives more health, stamina, or mana and better attributes.

## Magic

- A spell is a row of runes: a power rune (LO, UM, ON, EE, PAL, MON), then an element rune, then more runes for some spells.
- A stronger power rune makes a stronger spell and costs more mana.
- Potion spells need an empty flask in a hand.
- Scrolls and wall texts teach spells.

## Fights

- Each champion attacks with the object in the hand. The action menu of the object shows the attacks.
- Some creatures take damage only from magic or special weapons.
- A closed door blocks creatures. A door that closes on a creature hurts it.

## The dungeon

- **Doors:** a door opens with a button, a key in a lock, a switch, or a spell. Some wooden doors break under attacks.
- **Keys and locks:** each key fits locks of its own kind.
- **Switches and plates:** wall switches and floor plates open doors, close doors, open pits, move walls, or start traps. Some are hidden.
- **Walls:** some walls are illusions that the party can walk through.
- **Pits:** a pit can be the way down or a trap.
- **Teleporters:** a teleporter moves the party or objects to another square.
- **Stairs:** stairs lead to the level above or below.
- **Alcoves and altars:** alcoves hold objects. Some alcoves and altars react to the object that you put in.

## Saves

- A save keeps the game. After a death or a mistake, load goes back to the last save.

## When you are stuck

1. Stop moving. Save.
2. Write a list into `progress.md` under "Not tried yet": wall features you have not clicked, keys and items you have not tried on each lock or alcove, piles you did not empty, wall texts you did not read, unexplored edges of the map, walls you did not test for an illusion, doors you could not open.
3. Rank the list. Visible things come first: keys, piles, alcoves, switches, buttons, locks, doors. Hidden switches and illusory walls come last.
4. Try the most probable item first. After each result, rank again.

## Bugs

This port is under test. When something looks wrong, write it in `bugs.md` with the steps and the state. If the game stops answering or a bug blocks progress, stop and report.

## How to play

### Start of a session

1. Read this file.
2. Read your files in `.playthrough/`: `progress.md`, `lessons.md`, the map of the current level, `party.md`, `spells.md`.
3. Run `uv run dm-play --brief get_state`. If the position or the time does not match `progress.md`, run `uv run dm-play load` and check again.

### Rules

- Play only through `uv run dm-play`. Do not read or change source code, game data files, or the decompiled code. Do not search the internet for tips, maps, or walkthroughs.
- Your general knowledge of Dungeon Master is allowed.
- Do not start or stop the game program. The coordinator runs it.
- The user watches the window. Before each `dm-play` call, write one short line of text: what you do next and why.
- Plan a corridor or a fight in one `act_many` call. It stops early on damage, a new creature, a new message, or a failed action. Check why it stopped before you continue.
- One `wait` with many ticks stops by itself on damage, a new creature, or a message. Do not write helper loops that call `get_state`, `get_view`, or short `wait`s again and again.
- Ask for `get_map` only when you plan a route. The `act`, `act_many`, and `wait` responses already contain the state and the view.
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
| See the screen | `uv run dm-play screenshot '{"path":"/Users/lego/github/dm/.playthrough/shot.png"}'`, then read the PNG |

### dm-play

`dm-play` sends one command to the game and prints the answer.

```sh
uv run dm-play <cmd> [json-fields]
uv run dm-play act <action> [json-args]
uv run dm-play act_many '<json-list>'
```

`--brief` (before the command) prints a short summary instead of the JSON: the turn, the party, each champion, the open action menu, the messages, the walls to the sides, the visible squares ahead, the side passages, and the piles. For `get_map`, it prints the `grid`. After `look`, a `look: <name> | <line> | ...` line follows the turn line. A `seen:` line lists explosions and creature losses seen during the turn.

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
| `put_in_slot`, `take_from_slot` | `champion`, `slot` | Swaps the leader hand with the slot. Slots other than 0 and 1 need the open inventory of the champion. Slots 30-37 are the slots of the open chest. See [Chest](#chest). |
| `eat`, `drink` | none | Gives the leader hand object to the champion with the open inventory. |
| `choose_action` | `champion`, `index` | Index in the `actions` list of the champion. |
| `pass` | none | Closes an open action menu. |
| `set_caster` | `champion` | |
| `add_rune` | `symbol` 0-5 | |
| `delete_rune`, `cast` | none | |
| `recruit` | `choice`: `resurrect`, `reincarnate`, `cancel` | Only while a candidate is shown. |
| `set_leader` | `champion` | |
| `sleep`, `wake` | none | `sleep` opens the inventory of the leader when no inventory is open. |
| `swap_champions` | `a`, `b` | Clicks the icon of champion `a`, then the icon of champion `b`. The two champions swap their cells. See `position` in the state. |
| `look` | `champion`, `slot` (both optional) | Presses the eye of the open inventory and releases it. Without `slot`: the object in the leader hand. With `slot`: the empty leader hand takes the object of the slot (`champion`, default the open inventory), the eye looks, and the hand puts the object back. See [Look](#look). |

While a candidate is shown, only `recruit` is accepted. While the party sleeps, only `wake` is accepted. `wait` is always accepted.

### Chest

A chest in the action hand (slot 1) of the open inventory opens in the panel. `state.inventory.panel` is `chest`, and `state.inventory.chest` lists the chest slots that hold an object. The chest slots are 30-37: slot 30 is the first chest slot box, slot 37 the last. `take_from_slot`, `put_in_slot`, and `look` with `slot` use the same numbers. The action clicks the chest slot box in the panel, as a player does. The game rules apply: an object that does not fit in a chest (for example a sword) stays in the leader hand. Slot 38 and higher, or a chest slot without an open chest, gives `slot out of range`. When the chest leaves the action hand, or the inventory closes, the chest closes.

### Look

The response of `look` has `look: {name, lines}`. These are the texts of the eye panel: the name, then the description lines (burnt out, compass direction, waterskin level, attributes, and the weight, in the form `WEIGHS <kg>.<tenth> KG.`). With the advanced features, a weapon also has `ATTACK <value>` and an armour `DEFENSE <value>`, the values of the stat meters. A scroll and a chest also give the name and the weight; the scroll text is in the state.

`action_damage`: a value above 0 is the damage, 0 is no damage, -1 is "can't reach", -2 is "need ammo". After `act_many` it is a list with one value for each `choose_action`, in order.

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
- `champions[]`: `index`, `name`, `title`, `alive`, `health`, `stamina`, `mana` (each `[current, maximum]`), `food`, `water`, `load_kg` (`[current, maximum]` in kg, one decimal), `position`, `poisoned`, `action_disabled`, `statistics`, `skills` (`{level, rank}` for fighter, ninja, priest, wizard), `wounds`, `slots[]` (`slot`, `where`, `object`), `actions` (action menu of the action hand), `runes` (chosen runes).
- The values use the units of the game screen. The game stores stamina in tenths of the value that the inventory shows, so `stamina` is the stored value divided by 10. The game stores the load in tenths of a kilogram and shows it as `nn.n/ mm KG`, so `load_kg` is in kilograms. Health, mana, and statistics are not scaled. `food` and `water` are the stored values (the game shows them only as bars).
- `rank` is the rank name that the eye panel shows for the level (from the string table of the game, for example `APPRENTICE`). It is `null` for level 1, because the game shows no rank then.
- `action_menu`: `{champion, actions}` while a menu is open, else `null`.
- `spell`: `{caster, chosen, available[{symbol, name, mana}]}`, or `null`.
- `inventory`: `{champion, panel, chest?}`, or `null`.
- `messages`: the rows of the message area that hold text.

An object is `{name, kind}`. A scroll that a champion holds also has `text`.

### View fields

- `squares[]`: the visible squares, `depth` 0-3 and `side` `left`, `center`, `right`. Each has `x`, `y`, `element` (`wall`, `floor`, `door`, `pit`, `stairs_up`, `stairs_down`, `teleporter`). A door has `door` and `button`. A square that is not a wall has `objects[]` (`cell`, `name`), `flying[]` (projectiles and explosions), and `creatures` (`type`, `count`) when a group is there. From depth 1, a floor square has `floor_ornament` when the view draws one: `grate`, `pressure_pad`, `moss`, `round_grate`, `round_pressure_pad`, `flame_pit`, `crack`, `tiny_pressure_pad`, or `puddle` (the index of the floor ornament graphics of the game). Another ornament gives its index as a number. The view draws no floor ornament on the party row, and a floor sensor without an ornament shows nothing, as on the screen.
- A `flying` entry is `{kind, name}`. A projectile has `kind` `projectile` and the object name, or `spell`. An explosion has `kind` `explosion`, `type` (the number of the game), and `name`: `fireball`, `slime`, `lightning`, `harm_non_material`, `open_door`, `poison_bolt`, `poison_cloud`, `smoke`, `fluxcage`, or `rebirth`. Another type gives its number as `name`.
- A wall has `front_face` (the side that faces the party) and `side_face` (the side that faces the lane of the party) when the face shows something. A face is `{facing, text?, mirror?, switch?, lock?, torch_holder?, alcove?, fountain?, vi_altar?, ornament?}`. `facing` is the direction that the face points to. `switch` is a button or a lever. `lock` takes or holds an object: a keyhole or a slot. Its value names the picture, for example `iron_lock`, `gold_lock`, `sun_lock`, `eye_socket`, `gem_hole`, or `coin_slot`; a picture without a name gives its index in the wall ornament graphics. `torch_holder` is a wall holder for a torch. `ornament` is another ornament. The faces are the faces that the dungeon view draws: the front face of a center wall from depth 1, the side face of a side wall from depth 1, and also the front face of a side wall from depth 2. The view draws no ornaments on the side walls of depth 0, so these have no faces.
- `piles[]`: `{cell, top}` for each object that `take_object` can reach now.
- `wall_ahead_clickable`, `alcove`, `fountain`, `vi_altar`.

Visibility rule: the view has the party row (depth 0) and the rows ahead up to the first center square that blocks sight: a wall, a fake wall that looks closed, or a closed door. The row of that square is in the view, with its side squares. Nothing behind it is in the view. The map marks a side square only when the center square of its row or the side square one row nearer is open. Facing a wall at depth 1, the view has 6 squares.

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

A square is in the map when the party saw it, or when it is in the current view. The game marks the seen squares after each tick, in all modes.

A wall shows a feature symbol when one of its faces shows the feature and the square in front of that face is in the map: `M` mirror, `L` switch, `K` lock, `H` torch holder, `V` vi altar, `A` alcove, `F` fountain, `?` text. With more than one feature, the first in this list wins. Other walls are `#`.

A floor square with a pressure pad (`pressure_pad`, `round_pressure_pad`, `tiny_pressure_pad`) is `_`.

### End of a session

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Before you stop, save the game and update your files. The report: where the party is, party status, what you achieved, the current goal, bugs found, and the number of `dm-play` calls.
