# Creature, projectile, and magic rules

This page describes the creature rules of the C port: creature groups and their AI, creature attacks, damage and death, group generators, projectiles, explosions, fluxcages, the fuse action, and spell effects. The code is in `port/src/engine/group.c`, `port/src/engine/projectile.c`, and `port/src/engine/magic.c`. The rules follow KAOS range 4 (GROUP.C), range 7 (PROJEXPL.C), and range 8 (MENU.C). See [range-4-6.md](symbols/range-4-6.md) and [range-7-9.md](symbols/range-7-9.md) for the record layouts and the original addresses. See [rules-movement.md](rules-movement.md) for the game loop and the timeline dispatch.

## Records

| Thing type | Size | Accessors | Content |
|---|---|---|---|
| 4 group | 16 bytes | `group.h`, `things.h` | Next thing, possessions, creature type, cells byte, 4 health words, flags in word 14 (direction, last creature index, behavior). |
| 14 projectile | 8 bytes | `projectile.h`, `things.h` | Next thing, flying thing (slot), kinetic energy, attack, timeline event index. |
| 15 explosion | 4 bytes | `projectile.h`, `things.h` | Next thing, attack, type in bits 0–6, centered flag in bit 7. |

The flying thing of a spell projectile is an explosion thing `0xff80 + type` (`EXPLOSION_THING(type)`), not a record.

Creature types come from the creature info table in KAOS (`tables_creature_info`). The port reads the table at run time.

## Active groups

The party map has active groups. Other maps do not.

- `group_add_all_active_groups()` runs when the party enters a map. Each group on the map gets an `ActiveGroup` entry, its behavior events are deleted, and it starts to wander (event 37 after 1 tick).
- `group_remove_all_active_groups()` runs when the party leaves a map. Each record gets back its cells byte and one direction. A group that flees or attacks wanders again.
- On the party map, the cells byte of the group record holds the index of the active group. `group_active(group)` returns the entry. The entry holds the cells, the directions of the creatures, the aspect (graphic offsets, flip, attack) of each creature, the home square, the prior square, the target square, the time of the last move, and the flee delay.
- A group that enters the party map from another map becomes active. A group that leaves the party map becomes inactive.
- More than `g_max_active_group_count` active groups is System Error 71.

## Group events

The timeline sends events 29–41 to `group_process_reaction_event(x, y, type, ticks)`. Other rules send a reaction with a negative type. The reaction becomes event 32 + reaction after a short delay.

| Event | Reaction | Meaning |
|---|---|---|
| 29 | −3 `REACTION_DANGER_ON_SQUARE` | Danger on the square: a closing door, a poison cloud, a fluxcage around Lord Chaos. |
| 30 | −2 `REACTION_HIT_BY_PROJECTILE` | A projectile hit a creature of the group. |
| 31 | −1 `REACTION_PARTY_IS_ADJACENT` | The party is next to the group, or a champion hit the group. |
| 32 | — | Aspect update of the whole group. |
| 33–36 | — | Aspect update of creature 0–3. |
| 37 | — | Behavior update of the whole group. |
| 38–41 | — | Behavior update of creature 0–3 (attacking group). |
| 60, 61 | — | A group waits off the map until its square is free (61 plays the teleporter sound). |

An aspect event is a behavior event − 5. When the next aspect update comes before the next behavior update, the group adds the aspect event. Byte 8 of the event keeps the delay to the behavior update.

Off the party map a group only handles events 32, 33, 37, and 38. It moves one square in a random direction when it can. It adds the next event 37 after `max(16 × |map − party map|, 2 × movement ticks)` ticks.

While Freeze Life is active, a group (not Lord Chaos) adds its event again 4 ticks later. Lord Chaos does nothing after the game is won.

## Group behavior

Bits 0–3 of group word 14 hold the behavior.

