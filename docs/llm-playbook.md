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
