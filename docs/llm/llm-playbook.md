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
- The party turns as one. A creature attacks the champions on the side that faces it. A creature behind the party hits the back row.
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
- Before you go down to a new level, each champion has food above a quarter (512), or the packs hold food for each champion below that. Otherwise find food on the known levels first, because a new level may have no food near the stairs.
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
- Before you take the stairs down to a new level, follow the training rule in `progress.md`.

## Magic

- A spell is a row of runes: a power rune (LO, UM, ON, EE, PAL, MON), then an element rune, then more runes for some spells.
- A stronger power rune makes a stronger spell and costs more mana.
- Potion spells need an empty flask in a hand.
- Scrolls and wall texts teach spells.

## Fights

- Each champion attacks with the object in the action hand. The action menu of the object shows the attacks.
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
- Do not save while a champion is poisoned, starving, or below 25 health, or when the party is in a fight. You cannot undo a bad save.
- The coordinator keeps a checkpoint after each session. When the save is bad or the party cannot survive from it, ask in your report for the checkpoint of an earlier game time.
- A win ends the game. The turn response has `"game_over": true` and `state.party.dead` is `false`. Save before you enter the level of Lord Chaos.

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
- Plan a corridor or a fight in one `act_many` call. It stops early when something happens (see [Batches](llm-mode.md#batches)). Check why it stopped before you continue.
- One `wait` with many ticks stops by itself on damage, a new creature, or a message.
- A script must not loop over `get_state`, `get_view`, `wait`, or `choose_action`. Each turn is your decision.
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

The output of `--brief`, the batches, the stops, the actions, and the fields of `state`, `view`, and `map` are in [LLM mode: turn-based play over MCP](llm-mode.md#socket-protocol). Read that page as well.

### End of a session

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Count the calls with `wc -l < playthroughs/current/coordinator/calls.log` from time to time. 20 calls before the limit, save the game and update your files, then stop. The report: where the party is, party status, what you achieved, the current goal, bugs found, and a request for an earlier checkpoint when the save is bad.
