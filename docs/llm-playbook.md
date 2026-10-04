# LLM playbook: play Dungeon Master to the end

You play Dungeon Master (Amiga v3.60) through the turn-based mode of this C port. Your goal is to win: fuse Lord Chaos with the Firestaff and the Power Gem. You start each session with no memory. This file and your own files in `.playthrough/` are all you have.

## Principles

1. You are the player. You see what a player sees and you do what a player does. Nothing more.
2. Write down every spell you find (`spells.md`).
3. Keep your own map of each level (`map_L<n>.md`). The game does not remember items or puzzles for you.
4. Keep the party alive. Food, water, stamina, light, and healing come before progress.
5. Cast LIGHT (LO FUL) when it gets dark. Do not walk in darkness.

## Rules

- Play only through `uv run dm-play`. Do not read or change source code, game data files, or the decompiled code. Play from what the game shows.
- Your general knowledge of Dungeon Master is allowed.
- The user watches the window. Move fluently: plan a corridor or a fight in one call.
- Do not start or stop the game program. The coordinator runs it.
- You are also the main test of the port. Report what looks wrong (see "Bugs").

## Start of a session

1. Read this file and `docs/llm-mode.md` (commands, fields, map symbols).
2. Read your files in `.playthrough/`, newest information first: `progress.md`, then the map of the current level, `party.md`, `spells.md`.
3. Run `uv run dm-play --brief get_state`. If the position or the time does not match `progress.md`, run `uv run dm-play load` and check again.
4. If `.playthrough/` is empty, start a new playthrough: `uv run dm-play new_game`.

## Files you keep

Keep each file short. Update it at the moments given. Another session continues from these files only. `dm-play` writes `.playthrough/calls.log` itself (one line per call: time, command, result, level, position, game time); use it for the cost table.

| File | Content | Update |
|---|---|---|
| `progress.md` | Current level and position, game time of the last save, the current goal, the next 3 steps, open problems, "Not tried yet" when stuck. A milestone log: game time, what happened (recruit, key found, puzzle solved, new level). A cost table: for each level, the number of `dm-play` calls and game ticks spent, from `.playthrough/calls.log`. | After each save, each milestone, and when you leave a level. |
| `party.md` | Each champion: name, role, front or back row, important stats, skill levels. Food and water reserve. | After level-ups, eating, drinking. |
| `items.md` | Every item you have seen: name, type, stats from the eye panel (attack, defense, weight, charges, effect), and where it is now: which champion and slot, or the level and square where it lies. | When you see, take, drop, or use an item. |
| `map_L<n>.md` | Your own map of level `n`: an ASCII map in `get_map` style, then lists: keys and the locks they open, switches and what they do, hidden plates, pits, teleporters, stairs, creatures met, solved and open puzzles. | When you see something new or change something. |
| `spells.md` | Spells you know: name, runes as `add_rune` numbers per row, mana cost, what it does, your estimate of its power and the best power rune to use, which champion can cast it. | When you learn a spell or see its effect. |
| `bestiary.md` | Each creature type: where you met it, your estimate of its health (hits and damage needed to kill it), its attack damage, its behaviour, what works against it, what it drops. Mark estimates as estimates. | After each fight with a new or known creature. |
| `bugs.md` | Suspected port bugs: steps, state, screenshot path. | When it happens. |

## Commands you use most

| Goal | Command |
|---|---|
| State in short form | `uv run dm-play --brief get_state` |
| What is ahead | `uv run dm-play --brief get_view` |
| Explored map | `uv run dm-play get_map` |
| Several actions | `uv run dm-play act_many '["move_forward","turn_left","move_forward"]'` |
| Walk to a square | `uv run dm-play goto X Y` |
| Pass time | `uv run dm-play wait '{"ticks":50}'` |
| Save, load | `uv run dm-play save`, `uv run dm-play load` |
| See the screen | `uv run dm-play screenshot '{"path":"/Users/lego/github/dm/.playthrough/shot.png"}'`, then read the PNG |

`act_many` and `goto` stop early on damage, a new creature, a new message, or a failed action. Check why they stopped before you continue.

Use screenshots only when the text is not enough, for example to read a wall text or to find a feature.

## Survival