| Value | Behavior | Rule on event 37 |
|---|---|---|
| 0 | Wander | Sees the party: attacks when the party is in attack range and in line, else approaches. Does not see the party: moves toward the smell of the party, else moves randomly or stays. |
| 2, 3 | Useless | Like wander, but does not follow the smell. Behavior 2 does not approach. |
| 5 | Flee | Moves away from the party. Without sight of the party, it sometimes moves home. The flee ends after the flee delay. |
| 6 | Attack | Each creature gets its own events 38–41. |
| 7 | Approach | Moves toward the party at double speed. Without sight of the party, it moves to the target square, then wanders. |

### Sight

`group_get_distance_to_visible_party` returns the square distance to the party, or 0.

1. A creature without side attack sees only into the 90 degree cone in front of it (`group_is_destination_visible_from_source`).
2. The sight range comes from the creature info. Darkness reduces it by `palette index / 2`, unless the creature has night vision. An invisible party is hidden from a creature that does not see invisible.
3. Outside the sight range the creature can still notice the party by a random roll. Next to the party, the roll also adds the hearing and smell ranges.
4. A wall, a solid fake wall, or a closed door that is not see-through on the line from the party to the creature blocks the view.

Smell uses the smell range of the creature. A creature smells the party directly within half the smell range when only walls do not block the line. Else it follows the scent trail of the party (`g_party.scents`).

### Movement

- The creature moves when half its movement ticks have passed since the last move. Else the group waits.
- `group_get_directions_to_destination` gives a primary direction and a second direction (`g_group_secondary_direction`). Random numbers select between equal choices. This function matches the original exactly (differential test).
- `group_is_movement_possible` blocks walls, stairs, open pits (except for levitating creatures), closed fake walls, the party square, other groups, and closed doors that are too low for the creature. A wary creature avoids a teleporter to a map that does not allow its type. Lord Chaos does not enter a fluxcage.
- A creature never turns around at once. It turns left or right first.
- Lord Chaos sometimes jumps two squares over a free square.
- `group_get_move_result` moves a group through teleporters, pits (20 damage to each creature), and stairs. A group that reaches a map that does not allow its creature type dies and drops its possessions. When the destination is occupied, the group waits with event 60 or 61 and tries again every 5 ticks.

### Attack

An attacking creature (events 38–41) turns to the party, moves to the front cells of its square, then attacks. After an attack it waits its attack ticks plus a random part.

`group_is_creature_attacking` makes one attack:

- Attack range 1, or a party next to the group (half of the time): a melee hit on the champion in the cells that the creature reaches first. A creature with "attack any champion" picks a random living champion.
- A Giggler steals objects from random slots of the champion while the champion fails a dexterity and luck check. Then the Giggler often flees.
- Longer range: the creature shoots a spell projectile with step energy 8. The creature type selects the spell: slime for a Swamp Slime, lightning or open door for a Wizard Eye, a random spell for a Vexirk or Lord Chaos, poison cloud or fireball for a Materializer, else a fireball.

`group_get_champion_damage(group, champion)` gives the melee damage:

1. A sleeping party wakes up. The champion gets parry experience.
2. The champion dodges by dexterity against `random(32) + creature dexterity + 2 × map difficulty + dodge bonus − 16`. Darkness and invisibility increase the dodge bonus. Luck (60 %) can also avoid the hit.
3. The damage starts from `random(16) + creature attack + 2 × map difficulty − 2 × parry skill`, then gets a random spread.
4. `champion_add_pending_damage_and_wounds` applies armour by the attack type of the creature. A hit plays the damage sound of the champion.
5. A poison attack poisons the champion half of the time.

This function matches the original exactly (differential test).

## Damage and death

`group_get_damage_creature_outcome(group, creature, x, y, damage, notify)` hurts one creature. It returns `OUTCOME_NO_CREATURE_KILLED` (0), `OUTCOME_SOME_CREATURES_KILLED` (1), or `OUTCOME_ALL_CREATURES_KILLED` (2).

