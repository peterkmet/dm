# LLM playbook: play Dungeon Master to the end

You play Dungeon Master (Amiga v3.60) through the turn-based mode of this C port. Your goal is to win: fuse Lord Chaos with the Firestaff and the Power Gem. You start each session with no memory. This file and your own files in `.playthrough/` are all you have.

## Principles

1. You are the player. You see what a player sees and you do what a player does. Nothing more.
2. Write down every spell you find (`spells.md`).
3. Keep your own map of each level (`map_L<n>.md`). The game does not remember items or puzzles for you.
4. Keep the party alive. Food, water, stamina, light, and healing come before progress.

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

Keep each file short. Update it at the moments given. Another session continues from these files only.

| File | Content | Update |
|---|---|---|
| `progress.md` | Current level and position, game time of the last save, the current goal, the next 3 steps, open problems. | After each save. |
| `party.md` | Each champion: name, role, front or back row, important stats, what each one carries (hands, body, backpack). Food and water reserve. | After you take, drop, eat, or drink. |
| `map_L<n>.md` | Your own map of level `n`: an ASCII map in `get_map` style, then lists: items left on the floor (square, what), keys and the locks they open, switches and what they do, hidden plates, pits, teleporters, stairs, dangerous creatures, solved and open puzzles. | When you see something new or change something. |
| `spells.md` | Spells you know: name, runes as `add_rune` numbers per row, mana, what it does, which champion can cast it. | When you learn a spell. |
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
- When stuck, list the open features and unexplored edges from your level map, then visit them one by one.

## Bugs

When something looks wrong, write it in `bugs.md` with exact steps, the state, and a screenshot. If the game stops answering or a bug blocks progress, stop and report.

## When to stop and report

Stop when the coordinator's limit is reached, when the game stops answering, or when a bug blocks progress. Before you stop, save the game and update `progress.md`. The report: where the party is, party status, levels done, the current goal, and bugs found.
