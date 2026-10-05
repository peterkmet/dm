# LLM playbook: play Dungeon Master to the end

You play the original Dungeon Master (Amiga v3.60) through the turn-based mode of this C port. Your goal is to win: fuse Lord Chaos with the Firestaff and the Power Gem. You start each session in a running game with no memory. This file and your own files in `.playthrough/` are all you have.

## Principles

1. You are the player. You see what a player sees and you do what a player does. Nothing more.
2. Keep the party alive. Food, water, stamina, light, and healing come before progress.
3. The game does not remember items or puzzles for you. Your files do.
4. Learn from play. Write what you learn into `lessons.md` and use it.

## Rules

- Play only through `uv run dm-play`. Do not read or change source code, game data files, or the decompiled code. Play from what the game shows.
- Your general knowledge of Dungeon Master is allowed.
- The user watches the window. Move fluently: plan a corridor or a fight in one call.
- Before each `dm-play` call, write one short line of text: what you do next and why (example: "Go west to (0,19) to avoid the skeletons."). The user follows the session through these lines.
- Do not start or stop the game program. The coordinator runs it.
- You are also the main test of the port. Report what looks wrong (see "Bugs").

## Start of a session

1. Read this file.
2. Read these sections of `docs/llm-mode.md`: "Play from the shell: dm-play", "Actions", "Cells", "Look", "Chest", "State fields", "View fields", and "Map fields".
3. Read your files in `.playthrough/`, newest information first: `progress.md`, `lessons.md`, then the map of the current level, `party.md`, `spells.md`.
4. Run `uv run dm-play --brief get_state`. If the position or the time does not match `progress.md`, run `uv run dm-play load` and check again.

## Files you keep

Keep each file short. Update it at the moments given. Another session continues from these files only.

| File | Content | Update |
|---|---|---|
| `progress.md` | Current level and position, game time of the last save, the current goal, the next 3 steps, open problems, "Not tried yet" when stuck. A milestone log: game time, what happened (key found, puzzle solved, new level). | After each save, each milestone, and when you leave a level. |
| `lessons.md` | What you learned from play: what happens after an action, what works and what does not, about the game and about the commands. One line each, with the game time when you learned it. | After each surprise or mistake. |
| `party.md` | Each champion: name, role, front or back row, important stats, skill levels. Food and water reserve. | After level-ups, eating, drinking. |
| `items.md` | Every item you have seen: name, type, stats from the eye panel (attack, defense, weight, charges, effect), and where it is now: which champion and slot, or the level and square where it lies. | When you see, take, drop, or use an item. |
| `map_L<n>.md` | Your own map of level `n`: an ASCII map in `get_map` style, then lists: keys and the locks they open (by lock name), switches and what they do, hidden plates, pits, teleporters, stairs, creatures met, solved and open puzzles. | When you see something new or change something. |
| `spells.md` | Every spell you find: name, runes as `add_rune` numbers per row, mana cost, what it does, your estimate of its power and the best power rune to use, which champion can cast it. | When you learn a spell or see its effect. |
| `bestiary.md` | Each creature type: where you met it, your estimate of its health (hits and damage needed to kill it), its attack damage, its behaviour, what works against it, what it drops. Mark estimates as estimates. | After each fight with a new or known creature. |
| `bugs.md` | Suspected port bugs: steps, state, screenshot path. | When it happens. |

## Commands you use most

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

- Walk to a square: plan the path from your own map and send it as `act_many`.
- `act_many` stops early on damage, a new creature, a new message, or a failed action. Check why it stopped before you continue.
- The view gives wall texts (`text`) and the picture of each lock (`lock`, for example `gold_lock`). Use screenshots only to check a suspected bug or when the view does not explain what you see.
- Fewer calls: ask for `get_map` only when you plan a route, not after every move. The `act`, `act_many`, and `wait` responses already contain the state and the view.
- Do not write helper loops that call `get_state`, `get_view`, or short `wait`s again and again. One `wait` with many ticks stops by itself on damage, a new creature, or a message, the same as `act_many`.
- Before you add runes, check `state.spell.chosen`. Send `delete_rune` until it is empty.