- An invulnerable creature takes no damage.
- A creature that dies leaves smoke (explosion type 40). The smoke attack depends on the creature size.
- When the last creature dies and `notify` is set, the group drops its possessions and the record becomes free.
- When one creature of several dies, the other creatures move down one index. The creature can drop its fixed possessions. An attacking group can start to flee, by its fear resistance.

`group_get_damage_all_creatures_outcome(group, x, y, attack, notify)` hurts each creature with `attack ± attack / 8`.

### Possessions

- Fixed possessions: Skeleton, Stone Golem, Trolin, Animated Armour, Rock Pile, Pain Rat, Screamer, Magenta Worm, and Red Dragon drop fixed objects from a KAOS list. An entry with bit 15 drops half of the time. Objects of an Animated Armour are cursed.
- Carried possessions: objects that the group picked up (for example a caught dagger) fall on random cells of the square.

### Other damage

- A closing door hits the creatures on its square with 5 damage. The survivors react to danger.
- A pit hits a group that falls with 20 damage.

## Group generators

Event 5 (corridor) on a group generator sensor calls `group_generate_from_sensor(sensor, x, y)`:

- Sensor word 4 bits 7–10: the creature count. With bit 3 set, the count is random.
- Sensor word 6 bits 4–7: the health multiplier. 0 means the map difficulty.
- Each creature gets `base health × multiplier + random(base health / 4 + 1)` health.
- The group enters the square with a random direction and plays a buzz sound.

`group_get_generated` returns `THING_NONE` when the group becomes active on the party map, like the original. The group is on the square in that case. Use `group_get_thing(x, y)` to find it.

Event 65 enables a generator again after its delay (see [rules-movement.md](rules-movement.md)).

## Projectiles

`projectile_create(thing, x, y, cell, direction, kinetic_energy, attack, step_energy)` puts a projectile in a cell of a square. It adds event 48 after 1 tick, or event 49 for a launcher projectile. Event 48 ignores impacts on the first step. Without a free projectile record, an object drops to the square.

Event 48 or 49 (`projectile_process_move_event`) moves a projectile one cell forward:

1. Event 49 tests the party and the group on the square. A hit ends the projectile.
2. The step energy is subtracted from the kinetic energy and from the attack. A projectile without kinetic energy drops to the square.
3. A projectile in a front cell leaves the square. A wall, a closed fake wall, or stairs in front stop it with an impact.
4. In a door square, a closed door stops it with an impact.
5. The next event comes after 1 tick on the party map, 3 ticks on other maps.

`projectile_has_impact_occurred(impact, x, y, cell, projectile)` runs one impact:

- Party: the champion in the cell takes `projectile_get_impact_attack` damage on the head or torso. Poison can follow.
- Creature: damage `impact attack × 64 / creature defense` plus poison. A non-material creature takes damage only from Harm Non-Material. A Black Flame gains health from a fireball. Some creatures catch a thrown dagger, arrow, slayer, poison dart, or throwing star.
- Door: an Open Door spell toggles a door with a button. A door grate can let a projectile pass. Else the projectile bashes the door.
- A Ful bomb or Ven bomb flask explodes into a fireball or a poison cloud.
- A spell projectile becomes an explosion of its type. A lightning bolt keeps half its kinetic energy as attack, a poison bolt a quarter.

`projectile_get_impact_attack` and `projectile_get_object_for_launcher_or_generator` match the original exactly (differential tests).

### Launchers

Event 6 (wall) on a launcher sensor (type 7–10, 14, or 15) in the event cell calls `projectile_trigger_launcher`. It shoots one or two objects or explosions from the square in front of the wall. Types 14 and 15 shoot objects that lie in the launch cells. Sensor word 6 holds the kinetic energy (bits 4–11) and the step energy (bits 12–15). The attack is 100.

### Champion projectiles

`projectile_shoot_from_champion` shoots from the front cell of the side of the champion. The party cannot move in that direction for 4 ticks.