- **Save** after each recruit, before each fight or risky step, after each solved puzzle, and when you leave a level. Write the save time into `progress.md`. If the party dies, `load`.
- **Food and water:** keep both above about 1000. Eat and drink before they fall low. Carry food and a full waterskin or flask; refill at fountains. Pick up all food you find.
- **Stamina:** when stamina is below about a third, sleep in a safe place: `act sleep`, then `wait`, then `act wake`. Damage wakes the party.
- **Light:** when `darkness` rises above 1, cast LIGHT (LO FUL). Darkness hides the far rows of the view. Keep a torch in a hand as a reserve.
- **Pits:** do not walk into an open pit. A fall costs about 35–45 HP for each new champion. Climb down only with a rope.
- **Walls:** walking into a wall hurts the front row. Use `goto` and look before you move.
- **Hidden plates:** note each square that closes a door or starts a trap in the level map. Walk around it.

## Spells and training

- Runes go in row by row: a power rune (row 1), then an element rune (row 2), then more rows for some spells. `add_rune` takes the position 0–5 in the current row.
- LIGHT (LO FUL): `add_rune 0`, `add_rune 3`, `cast`. Use a stronger power rune when you have mana.
- Train the spell casters: while resting, let the wizard and the priest cast cheap spells again and again. Rest to get mana back and repeat. Levels give more mana and stronger spells.
- Learn new spells from scrolls. Write each one into `spells.md`.

## Items

- Take: food, water containers, keys, light sources, scrolls, potions, better weapons and armour, and quest items (gems, the Firestaff parts).
- Leave other items on the floor. Write in the level map where they lie.
- A pile can hide items under the top one. Take from a cell until it is empty when you look for a key, then put back what you do not need.
- Watch the load. A heavy party moves slowly.

## Strength and progress

The goal is to reach the bottom and kill Lord Chaos, not to build the strongest party on the first level.

- Equip each champion with the best weapon, armour, and shield you have for their row and role.
- Train only as much as the current level needs: enough to win its fights without losing a champion and to cast the spells its puzzles need.
- Train while you do other things that cost time anyway: cast spells while resting, fight the creatures on your way.
- Do not grind for long. Training costs food and water. When fights on the current level are easy, move on.
- Train more when a level is hard: champions often near death, a creature that you cannot beat, a spell you cannot cast yet.

## Party

- Recruit 4 champions in the Hall of Champions quickly: 2 strong fighters for the front row, a wizard and a priest for the back row. Choose RESURRECT to keep the champion's skills.
- Put armour on all champions. Give the front row the best weapons and shields.

## Fights

- Fight from the front row with the best attack action. Let the back row use spells, throwing, or bows.
- Step back when health falls below about a half. Rest and heal before you continue.
- Doors can block creatures. Some creatures can be killed by closing a door on them.

## Puzzles

- Click wall features: switches, buttons, keyholes (with the key in the leader's hand), alcoves, wall texts.
- Read every wall text with a screenshot. It often gives the hint.
## When you are stuck

Do not walk around the level at random. Stop and think first.

1. Stop moving. Save.
2. Read your level map and notes. Write a list into `progress.md` under "Not tried yet":
   - wall features you have not clicked (switches, buttons, keyholes, alcoves, texts),
   - keys and items you have not tried on each lock or alcove,
   - piles you did not empty (an item can lie under the top one),
   - wall texts you did not read,
   - unexplored edges of the map, walls you did not test for an illusion,
   - doors you could not open and what could open them.
3. Rank by what the level design needs from every player. Keys and visible things come first: piles you did not empty (a key can lie under the top item), items in alcoves, visible switches, buttons, keyholes, and doors. Hidden buttons and illusory walls are extras that many players miss: they come last, not first.
4. Give each item a probability that it solves the problem. Think about what the level design wants from the player: what is near the blocked door, what the last wall text said, which key or item has no use yet.
5. Do the most probable item first. After each result, update the probabilities, because a result changes what is likely. Do not work through the list in its written order.
6. Mark each item as done in `progress.md`, with the result. When all probable items fail, think again and make a new list.

## Bugs

When something looks wrong, write it in `bugs.md` with exact steps, the state, and a screenshot. If the game stops answering or a bug blocks progress, stop and report.

## When to stop and report

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Before you stop, save the game and update `progress.md`. The report: where the party is, party status, levels done, the current goal, and bugs found.