## What the party needs

- **Health:** a champion with 0 health dies. Potions, healing spells, and sleep give health back.
- **Stamina:** fighting, walking, and carrying a load use stamina. A champion with low stamina can carry less, and at 0 stamina loses health. Sleep gives stamina back.
- **Mana:** spells cost mana. Mana comes back over time, faster during sleep.
- **Food and water:** the champions use them all the time. Without them, the champions lose stamina and then health. Eat and drink from your items; fountains give water.
- **Light:** without light the view gets dark and you see less. LIGHT spells and torches give light. The game state has `darkness`.
- **Load:** each champion can carry a limited weight. A heavy load uses more stamina and makes the party slower.
- **Saves:** `save` keeps the game. After a death or a mistake, `load` goes back to the last save.

## The dungeon

- **Pits:** a pit can be the way down or a trap.
- **Hidden plates:** some floor squares close doors, open pits, or start traps.

## Spells and training

- Runes go in row by row: a power rune (row 1), then an element rune (row 2), then more rows for some spells. `add_rune` takes the position 0–5 in the current row.
- Learn new spells from scrolls. Use a stronger power rune when you have mana.
- Train the spell casters while you rest: let the wizard and the priest cast cheap spells again and again, rest to get mana back, and repeat.

## Items

- Take: food, water containers, keys, light sources, scrolls, potions, better weapons and armour, and quest items (gems, the Firestaff parts).
- Leave other items on the floor. Write in the level map where they lie.
- When you look for a key, take from a cell until it is empty, then put back what you do not need.
- Watch the load.

## Strength and progress

The goal is to reach the bottom and kill Lord Chaos, not to build the strongest party.

- Equip each champion with the best weapon, armour, and shield you have for their row and role.
- Train only as much as the current level needs: enough to win its fights without losing a champion and to cast the spells its puzzles need.
- Train while you do other things that cost time anyway: cast spells while resting, fight the creatures on your way.
- Do not grind for long. Training costs food and water. When fights on the current level are easy, move on.
- Train more when a level is hard: champions often near death, a creature that you cannot beat, a spell you cannot cast yet.

## Fights

- Fight from the front row with the best attack action. Let the back row use spells, throwing, or bows.
- Step back before a champion dies. Rest and heal before you continue.
- Use doors in fights.

## Puzzles

- Click wall features: switches, buttons, keyholes (with the key in the leader's hand), alcoves.
- Read every wall text. It often gives the hint.
- Match each key to a lock by the lock name.

## When you are stuck

Do not walk around the level at random. Stop and think first.

1. Stop moving. Save.
2. Read your level map and notes. Write a list into `progress.md` under "Not tried yet":
   - wall features you have not clicked (switches, buttons, keyholes, alcoves),
   - keys and items you have not tried on each lock or alcove,
   - piles you did not empty,
   - wall texts you did not read,
   - unexplored edges of the map, walls you did not test for an illusion,
   - doors you could not open and what could open them.
3. Rank by what the level design needs from every player. Keys and visible things come first: piles you did not empty, items in alcoves, visible switches, buttons, keyholes, and doors. Hidden buttons and illusory walls are extras that many players miss: they come last, not first.
4. Give each item a probability that it solves the problem. Think about what the level design wants from the player: what is near the blocked door, what the last wall text said, which key or item has no use yet.
5. Do the most probable item first. After each result, update the probabilities, because a result changes what is likely. Do not work through the list in its written order.
6. Mark each item as done in `progress.md`, with the result. When all probable items fail, think again and make a new list.

## Bugs

When something looks wrong, write it in `bugs.md` with exact steps, the state, and a screenshot. If the game stops answering or a bug blocks progress, stop and report.

## When to stop and report

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Before you stop, save the game and update `progress.md`. The report: where the party is, party status, levels done, the current goal, and bugs found.