## Explosions

`explosion_create(thing, attack, x, y, cell)` puts an explosion on a square and adds event 25 after 1 tick (5 for the first rebirth step). A fireball or lightning bolt damages the party or the group on the target square at once: `random(attack / 2 + 1) + attack / 2 + 2`, half for lightning. Fire resistance reduces the damage to creatures.

Event 25 (`explosion_process_step_event`):

| Type | Effect |
|---|---|
| 0 fireball | Breaks a door. |
| 2 lightning bolt | Breaks a door with half damage. |
| 3 harm non-material | Hurts non-material creatures. A Materializer is hurt only while it attacks. |
| 7 poison cloud | Hurts the party or the group each step. The attack fades by 3 per tick until it is 5 or less. |
| 40 smoke | Fades by 40 per tick while the attack is 56 or more. |
| 100, 101 rebirth | Step 1 becomes step 2 with a strong explosion sound. |

Then the explosion disappears.

### Fluxcage and fuse

- `projectile_fluxcage_action(x, y)` puts a fluxcage (type 50) on a square that is not a wall or stairs. Event 24 removes it after 100 ticks. When this fluxcage is the third one around Lord Chaos on an adjacent square, Lord Chaos senses danger.
- `projectile_fuse_action(x, y)` creates a Harm Non-Material explosion with attack 255. Lord Chaos in 4 fluxcages sets `g_fuse_sequence_requested`. Else Lord Chaos escapes through a free side to an open square.

## Spells

`magic_get_champion_spell_cast_result(champion)` casts the spell of the champion symbols. It returns `SPELL_CAST_SUCCESS` (1), `SPELL_CAST_FAILURE` (0), or `SPELL_CAST_FAILURE_NEEDS_FLASK` (3).

1. A dead champion or an unknown symbol string fails. An unknown string prints "meaningless spell".
2. The required level is `power + base level of the spell`. Each missing skill level is a draw against wisdom. A failed draw gives reduced experience and prints "needs more practice".
3. The spell kind selects the effect:
   - Potion: fills an empty flask in a hand. Potion power is `40 × power + random(16)`.
   - Projectile: the champion turns to the party direction and shoots the explosion. Kinetic energy is `(power + 2) × (4 + 2 × skill level)`, bounded to 21–255. Open Door counts the skill level twice. Step energy depends on the maximum mana.
   - Other: Light, Darkness, Thieves Eye, Invisibility, Party Shield, Magic Torch, Footprints, Zokathra, Fire Shield. Each effect adds its end event (70–79).
4. The champion gets the experience and an action delay (event 11).

`magic_get_click_on_spell_cast_result` casts for the magic caster and clears the symbols, except when a flask is missing.

## Fixed original bugs

Each fix has a short comment in the code.

- `group_get_generated`: a group of several creatures starts its cell value from 0, not from a stale register.
- `group_get_melee_action_damage`: a champion without strength uses base 0, not a stale register.
- Group reaction events set byte 8 (ticks) to 0. The original leaves it uninitialized.
- The square object search of a launcher stops at the end of the list.
- The spell experience squares the required level, not a stale register.

## Tests

`tests/test_creatures.py` runs these tests:

- Differential tests against the original code in `dmtools.emu`: directions to a destination, sight cone, poison resistance, champion damage by a creature, projectile impact attack, launcher objects.
- Scenario tests on the loaded dungeon: a group activates and comes next to the party, a group attacks the party, a skeleton dies and drops its possessions and smoke, a fireball flies and explodes on a wall, and spell casts (light, potion, meaningless spell, party shield, fireball).

## Not ported in this step

- Champion actions and the events 11, 12, 13, and 75 are in [rules-champions.md](rules-champions.md#actions).
- The fuse sequence (endgame). `g_fuse_sequence_requested` marks the request.
- The palette change after a light spell. The view step computes the palette from the light amount.
